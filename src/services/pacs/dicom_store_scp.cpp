#include "services/dicom_store_scp.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

// DCMTK headers
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>

#include <spdlog/spdlog.h>

namespace dicom_viewer::services {

class DicomStoreSCP::Impl {
public:
    Impl() = default;
    ~Impl() {
        stop();
    }

    std::expected<void, PacsErrorInfo> start(const StorageScpConfig& config) {
        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid Storage SCP configuration"
            });
        }

        if (isRunning_.load()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "Storage SCP is already running"
            });
        }

        // Store configuration
        config_ = config;

        // Create storage directory if it doesn't exist
        std::error_code ec;
        if (!std::filesystem::exists(config_.storageDirectory)) {
            if (!std::filesystem::create_directories(config_.storageDirectory, ec)) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::InternalError,
                    "Failed to create storage directory: " + ec.message()
                });
            }
        }

        // Initialize network
        OFCondition cond = ASC_initializeNetwork(
            NET_ACCEPTOR,
            static_cast<int>(config_.port),
            static_cast<int>(config_.connectionTimeout.count()),
            &network_
        );

        if (cond.bad()) {
            spdlog::error("Failed to initialize network: {}", cond.text());
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                std::string("Failed to initialize network: ") + cond.text()
            });
        }

        // Start acceptor thread
        stopRequested_.store(false);
        status_.isRunning = true;
        status_.port = config_.port;
        status_.startTime = std::chrono::system_clock::now();
        status_.totalImagesReceived = 0;
        status_.activeConnections = 0;

        acceptorThread_ = std::thread(&Impl::acceptorLoop, this);

        spdlog::info("Storage SCP started on port {} (AE: {})",
                     config_.port, config_.aeTitle);

        isRunning_.store(true);
        return {};
    }

    void stop() {
        if (!isRunning_.exchange(false)) {
            return;
        }

        stopRequested_.store(true);

        // Close network to interrupt accept
        if (network_) {
            ASC_dropNetwork(&network_);
            network_ = nullptr;
        }

        // Wait for acceptor thread
        if (acceptorThread_.joinable()) {
            acceptorThread_.join();
        }

        status_.isRunning = false;
        spdlog::info("Storage SCP stopped");
    }

    bool isRunning() const {
        return isRunning_.load();
    }

    StorageScpStatus getStatus() const {
        std::lock_guard lock(statusMutex_);
        return status_;
    }

    void setImageReceivedCallback(ImageReceivedCallback callback) {
        std::lock_guard lock(callbackMutex_);
        imageReceivedCallback_ = std::move(callback);
    }

    void setConnectionCallback(ConnectionCallback callback) {
        std::lock_guard lock(callbackMutex_);
        connectionCallback_ = std::move(callback);
    }

private:
    StorageScpConfig config_;
    T_ASC_Network* network_ = nullptr;
    std::thread acceptorThread_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequested_{false};

    mutable std::mutex statusMutex_;
    StorageScpStatus status_;

    mutable std::mutex callbackMutex_;
    ImageReceivedCallback imageReceivedCallback_;
    ConnectionCallback connectionCallback_;

    void acceptorLoop() {
        while (!stopRequested_.load()) {
            T_ASC_Association* assoc = nullptr;

            // Wait for association request
            OFCondition cond = ASC_receiveAssociation(
                network_,
                &assoc,
                config_.maxPduSize
            );

            if (stopRequested_.load()) {
                break;
            }

            if (cond.bad()) {
                if (cond == DUL_NETWORKCLOSED || cond == DUL_ILLEGALACCEPT) {
                    break;  // Network was closed, exit loop
                }
                spdlog::warn("Failed to receive association: {}", cond.text());
                continue;
            }

            // Process association in a new thread to allow concurrent connections
            std::thread([this, assoc]() {
                handleAssociation(assoc);
            }).detach();
        }
    }

    void handleAssociation(T_ASC_Association* assoc) {
        if (!assoc) return;

        std::string callingAeTitle = assoc->params->DULparams.callingAPTitle;
        spdlog::info("Association request from: {}", callingAeTitle);

        // Notify connection
        {
            std::lock_guard lock(callbackMutex_);
            if (connectionCallback_) {
                connectionCallback_(callingAeTitle, true);
            }
        }

        {
            std::lock_guard lock(statusMutex_);
            status_.activeConnections++;
        }

        // Accept presentation contexts for supported SOP classes
        acceptPresentationContexts(assoc);

        // Accept the association
        OFCondition cond = ASC_acknowledgeAssociation(assoc);
        if (cond.bad()) {
            spdlog::error("Failed to acknowledge association: {}", cond.text());
            ASC_dropAssociation(assoc);
            ASC_destroyAssociation(&assoc);
            decrementConnections(callingAeTitle);
            return;
        }

        // Process DIMSE commands
        processDimseCommands(assoc);

        // Release association
        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);

        decrementConnections(callingAeTitle);
    }

    void acceptPresentationContexts(T_ASC_Association* assoc) {
        const char* transferSyntaxes[] = {
            UID_LittleEndianExplicitTransferSyntax,
            UID_BigEndianExplicitTransferSyntax,
            UID_LittleEndianImplicitTransferSyntax
        };
        int numTransferSyntaxes = 3;

        // Get supported SOP classes
        auto sopClasses = DicomStoreSCP::getSupportedSopClasses();

        for (int i = 0; i < ASC_countPresentationContexts(assoc->params); i++) {
            T_ASC_PresentationContext pc;
            ASC_getPresentationContext(assoc->params, i, &pc);

            // Check if this SOP class is supported
            bool supported = false;
            for (const auto& sopClass : sopClasses) {
                if (sopClass == pc.abstractSyntax) {
                    supported = true;
                    break;
                }
            }

            // Also accept Verification SOP Class for C-ECHO
            if (std::string(pc.abstractSyntax) == UID_VerificationSOPClass) {
                supported = true;
            }

            if (supported) {
                // Accept with the first matching transfer syntax
                for (int j = 0; j < pc.transferSyntaxCount; j++) {
                    for (int k = 0; k < numTransferSyntaxes; k++) {
                        if (strcmp(pc.proposedTransferSyntaxes[j], transferSyntaxes[k]) == 0) {
                            ASC_acceptPresentationContext(
                                assoc->params,
                                pc.presentationContextID,
                                transferSyntaxes[k]
                            );
                            goto next_context;
                        }
                    }
                }
            }
            next_context:;
        }
    }

    void processDimseCommands(T_ASC_Association* assoc) {
        T_DIMSE_Message msg;
        T_ASC_PresentationContextID presId;

        while (!stopRequested_.load()) {
            OFCondition cond = DIMSE_receiveCommand(
                assoc,
                DIMSE_BLOCKING,
                static_cast<int>(config_.connectionTimeout.count()),
                &presId,
                &msg,
                nullptr
            );

            if (cond.bad()) {
                if (cond == DIMSE_NODATAAVAILABLE) {
                    continue;
                }
                break;
            }

            switch (msg.CommandField) {
                case DIMSE_C_STORE_RQ:
                    handleCStoreRequest(assoc, presId, msg.msg.CStoreRQ);
                    break;
                case DIMSE_C_ECHO_RQ:
                    handleCEchoRequest(assoc, presId, msg.msg.CEchoRQ);
                    break;
                default:
                    spdlog::warn("Unsupported DIMSE command: {}",
                                 static_cast<int>(msg.CommandField));
                    break;
            }
        }
    }

    void handleCStoreRequest(T_ASC_Association* assoc,
                             T_ASC_PresentationContextID presId,
                             T_DIMSE_C_StoreRQ& request) {
        // Prepare file path
        std::filesystem::path filePath = config_.storageDirectory /
            (std::string(request.AffectedSOPInstanceUID) + ".dcm");

        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();

        // Receive data
        OFCondition cond = DIMSE_storeProvider(
            assoc,
            presId,
            &request,
            nullptr,
            OFTrue,  // Use bit preserving mode
            &dataset,
            nullptr,
            nullptr,
            DIMSE_BLOCKING,
            static_cast<int>(config_.connectionTimeout.count())
        );

        T_DIMSE_C_StoreRSP response;
        bzero(&response, sizeof(response));
        response.DimseStatus = STATUS_Success;
        response.MessageIDBeingRespondedTo = request.MessageID;
        response.DataSetType = DIMSE_DATASET_NULL;
        strncpy(response.AffectedSOPClassUID, request.AffectedSOPClassUID,
                sizeof(response.AffectedSOPClassUID) - 1);
        strncpy(response.AffectedSOPInstanceUID, request.AffectedSOPInstanceUID,
                sizeof(response.AffectedSOPInstanceUID) - 1);

        if (cond.good()) {
            // Save the file
            cond = fileFormat.saveFile(filePath.string().c_str());

            if (cond.good()) {
                spdlog::info("Stored image: {}", filePath.string());

                // Build received image info
                ReceivedImageInfo info;
                info.filePath = filePath;
                info.sopClassUid = request.AffectedSOPClassUID;
                info.sopInstanceUid = request.AffectedSOPInstanceUID;
                info.callingAeTitle = assoc->params->DULparams.callingAPTitle;
                info.receivedTime = std::chrono::system_clock::now();

                // Extract additional metadata
                OFString value;
                if (dataset->findAndGetOFString(DCM_PatientID, value).good()) {
                    info.patientId = value.c_str();
                }
                if (dataset->findAndGetOFString(DCM_StudyInstanceUID, value).good()) {
                    info.studyInstanceUid = value.c_str();
                }
                if (dataset->findAndGetOFString(DCM_SeriesInstanceUID, value).good()) {
                    info.seriesInstanceUid = value.c_str();
                }

                // Update status
                {
                    std::lock_guard lock(statusMutex_);
                    status_.totalImagesReceived++;
                }

                // Notify callback
                {
                    std::lock_guard lock(callbackMutex_);
                    if (imageReceivedCallback_) {
                        imageReceivedCallback_(info);
                    }
                }
            } else {
                spdlog::error("Failed to save file: {}", cond.text());
                response.DimseStatus = STATUS_STORE_Refused_OutOfResources;
            }
        } else {
            spdlog::error("Failed to receive dataset: {}", cond.text());
            response.DimseStatus = STATUS_STORE_Error_CannotUnderstand;
        }

        // Send response
        DIMSE_sendStoreResponse(assoc, presId, &request, &response, nullptr);
    }

    void handleCEchoRequest(T_ASC_Association* assoc,
                            T_ASC_PresentationContextID presId,
                            T_DIMSE_C_EchoRQ& request) {
        OFCondition cond = DIMSE_sendEchoResponse(
            assoc,
            presId,
            &request,
            STATUS_Success,
            nullptr
        );

        if (cond.good()) {
            spdlog::debug("C-ECHO response sent successfully");
        } else {
            spdlog::warn("Failed to send C-ECHO response: {}", cond.text());
        }
    }

    void decrementConnections(const std::string& callingAeTitle) {
        {
            std::lock_guard lock(statusMutex_);
            if (status_.activeConnections > 0) {
                status_.activeConnections--;
            }
        }

        {
            std::lock_guard lock(callbackMutex_);
            if (connectionCallback_) {
                connectionCallback_(callingAeTitle, false);
            }
        }
    }
};

// Public interface implementation

DicomStoreSCP::DicomStoreSCP()
    : impl_(std::make_unique<Impl>()) {
}

DicomStoreSCP::~DicomStoreSCP() = default;

DicomStoreSCP::DicomStoreSCP(DicomStoreSCP&&) noexcept = default;
DicomStoreSCP& DicomStoreSCP::operator=(DicomStoreSCP&&) noexcept = default;

std::expected<void, PacsErrorInfo> DicomStoreSCP::start(const StorageScpConfig& config) {
    return impl_->start(config);
}

void DicomStoreSCP::stop() {
    impl_->stop();
}

bool DicomStoreSCP::isRunning() const {
    return impl_->isRunning();
}

StorageScpStatus DicomStoreSCP::getStatus() const {
    return impl_->getStatus();
}

void DicomStoreSCP::setImageReceivedCallback(ImageReceivedCallback callback) {
    impl_->setImageReceivedCallback(std::move(callback));
}

void DicomStoreSCP::setConnectionCallback(ConnectionCallback callback) {
    impl_->setConnectionCallback(std::move(callback));
}

std::vector<std::string> DicomStoreSCP::getSupportedSopClasses() {
    return {
        CT_IMAGE_STORAGE,
        MR_IMAGE_STORAGE,
        SECONDARY_CAPTURE_STORAGE,
        ENHANCED_CT_STORAGE,
        ENHANCED_MR_STORAGE
    };
}

} // namespace dicom_viewer::services

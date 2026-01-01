#include "services/dicom_move_scu.hpp"
#include "services/dicom_echo_scu.hpp"
#include "services/dicom_find_scu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

// DCMTK headers
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>

#include <spdlog/spdlog.h>

namespace dicom_viewer::services {

namespace {

/**
 * @brief Get SOP Class UID for the query root (MOVE variant)
 */
const char* getMoveSopClassUid(QueryRoot root) {
    switch (root) {
        case QueryRoot::PatientRoot:
            return DicomMoveSCU::PATIENT_ROOT_MOVE_SOP_CLASS_UID;
        case QueryRoot::StudyRoot:
            return DicomMoveSCU::STUDY_ROOT_MOVE_SOP_CLASS_UID;
    }
    return DicomMoveSCU::STUDY_ROOT_MOVE_SOP_CLASS_UID;
}

/**
 * @brief Convert RetrieveLevel to DICOM string
 */
const char* retrieveLevelToString(RetrieveLevel level) {
    switch (level) {
        case RetrieveLevel::Study:  return "STUDY";
        case RetrieveLevel::Series: return "SERIES";
        case RetrieveLevel::Image:  return "IMAGE";
    }
    return "STUDY";
}

/**
 * @brief Sanitize UID for use in filesystem paths
 */
std::string sanitizeUidForPath(const std::string& uid) {
    std::string result = uid;
    // Replace any characters that might cause issues in paths
    for (char& c : result) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return result;
}

} // anonymous namespace

/**
 * @brief Context for C-MOVE callback and sub-operations
 */
struct MoveCallbackContext {
    MoveProgress progress;
    DicomMoveSCU::ProgressCallback progressCallback;
    std::atomic<bool>* cancelled;
    std::mutex progressMutex;
    MoveConfig moveConfig;
    std::vector<std::filesystem::path> receivedFiles;

    // Current retrieval context
    std::string studyUid;
    std::string seriesUid;
};

/**
 * @brief Progress callback for receiving dataset
 *
 * This callback is invoked to report progress during dataset reception.
 */
static void datasetProgressCallback(
    void* /*callbackData*/,
    unsigned long bytesReceived
) {
    spdlog::trace("Dataset progress: {} bytes received", bytesReceived);
}

/**
 * @brief Sub-operation callback for handling incoming C-STORE during C-MOVE
 *
 * This function handles the sub-association where PACS sends images
 * via C-STORE in response to a C-MOVE request.
 */
static void moveSubOpCallback(
    void* callbackData,
    T_ASC_Network* network,
    T_ASC_Association** subAssoc
) {
    auto* ctx = static_cast<MoveCallbackContext*>(callbackData);

    if (ctx->cancelled->load()) {
        spdlog::debug("C-MOVE cancelled, aborting sub-association");
        if (*subAssoc) {
            ASC_abortAssociation(*subAssoc);
            ASC_dropAssociation(*subAssoc);
            ASC_destroyAssociation(subAssoc);
        }
        return;
    }

    // Handle incoming sub-association (C-STORE from PACS)
    if (*subAssoc == nullptr) {
        // No more sub-associations expected
        return;
    }

    T_ASC_Association* assoc = *subAssoc;

    // Process the sub-association
    bool finished = false;
    while (!finished && !ctx->cancelled->load()) {
        T_DIMSE_Message msg;
        T_ASC_PresentationContextID presId = 0;

        // Wait for incoming message
        OFCondition cond = DIMSE_receiveCommand(
            assoc,
            DIMSE_BLOCKING,
            0,
            &presId,
            &msg,
            nullptr,
            nullptr
        );

        if (cond.bad()) {
            if (cond == DUL_PEERREQUESTEDRELEASE ||
                cond == DUL_PEERABORTEDASSOCIATION) {
                finished = true;
                break;
            }
            spdlog::error("Failed to receive DIMSE command: {}", cond.text());
            finished = true;
            break;
        }

        // Check for C-STORE request (command field 0x0001)
        if (msg.CommandField == DIMSE_C_STORE_RQ) {
            T_DIMSE_C_StoreRQ& storeReq = msg.msg.CStoreRQ;
            DcmDataset* dataset = nullptr;

            spdlog::debug("Receiving C-STORE: {}", storeReq.AffectedSOPInstanceUID);

            // Receive the dataset
            cond = DIMSE_receiveDataSetInMemory(
                assoc,
                DIMSE_BLOCKING,
                0,
                &presId,
                &dataset,
                datasetProgressCallback,
                nullptr
            );

            Uint16 status = STATUS_Success;

            if (cond.good() && dataset) {
                // Determine storage path
                std::filesystem::path filePath = ctx->moveConfig.storageDirectory;

                if (ctx->moveConfig.createSubdirectories) {
                    filePath /= sanitizeUidForPath(ctx->studyUid);
                    if (!ctx->seriesUid.empty()) {
                        filePath /= sanitizeUidForPath(ctx->seriesUid);
                    }
                }

                std::filesystem::create_directories(filePath);

                std::string filename;
                if (ctx->moveConfig.useOriginalFilenames) {
                    filename = sanitizeUidForPath(storeReq.AffectedSOPInstanceUID) + ".dcm";
                } else {
                    std::lock_guard lock(ctx->progressMutex);
                    filename = std::to_string(ctx->receivedFiles.size() + 1) + ".dcm";
                }
                filePath /= filename;

                // Save to file
                DcmFileFormat fileFormat(dataset);
                OFCondition saveCond = fileFormat.saveFile(
                    filePath.string().c_str(),
                    EXS_LittleEndianExplicit
                );

                if (saveCond.good()) {
                    std::lock_guard lock(ctx->progressMutex);
                    ctx->receivedFiles.push_back(filePath);
                    spdlog::debug("Saved: {}", filePath.string());
                } else {
                    status = STATUS_STORE_Refused_OutOfResources;
                    spdlog::error("Failed to save file: {}", saveCond.text());
                }
            } else {
                status = STATUS_STORE_Error_DataSetDoesNotMatchSOPClass;
            }

            // Send C-STORE response
            T_DIMSE_C_StoreRSP storeRsp;
            memset(&storeRsp, 0, sizeof(storeRsp));
            storeRsp.MessageIDBeingRespondedTo = storeReq.MessageID;
            storeRsp.DimseStatus = status;
            storeRsp.DataSetType = DIMSE_DATASET_NULL;
            OFStandard::strlcpy(storeRsp.AffectedSOPClassUID,
                                storeReq.AffectedSOPClassUID,
                                sizeof(storeRsp.AffectedSOPClassUID));
            OFStandard::strlcpy(storeRsp.AffectedSOPInstanceUID,
                                storeReq.AffectedSOPInstanceUID,
                                sizeof(storeRsp.AffectedSOPInstanceUID));

            DIMSE_sendStoreResponse(assoc, presId, &storeReq, &storeRsp, nullptr);

            if (dataset) {
                delete dataset;
            }
        } else {
            // Unknown command, ignore
            spdlog::warn("Received unexpected command: 0x{:04X}",
                         static_cast<unsigned int>(msg.CommandField));
        }
    }

    // Release the sub-association
    if (*subAssoc) {
        ASC_releaseAssociation(*subAssoc);
        ASC_dropAssociation(*subAssoc);
        ASC_destroyAssociation(subAssoc);
    }
}

/**
 * @brief Callback for C-MOVE progress updates
 */
static void moveProgressCallback(
    void* callbackData,
    T_DIMSE_C_MoveRQ* /*request*/,
    int responseCount,
    T_DIMSE_C_MoveRSP* response
) {
    auto* ctx = static_cast<MoveCallbackContext*>(callbackData);

    if (ctx->cancelled->load()) {
        spdlog::debug("C-MOVE cancelled at response #{}", responseCount);
        return;
    }

    // Update progress from response
    {
        std::lock_guard lock(ctx->progressMutex);

        if (response->NumberOfRemainingSubOperations != 0xFFFF) {
            ctx->progress.remainingImages = response->NumberOfRemainingSubOperations;
        }
        if (response->NumberOfCompletedSubOperations != 0xFFFF) {
            ctx->progress.receivedImages = response->NumberOfCompletedSubOperations;
        }
        if (response->NumberOfFailedSubOperations != 0xFFFF) {
            ctx->progress.failedImages = response->NumberOfFailedSubOperations;
        }
        if (response->NumberOfWarningSubOperations != 0xFFFF) {
            ctx->progress.warningImages = response->NumberOfWarningSubOperations;
        }

        // Calculate total from first meaningful response
        if (ctx->progress.totalImages == 0 && ctx->progress.remainingImages > 0) {
            ctx->progress.totalImages = ctx->progress.remainingImages +
                                         ctx->progress.receivedImages +
                                         ctx->progress.failedImages;
        }

        ctx->progress.lastUpdate = std::chrono::steady_clock::now();
    }

    spdlog::debug("C-MOVE progress: {}/{} received, {} failed, {} remaining",
                  ctx->progress.receivedImages,
                  ctx->progress.totalImages,
                  ctx->progress.failedImages,
                  ctx->progress.remainingImages);

    // Notify callback
    if (ctx->progressCallback) {
        std::lock_guard lock(ctx->progressMutex);
        ctx->progressCallback(ctx->progress);
    }
}

class DicomMoveSCU::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    std::expected<MoveResult, PacsErrorInfo> retrieveStudy(
        const PacsServerConfig& config,
        const MoveConfig& moveConfig,
        const std::string& studyInstanceUid,
        ProgressCallback progressCallback
    ) {
        return performMove(
            config,
            moveConfig,
            RetrieveLevel::Study,
            studyInstanceUid,
            "",  // seriesUid
            "",  // sopInstanceUid
            progressCallback
        );
    }

    std::expected<MoveResult, PacsErrorInfo> retrieveSeries(
        const PacsServerConfig& config,
        const MoveConfig& moveConfig,
        const std::string& studyInstanceUid,
        const std::string& seriesInstanceUid,
        ProgressCallback progressCallback
    ) {
        return performMove(
            config,
            moveConfig,
            RetrieveLevel::Series,
            studyInstanceUid,
            seriesInstanceUid,
            "",  // sopInstanceUid
            progressCallback
        );
    }

    std::expected<MoveResult, PacsErrorInfo> retrieveImage(
        const PacsServerConfig& config,
        const MoveConfig& moveConfig,
        const std::string& studyInstanceUid,
        const std::string& seriesInstanceUid,
        const std::string& sopInstanceUid,
        ProgressCallback progressCallback
    ) {
        return performMove(
            config,
            moveConfig,
            RetrieveLevel::Image,
            studyInstanceUid,
            seriesInstanceUid,
            sopInstanceUid,
            progressCallback
        );
    }

    void cancel() {
        cancelled_.store(true);
    }

    bool isRetrieving() const {
        return isRetrieving_.load();
    }

    std::optional<MoveProgress> currentProgress() const {
        std::lock_guard lock(progressMutex_);
        if (isRetrieving_.load()) {
            return currentProgress_;
        }
        return std::nullopt;
    }

private:
    std::atomic<bool> isRetrieving_{false};
    std::atomic<bool> cancelled_{false};
    mutable std::mutex progressMutex_;
    MoveProgress currentProgress_;

    std::expected<MoveResult, PacsErrorInfo> performMove(
        const PacsServerConfig& config,
        const MoveConfig& moveConfig,
        RetrieveLevel level,
        const std::string& studyUid,
        const std::string& seriesUid,
        const std::string& sopInstanceUid,
        ProgressCallback progressCallback
    ) {
        // Validate configuration
        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid PACS server configuration"
            });
        }

        if (moveConfig.storageDirectory.empty()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Storage directory not specified"
            });
        }

        // Check for concurrent operations
        if (isRetrieving_.exchange(true)) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "A retrieval is already in progress"
            });
        }

        cancelled_.store(false);

        // Create storage directory
        std::error_code ec;
        std::filesystem::create_directories(moveConfig.storageDirectory, ec);
        if (ec) {
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "Failed to create storage directory: " + ec.message()
            });
        }

        auto startTime = std::chrono::steady_clock::now();

        // Initialize callback context
        MoveCallbackContext ctx;
        ctx.cancelled = &cancelled_;
        ctx.progressCallback = progressCallback;
        ctx.moveConfig = moveConfig;
        ctx.studyUid = studyUid;
        ctx.seriesUid = seriesUid;

        // Initialize network
        T_ASC_Network* network = nullptr;
        OFCondition cond = ASC_initializeNetwork(
            NET_ACCEPTORREQUESTOR,  // Both requestor and acceptor for sub-operations
            moveConfig.storeScpPort,
            static_cast<int>(config.connectionTimeout.count()),
            &network
        );

        if (cond.bad()) {
            isRetrieving_.store(false);
            spdlog::error("Failed to initialize network: {}", cond.text());
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                std::string("Failed to initialize network: ") + cond.text()
            });
        }

        // Create association parameters
        T_ASC_Parameters* params = nullptr;
        cond = ASC_createAssociationParameters(&params, config.maxPduSize);
        if (cond.bad()) {
            ASC_dropNetwork(&network);
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to create association parameters: ") + cond.text()
            });
        }

        // Set AE titles
        ASC_setAPTitles(
            params,
            config.callingAeTitle.c_str(),
            config.calledAeTitle.c_str(),
            nullptr
        );

        // Set peer address
        std::string peerAddress = config.hostname + ":" + std::to_string(config.port);
        ASC_setPresentationAddresses(
            params,
            OFStandard::getHostName().c_str(),
            peerAddress.c_str()
        );

        // Add presentation context for Query/Retrieve MOVE
        const char* transferSyntaxes[] = {
            UID_LittleEndianExplicitTransferSyntax,
            UID_BigEndianExplicitTransferSyntax,
            UID_LittleEndianImplicitTransferSyntax
        };

        const char* moveSopClassUid = getMoveSopClassUid(moveConfig.queryRoot);

        cond = ASC_addPresentationContext(
            params,
            1,
            moveSopClassUid,
            transferSyntaxes,
            3
        );

        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to add presentation context: ") + cond.text()
            });
        }

        // Check for cancellation
        if (cancelled_.load()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        // Request association
        T_ASC_Association* assoc = nullptr;
        spdlog::info("Requesting C-MOVE association with {}:{} (AE: {})",
                     config.hostname, config.port, config.calledAeTitle);

        cond = ASC_requestAssociation(network, params, &assoc);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            isRetrieving_.store(false);

            if (cond == DUL_ASSOCIATIONREJECTED) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::AssociationRejected,
                    std::string("Association rejected: ") + cond.text()
                });
            }

            return std::unexpected(PacsErrorInfo{
                PacsError::ConnectionFailed,
                std::string("Failed to request association: ") + cond.text()
            });
        }

        // Check if MOVE SOP class was accepted
        T_ASC_PresentationContextID presId = ASC_findAcceptedPresentationContextID(
            assoc, moveSopClassUid
        );

        if (presId == 0) {
            ASC_releaseAssociation(assoc);
            ASC_destroyAssociation(&assoc);
            ASC_dropNetwork(&network);
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Query/Retrieve MOVE SOP Class was not accepted by the server"
            });
        }

        // Build move request dataset
        DcmDataset moveDataset;
        buildMoveDataset(&moveDataset, level, studyUid, seriesUid, sopInstanceUid);

        // Prepare C-MOVE request
        T_DIMSE_C_MoveRQ moveRequest;
        memset(&moveRequest, 0, sizeof(moveRequest));
        moveRequest.MessageID = assoc->nextMsgID++;
        strncpy(moveRequest.AffectedSOPClassUID, moveSopClassUid,
                sizeof(moveRequest.AffectedSOPClassUID) - 1);
        moveRequest.DataSetType = DIMSE_DATASET_PRESENT;
        moveRequest.Priority = DIMSE_PRIORITY_MEDIUM;

        // Set move destination (where PACS should send images via C-STORE)
        std::string moveDestination = moveConfig.moveDestinationAeTitle.value_or(
            config.callingAeTitle
        );
        strncpy(moveRequest.MoveDestination, moveDestination.c_str(),
                sizeof(moveRequest.MoveDestination) - 1);

        spdlog::info("Sending C-MOVE request (Message ID: {}, Destination: {})",
                     moveRequest.MessageID, moveDestination);

        T_DIMSE_C_MoveRSP moveResponse;
        DcmDataset* statusDetail = nullptr;
        int responseCount = 0;

        // Execute C-MOVE
        cond = DIMSE_moveUser(
            assoc,
            presId,
            &moveRequest,
            &moveDataset,
            moveProgressCallback,
            &ctx,
            DIMSE_BLOCKING,
            static_cast<int>(config.dimseTimeout.count()),
            network,
            moveSubOpCallback,
            &ctx,
            &moveResponse,
            &statusDetail,
            nullptr,  // rspCommandSet
            OFFalse   // ignoreStore
        );

        auto endTime = std::chrono::steady_clock::now();

        // Build result
        MoveResult result;
        result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );
        result.progress = ctx.progress;
        result.receivedFiles = std::move(ctx.receivedFiles);
        result.cancelled = cancelled_.load();

        // Clean up status detail
        if (statusDetail) {
            delete statusDetail;
        }

        // Release association
        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        ASC_dropNetwork(&network);

        isRetrieving_.store(false);

        if (cond.bad() && !cancelled_.load()) {
            if (cond == DIMSE_NODATAAVAILABLE || cond == DIMSE_READPDVFAILED) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::Timeout,
                    std::string("C-MOVE timeout: ") + cond.text()
                });
            }
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                std::string("C-MOVE failed: ") + cond.text()
            });
        }

        spdlog::info("C-MOVE completed: {} files received, {} failed (latency: {}ms)",
                     result.progress.receivedImages,
                     result.progress.failedImages,
                     result.latency.count());

        return result;
    }

    void buildMoveDataset(
        DcmDataset* dataset,
        RetrieveLevel level,
        const std::string& studyUid,
        const std::string& seriesUid,
        const std::string& sopInstanceUid
    ) {
        // Set Query/Retrieve Level
        dataset->putAndInsertString(DCM_QueryRetrieveLevel, retrieveLevelToString(level));

        // Study UID is always required
        dataset->putAndInsertString(DCM_StudyInstanceUID, studyUid.c_str());

        // Series UID for Series and Image level
        if (level >= RetrieveLevel::Series && !seriesUid.empty()) {
            dataset->putAndInsertString(DCM_SeriesInstanceUID, seriesUid.c_str());
        }

        // SOP Instance UID for Image level
        if (level == RetrieveLevel::Image && !sopInstanceUid.empty()) {
            dataset->putAndInsertString(DCM_SOPInstanceUID, sopInstanceUid.c_str());
        }
    }
};

// Public interface implementation

DicomMoveSCU::DicomMoveSCU()
    : impl_(std::make_unique<Impl>()) {
}

DicomMoveSCU::~DicomMoveSCU() = default;

DicomMoveSCU::DicomMoveSCU(DicomMoveSCU&&) noexcept = default;
DicomMoveSCU& DicomMoveSCU::operator=(DicomMoveSCU&&) noexcept = default;

std::expected<MoveResult, PacsErrorInfo> DicomMoveSCU::retrieveStudy(
    const PacsServerConfig& config,
    const MoveConfig& moveConfig,
    const std::string& studyInstanceUid,
    ProgressCallback progressCallback
) {
    return impl_->retrieveStudy(config, moveConfig, studyInstanceUid, progressCallback);
}

std::expected<MoveResult, PacsErrorInfo> DicomMoveSCU::retrieveSeries(
    const PacsServerConfig& config,
    const MoveConfig& moveConfig,
    const std::string& studyInstanceUid,
    const std::string& seriesInstanceUid,
    ProgressCallback progressCallback
) {
    return impl_->retrieveSeries(config, moveConfig, studyInstanceUid, seriesInstanceUid, progressCallback);
}

std::expected<MoveResult, PacsErrorInfo> DicomMoveSCU::retrieveImage(
    const PacsServerConfig& config,
    const MoveConfig& moveConfig,
    const std::string& studyInstanceUid,
    const std::string& seriesInstanceUid,
    const std::string& sopInstanceUid,
    ProgressCallback progressCallback
) {
    return impl_->retrieveImage(config, moveConfig, studyInstanceUid, seriesInstanceUid, sopInstanceUid, progressCallback);
}

void DicomMoveSCU::cancel() {
    impl_->cancel();
}

bool DicomMoveSCU::isRetrieving() const {
    return impl_->isRetrieving();
}

std::optional<MoveProgress> DicomMoveSCU::currentProgress() const {
    return impl_->currentProgress();
}

} // namespace dicom_viewer::services

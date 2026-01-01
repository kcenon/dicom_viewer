#include "services/dicom_echo_scu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

// DCMTK headers
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>

#include <spdlog/spdlog.h>

namespace dicom_viewer::services {

class DicomEchoSCU::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    std::expected<EchoResult, PacsErrorInfo> verify(const PacsServerConfig& config) {
        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid PACS server configuration"
            });
        }

        if (isVerifying_.exchange(true)) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "A verification is already in progress"
            });
        }

        cancelled_.store(false);
        auto result = performEcho(config);
        isVerifying_.store(false);

        return result;
    }

    void cancel() {
        cancelled_.store(true);
    }

    bool isVerifying() const {
        return isVerifying_.load();
    }

private:
    std::atomic<bool> isVerifying_{false};
    std::atomic<bool> cancelled_{false};

    std::expected<EchoResult, PacsErrorInfo> performEcho(const PacsServerConfig& config) {
        auto startTime = std::chrono::steady_clock::now();

        // Initialize network
        T_ASC_Network* network = nullptr;
        OFCondition cond = ASC_initializeNetwork(
            NET_REQUESTOR,
            0,  // Use any available port
            static_cast<int>(config.connectionTimeout.count()),
            &network
        );

        if (cond.bad()) {
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
            nullptr  // Application context name (use default)
        );

        // Set transport layer type and peer address
        std::string peerAddress = config.hostname + ":" + std::to_string(config.port);
        ASC_setPresentationAddresses(
            params,
            OFStandard::getHostName().c_str(),
            peerAddress.c_str()
        );

        // Add Verification SOP Class (C-ECHO)
        const char* transferSyntaxes[] = {
            UID_LittleEndianExplicitTransferSyntax,
            UID_BigEndianExplicitTransferSyntax,
            UID_LittleEndianImplicitTransferSyntax
        };

        cond = ASC_addPresentationContext(
            params,
            1,  // Presentation context ID (odd number)
            VERIFICATION_SOP_CLASS_UID,
            transferSyntaxes,
            3   // Number of transfer syntaxes
        );

        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to add presentation context: ") + cond.text()
            });
        }

        // Check for cancellation
        if (cancelled_.load()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        // Create association
        T_ASC_Association* assoc = nullptr;
        spdlog::info("Requesting association with {}:{} (AE: {})",
                     config.hostname, config.port, config.calledAeTitle);

        cond = ASC_requestAssociation(network, params, &assoc);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);

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

        // Check if Verification SOP Class was accepted
        T_ASC_PresentationContextID presId = ASC_findAcceptedPresentationContextID(
            assoc, VERIFICATION_SOP_CLASS_UID
        );

        if (presId == 0) {
            ASC_releaseAssociation(assoc);
            ASC_destroyAssociation(&assoc);
            ASC_dropNetwork(&network);
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Verification SOP Class was not accepted by the server"
            });
        }

        // Send C-ECHO request
        DIC_US msgId = assoc->nextMsgID++;
        DIC_US status = 0;

        spdlog::debug("Sending C-ECHO request (Message ID: {})", msgId);

        cond = DIMSE_echoUser(
            assoc,
            msgId,
            DIMSE_BLOCKING,
            static_cast<int>(config.dimseTimeout.count()),
            &status,
            nullptr  // No dataset
        );

        auto endTime = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );

        // Clean up association
        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        ASC_dropNetwork(&network);

        if (cond.bad()) {
            if (cond == DIMSE_NODATAAVAILABLE || cond == DIMSE_READPDVFAILED) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::Timeout,
                    std::string("C-ECHO timeout: ") + cond.text()
                });
            }
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                std::string("C-ECHO failed: ") + cond.text()
            });
        }

        // Check DIMSE status
        if (status != STATUS_Success) {
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "C-ECHO returned non-success status: " + std::to_string(status)
            });
        }

        spdlog::info("C-ECHO successful to {} (latency: {}ms)",
                     config.calledAeTitle, latency.count());

        return EchoResult{
            .success = true,
            .latency = latency,
            .message = "Echo successful"
        };
    }
};

// Public interface implementation

DicomEchoSCU::DicomEchoSCU()
    : impl_(std::make_unique<Impl>()) {
}

DicomEchoSCU::~DicomEchoSCU() = default;

DicomEchoSCU::DicomEchoSCU(DicomEchoSCU&&) noexcept = default;
DicomEchoSCU& DicomEchoSCU::operator=(DicomEchoSCU&&) noexcept = default;

std::expected<EchoResult, PacsErrorInfo> DicomEchoSCU::verify(const PacsServerConfig& config) {
    return impl_->verify(config);
}

void DicomEchoSCU::cancel() {
    impl_->cancel();
}

bool DicomEchoSCU::isVerifying() const {
    return impl_->isVerifying();
}

} // namespace dicom_viewer::services

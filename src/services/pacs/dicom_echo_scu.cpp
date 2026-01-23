#include "services/dicom_echo_scu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

#include <spdlog/spdlog.h>

// pacs_system headers
#include <pacs/core/result.hpp>
#include <pacs/network/association.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>

namespace dicom_viewer::services {

// pacs_system-based implementation
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
    uint16_t nextMessageId_{1};

    std::expected<EchoResult, PacsErrorInfo> performEcho(const PacsServerConfig& config) {
        auto startTime = std::chrono::steady_clock::now();

        // Build association configuration
        pacs::network::association_config assocConfig;
        assocConfig.calling_ae_title = config.callingAeTitle;
        assocConfig.called_ae_title = config.calledAeTitle;
        assocConfig.max_pdu_length = config.maxPduSize;

        // Add Verification SOP Class presentation context
        pacs::network::proposed_presentation_context verificationCtx;
        verificationCtx.id = 1;
        verificationCtx.abstract_syntax = VERIFICATION_SOP_CLASS_UID;
        verificationCtx.transfer_syntaxes = {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2.2",  // Explicit VR Big Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        };
        assocConfig.proposed_contexts.push_back(verificationCtx);

        // Check for cancellation before connection
        if (cancelled_.load()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        spdlog::info("Requesting association with {}:{} (AE: {})",
                     config.hostname, config.port, config.calledAeTitle);

        // Connect to remote SCP
        auto timeout = std::chrono::duration_cast<pacs::network::association::duration>(
            config.connectionTimeout
        );
        auto connectResult = pacs::network::association::connect(
            config.hostname,
            config.port,
            assocConfig,
            timeout
        );

        if (!connectResult.is_ok()) {
            const auto& err = connectResult.error();
            return mapAssociationError(err);
        }

        auto assoc = std::move(connectResult.value());

        // Check if Verification SOP Class was accepted
        if (!assoc.has_accepted_context(VERIFICATION_SOP_CLASS_UID)) {
            spdlog::error("Verification SOP Class was not accepted by the server");
            assoc.abort();
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Verification SOP Class was not accepted by the server"
            });
        }

        auto contextId = assoc.accepted_context_id(VERIFICATION_SOP_CLASS_UID);
        if (!contextId) {
            assoc.abort();
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "Failed to get accepted context ID"
            });
        }

        // Check for cancellation before sending echo
        if (cancelled_.load()) {
            assoc.abort();
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        // Create and send C-ECHO request
        uint16_t messageId = nextMessageId_++;
        auto echoRq = pacs::network::dimse::make_c_echo_rq(messageId, VERIFICATION_SOP_CLASS_UID);

        spdlog::debug("Sending C-ECHO request (Message ID: {})", messageId);

        auto sendResult = assoc.send_dimse(*contextId, echoRq);
        if (!sendResult.is_ok()) {
            spdlog::error("Failed to send C-ECHO request: {}", sendResult.error().message);
            assoc.abort();
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Failed to send C-ECHO request: " + sendResult.error().message
            });
        }

        // Receive C-ECHO response
        auto dimseTimeout = std::chrono::duration_cast<pacs::network::association::duration>(
            config.dimseTimeout
        );
        auto receiveResult = assoc.receive_dimse(dimseTimeout);
        if (!receiveResult.is_ok()) {
            const auto& err = receiveResult.error();
            spdlog::error("Failed to receive C-ECHO response: {}", err.message);
            assoc.abort();

            if (err.code == pacs::error_codes::receive_timeout) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::Timeout,
                    "C-ECHO timeout: " + err.message
                });
            }
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "C-ECHO failed: " + err.message
            });
        }

        auto [respContextId, respMsg] = std::move(receiveResult).value();

        // Check response status
        auto status = respMsg.status();
        if (status != pacs::network::dimse::status_success) {
            spdlog::error("C-ECHO returned non-success status: {}", static_cast<uint16_t>(status));
            (void)assoc.release();
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "C-ECHO returned non-success status: " + std::to_string(static_cast<uint16_t>(status))
            });
        }

        // Calculate latency
        auto endTime = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );

        // Release association gracefully
        auto releaseResult = assoc.release(dimseTimeout);
        if (!releaseResult.is_ok()) {
            spdlog::warn("Failed to release association gracefully: {}",
                         releaseResult.error().message);
        }

        spdlog::info("C-ECHO successful to {} (latency: {}ms)",
                     config.calledAeTitle, latency.count());

        return EchoResult{
            .success = true,
            .latency = latency,
            .message = "Echo successful"
        };
    }

    std::unexpected<PacsErrorInfo> mapAssociationError(const pacs::error_info& err) {
        int code = err.code;

        if (code == pacs::error_codes::connection_failed ||
            code == pacs::error_codes::connection_timeout) {
            spdlog::error("Connection failed: {}", err.message);
            return std::unexpected(PacsErrorInfo{
                PacsError::ConnectionFailed,
                "Failed to connect: " + err.message
            });
        }

        if (code == pacs::error_codes::association_rejected) {
            spdlog::error("Association rejected: {}", err.message);
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Association rejected: " + err.message
            });
        }

        if (code == pacs::error_codes::receive_timeout ||
            code == pacs::error_codes::connection_timeout) {
            spdlog::error("Connection timeout: {}", err.message);
            return std::unexpected(PacsErrorInfo{
                PacsError::Timeout,
                "Connection timeout: " + err.message
            });
        }

        spdlog::error("Network error: {}", err.message);
        return std::unexpected(PacsErrorInfo{
            PacsError::NetworkError,
            "Network error: " + err.message
        });
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

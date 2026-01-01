#pragma once

#include "pacs_config.hpp"

#include <chrono>
#include <expected>
#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Error types for DICOM network operations
 */
enum class PacsError {
    ConfigurationInvalid,
    ConnectionFailed,
    AssociationRejected,
    Timeout,
    NetworkError,
    AbortedByRemote,
    InternalError
};

/**
 * @brief Detailed error information for PACS operations
 */
struct PacsErrorInfo {
    PacsError code;
    std::string message;

    /**
     * @brief Get human-readable error description
     */
    [[nodiscard]] std::string toString() const {
        return "[" + codeToString(code) + "] " + message;
    }

    static std::string codeToString(PacsError code) {
        switch (code) {
            case PacsError::ConfigurationInvalid: return "ConfigurationInvalid";
            case PacsError::ConnectionFailed: return "ConnectionFailed";
            case PacsError::AssociationRejected: return "AssociationRejected";
            case PacsError::Timeout: return "Timeout";
            case PacsError::NetworkError: return "NetworkError";
            case PacsError::AbortedByRemote: return "AbortedByRemote";
            case PacsError::InternalError: return "InternalError";
        }
        return "Unknown";
    }
};

/**
 * @brief Result of a C-ECHO verification request
 */
struct EchoResult {
    /// Whether the echo was successful
    bool success = false;

    /// Round-trip latency of the echo request
    std::chrono::milliseconds latency{0};

    /// Server response message (if any)
    std::string message;
};

/**
 * @brief DICOM C-ECHO Service Class User (SCU)
 *
 * Implements the DICOM Verification SOP Class (1.2.840.10008.1.1)
 * for testing connectivity to PACS servers.
 *
 * @example
 * @code
 * DicomEchoSCU echo;
 * PacsServerConfig config;
 * config.hostname = "pacs.hospital.com";
 * config.port = 104;
 * config.calledAeTitle = "PACS_SERVER";
 *
 * auto result = echo.verify(config);
 * if (result) {
 *     std::cout << "Echo successful! Latency: "
 *               << result->latency.count() << "ms\n";
 * } else {
 *     std::cerr << "Echo failed: " << result.error().toString() << "\n";
 * }
 * @endcode
 *
 * @trace SRS-FR-034
 */
class DicomEchoSCU {
public:
    /// Verification SOP Class UID
    static constexpr const char* VERIFICATION_SOP_CLASS_UID = "1.2.840.10008.1.1";

    DicomEchoSCU();
    ~DicomEchoSCU();

    // Non-copyable, movable
    DicomEchoSCU(const DicomEchoSCU&) = delete;
    DicomEchoSCU& operator=(const DicomEchoSCU&) = delete;
    DicomEchoSCU(DicomEchoSCU&&) noexcept;
    DicomEchoSCU& operator=(DicomEchoSCU&&) noexcept;

    /**
     * @brief Verify connectivity to a PACS server using C-ECHO
     *
     * Establishes a DICOM association with the server and sends
     * a C-ECHO request to verify the connection.
     *
     * @param config Server configuration
     * @return EchoResult on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<EchoResult, PacsErrorInfo>
    verify(const PacsServerConfig& config);

    /**
     * @brief Cancel any ongoing verification request
     *
     * Thread-safe method to abort current operation.
     */
    void cancel();

    /**
     * @brief Check if a verification is currently in progress
     */
    [[nodiscard]] bool isVerifying() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

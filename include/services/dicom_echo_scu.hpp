// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file dicom_echo_scu.hpp
 * @brief DICOM C-ECHO Service Class User for connection verification
 * @details Implements the DICOM C-ECHO SCU operation to verify connectivity
 *          and association negotiation with remote PACS servers.
 *          Uses the kcenon pacs_system library for network operations
 *          with configurable timeout and error reporting.
 *
 * ## Thread Safety
 * - Echo operations perform network I/O and should not block the UI thread
 * - Each operation creates its own network association
 * - PacsError results are safe to inspect from any thread
 *
 * @author kcenon
 * @since 1.0.0
 */

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

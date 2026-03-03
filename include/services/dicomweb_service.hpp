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
 * @file dicomweb_service.hpp
 * @brief DICOMweb REST API service for WADO-RS, STOW-RS, and QIDO-RS
 * @details Wraps pacs_system's rest_server to provide a viewer-level
 *          DICOMweb service supporting DICOM PS3.18 Web Services.
 *          Manages server lifecycle (start/stop) and configuration.
 *
 * ## Thread Safety
 * - Server lifecycle methods (start/stop) should be called from one thread
 * - isRunning() and getConfig() are safe to call from any thread
 * - The underlying REST server handles concurrent HTTP requests internally
 *
 * ## Supported Endpoints (provided by pacs::web::rest_server)
 * - WADO-RS: GET /studies/{studyUID} — Retrieve DICOM objects via REST
 * - STOW-RS: POST /studies — Store DICOM objects via multipart POST
 * - QIDO-RS: GET /studies?... — Query DICOM objects via REST
 * - WADO-URI: GET /wado — Legacy WADO-URI endpoint
 * - System: GET /api/v1/system/status — Health and metrics
 *
 * @see DICOM PS3.18 — Web Services
 * @see IHE RAD TF — Web-based Image Access (WIA)
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "dicom_echo_scu.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Configuration for the DICOMweb REST API service
 */
struct DicomWebConfig {
    /// Master enable/disable for DICOMweb service
    bool enabled = false;

    /// Address to bind the server to
    std::string bindAddress = "0.0.0.0";

    /// Port to listen on (default: 8080)
    uint16_t port = 8080;

    /// Number of worker threads for handling requests
    size_t concurrency = 4;

    /// Enable CORS (Cross-Origin Resource Sharing)
    bool enableCors = true;

    /// CORS allowed origins (empty = allow all)
    std::string corsAllowedOrigins = "*";

    /// Enable TLS/SSL encryption
    bool enableTls = false;

    /// Path to TLS certificate file
    std::string tlsCertPath;

    /// Path to TLS private key file
    std::string tlsKeyPath;

    /// Request timeout in seconds
    uint32_t requestTimeoutSeconds = 30;

    /// Maximum request body size in bytes (default 10MB)
    size_t maxBodySize = 10 * 1024 * 1024;

    /**
     * @brief Validate the configuration
     * @return true if configuration is valid for use
     */
    [[nodiscard]] bool isValid() const noexcept {
        if (!enabled) return true;
        return port > 0 && concurrency > 0;
    }
};

/**
 * @brief DICOMweb REST API service
 *
 * Provides a viewer-level API for managing an embedded HTTP server
 * that serves DICOMweb endpoints (WADO-RS, STOW-RS, QIDO-RS).
 * Wraps pacs_system's rest_server with viewer-specific configuration.
 *
 * @example
 * @code
 * DicomWebService web;
 * DicomWebConfig config;
 * config.enabled = true;
 * config.port = 8080;
 * config.enableCors = true;
 *
 * auto result = web.configure(config);
 * if (result) {
 *     auto startResult = web.start();
 *     if (startResult) {
 *         // Server is running on port 8080
 *         // WADO-RS: http://localhost:8080/studies/{uid}
 *         // QIDO-RS: http://localhost:8080/studies?PatientName=...
 *     }
 * }
 * @endcode
 *
 * @trace SRS-FR-041
 */
class DicomWebService {
public:
    DicomWebService();
    ~DicomWebService();

    // Non-copyable, movable
    DicomWebService(const DicomWebService&) = delete;
    DicomWebService& operator=(const DicomWebService&) = delete;
    DicomWebService(DicomWebService&&) noexcept;
    DicomWebService& operator=(DicomWebService&&) noexcept;

    /**
     * @brief Configure the DICOMweb service
     *
     * Sets up the underlying REST server configuration. The server
     * is not started automatically — call start() after configuring.
     *
     * @param config DICOMweb configuration
     * @return void on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<void, PacsErrorInfo> configure(
        const DicomWebConfig& config);

    /**
     * @brief Start the DICOMweb server (non-blocking)
     *
     * Starts the HTTP server in a background thread. The server will
     * begin accepting connections immediately.
     *
     * @return void on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<void, PacsErrorInfo> start();

    /**
     * @brief Stop the DICOMweb server
     *
     * Gracefully shuts down the HTTP server. Safe to call multiple times
     * or when the server is not running.
     */
    void stop();

    /**
     * @brief Check if the server is currently running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get the actual port the server is listening on
     * @return Port number, or 0 if not running
     */
    [[nodiscard]] uint16_t actualPort() const;

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] DicomWebConfig getConfig() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

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
 * @file dicom_store_scp.hpp
 * @brief DICOM C-STORE Service Class Provider for image reception
 * @details Implements a DICOM C-STORE SCP server that accepts incoming
 *          image storage requests from remote DICOM nodes. Uses
 *          std::atomic for server state management and the kcenon
 *          pacs_system library for DICOM network handling.
 *
 * ## Thread Safety
 * - Server runs on its own network thread via pacs_system
 * - Start/stop operations use atomic state transitions
 * - Received images are written with exclusive file access
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "dicom_echo_scu.hpp"
#include "pacs_config.hpp"

#include <atomic>
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Configuration for Storage SCP server
 *
 * @trace SRS-FR-037
 */
struct StorageScpConfig {
    /// Port number to listen on
    uint16_t port = 11112;

    /// AE Title for this SCP
    std::string aeTitle = "DICOM_VIEWER_SCP";

    /// Storage directory for received DICOM files
    std::filesystem::path storageDirectory;

    /// Maximum PDU size
    uint32_t maxPduSize = 16384;

    /// Connection timeout in seconds
    std::chrono::seconds connectionTimeout{30};

    /// Maximum concurrent associations
    uint32_t maxAssociations = 10;

    /**
     * @brief Validate the configuration
     * @return true if configuration is valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return port > 0 &&
               !aeTitle.empty() &&
               aeTitle.length() <= 16 &&
               !storageDirectory.empty();
    }
};

/**
 * @brief Information about a received DICOM image
 */
struct ReceivedImageInfo {
    /// Full path to the stored file
    std::filesystem::path filePath;

    /// SOP Class UID
    std::string sopClassUid;

    /// SOP Instance UID
    std::string sopInstanceUid;

    /// Patient ID (if available)
    std::string patientId;

    /// Study Instance UID
    std::string studyInstanceUid;

    /// Series Instance UID
    std::string seriesInstanceUid;

    /// Calling AE Title (sender)
    std::string callingAeTitle;

    /// Timestamp when received
    std::chrono::system_clock::time_point receivedTime;
};

/**
 * @brief Status information for the Storage SCP
 */
struct StorageScpStatus {
    /// Whether the server is running
    bool isRunning = false;

    /// Port number the server is listening on
    uint16_t port = 0;

    /// Total number of images received
    uint64_t totalImagesReceived = 0;

    /// Number of active connections
    uint32_t activeConnections = 0;

    /// Server start time
    std::chrono::system_clock::time_point startTime;
};

/**
 * @brief DICOM Storage Service Class Provider (SCP)
 *
 * Implements the Storage SCP to receive DICOM images transmitted
 * from external sources (C-STORE operations).
 *
 * Supported SOP Classes:
 * - CT Image Storage (1.2.840.10008.5.1.4.1.1.2)
 * - MR Image Storage (1.2.840.10008.5.1.4.1.1.4)
 * - Secondary Capture Image Storage (1.2.840.10008.5.1.4.1.1.7)
 * - Enhanced CT Image Storage (1.2.840.10008.5.1.4.1.1.2.1)
 * - Enhanced MR Image Storage (1.2.840.10008.5.1.4.1.1.4.1)
 *
 * @example
 * @code
 * DicomStoreSCP scp;
 * StorageScpConfig config;
 * config.port = 11112;
 * config.aeTitle = "MY_SCP";
 * config.storageDirectory = "/dicom/incoming";
 *
 * scp.setImageReceivedCallback([](const ReceivedImageInfo& info) {
 *     std::cout << "Received: " << info.filePath << "\n";
 * });
 *
 * auto result = scp.start(config);
 * if (!result) {
 *     std::cerr << "Failed to start: " << result.error().toString() << "\n";
 * }
 * @endcode
 *
 * @trace SRS-FR-037
 */
class DicomStoreSCP {
public:
    /// Callback type for image received notification
    using ImageReceivedCallback = std::function<void(const ReceivedImageInfo&)>;

    /// Callback type for connection events
    using ConnectionCallback = std::function<void(const std::string& callingAeTitle, bool connected)>;

    // Standard SOP Class UIDs
    static constexpr const char* CT_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.2";
    static constexpr const char* MR_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.4";
    static constexpr const char* SECONDARY_CAPTURE_STORAGE = "1.2.840.10008.5.1.4.1.1.7";
    static constexpr const char* ENHANCED_CT_STORAGE = "1.2.840.10008.5.1.4.1.1.2.1";
    static constexpr const char* ENHANCED_MR_STORAGE = "1.2.840.10008.5.1.4.1.1.4.1";

    DicomStoreSCP();
    ~DicomStoreSCP();

    // Non-copyable, movable
    DicomStoreSCP(const DicomStoreSCP&) = delete;
    DicomStoreSCP& operator=(const DicomStoreSCP&) = delete;
    DicomStoreSCP(DicomStoreSCP&&) noexcept;
    DicomStoreSCP& operator=(DicomStoreSCP&&) noexcept;

    /**
     * @brief Start the Storage SCP server
     *
     * Begins listening for incoming DICOM associations on the configured port.
     * The server runs in a background thread and accepts connections until
     * stop() is called.
     *
     * @param config Server configuration
     * @return void on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<void, PacsErrorInfo> start(const StorageScpConfig& config);

    /**
     * @brief Stop the Storage SCP server
     *
     * Gracefully stops the server and closes all active connections.
     */
    void stop();

    /**
     * @brief Check if the server is running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get current server status
     */
    [[nodiscard]] StorageScpStatus getStatus() const;

    /**
     * @brief Set callback for image received events
     *
     * The callback will be invoked from the server thread when an image
     * is successfully received and stored.
     *
     * @param callback Function to call when an image is received
     */
    void setImageReceivedCallback(ImageReceivedCallback callback);

    /**
     * @brief Set callback for connection events
     *
     * @param callback Function to call when a connection is established or closed
     */
    void setConnectionCallback(ConnectionCallback callback);

    /**
     * @brief Get list of supported SOP Class UIDs
     */
    [[nodiscard]] static std::vector<std::string> getSupportedSopClasses();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

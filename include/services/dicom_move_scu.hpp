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

#pragma once

#include "pacs_config.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declare PacsErrorInfo from dicom_echo_scu.hpp
namespace dicom_viewer::services {
struct PacsErrorInfo;
enum class QueryRoot;
}

namespace dicom_viewer::services {

/**
 * @brief Retrieval level for C-MOVE operations
 */
enum class RetrieveLevel {
    Study,   ///< Retrieve entire study
    Series,  ///< Retrieve specific series
    Image    ///< Retrieve specific image (instance)
};

/**
 * @brief Progress information for C-MOVE operations
 */
struct MoveProgress {
    /// Total number of images to be transferred
    int32_t totalImages = 0;

    /// Number of images successfully received
    int32_t receivedImages = 0;

    /// Number of images that failed to transfer
    int32_t failedImages = 0;

    /// Number of images with warnings
    int32_t warningImages = 0;

    /// Number of remaining images
    int32_t remainingImages = 0;

    /// Current Study Instance UID being processed
    std::string currentStudyUid;

    /// Current Series Instance UID being processed
    std::string currentSeriesUid;

    /// Timestamp of last progress update
    std::chrono::steady_clock::time_point lastUpdate;

    /**
     * @brief Check if transfer is complete
     */
    [[nodiscard]] bool isComplete() const noexcept {
        return remainingImages == 0 && totalImages > 0;
    }

    /**
     * @brief Get completion percentage (0-100)
     */
    [[nodiscard]] float percentComplete() const noexcept {
        if (totalImages == 0) return 0.0f;
        return static_cast<float>(receivedImages + failedImages) /
               static_cast<float>(totalImages) * 100.0f;
    }
};

/**
 * @brief Result of a C-MOVE retrieval operation
 */
struct MoveResult {
    /// Total operation latency
    std::chrono::milliseconds latency{0};

    /// Final progress state
    MoveProgress progress;

    /// Paths to successfully received files
    std::vector<std::filesystem::path> receivedFiles;

    /// Whether the operation was cancelled
    bool cancelled = false;

    /**
     * @brief Check if all images were successfully retrieved
     */
    [[nodiscard]] bool isSuccess() const noexcept {
        return !cancelled &&
               progress.failedImages == 0 &&
               progress.receivedImages == progress.totalImages &&
               progress.totalImages > 0;
    }

    /**
     * @brief Check if there were any failures
     */
    [[nodiscard]] bool hasFailures() const noexcept {
        return progress.failedImages > 0;
    }
};

/**
 * @brief Configuration for C-MOVE SCU operations
 */
struct MoveConfig {
    /// Query/Retrieve root (Patient or Study)
    QueryRoot queryRoot;

    /// Directory to store received files
    std::filesystem::path storageDirectory;

    /// AE Title for receiving C-STORE (defaults to calling AE title)
    std::optional<std::string> moveDestinationAeTitle;

    /// Port for C-STORE SCP (0 = use same association for sub-operations)
    uint16_t storeScpPort = 0;

    /// Maximum concurrent sub-operations
    int32_t maxConcurrentOperations = 1;

    /// Whether to create subdirectories based on Study/Series UIDs
    bool createSubdirectories = true;

    /// Whether to use the original SOP Instance UID as filename
    bool useOriginalFilenames = true;
};

/**
 * @brief DICOM C-MOVE Service Class User (SCU)
 *
 * Implements the DICOM Query/Retrieve Service Classes for retrieving
 * images from PACS servers using the C-MOVE protocol.
 *
 * Supports:
 * - Patient Root Query/Retrieve Information Model - MOVE (1.2.840.10008.5.1.4.1.2.1.2)
 * - Study Root Query/Retrieve Information Model - MOVE (1.2.840.10008.5.1.4.1.2.2.2)
 *
 * @note C-MOVE requires a C-STORE SCP to receive images. This implementation
 *       can either use the same association for sub-operations or start an
 *       internal C-STORE SCP on the specified port.
 *
 * @example
 * @code
 * DicomMoveSCU mover;
 * PacsServerConfig config;
 * config.hostname = "pacs.hospital.com";
 * config.port = 104;
 * config.calledAeTitle = "PACS_SERVER";
 *
 * MoveConfig moveConfig;
 * moveConfig.storageDirectory = "/tmp/dicom";
 * moveConfig.queryRoot = QueryRoot::StudyRoot;
 *
 * auto result = mover.retrieveStudy(config, moveConfig, studyUid,
 *     [](const MoveProgress& progress) {
 *         std::cout << "Progress: " << progress.percentComplete() << "%\n";
 *     });
 *
 * if (result) {
 *     std::cout << "Retrieved " << result->receivedFiles.size() << " files\n";
 * } else {
 *     std::cerr << "Retrieval failed: " << result.error().toString() << "\n";
 * }
 * @endcode
 *
 * @trace SRS-FR-036
 */
class DicomMoveSCU {
public:
    /// Patient Root Query/Retrieve Information Model - MOVE SOP Class UID
    static constexpr const char* PATIENT_ROOT_MOVE_SOP_CLASS_UID =
        "1.2.840.10008.5.1.4.1.2.1.2";

    /// Study Root Query/Retrieve Information Model - MOVE SOP Class UID
    static constexpr const char* STUDY_ROOT_MOVE_SOP_CLASS_UID =
        "1.2.840.10008.5.1.4.1.2.2.2";

    /// Progress callback type
    using ProgressCallback = std::function<void(const MoveProgress&)>;

    DicomMoveSCU();
    ~DicomMoveSCU();

    // Non-copyable, movable
    DicomMoveSCU(const DicomMoveSCU&) = delete;
    DicomMoveSCU& operator=(const DicomMoveSCU&) = delete;
    DicomMoveSCU(DicomMoveSCU&&) noexcept;
    DicomMoveSCU& operator=(DicomMoveSCU&&) noexcept;

    /**
     * @brief Retrieve an entire study from PACS
     *
     * Initiates a C-MOVE request to retrieve all images belonging
     * to the specified study.
     *
     * @param config PACS server configuration
     * @param moveConfig Move operation configuration
     * @param studyInstanceUid Study Instance UID to retrieve
     * @param progressCallback Optional callback for progress updates
     * @return MoveResult on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<MoveResult, PacsErrorInfo>
    retrieveStudy(
        const PacsServerConfig& config,
        const MoveConfig& moveConfig,
        const std::string& studyInstanceUid,
        ProgressCallback progressCallback = nullptr
    );

    /**
     * @brief Retrieve a specific series from PACS
     *
     * Initiates a C-MOVE request to retrieve all images belonging
     * to the specified series.
     *
     * @param config PACS server configuration
     * @param moveConfig Move operation configuration
     * @param studyInstanceUid Study Instance UID containing the series
     * @param seriesInstanceUid Series Instance UID to retrieve
     * @param progressCallback Optional callback for progress updates
     * @return MoveResult on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<MoveResult, PacsErrorInfo>
    retrieveSeries(
        const PacsServerConfig& config,
        const MoveConfig& moveConfig,
        const std::string& studyInstanceUid,
        const std::string& seriesInstanceUid,
        ProgressCallback progressCallback = nullptr
    );

    /**
     * @brief Retrieve a specific image from PACS
     *
     * Initiates a C-MOVE request to retrieve a single image.
     *
     * @param config PACS server configuration
     * @param moveConfig Move operation configuration
     * @param studyInstanceUid Study Instance UID containing the image
     * @param seriesInstanceUid Series Instance UID containing the image
     * @param sopInstanceUid SOP Instance UID of the image to retrieve
     * @param progressCallback Optional callback for progress updates
     * @return MoveResult on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<MoveResult, PacsErrorInfo>
    retrieveImage(
        const PacsServerConfig& config,
        const MoveConfig& moveConfig,
        const std::string& studyInstanceUid,
        const std::string& seriesInstanceUid,
        const std::string& sopInstanceUid,
        ProgressCallback progressCallback = nullptr
    );

    /**
     * @brief Cancel any ongoing retrieval operation
     *
     * Thread-safe method to abort current operation.
     * The operation will complete with cancelled=true in the result.
     */
    void cancel();

    /**
     * @brief Check if a retrieval is currently in progress
     */
    [[nodiscard]] bool isRetrieving() const;

    /**
     * @brief Get current progress of ongoing retrieval
     *
     * @return Current progress, or nullopt if no retrieval in progress
     */
    [[nodiscard]] std::optional<MoveProgress> currentProgress() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

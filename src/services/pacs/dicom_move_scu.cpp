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

#include "services/dicom_move_scu.hpp"
#include "services/dicom_echo_scu.hpp"
#include "services/dicom_find_scu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>

// pacs_system headers
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>
#include <pacs/network/association.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/services/retrieve_scu.hpp>
#include <pacs/encoding/vr_type.hpp>

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

/**
 * @brief Helper to extract string from pacs_system dataset
 */
std::string getStringFromDataset(const pacs::core::dicom_dataset& dataset,
                                  pacs::core::dicom_tag tag) {
    return dataset.get_string(tag, "");
}

} // anonymous namespace

// pacs_system-based implementation
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

        // Build association configuration
        pacs::network::association_config assocConfig;
        assocConfig.calling_ae_title = config.callingAeTitle;
        assocConfig.called_ae_title = config.calledAeTitle;
        assocConfig.max_pdu_length = config.maxPduSize;

        // Get MOVE SOP Class UID based on query root
        const char* moveSopClassUid = getMoveSopClassUid(moveConfig.queryRoot);

        // Add presentation context for Query/Retrieve MOVE
        pacs::network::proposed_presentation_context moveCtx;
        moveCtx.id = 1;
        moveCtx.abstract_syntax = moveSopClassUid;
        moveCtx.transfer_syntaxes = {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2.2",  // Explicit VR Big Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        };
        assocConfig.proposed_contexts.push_back(moveCtx);

        // Check for cancellation before connection
        if (cancelled_.load()) {
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        spdlog::info("Requesting C-MOVE association with {}:{} (AE: {})",
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
            isRetrieving_.store(false);
            const auto& err = connectResult.error();
            return mapAssociationError(err);
        }

        auto assoc = std::move(connectResult.value());

        // Check if Query/Retrieve MOVE SOP Class was accepted
        if (!assoc.has_accepted_context(moveSopClassUid)) {
            spdlog::error("Query/Retrieve MOVE SOP Class was not accepted by the server");
            assoc.abort();
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Query/Retrieve MOVE SOP Class was not accepted by the server"
            });
        }

        // Check for cancellation before sending request
        if (cancelled_.load()) {
            assoc.abort();
            isRetrieving_.store(false);
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        // Build query/move dataset
        pacs::core::dicom_dataset queryDataset;
        buildMoveDataset(queryDataset, level, studyUid, seriesUid, sopInstanceUid);

        // Determine move destination
        std::string moveDestination = moveConfig.moveDestinationAeTitle.value_or(
            config.callingAeTitle
        );

        spdlog::info("Sending C-MOVE request (Destination: {})", moveDestination);

        // Configure retrieve_scu
        pacs::services::retrieve_scu_config scuConfig;
        scuConfig.mode = pacs::services::retrieve_mode::c_move;
        scuConfig.model = (moveConfig.queryRoot == QueryRoot::PatientRoot)
            ? pacs::services::query_model::patient_root
            : pacs::services::query_model::study_root;

        switch (level) {
            case RetrieveLevel::Study:
                scuConfig.level = pacs::services::query_level::study;
                break;
            case RetrieveLevel::Series:
                scuConfig.level = pacs::services::query_level::series;
                break;
            case RetrieveLevel::Image:
                scuConfig.level = pacs::services::query_level::image;
                break;
        }
        scuConfig.move_destination = moveDestination;
        scuConfig.timeout = config.dimseTimeout;

        pacs::services::retrieve_scu scu(scuConfig);

        // Track progress and received files
        MoveResult moveResult;
        moveResult.cancelled = false;
        size_t fileIndex = 0;

        // Execute C-MOVE with progress callback
        auto retrieveResult = scu.move(
            assoc,
            queryDataset,
            moveDestination,
            [this, &moveResult, &progressCallback, &cancelled = cancelled_](
                const pacs::services::retrieve_progress& p
            ) {
                if (cancelled.load()) {
                    spdlog::debug("C-MOVE cancelled");
                    return;
                }

                // Update progress
                {
                    std::lock_guard lock(progressMutex_);
                    currentProgress_.remainingImages = p.remaining;
                    currentProgress_.receivedImages = p.completed;
                    currentProgress_.failedImages = p.failed;
                    currentProgress_.warningImages = p.warning;

                    if (currentProgress_.totalImages == 0 && p.total() > 0) {
                        currentProgress_.totalImages = p.total();
                    }

                    currentProgress_.lastUpdate = std::chrono::steady_clock::now();
                    moveResult.progress = currentProgress_;
                }

                spdlog::debug("C-MOVE progress: {}/{} received, {} failed, {} remaining",
                              p.completed, p.total(), p.failed, p.remaining);

                // Notify user callback
                if (progressCallback) {
                    std::lock_guard lock(progressMutex_);
                    progressCallback(currentProgress_);
                }
            }
        );

        auto endTime = std::chrono::steady_clock::now();
        moveResult.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );

        // Release association gracefully
        auto dimseTimeout = std::chrono::duration_cast<pacs::network::association::duration>(
            config.dimseTimeout
        );
        auto releaseResult = assoc.release(dimseTimeout);
        if (!releaseResult.is_ok()) {
            spdlog::warn("Failed to release association gracefully: {}",
                         releaseResult.error().message);
        }

        isRetrieving_.store(false);

        if (!retrieveResult.is_ok()) {
            const auto& err = retrieveResult.error();
            spdlog::error("C-MOVE failed: {}", err.message);

            if (cancelled_.load()) {
                moveResult.cancelled = true;
                return moveResult;  // Return partial results on cancellation
            }

            if (err.code == pacs::error_codes::receive_timeout) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::Timeout,
                    "C-MOVE timeout: " + err.message
                });
            }

            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "C-MOVE failed: " + err.message
            });
        }

        // Extract final result from retrieve_result
        const auto& result = retrieveResult.value();
        moveResult.progress.receivedImages = result.completed;
        moveResult.progress.failedImages = result.failed;
        moveResult.progress.warningImages = result.warning;
        moveResult.cancelled = result.is_cancelled();

        // Note: For C-MOVE, received instances are handled by the Storage SCP
        // The retrieve_result.received_instances is only populated for C-GET mode
        // In C-MOVE mode, files are received by a separate Storage SCP process

        spdlog::info("C-MOVE completed: {} images completed, {} failed (latency: {}ms)",
                     result.completed, result.failed, moveResult.latency.count());

        return moveResult;
    }

    void buildMoveDataset(
        pacs::core::dicom_dataset& dataset,
        RetrieveLevel level,
        const std::string& studyUid,
        const std::string& seriesUid,
        const std::string& sopInstanceUid
    ) {
        using namespace pacs::core;
        using vr = pacs::encoding::vr_type;

        // Set Query/Retrieve Level
        dataset.set_string(tags::query_retrieve_level, vr::CS, retrieveLevelToString(level));

        // Study UID is always required
        dataset.set_string(tags::study_instance_uid, vr::UI, studyUid);

        // Series UID for Series and Image level
        if (level >= RetrieveLevel::Series && !seriesUid.empty()) {
            dataset.set_string(tags::series_instance_uid, vr::UI, seriesUid);
        }

        // SOP Instance UID for Image level
        if (level == RetrieveLevel::Image && !sopInstanceUid.empty()) {
            dataset.set_string(tags::sop_instance_uid, vr::UI, sopInstanceUid);
        }
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

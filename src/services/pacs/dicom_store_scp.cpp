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

#include "services/dicom_store_scp.hpp"

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
#include <pacs/network/dicom_server.hpp>
#include <pacs/network/server_config.hpp>
#include <pacs/services/storage_scp.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/encoding/vr_type.hpp>
#include <pacs/encoding/transfer_syntax.hpp>

namespace dicom_viewer::services {

// pacs_system-based implementation
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

        // Configure server
        pacs::network::server_config serverConfig;
        serverConfig.ae_title = config_.aeTitle;
        serverConfig.port = config_.port;
        serverConfig.max_pdu_size = config_.maxPduSize;
        serverConfig.idle_timeout = config_.connectionTimeout;
        serverConfig.max_associations = config_.maxAssociations;

        // Create server
        server_ = std::make_unique<pacs::network::dicom_server>(serverConfig);

        // Create and configure storage SCP service
        pacs::services::storage_scp_config scpConfig;
        scpConfig.accepted_sop_classes = DicomStoreSCP::getSupportedSopClasses();
        scpConfig.dup_policy = pacs::services::duplicate_policy::replace;

        storageScp_ = std::make_unique<pacs::services::storage_scp>(scpConfig);

        // Set storage handler
        storageScp_->set_handler(
            [this](const pacs::core::dicom_dataset& dataset,
                   const std::string& callingAe,
                   const std::string& sopClassUid,
                   const std::string& sopInstanceUid) {
                return handleStoreRequest(dataset, callingAe, sopClassUid, sopInstanceUid);
            }
        );

        // Set post-store handler for callbacks
        storageScp_->set_post_store_handler(
            [this](const pacs::core::dicom_dataset& dataset,
                   const std::string& patientId,
                   const std::string& studyUid,
                   const std::string& seriesUid,
                   const std::string& sopInstanceUid) {
                handlePostStore(dataset, patientId, studyUid, seriesUid, sopInstanceUid);
            }
        );

        // Create verification SCP for C-ECHO support
        verificationScp_ = std::make_unique<pacs::services::verification_scp>();

        // Register services with server
        server_->register_service(std::move(storageScp_));
        server_->register_service(std::move(verificationScp_));

        // Set connection callbacks
        server_->on_association_established([this](const pacs::network::association& assoc) {
            std::string callingAe{assoc.calling_ae()};
            spdlog::info("Association request from: {}", callingAe);

            {
                std::lock_guard lock(callbackMutex_);
                if (connectionCallback_) {
                    connectionCallback_(callingAe, true);
                }
            }

            {
                std::lock_guard lock(statusMutex_);
                status_.activeConnections++;
            }
        });

        server_->on_association_released([this](const pacs::network::association& assoc) {
            std::string callingAe{assoc.calling_ae()};

            {
                std::lock_guard lock(statusMutex_);
                if (status_.activeConnections > 0) {
                    status_.activeConnections--;
                }
            }

            {
                std::lock_guard lock(callbackMutex_);
                if (connectionCallback_) {
                    connectionCallback_(callingAe, false);
                }
            }
        });

        server_->on_error([](const std::string& error) {
            spdlog::error("Storage SCP error: {}", error);
        });

        // Start server
        auto result = server_->start();
        if (!result.is_ok()) {
            spdlog::error("Failed to start server: {}", result.error().message);
            server_.reset();
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Failed to start server: " + result.error().message
            });
        }

        // Update status
        {
            std::lock_guard lock(statusMutex_);
            status_.isRunning = true;
            status_.port = config_.port;
            status_.startTime = std::chrono::system_clock::now();
            status_.totalImagesReceived = 0;
            status_.activeConnections = 0;
        }

        spdlog::info("Storage SCP started on port {} (AE: {})",
                     config_.port, config_.aeTitle);

        isRunning_.store(true);
        return {};
    }

    void stop() {
        if (!isRunning_.exchange(false)) {
            return;
        }

        if (server_) {
            server_->stop();
            server_.reset();
        }

        {
            std::lock_guard lock(statusMutex_);
            status_.isRunning = false;
        }

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
    std::unique_ptr<pacs::network::dicom_server> server_;
    std::unique_ptr<pacs::services::storage_scp> storageScp_;
    std::unique_ptr<pacs::services::verification_scp> verificationScp_;
    std::atomic<bool> isRunning_{false};

    mutable std::mutex statusMutex_;
    StorageScpStatus status_;

    mutable std::mutex callbackMutex_;
    ImageReceivedCallback imageReceivedCallback_;
    ConnectionCallback connectionCallback_;

    // Last received image info for post-store handler
    mutable std::mutex lastImageMutex_;
    std::string lastCallingAe_;
    std::filesystem::path lastFilePath_;

    pacs::services::storage_status handleStoreRequest(
        const pacs::core::dicom_dataset& dataset,
        const std::string& callingAe,
        const std::string& sopClassUid,
        const std::string& sopInstanceUid
    ) {
        // Prepare file path
        std::filesystem::path filePath = config_.storageDirectory /
            (sopInstanceUid + ".dcm");

        // Create DICOM file from dataset using default transfer syntax
        auto file = pacs::core::dicom_file::create(
            pacs::core::dicom_dataset{dataset},
            pacs::encoding::transfer_syntax::explicit_vr_little_endian
        );

        auto saveResult = file.save(filePath);
        if (!saveResult.is_ok()) {
            spdlog::error("Failed to save file: {}", saveResult.error().message);
            return pacs::services::storage_status::out_of_resources_unable_to_store;
        }

        spdlog::info("Stored image: {}", filePath.string());

        // Store info for post-store handler
        {
            std::lock_guard lock(lastImageMutex_);
            lastCallingAe_ = callingAe;
            lastFilePath_ = filePath;
        }

        return pacs::services::storage_status::success;
    }

    void handlePostStore(
        const pacs::core::dicom_dataset& dataset,
        const std::string& patientId,
        const std::string& studyUid,
        const std::string& seriesUid,
        const std::string& sopInstanceUid
    ) {
        using namespace pacs::core;

        // Get stored info
        std::string callingAe;
        std::filesystem::path filePath;
        {
            std::lock_guard lock(lastImageMutex_);
            callingAe = lastCallingAe_;
            filePath = lastFilePath_;
        }

        // Build received image info
        ReceivedImageInfo info;
        info.filePath = filePath;
        info.sopClassUid = dataset.get_string(tags::sop_class_uid, "");
        info.sopInstanceUid = sopInstanceUid;
        info.patientId = patientId;
        info.studyInstanceUid = studyUid;
        info.seriesInstanceUid = seriesUid;
        info.callingAeTitle = callingAe;
        info.receivedTime = std::chrono::system_clock::now();

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

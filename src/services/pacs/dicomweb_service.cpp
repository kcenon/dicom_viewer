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

#include "services/dicomweb_service.hpp"

#include <mutex>

#include <spdlog/spdlog.h>

#include <pacs/web/rest_config.hpp>
#include <pacs/web/rest_server.hpp>

namespace dicom_viewer::services {

namespace {

pacs::web::rest_server_config toRestConfig(const DicomWebConfig& config) {
    pacs::web::rest_server_config rc;
    rc.bind_address = config.bindAddress;
    rc.port = config.port;
    rc.concurrency = config.concurrency;
    rc.enable_cors = config.enableCors;
    rc.cors_allowed_origins = config.corsAllowedOrigins;
    rc.enable_tls = config.enableTls;
    rc.tls_cert_path = config.tlsCertPath;
    rc.tls_key_path = config.tlsKeyPath;
    rc.request_timeout_seconds = config.requestTimeoutSeconds;
    rc.max_body_size = config.maxBodySize;
    return rc;
}

} // anonymous namespace

class DicomWebService::Impl {
public:
    Impl() = default;

    std::expected<void, PacsErrorInfo> configure(const DicomWebConfig& config) {
        std::lock_guard lock(mutex_);

        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid DICOMweb configuration"
            });
        }

        if (server_ && server_->is_running()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "Cannot reconfigure while server is running. Stop first."
            });
        }

        config_ = config;

        if (!config.enabled) {
            server_.reset();
            spdlog::info("DICOMweb service configured but disabled");
            return {};
        }

        auto rc = toRestConfig(config);

        try {
            server_ = std::make_unique<pacs::web::rest_server>(rc);
        } catch (const std::exception& e) {
            server_.reset();
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to initialize DICOMweb server: ") + e.what()
            });
        }

        spdlog::info("DICOMweb service configured: {}:{} (CORS: {}, TLS: {})",
                     config.bindAddress, config.port,
                     config.enableCors ? "on" : "off",
                     config.enableTls ? "on" : "off");
        return {};
    }

    std::expected<void, PacsErrorInfo> start() {
        std::lock_guard lock(mutex_);

        if (!config_.enabled || !server_) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "DICOMweb service is not configured or not enabled"
            });
        }

        if (server_->is_running()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "DICOMweb server is already running"
            });
        }

        try {
            server_->start_async();
            spdlog::info("DICOMweb server started on port {}",
                         config_.port);
        } catch (const std::exception& e) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to start DICOMweb server: ") + e.what()
            });
        }

        return {};
    }

    void stop() {
        std::lock_guard lock(mutex_);
        if (server_ && server_->is_running()) {
            server_->stop();
            spdlog::info("DICOMweb server stopped");
        }
    }

    bool isRunning() const {
        std::lock_guard lock(mutex_);
        return server_ != nullptr && server_->is_running();
    }

    uint16_t actualPort() const {
        std::lock_guard lock(mutex_);
        if (!server_) return 0;
        return server_->port();
    }

    DicomWebConfig getConfig() const {
        std::lock_guard lock(mutex_);
        return config_;
    }

private:
    DicomWebConfig config_;
    std::unique_ptr<pacs::web::rest_server> server_;
    mutable std::mutex mutex_;
};

// Public interface implementation

DicomWebService::DicomWebService()
    : impl_(std::make_unique<Impl>()) {
}

DicomWebService::~DicomWebService() = default;

DicomWebService::DicomWebService(DicomWebService&&) noexcept = default;
DicomWebService& DicomWebService::operator=(DicomWebService&&) noexcept = default;

std::expected<void, PacsErrorInfo> DicomWebService::configure(
    const DicomWebConfig& config) {
    return impl_->configure(config);
}

std::expected<void, PacsErrorInfo> DicomWebService::start() {
    return impl_->start();
}

void DicomWebService::stop() {
    impl_->stop();
}

bool DicomWebService::isRunning() const {
    return impl_->isRunning();
}

uint16_t DicomWebService::actualPort() const {
    return impl_->actualPort();
}

DicomWebConfig DicomWebService::getConfig() const {
    return impl_->getConfig();
}

} // namespace dicom_viewer::services

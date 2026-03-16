// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
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

#include "api_server.hpp"
#include "jwt_middleware.hpp"

#include "auth_routes.hpp"
#include "cardiac_routes.hpp"
#include "export_routes.hpp"
#include "flow_routes.hpp"
#include "health_routes.hpp"
#include "measurement_routes.hpp"
#include "pacs_routes.hpp"
#include "render_routes.hpp"
#include "segmentation_routes.hpp"
#include "session_routes.hpp"
#include "study_routes.hpp"

#include "services/auth/auth_provider.hpp"
#include "services/dicom_echo_scu.hpp"
#include "services/dicom_find_scu.hpp"
#include "services/dicom_move_scu.hpp"
#include "services/render/render_session_manager.hpp"
#include "services/render/session_token_validator.hpp"
#include "services/audit_service.hpp"

#define CROW_DISABLE_STATIC_DIR
#include <crow.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace dicom_viewer::server {

using App = crow::App<JwtMiddleware>;

class ApiServer::Impl {
public:
    explicit Impl(const ApiServerConfig& cfg)
        : config_(cfg)
        , app_(std::make_unique<App>())
        , running_(false)
    {}

    ~Impl() { stop(); }

    void setServices(services::RenderSessionManager* sessions,
                     services::SessionTokenValidator* validator,
                     services::AuditService* audit) {
        sessions_ = sessions;
        validator_ = validator;
        audit_ = audit;
        app_->get_middleware<JwtMiddleware>().validator = validator;
    }

    void setAuthProvider(services::AuthProvider* auth) {
        auth_ = auth;
    }

    void setGpuBudgetManager(services::GpuMemoryBudgetManager* gpuBudget) {
        gpuBudget_ = gpuBudget;
    }

    void setPacsServices(services::DicomEchoSCU* echo,
                         services::DicomFindSCU* finder,
                         services::DicomMoveSCU* mover) {
        echo_   = echo;
        finder_ = finder;
        mover_  = mover;
    }

    bool start() {
        if (running_) return true;
        registerRoutes();
        app_->loglevel(crow::LogLevel::Warning);
        app_->concurrency(config_.concurrency);

        // Start in background thread
        serverThread_ = std::thread([this] {
            running_ = true;
            app_->port(config_.port).run();
            running_ = false;
        });

        // Wait briefly for server to bind
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        spdlog::info("REST API server started on port {}", config_.port);
        return true;
    }

    void stop() {
        if (app_) {
            app_->stop();
        }
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        running_ = false;
    }

    [[nodiscard]] bool isRunning() const { return running_; }
    [[nodiscard]] uint16_t port() const { return config_.port; }

private:
    void addCorsHeaders(crow::response& res) {
        res.add_header("Access-Control-Allow-Origin", config_.corsOrigin);
        res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
        res.add_header("Content-Type", "application/json");
    }

    void registerRoutes() {
        // ---- CORS preflight ----
        CROW_ROUTE((*app_), "/api/<path>").methods(crow::HTTPMethod::Options)(
            [this](const crow::request& /*req*/, crow::response& res,
                   const std::string& /*path*/) {
                addCorsHeaders(res);
                res.code = 204;
                res.end();
            });

        // ---- Health check (public) ----
        CROW_ROUTE((*app_), "/api/v1/health")(
            [this](const crow::request& /*req*/, crow::response& res) {
                nlohmann::json body;
                body["status"] = "ok";
                body["version"] = config_.serverVersion;
                body["activeSessions"] = sessions_ ? sessions_->activeSessionCount() : 0;

                addCorsHeaders(res);
                res.code = 200;
                res.body = body.dump();
                res.end();
            });

        // ---- Modular route registration ----
        registerAuthRoutes(app_.get(), auth_, audit_, config_.corsOrigin);
        registerSessionRoutes(app_.get(), sessions_, audit_, config_.wsBaseUrl, config_.corsOrigin);
        registerStudyRoutes(app_.get(), sessions_, audit_, config_.uploadDir, config_.corsOrigin);
        registerPacsRoutes(app_.get(), echo_, finder_, mover_, audit_, config_.corsOrigin);
        registerRenderRoutes(app_.get(), sessions_, config_.corsOrigin);
        registerSegmentationRoutes(app_.get(), sessions_, config_.corsOrigin);
        registerMeasurementRoutes(app_.get(), sessions_, audit_, config_.corsOrigin);
        registerFlowRoutes(app_.get(), sessions_, config_.corsOrigin);
        registerCardiacRoutes(app_.get(), sessions_, config_.corsOrigin);
        registerExportRoutes(app_.get(), sessions_, audit_, config_.exportDir, config_.corsOrigin);
        registerHealthRoutes(app_.get(), gpuBudget_, config_.corsOrigin);

        // ---- Catch-all 404 ----
        CROW_CATCHALL_ROUTE((*app_))([this](crow::response& res) {
            addCorsHeaders(res);
            res.code = 404;
            res.body = R"({"error":"not_found"})";
            res.end();
        });
    }

    ApiServerConfig config_;
    std::unique_ptr<App> app_;
    std::atomic<bool> running_;
    std::thread serverThread_;

    services::AuthProvider* auth_ = nullptr;
    services::RenderSessionManager* sessions_ = nullptr;
    services::SessionTokenValidator* validator_ = nullptr;
    services::AuditService* audit_ = nullptr;
    services::DicomEchoSCU* echo_ = nullptr;
    services::DicomFindSCU* finder_ = nullptr;
    services::DicomMoveSCU* mover_ = nullptr;
    services::GpuMemoryBudgetManager* gpuBudget_ = nullptr;
};

// ---- ApiServer public interface ----

ApiServer::ApiServer(const ApiServerConfig& config)
    : impl_(std::make_unique<Impl>(config))
{}

ApiServer::~ApiServer() = default;

void ApiServer::setServices(services::RenderSessionManager* sessions,
                             services::SessionTokenValidator* validator,
                             services::AuditService* audit) {
    impl_->setServices(sessions, validator, audit);
}

bool ApiServer::start() { return impl_->start(); }
void ApiServer::stop() { impl_->stop(); }
bool ApiServer::isRunning() const { return impl_->isRunning(); }
uint16_t ApiServer::port() const { return impl_->port(); }

void ApiServer::setAuthProvider(services::AuthProvider* auth) {
    impl_->setAuthProvider(auth);
}

void ApiServer::setGpuBudgetManager(services::GpuMemoryBudgetManager* gpuBudget) {
    impl_->setGpuBudgetManager(gpuBudget);
}

void ApiServer::setPacsServices(services::DicomEchoSCU* echo,
                                 services::DicomFindSCU* finder,
                                 services::DicomMoveSCU* mover) {
    impl_->setPacsServices(echo, finder, mover);
}

void ApiServer::registerRoutes() {
    // Routes are registered inside Impl::start()
}

} // namespace dicom_viewer::server

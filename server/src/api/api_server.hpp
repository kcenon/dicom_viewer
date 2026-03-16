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

/**
 * @file api_server.hpp
 * @brief Crow-based REST API server for the headless DICOM viewer server
 * @details Bootstraps a Crow application with JWT middleware, CORS headers,
 *          and the full set of API routes.
 *
 * ## Routes
 * | Method | Path                                        | Auth      | Description                  |
 * |--------|---------------------------------------------|-----------|------------------------------|
 * | GET    | /api/v1/health                              | No        | Health check                 |
 * | GET    | /api/v1/health/gpu                          | No        | GPU metrics (stub)           |
 * | POST   | /api/v1/auth/login                          | No        | User authentication          |
 * | POST   | /api/v1/auth/refresh                        | No        | Token refresh                |
 * | POST   | /api/v1/auth/logout                         | Bearer    | Token revocation             |
 * | POST   | /api/v1/sessions                            | Clinician | Create render session        |
 * | DELETE | /api/v1/sessions/{id}                       | Clinician | Destroy render session       |
 * | GET    | /api/v1/sessions/{id}                       | Viewer    | Get session status           |
 * | POST   | /api/v1/sessions/{id}/resize                | Clinician | Resize viewport              |
 * | POST   | /api/v1/sessions/{id}/viewport              | Clinician | Set viewport layout          |
 * | POST   | /api/v1/studies/upload                      | Clinician | DICOM file upload            |
 * | GET    | /api/v1/studies                             | Viewer    | List studies                 |
 * | GET    | /api/v1/studies/{uid}/series                | Viewer    | List series                  |
 * | POST   | /api/v1/sessions/{id}/load                  | Clinician | Load study into session      |
 * | POST   | /api/v1/pacs/servers/{id}/echo              | Clinician | C-ECHO connectivity test     |
 * | POST   | /api/v1/pacs/query                          | Clinician | C-FIND study query           |
 * | POST   | /api/v1/pacs/retrieve                       | Clinician | C-MOVE image retrieval       |
 * | POST   | /api/v1/sessions/{id}/render/preset         | Clinician | Apply render preset          |
 * | POST   | /api/v1/sessions/{id}/render/window-level   | Clinician | Set window/level             |
 * | POST   | /api/v1/sessions/{id}/render/blend-mode     | Clinician | Set blend mode               |
 * | GET    | /api/v1/sessions/{id}/render/snapshot       | Viewer    | Frame snapshot status        |
 * | POST   | /api/v1/sessions/{id}/segmentation/*        | Clinician | Segmentation tools           |
 * | GET    | /api/v1/sessions/{id}/measurements          | Viewer    | List measurements            |
 * | POST   | /api/v1/sessions/{id}/measurements/*        | Clinician | Create measurement           |
 * | POST   | /api/v1/sessions/{id}/flow/*                | Clinician | 4D flow operations           |
 * | POST   | /api/v1/sessions/{id}/cardiac/*             | Clinician | Cardiac analysis             |
 * | POST   | /api/v1/sessions/{id}/export/report         | Clinician | Start report export          |
 * | POST   | /api/v1/sessions/{id}/export/csv            | Clinician | Start CSV export             |
 * | GET    | /api/v1/export/{job_id}                     | Clinician | Poll export job              |
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace dicom_viewer::services {
class AuthProvider;
class GpuMemoryBudgetManager;
class RenderSessionManager;
class SessionTokenValidator;
class AuditService;
class DicomEchoSCU;
class DicomFindSCU;
class DicomMoveSCU;
} // namespace dicom_viewer::services

namespace dicom_viewer::server {

/**
 * @brief Configuration for the REST API server
 */
struct ApiServerConfig {
    /// HTTP listen port (default: 8080)
    uint16_t port = 8080;

    /// Number of Crow worker threads
    uint16_t concurrency = 4;

    /// CORS allowed origins (empty = allow all)
    std::string corsOrigin = "*";

    /// Server version string included in responses
    std::string serverVersion = "0.3.0";

    /// WebSocket server base URL for session wsUrl construction
    /// e.g., "ws://localhost:8081"
    std::string wsBaseUrl = "ws://localhost:8081";

    /// Directory for incoming DICOM file uploads
    std::string uploadDir = "/tmp/dicom_uploads";

    /// Directory for generated export files
    std::string exportDir = "/tmp/dicom_exports";
};

/**
 * @brief Crow REST API server with JWT middleware and CORS support
 *
 * Manages the lifecycle of the Crow HTTP server for the headless
 * DICOM rendering backend. Routes are registered in registerRoutes()
 * using service pointers injected via setServices().
 *
 * @trace SRS-FR-SERVER-001
 */
class ApiServer {
public:
    explicit ApiServer(const ApiServerConfig& config = {});
    ~ApiServer();

    // Non-copyable, non-movable (owns Crow app and background thread)
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
    ApiServer(ApiServer&&) = delete;
    ApiServer& operator=(ApiServer&&) = delete;

    /**
     * @brief Inject service dependencies used by route handlers
     * @param sessions Render session manager (non-owning)
     * @param validator JWT token validator (non-owning, may be nullptr)
     * @param audit ATNA audit service (non-owning, may be nullptr)
     */
    void setServices(services::RenderSessionManager* sessions,
                     services::SessionTokenValidator* validator,
                     services::AuditService* audit);

    /**
     * @brief Inject the authentication provider for auth routes
     * @param auth AuthProvider implementation (non-owning, may be nullptr)
     * @note If nullptr, auth routes return 503 Service Unavailable
     */
    void setAuthProvider(services::AuthProvider* auth);

    /**
     * @brief Inject the GPU memory budget manager for health routes
     * @param gpuBudget GpuMemoryBudgetManager instance (non-owning, may be nullptr)
     */
    void setGpuBudgetManager(services::GpuMemoryBudgetManager* gpuBudget);

    /**
     * @brief Inject PACS SCU services for PACS integration routes
     * @param echo   C-ECHO SCU (non-owning, may be nullptr)
     * @param finder C-FIND SCU (non-owning, may be nullptr)
     * @param mover  C-MOVE SCU (non-owning, may be nullptr)
     */
    void setPacsServices(services::DicomEchoSCU* echo,
                         services::DicomFindSCU* finder,
                         services::DicomMoveSCU* mover);

    /**
     * @brief Start the HTTP server (non-blocking — runs in background thread)
     * @return true if server started successfully
     */
    bool start();

    /**
     * @brief Stop the HTTP server and release its thread
     */
    void stop();

    /**
     * @brief Check if the server is running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get the actual port the server is listening on
     */
    [[nodiscard]] uint16_t port() const;

private:
    void registerRoutes();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::server

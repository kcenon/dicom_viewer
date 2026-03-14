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

#include "export_routes.hpp"

#include "services/render/render_session_manager.hpp"
#include "services/audit_service.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <random>
#include <sstream>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireRole;
using nlohmann::json;

namespace {

std::string newJobId() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream oss;
    oss << "job-" << std::hex << std::setw(8) << std::setfill('0') << dist(rng);
    return oss.str();
}

} // anonymous namespace

void registerExportRoutes(routes::App* app,
                           services::RenderSessionManager* sessions,
                           services::AuditService* audit,
                           const std::string& exportDir,
                           const std::string& corsOrigin) {
    // POST /api/v1/sessions/{id}/export/report — Start report generation (Clinician+)
    // Returns 202 Accepted with a job ID.
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/export/report")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, audit, exportDir, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!sessions || !sessions->hasSession(sessionId)) {
                res.code = 404;
                res.body = R"({"error":"not_found","message":"Session not found"})";
                res.end();
                return;
            }

            json body;
            try { body = json::parse(req.body); } catch (...) {
                // Body is optional for report export; use defaults
            }

            const auto jobId = newJobId();
            sessions->touchSession(sessionId);

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            if (audit) {
                audit->auditSecurityAlert(ctx.userId,
                    "export_report_requested:" + jobId + " session:" + sessionId);
            }
            spdlog::info("[export] report: session='{}' job='{}' user='{}'",
                         sessionId, jobId, ctx.userId);

            json resp;
            resp["jobId"]     = jobId;
            resp["sessionId"] = sessionId;
            resp["type"]      = "report";
            resp["status"]    = "queued";
            resp["pollUrl"]   = "/api/v1/export/" + jobId;

            res.code = 202;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/export/csv — Start CSV export (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/export/csv")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, audit, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!sessions || !sessions->hasSession(sessionId)) {
                res.code = 404;
                res.body = R"({"error":"not_found","message":"Session not found"})";
                res.end();
                return;
            }

            const auto jobId = newJobId();
            sessions->touchSession(sessionId);

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            if (audit) {
                audit->auditSecurityAlert(ctx.userId,
                    "export_csv_requested:" + jobId + " session:" + sessionId);
            }
            spdlog::info("[export] csv: session='{}' job='{}' user='{}'",
                         sessionId, jobId, ctx.userId);

            json resp;
            resp["jobId"]     = jobId;
            resp["sessionId"] = sessionId;
            resp["type"]      = "csv";
            resp["status"]    = "queued";
            resp["pollUrl"]   = "/api/v1/export/" + jobId;

            res.code = 202;
            res.body = resp.dump();
            res.end();
        });

    // GET /api/v1/export/{job_id} — Poll export job status / download (Clinician+)
    // In a full implementation this would look up a job registry.
    // Currently returns a stub "not_ready" response until the export pipeline
    // is wired to a background job queue (see issue #493).
    CROW_ROUTE(*app, "/api/v1/export/<string>")
        .methods(crow::HTTPMethod::Get)(
        [app, exportDir, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& jobId) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            json resp;
            resp["jobId"]   = jobId;
            resp["status"]  = "not_ready";
            resp["note"]    = "Export job queue not yet wired (see issue #493)";

            res.code = 200;
            res.body = resp.dump();
            res.end();

            (void)exportDir;
        });
}

} // namespace dicom_viewer::server

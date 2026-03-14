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

#include "session_routes.hpp"

#include "services/auth/rbac_middleware.hpp"
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

/// Generate a random hex session ID (e.g., "a1b2c3d4-e5f6a7b8-c9d0e1f2-a3b4c5d6")
std::string newSessionId() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream oss;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) oss << '-';
        oss << std::hex << std::setw(8) << std::setfill('0') << dist(rng);
    }
    return oss.str();
}

} // anonymous namespace

void registerSessionRoutes(routes::App* app,
                           services::RenderSessionManager* sessions,
                           services::AuditService* audit,
                           const std::string& wsBaseUrl,
                           const std::string& corsOrigin) {
    // POST /api/v1/sessions — Create a new render session (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, audit, wsBaseUrl, corsOrigin]
        (const crow::request& req, crow::response& res) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!sessions) {
                res.code = 503;
                res.body = R"({"error":"service_unavailable","message":"Session manager not configured"})";
                res.end();
                return;
            }

            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            const auto width  = body.value("width",  0u);
            const auto height = body.value("height", 0u);
            const auto sessionId = newSessionId();

            if (!sessions->createSession(sessionId, width, height)) {
                res.code = 429;
                res.body = R"({"error":"session_limit_reached","message":"Maximum concurrent sessions reached"})";
                res.end();
                return;
            }

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            if (audit) {
                audit->auditSecurityAlert(ctx.userId, "session_created:" + sessionId);
            }
            spdlog::info("[session] Created: id='{}' user='{}' size={}x{}",
                         sessionId, ctx.userId, width, height);

            json resp;
            resp["sessionId"] = sessionId;
            resp["wsUrl"]     = wsBaseUrl + "/render/" + sessionId;
            resp["channelId"] = 0;

            res.code = 201;
            res.body = resp.dump();
            res.end();
        });

    // DELETE /api/v1/sessions/{id} — Destroy a render session (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>")
        .methods(crow::HTTPMethod::Delete)(
        [app, sessions, audit, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!sessions || !sessions->destroySession(sessionId)) {
                res.code = 404;
                res.body = R"({"error":"not_found","message":"Session not found"})";
                res.end();
                return;
            }

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            if (audit) {
                audit->auditSecurityAlert(ctx.userId, "session_destroyed:" + sessionId);
            }
            spdlog::info("[session] Destroyed: id='{}' by user='{}'", sessionId, ctx.userId);

            res.code = 204;
            res.end();
        });

    // GET /api/v1/sessions/{id} — Query session status (Viewer+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>")
        .methods(crow::HTTPMethod::Get)(
        [app, sessions, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!requireRole(*app, req, res, services::Role::Viewer, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!sessions || !sessions->hasSession(sessionId)) {
                res.code = 404;
                res.body = R"({"error":"not_found","message":"Session not found"})";
                res.end();
                return;
            }

            json resp;
            resp["sessionId"] = sessionId;
            resp["active"]    = true;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/resize — Resize viewport (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/resize")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!sessions || !sessions->hasSession(sessionId)) {
                res.code = 404;
                res.body = R"({"error":"not_found","message":"Session not found"})";
                res.end();
                return;
            }

            sessions->touchSession(sessionId);

            res.code = 200;
            res.body = R"({})";
            res.end();
        });

    // POST /api/v1/sessions/{id}/viewport — Set viewport layout (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/viewport")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!sessions || !sessions->hasSession(sessionId)) {
                res.code = 404;
                res.body = R"({"error":"not_found","message":"Session not found"})";
                res.end();
                return;
            }

            sessions->touchSession(sessionId);

            res.code = 200;
            res.body = R"({})";
            res.end();
        });
}

} // namespace dicom_viewer::server

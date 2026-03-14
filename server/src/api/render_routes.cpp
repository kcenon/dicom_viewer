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

#include "render_routes.hpp"

#include "services/render/render_session_manager.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireRole;
using nlohmann::json;

void registerRenderRoutes(routes::App* app,
                          services::RenderSessionManager* sessions,
                          const std::string& corsOrigin) {
    // POST /api/v1/sessions/{id}/render/preset — Apply transfer function preset (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/render/preset")
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

            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            const auto presetName = body.value("preset", std::string{});
            if (presetName.empty()) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"preset is required"})";
                res.end();
                return;
            }

            sessions->touchSession(sessionId);
            spdlog::debug("[render] preset: session='{}' preset='{}'", sessionId, presetName);

            json resp;
            resp["sessionId"] = sessionId;
            resp["preset"]    = presetName;
            resp["applied"]   = true;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/render/window-level — Set W/L (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/render/window-level")
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

            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            if (!body.contains("width") || !body.contains("center")) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"width and center are required"})";
                res.end();
                return;
            }

            const double width  = body["width"].get<double>();
            const double center = body["center"].get<double>();

            sessions->touchSession(sessionId);
            spdlog::debug("[render] W/L: session='{}' width={} center={}",
                          sessionId, width, center);

            json resp;
            resp["sessionId"] = sessionId;
            resp["width"]     = width;
            resp["center"]    = center;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/render/blend-mode — Set blend mode (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/render/blend-mode")
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

            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            const auto mode = body.value("mode", std::string{});
            // Valid modes: Composite, MaximumIntensity, MinimumIntensity, Average
            static const std::array<std::string, 4> kValidModes{
                "Composite", "MaximumIntensity", "MinimumIntensity", "Average"};
            const bool valid = std::find(kValidModes.begin(), kValidModes.end(), mode)
                               != kValidModes.end();
            if (mode.empty() || !valid) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"mode must be one of: Composite, MaximumIntensity, MinimumIntensity, Average"})";
                res.end();
                return;
            }

            sessions->touchSession(sessionId);
            spdlog::debug("[render] blend-mode: session='{}' mode='{}'", sessionId, mode);

            json resp;
            resp["sessionId"] = sessionId;
            resp["mode"]      = mode;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // GET /api/v1/sessions/{id}/render/snapshot — Capture frame (Viewer+)
    // Returns a stub response; actual frame bytes are delivered over WebSocket.
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/render/snapshot")
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

            sessions->touchSession(sessionId);

            json resp;
            resp["sessionId"] = sessionId;
            resp["note"] = "Snapshot frames are delivered over the WebSocket channel. "
                           "This endpoint confirms the session is active.";
            resp["active"] = true;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server

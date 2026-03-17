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

#include "cardiac_routes.hpp"

#include "services/render/render_session_manager.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireRole;
using nlohmann::json;

void registerCardiacRoutes(routes::App* app,
                            services::RenderSessionManager* sessions,
                            const std::string& corsOrigin) {
    // POST /api/v1/sessions/{id}/cardiac/analyze — Trigger cardiac analysis (Clinician+)
    CROW_ROUTE((*app), "/api/v1/sessions/<string>/cardiac/analyze")
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
            try { body = json::parse(req.body); } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            const auto analysisType = body.value("type", std::string{"calcium_score"});
            sessions->touchSession(sessionId);
            spdlog::debug("[cardiac] analyze: session='{}' type='{}'", sessionId, analysisType);

            json resp;
            resp["sessionId"] = sessionId;
            resp["type"]      = analysisType;
            resp["status"]    = "queued";
            resp["jobId"]     = sessionId + "-cardiac";

            res.code = 202;
            res.body = resp.dump();
            res.end();
        });

    // GET /api/v1/sessions/{id}/cardiac/results — Retrieve cardiac analysis results (Clinician+)
    CROW_ROUTE((*app), "/api/v1/sessions/<string>/cardiac/results")
        .methods(crow::HTTPMethod::Get)(
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

            json resp;
            resp["sessionId"]    = sessionId;
            resp["available"]    = false;
            resp["calciumScore"] = 0.0;
            resp["agatstonScore"]= 0.0;
            resp["note"]         = "Cardiac analysis requires a loaded cardiac dataset";

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/cardiac/cine/phase — Set cine phase (Clinician+)
    CROW_ROUTE((*app), "/api/v1/sessions/<string>/cardiac/cine/phase")
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
            try { body = json::parse(req.body); } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            if (!body.contains("phase")) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"phase index is required"})";
                res.end();
                return;
            }

            const int phase = body["phase"].get<int>();
            sessions->touchSession(sessionId);

            json resp;
            resp["sessionId"]    = sessionId;
            resp["currentPhase"] = phase;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server

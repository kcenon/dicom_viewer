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

#include "segmentation_routes.hpp"

#include "services/render/render_session_manager.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireRole;
using nlohmann::json;

namespace {

/// Common session guard used by all segmentation routes.
bool guardSession(routes::App& app,
                  const crow::request& req,
                  crow::response& res,
                  services::RenderSessionManager* sessions,
                  const std::string& sessionId,
                  const std::string& corsOrigin) {
    if (!requireRole(app, req, res, services::Role::Clinician, corsOrigin)) return false;
    addCorsHeaders(res, corsOrigin);
    if (!sessions || !sessions->hasSession(sessionId)) {
        res.code = 404;
        res.body = R"({"error":"not_found","message":"Session not found"})";
        res.end();
        return false;
    }
    return true;
}

} // anonymous namespace

void registerSegmentationRoutes(routes::App* app,
                                 services::RenderSessionManager* sessions,
                                 const std::string& corsOrigin) {
    // POST /api/v1/sessions/{id}/segmentation/threshold (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/segmentation/threshold")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!guardSession(*app, req, res, sessions, sessionId, corsOrigin)) return;

            json body;
            try { body = json::parse(req.body); } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            const auto lower = body.value("lower", 0.0);
            const auto upper = body.value("upper", 3071.0);

            sessions->touchSession(sessionId);
            spdlog::debug("[seg] threshold: session='{}' [{}, {}]", sessionId, lower, upper);

            json resp;
            resp["sessionId"] = sessionId;
            resp["type"]      = "threshold";
            resp["lower"]     = lower;
            resp["upper"]     = upper;
            resp["status"]    = "applied";

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/segmentation/region-grow (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/segmentation/region-grow")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!guardSession(*app, req, res, sessions, sessionId, corsOrigin)) return;

            json body;
            try { body = json::parse(req.body); } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            if (!body.contains("seed")) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"seed [x,y,z] is required"})";
                res.end();
                return;
            }

            sessions->touchSession(sessionId);
            spdlog::debug("[seg] region-grow: session='{}'", sessionId);

            json resp;
            resp["sessionId"] = sessionId;
            resp["type"]      = "region-grow";
            resp["status"]    = "applied";

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/segmentation/brush (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/segmentation/brush")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!guardSession(*app, req, res, sessions, sessionId, corsOrigin)) return;

            json body;
            try { body = json::parse(req.body); } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            sessions->touchSession(sessionId);
            spdlog::debug("[seg] brush: session='{}'", sessionId);

            json resp;
            resp["sessionId"] = sessionId;
            resp["type"]      = "brush";
            resp["status"]    = "applied";

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/segmentation/undo (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/segmentation/undo")
        .methods(crow::HTTPMethod::Post)(
        [app, sessions, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& sessionId) {
            if (!guardSession(*app, req, res, sessions, sessionId, corsOrigin)) return;

            sessions->touchSession(sessionId);
            spdlog::debug("[seg] undo: session='{}'", sessionId);

            json resp;
            resp["sessionId"] = sessionId;
            resp["type"]      = "undo";
            resp["status"]    = "applied";

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server

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

#include "measurement_routes.hpp"

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

std::string newMeasurementId() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream oss;
    oss << "m-" << std::hex << std::setw(8) << std::setfill('0') << dist(rng);
    return oss.str();
}

} // anonymous namespace

void registerMeasurementRoutes(routes::App* app,
                                services::RenderSessionManager* sessions,
                                services::AuditService* audit,
                                const std::string& corsOrigin) {
    // GET /api/v1/sessions/{id}/measurements — List all measurements (Viewer+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/measurements")
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
            resp["sessionId"]    = sessionId;
            resp["measurements"] = json::array();

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/measurements/distance — Add distance (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/measurements/distance")
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

            json body;
            try { body = json::parse(req.body); } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            if (!body.contains("start") || !body.contains("end")) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"start and end points are required"})";
                res.end();
                return;
            }

            const auto measurementId = newMeasurementId();
            sessions->touchSession(sessionId);

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            if (audit) {
                audit->auditSecurityAlert(ctx.userId,
                    "measurement_created:" + measurementId + " session:" + sessionId);
            }
            spdlog::debug("[measurement] distance: session='{}' id='{}'",
                          sessionId, measurementId);

            json resp;
            resp["measurementId"] = measurementId;
            resp["sessionId"]     = sessionId;
            resp["type"]          = "distance";
            resp["distanceMm"]    = 0.0;

            res.code = 201;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/measurements/area — Add area (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/measurements/area")
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

            json resp;
            resp["measurementId"] = newMeasurementId();
            resp["sessionId"]     = sessionId;
            resp["type"]          = "area";
            resp["areaMm2"]       = 0.0;

            res.code = 201;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/measurements/roi-stats — ROI statistics (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/measurements/roi-stats")
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

            json resp;
            resp["sessionId"] = sessionId;
            resp["type"]      = "roi-stats";
            resp["mean"]      = 0.0;
            resp["stdDev"]    = 0.0;
            resp["min"]       = 0.0;
            resp["max"]       = 0.0;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/measurements/volume — Volume measurement (Clinician+)
    CROW_ROUTE(*app, "/api/v1/sessions/<string>/measurements/volume")
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

            json resp;
            resp["sessionId"] = sessionId;
            resp["type"]      = "volume";
            resp["volumeMl"]  = 0.0;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server

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

#include "pacs_routes.hpp"

#include "services/dicom_echo_scu.hpp"
#include "services/dicom_find_scu.hpp"
#include "services/dicom_move_scu.hpp"
#include "services/audit_service.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireRole;
using nlohmann::json;

namespace {

/// Build a PacsServerConfig from a JSON object.
/// Returns nullopt and sets a 400 response if required fields are missing.
std::optional<services::PacsServerConfig>
parsePacsConfig(const json& j, crow::response& res, const std::string& corsOrigin) {
    const auto hostname = j.value("hostname", std::string{});
    const auto aeTitle  = j.value("aeTitle",  std::string{});
    if (hostname.empty() || aeTitle.empty()) {
        addCorsHeaders(res, corsOrigin);
        res.code = 400;
        res.body = R"({"error":"bad_request","message":"hostname and aeTitle are required"})";
        res.end();
        return std::nullopt;
    }
    services::PacsServerConfig cfg;
    cfg.hostname       = hostname;
    cfg.calledAeTitle  = aeTitle;
    cfg.port           = static_cast<uint16_t>(j.value("port", 104));
    return cfg;
}

} // anonymous namespace

void registerPacsRoutes(routes::App* app,
                        services::DicomEchoSCU* echo,
                        services::DicomFindSCU* finder,
                        services::DicomMoveSCU* mover,
                        services::AuditService* audit,
                        const std::string& corsOrigin) {
    // POST /api/v1/pacs/servers/{id}/echo — C-ECHO connectivity test (Clinician+)
    CROW_ROUTE((*app), "/api/v1/pacs/servers/<string>/echo")
        .methods(crow::HTTPMethod::Post)(
        [app, echo, audit, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& serverId) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!echo) {
                res.code = 503;
                res.body = R"({"error":"service_unavailable","message":"PACS echo service not configured"})";
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

            const auto cfg = parsePacsConfig(body, res, corsOrigin);
            if (!cfg) return;

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            spdlog::info("[pacs] C-ECHO: server='{}' user='{}'", serverId, ctx.userId);

            const auto result = echo->verify(*cfg);
            if (!result) {
                spdlog::warn("[pacs] C-ECHO failed: server='{}' error={}",
                             serverId, result.error().message);
                if (audit) {
                    audit->auditQuery(cfg->callingAeTitle, cfg->calledAeTitle, "ECHO", false);
                }
                res.code = 502;
                json err;
                err["error"]   = "pacs_error";
                err["message"] = result.error().message;
                res.body = err.dump();
                res.end();
                return;
            }

            if (audit) {
                audit->auditQuery(cfg->callingAeTitle, cfg->calledAeTitle, "ECHO", true);
            }

            json resp;
            resp["serverId"] = serverId;
            resp["success"]  = result->success;
            resp["latencyMs"] = result->latency.count();
            resp["message"]  = result->message;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/pacs/query — C-FIND study/series query (Clinician+)
    CROW_ROUTE((*app), "/api/v1/pacs/query")
        .methods(crow::HTTPMethod::Post)(
        [app, finder, audit, corsOrigin]
        (const crow::request& req, crow::response& res) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!finder) {
                res.code = 503;
                res.body = R"({"error":"service_unavailable","message":"PACS query service not configured"})";
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

            const auto serverObj = body.value("server", json::object());
            const auto cfg = parsePacsConfig(serverObj, res, corsOrigin);
            if (!cfg) return;

            services::FindQuery query;
            query.root  = services::QueryRoot::StudyRoot;
            query.level = services::QueryLevel::Study;

            const auto filters = body.value("filters", json::object());
            if (filters.contains("patientName")) {
                query.patientName = filters["patientName"].get<std::string>();
            }
            if (filters.contains("patientId")) {
                query.patientId = filters["patientId"].get<std::string>();
            }
            if (filters.contains("studyUid")) {
                query.studyInstanceUid = filters["studyUid"].get<std::string>();
            }
            if (filters.contains("modality")) {
                query.modality = filters["modality"].get<std::string>();
            }

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            spdlog::info("[pacs] C-FIND: server='{}' user='{}'",
                         cfg->calledAeTitle, ctx.userId);

            const auto result = finder->find(*cfg, query);
            if (!result) {
                spdlog::warn("[pacs] C-FIND failed: {}", result.error().message);
                if (audit) {
                    audit->auditQuery(cfg->callingAeTitle, cfg->calledAeTitle,
                                      "STUDY", false);
                }
                res.code = 502;
                json err;
                err["error"]   = "pacs_error";
                err["message"] = result.error().message;
                res.body = err.dump();
                res.end();
                return;
            }

            if (audit) {
                audit->auditQuery(cfg->callingAeTitle, cfg->calledAeTitle,
                                  "STUDY", true);
            }

            json studies = json::array();
            for (const auto& s : result->studies) {
                json entry;
                entry["studyUid"]         = s.studyInstanceUid;
                entry["studyDate"]        = s.studyDate;
                entry["studyDescription"] = s.studyDescription;
                entry["patientName"]      = s.patientName;
                entry["patientId"]        = s.patientId;
                entry["modalitiesInStudy"]= s.modalitiesInStudy;
                entry["numberOfSeries"]   = s.numberOfSeries;
                studies.push_back(entry);
            }

            json resp;
            resp["studies"]   = studies;
            resp["total"]     = static_cast<int>(result->studies.size());
            resp["latencyMs"] = result->latency.count();

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/pacs/retrieve — C-MOVE image retrieval (Clinician+)
    CROW_ROUTE((*app), "/api/v1/pacs/retrieve")
        .methods(crow::HTTPMethod::Post)(
        [app, mover, audit, corsOrigin]
        (const crow::request& req, crow::response& res) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (!mover) {
                res.code = 503;
                res.body = R"({"error":"service_unavailable","message":"PACS retrieve service not configured"})";
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

            const auto serverObj = body.value("server", json::object());
            const auto cfg = parsePacsConfig(serverObj, res, corsOrigin);
            if (!cfg) return;

            const auto studyUid = body.value("studyUid", std::string{});
            if (studyUid.empty()) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"studyUid is required"})";
                res.end();
                return;
            }

            services::MoveConfig moveConfig;
            moveConfig.queryRoot        = services::QueryRoot::StudyRoot;
            moveConfig.storageDirectory = "/tmp/dicom_retrieve";

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            spdlog::info("[pacs] C-MOVE: study='{}' server='{}' user='{}'",
                         studyUid, cfg->calledAeTitle, ctx.userId);

            const auto result = mover->retrieveStudy(*cfg, moveConfig, studyUid);
            if (!result) {
                spdlog::warn("[pacs] C-MOVE failed: {}", result.error().message);
                res.code = 502;
                json err;
                err["error"]   = "pacs_error";
                err["message"] = result.error().message;
                res.body = err.dump();
                res.end();
                return;
            }

            if (audit) {
                audit->auditInstanceStored(cfg->calledAeTitle, "LOCAL",
                                           studyUid, ctx.userId,
                                           result->isSuccess());
            }

            json resp;
            resp["studyUid"]       = studyUid;
            resp["success"]        = result->isSuccess();
            resp["receivedImages"] = result->progress.receivedImages;
            resp["failedImages"]   = result->progress.failedImages;
            resp["latencyMs"]      = result->latency.count();

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server

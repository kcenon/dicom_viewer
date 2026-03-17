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

#include "study_routes.hpp"

#include "services/render/render_session_manager.hpp"
#include "services/audit_service.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireRole;
using nlohmann::json;

namespace {

/// DICOM magic bytes: bytes 128-131 must be "DICM"
bool isDicomFile(const std::string& data) {
    if (data.size() < 132) return false;
    return data[128] == 'D' &&
           data[129] == 'I' &&
           data[130] == 'C' &&
           data[131] == 'M';
}

/// Max DICOM upload size: 512 MB
constexpr size_t kMaxUploadBytes = 512UL * 1024 * 1024;

} // anonymous namespace

void registerStudyRoutes(routes::App* app,
                         services::RenderSessionManager* sessions,
                         services::AuditService* audit,
                         const std::string& uploadDir,
                         const std::string& corsOrigin) {
    // POST /api/v1/studies/upload — Upload a DICOM file (Clinician+)
    CROW_ROUTE((*app), "/api/v1/studies/upload")
        .methods(crow::HTTPMethod::Post)(
        [app, audit, uploadDir, corsOrigin]
        (const crow::request& req, crow::response& res) {
            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            if (req.body.size() > kMaxUploadBytes) {
                res.code = 413;
                res.body = R"({"error":"payload_too_large","message":"Upload exceeds 512 MB limit"})";
                res.end();
                return;
            }

            if (req.body.empty()) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Empty body — expected raw DICOM bytes"})";
                res.end();
                return;
            }

            if (!isDicomFile(req.body)) {
                res.code = 422;
                res.body = R"({"error":"invalid_dicom","message":"File does not have a valid DICOM preamble"})";
                res.end();
                return;
            }

            // Persist to upload directory
            const auto dir = std::filesystem::path(uploadDir);
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                spdlog::error("[study] Cannot create upload directory '{}': {}", uploadDir, ec.message());
                res.code = 500;
                res.body = R"({"error":"storage_error","message":"Upload directory unavailable"})";
                res.end();
                return;
            }

            // Generate a unique filename from current time + random suffix
            const auto filename = std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count()) + ".dcm";
            const auto filePath = dir / filename;

            {
                std::ofstream ofs(filePath, std::ios::binary);
                if (!ofs) {
                    res.code = 500;
                    res.body = R"({"error":"storage_error","message":"Could not write upload file"})";
                    res.end();
                    return;
                }
                ofs.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
            }

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            spdlog::info("[study] Upload: user='{}' file='{}' bytes={}",
                         ctx.userId, filename, req.body.size());
            if (audit) {
                audit->auditInstanceStored("API_UPLOAD", "LOCAL",
                                           "", ctx.userId, true);
            }

            json resp;
            resp["filename"] = filename;
            resp["bytes"]    = req.body.size();
            resp["message"]  = "DICOM file uploaded successfully";

            res.code = 201;
            res.body = resp.dump();
            res.end();
        });

    // GET /api/v1/studies — List all studies (Viewer+)
    // Stub: returns empty list until DicomLoader storage index is available.
    CROW_ROUTE((*app), "/api/v1/studies")
        .methods(crow::HTTPMethod::Get)(
        [app, corsOrigin]
        (const crow::request& req, crow::response& res) {
            if (!requireRole(*app, req, res, services::Role::Viewer, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            json resp;
            resp["studies"] = json::array();
            resp["total"]   = 0;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // GET /api/v1/studies/{uid}/series — List series for a study (Viewer+)
    // Stub: returns empty list until storage index is available.
    CROW_ROUTE((*app), "/api/v1/studies/<string>/series")
        .methods(crow::HTTPMethod::Get)(
        [app, corsOrigin]
        (const crow::request& req, crow::response& res, const std::string& studyUid) {
            if (!requireRole(*app, req, res, services::Role::Viewer, corsOrigin)) return;
            addCorsHeaders(res, corsOrigin);

            json resp;
            resp["studyUid"] = studyUid;
            resp["series"]   = json::array();
            resp["total"]    = 0;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/sessions/{id}/load — Load a study into a render session (Clinician+)
    CROW_ROUTE((*app), "/api/v1/sessions/<string>/load")
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
            try {
                body = json::parse(req.body);
            } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            const auto studyUid = body.value("studyUid", std::string{});
            if (studyUid.empty()) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"studyUid is required"})";
                res.end();
                return;
            }

            sessions->touchSession(sessionId);

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            if (audit) {
                audit->auditSecurityAlert(ctx.userId, "study_loaded:" + studyUid
                                          + " session:" + sessionId);
            }
            spdlog::info("[study] Load: session='{}' study='{}' user='{}'",
                         sessionId, studyUid, ctx.userId);

            json resp;
            resp["sessionId"] = sessionId;
            resp["studyUid"]  = studyUid;
            resp["status"]    = "loading";

            res.code = 202;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server

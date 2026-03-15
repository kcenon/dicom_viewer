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
 * @file study_routes.hpp
 * @brief DICOM study upload and listing REST API routes
 * @details Registers routes for DICOM file upload with magic-byte validation,
 *          study listing, and series listing for a given study UID.
 *          Study-load-into-session is also registered here.
 *
 * ## Routes
 * | Method | Path                                    | Min Role  |
 * |--------|-----------------------------------------|-----------|
 * | POST   | /api/v1/studies/upload                  | Clinician |
 * | GET    | /api/v1/studies                         | Viewer    |
 * | GET    | /api/v1/studies/{uid}/series            | Viewer    |
 * | POST   | /api/v1/sessions/{id}/load              | Clinician |
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "route_helpers.hpp"

#include <string>

namespace dicom_viewer::services {
class RenderSessionManager;
class AuditService;
} // namespace dicom_viewer::services

namespace dicom_viewer::server {

/**
 * @brief Register DICOM study management routes on the Crow application.
 * @param app        Crow application with JwtMiddleware (non-owning)
 * @param sessions   RenderSessionManager for session-load endpoint (may be nullptr)
 * @param audit      AuditService for ePHI event logging (may be nullptr)
 * @param uploadDir  Local directory for incoming DICOM uploads
 * @param corsOrigin CORS allowed-origin header value
 */
void registerStudyRoutes(routes::App* app,
                         services::RenderSessionManager* sessions,
                         services::AuditService* audit,
                         const std::string& uploadDir,
                         const std::string& corsOrigin);

} // namespace dicom_viewer::server

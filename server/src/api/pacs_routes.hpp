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
 * @file pacs_routes.hpp
 * @brief DICOM PACS integration REST API routes (C-ECHO, C-FIND, C-MOVE)
 * @details All PACS routes require Clinician role and delegate to
 *          DicomEchoSCU, DicomFindSCU, and DicomMoveSCU services.
 *
 * ## Routes
 * | Method | Path                                    | Min Role  |
 * |--------|-----------------------------------------|-----------|
 * | POST   | /api/v1/pacs/servers/{id}/echo          | Clinician |
 * | POST   | /api/v1/pacs/query                      | Clinician |
 * | POST   | /api/v1/pacs/retrieve                   | Clinician |
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "route_helpers.hpp"

#include <string>

namespace dicom_viewer::services {
class DicomEchoSCU;
class DicomFindSCU;
class DicomMoveSCU;
class AuditService;
} // namespace dicom_viewer::services

namespace dicom_viewer::server {

/**
 * @brief Register PACS integration routes on the Crow application.
 * @param app        Crow application with JwtMiddleware (non-owning)
 * @param echo       C-ECHO SCU service (may be nullptr)
 * @param finder     C-FIND SCU service (may be nullptr)
 * @param mover      C-MOVE SCU service (may be nullptr)
 * @param audit      AuditService for ATNA logging (may be nullptr)
 * @param corsOrigin CORS allowed-origin header value
 */
void registerPacsRoutes(routes::App* app,
                        services::DicomEchoSCU* echo,
                        services::DicomFindSCU* finder,
                        services::DicomMoveSCU* mover,
                        services::AuditService* audit,
                        const std::string& corsOrigin);

} // namespace dicom_viewer::server

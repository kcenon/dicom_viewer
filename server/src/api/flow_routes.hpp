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
 * @file flow_routes.hpp
 * @brief 4D Flow MRI analysis REST API routes
 * @details Routes for loading flow data, temporal navigation, and
 *          flow quantification. All routes require Clinician role.
 *
 * ## Routes (all under /api/v1/sessions/{id}/flow/)
 * | Method | Path                                          | Min Role  |
 * |--------|-----------------------------------------------|-----------|
 * | POST   | /api/v1/sessions/{id}/flow/load               | Clinician |
 * | POST   | /api/v1/sessions/{id}/flow/phase              | Clinician |
 * | GET    | /api/v1/sessions/{id}/flow/quantification     | Clinician |
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "route_helpers.hpp"

#include <string>

namespace dicom_viewer::services {
class RenderSessionManager;
} // namespace dicom_viewer::services

namespace dicom_viewer::server {

/**
 * @brief Register 4D flow analysis routes on the Crow application.
 * @param app        Crow application with JwtMiddleware (non-owning)
 * @param sessions   RenderSessionManager (may be nullptr)
 * @param corsOrigin CORS allowed-origin header value
 */
void registerFlowRoutes(routes::App* app,
                        services::RenderSessionManager* sessions,
                        const std::string& corsOrigin);

} // namespace dicom_viewer::server

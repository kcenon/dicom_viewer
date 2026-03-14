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
 * @file auth_routes.hpp
 * @brief Authentication REST API routes (login, refresh, logout, emergency access)
 * @details Registers public login/refresh routes, the authenticated logout route,
 *          and the HIPAA break-glass emergency access route.
 *
 * ## Routes
 * | Method | Path                              | Auth      |
 * |--------|-----------------------------------|-----------|
 * | POST   | /api/v1/auth/login                | Public    |
 * | POST   | /api/v1/auth/refresh              | Public    |
 * | POST   | /api/v1/auth/logout               | Bearer    |
 * | POST   | /api/v1/auth/emergency-access     | Clinician |
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "route_helpers.hpp"

#include <string>

namespace dicom_viewer::services {
class AuthProvider;
class AuditService;
} // namespace dicom_viewer::services

namespace dicom_viewer::server {

/**
 * @brief Register authentication routes on the Crow application.
 * @param app    Crow application with JwtMiddleware (non-owning)
 * @param auth   AuthProvider for credential validation (may be nullptr)
 * @param audit  AuditService for ATNA event logging (may be nullptr)
 * @param corsOrigin CORS allowed-origin header value
 */
void registerAuthRoutes(routes::App* app,
                        services::AuthProvider* auth,
                        services::AuditService* audit,
                        const std::string& corsOrigin);

} // namespace dicom_viewer::server

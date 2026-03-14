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
 * @file route_helpers.hpp
 * @brief Shared CORS and RBAC utilities for Crow route registration modules
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "jwt_middleware.hpp"
#include "services/auth/rbac_middleware.hpp"

#include <crow.h>
#include <string>

namespace dicom_viewer::server::routes {

/// Type alias for the Crow application with JWT middleware
using App = crow::App<JwtMiddleware>;

/// Add standard CORS headers and set Content-Type to application/json
inline void addCorsHeaders(crow::response& res, const std::string& origin) {
    res.add_header("Access-Control-Allow-Origin", origin);
    res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.add_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
    res.add_header("Content-Type", "application/json");
}

/**
 * @brief Check that the request carries a valid JWT.
 *
 * If the check fails, sets 401 on @p res and calls res.end().
 * @return true when the request is authenticated, false otherwise.
 */
inline bool requireAuth(App& app, const crow::request& req, crow::response& res,
                        const std::string& corsOrigin) {
    const auto& ctx = app.get_context<JwtMiddleware>(req);
    if (!ctx.authenticated) {
        addCorsHeaders(res, corsOrigin);
        res.code = 401;
        res.body = R"({"error":"unauthorized","message":"Authentication required"})";
        res.end();
        return false;
    }
    return true;
}

/**
 * @brief Check that the authenticated user meets @p minRole.
 *
 * Calls requireAuth first; if that fails this returns false immediately.
 * On role failure sets 403 on @p res and calls res.end().
 * @return true when role check passes, false otherwise.
 */
inline bool requireRole(App& app, const crow::request& req, crow::response& res,
                        services::Role minRole, const std::string& corsOrigin) {
    if (!requireAuth(app, req, res, corsOrigin)) return false;
    const auto& ctx = app.get_context<JwtMiddleware>(req);
    const auto userRole = services::RbacChecker::fromString(ctx.role);
    if (!services::RbacChecker::hasMinimumRole(userRole, minRole)) {
        addCorsHeaders(res, corsOrigin);
        res.code = 403;
        res.body = R"({"error":"forbidden","message":"Insufficient role"})";
        res.end();
        return false;
    }
    return true;
}

} // namespace dicom_viewer::server::routes

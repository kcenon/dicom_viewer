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
 * @file jwt_middleware.hpp
 * @brief Crow middleware for JWT Bearer token validation on REST API routes
 * @details Extracts the Bearer token from the Authorization header and
 *          validates it using SessionTokenValidator. Attaches decoded payload
 *          to the Crow context for downstream route handlers.
 *
 * ## Usage
 * Routes that require authentication should be defined on a Crow app
 * configured with JwtMiddleware. Unauthenticated routes (e.g., /health)
 * can bypass validation by checking context().skip = true before
 * calling next().
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "services/render/session_token_validator.hpp"

#include <crow.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <string>

namespace dicom_viewer::server {

/**
 * @brief JWT validation result attached to Crow request context
 */
struct JwtContext {
    bool authenticated = false;
    std::string userId;
    std::string role;
    std::string organization;

    /// Skip validation for this request (set by route handlers before middleware)
    bool skip = false;
};

/**
 * @brief Crow middleware that validates JWT Bearer tokens
 *
 * Validates the Authorization: Bearer <token> header on each request.
 * Sets JwtContext::authenticated = true on success.
 * Returns 401 Unauthorized on invalid or missing tokens for protected routes.
 *
 * Public paths (health check, static docs) set skip = true to bypass validation.
 */
struct JwtMiddleware {
    struct context : JwtContext {};

    /**
     * @brief Pointer to the token validator (must outlive this middleware)
     * Set before registering routes.
     */
    services::SessionTokenValidator* validator = nullptr;

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        // Public endpoints: health check, auth login/refresh (no Bearer token yet)
        if (req.url == "/api/v1/health" ||
            req.url == "/api/v1/health/gpu" ||
            req.url == "/api/v1/auth/login" ||
            req.url == "/api/v1/auth/refresh" ||
            req.url == "/") {
            ctx.skip = true;
            return;
        }

        if (!validator) {
            spdlog::critical("JwtMiddleware: validator is null — rejecting request (fail-closed)");
            res.code = 500;
            res.body = R"({"error":"server_misconfigured","message":"Authentication service unavailable"})";
            res.end();
            return;
        }

        const std::string authHeader = req.get_header_value("Authorization");
        if (authHeader.substr(0, 7) != "Bearer ") {
            res.code = 401;
            res.body = R"({"error":"missing_token","message":"Authorization: Bearer <token> required"})";
            res.end();
            return;
        }

        const std::string token = authHeader.substr(7);
        services::TokenPayload payload;
        const auto result = validator->validateToken(token, payload);

        if (result != services::TokenValidationResult::Valid) {
            spdlog::warn("JWT validation failed: token={:.8}... result={}",
                         token, static_cast<int>(result));
            res.code = 401;
            res.body = R"({"error":"invalid_token","message":"Token validation failed"})";
            res.end();
            return;
        }

        ctx.authenticated = true;
        ctx.userId = payload.userId;
        ctx.role = payload.role;
        ctx.organization = payload.organization;
    }

    void after_handle(crow::request& /*req*/, crow::response& /*res*/, context& /*ctx*/) {
        // No post-processing needed
    }
};

} // namespace dicom_viewer::server

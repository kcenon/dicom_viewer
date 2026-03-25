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
 * @brief Crow middleware for JWT token validation on REST API routes
 * @details Extracts the JWT from the Cookie header (httpOnly cookie) with
 *          fallback to Authorization: Bearer header for non-browser API clients.
 *          Validates using SessionTokenValidator and attaches decoded payload
 *          to the Crow context for downstream route handlers.
 *          Enforces CSRF token validation for state-changing requests (POST/PUT/DELETE)
 *          when the token is sourced from a cookie.
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

/// Cookie name for the httpOnly JWT access token
inline constexpr const char* kAccessTokenCookie = "access_token";

/// CSRF token header name
inline constexpr const char* kCsrfTokenHeader = "X-CSRF-Token";

/**
 * @brief Crow middleware that validates JWT tokens from Cookie or Authorization header
 *
 * Token resolution order:
 * 1. Cookie: access_token=<jwt> (browser clients)
 * 2. Authorization: Bearer <token> (non-browser API clients)
 *
 * When the token is sourced from a cookie, state-changing requests (POST/PUT/DELETE)
 * require a valid X-CSRF-Token header (double-submit cookie pattern).
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
        // Public endpoints: health check, auth login/refresh, CSRF token endpoint
        if (req.url == "/api/v1/health" ||
            req.url == "/api/v1/health/gpu" ||
            req.url == "/api/v1/auth/login" ||
            req.url == "/api/v1/auth/refresh" ||
            req.url == "/api/v1/auth/csrf-token" ||
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

        // Resolve token: Cookie first, then Authorization header fallback
        std::string token;
        bool tokenFromCookie = false;

        const std::string cookieHeader = req.get_header_value("Cookie");
        if (!cookieHeader.empty()) {
            token = extractCookieValue(cookieHeader, kAccessTokenCookie);
            if (!token.empty()) {
                tokenFromCookie = true;
            }
        }

        if (token.empty()) {
            const std::string authHeader = req.get_header_value("Authorization");
            if (authHeader.size() > 7 && authHeader.compare(0, 7, "Bearer ") == 0) {
                token = authHeader.substr(7);
            }
        }

        if (token.empty()) {
            res.code = 401;
            res.body = R"({"error":"missing_token","message":"Authentication token required"})";
            res.end();
            return;
        }

        // CSRF validation for state-changing requests when token comes from cookie
        if (tokenFromCookie && isStateChangingMethod(req.method)) {
            const std::string csrfToken = req.get_header_value(kCsrfTokenHeader);
            const std::string csrfCookie = extractCookieValue(cookieHeader, "csrf_token");
            if (csrfToken.empty() || csrfCookie.empty() || csrfToken != csrfCookie) {
                res.code = 403;
                res.body = R"({"error":"csrf_validation_failed","message":"Invalid or missing CSRF token"})";
                res.end();
                return;
            }
        }

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

private:
    static std::string extractCookieValue(const std::string& cookieHeader,
                                          const char* name) {
        const std::string prefix = std::string(name) + "=";
        std::string::size_type pos = 0;
        while (pos < cookieHeader.size()) {
            // Skip leading whitespace
            while (pos < cookieHeader.size() && cookieHeader[pos] == ' ') ++pos;
            if (cookieHeader.compare(pos, prefix.size(), prefix) == 0) {
                const auto valueStart = pos + prefix.size();
                const auto valueEnd = cookieHeader.find(';', valueStart);
                return cookieHeader.substr(valueStart,
                    valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
            }
            pos = cookieHeader.find(';', pos);
            if (pos == std::string::npos) break;
            ++pos; // skip ';'
        }
        return {};
    }

    static bool isStateChangingMethod(crow::HTTPMethod method) {
        return method == crow::HTTPMethod::Post ||
               method == crow::HTTPMethod::Put ||
               method == crow::HTTPMethod::Delete ||
               method == crow::HTTPMethod::Patch;
    }
};

} // namespace dicom_viewer::server

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

#include "auth_routes.hpp"

#include "services/auth/auth_provider.hpp"
#include "services/audit_service.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireAuth;
using nlohmann::json;

namespace {

/// Map AuthError to HTTP response and end the response.
void respondAuthError(crow::response& res, services::AuthError err,
                      const std::string& corsOrigin) {
    addCorsHeaders(res, corsOrigin);
    switch (err) {
        case services::AuthError::InvalidCredentials:
            res.code = 401;
            res.body = R"({"error":"invalid_credentials","message":"Username or password incorrect"})";
            break;
        case services::AuthError::AccountLocked:
            res.code = 403;
            res.body = R"({"error":"account_locked","message":"Account is locked or disabled"})";
            break;
        case services::AuthError::SessionLimitExceeded:
            res.code = 429;
            res.body = R"({"error":"session_limit_exceeded","message":"Maximum concurrent sessions reached"})";
            break;
        case services::AuthError::TokenExpired:
        case services::AuthError::TokenRevoked:
        case services::AuthError::TokenInvalid:
            res.code = 401;
            res.body = R"({"error":"invalid_token","message":"Token is invalid or expired"})";
            break;
        case services::AuthError::ProviderUnavailable:
            res.code = 503;
            res.body = R"({"error":"provider_unavailable","message":"Identity provider is unreachable"})";
            break;
        case services::AuthError::ConfigurationError:
            res.code = 500;
            res.body = R"({"error":"configuration_error","message":"Authentication provider misconfigured"})";
            break;
        default:
            res.code = 500;
            res.body = R"({"error":"internal_error","message":"Authentication service error"})";
            break;
    }
    res.end();
}

} // anonymous namespace

void registerAuthRoutes(routes::App* app,
                        services::AuthProvider* auth,
                        services::AuditService* audit,
                        const std::string& corsOrigin) {
    // POST /api/v1/auth/login — Public (no JWT required; middleware skips this path)
    CROW_ROUTE(*app, "/api/v1/auth/login")
        .methods(crow::HTTPMethod::Post)(
        [app, auth, audit, corsOrigin](const crow::request& req, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            if (!auth) {
                res.code = 503;
                res.body = R"({"error":"auth_unavailable","message":"Authentication service not configured"})";
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

            const auto username = body.value("username", std::string{});
            const auto password = body.value("password", std::string{});
            if (username.empty() || password.empty()) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"username and password are required"})";
                res.end();
                return;
            }

            auto result = auth->authenticate(username, password);
            if (!result) {
                spdlog::warn("[auth] Login failed: user='{}' error={}",
                             username, static_cast<int>(result.error()));
                if (audit) audit->auditAssociation(username, true, false);
                respondAuthError(res, result.error(), corsOrigin);
                return;
            }

            const auto& authResult = *result;
            if (audit) audit->auditAssociation(authResult.userInfo.userId, true, true);
            spdlog::info("[auth] Login success: user='{}' role='{}'",
                         authResult.userInfo.userId, authResult.userInfo.role);

            json resp;
            resp["accessToken"]  = authResult.tokens.accessToken;
            resp["refreshToken"] = authResult.tokens.refreshToken;
            resp["expiresAt"]    = authResult.tokens.accessExpiresAt;
            resp["user"] = {
                {"id",       authResult.userInfo.userId},
                {"username", authResult.userInfo.displayName},
                {"role",     authResult.userInfo.role},
                {"email",    authResult.userInfo.email}
            };

            res.code = 200;
            res.body = resp.dump();
            res.end();

            // Suppress unused variable warning (app captured for consistency)
            (void)app;
        });

    // POST /api/v1/auth/refresh — Public (refresh token in body, not Authorization header)
    CROW_ROUTE(*app, "/api/v1/auth/refresh")
        .methods(crow::HTTPMethod::Post)(
        [app, auth, corsOrigin](const crow::request& req, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            if (!auth) {
                res.code = 503;
                res.body = R"({"error":"auth_unavailable"})";
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

            const auto refreshToken = body.value("refreshToken", std::string{});
            if (refreshToken.empty()) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"refreshToken is required"})";
                res.end();
                return;
            }

            auto result = auth->refreshToken(refreshToken);
            if (!result) {
                respondAuthError(res, result.error(), corsOrigin);
                return;
            }

            const auto& tokens = *result;
            json resp;
            resp["accessToken"] = tokens.accessToken;
            resp["expiresAt"]   = tokens.accessExpiresAt;

            res.code = 200;
            res.body = resp.dump();
            res.end();

            (void)app;
        });

    // POST /api/v1/auth/logout — Authenticated (revokes the access token)
    CROW_ROUTE(*app, "/api/v1/auth/logout")
        .methods(crow::HTTPMethod::Post)(
        [app, auth, audit, corsOrigin](const crow::request& req, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            if (!requireAuth(*app, req, res, corsOrigin)) return;

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            const auto  userId = ctx.userId;

            // Extract the Bearer token to revoke it
            const std::string authHeader = req.get_header_value("Authorization");
            const bool hasBearer = authHeader.size() > 7 &&
                                   authHeader.compare(0, 7, "Bearer ") == 0;

            if (auth && hasBearer) {
                const std::string token = authHeader.substr(7);
                const auto revokeResult = auth->revokeToken(token);
                if (!revokeResult) {
                    spdlog::warn("[auth] Token revocation failed: user='{}' error={}",
                                 userId, static_cast<int>(revokeResult.error()));
                }
            }

            if (audit) audit->auditAssociation(userId, false, true);
            spdlog::info("[auth] Logout: user='{}'", userId);

            res.code = 204;
            res.end();
        });
}

} // namespace dicom_viewer::server

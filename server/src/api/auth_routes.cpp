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

#include <random>
#include <sstream>
#include <iomanip>

namespace dicom_viewer::server {

using routes::addCorsHeaders;
using routes::requireAuth;
using routes::requireRole;
using nlohmann::json;

namespace {

/// Generate a cryptographically random hex string for CSRF tokens
std::string generateCsrfToken() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(16) << dist(gen);
    oss << std::setw(16) << dist(gen);
    return oss.str();
}

/// Build a Set-Cookie header value for the httpOnly access token
std::string buildAccessTokenCookie(const std::string& token, bool clear = false) {
    std::string cookie = "access_token=";
    if (clear) {
        cookie += "; Max-Age=0";
    } else {
        cookie += token;
    }
    cookie += "; Path=/api; HttpOnly; Secure; SameSite=Strict";
    return cookie;
}

/// Build a Set-Cookie header value for the CSRF token (readable by JavaScript)
std::string buildCsrfCookie(const std::string& token, bool clear = false) {
    std::string cookie = "csrf_token=";
    if (clear) {
        cookie += "; Max-Age=0";
    } else {
        cookie += token;
    }
    cookie += "; Path=/api; Secure; SameSite=Strict";
    return cookie;
}

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
    CROW_ROUTE((*app), "/api/v1/auth/login")
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

            // Set httpOnly cookie with access token
            res.add_header("Set-Cookie", buildAccessTokenCookie(authResult.tokens.accessToken));

            // Set CSRF token cookie (JavaScript-readable for double-submit pattern)
            const auto csrfToken = generateCsrfToken();
            res.add_header("Set-Cookie", buildCsrfCookie(csrfToken));

            json resp;
            resp["expiresAt"]    = authResult.tokens.accessExpiresAt;
            resp["refreshToken"] = authResult.tokens.refreshToken;
            resp["csrfToken"]    = csrfToken;
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
    CROW_ROUTE((*app), "/api/v1/auth/refresh")
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

            // Update httpOnly cookie with new access token
            res.add_header("Set-Cookie", buildAccessTokenCookie(tokens.accessToken));

            // Rotate CSRF token on refresh
            const auto csrfToken = generateCsrfToken();
            res.add_header("Set-Cookie", buildCsrfCookie(csrfToken));

            json resp;
            resp["expiresAt"]  = tokens.accessExpiresAt;
            resp["csrfToken"]  = csrfToken;

            res.code = 200;
            res.body = resp.dump();
            res.end();

            (void)app;
        });

    // POST /api/v1/auth/logout — Authenticated (revokes the access token, clears cookie)
    CROW_ROUTE((*app), "/api/v1/auth/logout")
        .methods(crow::HTTPMethod::Post)(
        [app, auth, audit, corsOrigin](const crow::request& req, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            if (!requireAuth(*app, req, res, corsOrigin)) return;

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            const auto  userId = ctx.userId;

            // Extract token from Authorization header or Cookie for revocation
            std::string token;
            const std::string authHeader = req.get_header_value("Authorization");
            if (authHeader.size() > 7 && authHeader.compare(0, 7, "Bearer ") == 0) {
                token = authHeader.substr(7);
            } else {
                const std::string cookieHeader = req.get_header_value("Cookie");
                if (!cookieHeader.empty()) {
                    // Reuse middleware's cookie extraction logic inline
                    const std::string prefix = "access_token=";
                    auto pos = cookieHeader.find(prefix);
                    if (pos != std::string::npos) {
                        const auto valueStart = pos + prefix.size();
                        const auto valueEnd = cookieHeader.find(';', valueStart);
                        token = cookieHeader.substr(valueStart,
                            valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
                    }
                }
            }

            if (auth && !token.empty()) {
                const auto revokeResult = auth->revokeToken(token);
                if (!revokeResult) {
                    spdlog::warn("[auth] Token revocation failed: user='{}' error={}",
                                 userId, static_cast<int>(revokeResult.error()));
                }
            }

            // Clear auth cookies
            res.add_header("Set-Cookie", buildAccessTokenCookie("", true));
            res.add_header("Set-Cookie", buildCsrfCookie("", true));

            if (audit) audit->auditAssociation(userId, false, true);
            spdlog::info("[auth] Logout: user='{}'", userId);

            res.code = 204;
            res.end();
        });

    // GET /api/v1/auth/csrf-token — Public (returns a fresh CSRF token pair)
    CROW_ROUTE((*app), "/api/v1/auth/csrf-token")
        .methods(crow::HTTPMethod::Get)(
        [corsOrigin](const crow::request& /*req*/, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            const auto csrfToken = generateCsrfToken();
            res.add_header("Set-Cookie", buildCsrfCookie(csrfToken));

            json resp;
            resp["csrfToken"] = csrfToken;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // GET /api/v1/auth/me — Authenticated (returns current user info from JWT context)
    CROW_ROUTE((*app), "/api/v1/auth/me")
        .methods(crow::HTTPMethod::Get)(
        [app, corsOrigin](const crow::request& req, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            if (!requireAuth(*app, req, res, corsOrigin)) return;

            const auto& ctx = app->get_context<JwtMiddleware>(req);

            json resp;
            resp["id"]       = ctx.userId;
            resp["role"]     = ctx.role;

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });

    // POST /api/v1/auth/emergency-access — HIPAA break-glass (Clinician+)
    // Logs an emergency access event and confirms the clinician's identity.
    CROW_ROUTE((*app), "/api/v1/auth/emergency-access")
        .methods(crow::HTTPMethod::Post)(
        [app, auth, audit, corsOrigin](const crow::request& req, crow::response& res) {
            addCorsHeaders(res, corsOrigin);

            if (!requireRole(*app, req, res, services::Role::Clinician, corsOrigin)) return;

            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"Invalid JSON body"})";
                res.end();
                return;
            }

            const auto reason = body.value("reason", std::string{});
            if (reason.empty()) {
                res.code = 400;
                res.body = R"({"error":"bad_request","message":"reason is required for emergency access"})";
                res.end();
                return;
            }

            const auto& ctx = app->get_context<JwtMiddleware>(req);
            const auto userId = ctx.userId;

            // Emit a security alert to the ATNA audit trail (mandatory for HIPAA)
            if (audit) {
                audit->auditSecurityAlert(userId, "emergency_access: " + reason);
            }
            spdlog::warn("[auth] EMERGENCY ACCESS: user='{}' reason='{}'", userId, reason);

            json resp;
            resp["userId"]    = userId;
            resp["role"]      = ctx.role;
            resp["emergency"] = true;
            resp["reason"]    = reason;
            resp["note"]      = "Emergency access logged to ATNA audit trail";

            res.code = 200;
            res.body = resp.dump();
            res.end();
        });
}

} // namespace dicom_viewer::server

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

#include <gtest/gtest.h>

#include "api/jwt_middleware.hpp"

#include <string>

using namespace dicom_viewer::server;

// ============================================================================
// JwtMiddleware — null validator (fail-closed)
// ============================================================================

TEST(JwtMiddleware, NullValidatorReturns500) {
    JwtMiddleware mw;
    mw.validator = nullptr;  // deliberately null

    crow::request req;
    req.url = "/api/v1/sessions";  // protected endpoint
    req.add_header("Authorization", "Bearer some-token");

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, 500);
    EXPECT_FALSE(ctx.authenticated);
    EXPECT_NE(res.body.find("server_misconfigured"), std::string::npos);
    EXPECT_NE(res.body.find("Authentication service unavailable"), std::string::npos);
}

TEST(JwtMiddleware, NullValidatorDoesNotGrantAnonymousAccess) {
    JwtMiddleware mw;
    mw.validator = nullptr;

    crow::request req;
    req.url = "/api/v1/studies";

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    // Must NOT grant anonymous access
    EXPECT_FALSE(ctx.authenticated);
    EXPECT_EQ(ctx.userId, "");
    EXPECT_EQ(ctx.role, "");
    EXPECT_EQ(res.code, 500);
}

// ============================================================================
// JwtMiddleware — public endpoint bypass
// ============================================================================

TEST(JwtMiddleware, PublicEndpointBypassesValidation) {
    JwtMiddleware mw;
    mw.validator = nullptr;  // null validator should not matter for public routes

    crow::request req;
    crow::response res;
    JwtMiddleware::context ctx;

    // Health check
    req.url = "/api/v1/health";
    mw.before_handle(req, res, ctx);
    EXPECT_TRUE(ctx.skip);
    EXPECT_NE(res.code, 500);

    // Auth login
    ctx = {};
    res = {};
    req.url = "/api/v1/auth/login";
    mw.before_handle(req, res, ctx);
    EXPECT_TRUE(ctx.skip);
    EXPECT_NE(res.code, 500);

    // Root
    ctx = {};
    res = {};
    req.url = "/";
    mw.before_handle(req, res, ctx);
    EXPECT_TRUE(ctx.skip);
    EXPECT_NE(res.code, 500);
}

// ============================================================================
// JwtMiddleware — missing token (no Cookie, no Authorization header)
// ============================================================================

TEST(JwtMiddleware, MissingTokenReturns401) {
    // Use a real validator to test the missing-token path
    dicom_viewer::services::SessionTokenConfig cfg;
    cfg.allowEphemeralKeys = true;
    dicom_viewer::services::SessionTokenValidator validator(cfg);

    JwtMiddleware mw;
    mw.validator = &validator;

    crow::request req;
    req.url = "/api/v1/sessions";
    // No Authorization header or Cookie set

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, 401);
    EXPECT_FALSE(ctx.authenticated);
    EXPECT_NE(res.body.find("missing_token"), std::string::npos);
}

// ============================================================================
// JwtMiddleware — Authorization: Bearer fallback still works
// ============================================================================

TEST(JwtMiddleware, BearerTokenFallbackStillWorks) {
    JwtMiddleware mw;
    mw.validator = nullptr;  // will hit null-validator check after token extraction

    crow::request req;
    req.url = "/api/v1/sessions";
    req.add_header("Authorization", "Bearer test-token");

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    // Should reach the null-validator check (500), not token-missing (401)
    EXPECT_EQ(res.code, 500);
    EXPECT_FALSE(ctx.authenticated);
}

// ============================================================================
// JwtMiddleware — CSRF token endpoint is public
// ============================================================================

TEST(JwtMiddleware, CsrfTokenEndpointIsPublic) {
    JwtMiddleware mw;
    mw.validator = nullptr;

    crow::request req;
    req.url = "/api/v1/auth/csrf-token";

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    EXPECT_TRUE(ctx.skip);
    EXPECT_NE(res.code, 500);
}

// ============================================================================
// JwtMiddleware — Cookie-based token extraction (null validator path)
// ============================================================================

TEST(JwtMiddleware, CookieTokenExtraction) {
    JwtMiddleware mw;
    mw.validator = nullptr;  // will hit null-validator check after extraction

    crow::request req;
    req.url = "/api/v1/sessions";
    req.add_header("Cookie", "access_token=my-jwt-token; csrf_token=my-csrf");

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    // Should reach the null-validator check (500), meaning token was extracted
    EXPECT_EQ(res.code, 500);
    EXPECT_FALSE(ctx.authenticated);
}

// ============================================================================
// JwtMiddleware — Cookie preferred over Authorization header
// ============================================================================

TEST(JwtMiddleware, CookiePreferredOverBearerHeader) {
    JwtMiddleware mw;
    mw.validator = nullptr;

    crow::request req;
    req.url = "/api/v1/sessions";
    req.add_header("Cookie", "access_token=cookie-token");
    req.add_header("Authorization", "Bearer bearer-token");

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    // Should reach null-validator (500) — cookie token extracted first
    EXPECT_EQ(res.code, 500);
}

// ============================================================================
// JwtMiddleware — CSRF validation for state-changing methods with cookie auth
// ============================================================================

TEST(JwtMiddleware, CsrfRequiredForPostWithCookieAuth) {
    dicom_viewer::services::SessionTokenConfig cfg;
    cfg.allowEphemeralKeys = true;
    dicom_viewer::services::SessionTokenValidator validator(cfg);

    JwtMiddleware mw;
    mw.validator = &validator;

    crow::request req;
    req.url = "/api/v1/auth/logout";
    req.method = crow::HTTPMethod::Post;
    // Cookie has access_token but no matching CSRF
    req.add_header("Cookie", "access_token=some-token");

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, 403);
    EXPECT_FALSE(ctx.authenticated);
    EXPECT_NE(res.body.find("csrf_validation_failed"), std::string::npos);
}

TEST(JwtMiddleware, CsrfMismatchReturns403) {
    dicom_viewer::services::SessionTokenConfig cfg;
    cfg.allowEphemeralKeys = true;
    dicom_viewer::services::SessionTokenValidator validator(cfg);

    JwtMiddleware mw;
    mw.validator = &validator;

    crow::request req;
    req.url = "/api/v1/auth/logout";
    req.method = crow::HTTPMethod::Post;
    req.add_header("Cookie", "access_token=some-token; csrf_token=cookie-csrf");
    req.add_header("X-CSRF-Token", "different-csrf-value");

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, 403);
    EXPECT_NE(res.body.find("csrf_validation_failed"), std::string::npos);
}

TEST(JwtMiddleware, CsrfNotRequiredForGetWithCookieAuth) {
    JwtMiddleware mw;
    mw.validator = nullptr;  // will hit null-validator after CSRF check skipped

    crow::request req;
    req.url = "/api/v1/studies";
    req.method = crow::HTTPMethod::Get;
    req.add_header("Cookie", "access_token=some-token");
    // No CSRF header — should be fine for GET

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    // Should NOT be 403 (CSRF), should be 500 (null validator)
    EXPECT_EQ(res.code, 500);
}

TEST(JwtMiddleware, CsrfNotRequiredForBearerAuth) {
    JwtMiddleware mw;
    mw.validator = nullptr;

    crow::request req;
    req.url = "/api/v1/auth/logout";
    req.method = crow::HTTPMethod::Post;
    // Bearer auth — no CSRF needed
    req.add_header("Authorization", "Bearer some-token");

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    // Should NOT be 403 (CSRF), should be 500 (null validator)
    EXPECT_EQ(res.code, 500);
}

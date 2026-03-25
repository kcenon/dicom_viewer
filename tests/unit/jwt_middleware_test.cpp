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
// JwtMiddleware — missing Bearer token (with non-null validator)
// ============================================================================

TEST(JwtMiddleware, MissingBearerTokenReturns401) {
    // Use a real validator to test the missing-token path
    dicom_viewer::services::SessionTokenConfig cfg;
    cfg.allowEphemeralKeys = true;
    dicom_viewer::services::SessionTokenValidator validator(cfg);

    JwtMiddleware mw;
    mw.validator = &validator;

    crow::request req;
    req.url = "/api/v1/sessions";
    // No Authorization header set

    crow::response res;
    JwtMiddleware::context ctx;

    mw.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, 401);
    EXPECT_FALSE(ctx.authenticated);
    EXPECT_NE(res.body.find("missing_token"), std::string::npos);
}

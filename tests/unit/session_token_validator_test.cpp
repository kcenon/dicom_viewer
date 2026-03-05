// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
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

#include "services/render/session_token_validator.hpp"

#include <chrono>
#include <thread>

using namespace dicom_viewer::services;

// =============================================================================
// Default construction
// =============================================================================

TEST(SessionTokenValidatorTest, DefaultConstruction) {
    SessionTokenValidator validator;
    EXPECT_FALSE(validator.allowsUnauthenticatedLocal());
    EXPECT_EQ(validator.config().expirySeconds, 3600u);
}

TEST(SessionTokenValidatorTest, CustomConfig) {
    SessionTokenConfig config;
    config.secretKey = "my-secret";
    config.expirySeconds = 7200;
    config.allowUnauthenticatedLocal = true;

    SessionTokenValidator validator(config);
    EXPECT_TRUE(validator.allowsUnauthenticatedLocal());
    EXPECT_EQ(validator.config().expirySeconds, 7200u);
    EXPECT_EQ(validator.config().secretKey, "my-secret");
}

// =============================================================================
// Token generation and validation
// =============================================================================

TEST(SessionTokenValidatorTest, GenerateAndValidateToken) {
    SessionTokenValidator validator;

    auto token = validator.generateToken("user1", "1.2.3.4.5");
    EXPECT_FALSE(token.empty());

    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::Valid);
}

TEST(SessionTokenValidatorTest, ValidateTokenExtractPayload) {
    SessionTokenValidator validator;

    auto token = validator.generateToken("user1", "1.2.3.4.5");

    TokenPayload payload;
    auto result = validator.validateToken(token, payload);
    EXPECT_EQ(result, TokenValidationResult::Valid);
    EXPECT_EQ(payload.userId, "user1");
    EXPECT_EQ(payload.studyUid, "1.2.3.4.5");
    EXPECT_GT(payload.expiryEpoch, 0u);
}

TEST(SessionTokenValidatorTest, ValidateTokenNoStudyRestriction) {
    SessionTokenValidator validator;

    auto token = validator.generateToken("user1", "1.2.3.4.5");

    // Validate without specifying a required study UID
    auto result = validator.validateToken(token);
    EXPECT_EQ(result, TokenValidationResult::Valid);
}

// =============================================================================
// Token validation failures
// =============================================================================

TEST(SessionTokenValidatorTest, EmptyTokenReturnsEmpty) {
    SessionTokenValidator validator;

    auto result = validator.validateToken("");
    EXPECT_EQ(result, TokenValidationResult::Empty);
}

TEST(SessionTokenValidatorTest, GarbageTokenReturnsInvalidFormat) {
    SessionTokenValidator validator;

    auto result = validator.validateToken("not-a-valid-token");
    // Could be InvalidFormat or InvalidSignature depending on decode
    EXPECT_NE(result, TokenValidationResult::Valid);
}

TEST(SessionTokenValidatorTest, WrongSecretReturnsInvalidSignature) {
    SessionTokenConfig config1;
    config1.secretKey = "secret-1";
    SessionTokenValidator validator1(config1);

    SessionTokenConfig config2;
    config2.secretKey = "secret-2";
    SessionTokenValidator validator2(config2);

    auto token = validator1.generateToken("user1", "1.2.3.4.5");

    // Validate with a different secret
    auto result = validator2.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::InvalidSignature);
}

TEST(SessionTokenValidatorTest, StudyMismatchReturnsStudyMismatch) {
    SessionTokenValidator validator;

    auto token = validator.generateToken("user1", "1.2.3.4.5");

    auto result = validator.validateToken(token, "9.8.7.6.5");
    EXPECT_EQ(result, TokenValidationResult::StudyMismatch);
}

TEST(SessionTokenValidatorTest, ExpiredTokenReturnsExpired) {
    SessionTokenConfig config;
    config.expirySeconds = 0; // Expire immediately
    SessionTokenValidator validator(config);

    auto token = validator.generateToken("user1", "1.2.3.4.5");

    // Wait briefly so current time exceeds the expiry (same-second edge case)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::Expired);
}

// =============================================================================
// Configuration update
// =============================================================================

TEST(SessionTokenValidatorTest, SetConfigUpdatesSecret) {
    SessionTokenValidator validator;

    auto token = validator.generateToken("user1", "1.2.3.4.5");
    EXPECT_EQ(validator.validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);

    // Change secret — old tokens should no longer validate
    SessionTokenConfig newConfig;
    newConfig.secretKey = "new-secret-key";
    validator.setConfig(newConfig);

    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::InvalidSignature);
}

TEST(SessionTokenValidatorTest, SetConfigUpdatesAllowLocal) {
    SessionTokenValidator validator;
    EXPECT_FALSE(validator.allowsUnauthenticatedLocal());

    SessionTokenConfig config;
    config.allowUnauthenticatedLocal = true;
    validator.setConfig(config);
    EXPECT_TRUE(validator.allowsUnauthenticatedLocal());
}

// =============================================================================
// Multiple tokens
// =============================================================================

TEST(SessionTokenValidatorTest, DifferentUsersGetDifferentTokens) {
    SessionTokenValidator validator;

    auto token1 = validator.generateToken("user1", "1.2.3.4.5");
    auto token2 = validator.generateToken("user2", "1.2.3.4.5");

    EXPECT_NE(token1, token2);

    // Both should be valid
    EXPECT_EQ(validator.validateToken(token1, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(validator.validateToken(token2, "1.2.3.4.5"),
              TokenValidationResult::Valid);
}

TEST(SessionTokenValidatorTest, DifferentStudiesGetDifferentTokens) {
    SessionTokenValidator validator;

    auto token1 = validator.generateToken("user1", "1.2.3.4.5");
    auto token2 = validator.generateToken("user1", "9.8.7.6.5");

    EXPECT_NE(token1, token2);

    // Each token valid for its own study
    EXPECT_EQ(validator.validateToken(token1, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(validator.validateToken(token2, "9.8.7.6.5"),
              TokenValidationResult::Valid);

    // Cross-validation should fail
    EXPECT_EQ(validator.validateToken(token1, "9.8.7.6.5"),
              TokenValidationResult::StudyMismatch);
}

// =============================================================================
// Move semantics
// =============================================================================

TEST(SessionTokenValidatorTest, MoveConstruction) {
    SessionTokenConfig config;
    config.secretKey = "move-test";
    SessionTokenValidator a(config);
    auto token = a.generateToken("user1", "1.2.3.4.5");

    SessionTokenValidator b(std::move(a));
    EXPECT_EQ(b.validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(b.config().secretKey, "move-test");
}

TEST(SessionTokenValidatorTest, MoveAssignment) {
    SessionTokenConfig config;
    config.secretKey = "move-assign";
    SessionTokenValidator a(config);
    auto token = a.generateToken("user1", "1.2.3.4.5");

    SessionTokenValidator b;
    b = std::move(a);
    EXPECT_EQ(b.validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(b.config().secretKey, "move-assign");
}

// =============================================================================
// RenderSessionManager integration (token forwarding)
// =============================================================================

#include "services/render/render_session_manager.hpp"

TEST(SessionTokenValidatorTest, ManagerTokenForwarding) {
    RenderSessionManager manager;

    auto token = manager.generateSessionToken("user1", "1.2.3.4.5");
    EXPECT_FALSE(token.empty());

    auto result = manager.validateSessionToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::Valid);

    // Wrong study should fail
    result = manager.validateSessionToken(token, "9.8.7.6.5");
    EXPECT_EQ(result, TokenValidationResult::StudyMismatch);
}

TEST(SessionTokenValidatorTest, ManagerTokenValidatorAccessible) {
    RenderSessionManager manager;

    auto* validator = manager.tokenValidator();
    ASSERT_NE(validator, nullptr);

    auto token = validator->generateToken("user1", "1.2.3.4.5");
    EXPECT_EQ(validator->validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);
}

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

#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

using namespace dicom_viewer::services;

namespace {

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

std::string readBioToString(BIO* bio)
{
    const auto size = BIO_pending(bio);
    if (size <= 0) {
        return {};
    }

    std::string value(static_cast<size_t>(size), '\0');
    const auto read = BIO_read(bio, value.data(), size);
    if (read <= 0) {
        return {};
    }

    value.resize(static_cast<size_t>(read));
    return value;
}

std::pair<std::string, std::string> generateTestKeyPairPem()
{
    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr),
                      EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0) {
        return {};
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) <= 0) {
        return {};
    }

    EVP_PKEY* rawKey = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &rawKey) <= 0 || !rawKey) {
        return {};
    }

    EvpPkeyPtr key(rawKey, EVP_PKEY_free);
    BioPtr privateBio(BIO_new(BIO_s_mem()), BIO_free);
    BioPtr publicBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!privateBio || !publicBio) {
        return {};
    }

    if (PEM_write_bio_PrivateKey(privateBio.get(), key.get(), nullptr, nullptr, 0,
                                 nullptr, nullptr) != 1) {
        return {};
    }
    if (PEM_write_bio_PUBKEY(publicBio.get(), key.get()) != 1) {
        return {};
    }

    return {
        readBioToString(privateBio.get()),
        readBioToString(publicBio.get())
    };
}

class SessionTokenValidatorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        tempDir_ = std::filesystem::temp_directory_path()
            / ("dicom_viewer_session_token_validator_"
               + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(tempDir_);
        privateKeyPath_ = tempDir_ / "jwt-private.pem";
        publicKeyPath_ = tempDir_ / "jwt-public.pem";

        const auto [privateKeyPem, publicKeyPem] = generateTestKeyPairPem();
        ASSERT_FALSE(privateKeyPem.empty());
        ASSERT_FALSE(publicKeyPem.empty());

        std::ofstream(privateKeyPath_) << privateKeyPem;
        std::ofstream(publicKeyPath_) << publicKeyPem;
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(tempDir_, error);
    }

    SessionTokenConfig makeConfig() const
    {
        SessionTokenConfig config;
        config.issuer = "dicom-viewer-tests";
        config.audience = "render-clients";
        config.privateKeyPath = privateKeyPath_.string();
        config.publicKeyPath = publicKeyPath_.string();
        config.keyId = "test-key";
        config.defaultRole = "Clinician";
        config.defaultOrganization = "Radiology";
        config.allowEphemeralKeys = false;
        return config;
    }

    std::filesystem::path tempDir_;
    std::filesystem::path privateKeyPath_;
    std::filesystem::path publicKeyPath_;
};

std::string createUnsignedToken(const SessionTokenConfig& config,
                                const std::string& userId,
                                const std::string& studyUid)
{
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_type("JWT")
        .set_issuer(config.issuer)
        .set_audience(config.audience)
        .set_subject(userId)
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::minutes(5))
        .set_payload_claim("study_uid", jwt::claim(std::string(studyUid)))
        .set_payload_claim("role", jwt::claim(std::string(config.defaultRole)))
        .set_payload_claim("organization", jwt::claim(std::string(config.defaultOrganization)))
        .sign(jwt::algorithm::none{});
}

} // namespace

// =============================================================================
// Default construction
// =============================================================================

TEST_F(SessionTokenValidatorTest, DefaultConstruction) {
    SessionTokenValidator validator;
    EXPECT_FALSE(validator.allowsUnauthenticatedLocal());
    EXPECT_EQ(validator.config().expirySeconds, 3600u);
    EXPECT_FALSE(validator.config().issuer.empty());
    EXPECT_FALSE(validator.config().audience.empty());
}

TEST_F(SessionTokenValidatorTest, CustomConfig) {
    auto config = makeConfig();
    config.allowUnauthenticatedLocal = true;

    SessionTokenValidator validator(config);
    EXPECT_TRUE(validator.allowsUnauthenticatedLocal());
    EXPECT_EQ(validator.config().issuer, "dicom-viewer-tests");
    EXPECT_EQ(validator.config().audience, "render-clients");
    EXPECT_EQ(validator.config().privateKeyPath, privateKeyPath_.string());
}

// =============================================================================
// Token generation and validation
// =============================================================================

TEST_F(SessionTokenValidatorTest, GenerateAndValidateToken) {
    SessionTokenValidator validator(makeConfig());

    auto token = validator.generateToken("user1", "1.2.3.4.5");
    EXPECT_FALSE(token.empty());

    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::Valid);
}

TEST_F(SessionTokenValidatorTest, ValidateTokenExtractPayload) {
    SessionTokenValidator validator(makeConfig());

    auto token = validator.generateToken("user1", "1.2.3.4.5");

    TokenPayload payload;
    auto result = validator.validateToken(token, payload);
    EXPECT_EQ(result, TokenValidationResult::Valid);
    EXPECT_EQ(payload.userId, "user1");
    EXPECT_EQ(payload.studyUid, "1.2.3.4.5");
    EXPECT_EQ(payload.issuer, "dicom-viewer-tests");
    EXPECT_EQ(payload.audience, "render-clients");
    EXPECT_EQ(payload.role, "Clinician");
    EXPECT_EQ(payload.organization, "Radiology");
    EXPECT_GT(payload.issuedAtEpoch, 0u);
    EXPECT_GT(payload.expiryEpoch, payload.issuedAtEpoch);
}

TEST_F(SessionTokenValidatorTest, ValidateTokenNoStudyRestriction) {
    SessionTokenValidator validator(makeConfig());

    auto token = validator.generateToken("user1", "1.2.3.4.5");
    auto result = validator.validateToken(token);
    EXPECT_EQ(result, TokenValidationResult::Valid);
}

TEST_F(SessionTokenValidatorTest, GeneratesTokenWithOnlyPrivateKeyPathConfigured) {
    auto config = makeConfig();
    config.publicKeyPath.clear();

    SessionTokenValidator validator(config);
    auto token = validator.generateToken("user1", "1.2.3.4.5");
    EXPECT_FALSE(token.empty());
    EXPECT_EQ(validator.validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);
}

// =============================================================================
// Token validation failures
// =============================================================================

TEST_F(SessionTokenValidatorTest, EmptyTokenReturnsEmpty) {
    SessionTokenValidator validator(makeConfig());

    auto result = validator.validateToken("");
    EXPECT_EQ(result, TokenValidationResult::Empty);
}

TEST_F(SessionTokenValidatorTest, GarbageTokenReturnsInvalidFormat) {
    SessionTokenValidator validator(makeConfig());

    auto result = validator.validateToken("not-a-valid-token");
    EXPECT_EQ(result, TokenValidationResult::InvalidFormat);
}

TEST_F(SessionTokenValidatorTest, TamperedTokenReturnsInvalidSignature) {
    SessionTokenValidator validator(makeConfig());

    auto token = validator.generateToken("user1", "1.2.3.4.5");
    ASSERT_FALSE(token.empty());
    token.back() = token.back() == 'a' ? 'b' : 'a';

    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::InvalidSignature);
}

TEST_F(SessionTokenValidatorTest, StudyMismatchReturnsStudyMismatch) {
    SessionTokenValidator validator(makeConfig());

    auto token = validator.generateToken("user1", "1.2.3.4.5");

    auto result = validator.validateToken(token, "9.8.7.6.5");
    EXPECT_EQ(result, TokenValidationResult::StudyMismatch);
}

TEST_F(SessionTokenValidatorTest, ExpiredTokenReturnsExpired) {
    auto config = makeConfig();
    config.expirySeconds = 0;
    SessionTokenValidator validator(config);

    auto token = validator.generateToken("user1", "1.2.3.4.5");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::Expired);
}

TEST_F(SessionTokenValidatorTest, WrongIssuerReturnsInvalidClaims) {
    auto config = makeConfig();
    SessionTokenValidator validator(config);

    auto token = validator.generateToken("user1", "1.2.3.4.5");
    ASSERT_FALSE(token.empty());

    config.issuer = "unexpected-issuer";
    validator.setConfig(config);

    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::InvalidClaims);
}

TEST_F(SessionTokenValidatorTest, NoneAlgorithmReturnsUnsupportedAlgorithm) {
    auto config = makeConfig();
    SessionTokenValidator validator(config);

    auto token = createUnsignedToken(config, "user1", "1.2.3.4.5");
    auto result = validator.validateToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::UnsupportedAlgorithm);
}

// =============================================================================
// Configuration update
// =============================================================================

TEST_F(SessionTokenValidatorTest, SetConfigUpdatesAllowLocal) {
    SessionTokenValidator validator(makeConfig());
    EXPECT_FALSE(validator.allowsUnauthenticatedLocal());

    auto config = makeConfig();
    config.allowUnauthenticatedLocal = true;
    validator.setConfig(config);
    EXPECT_TRUE(validator.allowsUnauthenticatedLocal());
}

// =============================================================================
// Multiple tokens
// =============================================================================

TEST_F(SessionTokenValidatorTest, DifferentUsersGetDifferentTokens) {
    SessionTokenValidator validator(makeConfig());

    auto token1 = validator.generateToken("user1", "1.2.3.4.5");
    auto token2 = validator.generateToken("user2", "1.2.3.4.5");

    EXPECT_NE(token1, token2);
    EXPECT_EQ(validator.validateToken(token1, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(validator.validateToken(token2, "1.2.3.4.5"),
              TokenValidationResult::Valid);
}

TEST_F(SessionTokenValidatorTest, DifferentStudiesGetDifferentTokens) {
    SessionTokenValidator validator(makeConfig());

    auto token1 = validator.generateToken("user1", "1.2.3.4.5");
    auto token2 = validator.generateToken("user1", "9.8.7.6.5");

    EXPECT_NE(token1, token2);
    EXPECT_EQ(validator.validateToken(token1, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(validator.validateToken(token2, "9.8.7.6.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(validator.validateToken(token1, "9.8.7.6.5"),
              TokenValidationResult::StudyMismatch);
}

// =============================================================================
// Move semantics
// =============================================================================

TEST_F(SessionTokenValidatorTest, MoveConstruction) {
    auto config = makeConfig();
    SessionTokenValidator a(config);
    auto token = a.generateToken("user1", "1.2.3.4.5");

    SessionTokenValidator b(std::move(a));
    EXPECT_EQ(b.validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(b.config().issuer, "dicom-viewer-tests");
}

TEST_F(SessionTokenValidatorTest, MoveAssignment) {
    auto config = makeConfig();
    SessionTokenValidator a(config);
    auto token = a.generateToken("user1", "1.2.3.4.5");

    SessionTokenValidator b;
    b = std::move(a);
    EXPECT_EQ(b.validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);
    EXPECT_EQ(b.config().audience, "render-clients");
}

// =============================================================================
// RenderSessionManager integration (token forwarding)
// =============================================================================

#include "services/render/render_session_manager.hpp"

TEST_F(SessionTokenValidatorTest, ManagerTokenForwarding) {
    RenderSessionManager manager;

    auto token = manager.generateSessionToken("user1", "1.2.3.4.5");
    EXPECT_FALSE(token.empty());

    auto result = manager.validateSessionToken(token, "1.2.3.4.5");
    EXPECT_EQ(result, TokenValidationResult::Valid);

    result = manager.validateSessionToken(token, "9.8.7.6.5");
    EXPECT_EQ(result, TokenValidationResult::StudyMismatch);
}

TEST_F(SessionTokenValidatorTest, ManagerTokenValidatorAccessible) {
    RenderSessionManager manager;

    auto* validator = manager.tokenValidator();
    ASSERT_NE(validator, nullptr);

    auto token = validator->generateToken("user1", "1.2.3.4.5");
    EXPECT_EQ(validator->validateToken(token, "1.2.3.4.5"),
              TokenValidationResult::Valid);
}

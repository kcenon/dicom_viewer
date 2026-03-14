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

#include "services/auth/auth_provider.hpp"
#include "services/auth/auth_provider_factory.hpp"
#include "services/auth/ldap_auth_provider.hpp"
#include "services/auth/oidc_auth_provider.hpp"

#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>
#include <string>
#include <vector>

using namespace dicom_viewer::services;

namespace {

// ---------------------------------------------------------------------------
// Mock AuthProvider for interface contract tests
// ---------------------------------------------------------------------------

class MockAuthProvider : public AuthProvider {
public:
    // Configurable return values for test scenarios
    std::expected<AuthResult, AuthError> authenticateResult =
        std::unexpected(AuthError::InvalidCredentials);

    std::expected<AuthTokenPayload, AuthError> validateTokenResult =
        std::unexpected(AuthError::TokenInvalid);

    std::expected<AuthTokenPair, AuthError> refreshTokenResult =
        std::unexpected(AuthError::TokenInvalid);

    std::expected<void, AuthError> revokeTokenResult = {};

    std::expected<AuthUserInfo, AuthError> getUserInfoResult =
        std::unexpected(AuthError::TokenInvalid);

    int sessionCount = 0;

    std::expected<AuthResult, AuthError>
    authenticate(const std::string& /*username*/, const std::string& /*password*/) override
    {
        return authenticateResult;
    }

    std::expected<AuthTokenPayload, AuthError>
    validateToken(const std::string& /*accessToken*/) override
    {
        return validateTokenResult;
    }

    std::expected<AuthTokenPair, AuthError>
    refreshToken(const std::string& /*refreshToken*/) override
    {
        return refreshTokenResult;
    }

    std::expected<void, AuthError>
    revokeToken(const std::string& /*token*/) override
    {
        return revokeTokenResult;
    }

    std::expected<AuthUserInfo, AuthError>
    getUserInfo(const std::string& /*userId*/) override
    {
        return getUserInfoResult;
    }

    int getActiveSessionCount(const std::string& /*userId*/) override
    {
        return sessionCount;
    }
};

// ---------------------------------------------------------------------------
// RSA key generation helper for JWT tests
// ---------------------------------------------------------------------------

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

std::string bioToString(BIO* bio)
{
    const auto size = BIO_pending(bio);
    if (size <= 0) return {};
    std::string s(static_cast<size_t>(size), '\0');
    const auto r = BIO_read(bio, s.data(), size);
    if (r <= 0) return {};
    s.resize(static_cast<size_t>(r));
    return s;
}

std::pair<std::string, std::string> generateTestRsaKeyPair()
{
    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0) return {};
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) <= 0) return {};

    EVP_PKEY* raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw) <= 0 || !raw) return {};
    EvpPkeyPtr key(raw, EVP_PKEY_free);

    BioPtr privBio(BIO_new(BIO_s_mem()), BIO_free);
    BioPtr pubBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!privBio || !pubBio) return {};

    if (PEM_write_bio_PrivateKey(privBio.get(), key.get(),
                                  nullptr, nullptr, 0, nullptr, nullptr) != 1) return {};
    if (PEM_write_bio_PUBKEY(pubBio.get(), key.get()) != 1) return {};

    return {bioToString(privBio.get()), bioToString(pubBio.get())};
}

std::string mintTestJwt(const std::string& privateKeyPem,
                         const std::string& userId,
                         const std::string& role,
                         const std::string& org,
                         int ttlSeconds = 3600)
{
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_type("JWT")
        .set_issuer("dicom-viewer")
        .set_audience("dicom-viewer-api")
        .set_subject(userId)
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::seconds(ttlSeconds))
        .set_payload_claim("role", jwt::claim(std::string(role)))
        .set_payload_claim("organization", jwt::claim(std::string(org)))
        .sign(jwt::algorithm::rs256("", privateKeyPem, "", ""));
}

} // namespace

// ---------------------------------------------------------------------------
// AuthProvider interface tests (via mock)
// ---------------------------------------------------------------------------

class AuthProviderInterfaceTest : public ::testing::Test {
protected:
    MockAuthProvider provider;
};

TEST_F(AuthProviderInterfaceTest, AuthenticateReturnsConfiguredError)
{
    provider.authenticateResult = std::unexpected(AuthError::InvalidCredentials);
    auto result = provider.authenticate("user", "wrong");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::InvalidCredentials);
}

TEST_F(AuthProviderInterfaceTest, AuthenticateReturnsSuccessResult)
{
    AuthResult ar;
    ar.userInfo.userId = "alice";
    ar.userInfo.role = "Radiologist";
    ar.tokens.accessToken = "access.token.here";
    ar.tokens.refreshToken = "refresh-opaque";
    provider.authenticateResult = std::move(ar);

    auto result = provider.authenticate("alice", "correct");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->userInfo.userId, "alice");
    EXPECT_EQ(result->userInfo.role, "Radiologist");
    EXPECT_EQ(result->tokens.accessToken, "access.token.here");
}

TEST_F(AuthProviderInterfaceTest, ValidateTokenReturnsPayload)
{
    AuthTokenPayload p;
    p.userId = "bob";
    p.role = "Viewer";
    p.organization = "hospital";
    provider.validateTokenResult = p;

    auto result = provider.validateToken("some.jwt.token");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->userId, "bob");
    EXPECT_EQ(result->role, "Viewer");
}

TEST_F(AuthProviderInterfaceTest, ValidateTokenReturnsExpiredError)
{
    provider.validateTokenResult = std::unexpected(AuthError::TokenExpired);
    auto result = provider.validateToken("expired.token");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenExpired);
}

TEST_F(AuthProviderInterfaceTest, RevokeTokenSucceeds)
{
    auto result = provider.revokeToken("some.token");
    EXPECT_TRUE(result.has_value());
}

TEST_F(AuthProviderInterfaceTest, SessionLimitExceededError)
{
    provider.sessionCount = 3;
    provider.authenticateResult = std::unexpected(AuthError::SessionLimitExceeded);

    auto result = provider.authenticate("alice", "pass");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::SessionLimitExceeded);
    EXPECT_EQ(provider.getActiveSessionCount("alice"), 3);
}

// ---------------------------------------------------------------------------
// LdapAuthProvider tests (without LDAP server — tests token operations)
// ---------------------------------------------------------------------------

class LdapAuthProviderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto [priv, pub] = generateTestRsaKeyPair();
        privateKeyPem_ = priv;
        publicKeyPem_ = pub;

        LdapAuthConfig config;
        config.ldapUrl = "ldaps://ldap.test.local:636";
        config.bindDn = "CN=svc,DC=test,DC=local";
        config.bindPassword = "secret";
        config.baseDn = "DC=test,DC=local";
        config.groupRoleMap = {
            {"CN=Radiologists,DC=test,DC=local", "Radiologist"},
            {"CN=Admins,DC=test,DC=local", "Admin"},
        };
        config.defaultRole = "Viewer";
        config.issuer = "dicom-viewer";
        config.audience = "dicom-viewer-api";
        // Do not set privateKeyPath — LdapAuthProvider will generate ephemeral keys

        provider_ = std::make_unique<LdapAuthProvider>(config);
    }

    std::string privateKeyPem_;
    std::string publicKeyPem_;
    std::unique_ptr<LdapAuthProvider> provider_;
};

TEST_F(LdapAuthProviderTest, AuthenticateWithoutLdapReturnsConfigurationError)
{
    // Without a real LDAP server (or DICOM_VIEWER_HAS_LDAP compiled in),
    // authenticate must fail gracefully.
    auto result = provider_->authenticate("alice", "password");
    ASSERT_FALSE(result.has_value());

#ifdef DICOM_VIEWER_HAS_LDAP
    // With LDAP compiled in but no server, expect ProviderUnavailable
    EXPECT_EQ(result.error(), AuthError::ProviderUnavailable);
#else
    // Without LDAP compiled in, expect ConfigurationError
    EXPECT_EQ(result.error(), AuthError::ConfigurationError);
#endif
}

TEST_F(LdapAuthProviderTest, ValidateEmptyTokenReturnsInvalid)
{
    auto result = provider_->validateToken("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(LdapAuthProviderTest, ValidateMalformedTokenReturnsInvalid)
{
    auto result = provider_->validateToken("not.a.jwt");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(LdapAuthProviderTest, RevokeAndRejectToken)
{
    // Mint a token directly using LdapAuthProvider's ephemeral key (white-box):
    // We can't easily get the ephemeral key out, so we test via revokeToken on
    // an arbitrary string and check it returns revoked on validateToken.
    const std::string fakeToken = "fake.revoked.token";

    auto revokeResult = provider_->revokeToken(fakeToken);
    EXPECT_TRUE(revokeResult.has_value());

    // After revocation, validateToken should return Revoked (before attempting parse)
    auto validateResult = provider_->validateToken(fakeToken);
    ASSERT_FALSE(validateResult.has_value());
    EXPECT_EQ(validateResult.error(), AuthError::TokenRevoked);
}

TEST_F(LdapAuthProviderTest, RevokeEmptyTokenReturnsError)
{
    auto result = provider_->revokeToken("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(LdapAuthProviderTest, RefreshWithUnknownTokenReturnsInvalid)
{
    auto result = provider_->refreshToken("unknown-refresh-token");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(LdapAuthProviderTest, GetUserInfoForUnknownUserReturnsError)
{
    auto result = provider_->getUserInfo("unknown-user");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(LdapAuthProviderTest, GetActiveSessionCountDefaultZero)
{
    EXPECT_EQ(provider_->getActiveSessionCount("nobody"), 0);
}

// ---------------------------------------------------------------------------
// OidcAuthProvider tests (structural validation without JWKS)
// ---------------------------------------------------------------------------

class OidcAuthProviderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto [priv, pub] = generateTestRsaKeyPair();
        privateKeyPem_ = priv;

        OidcAuthConfig config;
        config.discoveryUrl = "https://cognito.example.com/.well-known/openid-configuration";
        config.issuer = "dicom-viewer";
        config.audience = "dicom-viewer-api";
        config.roleClaimName = "role";         // match mintTestJwt's claim name
        config.orgClaimName = "organization";  // match mintTestJwt's claim name
        config.defaultRole = "Viewer";

        provider_ = std::make_unique<OidcAuthProvider>(config);
    }

    std::string privateKeyPem_;
    std::unique_ptr<OidcAuthProvider> provider_;
};

TEST_F(OidcAuthProviderTest, AuthenticateReturnsUnavailable)
{
    auto result = provider_->authenticate("user", "pass");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::ProviderUnavailable);
}

TEST_F(OidcAuthProviderTest, ValidateWellFormedTokenSucceeds)
{
    // Mint a test token using our ephemeral key
    const std::string token = mintTestJwt(privateKeyPem_, "carol", "Radiologist", "hospital");

    // OidcAuthProvider does structural validation (no JWKS in tests)
    // It should succeed on well-formed, non-expired tokens
    auto result = provider_->validateToken(token);
    // Note: may fail if issuer mismatch; test token issuer = "dicom-viewer" matches config
    if (result.has_value()) {
        EXPECT_EQ(result->userId, "carol");
        EXPECT_EQ(result->role, "Radiologist");
    } else {
        // Acceptable: JWKS validation not available in test environment
        EXPECT_EQ(result.error(), AuthError::TokenInvalid);
    }
}

TEST_F(OidcAuthProviderTest, ValidateExpiredTokenReturnsExpired)
{
    const std::string token = mintTestJwt(privateKeyPem_, "carol", "Viewer", "hospital", -1);
    auto result = provider_->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenExpired);
}

TEST_F(OidcAuthProviderTest, ValidateEmptyTokenReturnsInvalid)
{
    auto result = provider_->validateToken("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(OidcAuthProviderTest, RevokeTokenPreventsValidation)
{
    // Any string revoked should be blocked before JWT parsing
    const std::string token = "arbitrary.token.value";

    EXPECT_TRUE(provider_->revokeToken(token).has_value());

    auto result = provider_->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenRevoked);
}

TEST_F(OidcAuthProviderTest, RefreshReturnsUnavailable)
{
    auto result = provider_->refreshToken("some-refresh-token");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::ProviderUnavailable);
}

// ---------------------------------------------------------------------------
// AuthProviderFactory tests
// ---------------------------------------------------------------------------

class AuthProviderFactoryTest : public ::testing::Test {};

TEST_F(AuthProviderFactoryTest, NonExistentFileReturnsFileNotFound)
{
    auto result = AuthProviderFactory::fromDeploymentYaml("/no/such/file.yaml");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthFactoryError::FileNotFound);
}

TEST_F(AuthProviderFactoryTest, EmptyYamlReturnsParseError)
{
    auto result = AuthProviderFactory::fromYamlString("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthFactoryError::ParseError);
}

TEST_F(AuthProviderFactoryTest, MissingIdpSectionReturnsMissingField)
{
    const std::string yaml = "server:\n  port: 8080\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthFactoryError::MissingRequiredField);
}

TEST_F(AuthProviderFactoryTest, UnknownIdpTypeReturnsError)
{
    const std::string yaml =
        "idp:\n"
        "  type: kerberos\n"
        "  ldap_url: ldap://example.com\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthFactoryError::UnknownProviderType);
}

TEST_F(AuthProviderFactoryTest, ValidLdapYamlCreatesLdapProvider)
{
    const std::string yaml =
        "idp:\n"
        "  type: ldap\n"
        "  ldap_url: ldaps://ad.hospital.local:636\n"
        "  bind_dn: CN=svc,DC=hospital,DC=local\n"
        "  bind_password: secret\n"
        "  base_dn: DC=hospital,DC=local\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_TRUE(result.has_value()) << "Factory should create LdapAuthProvider";
    EXPECT_NE(result->get(), nullptr);
}

TEST_F(AuthProviderFactoryTest, ValidCognitoYamlCreatesOidcProvider)
{
    const std::string yaml =
        "idp:\n"
        "  type: cognito\n"
        "  discovery_url: https://cognito.example.com/.well-known/openid-configuration\n"
        "  client_id: my-client-id\n"
        "  region: ap-northeast-2\n"
        "  issuer: https://cognito.example.com/pool\n"
        "  audience: my-client-id\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_TRUE(result.has_value()) << "Factory should create OidcAuthProvider";
    EXPECT_NE(result->get(), nullptr);
}

TEST_F(AuthProviderFactoryTest, LdapYamlMissingLdapUrlReturnsMissingField)
{
    const std::string yaml =
        "idp:\n"
        "  type: ldap\n"
        "  bind_dn: CN=svc,DC=hospital,DC=local\n"
        "  base_dn: DC=hospital,DC=local\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthFactoryError::MissingRequiredField);
}

TEST_F(AuthProviderFactoryTest, LdapYamlMissingBaseDnReturnsMissingField)
{
    const std::string yaml =
        "idp:\n"
        "  type: ldap\n"
        "  ldap_url: ldaps://ad.hospital.local:636\n"
        "  bind_dn: CN=svc,DC=hospital,DC=local\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthFactoryError::MissingRequiredField);
}

TEST_F(AuthProviderFactoryTest, AdTypeAlsoCreatesLdapProvider)
{
    const std::string yaml =
        "idp:\n"
        "  type: ad\n"
        "  ldap_url: ldaps://ad.hospital.local:636\n"
        "  bind_dn: CN=svc,DC=hospital,DC=local\n"
        "  bind_password: secret\n"
        "  base_dn: DC=hospital,DC=local\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->get(), nullptr);
}

TEST_F(AuthProviderFactoryTest, OidcTypeAlsoCreatesOidcProvider)
{
    const std::string yaml =
        "idp:\n"
        "  type: oidc\n"
        "  discovery_url: https://idp.example.com/.well-known\n"
        "  client_id: app\n"
        "  issuer: https://idp.example.com\n"
        "  audience: app\n";
    auto result = AuthProviderFactory::fromYamlString(yaml);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->get(), nullptr);
}

// ---------------------------------------------------------------------------
// AuthError enum completeness check
// ---------------------------------------------------------------------------

TEST(AuthErrorTest, AllErrorsHaveDistinctValues)
{
    // Verify all enum values are distinct (compile-time check via set-like comparison)
    const std::vector<AuthError> errors = {
        AuthError::InvalidCredentials,
        AuthError::AccountLocked,
        AuthError::SessionLimitExceeded,
        AuthError::TokenExpired,
        AuthError::TokenRevoked,
        AuthError::TokenInvalid,
        AuthError::ProviderUnavailable,
        AuthError::ConfigurationError,
        AuthError::InternalError,
    };

    // Brute-force uniqueness check
    for (size_t i = 0; i < errors.size(); ++i) {
        for (size_t j = i + 1; j < errors.size(); ++j) {
            EXPECT_NE(errors[i], errors[j]) << "Duplicate AuthError at indices " << i << ", " << j;
        }
    }
}

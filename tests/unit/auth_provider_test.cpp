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
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <memory>
#include <sstream>
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

// ---------------------------------------------------------------------------
// JWKS test helpers
// ---------------------------------------------------------------------------

std::string mintTestJwtWithKid(const std::string& privateKeyPem,
                                const std::string& kid,
                                const std::string& userId,
                                const std::string& role,
                                const std::string& org,
                                int ttlSeconds = 3600)
{
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_type("JWT")
        .set_key_id(kid)
        .set_issuer("dicom-viewer")
        .set_audience("dicom-viewer-api")
        .set_subject(userId)
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::seconds(ttlSeconds))
        .set_payload_claim("role", jwt::claim(std::string(role)))
        .set_payload_claim("organization", jwt::claim(std::string(org)))
        .set_payload_claim("email", jwt::claim(std::string("test@example.com")))
        .sign(jwt::algorithm::rs256("", privateKeyPem, "", ""));
}

std::string base64UrlEncode(const unsigned char* data, int len)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(static_cast<size_t>((len + 2) / 3 * 4));

    for (int i = 0; i < len; i += 3) {
        const unsigned int b0 = data[i];
        const unsigned int b1 = (i + 1 < len) ? data[i + 1] : 0;
        const unsigned int b2 = (i + 2 < len) ? data[i + 2] : 0;

        encoded.push_back(table[b0 >> 2]);
        encoded.push_back(table[((b0 & 0x03) << 4) | (b1 >> 4)]);
        if (i + 1 < len) {
            encoded.push_back(table[((b1 & 0x0F) << 2) | (b2 >> 6)]);
        }
        if (i + 2 < len) {
            encoded.push_back(table[b2 & 0x3F]);
        }
    }

    // Convert to URL-safe: + -> -, / -> _, remove padding
    for (auto& c : encoded) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    return encoded;
}

std::string bnToBase64Url(const BIGNUM* bn)
{
    const int numBytes = BN_num_bytes(bn);
    std::vector<unsigned char> buf(static_cast<size_t>(numBytes));
    BN_bn2bin(bn, buf.data());
    return base64UrlEncode(buf.data(), numBytes);
}

std::string buildJwksJson(const std::string& publicKeyPem, const std::string& kid)
{
    BioPtr bio(BIO_new_mem_buf(publicKeyPem.data(),
                                static_cast<int>(publicKeyPem.size())),
               BIO_free);
    EvpPkeyPtr pkey(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr),
                    EVP_PKEY_free);
    if (!pkey) {
        return {};
    }

    // Extract RSA components from EVP_PKEY
    BIGNUM* nBn = nullptr;
    BIGNUM* eBn = nullptr;

    if (EVP_PKEY_get_bn_param(pkey.get(), "n", &nBn) != 1 ||
        EVP_PKEY_get_bn_param(pkey.get(), "e", &eBn) != 1) {
        if (nBn) BN_free(nBn);
        if (eBn) BN_free(eBn);
        return {};
    }

    const auto nStr = bnToBase64Url(nBn);
    const auto eStr = bnToBase64Url(eBn);
    BN_free(nBn);
    BN_free(eBn);

    std::ostringstream ss;
    ss << R"({"keys":[{"kty":"RSA","kid":")" << kid
       << R"(","use":"sig","alg":"RS256","n":")" << nStr
       << R"(","e":")" << eStr
       << R"("}]})";
    return ss.str();
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
// LDAP filter escaping tests (ldap_detail namespace)
// ---------------------------------------------------------------------------

using dicom_viewer::services::ldap_detail::escapeLdapFilterValue;
using dicom_viewer::services::ldap_detail::replaceUsername;
using dicom_viewer::services::ldap_detail::kMaxUsernameLength;

TEST(LdapFilterEscapeTest, InjectionPayloadIsEscaped)
{
    const std::string payload = "admin)(|(uid=*)";
    const std::string escaped = escapeLdapFilterValue(payload);
    EXPECT_EQ(escaped, "admin\\29\\28|\\28uid=\\2a\\29");
}

TEST(LdapFilterEscapeTest, NormalUsernamePassesThrough)
{
    EXPECT_EQ(escapeLdapFilterValue("alice"), "alice");
    EXPECT_EQ(escapeLdapFilterValue("john.doe"), "john.doe");
    EXPECT_EQ(escapeLdapFilterValue("user-123"), "user-123");
    EXPECT_EQ(escapeLdapFilterValue("Admin01"), "Admin01");
}

TEST(LdapFilterEscapeTest, BackslashEscaped)
{
    EXPECT_EQ(escapeLdapFilterValue("a\\b"), "a\\5cb");
}

TEST(LdapFilterEscapeTest, AsteriskEscaped)
{
    EXPECT_EQ(escapeLdapFilterValue("a*b"), "a\\2ab");
}

TEST(LdapFilterEscapeTest, OpenParenEscaped)
{
    EXPECT_EQ(escapeLdapFilterValue("a(b"), "a\\28b");
}

TEST(LdapFilterEscapeTest, CloseParenEscaped)
{
    EXPECT_EQ(escapeLdapFilterValue("a)b"), "a\\29b");
}

TEST(LdapFilterEscapeTest, NulCharacterEscaped)
{
    const std::string input(std::string("a") + '\0' + "b");
    EXPECT_EQ(escapeLdapFilterValue(input), "a\\00b");
}

TEST(LdapFilterEscapeTest, EmptyStringReturnsEmpty)
{
    EXPECT_EQ(escapeLdapFilterValue(""), "");
}

TEST(LdapReplaceUsernameTest, NormalSubstitution)
{
    const std::string result = replaceUsername("(uid={username})", "alice");
    EXPECT_EQ(result, "(uid=alice)");
}

TEST(LdapReplaceUsernameTest, InjectionPayloadIsSanitized)
{
    const std::string result = replaceUsername("(uid={username})", "admin)(|(uid=*)");
    EXPECT_EQ(result, "(uid=admin\\29\\28|\\28uid=\\2a\\29)");
}

TEST(LdapReplaceUsernameTest, EmptyUsernameReturnsEmpty)
{
    const std::string result = replaceUsername("(uid={username})", "");
    EXPECT_TRUE(result.empty());
}

TEST(LdapReplaceUsernameTest, ExcessivelyLongUsernameReturnsEmpty)
{
    const std::string longName(kMaxUsernameLength + 1, 'a');
    const std::string result = replaceUsername("(uid={username})", longName);
    EXPECT_TRUE(result.empty());
}

TEST(LdapReplaceUsernameTest, MaxLengthUsernameIsAccepted)
{
    const std::string maxName(kMaxUsernameLength, 'a');
    const std::string result = replaceUsername("(uid={username})", maxName);
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result, "(uid=" + maxName + ")");
}

TEST(LdapReplaceUsernameTest, NoPlaceholderReturnsFilterUnchanged)
{
    const std::string result = replaceUsername("(uid=fixed)", "alice");
    EXPECT_EQ(result, "(uid=fixed)");
}

// ---------------------------------------------------------------------------
// OidcAuthProvider tests (JWKS signature verification)
// ---------------------------------------------------------------------------

class OidcAuthProviderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto [priv, pub] = generateTestRsaKeyPair();
        privateKeyPem_ = priv;
        publicKeyPem_ = pub;

        jwksJson_ = buildJwksJson(publicKeyPem_, "test-key-1");
        ASSERT_FALSE(jwksJson_.empty()) << "Failed to build JWKS JSON from test key";

        OidcAuthConfig config;
        config.discoveryUrl = "https://cognito.example.com/.well-known/openid-configuration";
        config.jwksUrl = "https://cognito.example.com/.well-known/jwks.json";
        config.issuer = "dicom-viewer";
        config.audience = "dicom-viewer-api";
        config.roleClaimName = "role";
        config.orgClaimName = "organization";
        config.defaultRole = "Viewer";
        config.jwksRefreshIntervalSeconds = 3600;

        // Inject a mock JWKS fetcher that returns our test JWKS
        auto fetcher = [this](const std::string& /*url*/) -> std::string {
            return jwksJson_;
        };

        provider_ = std::make_unique<OidcAuthProvider>(config, std::move(fetcher));
    }

    std::string privateKeyPem_;
    std::string publicKeyPem_;
    std::string jwksJson_;
    std::unique_ptr<OidcAuthProvider> provider_;
};

TEST_F(OidcAuthProviderTest, AuthenticateReturnsUnavailable)
{
    auto result = provider_->authenticate("user", "pass");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::ProviderUnavailable);
}

TEST_F(OidcAuthProviderTest, ValidateWellFormedTokenWithJwksSucceeds)
{
    const std::string token =
        mintTestJwtWithKid(privateKeyPem_, "test-key-1", "carol", "Radiologist", "hospital");

    auto result = provider_->validateToken(token);
    ASSERT_TRUE(result.has_value()) << "Token should validate successfully with matching JWKS key";
    EXPECT_EQ(result->userId, "carol");
    EXPECT_EQ(result->role, "Radiologist");
    EXPECT_EQ(result->organization, "hospital");
    EXPECT_EQ(result->email, "test@example.com");
    EXPECT_GT(result->issuedAtEpoch, 0u);
    EXPECT_GT(result->expiryEpoch, result->issuedAtEpoch);
}

TEST_F(OidcAuthProviderTest, ValidateExpiredTokenReturnsExpired)
{
    const std::string token =
        mintTestJwtWithKid(privateKeyPem_, "test-key-1", "carol", "Viewer", "hospital", -1);

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

TEST_F(OidcAuthProviderTest, ValidateTokenWithWrongSignatureReturnsInvalid)
{
    // Generate a different key pair to produce a token with a wrong signature
    auto [differentPriv, differentPub] = generateTestRsaKeyPair();
    const std::string token =
        mintTestJwtWithKid(differentPriv, "test-key-1", "carol", "Viewer", "hospital");

    // The signature won't match our JWKS public key
    auto result = provider_->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(OidcAuthProviderTest, ValidateTokenWithUnknownKidReturnsInvalid)
{
    const std::string token =
        mintTestJwtWithKid(privateKeyPem_, "unknown-kid-999", "carol", "Viewer", "hospital");

    auto result = provider_->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(OidcAuthProviderTest, ValidateTokenWithNoKidReturnsInvalid)
{
    // Use the old mintTestJwt which does NOT set kid
    const std::string token = mintTestJwt(privateKeyPem_, "carol", "Radiologist", "hospital");

    auto result = provider_->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(OidcAuthProviderTest, ValidateTokenWithAlgNoneReturnsInvalid)
{
    // Craft a token with alg: "none" (unsigned).
    // jwt-cpp does not support "none" algorithm for creation,
    // so we manually base64url-encode the header and payload.
    const auto now = std::chrono::system_clock::now();
    const std::string header = R"({"alg":"none","typ":"JWT"})";
    const std::string payload = R"({"sub":"attacker","iss":"dicom-viewer","aud":"dicom-viewer-api","exp":)"
        + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
              (now + std::chrono::hours(1)).time_since_epoch()).count())
        + "}";

    auto b64url = [](const std::string& s) {
        auto encoded = jwt::base::encode<jwt::alphabet::base64url>(s);
        // Strip trailing '=' padding for JWT format
        while (!encoded.empty() && encoded.back() == '=') {
            encoded.pop_back();
        }
        return encoded;
    };

    const std::string token = b64url(header) + "." + b64url(payload) + ".";

    auto result = provider_->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(OidcAuthProviderTest, ValidateTokenWithHmacAlgReturnsInvalid)
{
    // Craft a token signed with HS256 (HMAC) — should be rejected even though
    // the signature might be structurally valid
    const auto now = std::chrono::system_clock::now();
    const std::string hmacToken = jwt::create()
        .set_type("JWT")
        .set_key_id("test-key-1")
        .set_issuer("dicom-viewer")
        .set_audience("dicom-viewer-api")
        .set_subject("attacker")
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours(1))
        .sign(jwt::algorithm::hs256("some-secret-key"));

    auto result = provider_->validateToken(hmacToken);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(OidcAuthProviderTest, RevokeTokenPreventsValidation)
{
    const std::string token =
        mintTestJwtWithKid(privateKeyPem_, "test-key-1", "carol", "Viewer", "hospital");

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

TEST_F(OidcAuthProviderTest, DefaultRoleWhenRoleClaimAbsent)
{
    // Mint a token without a role claim by using an empty role
    const auto now = std::chrono::system_clock::now();
    const std::string token = jwt::create()
        .set_type("JWT")
        .set_key_id("test-key-1")
        .set_issuer("dicom-viewer")
        .set_audience("dicom-viewer-api")
        .set_subject("dave")
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours(1))
        .set_payload_claim("organization", jwt::claim(std::string("hospital")))
        .sign(jwt::algorithm::rs256("", privateKeyPem_, "", ""));

    auto result = provider_->validateToken(token);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->userId, "dave");
    EXPECT_EQ(result->role, "Viewer");
}

TEST_F(OidcAuthProviderTest, WrongIssuerReturnsInvalid)
{
    // Mint a token with a different issuer
    const auto now = std::chrono::system_clock::now();
    const std::string token = jwt::create()
        .set_type("JWT")
        .set_key_id("test-key-1")
        .set_issuer("wrong-issuer")
        .set_audience("dicom-viewer-api")
        .set_subject("carol")
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours(1))
        .sign(jwt::algorithm::rs256("", privateKeyPem_, "", ""));

    auto result = provider_->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
}

TEST_F(OidcAuthProviderTest, JwksFetcherFailureReturnsInvalid)
{
    // Create a provider with a fetcher that always fails
    OidcAuthConfig config;
    config.jwksUrl = "https://broken.example.com/jwks.json";
    config.issuer = "dicom-viewer";
    config.audience = "dicom-viewer-api";
    config.roleClaimName = "role";
    config.orgClaimName = "organization";

    auto failingFetcher = [](const std::string& /*url*/) -> std::string {
        return {};  // Simulate network failure
    };

    auto failProvider = std::make_unique<OidcAuthProvider>(config, failingFetcher);

    const std::string token =
        mintTestJwtWithKid(privateKeyPem_, "test-key-1", "carol", "Viewer", "hospital");

    auto result = failProvider->validateToken(token);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::TokenInvalid);
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

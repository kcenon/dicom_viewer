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

#include "services/auth/ldap_auth_provider.hpp"

#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#ifdef DICOM_VIEWER_HAS_LDAP
#include <ldap.h>
#include <lber.h>
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace dicom_viewer::services {

namespace {

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

struct KeyMaterial {
    std::string privateKeyPem;
    std::string publicKeyPem;
};

struct RefreshTokenInfo {
    std::string userId;
    std::string role;
    std::string organization;
    std::string email;
    uint64_t expiresAt = 0;
};

std::string readFileIfPresent(const std::string& path)
{
    if (path.empty() || !std::filesystem::exists(path)) {
        return {};
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

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

std::string derivePublicKeyPem(const std::string& privateKeyPem)
{
    if (privateKeyPem.empty()) {
        return {};
    }

    BioPtr privateBio(BIO_new_mem_buf(privateKeyPem.data(),
                                       static_cast<int>(privateKeyPem.size())),
                      BIO_free);
    if (!privateBio) {
        return {};
    }

    EvpPkeyPtr key(PEM_read_bio_PrivateKey(privateBio.get(), nullptr, nullptr, nullptr),
                   EVP_PKEY_free);
    if (!key) {
        return {};
    }

    BioPtr publicBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!publicBio || PEM_write_bio_PUBKEY(publicBio.get(), key.get()) != 1) {
        return {};
    }

    return readBioToString(publicBio.get());
}

KeyMaterial generateEphemeralKeyMaterial()
{
    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
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

    if (PEM_write_bio_PrivateKey(privateBio.get(), key.get(),
                                  nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        return {};
    }
    if (PEM_write_bio_PUBKEY(publicBio.get(), key.get()) != 1) {
        return {};
    }

    return {readBioToString(privateBio.get()), readBioToString(publicBio.get())};
}

KeyMaterial loadOrGenerateKeys(const std::string& privateKeyPath,
                                const std::string& publicKeyPath)
{
    KeyMaterial km;
    km.privateKeyPem = readFileIfPresent(privateKeyPath);
    km.publicKeyPem = readFileIfPresent(publicKeyPath);

    if (km.publicKeyPem.empty() && !km.privateKeyPem.empty()) {
        km.publicKeyPem = derivePublicKeyPem(km.privateKeyPem);
    }

    if (km.privateKeyPem.empty() && km.publicKeyPem.empty()) {
        km = generateEphemeralKeyMaterial();
    }

    return km;
}

uint64_t nowEpochSeconds()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string generateOpaqueToken()
{
    constexpr int kBytes = 32;
    unsigned char buf[kBytes];
    if (RAND_bytes(buf, kBytes) != 1) {
        return {};
    }

    std::ostringstream ss;
    for (auto b : buf) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return ss.str();
}

} // namespace

// ---------------------------------------------------------------------------
// LDAP filter escaping (ldap_detail -- visible to tests)
// ---------------------------------------------------------------------------

namespace ldap_detail {

std::string escapeLdapFilterValue(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());

    for (unsigned char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\5c"; break;
        case '*':  escaped += "\\2a"; break;
        case '(':  escaped += "\\28"; break;
        case ')':  escaped += "\\29"; break;
        case '\0': escaped += "\\00"; break;
        default:   escaped += static_cast<char>(ch); break;
        }
    }

    return escaped;
}

std::string replaceUsername(const std::string& filter, const std::string& username)
{
    const std::string placeholder = "{username}";
    const auto pos = filter.find(placeholder);
    if (pos == std::string::npos) {
        return filter;
    }

    if (username.empty() || username.size() > kMaxUsernameLength) {
        return {};
    }

    const std::string safe = escapeLdapFilterValue(username);
    return filter.substr(0, pos) + safe + filter.substr(pos + placeholder.size());
}

} // namespace ldap_detail

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

class LdapAuthProvider::Impl {
public:
    explicit Impl(const LdapAuthConfig& config)
        : config_(config)
        , keyMaterial_(loadOrGenerateKeys(config.privateKeyPath, config.publicKeyPath))
    {
    }

    std::expected<AuthResult, AuthError>
    authenticate(const std::string& username, const std::string& password)
    {
        {
            std::lock_guard lock(sessionMutex_);
            const auto it = sessionCounts_.find(username);
            if (it != sessionCounts_.end() && it->second >= config_.maxConcurrentSessions) {
                return std::unexpected(AuthError::SessionLimitExceeded);
            }
        }

        auto userInfoResult = ldapAuthenticate(username, password);
        if (!userInfoResult) {
            return std::unexpected(userInfoResult.error());
        }

        AuthUserInfo& userInfo = *userInfoResult;
        auto tokens = issueTokenPair(userInfo);
        if (!tokens) {
            return std::unexpected(AuthError::InternalError);
        }

        {
            std::lock_guard lock(sessionMutex_);
            sessionCounts_[username]++;
        }

        {
            std::lock_guard lock(userCacheMutex_);
            userCache_[userInfo.userId] = userInfo;
        }

        return AuthResult{std::move(userInfo), std::move(*tokens)};
    }

    std::expected<AuthTokenPayload, AuthError>
    validateToken(const std::string& accessToken)
    {
        if (accessToken.empty()) {
            return std::unexpected(AuthError::TokenInvalid);
        }

        {
            std::lock_guard lock(blacklistMutex_);
            if (revokedTokens_.count(accessToken)) {
                return std::unexpected(AuthError::TokenRevoked);
            }
        }

        if (keyMaterial_.publicKeyPem.empty()) {
            return std::unexpected(AuthError::ConfigurationError);
        }

        try {
            auto decoded = jwt::decode(accessToken);

            if (!decoded.has_algorithm() || decoded.get_algorithm() != "RS256") {
                return std::unexpected(AuthError::TokenInvalid);
            }

            std::error_code ec;
            jwt::verify()
                .allow_algorithm(jwt::algorithm::rs256(keyMaterial_.publicKeyPem, "", "", ""))
                .with_issuer(config_.issuer)
                .with_audience(config_.audience)
                .verify(decoded, ec);

            if (ec) {
                using jwt::error::token_verification_error;
                if (ec == token_verification_error::token_expired) {
                    return std::unexpected(AuthError::TokenExpired);
                }
                return std::unexpected(AuthError::TokenInvalid);
            }

            AuthTokenPayload payload;
            payload.userId = decoded.get_subject();
            if (decoded.has_payload_claim("role")) {
                payload.role = decoded.get_payload_claim("role").as_string();
            }
            if (decoded.has_payload_claim("organization")) {
                payload.organization = decoded.get_payload_claim("organization").as_string();
            }
            if (decoded.has_payload_claim("email")) {
                payload.email = decoded.get_payload_claim("email").as_string();
            }
            if (decoded.has_issued_at()) {
                payload.issuedAtEpoch = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                        decoded.get_issued_at().time_since_epoch()).count());
            }
            if (decoded.has_expires_at()) {
                payload.expiryEpoch = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                        decoded.get_expires_at().time_since_epoch()).count());
            }

            if (payload.userId.empty()) {
                return std::unexpected(AuthError::TokenInvalid);
            }

            return payload;
        } catch (...) {
            return std::unexpected(AuthError::TokenInvalid);
        }
    }

    std::expected<AuthTokenPair, AuthError>
    refreshToken(const std::string& refreshToken)
    {
        if (refreshToken.empty()) {
            return std::unexpected(AuthError::TokenInvalid);
        }

        {
            std::lock_guard lock(blacklistMutex_);
            if (revokedTokens_.count(refreshToken)) {
                return std::unexpected(AuthError::TokenRevoked);
            }
        }

        RefreshTokenInfo info;
        {
            std::lock_guard lock(refreshMutex_);
            const auto it = refreshTokens_.find(refreshToken);
            if (it == refreshTokens_.end()) {
                return std::unexpected(AuthError::TokenInvalid);
            }
            info = it->second;
        }

        if (nowEpochSeconds() > info.expiresAt) {
            std::lock_guard lock(refreshMutex_);
            refreshTokens_.erase(refreshToken);
            return std::unexpected(AuthError::TokenExpired);
        }

        AuthUserInfo fakeInfo;
        fakeInfo.userId = info.userId;
        fakeInfo.role = info.role;
        fakeInfo.organization = info.organization;
        fakeInfo.email = info.email;

        auto tokens = issueTokenPair(fakeInfo);
        if (!tokens) {
            return std::unexpected(AuthError::InternalError);
        }

        return *tokens;
    }

    std::expected<void, AuthError>
    revokeToken(const std::string& token)
    {
        if (token.empty()) {
            return std::unexpected(AuthError::TokenInvalid);
        }

        {
            std::lock_guard lock(blacklistMutex_);
            revokedTokens_.insert(token);
        }

        {
            std::lock_guard lock(refreshMutex_);
            const auto it = refreshTokens_.find(token);
            if (it != refreshTokens_.end()) {
                // Decrement session count when refresh token is revoked
                std::lock_guard sessionLock(sessionMutex_);
                auto& count = sessionCounts_[it->second.userId];
                if (count > 0) {
                    --count;
                }
                refreshTokens_.erase(it);
            }
        }

        return {};
    }

    std::expected<AuthUserInfo, AuthError>
    getUserInfo(const std::string& userId)
    {
        std::lock_guard lock(userCacheMutex_);
        const auto it = userCache_.find(userId);
        if (it == userCache_.end()) {
            return std::unexpected(AuthError::TokenInvalid);
        }
        return it->second;
    }

    int getActiveSessionCount(const std::string& userId)
    {
        std::lock_guard lock(sessionMutex_);
        const auto it = sessionCounts_.find(userId);
        return (it != sessionCounts_.end()) ? it->second : 0;
    }

private:

#ifdef DICOM_VIEWER_HAS_LDAP

    std::expected<AuthUserInfo, AuthError>
    ldapAuthenticate(const std::string& username, const std::string& password)
    {
        LDAP* ld = nullptr;
        int rc = ldap_initialize(&ld, config_.ldapUrl.c_str());
        if (rc != LDAP_SUCCESS || !ld) {
            return std::unexpected(AuthError::ProviderUnavailable);
        }

        struct LdapGuard {
            LDAP* ld;
            ~LdapGuard() { if (ld) ldap_unbind_ext_s(ld, nullptr, nullptr); }
        } guard{ld};

        // Require LDAPv3
        int version = LDAP_VERSION3;
        ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &version);

        // Configure TLS options
        if (config_.verifyServerCert) {
            int certReq = LDAP_OPT_X_TLS_DEMAND;
            ldap_set_option(nullptr, LDAP_OPT_X_TLS_REQUIRE_CERT, &certReq);
        } else {
            int certReq = LDAP_OPT_X_TLS_NEVER;
            ldap_set_option(nullptr, LDAP_OPT_X_TLS_REQUIRE_CERT, &certReq);
        }

        if (!config_.caCertPath.empty()) {
            ldap_set_option(nullptr, LDAP_OPT_X_TLS_CACERTFILE, config_.caCertPath.c_str());
        }

        // Set connection timeout
        struct timeval timeout;
        timeout.tv_sec = config_.connectionTimeoutMs / 1000;
        timeout.tv_usec = (config_.connectionTimeoutMs % 1000) * 1000;
        ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &timeout);

        // Bind service account
        berval bindPw;
        bindPw.bv_val = const_cast<char*>(config_.bindPassword.c_str());
        bindPw.bv_len = config_.bindPassword.size();

        rc = ldap_sasl_bind_s(ld, config_.bindDn.c_str(), LDAP_SASL_SIMPLE,
                               &bindPw, nullptr, nullptr, nullptr);
        if (rc != LDAP_SUCCESS) {
            return std::unexpected(AuthError::ProviderUnavailable);
        }

        // Search for user entry
        const std::string filter = ldap_detail::replaceUsername(config_.userSearchFilter, username);
        if (filter.empty()) {
            return std::unexpected(AuthError::InvalidCredentials);
        }
        const char* attrs[] = {"dn", "cn", "mail", "displayName",
                                config_.groupAttribute.c_str(), nullptr};

        LDAPMessage* result = nullptr;
        struct timeval searchTimeout;
        searchTimeout.tv_sec = config_.connectionTimeoutMs / 1000;
        searchTimeout.tv_usec = 0;

        rc = ldap_search_ext_s(ld, config_.baseDn.c_str(), LDAP_SCOPE_SUBTREE,
                                filter.c_str(),
                                const_cast<char**>(attrs),
                                0, nullptr, nullptr,
                                &searchTimeout, 1, &result);

        struct ResultGuard {
            LDAPMessage* msg;
            ~ResultGuard() { if (msg) ldap_msgfree(msg); }
        } resultGuard{result};

        if (rc != LDAP_SUCCESS || !result) {
            return std::unexpected(AuthError::ProviderUnavailable);
        }

        LDAPMessage* entry = ldap_first_entry(ld, result);
        if (!entry) {
            return std::unexpected(AuthError::InvalidCredentials);
        }

        char* userDn = ldap_get_dn(ld, entry);
        if (!userDn) {
            return std::unexpected(AuthError::InternalError);
        }
        const std::string userDnStr(userDn);
        ldap_memfree(userDn);

        // Verify user password by binding as the user
        berval userPw;
        userPw.bv_val = const_cast<char*>(password.c_str());
        userPw.bv_len = password.size();

        rc = ldap_sasl_bind_s(ld, userDnStr.c_str(), LDAP_SASL_SIMPLE,
                               &userPw, nullptr, nullptr, nullptr);
        if (rc == LDAP_INVALID_CREDENTIALS) {
            return std::unexpected(AuthError::InvalidCredentials);
        }
        if (rc != LDAP_SUCCESS) {
            return std::unexpected(AuthError::ProviderUnavailable);
        }

        // Extract user attributes
        AuthUserInfo userInfo;
        userInfo.userId = username;

        auto getAttr = [&](const char* attrName) -> std::string {
            berval** vals = ldap_get_values_len(ld, entry, attrName);
            if (!vals || !vals[0]) return {};
            std::string value(vals[0]->bv_val, vals[0]->bv_len);
            ldap_value_free_len(vals);
            return value;
        };

        userInfo.displayName = getAttr("displayName");
        if (userInfo.displayName.empty()) {
            userInfo.displayName = getAttr("cn");
        }
        userInfo.email = getAttr("mail");
        userInfo.organization = config_.organization;

        // Get group memberships
        berval** groupVals = ldap_get_values_len(ld, entry, config_.groupAttribute.c_str());
        if (groupVals) {
            for (int i = 0; groupVals[i]; ++i) {
                userInfo.groups.emplace_back(groupVals[i]->bv_val, groupVals[i]->bv_len);
            }
            ldap_value_free_len(groupVals);
        }

        // Map groups to RBAC role (first match wins)
        userInfo.role = config_.defaultRole;
        for (const auto& group : userInfo.groups) {
            const auto it = config_.groupRoleMap.find(group);
            if (it != config_.groupRoleMap.end()) {
                userInfo.role = it->second;
                break;
            }
        }

        return userInfo;
    }

#else

    std::expected<AuthUserInfo, AuthError>
    ldapAuthenticate(const std::string& /*username*/, const std::string& /*password*/)
    {
        // libldap not compiled in — LDAP authentication unavailable
        return std::unexpected(AuthError::ConfigurationError);
    }

#endif // DICOM_VIEWER_HAS_LDAP

    std::expected<AuthTokenPair, AuthError>
    issueTokenPair(const AuthUserInfo& userInfo)
    {
        if (keyMaterial_.privateKeyPem.empty()) {
            return std::unexpected(AuthError::ConfigurationError);
        }

        const auto now = std::chrono::system_clock::now();
        const auto accessExpiry = now + std::chrono::seconds(config_.accessTokenTtlSeconds);

        std::string accessToken;
        try {
            accessToken = jwt::create()
                .set_type("JWT")
                .set_issuer(config_.issuer)
                .set_audience(config_.audience)
                .set_subject(userInfo.userId)
                .set_issued_at(now)
                .set_expires_at(accessExpiry)
                .set_payload_claim("role", jwt::claim(std::string(userInfo.role)))
                .set_payload_claim("organization", jwt::claim(std::string(userInfo.organization)))
                .set_payload_claim("email", jwt::claim(std::string(userInfo.email)))
                .sign(jwt::algorithm::rs256("", keyMaterial_.privateKeyPem, "", ""));
        } catch (...) {
            return std::unexpected(AuthError::InternalError);
        }

        const std::string refreshToken = generateOpaqueToken();
        if (refreshToken.empty()) {
            return std::unexpected(AuthError::InternalError);
        }

        const auto nowEpoch = nowEpochSeconds();
        const uint64_t accessExpiresAt = nowEpoch + config_.accessTokenTtlSeconds;
        const uint64_t refreshExpiresAt = nowEpoch + config_.refreshTokenTtlSeconds;

        {
            std::lock_guard lock(refreshMutex_);
            refreshTokens_[refreshToken] = RefreshTokenInfo{
                userInfo.userId, userInfo.role, userInfo.organization,
                userInfo.email, refreshExpiresAt
            };
        }

        return AuthTokenPair{
            std::move(accessToken), refreshToken,
            accessExpiresAt, refreshExpiresAt
        };
    }

    LdapAuthConfig config_;
    KeyMaterial keyMaterial_;

    mutable std::mutex blacklistMutex_;
    std::unordered_set<std::string> revokedTokens_;

    mutable std::mutex refreshMutex_;
    std::unordered_map<std::string, RefreshTokenInfo> refreshTokens_;

    mutable std::mutex sessionMutex_;
    std::unordered_map<std::string, int> sessionCounts_;

    mutable std::mutex userCacheMutex_;
    std::unordered_map<std::string, AuthUserInfo> userCache_;
};

// ---------------------------------------------------------------------------
// LdapAuthProvider public interface
// ---------------------------------------------------------------------------

LdapAuthProvider::LdapAuthProvider(const LdapAuthConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

LdapAuthProvider::~LdapAuthProvider() = default;

std::expected<AuthResult, AuthError>
LdapAuthProvider::authenticate(const std::string& username, const std::string& password)
{
    return impl_->authenticate(username, password);
}

std::expected<AuthTokenPayload, AuthError>
LdapAuthProvider::validateToken(const std::string& accessToken)
{
    return impl_->validateToken(accessToken);
}

std::expected<AuthTokenPair, AuthError>
LdapAuthProvider::refreshToken(const std::string& refreshToken)
{
    return impl_->refreshToken(refreshToken);
}

std::expected<void, AuthError>
LdapAuthProvider::revokeToken(const std::string& token)
{
    return impl_->revokeToken(token);
}

std::expected<AuthUserInfo, AuthError>
LdapAuthProvider::getUserInfo(const std::string& userId)
{
    return impl_->getUserInfo(userId);
}

int LdapAuthProvider::getActiveSessionCount(const std::string& userId)
{
    return impl_->getActiveSessionCount(userId);
}

} // namespace dicom_viewer::services

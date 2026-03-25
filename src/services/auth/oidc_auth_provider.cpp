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

#include "services/auth/oidc_auth_provider.hpp"

#include <jwt-cpp/jwt.h>

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#ifdef DICOM_VIEWER_HAS_CURL
#include <curl/curl.h>
#endif

namespace dicom_viewer::services {

namespace {

/// Maximum JWKS response size (1 MB) to prevent resource exhaustion
constexpr long kMaxJwksResponseBytes = 1L * 1024 * 1024;

/// Minimum cooldown between kid-miss-triggered JWKS refreshes (seconds)
constexpr int kKidMissRefreshCooldownSeconds = 30;

/// Allowed RSA algorithm families for OIDC token verification
const std::unordered_set<std::string> kAllowedAlgorithms = {"RS256", "RS384", "RS512"};

/// Algorithms that must be explicitly rejected (security: prevent algorithm confusion)
const std::unordered_set<std::string> kDeniedAlgorithms = {
    "none", "HS256", "HS384", "HS512"
};

#ifdef DICOM_VIEWER_HAS_CURL

size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    const auto totalBytes = size * nmemb;
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, totalBytes);
    return totalBytes;
}

std::string defaultJwksFetcher(const std::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {};
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, kMaxJwksResponseBytes);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return {};
    }

    return response;
}

#else

std::string defaultJwksFetcher(const std::string& /*url*/)
{
    // libcurl not compiled in — JWKS fetch unavailable
    return {};
}

#endif // DICOM_VIEWER_HAS_CURL

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

class OidcAuthProvider::Impl {
public:
    explicit Impl(const OidcAuthConfig& config)
        : config_(config), jwksFetcher_(defaultJwksFetcher)
    {
    }

    Impl(const OidcAuthConfig& config, JwksFetcher fetcher)
        : config_(config), jwksFetcher_(std::move(fetcher))
    {
    }

    std::expected<AuthResult, AuthError>
    authenticate(const std::string& /*username*/, const std::string& /*password*/)
    {
        // OIDC authentication requires federation (Cognito Hosted UI).
        // Direct username/password is not an OIDC pattern.
        return std::unexpected(AuthError::ProviderUnavailable);
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

        try {
            auto decoded = jwt::decode(accessToken);

            // Reject tokens without an algorithm header
            if (!decoded.has_algorithm()) {
                return std::unexpected(AuthError::TokenInvalid);
            }

            const auto alg = decoded.get_algorithm();

            // Reject explicitly denied algorithms (none, HMAC)
            if (kDeniedAlgorithms.count(alg)) {
                return std::unexpected(AuthError::TokenInvalid);
            }

            // Only allow RSA algorithm family
            if (!kAllowedAlgorithms.count(alg)) {
                return std::unexpected(AuthError::TokenInvalid);
            }

            // Extract kid from token header for JWKS lookup
            if (!decoded.has_key_id()) {
                return std::unexpected(AuthError::TokenInvalid);
            }
            const auto kid = decoded.get_key_id();

            // Retrieve the public key from JWKS cache
            auto publicKeyPem = getPublicKeyForKid(kid);
            if (publicKeyPem.empty()) {
                return std::unexpected(AuthError::TokenInvalid);
            }

            // Build the verifier with the appropriate RSA algorithm
            auto verifier = jwt::verify();

            if (alg == "RS256") {
                verifier.allow_algorithm(jwt::algorithm::rs256(publicKeyPem, "", "", ""));
            } else if (alg == "RS384") {
                verifier.allow_algorithm(jwt::algorithm::rs384(publicKeyPem, "", "", ""));
            } else if (alg == "RS512") {
                verifier.allow_algorithm(jwt::algorithm::rs512(publicKeyPem, "", "", ""));
            }

            // Verify issuer if configured
            if (!config_.issuer.empty()) {
                verifier.with_issuer(config_.issuer);
            }

            // Verify audience if configured
            if (!config_.audience.empty()) {
                verifier.with_audience(config_.audience);
            }

            std::error_code ec;
            verifier.verify(decoded, ec);

            if (ec) {
                using jwt::error::token_verification_error;
                if (ec == token_verification_error::token_expired) {
                    return std::unexpected(AuthError::TokenExpired);
                }
                return std::unexpected(AuthError::TokenInvalid);
            }

            // Extract claims into payload
            AuthTokenPayload payload;
            payload.userId = decoded.get_subject();
            if (decoded.has_payload_claim(config_.roleClaimName)) {
                payload.role = decoded.get_payload_claim(config_.roleClaimName).as_string();
            }
            if (payload.role.empty()) {
                payload.role = config_.defaultRole;
            }
            if (decoded.has_payload_claim(config_.orgClaimName)) {
                payload.organization =
                    decoded.get_payload_claim(config_.orgClaimName).as_string();
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
    refreshToken(const std::string& /*refreshToken*/)
    {
        // TODO(Phase 2): POST to Cognito OAuth2 /token endpoint with
        //                grant_type=refresh_token and the provided refresh token.
        return std::unexpected(AuthError::ProviderUnavailable);
    }

    std::expected<void, AuthError>
    revokeToken(const std::string& token)
    {
        if (token.empty()) {
            return std::unexpected(AuthError::TokenInvalid);
        }
        std::lock_guard lock(blacklistMutex_);
        revokedTokens_.insert(token);
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
    /// Extract PEM public key from a single JWK entry
    static std::string extractPublicKeyPem(const jwt::jwk<jwt::traits::kazuho_picojson>& jwk)
    {
        // Check key type — only RSA keys
        if (jwk.has_jwk_claim("kty")) {
            const auto kty = jwk.get_jwk_claim("kty").as_string();
            if (kty != "RSA") {
                return {};
            }
        }

        // Try x5c first, then fall back to n/e components
        try {
            if (jwk.has_x5c_key_value()) {
                const auto x5c = jwk.get_x5c_key_value();
                if (!x5c.empty()) {
                    return jwt::helper::convert_base64_der_to_pem(x5c);
                }
            }
        } catch (...) {
            // Fall through to n/e extraction
        }

        try {
            if (jwk.has_jwk_claim("n") && jwk.has_jwk_claim("e")) {
                const auto n = jwk.get_jwk_claim("n").as_string();
                const auto e = jwk.get_jwk_claim("e").as_string();
                return jwt::helper::create_public_key_from_rsa_components(n, e);
            }
        } catch (...) {
            // Malformed key
        }

        return {};
    }

    /// Retrieve the PEM public key for a given kid, refreshing JWKS if needed
    std::string getPublicKeyForKid(const std::string& kid)
    {
        // Try reading from cache first (shared/read lock)
        {
            std::shared_lock readLock(jwksMutex_);
            if (jwksData_ && !isCacheExpired() && jwksData_->has_jwk(kid)) {
                return extractPublicKeyPem(jwksData_->get_jwk(kid));
            }
        }

        // Cache miss or expired — refresh JWKS (exclusive/write lock)
        {
            std::unique_lock writeLock(jwksMutex_);

            // Double-check after acquiring write lock
            if (jwksData_ && !isCacheExpired() && jwksData_->has_jwk(kid)) {
                return extractPublicKeyPem(jwksData_->get_jwk(kid));
            }

            // Rate-limit kid-miss-triggered refreshes to prevent IdP DoS
            const auto now = std::chrono::steady_clock::now();
            const bool isCacheFresh = jwksData_ && !isCacheExpired();
            const auto sinceLastFetch = now - jwksLastFetch_;
            if (isCacheFresh &&
                sinceLastFetch < std::chrono::seconds(kKidMissRefreshCooldownSeconds)) {
                return {};
            }

            refreshJwksCache();

            if (jwksData_ && jwksData_->has_jwk(kid)) {
                return extractPublicKeyPem(jwksData_->get_jwk(kid));
            }
        }

        return {};
    }

    /// Check whether the JWKS cache has expired
    bool isCacheExpired() const
    {
        if (config_.jwksRefreshIntervalSeconds == 0) {
            return true;
        }
        const auto elapsed = std::chrono::steady_clock::now() - jwksLastFetch_;
        return elapsed > std::chrono::seconds(config_.jwksRefreshIntervalSeconds);
    }

    /// Fetch and parse JWKS from the configured endpoint (caller must hold write lock)
    void refreshJwksCache()
    {
        if (config_.jwksUrl.empty() || !jwksFetcher_) {
            return;
        }

        const auto jwksJson = jwksFetcher_(config_.jwksUrl);
        if (jwksJson.empty()) {
            return;
        }

        try {
            jwksData_ = std::make_unique<jwt::jwks<jwt::traits::kazuho_picojson>>(
                jwt::parse_jwks(jwksJson));
            jwksLastFetch_ = std::chrono::steady_clock::now();
        } catch (...) {
            // JWKS parse failure — keep existing cache
        }
    }

    OidcAuthConfig config_;
    JwksFetcher jwksFetcher_;

    // JWKS cache protected by shared_mutex (read-heavy workload)
    mutable std::shared_mutex jwksMutex_;
    std::unique_ptr<jwt::jwks<jwt::traits::kazuho_picojson>> jwksData_;
    std::chrono::steady_clock::time_point jwksLastFetch_{};

    mutable std::mutex blacklistMutex_;
    std::unordered_set<std::string> revokedTokens_;

    mutable std::mutex userCacheMutex_;
    std::unordered_map<std::string, AuthUserInfo> userCache_;

    mutable std::mutex sessionMutex_;
    std::unordered_map<std::string, int> sessionCounts_;
};

// ---------------------------------------------------------------------------
// OidcAuthProvider public interface
// ---------------------------------------------------------------------------

OidcAuthProvider::OidcAuthProvider(const OidcAuthConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

OidcAuthProvider::OidcAuthProvider(const OidcAuthConfig& config, JwksFetcher fetcher)
    : impl_(std::make_unique<Impl>(config, std::move(fetcher)))
{
}

OidcAuthProvider::~OidcAuthProvider() = default;

std::expected<AuthResult, AuthError>
OidcAuthProvider::authenticate(const std::string& username, const std::string& password)
{
    return impl_->authenticate(username, password);
}

std::expected<AuthTokenPayload, AuthError>
OidcAuthProvider::validateToken(const std::string& accessToken)
{
    return impl_->validateToken(accessToken);
}

std::expected<AuthTokenPair, AuthError>
OidcAuthProvider::refreshToken(const std::string& refreshToken)
{
    return impl_->refreshToken(refreshToken);
}

std::expected<void, AuthError>
OidcAuthProvider::revokeToken(const std::string& token)
{
    return impl_->revokeToken(token);
}

std::expected<AuthUserInfo, AuthError>
OidcAuthProvider::getUserInfo(const std::string& userId)
{
    return impl_->getUserInfo(userId);
}

int OidcAuthProvider::getActiveSessionCount(const std::string& userId)
{
    return impl_->getActiveSessionCount(userId);
}

} // namespace dicom_viewer::services

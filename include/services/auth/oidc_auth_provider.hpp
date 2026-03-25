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

/**
 * @file oidc_auth_provider.hpp
 * @brief OIDC/Cognito authentication provider with JWKS signature verification
 * @details Implementation of AuthProvider for OpenID Connect / AWS Cognito.
 *          Fetches JWKS from the identity provider's endpoint and caches
 *          public keys for RS256 JWT signature verification.
 *
 * ## Security
 * - Cryptographic signature verification via JWKS public keys
 * - Algorithm restriction: only RS256/RS384/RS512 allowed
 * - Explicit rejection of `alg: "none"` and HMAC algorithms
 * - Thread-safe JWKS cache with `std::shared_mutex`
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "services/auth/auth_provider.hpp"

#include <functional>
#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Configuration for OIDC/Cognito authentication
 */
struct OidcAuthConfig {
    /// OIDC discovery endpoint (e.g., "https://cognito-idp.region.amazonaws.com/pool/.well-known/openid-configuration")
    std::string discoveryUrl;

    /// Cognito JWKS endpoint URL (e.g., "https://cognito-idp.region.amazonaws.com/pool/.well-known/jwks.json")
    std::string jwksUrl;

    /// OAuth2 client ID registered in Cognito
    std::string clientId;

    /// OAuth2 client secret
    std::string clientSecret;

    /// Cognito User Pool region (e.g., "ap-northeast-2")
    std::string region;

    /// Token issuer claim (`iss`) to validate against
    std::string issuer;

    /// JWT audience claim (`aud`) to validate against
    std::string audience;

    /// Cognito custom claim name for RBAC role (e.g., "custom:role")
    std::string roleClaimName = "custom:role";

    /// Cognito custom claim name for organization (e.g., "custom:organization")
    std::string orgClaimName = "custom:organization";

    /// Default role when role claim is absent
    std::string defaultRole = "Viewer";

    /// JWKS refresh interval in seconds (0 = refresh on each validation)
    uint32_t jwksRefreshIntervalSeconds = 3600;

    /// Maximum allowed concurrent sessions per user (HIPAA: 3)
    int maxConcurrentSessions = 3;
};

/// Callback type for fetching JWKS JSON from a URL.
/// Returns the raw JSON string on success, or empty string on failure.
using JwksFetcher = std::function<std::string(const std::string& url)>;

/**
 * @brief OIDC/Cognito authentication provider with JWKS verification
 *
 * Validates Cognito-issued tokens by verifying the RS256 signature
 * against public keys fetched from the JWKS endpoint. Keys are cached
 * in memory and refreshed periodically or on `kid` cache miss.
 *
 * @trace SRS-FR-AUTH-003
 */
class OidcAuthProvider : public AuthProvider {
public:
    explicit OidcAuthProvider(const OidcAuthConfig& config);

    /// Constructor with custom JWKS fetcher (for testing)
    OidcAuthProvider(const OidcAuthConfig& config, JwksFetcher fetcher);

    ~OidcAuthProvider() override;

    [[nodiscard]] std::expected<AuthResult, AuthError>
    authenticate(const std::string& username, const std::string& password) override;

    [[nodiscard]] std::expected<AuthTokenPayload, AuthError>
    validateToken(const std::string& accessToken) override;

    [[nodiscard]] std::expected<AuthTokenPair, AuthError>
    refreshToken(const std::string& refreshToken) override;

    [[nodiscard]] std::expected<void, AuthError>
    revokeToken(const std::string& token) override;

    [[nodiscard]] std::expected<AuthUserInfo, AuthError>
    getUserInfo(const std::string& userId) override;

    [[nodiscard]] int getActiveSessionCount(const std::string& userId) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

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
 * @brief OIDC/Cognito authentication provider stub (Phase 2)
 * @details Stub implementation of AuthProvider for OpenID Connect / AWS Cognito.
 *          Currently validates structure only; full JWKS fetch and claim extraction
 *          will be completed in Phase 2 when cloud deployment is targeted.
 *
 * ## Phase 2 TODOs
 * - Implement JWKS endpoint fetching with periodic refresh
 * - Validate Cognito-issued ID tokens (signature + standard claims)
 * - Extract custom claims (custom:role, custom:organization)
 * - Implement token refresh via Cognito OAuth2 endpoint
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "services/auth/auth_provider.hpp"

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

/**
 * @brief OIDC/Cognito authentication provider (Phase 2 stub)
 *
 * Validates Cognito-issued tokens using the JWKS endpoint.
 * Full implementation is deferred to Phase 2 (cloud deployment).
 * Currently returns AuthError::ProviderUnavailable for all auth operations.
 *
 * @trace SRS-FR-AUTH-003
 */
class OidcAuthProvider : public AuthProvider {
public:
    explicit OidcAuthProvider(const OidcAuthConfig& config);
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

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
 * @file ldap_auth_provider.hpp
 * @brief LDAP/Active Directory authentication provider
 * @details Implements AuthProvider using LDAPS (port 636, TLS mandatory) for
 *          on-premise AD/LDAP identity providers.
 *
 * ## Authentication Flow
 * 1. Service account binds via LDAPS (bind_dn + bind_password)
 * 2. Search for user entry by sAMAccountName (or configurable filter)
 * 3. Bind with user DN + supplied password to verify credentials
 * 4. Retrieve group memberships from user entry
 * 5. Map AD groups → RBAC roles via configurable JSON mapping
 * 6. Issue RS256 JWT (access token) + opaque refresh token
 *
 * ## Compilation Guard
 * Requires `DICOM_VIEWER_HAS_LDAP` to be defined (set by CMake when libldap
 * is found). If not compiled in, authenticate() returns AuthError::ConfigurationError.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "services/auth/auth_provider.hpp"

#include <map>
#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Configuration for LDAP/AD authentication
 */
struct LdapAuthConfig {
    /// LDAP URL, must use ldaps:// scheme for TLS (e.g., "ldaps://ad.hospital.local:636")
    std::string ldapUrl;

    /// Service account DN used for directory bind (e.g., "CN=svc,OU=Services,DC=hospital,DC=local")
    std::string bindDn;

    /// Service account password (should be loaded from environment or vault, not hardcoded)
    std::string bindPassword;

    /// Base DN for user search (e.g., "DC=hospital,DC=local")
    std::string baseDn;

    /// Base DN for group search (defaults to baseDn if empty)
    std::string groupBaseDn;

    /// LDAP attribute listing group members (default: "member" for AD)
    std::string groupAttribute = "member";

    /// User search filter template; `{username}` is replaced at runtime
    std::string userSearchFilter = "(sAMAccountName={username})";

    /// AD group DN → RBAC role name mapping
    std::map<std::string, std::string> groupRoleMap;

    /// Role assigned to users with no matching group (default: "Viewer")
    std::string defaultRole = "Viewer";

    /// JWT issuer claim
    std::string issuer = "dicom-viewer";

    /// JWT audience claim
    std::string audience = "dicom-viewer-api";

    /// Organization claim embedded in JWT
    std::string organization = "hospital";

    /// RSA private key PEM file for JWT signing (empty = ephemeral key)
    std::string privateKeyPath;

    /// RSA public key PEM file for JWT verification (derived from private key if empty)
    std::string publicKeyPath;

    /// Access token TTL in seconds (HIPAA: 1 hour)
    uint32_t accessTokenTtlSeconds = 3600;

    /// Refresh token TTL in seconds (HIPAA: 8 hours)
    uint32_t refreshTokenTtlSeconds = 28800;

    /// Maximum allowed concurrent sessions per user (HIPAA: 3)
    int maxConcurrentSessions = 3;

    /// LDAP connection timeout in milliseconds
    int connectionTimeoutMs = 5000;

    /// Require and verify the LDAP server's TLS certificate
    bool verifyServerCert = true;

    /// Path to CA certificate file for TLS verification
    std::string caCertPath;
};

/**
 * @brief LDAP/AD authentication provider with RS256 JWT issuance
 *
 * @trace SRS-FR-AUTH-002
 */
class LdapAuthProvider : public AuthProvider {
public:
    /**
     * @brief Construct with LDAP configuration
     * @param config LDAP connection and JWT signing configuration
     */
    explicit LdapAuthProvider(const LdapAuthConfig& config);
    ~LdapAuthProvider() override;

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

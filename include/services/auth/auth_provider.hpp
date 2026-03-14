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
 * @file auth_provider.hpp
 * @brief Abstract authentication provider interface
 * @details Defines the AuthProvider interface that abstracts identity provider
 *          details, enabling seamless switching between AD/LDAP (on-premise)
 *          and OIDC/Cognito (cloud) without application code changes.
 *
 * ## HIPAA Session Requirements
 * - Access Token TTL: 1 hour
 * - Refresh Token TTL: 8 hours
 * - Idle timeout: 15 minutes (enforced at REST API layer)
 * - Max concurrent sessions per user: 3
 *
 * ## Thread Safety
 * All AuthProvider implementations must be fully thread-safe.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Authentication error codes returned by AuthProvider operations
 */
enum class AuthError : uint8_t {
    InvalidCredentials,    ///< Username/password combination incorrect
    AccountLocked,         ///< Account is locked or disabled in the IDP
    SessionLimitExceeded,  ///< Max concurrent sessions reached (HIPAA: max 3)
    TokenExpired,          ///< Access or refresh token has expired
    TokenRevoked,          ///< Token was explicitly revoked before expiry
    TokenInvalid,          ///< Token is malformed or has invalid signature
    ProviderUnavailable,   ///< IDP (LDAP/OIDC endpoint) is not reachable
    ConfigurationError,    ///< Provider is misconfigured or missing required keys
    InternalError          ///< Unexpected internal failure
};

/**
 * @brief Decoded token claims for downstream authorization decisions
 */
struct AuthTokenPayload {
    std::string userId;        ///< Unique user identifier (`sub` claim)
    std::string role;          ///< RBAC role (e.g., "Viewer", "Radiologist", "Admin")
    std::string organization;  ///< Tenant / organization scope
    std::string email;         ///< User email address
    uint64_t issuedAtEpoch = 0;  ///< Issued-at time as Unix epoch seconds
    uint64_t expiryEpoch = 0;    ///< Expiry time as Unix epoch seconds
};

/**
 * @brief Access + refresh token pair returned after successful authentication
 *
 * The access token is a short-lived RS256 JWT. The refresh token is an opaque
 * random string (128 hex chars) stored server-side and must be transmitted
 * via an HttpOnly Secure cookie at the REST API layer.
 */
struct AuthTokenPair {
    std::string accessToken;         ///< RS256 JWT (1 hour TTL)
    std::string refreshToken;        ///< Opaque refresh token (8 hour TTL)
    uint64_t accessExpiresAt = 0;    ///< Access token expiry (Unix epoch seconds)
    uint64_t refreshExpiresAt = 0;   ///< Refresh token expiry (Unix epoch seconds)
};

/**
 * @brief User information retrieved from the identity provider
 */
struct AuthUserInfo {
    std::string userId;                   ///< Unique user identifier
    std::string displayName;              ///< Full display name
    std::string email;                    ///< Email address
    std::string role;                     ///< Mapped RBAC role
    std::string organization;             ///< Organization / tenant
    std::vector<std::string> groups;      ///< IDP groups (before role mapping)
};

/**
 * @brief Result returned after successful authentication
 */
struct AuthResult {
    AuthUserInfo userInfo;   ///< Authenticated user information
    AuthTokenPair tokens;    ///< Token pair for session management
};

/**
 * @brief Abstract authentication provider
 *
 * Abstracts IDP details so that application code remains identical
 * across on-premise (LDAP) and cloud (OIDC) deployments. Only
 * `deployment.yaml` must change to switch providers.
 *
 * ## Contract
 * - Implementations are non-copyable and non-movable
 * - All methods must be thread-safe
 * - `authenticate` enforces HIPAA concurrent session limits
 * - Token blacklisting persists for the lifetime of the provider instance
 *
 * @trace SRS-FR-AUTH-001
 */
class AuthProvider {
public:
    virtual ~AuthProvider() = default;

    // Non-copyable, non-movable (identity semantics)
    AuthProvider(const AuthProvider&) = delete;
    AuthProvider& operator=(const AuthProvider&) = delete;
    AuthProvider(AuthProvider&&) = delete;
    AuthProvider& operator=(AuthProvider&&) = delete;

    /**
     * @brief Authenticate a user with username and password
     *
     * Verifies credentials against the IDP, enforces concurrent session
     * limits, and issues an access + refresh token pair on success.
     *
     * @param username User login name (e.g., sAMAccountName for LDAP)
     * @param password Plaintext password (caller must zero memory after call)
     * @return AuthResult on success, AuthError on failure
     */
    [[nodiscard]] virtual std::expected<AuthResult, AuthError>
    authenticate(const std::string& username, const std::string& password) = 0;

    /**
     * @brief Validate an access token and extract its payload
     *
     * Verifies the RS256 signature, checks expiry, and checks the revocation
     * blacklist. Does not contact the IDP.
     *
     * @param accessToken RS256 JWT access token
     * @return Token payload on success, AuthError on failure
     */
    [[nodiscard]] virtual std::expected<AuthTokenPayload, AuthError>
    validateToken(const std::string& accessToken) = 0;

    /**
     * @brief Issue a new access token from a valid refresh token
     *
     * Validates the refresh token, checks it has not been revoked, and
     * issues a new access token (sliding window). The refresh token itself
     * is NOT rotated.
     *
     * @param refreshToken Opaque refresh token from a prior authenticate call
     * @return New token pair on success, AuthError on failure
     */
    [[nodiscard]] virtual std::expected<AuthTokenPair, AuthError>
    refreshToken(const std::string& refreshToken) = 0;

    /**
     * @brief Revoke a token before its natural expiry
     *
     * Adds the token to the in-process blacklist. Blacklist entries are
     * kept until the token's natural expiry to bound memory usage.
     *
     * @param token Access or refresh token to revoke
     * @return void on success, AuthError on failure
     */
    [[nodiscard]] virtual std::expected<void, AuthError>
    revokeToken(const std::string& token) = 0;

    /**
     * @brief Retrieve user information from the IDP cache
     *
     * Returns cached user info obtained during the most recent
     * authenticate call. Does not re-query the IDP.
     *
     * @param userId User identifier from token payload
     * @return User info on success, AuthError::TokenInvalid if not found
     */
    [[nodiscard]] virtual std::expected<AuthUserInfo, AuthError>
    getUserInfo(const std::string& userId) = 0;

    /**
     * @brief Get the current active session count for a user
     * @param userId User identifier
     * @return Number of active sessions (0 if user has no sessions)
     */
    [[nodiscard]] virtual int getActiveSessionCount(const std::string& userId) = 0;

protected:
    AuthProvider() = default;
};

} // namespace dicom_viewer::services

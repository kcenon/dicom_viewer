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
 * @file session_token_validator.hpp
 * @brief Token-based authentication for render session access
 * @details Generates and validates signed opaque tokens that bind a user
 *          identity to a specific Study Instance UID with an expiry time.
 *
 * ## Token Format (opaque, base64url-encoded)
 * ```
 * base64url(study_uid:user_id:expiry_epoch:signature)
 * ```
 *
 * The signature is computed over the payload using a server-side secret key.
 * Tokens are short-lived (default 1 hour) and non-renewable.
 *
 * ## Thread Safety
 * - All methods are thread-safe (internal mutex)
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Configuration for session token validation
 */
struct SessionTokenConfig {
    /// Server-side secret key for token signing
    std::string secretKey = "dicom-viewer-default-secret";

    /// Token expiry duration in seconds (default: 1 hour)
    uint32_t expirySeconds = 3600;

    /// Allow unauthenticated local connections for development
    bool allowUnauthenticatedLocal = false;
};

/**
 * @brief Result of token validation
 */
enum class TokenValidationResult {
    Valid,              ///< Token is valid and authorized
    Expired,            ///< Token has expired
    InvalidSignature,   ///< Token signature does not match
    InvalidFormat,      ///< Token format is malformed
    StudyMismatch,      ///< Token was issued for a different study
    Empty               ///< No token provided
};

/**
 * @brief Decoded token payload
 */
struct TokenPayload {
    std::string studyUid;    ///< Study Instance UID this token grants access to
    std::string userId;      ///< User identity
    uint64_t expiryEpoch = 0; ///< Expiry time as Unix epoch seconds
};

/**
 * @brief Token-based authentication for render sessions
 *
 * Generates opaque tokens that bind a user to a specific study with
 * an expiry time. Validates tokens on WebSocket connection to ensure
 * only authorized clients can access render sessions.
 *
 * @trace SRS-FR-REMOTE-008
 */
class SessionTokenValidator {
public:
    explicit SessionTokenValidator(const SessionTokenConfig& config = {});
    ~SessionTokenValidator();

    // Non-copyable, movable
    SessionTokenValidator(const SessionTokenValidator&) = delete;
    SessionTokenValidator& operator=(const SessionTokenValidator&) = delete;
    SessionTokenValidator(SessionTokenValidator&&) noexcept;
    SessionTokenValidator& operator=(SessionTokenValidator&&) noexcept;

    /**
     * @brief Generate a new token for a user and study
     * @param userId User identity
     * @param studyUid Study Instance UID to grant access to
     * @return Signed token string
     */
    [[nodiscard]] std::string generateToken(
        const std::string& userId,
        const std::string& studyUid) const;

    /**
     * @brief Validate a token
     * @param token Token string to validate
     * @param requiredStudyUid Study UID that the token must grant access to
     * @return Validation result
     */
    [[nodiscard]] TokenValidationResult validateToken(
        const std::string& token,
        const std::string& requiredStudyUid = {}) const;

    /**
     * @brief Validate a token and extract its payload
     * @param token Token string to validate
     * @param[out] payload Decoded payload (populated on success)
     * @return Validation result
     */
    [[nodiscard]] TokenValidationResult validateToken(
        const std::string& token,
        TokenPayload& payload) const;

    /**
     * @brief Check if unauthenticated local access is allowed
     */
    [[nodiscard]] bool allowsUnauthenticatedLocal() const;

    /**
     * @brief Get the current configuration
     */
    [[nodiscard]] const SessionTokenConfig& config() const;

    /**
     * @brief Update configuration
     */
    void setConfig(const SessionTokenConfig& config);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

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

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

class OidcAuthProvider::Impl {
public:
    explicit Impl(const OidcAuthConfig& config) : config_(config) {}

    // Phase 2 stub: password-based auth is not an OIDC pattern.
    // This stub signals that the OIDC provider requires federation
    // (user logs in via Cognito Hosted UI, not username/password here).
    std::expected<AuthResult, AuthError>
    authenticate(const std::string& /*username*/, const std::string& /*password*/)
    {
        // TODO(Phase 2): Support resource owner password credentials flow
        //                if Cognito pool is configured for it.
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

        // TODO(Phase 2): Fetch JWKS from config_.jwksUrl and cache with
        //                periodic refresh every config_.jwksRefreshIntervalSeconds.
        //                For now, perform structural validation only.
        try {
            auto decoded = jwt::decode(accessToken);

            if (!decoded.has_algorithm()) {
                return std::unexpected(AuthError::TokenInvalid);
            }

            // Verify issuer if configured
            if (!config_.issuer.empty()) {
                if (!decoded.has_issuer() || decoded.get_issuer() != config_.issuer) {
                    return std::unexpected(AuthError::TokenInvalid);
                }
            }

            // Check expiry structurally (no signature validation without JWKS)
            if (decoded.has_expires_at()) {
                const auto expiresAt = decoded.get_expires_at();
                if (expiresAt < std::chrono::system_clock::now()) {
                    return std::unexpected(AuthError::TokenExpired);
                }
            }

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
        // TODO(Phase 2): Query Cognito /oauth2/userInfo endpoint.
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

    OidcAuthConfig config_;

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

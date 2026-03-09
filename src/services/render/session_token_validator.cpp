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

#include "services/render/session_token_validator.hpp"

#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <system_error>

namespace dicom_viewer::services {

namespace {

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

struct KeyMaterial {
    std::string privateKeyPem;
    std::string publicKeyPem;
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

    BioPtr privateBio(BIO_new_mem_buf(
                          privateKeyPem.data(),
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
    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr),
                      EVP_PKEY_CTX_free);
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

    if (PEM_write_bio_PrivateKey(privateBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        return {};
    }
    if (PEM_write_bio_PUBKEY(publicBio.get(), key.get()) != 1) {
        return {};
    }

    return {
        readBioToString(privateBio.get()),
        readBioToString(publicBio.get())
    };
}

uint64_t toEpochSeconds(const std::chrono::system_clock::time_point& timePoint)
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            timePoint.time_since_epoch()).count());
}

template<typename DecodedJwt>
std::string getOptionalStringClaim(const DecodedJwt& decoded,
                                   const std::string& claimName)
{
    if (!decoded.has_payload_claim(claimName)) {
        return {};
    }

    return decoded.get_payload_claim(claimName).as_string();
}

TokenValidationResult mapVerificationError(const std::error_code& error)
{
    using jwt::error::token_verification_error;

    if (!error) {
        return TokenValidationResult::Valid;
    }

    if (error == token_verification_error::token_expired) {
        return TokenValidationResult::Expired;
    }
    if (error == token_verification_error::wrong_algorithm) {
        return TokenValidationResult::UnsupportedAlgorithm;
    }
    if (error == token_verification_error::audience_missmatch
        || error == token_verification_error::claim_value_missmatch
        || error == token_verification_error::claim_type_missmatch
        || error == token_verification_error::missing_claim) {
        return TokenValidationResult::InvalidClaims;
    }

    return TokenValidationResult::InvalidSignature;
}

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class SessionTokenValidator::Impl {
public:
    explicit Impl(const SessionTokenConfig& config) : config_(config)
    {
        refreshKeyMaterial();
    }

    std::string generateToken(const std::string& userId,
                              const std::string& studyUid) const
    {
        std::lock_guard lock(mutex_);

        if (keyMaterial_.privateKeyPem.empty()) {
            return {};
        }

        const auto now = std::chrono::system_clock::now();
        const auto expiry = now + std::chrono::seconds(config_.expirySeconds);

        try {
            return jwt::create()
                .set_type("JWT")
                .set_key_id(config_.keyId)
                .set_issuer(config_.issuer)
                .set_audience(config_.audience)
                .set_subject(userId)
                .set_issued_at(now)
                .set_expires_at(expiry)
                .set_payload_claim("study_uid", jwt::claim(std::string(studyUid)))
                .set_payload_claim("role", jwt::claim(std::string(config_.defaultRole)))
                .set_payload_claim("organization", jwt::claim(std::string(config_.defaultOrganization)))
                .sign(jwt::algorithm::rs256("", keyMaterial_.privateKeyPem, "", ""));
        } catch (...) {
            return {};
        }
    }

    TokenValidationResult validateToken(const std::string& token,
                                        TokenPayload& payload) const
    {
        std::lock_guard lock(mutex_);

        if (token.empty()) {
            return TokenValidationResult::Empty;
        }

        auto decoded = jwt::decode(token);
        if (!decoded.has_algorithm()) {
            return TokenValidationResult::InvalidFormat;
        }
        if (decoded.get_algorithm() != "RS256") {
            return TokenValidationResult::UnsupportedAlgorithm;
        }

        if (keyMaterial_.publicKeyPem.empty()) {
            return TokenValidationResult::InvalidSignature;
        }

        std::error_code error;
        jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(keyMaterial_.publicKeyPem, "", "", ""))
            .with_issuer(config_.issuer)
            .with_audience(config_.audience)
            .verify(decoded, error);

        auto result = mapVerificationError(error);
        if (result != TokenValidationResult::Valid) {
            return result;
        }

        try {
            payload.userId = decoded.get_subject();
            payload.studyUid = getOptionalStringClaim(decoded, "study_uid");
            payload.issuer = decoded.get_issuer();
            if (decoded.has_audience()) {
                const auto audiences = decoded.get_audience();
                if (!audiences.empty()) {
                    payload.audience = *audiences.begin();
                }
            }
            payload.role = getOptionalStringClaim(decoded, "role");
            payload.organization = getOptionalStringClaim(decoded, "organization");
            payload.issuedAtEpoch = decoded.has_issued_at()
                ? toEpochSeconds(decoded.get_issued_at()) : 0;
            payload.expiryEpoch = decoded.has_expires_at()
                ? toEpochSeconds(decoded.get_expires_at()) : 0;
        } catch (...) {
            return TokenValidationResult::InvalidClaims;
        }

        if (payload.userId.empty() || payload.studyUid.empty()) {
            return TokenValidationResult::InvalidClaims;
        }

        return TokenValidationResult::Valid;
    }

    TokenValidationResult validateToken(const std::string& token,
                                        const std::string& requiredStudyUid) const
    {
        TokenPayload payload;
        auto result = validateToken(token, payload);
        if (result != TokenValidationResult::Valid) {
            return result;
        }

        if (!requiredStudyUid.empty() && payload.studyUid != requiredStudyUid) {
            return TokenValidationResult::StudyMismatch;
        }

        return TokenValidationResult::Valid;
    }

    void setConfig(const SessionTokenConfig& config)
    {
        std::lock_guard lock(mutex_);
        config_ = config;
        refreshKeyMaterial();
    }

    void refreshKeyMaterial()
    {
        keyMaterial_.privateKeyPem = readFileIfPresent(config_.privateKeyPath);
        keyMaterial_.publicKeyPem = readFileIfPresent(config_.publicKeyPath);

        if (keyMaterial_.publicKeyPem.empty() && !keyMaterial_.privateKeyPem.empty()) {
            keyMaterial_.publicKeyPem = derivePublicKeyPem(keyMaterial_.privateKeyPem);
        }

        if (keyMaterial_.privateKeyPem.empty()
            && keyMaterial_.publicKeyPem.empty()
            && config_.allowEphemeralKeys) {
            keyMaterial_ = generateEphemeralKeyMaterial();
        }
    }

    mutable std::mutex mutex_;
    SessionTokenConfig config_;
    KeyMaterial keyMaterial_;
};

// ---------------------------------------------------------------------------
// SessionTokenValidator lifecycle
// ---------------------------------------------------------------------------
SessionTokenValidator::SessionTokenValidator(const SessionTokenConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

SessionTokenValidator::~SessionTokenValidator() = default;

SessionTokenValidator::SessionTokenValidator(
    SessionTokenValidator&&) noexcept = default;
SessionTokenValidator& SessionTokenValidator::operator=(
    SessionTokenValidator&&) noexcept = default;

std::string SessionTokenValidator::generateToken(
    const std::string& userId, const std::string& studyUid) const
{
    return impl_->generateToken(userId, studyUid);
}

TokenValidationResult SessionTokenValidator::validateToken(
    const std::string& token,
    const std::string& requiredStudyUid) const
{
    try {
        return impl_->validateToken(token, requiredStudyUid);
    } catch (...) {
        return TokenValidationResult::InvalidFormat;
    }
}

TokenValidationResult SessionTokenValidator::validateToken(
    const std::string& token, TokenPayload& payload) const
{
    try {
        return impl_->validateToken(token, payload);
    } catch (...) {
        return TokenValidationResult::InvalidFormat;
    }
}

bool SessionTokenValidator::allowsUnauthenticatedLocal() const
{
    std::lock_guard lock(impl_->mutex_);
    return impl_->config_.allowUnauthenticatedLocal;
}

const SessionTokenConfig& SessionTokenValidator::config() const
{
    return impl_->config_;
}

void SessionTokenValidator::setConfig(const SessionTokenConfig& config)
{
    impl_->setConfig(config);
}

} // namespace dicom_viewer::services

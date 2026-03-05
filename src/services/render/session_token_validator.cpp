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

#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <sstream>

namespace dicom_viewer::services {

namespace {

// Simple base64url encoding (no padding)
std::string base64urlEncode(const std::string& input)
{
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string result;
    result.reserve((input.size() + 2) / 3 * 4);

    size_t i = 0;
    while (i < input.size()) {
        size_t start = i;
        uint32_t a = static_cast<uint8_t>(input[i++]);
        uint32_t b = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
        uint32_t c = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;

        uint32_t triple = (a << 16) | (b << 8) | c;

        size_t remaining = input.size() - start;
        result.push_back(table[(triple >> 18) & 0x3F]);
        result.push_back(table[(triple >> 12) & 0x3F]);
        if (remaining > 1) {
            result.push_back(table[(triple >> 6) & 0x3F]);
        }
        if (remaining > 2) {
            result.push_back(table[triple & 0x3F]);
        }
    }

    return result;
}

// Simple base64url decoding
std::string base64urlDecode(const std::string& input)
{
    auto decodeChar = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '-') return 62;
        if (c == '_') return 63;
        return -1;
    };

    std::string result;
    result.reserve(input.size() * 3 / 4);

    size_t i = 0;
    while (i < input.size()) {
        int a = (i < input.size()) ? decodeChar(input[i++]) : -1;
        int b = (i < input.size()) ? decodeChar(input[i++]) : -1;
        int c = (i < input.size()) ? decodeChar(input[i++]) : -1;
        int d = (i < input.size()) ? decodeChar(input[i++]) : -1;

        if (a < 0 || b < 0) break;

        uint32_t triple = (static_cast<uint32_t>(a) << 18)
                         | (static_cast<uint32_t>(b) << 12);

        result.push_back(static_cast<char>((triple >> 16) & 0xFF));

        if (c >= 0) {
            triple |= (static_cast<uint32_t>(c) << 6);
            result.push_back(static_cast<char>((triple >> 8) & 0xFF));
        }

        if (d >= 0) {
            triple |= static_cast<uint32_t>(d);
            result.push_back(static_cast<char>(triple & 0xFF));
        }
    }

    return result;
}

// Compute a keyed hash (HMAC-like) using std::hash for portability
// Not cryptographically secure — suitable for same-process token validation
std::string computeSignature(const std::string& payload,
                              const std::string& secret)
{
    std::hash<std::string> hasher;
    size_t h1 = hasher(secret + ":" + payload);
    size_t h2 = hasher(payload + ":" + secret);

    // Combine both hashes for reduced collision probability
    uint64_t combined = (static_cast<uint64_t>(h1) << 32)
                       | (static_cast<uint64_t>(h2) & 0xFFFFFFFF);

    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(combined));
    return std::string(buf);
}

uint64_t currentEpochSeconds()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class SessionTokenValidator::Impl {
public:
    explicit Impl(const SessionTokenConfig& config) : config_(config) {}

    std::string generateToken(const std::string& userId,
                               const std::string& studyUid) const
    {
        std::lock_guard lock(mutex_);

        uint64_t expiry = currentEpochSeconds() + config_.expirySeconds;

        // Payload: study_uid|user_id|expiry
        std::string payload = studyUid + "|" + userId + "|"
                             + std::to_string(expiry);

        std::string signature = computeSignature(payload, config_.secretKey);

        // Token: base64url(payload|signature)
        return base64urlEncode(payload + "|" + signature);
    }

    TokenValidationResult validateToken(
        const std::string& token,
        TokenPayload& payload) const
    {
        std::lock_guard lock(mutex_);

        if (token.empty()) {
            return TokenValidationResult::Empty;
        }

        std::string decoded = base64urlDecode(token);
        if (decoded.empty()) {
            return TokenValidationResult::InvalidFormat;
        }

        // Parse: study_uid|user_id|expiry|signature
        std::vector<std::string> parts;
        std::istringstream stream(decoded);
        std::string part;
        while (std::getline(stream, part, '|')) {
            parts.push_back(part);
        }

        if (parts.size() != 4) {
            return TokenValidationResult::InvalidFormat;
        }

        const auto& studyUid = parts[0];
        const auto& userId = parts[1];
        const auto& expiryStr = parts[2];
        const auto& signature = parts[3];

        // Reconstruct payload and verify signature
        std::string payloadStr = studyUid + "|" + userId + "|" + expiryStr;
        std::string expectedSig = computeSignature(
            payloadStr, config_.secretKey);

        if (signature != expectedSig) {
            return TokenValidationResult::InvalidSignature;
        }

        // Parse expiry
        uint64_t expiry = 0;
        try {
            expiry = std::stoull(expiryStr);
        } catch (...) {
            return TokenValidationResult::InvalidFormat;
        }

        // Check expiry
        if (currentEpochSeconds() > expiry) {
            return TokenValidationResult::Expired;
        }

        payload.studyUid = studyUid;
        payload.userId = userId;
        payload.expiryEpoch = expiry;

        return TokenValidationResult::Valid;
    }

    TokenValidationResult validateToken(
        const std::string& token,
        const std::string& requiredStudyUid) const
    {
        TokenPayload payload;
        auto result = validateToken(token, payload);

        if (result != TokenValidationResult::Valid) {
            return result;
        }

        if (!requiredStudyUid.empty()
            && payload.studyUid != requiredStudyUid) {
            return TokenValidationResult::StudyMismatch;
        }

        return TokenValidationResult::Valid;
    }

    mutable std::mutex mutex_;
    SessionTokenConfig config_;
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
    return impl_->validateToken(token, requiredStudyUid);
}

TokenValidationResult SessionTokenValidator::validateToken(
    const std::string& token, TokenPayload& payload) const
{
    return impl_->validateToken(token, payload);
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
    std::lock_guard lock(impl_->mutex_);
    impl_->config_ = config;
}

} // namespace dicom_viewer::services

// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
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
 * @file session_store.hpp
 * @brief Abstract session store interface and in-memory implementation
 * @details Defines the contract for session metadata persistence,
 *          token blacklisting, and per-user concurrent session counting.
 *          The in-memory implementation wraps the current behavior;
 *          RedisSessionStore provides durable persistence.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Metadata describing a render session (persistable subset)
 */
struct SessionMetadata {
    std::string sessionId;
    std::string userId;
    std::string studyUid;
    uint32_t width = 0;
    uint32_t height = 0;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastActive;
};

/**
 * @brief Abstract interface for session metadata persistence
 *
 * Implementations must be thread-safe. Methods are called from
 * RenderSessionManager under its own mutex, but external callers
 * (e.g., token validation middleware) may also invoke blacklist checks.
 */
class ISessionStore {
public:
    virtual ~ISessionStore() = default;

    // -- Session metadata --

    virtual bool saveSession(const SessionMetadata& meta) = 0;
    [[nodiscard]] virtual std::optional<SessionMetadata> loadSession(
        const std::string& sessionId) = 0;
    virtual bool removeSession(const std::string& sessionId) = 0;
    [[nodiscard]] virtual bool sessionExists(const std::string& sessionId) = 0;
    virtual void touchSession(const std::string& sessionId) = 0;
    [[nodiscard]] virtual std::vector<std::string> listSessions() = 0;

    // -- Token blacklist --

    virtual bool blacklistToken(const std::string& token,
                                std::chrono::seconds ttl) = 0;
    [[nodiscard]] virtual bool isTokenBlacklisted(const std::string& token) = 0;

    // -- Per-user concurrent session counter --

    virtual int64_t incrementUserSessions(const std::string& userId) = 0;
    virtual int64_t decrementUserSessions(const std::string& userId) = 0;
    [[nodiscard]] virtual int64_t getUserSessionCount(
        const std::string& userId) = 0;
};

/**
 * @brief In-memory session store (default fallback)
 *
 * Thread-safe via internal mutex. Suitable for single-process deployments
 * where session persistence across restarts is not required.
 */
class InMemorySessionStore : public ISessionStore {
public:
    bool saveSession(const SessionMetadata& meta) override;
    [[nodiscard]] std::optional<SessionMetadata> loadSession(
        const std::string& sessionId) override;
    bool removeSession(const std::string& sessionId) override;
    [[nodiscard]] bool sessionExists(const std::string& sessionId) override;
    void touchSession(const std::string& sessionId) override;
    [[nodiscard]] std::vector<std::string> listSessions() override;

    bool blacklistToken(const std::string& token,
                        std::chrono::seconds ttl) override;
    [[nodiscard]] bool isTokenBlacklisted(const std::string& token) override;

    int64_t incrementUserSessions(const std::string& userId) override;
    int64_t decrementUserSessions(const std::string& userId) override;
    [[nodiscard]] int64_t getUserSessionCount(
        const std::string& userId) override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SessionMetadata> sessions_;
    std::unordered_set<std::string> blacklistedTokens_;
    std::unordered_map<std::string, int64_t> userSessionCounts_;
};

} // namespace dicom_viewer::services

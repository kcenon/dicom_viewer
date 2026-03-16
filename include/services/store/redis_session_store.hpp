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
 * @file redis_session_store.hpp
 * @brief Redis-backed session store using hiredis
 * @details Persists session metadata, token blacklist, and per-user
 *          concurrent session counters in Redis. Supports AUTH + TLS.
 *          Falls back to returning failure on connection errors
 *          (caller should use InMemorySessionStore as fallback).
 *
 * ## Redis Key Layout
 * ```
 * dv:session:{sessionId}       HASH  (metadata fields)
 * dv:sessions                  SET   (all active session IDs)
 * dv:blacklist:{tokenHash}     STRING with TTL
 * dv:user_sessions:{userId}    STRING (atomic counter)
 * ```
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "services/store/session_store.hpp"

#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Configuration for Redis connection
 */
struct RedisConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    std::string password;
    uint32_t database = 0;
    uint32_t connectTimeoutMs = 3000;
    uint32_t commandTimeoutMs = 1000;
    uint32_t sessionTtlSeconds = 86400;  // 24 hours
    bool useTls = false;
    std::string caCertPath;
};

/**
 * @brief Redis-backed implementation of ISessionStore
 *
 * Thread-safe via hiredis connection pool (Pimpl pattern).
 * On connection failure, methods return failure values rather
 * than throwing, allowing the caller to fall back gracefully.
 */
class RedisSessionStore : public ISessionStore {
public:
    explicit RedisSessionStore(const RedisConfig& config = {});
    ~RedisSessionStore() override;

    RedisSessionStore(const RedisSessionStore&) = delete;
    RedisSessionStore& operator=(const RedisSessionStore&) = delete;
    RedisSessionStore(RedisSessionStore&&) noexcept;
    RedisSessionStore& operator=(RedisSessionStore&&) noexcept;

    /**
     * @brief Test the Redis connection
     * @return true if connected and authenticated
     */
    [[nodiscard]] bool isConnected() const;

    // -- ISessionStore interface --

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
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

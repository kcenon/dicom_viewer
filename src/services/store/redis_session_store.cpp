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

#include "services/store/redis_session_store.hpp"

#include <spdlog/spdlog.h>

#include <hiredis/hiredis.h>
#ifdef DICOM_VIEWER_REDIS_TLS
#include <hiredis/hiredis_ssl.h>
#endif

#include <cstring>
#include <functional>
#include <mutex>

namespace dicom_viewer::services {

// Key prefix to avoid collisions with other applications
static constexpr const char* kPrefix = "dv:";

// ---------------------------------------------------------------------------
// RAII wrapper for redisReply
// ---------------------------------------------------------------------------
struct ReplyDeleter {
    void operator()(redisReply* r) const {
        if (r) freeReplyObject(r);
    }
};
using UniqueReply = std::unique_ptr<redisReply, ReplyDeleter>;

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class RedisSessionStore::Impl {
public:
    explicit Impl(const RedisConfig& config) : config_(config)
    {
        connect_unlocked();
    }

    ~Impl()
    {
        disconnect();
    }

    bool isConnected() const
    {
        std::lock_guard lock(mutex_);
        return ctx_ != nullptr && ctx_->err == 0;
    }

    // -- Session metadata --

    bool saveSession(const SessionMetadata& meta)
    {
        auto createdEpoch = std::chrono::duration_cast<std::chrono::seconds>(
            meta.createdAt.time_since_epoch()).count();
        auto activeEpoch = std::chrono::duration_cast<std::chrono::seconds>(
            meta.lastActive.time_since_epoch()).count();

        std::string key = sessionKey(meta.sessionId);

        auto reply = command(
            "HSET %s userId %s studyUid %s width %u height %u "
            "createdAt %lld lastActive %lld",
            key.c_str(),
            meta.userId.c_str(),
            meta.studyUid.c_str(),
            meta.width,
            meta.height,
            static_cast<long long>(createdEpoch),
            static_cast<long long>(activeEpoch));

        if (!reply) return false;

        // Set TTL on the session key
        command("EXPIRE %s %u", key.c_str(), config_.sessionTtlSeconds);

        // Add to sessions set
        command("SADD %ssessions %s", kPrefix, meta.sessionId.c_str());

        return true;
    }

    std::optional<SessionMetadata> loadSession(const std::string& sessionId)
    {
        auto reply = command("HGETALL %s", sessionKey(sessionId).c_str());
        if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
            return std::nullopt;
        }

        SessionMetadata meta;
        meta.sessionId = sessionId;

        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            const char* field = reply->element[i]->str;
            const char* value = reply->element[i + 1]->str;
            if (!field || !value) continue;

            if (std::strcmp(field, "userId") == 0) {
                meta.userId = value;
            } else if (std::strcmp(field, "studyUid") == 0) {
                meta.studyUid = value;
            } else if (std::strcmp(field, "width") == 0) {
                meta.width = static_cast<uint32_t>(std::stoul(value));
            } else if (std::strcmp(field, "height") == 0) {
                meta.height = static_cast<uint32_t>(std::stoul(value));
            } else if (std::strcmp(field, "createdAt") == 0) {
                meta.createdAt = std::chrono::system_clock::time_point(
                    std::chrono::seconds(std::stoll(value)));
            } else if (std::strcmp(field, "lastActive") == 0) {
                meta.lastActive = std::chrono::system_clock::time_point(
                    std::chrono::seconds(std::stoll(value)));
            }
        }

        return meta;
    }

    bool removeSession(const std::string& sessionId)
    {
        auto reply = command("DEL %s", sessionKey(sessionId).c_str());
        command("SREM %ssessions %s", kPrefix, sessionId.c_str());
        return reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    }

    bool sessionExists(const std::string& sessionId)
    {
        auto reply = command("EXISTS %s", sessionKey(sessionId).c_str());
        return reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    }

    void touchSession(const std::string& sessionId)
    {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        command("HSET %s lastActive %lld",
                sessionKey(sessionId).c_str(),
                static_cast<long long>(now));
        command("EXPIRE %s %u",
                sessionKey(sessionId).c_str(),
                config_.sessionTtlSeconds);
    }

    std::vector<std::string> listSessions()
    {
        auto reply = command("SMEMBERS %ssessions", kPrefix);
        std::vector<std::string> ids;
        if (!reply || reply->type != REDIS_REPLY_ARRAY) return ids;

        ids.reserve(reply->elements);
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->str) {
                ids.emplace_back(reply->element[i]->str);
            }
        }
        return ids;
    }

    // -- Token blacklist --

    bool blacklistToken(const std::string& token, std::chrono::seconds ttl)
    {
        auto reply = command("SET %sblacklist:%s 1 EX %lld",
                            kPrefix, token.c_str(),
                            static_cast<long long>(ttl.count()));
        return reply && reply->type == REDIS_REPLY_STATUS;
    }

    bool isTokenBlacklisted(const std::string& token)
    {
        auto reply = command("EXISTS %sblacklist:%s",
                            kPrefix, token.c_str());
        return reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    }

    // -- Per-user concurrent session counter --

    int64_t incrementUserSessions(const std::string& userId)
    {
        auto reply = command("INCR %suser_sessions:%s",
                            kPrefix, userId.c_str());
        return (reply && reply->type == REDIS_REPLY_INTEGER)
               ? reply->integer : -1;
    }

    int64_t decrementUserSessions(const std::string& userId)
    {
        // Use Lua script to prevent going below zero
        auto reply = command(
            "EVAL \"local v = redis.call('DECR', KEYS[1]); "
            "if v < 0 then redis.call('SET', KEYS[1], 0); return 0; end; "
            "return v\" 1 %suser_sessions:%s",
            kPrefix, userId.c_str());
        return (reply && reply->type == REDIS_REPLY_INTEGER)
               ? reply->integer : -1;
    }

    int64_t getUserSessionCount(const std::string& userId)
    {
        auto reply = command("GET %suser_sessions:%s",
                            kPrefix, userId.c_str());
        if (!reply) return -1;
        if (reply->type == REDIS_REPLY_NIL) return 0;
        if (reply->type == REDIS_REPLY_STRING && reply->str) {
            return std::stoll(reply->str);
        }
        return -1;
    }

private:
    static std::string sessionKey(const std::string& sessionId)
    {
        return std::string(kPrefix) + "session:" + sessionId;
    }

    void connect()
    {
        std::lock_guard lock(mutex_);
        connect_unlocked();
    }

    void connect_unlocked()
    {
        disconnect_unlocked();

        struct timeval tv;
        tv.tv_sec = config_.connectTimeoutMs / 1000;
        tv.tv_usec = (config_.connectTimeoutMs % 1000) * 1000;

        ctx_ = redisConnectWithTimeout(
            config_.host.c_str(), config_.port, tv);

        if (!ctx_) {
            spdlog::error("Redis: allocation failure");
            return;
        }
        if (ctx_->err) {
            spdlog::error("Redis: connection failed: {}", ctx_->errstr);
            disconnect_unlocked();
            return;
        }

#ifdef DICOM_VIEWER_REDIS_TLS
        if (config_.useTls) {
            redisInitOpenSSL();
            auto* sslCtx = redisCreateSSLContext(
                config_.caCertPath.empty() ? nullptr : config_.caCertPath.c_str(),
                nullptr, nullptr, nullptr, nullptr, nullptr);
            if (!sslCtx) {
                spdlog::error("Redis: TLS context creation failed");
                disconnect_unlocked();
                return;
            }
            if (redisInitiateSSLWithContext(ctx_, sslCtx) != REDIS_OK) {
                spdlog::error("Redis: TLS handshake failed: {}", ctx_->errstr);
                redisFreeSSLContext(sslCtx);
                disconnect_unlocked();
                return;
            }
        }
#endif

        // Authenticate if password is set
        if (!config_.password.empty()) {
            auto reply = UniqueReply(static_cast<redisReply*>(
                redisCommand(ctx_, "AUTH %s", config_.password.c_str())));
            if (!reply || reply->type == REDIS_REPLY_ERROR) {
                spdlog::error("Redis: AUTH failed: {}",
                             reply ? reply->str : "null reply");
                disconnect_unlocked();
                return;
            }
        }

        // Select database
        if (config_.database != 0) {
            auto reply = UniqueReply(static_cast<redisReply*>(
                redisCommand(ctx_, "SELECT %u", config_.database)));
            if (!reply || reply->type == REDIS_REPLY_ERROR) {
                spdlog::error("Redis: SELECT {} failed", config_.database);
                disconnect_unlocked();
                return;
            }
        }

        // Set command timeout
        struct timeval cmdTv;
        cmdTv.tv_sec = config_.commandTimeoutMs / 1000;
        cmdTv.tv_usec = (config_.commandTimeoutMs % 1000) * 1000;
        redisSetTimeout(ctx_, cmdTv);

        spdlog::info("Redis: connected to {}:{} db={}",
                     config_.host, config_.port, config_.database);
    }

    void disconnect()
    {
        std::lock_guard lock(mutex_);
        disconnect_unlocked();
    }

    void disconnect_unlocked()
    {
        if (ctx_) {
            redisFree(ctx_);
            ctx_ = nullptr;
        }
    }

    UniqueReply command(const char* fmt, ...)
    {
        std::lock_guard lock(mutex_);
        if (!ctx_ || ctx_->err) {
            connect_unlocked();
            if (!ctx_ || ctx_->err) {
                return nullptr;
            }
        }

        va_list ap;
        va_start(ap, fmt);
        auto* raw = static_cast<redisReply*>(redisvCommand(ctx_, fmt, ap));
        va_end(ap);

        if (!raw) {
            spdlog::warn("Redis: command failed (null reply), will reconnect");
            disconnect_unlocked();
            return nullptr;
        }

        if (raw->type == REDIS_REPLY_ERROR) {
            spdlog::warn("Redis: error: {}", raw->str ? raw->str : "unknown");
        }

        return UniqueReply(raw);
    }

    RedisConfig config_;
    mutable std::mutex mutex_;
    redisContext* ctx_ = nullptr;
};

// ---------------------------------------------------------------------------
// RedisSessionStore lifecycle (delegates to Impl)
// ---------------------------------------------------------------------------
RedisSessionStore::RedisSessionStore(const RedisConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

RedisSessionStore::~RedisSessionStore() = default;
RedisSessionStore::RedisSessionStore(RedisSessionStore&&) noexcept = default;
RedisSessionStore& RedisSessionStore::operator=(RedisSessionStore&&) noexcept = default;

bool RedisSessionStore::isConnected() const { return impl_->isConnected(); }

bool RedisSessionStore::saveSession(const SessionMetadata& meta)
{
    return impl_->saveSession(meta);
}

std::optional<SessionMetadata> RedisSessionStore::loadSession(
    const std::string& sessionId)
{
    return impl_->loadSession(sessionId);
}

bool RedisSessionStore::removeSession(const std::string& sessionId)
{
    return impl_->removeSession(sessionId);
}

bool RedisSessionStore::sessionExists(const std::string& sessionId)
{
    return impl_->sessionExists(sessionId);
}

void RedisSessionStore::touchSession(const std::string& sessionId)
{
    impl_->touchSession(sessionId);
}

std::vector<std::string> RedisSessionStore::listSessions()
{
    return impl_->listSessions();
}

bool RedisSessionStore::blacklistToken(const std::string& token,
                                       std::chrono::seconds ttl)
{
    return impl_->blacklistToken(token, ttl);
}

bool RedisSessionStore::isTokenBlacklisted(const std::string& token)
{
    return impl_->isTokenBlacklisted(token);
}

int64_t RedisSessionStore::incrementUserSessions(const std::string& userId)
{
    return impl_->incrementUserSessions(userId);
}

int64_t RedisSessionStore::decrementUserSessions(const std::string& userId)
{
    return impl_->decrementUserSessions(userId);
}

int64_t RedisSessionStore::getUserSessionCount(const std::string& userId)
{
    return impl_->getUserSessionCount(userId);
}

} // namespace dicom_viewer::services

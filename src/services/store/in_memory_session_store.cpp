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

#include "services/store/session_store.hpp"

namespace dicom_viewer::services {

bool InMemorySessionStore::saveSession(const SessionMetadata& meta)
{
    std::lock_guard lock(mutex_);
    sessions_[meta.sessionId] = meta;
    return true;
}

std::optional<SessionMetadata> InMemorySessionStore::loadSession(
    const std::string& sessionId)
{
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InMemorySessionStore::removeSession(const std::string& sessionId)
{
    std::lock_guard lock(mutex_);
    return sessions_.erase(sessionId) > 0;
}

bool InMemorySessionStore::sessionExists(const std::string& sessionId)
{
    std::lock_guard lock(mutex_);
    return sessions_.count(sessionId) > 0;
}

void InMemorySessionStore::touchSession(const std::string& sessionId)
{
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.lastActive = std::chrono::system_clock::now();
    }
}

std::vector<std::string> InMemorySessionStore::listSessions()
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(sessions_.size());
    for (const auto& [id, _] : sessions_) {
        ids.push_back(id);
    }
    return ids;
}

bool InMemorySessionStore::blacklistToken(const std::string& token,
                                          std::chrono::seconds /*ttl*/)
{
    std::lock_guard lock(mutex_);
    blacklistedTokens_.insert(token);
    return true;
}

bool InMemorySessionStore::isTokenBlacklisted(const std::string& token)
{
    std::lock_guard lock(mutex_);
    return blacklistedTokens_.count(token) > 0;
}

int64_t InMemorySessionStore::incrementUserSessions(const std::string& userId)
{
    std::lock_guard lock(mutex_);
    return ++userSessionCounts_[userId];
}

int64_t InMemorySessionStore::decrementUserSessions(const std::string& userId)
{
    std::lock_guard lock(mutex_);
    auto it = userSessionCounts_.find(userId);
    if (it == userSessionCounts_.end() || it->second <= 0) {
        return 0;
    }
    --it->second;
    if (it->second == 0) {
        userSessionCounts_.erase(it);
        return 0;
    }
    return it->second;
}

int64_t InMemorySessionStore::getUserSessionCount(const std::string& userId)
{
    std::lock_guard lock(mutex_);
    auto it = userSessionCounts_.find(userId);
    return (it != userSessionCounts_.end()) ? it->second : 0;
}

} // namespace dicom_viewer::services

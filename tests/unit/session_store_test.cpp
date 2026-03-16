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

#include <gtest/gtest.h>

#include <services/store/session_store.hpp>

#include <chrono>
#include <string>
#include <thread>

namespace {

using namespace dicom_viewer::services;

class InMemorySessionStoreTest : public ::testing::Test {
protected:
    InMemorySessionStore store;

    static SessionMetadata makeSession(const std::string& id,
                                       const std::string& userId = "user1") {
        SessionMetadata meta;
        meta.sessionId = id;
        meta.userId = userId;
        meta.studyUid = "1.2.3.4.5";
        meta.width = 512;
        meta.height = 512;
        meta.createdAt = std::chrono::system_clock::now();
        meta.lastActive = meta.createdAt;
        return meta;
    }
};

// --- Session Metadata ---

TEST_F(InMemorySessionStoreTest, SaveAndLoad) {
    auto meta = makeSession("sess-001");
    EXPECT_TRUE(store.saveSession(meta));

    auto loaded = store.loadSession("sess-001");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->sessionId, "sess-001");
    EXPECT_EQ(loaded->userId, "user1");
    EXPECT_EQ(loaded->width, 512u);
    EXPECT_EQ(loaded->height, 512u);
}

TEST_F(InMemorySessionStoreTest, LoadNonExistent) {
    auto loaded = store.loadSession("no-such-session");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(InMemorySessionStoreTest, SessionExists) {
    EXPECT_FALSE(store.sessionExists("sess-001"));
    store.saveSession(makeSession("sess-001"));
    EXPECT_TRUE(store.sessionExists("sess-001"));
}

TEST_F(InMemorySessionStoreTest, RemoveSession) {
    store.saveSession(makeSession("sess-001"));
    EXPECT_TRUE(store.removeSession("sess-001"));
    EXPECT_FALSE(store.sessionExists("sess-001"));
}

TEST_F(InMemorySessionStoreTest, RemoveNonExistent) {
    EXPECT_FALSE(store.removeSession("no-such-session"));
}

TEST_F(InMemorySessionStoreTest, ListSessions) {
    store.saveSession(makeSession("sess-001"));
    store.saveSession(makeSession("sess-002"));
    store.saveSession(makeSession("sess-003"));

    auto ids = store.listSessions();
    EXPECT_EQ(ids.size(), 3u);
}

TEST_F(InMemorySessionStoreTest, TouchSession) {
    auto meta = makeSession("sess-001");
    store.saveSession(meta);

    // Small delay so system_clock advances
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    store.touchSession("sess-001");

    auto loaded = store.loadSession("sess-001");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_GT(loaded->lastActive, meta.lastActive);
}

TEST_F(InMemorySessionStoreTest, OverwriteSession) {
    store.saveSession(makeSession("sess-001", "user1"));
    store.saveSession(makeSession("sess-001", "user2"));

    auto loaded = store.loadSession("sess-001");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->userId, "user2");
}

// --- Token Blacklist ---

TEST_F(InMemorySessionStoreTest, BlacklistToken) {
    EXPECT_FALSE(store.isTokenBlacklisted("token-abc"));
    store.blacklistToken("token-abc", std::chrono::seconds(3600));
    EXPECT_TRUE(store.isTokenBlacklisted("token-abc"));
}

TEST_F(InMemorySessionStoreTest, NonBlacklistedToken) {
    store.blacklistToken("token-abc", std::chrono::seconds(3600));
    EXPECT_FALSE(store.isTokenBlacklisted("token-xyz"));
}

// --- User Session Counter ---

TEST_F(InMemorySessionStoreTest, IncrementUserSessions) {
    EXPECT_EQ(store.getUserSessionCount("user1"), 0);
    EXPECT_EQ(store.incrementUserSessions("user1"), 1);
    EXPECT_EQ(store.incrementUserSessions("user1"), 2);
    EXPECT_EQ(store.getUserSessionCount("user1"), 2);
}

TEST_F(InMemorySessionStoreTest, DecrementUserSessions) {
    store.incrementUserSessions("user1");
    store.incrementUserSessions("user1");
    EXPECT_EQ(store.decrementUserSessions("user1"), 1);
    EXPECT_EQ(store.decrementUserSessions("user1"), 0);
}

TEST_F(InMemorySessionStoreTest, DecrementBelowZero) {
    // Should not go below zero
    EXPECT_EQ(store.decrementUserSessions("user1"), 0);
    EXPECT_EQ(store.getUserSessionCount("user1"), 0);
}

TEST_F(InMemorySessionStoreTest, IndependentUserCounters) {
    store.incrementUserSessions("user1");
    store.incrementUserSessions("user1");
    store.incrementUserSessions("user2");

    EXPECT_EQ(store.getUserSessionCount("user1"), 2);
    EXPECT_EQ(store.getUserSessionCount("user2"), 1);
}

} // namespace

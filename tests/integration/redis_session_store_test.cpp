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
 * @file redis_session_store_test.cpp
 * @brief Integration tests for RedisSessionStore with real Redis 7 instance
 * @details Requires Docker: docker compose -f tests/integration/docker-compose.yml up -d
 *          Tests are skipped automatically when Redis is not available.
 */

#ifdef DICOM_VIEWER_HAS_HIREDIS

#include <gtest/gtest.h>

#include <services/store/redis_session_store.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>

namespace {

using namespace dicom_viewer::services;

// Read test config from environment (set by .env.test / CI)
static std::string getEnv(const char* name, const char* fallback) {
    const char* val = std::getenv(name);
    return val ? val : fallback;
}

class RedisSessionStoreIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        RedisConfig config;
        config.host = "127.0.0.1";
        config.port = static_cast<uint16_t>(
            std::stoi(getEnv("TEST_REDIS_PORT", "6399")));
        config.password = getEnv("TEST_REDIS_PASSWORD", "");
        config.connectTimeoutMs = 2000;
        config.sessionTtlSeconds = 60;

        store = std::make_unique<RedisSessionStore>(config);

        if (!store->isConnected()) {
            GTEST_SKIP() << "Redis not available at 127.0.0.1:" << config.port
                         << " — run: docker compose -f tests/integration/docker-compose.yml up -d";
        }
    }

    void TearDown() override {
        // Clean up test sessions
        if (store && store->isConnected()) {
            for (const auto& id : store->listSessions()) {
                store->removeSession(id);
            }
        }
    }

    static SessionMetadata makeSession(const std::string& id,
                                       const std::string& userId = "test-user") {
        SessionMetadata meta;
        meta.sessionId = id;
        meta.userId = userId;
        meta.studyUid = "1.2.840.10008.5.1.4.1.1.2";
        meta.width = 512;
        meta.height = 512;
        meta.createdAt = std::chrono::system_clock::now();
        meta.lastActive = meta.createdAt;
        return meta;
    }

    std::unique_ptr<RedisSessionStore> store;
};

// --- Connection ---

TEST_F(RedisSessionStoreIntegrationTest, ConnectedToRedis) {
    EXPECT_TRUE(store->isConnected());
}

// --- Session CRUD ---

TEST_F(RedisSessionStoreIntegrationTest, SaveAndLoadSession) {
    auto meta = makeSession("it-sess-001");
    EXPECT_TRUE(store->saveSession(meta));

    auto loaded = store->loadSession("it-sess-001");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->sessionId, "it-sess-001");
    EXPECT_EQ(loaded->userId, "test-user");
    EXPECT_EQ(loaded->studyUid, "1.2.840.10008.5.1.4.1.1.2");
    EXPECT_EQ(loaded->width, 512u);
    EXPECT_EQ(loaded->height, 512u);
}

TEST_F(RedisSessionStoreIntegrationTest, LoadNonExistentReturnsNullopt) {
    auto loaded = store->loadSession("it-nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(RedisSessionStoreIntegrationTest, SessionExistsAfterSave) {
    EXPECT_FALSE(store->sessionExists("it-sess-002"));
    store->saveSession(makeSession("it-sess-002"));
    EXPECT_TRUE(store->sessionExists("it-sess-002"));
}

TEST_F(RedisSessionStoreIntegrationTest, RemoveSession) {
    store->saveSession(makeSession("it-sess-003"));
    EXPECT_TRUE(store->removeSession("it-sess-003"));
    EXPECT_FALSE(store->sessionExists("it-sess-003"));
}

TEST_F(RedisSessionStoreIntegrationTest, ListSessions) {
    store->saveSession(makeSession("it-list-001"));
    store->saveSession(makeSession("it-list-002"));

    auto ids = store->listSessions();
    EXPECT_GE(ids.size(), 2u);

    bool found1 = false, found2 = false;
    for (const auto& id : ids) {
        if (id == "it-list-001") found1 = true;
        if (id == "it-list-002") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(RedisSessionStoreIntegrationTest, TouchSessionUpdatesLastActive) {
    store->saveSession(makeSession("it-sess-touch"));
    auto before = store->loadSession("it-sess-touch");
    ASSERT_TRUE(before.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    store->touchSession("it-sess-touch");

    auto after = store->loadSession("it-sess-touch");
    ASSERT_TRUE(after.has_value());
    EXPECT_GE(after->lastActive, before->lastActive);
}

// --- Token Blacklist ---

TEST_F(RedisSessionStoreIntegrationTest, BlacklistToken) {
    EXPECT_FALSE(store->isTokenBlacklisted("it-token-abc"));
    EXPECT_TRUE(store->blacklistToken("it-token-abc", std::chrono::seconds(60)));
    EXPECT_TRUE(store->isTokenBlacklisted("it-token-abc"));
}

TEST_F(RedisSessionStoreIntegrationTest, NonBlacklistedToken) {
    store->blacklistToken("it-token-xyz", std::chrono::seconds(60));
    EXPECT_FALSE(store->isTokenBlacklisted("it-token-other"));
}

// --- User Session Counter ---

TEST_F(RedisSessionStoreIntegrationTest, IncrementUserSessions) {
    // Use unique user ID to avoid collisions between tests
    EXPECT_EQ(store->incrementUserSessions("it-user-incr"), 1);
    EXPECT_EQ(store->incrementUserSessions("it-user-incr"), 2);
    EXPECT_EQ(store->getUserSessionCount("it-user-incr"), 2);
}

TEST_F(RedisSessionStoreIntegrationTest, DecrementUserSessions) {
    store->incrementUserSessions("it-user-decr");
    store->incrementUserSessions("it-user-decr");
    EXPECT_EQ(store->decrementUserSessions("it-user-decr"), 1);
    EXPECT_EQ(store->decrementUserSessions("it-user-decr"), 0);
}

TEST_F(RedisSessionStoreIntegrationTest, DecrementBelowZero) {
    EXPECT_EQ(store->decrementUserSessions("it-user-zero"), 0);
}

// --- Persistence across reconnect ---

TEST_F(RedisSessionStoreIntegrationTest, DataPersistsAcrossNewStoreInstance) {
    store->saveSession(makeSession("it-persist-001", "persist-user"));

    // Create a new store instance (simulating server restart)
    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = static_cast<uint16_t>(
        std::stoi(getEnv("TEST_REDIS_PORT", "6399")));
    config.password = getEnv("TEST_REDIS_PASSWORD", "");
    config.connectTimeoutMs = 2000;

    auto store2 = std::make_unique<RedisSessionStore>(config);
    ASSERT_TRUE(store2->isConnected());

    auto loaded = store2->loadSession("it-persist-001");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->userId, "persist-user");
}

} // namespace

#else

#include <gtest/gtest.h>

TEST(RedisSessionStoreIntegrationTest, SkippedNoHiredis) {
    GTEST_SKIP() << "hiredis not available — RedisSessionStore integration tests disabled";
}

#endif

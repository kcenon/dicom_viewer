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

#include <gtest/gtest.h>

#include <services/query_cache_manager.hpp>

#include <pacs/services/cache/query_cache.hpp>

#include <string>
#include <vector>

namespace {

using namespace dicom_viewer::services;

class QueryCacheManagerTest : public ::testing::Test {
protected:
    QueryCacheManager manager;

    QueryCacheConfig makeEnabledConfig(size_t maxEntries = 100,
                                       std::chrono::seconds ttl = std::chrono::seconds{300}) {
        QueryCacheConfig config;
        config.enabled = true;
        config.maxEntries = maxEntries;
        config.ttl = ttl;
        return config;
    }

    CachedResult makeSampleResult(const std::string& level = "STUDY",
                                  uint32_t matchCount = 5) {
        CachedResult result;
        result.data = {0x01, 0x02, 0x03, 0x04};
        result.matchCount = matchCount;
        result.queryLevel = level;
        return result;
    }
};

// --- Default State ---

TEST_F(QueryCacheManagerTest, DefaultNotEnabled) {
    EXPECT_FALSE(manager.isEnabled());
}

TEST_F(QueryCacheManagerTest, DefaultConfigDisabled) {
    auto config = manager.getConfig();
    EXPECT_FALSE(config.enabled);
}

TEST_F(QueryCacheManagerTest, DefaultStatisticsZero) {
    auto stats = manager.getStatistics();
    EXPECT_EQ(stats.hits, 0u);
    EXPECT_EQ(stats.misses, 0u);
    EXPECT_EQ(stats.insertions, 0u);
    EXPECT_EQ(stats.evictions, 0u);
    EXPECT_EQ(stats.currentSize, 0u);
}

TEST_F(QueryCacheManagerTest, LookupReturnsNulloptWhenDisabled) {
    auto result = manager.lookup("any_key");
    EXPECT_FALSE(result.has_value());
}

// --- Configuration Validation ---

TEST_F(QueryCacheManagerTest, ConfigureDisabledSucceeds) {
    QueryCacheConfig config;
    config.enabled = false;
    auto result = manager.configure(config);
    EXPECT_TRUE(result.has_value())
        << "Disabled configuration should always succeed";
}

TEST_F(QueryCacheManagerTest, ConfigureZeroEntriesFails) {
    QueryCacheConfig config;
    config.enabled = true;
    config.maxEntries = 0;
    auto result = manager.configure(config);
    EXPECT_FALSE(result.has_value())
        << "Zero max entries should fail validation";
}

TEST_F(QueryCacheManagerTest, ConfigureZeroTtlFails) {
    QueryCacheConfig config;
    config.enabled = true;
    config.ttl = std::chrono::seconds{0};
    auto result = manager.configure(config);
    EXPECT_FALSE(result.has_value())
        << "Zero TTL should fail validation";
}

TEST_F(QueryCacheManagerTest, ConfigureValidSucceeds) {
    auto result = manager.configure(makeEnabledConfig());
    EXPECT_TRUE(result.has_value())
        << "Valid configuration should succeed";
}

TEST_F(QueryCacheManagerTest, ConfigurePreservesSettings) {
    QueryCacheConfig config;
    config.enabled = true;
    config.maxEntries = 500;
    config.ttl = std::chrono::seconds{120};
    config.cacheName = "test_cache";

    (void)manager.configure(config);
    auto retrieved = manager.getConfig();

    EXPECT_EQ(retrieved.maxEntries, 500u);
    EXPECT_EQ(retrieved.ttl.count(), 120);
    EXPECT_EQ(retrieved.cacheName, "test_cache");
}

// --- Enable/Disable ---

TEST_F(QueryCacheManagerTest, EnableAfterConfigure) {
    (void)manager.configure(makeEnabledConfig());
    EXPECT_TRUE(manager.isEnabled());
}

TEST_F(QueryCacheManagerTest, DisableAfterEnable) {
    (void)manager.configure(makeEnabledConfig());
    manager.setEnabled(false);
    EXPECT_FALSE(manager.isEnabled());
}

TEST_F(QueryCacheManagerTest, ReEnable) {
    (void)manager.configure(makeEnabledConfig());
    manager.setEnabled(false);
    manager.setEnabled(true);
    EXPECT_TRUE(manager.isEnabled());
}

// --- Store and Lookup ---

TEST_F(QueryCacheManagerTest, StoreAndLookup) {
    (void)manager.configure(makeEnabledConfig());

    auto key = QueryCacheManager::buildKey("STUDY", {
        {"PatientID", "PAT001"},
        {"StudyDate", "20240101"}
    });

    manager.store(key, makeSampleResult());

    auto cached = manager.lookup(key);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->matchCount, 5u);
    EXPECT_EQ(cached->queryLevel, "STUDY");
    EXPECT_EQ(cached->data.size(), 4u);
}

TEST_F(QueryCacheManagerTest, LookupMissReturnsNullopt) {
    (void)manager.configure(makeEnabledConfig());

    auto result = manager.lookup("nonexistent_key");
    EXPECT_FALSE(result.has_value());
}

TEST_F(QueryCacheManagerTest, StoreOverwritesExisting) {
    (void)manager.configure(makeEnabledConfig());
    auto key = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});

    manager.store(key, makeSampleResult("STUDY", 5));
    manager.store(key, makeSampleResult("STUDY", 10));

    auto cached = manager.lookup(key);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->matchCount, 10u);
}

TEST_F(QueryCacheManagerTest, StoreDoesNothingWhenDisabled) {
    (void)manager.configure(makeEnabledConfig());
    manager.setEnabled(false);

    auto key = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});
    manager.store(key, makeSampleResult());

    manager.setEnabled(true);
    auto cached = manager.lookup(key);
    EXPECT_FALSE(cached.has_value())
        << "Store should be no-op when disabled";
}

// --- Invalidation ---

TEST_F(QueryCacheManagerTest, InvalidateRemovesEntry) {
    (void)manager.configure(makeEnabledConfig());
    auto key = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});

    manager.store(key, makeSampleResult());
    EXPECT_TRUE(manager.invalidate(key));

    auto cached = manager.lookup(key);
    EXPECT_FALSE(cached.has_value());
}

TEST_F(QueryCacheManagerTest, InvalidateNonexistentReturnsFalse) {
    (void)manager.configure(makeEnabledConfig());
    EXPECT_FALSE(manager.invalidate("nonexistent_key"));
}

TEST_F(QueryCacheManagerTest, InvalidateByQueryLevel) {
    (void)manager.configure(makeEnabledConfig());

    auto studyKey1 = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});
    auto studyKey2 = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT002"}});
    auto patientKey = QueryCacheManager::buildKey("PATIENT", {{"PatientName", "Test"}});

    manager.store(studyKey1, makeSampleResult("STUDY"));
    manager.store(studyKey2, makeSampleResult("STUDY"));
    manager.store(patientKey, makeSampleResult("PATIENT"));

    auto removed = manager.invalidateByQueryLevel("STUDY");
    EXPECT_EQ(removed, 2u);

    EXPECT_FALSE(manager.lookup(studyKey1).has_value());
    EXPECT_FALSE(manager.lookup(studyKey2).has_value());
    EXPECT_TRUE(manager.lookup(patientKey).has_value());
}

TEST_F(QueryCacheManagerTest, Clear) {
    (void)manager.configure(makeEnabledConfig());

    auto key1 = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});
    auto key2 = QueryCacheManager::buildKey("PATIENT", {{"PatientName", "Test"}});

    manager.store(key1, makeSampleResult());
    manager.store(key2, makeSampleResult("PATIENT"));

    manager.clear();

    EXPECT_FALSE(manager.lookup(key1).has_value());
    EXPECT_FALSE(manager.lookup(key2).has_value());
}

// --- Statistics ---

TEST_F(QueryCacheManagerTest, StatisticsTrackHitsAndMisses) {
    (void)manager.configure(makeEnabledConfig());

    auto key = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});
    manager.store(key, makeSampleResult());

    (void)manager.lookup(key);         // hit
    (void)manager.lookup("missing");   // miss

    auto stats = manager.getStatistics();
    EXPECT_EQ(stats.hits, 1u);
    EXPECT_EQ(stats.misses, 1u);
    EXPECT_EQ(stats.insertions, 1u);
}

TEST_F(QueryCacheManagerTest, ResetStatistics) {
    (void)manager.configure(makeEnabledConfig());

    auto key = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});
    manager.store(key, makeSampleResult());
    (void)manager.lookup(key);

    manager.resetStatistics();
    auto stats = manager.getStatistics();
    EXPECT_EQ(stats.hits, 0u);
    EXPECT_EQ(stats.misses, 0u);
}

TEST_F(QueryCacheManagerTest, ResetStatisticsWhenNotConfigured) {
    EXPECT_NO_THROW(manager.resetStatistics());
    auto stats = manager.getStatistics();
    EXPECT_EQ(stats.hits, 0u);
}

// --- LRU Eviction ---

TEST_F(QueryCacheManagerTest, LruEviction) {
    (void)manager.configure(makeEnabledConfig(3));

    auto key1 = QueryCacheManager::buildKey("STUDY", {{"ID", "1"}});
    auto key2 = QueryCacheManager::buildKey("STUDY", {{"ID", "2"}});
    auto key3 = QueryCacheManager::buildKey("STUDY", {{"ID", "3"}});
    auto key4 = QueryCacheManager::buildKey("STUDY", {{"ID", "4"}});

    manager.store(key1, makeSampleResult());
    manager.store(key2, makeSampleResult());
    manager.store(key3, makeSampleResult());
    manager.store(key4, makeSampleResult());  // evicts key1

    EXPECT_FALSE(manager.lookup(key1).has_value())
        << "Oldest entry should be evicted";
    EXPECT_TRUE(manager.lookup(key4).has_value())
        << "Newest entry should exist";
}

// --- Key Generation ---

TEST_F(QueryCacheManagerTest, BuildKeyDeterministic) {
    auto key1 = QueryCacheManager::buildKey("STUDY", {
        {"PatientID", "PAT001"},
        {"StudyDate", "20240101"}
    });

    auto key2 = QueryCacheManager::buildKey("STUDY", {
        {"StudyDate", "20240101"},
        {"PatientID", "PAT001"}
    });

    EXPECT_EQ(key1, key2)
        << "Key generation should be deterministic regardless of param order";
}

TEST_F(QueryCacheManagerTest, BuildKeyDifferentLevels) {
    auto studyKey = QueryCacheManager::buildKey("STUDY", {{"PatientID", "PAT001"}});
    auto patientKey = QueryCacheManager::buildKey("PATIENT", {{"PatientID", "PAT001"}});

    EXPECT_NE(studyKey, patientKey)
        << "Different query levels should produce different keys";
}

TEST_F(QueryCacheManagerTest, BuildKeyWithAeIncludesAeTitle) {
    auto key1 = QueryCacheManager::buildKeyWithAe("AE1", "STUDY",
        {{"PatientID", "PAT001"}});
    auto key2 = QueryCacheManager::buildKeyWithAe("AE2", "STUDY",
        {{"PatientID", "PAT001"}});

    EXPECT_NE(key1, key2)
        << "Different AE titles should produce different keys";
}

TEST_F(QueryCacheManagerTest, BuildKeyNotEmpty) {
    auto key = QueryCacheManager::buildKey("STUDY", {});
    EXPECT_FALSE(key.empty());
}

// --- pacs_system Cache Integration ---

TEST_F(QueryCacheManagerTest, PacsQueryCacheDirectConstruction) {
    kcenon::pacs::services::cache::query_cache_config config;
    config.max_entries = 10;
    config.ttl = std::chrono::seconds{60};

    kcenon::pacs::services::cache::query_cache cache(config);
    EXPECT_EQ(cache.max_size(), 10u);
    EXPECT_TRUE(cache.empty());
}

TEST_F(QueryCacheManagerTest, PacsQueryCacheBuildKey) {
    auto key = kcenon::pacs::services::cache::query_cache::build_key("STUDY", {
        {"PatientID", "12345"}
    });
    EXPECT_FALSE(key.empty());
}

// --- Safe Operations When Disabled ---

TEST_F(QueryCacheManagerTest, OperationsDoNotCrashWhenDisabled) {
    EXPECT_NO_THROW(manager.store("key", makeSampleResult()));
    EXPECT_NO_THROW(manager.invalidate("key"));
    EXPECT_NO_THROW(manager.invalidateByQueryLevel("STUDY"));
    EXPECT_NO_THROW(manager.invalidateByPrefix("prefix"));
    EXPECT_NO_THROW(manager.clear());
    EXPECT_NO_THROW(manager.purgeExpired());
}

TEST_F(QueryCacheManagerTest, OperationsDoNotCrashWhenConfiguredButDisabled) {
    QueryCacheConfig config;
    config.enabled = false;
    (void)manager.configure(config);

    EXPECT_NO_THROW(manager.store("key", makeSampleResult()));
    EXPECT_NO_THROW(manager.lookup("key"));
}

// --- Move Semantics ---

TEST_F(QueryCacheManagerTest, MoveConstruction) {
    QueryCacheConfig config;
    config.enabled = false;
    config.maxEntries = 500;
    (void)manager.configure(config);

    QueryCacheManager moved(std::move(manager));
    auto retrievedConfig = moved.getConfig();
    EXPECT_EQ(retrievedConfig.maxEntries, 500u);
}

TEST_F(QueryCacheManagerTest, MoveAssignment) {
    QueryCacheConfig config;
    config.enabled = false;
    config.maxEntries = 250;
    (void)manager.configure(config);

    QueryCacheManager other;
    other = std::move(manager);
    auto retrievedConfig = other.getConfig();
    EXPECT_EQ(retrievedConfig.maxEntries, 250u);
}

// --- QueryCacheConfig::isValid ---

TEST_F(QueryCacheManagerTest, DisabledConfigAlwaysValid) {
    QueryCacheConfig config;
    config.enabled = false;
    config.maxEntries = 0;
    config.ttl = std::chrono::seconds{0};
    EXPECT_TRUE(config.isValid())
        << "Disabled config should always be valid regardless of fields";
}

TEST_F(QueryCacheManagerTest, ValidEnabledConfig) {
    QueryCacheConfig config;
    config.enabled = true;
    config.maxEntries = 100;
    config.ttl = std::chrono::seconds{60};
    EXPECT_TRUE(config.isValid());
}

} // anonymous namespace

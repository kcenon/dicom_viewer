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

/**
 * @file query_cache_manager.hpp
 * @brief C-FIND query result caching service for improved performance
 * @details Wraps pacs_system's query_cache to provide a viewer-level
 *          caching service for DICOM C-FIND query results. Supports
 *          configurable cache size, TTL, and per-query-level invalidation.
 *
 * ## Thread Safety
 * - All public methods are thread-safe
 * - Configuration changes take effect immediately
 * - Statistics can be read concurrently with cache operations
 *
 * @see pacs::services::cache::query_cache
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "dicom_echo_scu.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Configuration for the query cache
 */
struct QueryCacheConfig {
    /// Enable/disable caching
    bool enabled = false;

    /// Maximum number of cached query results
    size_t maxEntries = 1000;

    /// Time-to-live for cached results in seconds
    std::chrono::seconds ttl{300};

    /// Cache identifier for logging
    std::string cacheName = "cfind_query_cache";

    /**
     * @brief Validate the configuration
     * @return true if configuration is valid for use
     */
    [[nodiscard]] bool isValid() const noexcept {
        if (!enabled) return true;
        return maxEntries > 0 && ttl.count() > 0;
    }
};

/**
 * @brief Cached query result metadata
 */
struct CachedResult {
    /// Serialized query result data
    std::vector<uint8_t> data;

    /// Number of matching records
    uint32_t matchCount = 0;

    /// Query level (PATIENT, STUDY, SERIES, IMAGE)
    std::string queryLevel;
};

/**
 * @brief Cache performance statistics
 */
struct QueryCacheStatistics {
    /// Number of cache hits
    uint64_t hits = 0;

    /// Number of cache misses
    uint64_t misses = 0;

    /// Number of insertions
    uint64_t insertions = 0;

    /// Number of LRU evictions
    uint64_t evictions = 0;

    /// Number of TTL expirations
    uint64_t expirations = 0;

    /// Current number of cached entries
    size_t currentSize = 0;

    /// Cache hit rate as percentage (0.0 to 100.0)
    double hitRate = 0.0;
};

/**
 * @brief C-FIND query result caching service
 *
 * Provides a viewer-level API for caching DICOM C-FIND query results
 * using an LRU eviction policy with configurable TTL. Wraps
 * pacs_system's query_cache with viewer-specific configuration.
 *
 * @example
 * @code
 * QueryCacheManager cache;
 * QueryCacheConfig config;
 * config.enabled = true;
 * config.maxEntries = 500;
 * config.ttl = std::chrono::seconds{120};
 *
 * auto result = cache.configure(config);
 * if (result) {
 *     auto key = QueryCacheManager::buildKey("STUDY", {
 *         {"PatientID", "12345"},
 *         {"StudyDate", "20240101"}
 *     });
 *
 *     CachedResult entry;
 *     entry.data = serialized_data;
 *     entry.matchCount = 5;
 *     entry.queryLevel = "STUDY";
 *     cache.store(key, entry);
 *
 *     auto cached = cache.lookup(key);
 *     if (cached) {
 *         // Use cached result
 *     }
 * }
 * @endcode
 *
 * @trace SRS-FR-040
 */
class QueryCacheManager {
public:
    QueryCacheManager();
    ~QueryCacheManager();

    // Non-copyable, movable
    QueryCacheManager(const QueryCacheManager&) = delete;
    QueryCacheManager& operator=(const QueryCacheManager&) = delete;
    QueryCacheManager(QueryCacheManager&&) noexcept;
    QueryCacheManager& operator=(QueryCacheManager&&) noexcept;

    /**
     * @brief Configure and initialize the cache
     *
     * Creates the underlying query cache based on the provided
     * configuration. If config.enabled is false, the cache will
     * accept calls but return misses for all lookups.
     *
     * @param config Cache configuration
     * @return void on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<void, PacsErrorInfo> configure(
        const QueryCacheConfig& config);

    /**
     * @brief Check if the cache is configured and enabled
     */
    [[nodiscard]] bool isEnabled() const;

    /**
     * @brief Enable or disable caching
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] QueryCacheConfig getConfig() const;

    // -- Cache Operations --

    /**
     * @brief Look up a cached query result
     *
     * @param key The cache key (use buildKey to generate)
     * @return The cached result if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<CachedResult> lookup(const std::string& key);

    /**
     * @brief Store a query result in the cache
     *
     * @param key The cache key
     * @param result The query result to cache
     */
    void store(const std::string& key, const CachedResult& result);

    /**
     * @brief Remove a specific entry from the cache
     *
     * @param key The cache key
     * @return true if the entry was found and removed
     */
    bool invalidate(const std::string& key);

    /**
     * @brief Remove all entries for a specific query level
     *
     * @param queryLevel The query level (PATIENT, STUDY, SERIES, IMAGE)
     * @return Number of entries removed
     */
    size_t invalidateByQueryLevel(const std::string& queryLevel);

    /**
     * @brief Remove all entries with keys starting with the given prefix
     *
     * @param prefix The key prefix to match
     * @return Number of entries removed
     */
    size_t invalidateByPrefix(const std::string& prefix);

    /**
     * @brief Remove all entries from the cache
     */
    void clear();

    /**
     * @brief Remove all expired entries
     * @return Number of entries removed
     */
    size_t purgeExpired();

    // -- Statistics --

    /**
     * @brief Get cache performance statistics
     */
    [[nodiscard]] QueryCacheStatistics getStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    // -- Key Generation --

    /**
     * @brief Build a cache key from query parameters
     *
     * Creates a deterministic key from the query level and parameters.
     * Parameters are sorted by name for consistent key generation.
     *
     * @param queryLevel The query retrieve level (PATIENT, STUDY, SERIES, IMAGE)
     * @param params List of parameter name-value pairs
     * @return Cache key string
     */
    [[nodiscard]] static std::string buildKey(
        const std::string& queryLevel,
        const std::vector<std::pair<std::string, std::string>>& params);

    /**
     * @brief Build a cache key with AE title prefix
     *
     * Includes the calling AE title in the key for per-client caching.
     *
     * @param callingAe The calling AE title
     * @param queryLevel The query retrieve level
     * @param params List of parameter name-value pairs
     * @return Cache key string
     */
    [[nodiscard]] static std::string buildKeyWithAe(
        const std::string& callingAe,
        const std::string& queryLevel,
        const std::vector<std::pair<std::string, std::string>>& params);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

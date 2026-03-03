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

#include "services/query_cache_manager.hpp"

#include <mutex>

#include <spdlog/spdlog.h>

#include <pacs/services/cache/query_cache.hpp>

namespace dicom_viewer::services {

class QueryCacheManager::Impl {
public:
    Impl() = default;

    std::expected<void, PacsErrorInfo> configure(const QueryCacheConfig& config) {
        std::lock_guard lock(mutex_);

        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid query cache configuration"
            });
        }

        config_ = config;

        if (!config.enabled) {
            cache_.reset();
            spdlog::info("Query cache configured but disabled");
            return {};
        }

        pacs::services::cache::query_cache_config qc;
        qc.max_entries = config.maxEntries;
        qc.ttl = config.ttl;
        qc.cache_name = config.cacheName;
        qc.enable_logging = false;
        qc.enable_metrics = true;

        try {
            cache_ = std::make_unique<pacs::services::cache::query_cache>(qc);
        } catch (const std::exception& e) {
            cache_.reset();
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to initialize query cache: ") + e.what()
            });
        }

        spdlog::info("Query cache configured: max_entries={}, ttl={}s",
                     config.maxEntries, config.ttl.count());
        return {};
    }

    bool isEnabled() const {
        std::lock_guard lock(mutex_);
        return config_.enabled && cache_ != nullptr;
    }

    void setEnabled(bool enabled) {
        std::lock_guard lock(mutex_);
        config_.enabled = enabled;
    }

    QueryCacheConfig getConfig() const {
        std::lock_guard lock(mutex_);
        return config_;
    }

    std::optional<CachedResult> lookup(const std::string& key) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return std::nullopt;

        auto result = cache_->get(key);
        if (!result) return std::nullopt;

        CachedResult cr;
        cr.data = std::move(result->data);
        cr.matchCount = result->match_count;
        cr.queryLevel = std::move(result->query_level);
        return cr;
    }

    void store(const std::string& key, const CachedResult& result) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return;

        pacs::services::cache::cached_query_result cqr;
        cqr.data = result.data;
        cqr.match_count = result.matchCount;
        cqr.cached_at = std::chrono::steady_clock::now();
        cqr.query_level = result.queryLevel;

        cache_->put(key, std::move(cqr));
    }

    bool invalidate(const std::string& key) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return false;

        return cache_->invalidate(key);
    }

    size_t invalidateByQueryLevel(const std::string& queryLevel) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return 0;

        return cache_->invalidate_by_query_level(queryLevel);
    }

    size_t invalidateByPrefix(const std::string& prefix) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return 0;

        return cache_->invalidate_by_prefix(prefix);
    }

    void clear() {
        std::lock_guard lock(mutex_);
        if (cache_) {
            cache_->clear();
        }
    }

    size_t purgeExpired() {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return 0;

        return cache_->purge_expired();
    }

    QueryCacheStatistics getStatistics() const {
        std::lock_guard lock(mutex_);
        if (!cache_) {
            return {};
        }

        const auto& stats = cache_->stats();
        QueryCacheStatistics qs;
        qs.hits = stats.hits.load(std::memory_order_relaxed);
        qs.misses = stats.misses.load(std::memory_order_relaxed);
        qs.insertions = stats.insertions.load(std::memory_order_relaxed);
        qs.evictions = stats.evictions.load(std::memory_order_relaxed);
        qs.expirations = stats.expirations.load(std::memory_order_relaxed);
        qs.currentSize = stats.current_size.load(std::memory_order_relaxed);
        qs.hitRate = cache_->hit_rate();
        return qs;
    }

    void resetStatistics() {
        std::lock_guard lock(mutex_);
        if (cache_) {
            cache_->reset_stats();
        }
    }

private:
    bool isEnabledLocked() const {
        return config_.enabled && cache_ != nullptr;
    }

    QueryCacheConfig config_;
    std::unique_ptr<pacs::services::cache::query_cache> cache_;
    mutable std::mutex mutex_;
};

// Public interface implementation

QueryCacheManager::QueryCacheManager()
    : impl_(std::make_unique<Impl>()) {
}

QueryCacheManager::~QueryCacheManager() = default;

QueryCacheManager::QueryCacheManager(QueryCacheManager&&) noexcept = default;
QueryCacheManager& QueryCacheManager::operator=(QueryCacheManager&&) noexcept = default;

std::expected<void, PacsErrorInfo> QueryCacheManager::configure(
    const QueryCacheConfig& config) {
    return impl_->configure(config);
}

bool QueryCacheManager::isEnabled() const {
    return impl_->isEnabled();
}

void QueryCacheManager::setEnabled(bool enabled) {
    impl_->setEnabled(enabled);
}

QueryCacheConfig QueryCacheManager::getConfig() const {
    return impl_->getConfig();
}

std::optional<CachedResult> QueryCacheManager::lookup(const std::string& key) {
    return impl_->lookup(key);
}

void QueryCacheManager::store(const std::string& key,
                               const CachedResult& result) {
    impl_->store(key, result);
}

bool QueryCacheManager::invalidate(const std::string& key) {
    return impl_->invalidate(key);
}

size_t QueryCacheManager::invalidateByQueryLevel(const std::string& queryLevel) {
    return impl_->invalidateByQueryLevel(queryLevel);
}

size_t QueryCacheManager::invalidateByPrefix(const std::string& prefix) {
    return impl_->invalidateByPrefix(prefix);
}

void QueryCacheManager::clear() {
    impl_->clear();
}

size_t QueryCacheManager::purgeExpired() {
    return impl_->purgeExpired();
}

QueryCacheStatistics QueryCacheManager::getStatistics() const {
    return impl_->getStatistics();
}

void QueryCacheManager::resetStatistics() {
    impl_->resetStatistics();
}

std::string QueryCacheManager::buildKey(
    const std::string& queryLevel,
    const std::vector<std::pair<std::string, std::string>>& params) {
    return pacs::services::cache::query_cache::build_key(queryLevel, params);
}

std::string QueryCacheManager::buildKeyWithAe(
    const std::string& callingAe,
    const std::string& queryLevel,
    const std::vector<std::pair<std::string, std::string>>& params) {
    return pacs::services::cache::query_cache::build_key_with_ae(
        callingAe, queryLevel, params);
}

} // namespace dicom_viewer::services

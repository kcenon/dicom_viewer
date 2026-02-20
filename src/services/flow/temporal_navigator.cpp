#include "services/flow/temporal_navigator.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include <kcenon/common/logging/log_macros.h>

namespace dicom_viewer::services {

// =============================================================================
// PhaseCache implementation
// =============================================================================

PhaseCache::PhaseCache(int windowSize)
    : windowSize_(std::max(1, windowSize)) {}

void PhaseCache::setPhaseLoader(
    std::function<std::expected<VelocityPhase, FlowError>(int)> loader) {
    std::lock_guard lock(mutex_);
    loader_ = std::move(loader);
}

std::expected<VelocityPhase, FlowError>
PhaseCache::getPhase(int phaseIndex) {
    std::lock_guard lock(mutex_);

    // Check cache first
    auto it = cache_.find(phaseIndex);
    if (it != cache_.end()) {
        touchPhase(phaseIndex);
        return it->second;
    }

    // Load from disk
    if (!loader_) {
        return std::unexpected(FlowError{
            FlowError::Code::InternalError,
            "No phase loader configured"});
    }

    auto result = loader_(phaseIndex);
    if (!result) {
        return result;
    }

    // Evict if needed before inserting
    evictIfNeeded();

    cache_[phaseIndex] = result.value();
    accessOrder_.push_front(phaseIndex);

    return result;
}

bool PhaseCache::isCached(int phaseIndex) const {
    std::lock_guard lock(mutex_);
    return cache_.contains(phaseIndex);
}

std::vector<int> PhaseCache::getCachedPhases() const {
    std::lock_guard lock(mutex_);
    std::vector<int> phases;
    phases.reserve(cache_.size());
    for (const auto& [idx, _] : cache_) {
        phases.push_back(idx);
    }
    std::sort(phases.begin(), phases.end());
    return phases;
}

CacheStatus PhaseCache::getStatus() const {
    std::lock_guard lock(mutex_);
    CacheStatus status;
    status.cachedCount = static_cast<int>(cache_.size());
    status.totalPhases = totalPhases_;
    status.windowSize = windowSize_;

    // Estimate memory: each VelocityPhase holds a VectorImage3D + FloatImage3D
    // Rough estimation only — actual size depends on image dimensions
    status.memoryUsageBytes = cache_.size() * 50 * 1024 * 1024;  // ~50MB/phase estimate

    return status;
}

void PhaseCache::setTotalPhases(int total) {
    std::lock_guard lock(mutex_);
    totalPhases_ = total;
}

void PhaseCache::clear() {
    std::lock_guard lock(mutex_);
    cache_.clear();
    accessOrder_.clear();
}

int PhaseCache::windowSize() const noexcept {
    return windowSize_;
}

void PhaseCache::evictIfNeeded() {
    // Already holding mutex
    while (static_cast<int>(cache_.size()) >= windowSize_ &&
           !accessOrder_.empty()) {
        int oldest = accessOrder_.back();
        accessOrder_.pop_back();
        cache_.erase(oldest);
    }
}

void PhaseCache::touchPhase(int phaseIndex) {
    // Already holding mutex — move to front of access order
    accessOrder_.remove(phaseIndex);
    accessOrder_.push_front(phaseIndex);
}

// =============================================================================
// TemporalNavigator implementation
// =============================================================================

class TemporalNavigator::Impl {
public:
    std::unique_ptr<PhaseCache> cache = std::make_unique<PhaseCache>(5);
    int phaseCount_ = 0;
    double temporalResolution_ = 0.0;
    int currentPhase_ = 0;
    bool initialized_ = false;

    PlaybackState playback;

    PhaseChangedCallback phaseChangedCb;
    PlaybackChangedCallback playbackChangedCb;
    CacheStatusCallback cacheStatusCb;

    void notifyPhaseChanged(int phase) {
        if (phaseChangedCb) {
            phaseChangedCb(phase);
        }
    }

    void notifyPlaybackChanged() {
        if (playbackChangedCb) {
            playbackChangedCb(playback);
        }
    }

    void notifyCacheStatus() {
        if (cacheStatusCb) {
            cacheStatusCb(cache->getStatus());
        }
    }

    int wrapPhase(int phase) const {
        if (phaseCount_ <= 0) return 0;
        if (playback.looping) {
            return ((phase % phaseCount_) + phaseCount_) % phaseCount_;
        }
        return std::clamp(phase, 0, phaseCount_ - 1);
    }
};

TemporalNavigator::TemporalNavigator()
    : impl_(std::make_unique<Impl>()) {}

TemporalNavigator::~TemporalNavigator() = default;

TemporalNavigator::TemporalNavigator(TemporalNavigator&&) noexcept = default;
TemporalNavigator& TemporalNavigator::operator=(TemporalNavigator&&) noexcept = default;

void TemporalNavigator::initialize(
    int phaseCount, double temporalResolution, int cacheWindowSize) {
    impl_->phaseCount_ = phaseCount;
    impl_->temporalResolution_ = temporalResolution;
    impl_->currentPhase_ = 0;
    impl_->initialized_ = true;

    impl_->cache = std::make_unique<PhaseCache>(cacheWindowSize);
    impl_->cache->setTotalPhases(phaseCount);

    impl_->playback = PlaybackState{};
    impl_->playback.currentPhase = 0;
    impl_->playback.currentTimeMs = 0.0;

    LOG_INFO(std::format("Initialized: {} phases, {:.1f} ms/phase, cache={}",
                         phaseCount, temporalResolution, cacheWindowSize));
}

void TemporalNavigator::setPhaseLoader(
    std::function<std::expected<VelocityPhase, FlowError>(int)> loader) {
    impl_->cache->setPhaseLoader(std::move(loader));
}

std::expected<VelocityPhase, FlowError>
TemporalNavigator::goToPhase(int phaseIndex) {
    if (!impl_->initialized_) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "TemporalNavigator not initialized"});
    }

    if (phaseIndex < 0 || phaseIndex >= impl_->phaseCount_) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Phase index " + std::to_string(phaseIndex) + " out of range [0, " +
                std::to_string(impl_->phaseCount_) + ")"});
    }

    auto result = impl_->cache->getPhase(phaseIndex);
    if (result) {
        impl_->currentPhase_ = phaseIndex;
        impl_->playback.currentPhase = phaseIndex;
        impl_->playback.currentTimeMs =
            phaseIndex * impl_->temporalResolution_;

        impl_->notifyPhaseChanged(phaseIndex);
        impl_->notifyCacheStatus();
    }

    return result;
}

std::expected<VelocityPhase, FlowError>
TemporalNavigator::nextPhase() {
    if (!impl_->initialized_) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "TemporalNavigator not initialized"});
    }

    int next = impl_->currentPhase_ + 1;
    if (next >= impl_->phaseCount_) {
        if (impl_->playback.looping) {
            next = 0;
        } else {
            next = impl_->phaseCount_ - 1;
        }
    }

    return goToPhase(next);
}

std::expected<VelocityPhase, FlowError>
TemporalNavigator::previousPhase() {
    if (!impl_->initialized_) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "TemporalNavigator not initialized"});
    }

    int prev = impl_->currentPhase_ - 1;
    if (prev < 0) {
        if (impl_->playback.looping) {
            prev = impl_->phaseCount_ - 1;
        } else {
            prev = 0;
        }
    }

    return goToPhase(prev);
}

void TemporalNavigator::play(double fps) {
    impl_->playback.isPlaying = true;
    impl_->playback.fps = std::clamp(fps, 1.0, 60.0);
    impl_->notifyPlaybackChanged();
    LOG_DEBUG(std::format("Playback started: {:.1f} fps", fps));
}

void TemporalNavigator::pause() {
    impl_->playback.isPlaying = false;
    impl_->notifyPlaybackChanged();
}

void TemporalNavigator::stop() {
    impl_->playback.isPlaying = false;
    impl_->currentPhase_ = 0;
    impl_->playback.currentPhase = 0;
    impl_->playback.currentTimeMs = 0.0;
    impl_->notifyPlaybackChanged();
    impl_->notifyPhaseChanged(0);
}

void TemporalNavigator::setPlaybackSpeed(double multiplier) {
    impl_->playback.speedMultiplier = std::clamp(multiplier, 0.25, 4.0);
    impl_->notifyPlaybackChanged();
}

void TemporalNavigator::setLooping(bool loop) {
    impl_->playback.looping = loop;
    impl_->notifyPlaybackChanged();
}

std::expected<VelocityPhase, FlowError>
TemporalNavigator::tick() {
    if (!impl_->playback.isPlaying) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Not in playback mode"});
    }

    int next = impl_->currentPhase_ + 1;
    if (next >= impl_->phaseCount_) {
        if (impl_->playback.looping) {
            next = 0;
        } else {
            pause();
            return std::unexpected(FlowError{
                FlowError::Code::InvalidInput,
                "Reached end of sequence"});
        }
    }

    return goToPhase(next);
}

int TemporalNavigator::currentPhase() const {
    return impl_->currentPhase_;
}

int TemporalNavigator::phaseCount() const {
    return impl_->phaseCount_;
}

double TemporalNavigator::temporalResolution() const {
    return impl_->temporalResolution_;
}

double TemporalNavigator::currentTimeMs() const {
    return impl_->currentPhase_ * impl_->temporalResolution_;
}

PlaybackState TemporalNavigator::playbackState() const {
    return impl_->playback;
}

CacheStatus TemporalNavigator::cacheStatus() const {
    return impl_->cache->getStatus();
}

bool TemporalNavigator::isInitialized() const {
    return impl_->initialized_;
}

void TemporalNavigator::setPhaseChangedCallback(PhaseChangedCallback callback) {
    impl_->phaseChangedCb = std::move(callback);
}

void TemporalNavigator::setPlaybackChangedCallback(
    PlaybackChangedCallback callback) {
    impl_->playbackChangedCb = std::move(callback);
}

void TemporalNavigator::setCacheStatusCallback(CacheStatusCallback callback) {
    impl_->cacheStatusCb = std::move(callback);
}

}  // namespace dicom_viewer::services

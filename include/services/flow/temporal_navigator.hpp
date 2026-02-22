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

#pragma once

#include <expected>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/velocity_field_assembler.hpp"

namespace dicom_viewer::services {

/**
 * @brief Cache status information for monitoring
 *
 * @trace SRS-FR-048
 */
struct CacheStatus {
    int cachedCount = 0;
    int totalPhases = 0;
    size_t memoryUsageBytes = 0;
    int windowSize = 0;
};

/**
 * @brief Playback state for cine mode
 *
 * @trace SRS-FR-048
 */
struct PlaybackState {
    bool isPlaying = false;
    double fps = 15.0;
    double speedMultiplier = 1.0;
    bool looping = true;
    int currentPhase = 0;
    double currentTimeMs = 0.0;
};

/**
 * @brief LRU sliding window cache for velocity phase data
 *
 * Manages memory by keeping only a configurable number of phases
 * in memory, evicting least-recently-used phases when the window
 * size is exceeded.
 *
 * @trace SRS-FR-048
 */
class PhaseCache {
public:
    /**
     * @brief Construct cache with window size
     * @param windowSize Maximum number of phases to keep in memory
     */
    explicit PhaseCache(int windowSize = 5);

    /**
     * @brief Set the phase loader function
     *
     * The loader is called when a phase is not in cache and needs
     * to be loaded from disk.
     *
     * @param loader Function that loads a phase by index
     */
    void setPhaseLoader(
        std::function<std::expected<VelocityPhase, FlowError>(int)> loader);

    /**
     * @brief Get a phase, loading if not cached
     * @param phaseIndex 0-based phase index
     * @return VelocityPhase on success, FlowError on failure
     */
    [[nodiscard]] std::expected<VelocityPhase, FlowError>
    getPhase(int phaseIndex);

    /**
     * @brief Check if a phase is currently cached
     */
    [[nodiscard]] bool isCached(int phaseIndex) const;

    /**
     * @brief Get all currently cached phase indices
     */
    [[nodiscard]] std::vector<int> getCachedPhases() const;

    /**
     * @brief Get cache statistics
     */
    [[nodiscard]] CacheStatus getStatus() const;

    /**
     * @brief Set total number of phases (for status reporting)
     */
    void setTotalPhases(int total);

    /**
     * @brief Clear all cached phases
     */
    void clear();

    /**
     * @brief Get the window size
     */
    [[nodiscard]] int windowSize() const noexcept;

private:
    void evictIfNeeded();
    void touchPhase(int phaseIndex);

    int windowSize_;
    int totalPhases_ = 0;
    std::unordered_map<int, VelocityPhase> cache_;
    std::list<int> accessOrder_;  // Front = most recent
    std::function<std::expected<VelocityPhase, FlowError>(int)> loader_;
    mutable std::mutex mutex_;
};

/**
 * @brief Cardiac phase navigation controller for 4D Flow MRI
 *
 * Provides phase-by-phase navigation, cine playback controls,
 * and LRU cache management for temporal 4D Flow sequences.
 *
 * This is a service-layer class without Qt dependency. UI integration
 * (QTimer, signals/slots) is handled by the UI layer.
 *
 * @trace SRS-FR-048
 */
class TemporalNavigator {
public:
    /// Callback when phase changes
    using PhaseChangedCallback = std::function<void(int phaseIndex)>;
    /// Callback when playback state changes
    using PlaybackChangedCallback = std::function<void(const PlaybackState& state)>;
    /// Callback for cache status updates
    using CacheStatusCallback = std::function<void(const CacheStatus& status)>;

    TemporalNavigator();
    ~TemporalNavigator();

    // Non-copyable, movable
    TemporalNavigator(const TemporalNavigator&) = delete;
    TemporalNavigator& operator=(const TemporalNavigator&) = delete;
    TemporalNavigator(TemporalNavigator&&) noexcept;
    TemporalNavigator& operator=(TemporalNavigator&&) noexcept;

    /**
     * @brief Initialize with series information
     * @param phaseCount Total number of cardiac phases
     * @param temporalResolution Time between phases (ms)
     * @param cacheWindowSize Number of phases to keep in memory
     */
    void initialize(int phaseCount, double temporalResolution,
                    int cacheWindowSize = 5);

    /**
     * @brief Set the phase loader for cache
     */
    void setPhaseLoader(
        std::function<std::expected<VelocityPhase, FlowError>(int)> loader);

    // --- Navigation ---

    /**
     * @brief Go to a specific phase
     * @return The loaded phase data, or error
     */
    [[nodiscard]] std::expected<VelocityPhase, FlowError>
    goToPhase(int phaseIndex);

    /** @brief Advance to next phase (wraps if looping) */
    [[nodiscard]] std::expected<VelocityPhase, FlowError> nextPhase();

    /** @brief Go to previous phase (wraps if looping) */
    [[nodiscard]] std::expected<VelocityPhase, FlowError> previousPhase();

    // --- Playback control ---

    /** @brief Start cine playback */
    void play(double fps = 15.0);

    /** @brief Pause playback */
    void pause();

    /** @brief Stop and reset to phase 0 */
    void stop();

    /** @brief Set playback speed multiplier (0.25x - 4x) */
    void setPlaybackSpeed(double multiplier);

    /** @brief Set looping mode */
    void setLooping(bool loop);

    /**
     * @brief Advance one tick in playback mode
     *
     * Call this method from a timer (e.g., QTimer) at the configured
     * frame rate. Returns the next phase to display.
     *
     * @return Next phase data, or error, or nullopt if not playing
     */
    [[nodiscard]] std::expected<VelocityPhase, FlowError> tick();

    // --- State queries ---

    [[nodiscard]] int currentPhase() const;
    [[nodiscard]] int phaseCount() const;
    [[nodiscard]] double temporalResolution() const;
    [[nodiscard]] double currentTimeMs() const;
    [[nodiscard]] PlaybackState playbackState() const;
    [[nodiscard]] CacheStatus cacheStatus() const;
    [[nodiscard]] bool isInitialized() const;

    // --- Callbacks ---

    void setPhaseChangedCallback(PhaseChangedCallback callback);
    void setPlaybackChangedCallback(PlaybackChangedCallback callback);
    void setCacheStatusCallback(CacheStatusCallback callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

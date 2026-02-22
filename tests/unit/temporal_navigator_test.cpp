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

#include <vector>

#include "services/flow/temporal_navigator.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a mock phase loader that returns synthetic VelocityPhase data
auto createMockLoader(int maxPhases) {
    return [maxPhases](int phaseIndex)
               -> std::expected<VelocityPhase, FlowError> {
        if (phaseIndex < 0 || phaseIndex >= maxPhases) {
            return std::unexpected(FlowError{
                FlowError::Code::InvalidInput,
                "Phase " + std::to_string(phaseIndex) + " out of range"});
        }
        VelocityPhase phase;
        phase.phaseIndex = phaseIndex;
        phase.triggerTime = phaseIndex * 40.0;  // 40ms per phase
        // velocityField is null — fine for navigation tests
        return phase;
    };
}

}  // anonymous namespace

// =============================================================================
// CacheStatus / PlaybackState defaults
// =============================================================================

TEST(CacheStatusTest, Defaults) {
    CacheStatus status;
    EXPECT_EQ(status.cachedCount, 0);
    EXPECT_EQ(status.totalPhases, 0);
    EXPECT_EQ(status.memoryUsageBytes, 0);
    EXPECT_EQ(status.windowSize, 0);
}

TEST(PlaybackStateTest, Defaults) {
    PlaybackState state;
    EXPECT_FALSE(state.isPlaying);
    EXPECT_DOUBLE_EQ(state.fps, 15.0);
    EXPECT_DOUBLE_EQ(state.speedMultiplier, 1.0);
    EXPECT_TRUE(state.looping);
    EXPECT_EQ(state.currentPhase, 0);
    EXPECT_DOUBLE_EQ(state.currentTimeMs, 0.0);
}

// =============================================================================
// PhaseCache tests
// =============================================================================

TEST(PhaseCacheTest, DefaultWindowSize) {
    PhaseCache cache;
    EXPECT_EQ(cache.windowSize(), 5);
}

TEST(PhaseCacheTest, CustomWindowSize) {
    PhaseCache cache(10);
    EXPECT_EQ(cache.windowSize(), 10);
}

TEST(PhaseCacheTest, MinimumWindowSize) {
    PhaseCache cache(0);
    EXPECT_EQ(cache.windowSize(), 1);
    PhaseCache cache2(-5);
    EXPECT_EQ(cache2.windowSize(), 1);
}

TEST(PhaseCacheTest, GetPhaseWithoutLoader) {
    PhaseCache cache(5);
    auto result = cache.getPhase(0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InternalError);
}

TEST(PhaseCacheTest, GetPhaseWithLoader) {
    PhaseCache cache(5);
    cache.setPhaseLoader(createMockLoader(20));
    cache.setTotalPhases(20);

    auto result = cache.getPhase(3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->phaseIndex, 3);
}

TEST(PhaseCacheTest, CachesLoadedPhases) {
    PhaseCache cache(5);
    cache.setPhaseLoader(createMockLoader(20));

    EXPECT_FALSE(cache.isCached(3));
    (void)cache.getPhase(3);
    EXPECT_TRUE(cache.isCached(3));
}

TEST(PhaseCacheTest, LRUEviction) {
    PhaseCache cache(3);  // Max 3 phases
    cache.setPhaseLoader(createMockLoader(20));

    (void)cache.getPhase(0);
    (void)cache.getPhase(1);
    (void)cache.getPhase(2);
    EXPECT_TRUE(cache.isCached(0));
    EXPECT_TRUE(cache.isCached(1));
    EXPECT_TRUE(cache.isCached(2));

    // Loading phase 3 should evict phase 0 (oldest)
    (void)cache.getPhase(3);
    EXPECT_FALSE(cache.isCached(0));
    EXPECT_TRUE(cache.isCached(1));
    EXPECT_TRUE(cache.isCached(2));
    EXPECT_TRUE(cache.isCached(3));
}

TEST(PhaseCacheTest, LRUTouchReorders) {
    PhaseCache cache(3);
    cache.setPhaseLoader(createMockLoader(20));

    (void)cache.getPhase(0);
    (void)cache.getPhase(1);
    (void)cache.getPhase(2);

    // Touch phase 0 again — now 1 is the oldest
    (void)cache.getPhase(0);

    // Loading phase 3 should evict phase 1 (now oldest)
    (void)cache.getPhase(3);
    EXPECT_TRUE(cache.isCached(0));   // Recently touched
    EXPECT_FALSE(cache.isCached(1));  // Evicted
    EXPECT_TRUE(cache.isCached(2));
    EXPECT_TRUE(cache.isCached(3));
}

TEST(PhaseCacheTest, GetCachedPhases) {
    PhaseCache cache(5);
    cache.setPhaseLoader(createMockLoader(20));

    (void)cache.getPhase(5);
    (void)cache.getPhase(2);
    (void)cache.getPhase(8);

    auto phases = cache.getCachedPhases();
    ASSERT_EQ(phases.size(), 3);
    EXPECT_EQ(phases[0], 2);  // Sorted
    EXPECT_EQ(phases[1], 5);
    EXPECT_EQ(phases[2], 8);
}

TEST(PhaseCacheTest, Clear) {
    PhaseCache cache(5);
    cache.setPhaseLoader(createMockLoader(20));

    (void)cache.getPhase(0);
    (void)cache.getPhase(1);
    EXPECT_EQ(cache.getCachedPhases().size(), 2);

    cache.clear();
    EXPECT_EQ(cache.getCachedPhases().size(), 0);
    EXPECT_FALSE(cache.isCached(0));
}

TEST(PhaseCacheTest, Status) {
    PhaseCache cache(5);
    cache.setPhaseLoader(createMockLoader(20));
    cache.setTotalPhases(20);

    (void)cache.getPhase(0);
    (void)cache.getPhase(1);

    auto status = cache.getStatus();
    EXPECT_EQ(status.cachedCount, 2);
    EXPECT_EQ(status.totalPhases, 20);
    EXPECT_EQ(status.windowSize, 5);
    EXPECT_GT(status.memoryUsageBytes, 0);
}

// =============================================================================
// TemporalNavigator construction tests
// =============================================================================

TEST(TemporalNavigatorTest, DefaultConstruction) {
    TemporalNavigator nav;
    EXPECT_FALSE(nav.isInitialized());
    EXPECT_EQ(nav.currentPhase(), 0);
    EXPECT_EQ(nav.phaseCount(), 0);
}

TEST(TemporalNavigatorTest, MoveConstruction) {
    TemporalNavigator nav;
    TemporalNavigator moved(std::move(nav));
}

TEST(TemporalNavigatorTest, MoveAssignment) {
    TemporalNavigator nav;
    TemporalNavigator other;
    other = std::move(nav);
}

TEST(TemporalNavigatorTest, Initialize) {
    TemporalNavigator nav;
    nav.initialize(25, 40.0, 5);
    EXPECT_TRUE(nav.isInitialized());
    EXPECT_EQ(nav.phaseCount(), 25);
    EXPECT_DOUBLE_EQ(nav.temporalResolution(), 40.0);
    EXPECT_EQ(nav.currentPhase(), 0);
}

// =============================================================================
// Navigation tests
// =============================================================================

TEST(TemporalNavigatorTest, GoToPhaseNotInitialized) {
    TemporalNavigator nav;
    auto result = nav.goToPhase(0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(TemporalNavigatorTest, GoToPhaseOutOfRange) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);
    nav.setPhaseLoader(createMockLoader(10));

    auto result = nav.goToPhase(15);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(TemporalNavigatorTest, GoToPhaseNegative) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);
    nav.setPhaseLoader(createMockLoader(10));

    auto result = nav.goToPhase(-1);
    ASSERT_FALSE(result.has_value());
}

TEST(TemporalNavigatorTest, GoToPhaseSuccess) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);
    nav.setPhaseLoader(createMockLoader(10));

    auto result = nav.goToPhase(5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->phaseIndex, 5);
    EXPECT_EQ(nav.currentPhase(), 5);
    EXPECT_DOUBLE_EQ(nav.currentTimeMs(), 200.0);  // 5 × 40ms
}

TEST(TemporalNavigatorTest, NextPhaseWraps) {
    TemporalNavigator nav;
    nav.initialize(3, 40.0);
    nav.setPhaseLoader(createMockLoader(3));
    nav.setLooping(true);

    (void)nav.goToPhase(2);  // Last phase
    auto result = nav.nextPhase();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(nav.currentPhase(), 0);  // Wrapped to start
}

TEST(TemporalNavigatorTest, NextPhaseNoWrap) {
    TemporalNavigator nav;
    nav.initialize(3, 40.0);
    nav.setPhaseLoader(createMockLoader(3));
    nav.setLooping(false);

    (void)nav.goToPhase(2);
    auto result = nav.nextPhase();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(nav.currentPhase(), 2);  // Stays at end
}

TEST(TemporalNavigatorTest, PreviousPhaseWraps) {
    TemporalNavigator nav;
    nav.initialize(3, 40.0);
    nav.setPhaseLoader(createMockLoader(3));
    nav.setLooping(true);

    // Already at phase 0
    auto result = nav.previousPhase();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(nav.currentPhase(), 2);  // Wrapped to end
}

TEST(TemporalNavigatorTest, PreviousPhaseNoWrap) {
    TemporalNavigator nav;
    nav.initialize(3, 40.0);
    nav.setPhaseLoader(createMockLoader(3));
    nav.setLooping(false);

    auto result = nav.previousPhase();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(nav.currentPhase(), 0);  // Stays at start
}

// =============================================================================
// Playback control tests
// =============================================================================

TEST(TemporalNavigatorTest, PlayPauseStop) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);

    EXPECT_FALSE(nav.playbackState().isPlaying);

    nav.play(20.0);
    EXPECT_TRUE(nav.playbackState().isPlaying);
    EXPECT_DOUBLE_EQ(nav.playbackState().fps, 20.0);

    nav.pause();
    EXPECT_FALSE(nav.playbackState().isPlaying);

    nav.play();
    nav.stop();
    EXPECT_FALSE(nav.playbackState().isPlaying);
    EXPECT_EQ(nav.currentPhase(), 0);  // Reset
}

TEST(TemporalNavigatorTest, PlaybackSpeedClamp) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);

    nav.setPlaybackSpeed(0.1);  // Below min
    EXPECT_DOUBLE_EQ(nav.playbackState().speedMultiplier, 0.25);

    nav.setPlaybackSpeed(10.0);  // Above max
    EXPECT_DOUBLE_EQ(nav.playbackState().speedMultiplier, 4.0);

    nav.setPlaybackSpeed(2.0);
    EXPECT_DOUBLE_EQ(nav.playbackState().speedMultiplier, 2.0);
}

TEST(TemporalNavigatorTest, FPSClamp) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);

    nav.play(0.5);  // Below min
    EXPECT_DOUBLE_EQ(nav.playbackState().fps, 1.0);

    nav.play(100.0);  // Above max
    EXPECT_DOUBLE_EQ(nav.playbackState().fps, 60.0);
}

TEST(TemporalNavigatorTest, TickAdvancesPhase) {
    TemporalNavigator nav;
    nav.initialize(5, 40.0);
    nav.setPhaseLoader(createMockLoader(5));

    nav.play();
    auto result = nav.tick();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(nav.currentPhase(), 1);

    result = nav.tick();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(nav.currentPhase(), 2);
}

TEST(TemporalNavigatorTest, TickWrapsWithLooping) {
    TemporalNavigator nav;
    nav.initialize(3, 40.0);
    nav.setPhaseLoader(createMockLoader(3));
    nav.setLooping(true);

    (void)nav.goToPhase(2);
    nav.play();
    auto result = nav.tick();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(nav.currentPhase(), 0);
}

TEST(TemporalNavigatorTest, TickStopsWithoutLooping) {
    TemporalNavigator nav;
    nav.initialize(3, 40.0);
    nav.setPhaseLoader(createMockLoader(3));
    nav.setLooping(false);

    (void)nav.goToPhase(2);
    nav.play();
    auto result = nav.tick();
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(nav.playbackState().isPlaying);  // Auto-paused
}

TEST(TemporalNavigatorTest, TickWhenNotPlaying) {
    TemporalNavigator nav;
    nav.initialize(5, 40.0);
    nav.setPhaseLoader(createMockLoader(5));

    auto result = nav.tick();
    ASSERT_FALSE(result.has_value());
}

// =============================================================================
// Callback tests
// =============================================================================

TEST(TemporalNavigatorTest, PhaseChangedCallback) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);
    nav.setPhaseLoader(createMockLoader(10));

    int lastPhase = -1;
    nav.setPhaseChangedCallback([&](int p) { lastPhase = p; });

    (void)nav.goToPhase(7);
    EXPECT_EQ(lastPhase, 7);
}

TEST(TemporalNavigatorTest, PlaybackChangedCallback) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);

    PlaybackState lastState;
    nav.setPlaybackChangedCallback([&](const PlaybackState& s) {
        lastState = s;
    });

    nav.play(25.0);
    EXPECT_TRUE(lastState.isPlaying);
    EXPECT_DOUBLE_EQ(lastState.fps, 25.0);

    nav.pause();
    EXPECT_FALSE(lastState.isPlaying);
}

TEST(TemporalNavigatorTest, CacheStatusCallback) {
    TemporalNavigator nav;
    nav.initialize(10, 40.0);
    nav.setPhaseLoader(createMockLoader(10));

    CacheStatus lastStatus;
    nav.setCacheStatusCallback([&](const CacheStatus& s) {
        lastStatus = s;
    });

    (void)nav.goToPhase(3);
    EXPECT_EQ(lastStatus.cachedCount, 1);
    EXPECT_EQ(lastStatus.totalPhases, 10);
}

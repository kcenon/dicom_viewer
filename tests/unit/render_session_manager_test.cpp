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

#include "services/render/render_session_manager.hpp"
#include "services/render/adaptive_quality_controller.hpp"
#include "services/render/render_session.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace dicom_viewer::services;

// =============================================================================
// Test fixture
// =============================================================================

class RenderSessionManagerTest : public ::testing::Test {
protected:
    RenderSessionManagerConfig defaultConfig()
    {
        RenderSessionManagerConfig cfg;
        cfg.maxSessions = 4;
        cfg.targetFps = 10;
        cfg.idleTimeoutSeconds = 1;
        cfg.defaultWidth = 64;
        cfg.defaultHeight = 64;
        return cfg;
    }
};

// =============================================================================
// Construction and configuration
// =============================================================================

TEST_F(RenderSessionManagerTest, DefaultConstruction) {
    RenderSessionManager mgr;
    EXPECT_EQ(mgr.activeSessionCount(), 0u);
    EXPECT_FALSE(mgr.isRenderLoopRunning());
}

TEST_F(RenderSessionManagerTest, CustomConfig) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_EQ(mgr.config().maxSessions, 4u);
    EXPECT_EQ(mgr.config().targetFps, 10u);
    EXPECT_EQ(mgr.config().idleTimeoutSeconds, 1u);
    EXPECT_EQ(mgr.config().defaultWidth, 64u);
    EXPECT_EQ(mgr.config().defaultHeight, 64u);
}

TEST_F(RenderSessionManagerTest, DefaultConfigValues) {
    RenderSessionManager mgr;

    EXPECT_EQ(mgr.config().maxSessions, 8u);
    EXPECT_EQ(mgr.config().targetFps, 30u);
    EXPECT_EQ(mgr.config().idleTimeoutSeconds, 300u);
    EXPECT_EQ(mgr.config().defaultWidth, 512u);
    EXPECT_EQ(mgr.config().defaultHeight, 512u);
}

// =============================================================================
// Session creation and destruction
// =============================================================================

TEST_F(RenderSessionManagerTest, CreateSession) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_TRUE(mgr.createSession("session-1"));
    EXPECT_TRUE(mgr.hasSession("session-1"));
    EXPECT_EQ(mgr.activeSessionCount(), 1u);
}

TEST_F(RenderSessionManagerTest, CreateSessionCustomSize) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_TRUE(mgr.createSession("session-1", 128, 128));
    EXPECT_TRUE(mgr.hasSession("session-1"));
}

TEST_F(RenderSessionManagerTest, CreateDuplicateSessionFails) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_TRUE(mgr.createSession("session-1"));
    EXPECT_FALSE(mgr.createSession("session-1"));
    EXPECT_EQ(mgr.activeSessionCount(), 1u);
}

TEST_F(RenderSessionManagerTest, DestroySession) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.createSession("session-1");
    EXPECT_TRUE(mgr.destroySession("session-1"));
    EXPECT_FALSE(mgr.hasSession("session-1"));
    EXPECT_EQ(mgr.activeSessionCount(), 0u);
}

TEST_F(RenderSessionManagerTest, DestroyNonexistentSessionFails) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_FALSE(mgr.destroySession("nonexistent"));
}

TEST_F(RenderSessionManagerTest, GetSession) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.createSession("session-1");
    EXPECT_NE(mgr.getSession("session-1"), nullptr);
    EXPECT_EQ(mgr.getSession("nonexistent"), nullptr);
}

TEST_F(RenderSessionManagerTest, HasSessionFalseForMissing) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_FALSE(mgr.hasSession("nonexistent"));
}

// =============================================================================
// Max session enforcement
// =============================================================================

TEST_F(RenderSessionManagerTest, MaxSessionLimit) {
    auto cfg = defaultConfig();
    cfg.maxSessions = 2;
    RenderSessionManager mgr(cfg);

    EXPECT_TRUE(mgr.createSession("s1"));
    EXPECT_TRUE(mgr.createSession("s2"));
    EXPECT_FALSE(mgr.createSession("s3"));
    EXPECT_EQ(mgr.activeSessionCount(), 2u);
}

TEST_F(RenderSessionManagerTest, MaxSessionZeroUnlimited) {
    auto cfg = defaultConfig();
    cfg.maxSessions = 0;
    RenderSessionManager mgr(cfg);

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(mgr.createSession("s" + std::to_string(i)));
    }
    EXPECT_EQ(mgr.activeSessionCount(), 10u);
}

TEST_F(RenderSessionManagerTest, CreateAfterDestroyWithinLimit) {
    auto cfg = defaultConfig();
    cfg.maxSessions = 2;
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    mgr.createSession("s2");
    EXPECT_FALSE(mgr.createSession("s3"));

    mgr.destroySession("s1");
    EXPECT_TRUE(mgr.createSession("s3"));
    EXPECT_EQ(mgr.activeSessionCount(), 2u);
}

// =============================================================================
// Session IDs
// =============================================================================

TEST_F(RenderSessionManagerTest, ActiveSessionIds) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.createSession("alpha");
    mgr.createSession("beta");
    mgr.createSession("gamma");

    auto ids = mgr.activeSessionIds();
    EXPECT_EQ(ids.size(), 3u);

    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids[0], "alpha");
    EXPECT_EQ(ids[1], "beta");
    EXPECT_EQ(ids[2], "gamma");
}

TEST_F(RenderSessionManagerTest, ActiveSessionIdsEmpty) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_TRUE(mgr.activeSessionIds().empty());
}

// =============================================================================
// Idle timeout cleanup
// =============================================================================

TEST_F(RenderSessionManagerTest, CleanupIdleSessions) {
    auto cfg = defaultConfig();
    cfg.idleTimeoutSeconds = 1;
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    mgr.createSession("s2");

    // Wait for sessions to become idle
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    size_t removed = mgr.cleanupIdleSessions();
    EXPECT_EQ(removed, 2u);
    EXPECT_EQ(mgr.activeSessionCount(), 0u);
}

TEST_F(RenderSessionManagerTest, TouchPreventsCleanup) {
    auto cfg = defaultConfig();
    cfg.idleTimeoutSeconds = 1;
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    mgr.createSession("s2");

    // Wait partway through the timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Touch one session
    mgr.touchSession("s1");

    // Wait for the rest of the timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    size_t removed = mgr.cleanupIdleSessions();
    EXPECT_EQ(removed, 1u);
    EXPECT_TRUE(mgr.hasSession("s1"));
    EXPECT_FALSE(mgr.hasSession("s2"));
}

TEST_F(RenderSessionManagerTest, CleanupNoTimeoutConfigured) {
    auto cfg = defaultConfig();
    cfg.idleTimeoutSeconds = 0;
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(mgr.cleanupIdleSessions(), 0u);
    EXPECT_EQ(mgr.activeSessionCount(), 1u);
}

TEST_F(RenderSessionManagerTest, CleanupEmptySessions) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_EQ(mgr.cleanupIdleSessions(), 0u);
}

// =============================================================================
// Render loop lifecycle
// =============================================================================

TEST_F(RenderSessionManagerTest, StartStopRenderLoop) {
    auto cfg = defaultConfig();
    cfg.targetFps = 10;
    RenderSessionManager mgr(cfg);

    EXPECT_FALSE(mgr.isRenderLoopRunning());

    mgr.startRenderLoop();
    EXPECT_TRUE(mgr.isRenderLoopRunning());

    mgr.stopRenderLoop();
    EXPECT_FALSE(mgr.isRenderLoopRunning());
}

TEST_F(RenderSessionManagerTest, StartRenderLoopIdempotent) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.startRenderLoop();
    mgr.startRenderLoop(); // Should not crash or create second thread
    EXPECT_TRUE(mgr.isRenderLoopRunning());

    mgr.stopRenderLoop();
}

TEST_F(RenderSessionManagerTest, StopRenderLoopIdempotent) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.stopRenderLoop(); // Should not crash when not running
    EXPECT_FALSE(mgr.isRenderLoopRunning());
}

TEST_F(RenderSessionManagerTest, DestructorStopsRenderLoop) {
    auto cfg = defaultConfig();
    {
        RenderSessionManager mgr(cfg);
        mgr.startRenderLoop();
        EXPECT_TRUE(mgr.isRenderLoopRunning());
        // Destructor should stop cleanly
    }
    // If we get here without hanging, the test passes
    SUCCEED();
}

// =============================================================================
// Frame callback
// =============================================================================

TEST_F(RenderSessionManagerTest, SetFrameReadyCallback) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    std::atomic<int> callCount{0};
    mgr.setFrameReadyCallback(
        [&](const std::string&, const std::vector<uint8_t>&,
            uint32_t, uint32_t) {
            callCount.fetch_add(1);
        });

    // Callback is set but no sessions or loop, so nothing should fire
    EXPECT_EQ(callCount.load(), 0);
}

// =============================================================================
// Thread safety
// =============================================================================

TEST_F(RenderSessionManagerTest, ConcurrentCreateDestroy) {
    auto cfg = defaultConfig();
    cfg.maxSessions = 0; // Unlimited
    RenderSessionManager mgr(cfg);

    auto createN = [&](int start, int count) {
        for (int i = start; i < start + count; ++i) {
            mgr.createSession("session-" + std::to_string(i));
        }
    };

    auto destroyN = [&](int start, int count) {
        for (int i = start; i < start + count; ++i) {
            mgr.destroySession("session-" + std::to_string(i));
        }
    };

    // Create from multiple threads
    std::thread t1(createN, 0, 50);
    std::thread t2(createN, 50, 50);
    t1.join();
    t2.join();

    EXPECT_EQ(mgr.activeSessionCount(), 100u);

    // Destroy from multiple threads
    std::thread t3(destroyN, 0, 50);
    std::thread t4(destroyN, 50, 50);
    t3.join();
    t4.join();

    EXPECT_EQ(mgr.activeSessionCount(), 0u);
}

TEST_F(RenderSessionManagerTest, ConcurrentTouchAndCleanup) {
    auto cfg = defaultConfig();
    cfg.idleTimeoutSeconds = 1;
    cfg.maxSessions = 0;
    RenderSessionManager mgr(cfg);

    for (int i = 0; i < 20; ++i) {
        mgr.createSession("s" + std::to_string(i));
    }

    // Touch sessions concurrently while cleanup runs
    std::atomic<bool> done{false};
    std::thread toucher([&]() {
        while (!done.load()) {
            for (int i = 0; i < 10; ++i) {
                mgr.touchSession("s" + std::to_string(i));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    mgr.cleanupIdleSessions();

    done.store(true);
    toucher.join();

    // Sessions 0-9 should still exist (touched), 10-19 should be cleaned
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(mgr.hasSession("s" + std::to_string(i)));
    }
    for (int i = 10; i < 20; ++i) {
        EXPECT_FALSE(mgr.hasSession("s" + std::to_string(i)));
    }
}

// =============================================================================
// Touch non-existent session (no-op)
// =============================================================================

TEST_F(RenderSessionManagerTest, TouchNonexistentSession) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    // Should not crash
    mgr.touchSession("nonexistent");
    EXPECT_EQ(mgr.activeSessionCount(), 0u);
}

// =============================================================================
// Multiple session operations
// =============================================================================

TEST_F(RenderSessionManagerTest, MultipleSessions) {
    auto cfg = defaultConfig();
    cfg.maxSessions = 4;
    RenderSessionManager mgr(cfg);

    EXPECT_TRUE(mgr.createSession("s1"));
    EXPECT_TRUE(mgr.createSession("s2"));
    EXPECT_TRUE(mgr.createSession("s3"));
    EXPECT_EQ(mgr.activeSessionCount(), 3u);

    EXPECT_TRUE(mgr.destroySession("s2"));
    EXPECT_EQ(mgr.activeSessionCount(), 2u);

    EXPECT_TRUE(mgr.hasSession("s1"));
    EXPECT_FALSE(mgr.hasSession("s2"));
    EXPECT_TRUE(mgr.hasSession("s3"));
}

TEST_F(RenderSessionManagerTest, GetSessionReturnsValidPointer) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    auto* session = mgr.getSession("s1");
    ASSERT_NE(session, nullptr);

    // The RenderSession should be a valid object we can interact with
    // (VolumeRenderer and MPRRenderer are initialized in off-screen mode)
}

// =============================================================================
// Adaptive quality controller integration
// =============================================================================

TEST_F(RenderSessionManagerTest, GetQualityControllerReturnsNullForMissing) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    EXPECT_EQ(mgr.getQualityController("nonexistent"), nullptr);
}

TEST_F(RenderSessionManagerTest, GetQualityControllerReturnsValidPointer) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    auto* qc = mgr.getQualityController("s1");
    ASSERT_NE(qc, nullptr);
    EXPECT_EQ(qc->state(), QualityState::Idle);
}

TEST_F(RenderSessionManagerTest, NotifyInteractionStartChangesState) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    mgr.notifyInteractionStart("s1");

    auto* qc = mgr.getQualityController("s1");
    ASSERT_NE(qc, nullptr);
    EXPECT_EQ(qc->state(), QualityState::Interacting);
}

TEST_F(RenderSessionManagerTest, NotifyInteractionEndChangesState) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    mgr.createSession("s1");
    mgr.notifyInteractionStart("s1");
    mgr.notifyInteractionEnd("s1");

    auto* qc = mgr.getQualityController("s1");
    ASSERT_NE(qc, nullptr);
    EXPECT_EQ(qc->state(), QualityState::PostInteraction);
}

TEST_F(RenderSessionManagerTest, NotifyInteractionOnMissingSessionIsNoop) {
    auto cfg = defaultConfig();
    RenderSessionManager mgr(cfg);

    // Should not crash
    mgr.notifyInteractionStart("nonexistent");
    mgr.notifyInteractionEnd("nonexistent");
}

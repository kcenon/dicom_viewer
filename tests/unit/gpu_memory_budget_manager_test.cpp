// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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

#include "services/render/gpu_memory_budget_manager.hpp"

#include <chrono>
#include <string>
#include <thread>

using namespace dicom_viewer::services;

// =============================================================================
// Default construction and configuration
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, DefaultConstruction) {
    GpuMemoryBudgetManager manager;
    const auto& cfg = manager.config();
    EXPECT_DOUBLE_EQ(cfg.rejectThreshold, 85.0);
    EXPECT_DOUBLE_EQ(cfg.degradeThreshold, 90.0);
    EXPECT_DOUBLE_EQ(cfg.terminateThreshold, 95.0);
    EXPECT_EQ(cfg.deviceIndex, 0u);
}

TEST(GpuMemoryBudgetManagerTest, CustomConfig) {
    GpuBudgetConfig config;
    config.rejectThreshold = 80.0;
    config.degradeThreshold = 88.0;
    config.terminateThreshold = 93.0;
    config.deviceIndex = 1;

    GpuMemoryBudgetManager manager(config);
    const auto& cfg = manager.config();
    EXPECT_DOUBLE_EQ(cfg.rejectThreshold, 80.0);
    EXPECT_DOUBLE_EQ(cfg.degradeThreshold, 88.0);
    EXPECT_DOUBLE_EQ(cfg.terminateThreshold, 93.0);
    EXPECT_EQ(cfg.deviceIndex, 1u);
}

// =============================================================================
// Session budget values
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, SessionBudgetCT) {
    GpuMemoryBudgetManager manager;
    uint64_t expected = 2ULL * 1024 * 1024 * 1024; // 2 GB
    EXPECT_EQ(manager.sessionBudget(GpuSessionType::CT), expected);
}

TEST(GpuMemoryBudgetManagerTest, SessionBudgetFlow4D) {
    GpuMemoryBudgetManager manager;
    uint64_t expected = 3584ULL * 1024 * 1024; // 3.5 GB
    EXPECT_EQ(manager.sessionBudget(GpuSessionType::Flow4D), expected);
}

TEST(GpuMemoryBudgetManagerTest, SessionBudgetGeneric) {
    GpuMemoryBudgetManager manager;
    uint64_t expected = 1ULL * 1024 * 1024 * 1024; // 1 GB
    EXPECT_EQ(manager.sessionBudget(GpuSessionType::Generic), expected);
}

TEST(GpuMemoryBudgetManagerTest, CustomSessionBudget) {
    GpuBudgetConfig config;
    config.ctSessionBudgetBytes = 4ULL * 1024 * 1024 * 1024;
    GpuMemoryBudgetManager manager(config);
    EXPECT_EQ(manager.sessionBudget(GpuSessionType::CT),
              4ULL * 1024 * 1024 * 1024);
}

// =============================================================================
// Graceful fallback when NVML is unavailable
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, FallbackAvailability) {
    GpuMemoryBudgetManager manager;
    // On macOS and CI (non-NVIDIA), NVML should be unavailable
#ifndef __linux__
    EXPECT_FALSE(manager.isAvailable());
#endif
    // On any platform: should not crash
}

TEST(GpuMemoryBudgetManagerTest, FallbackCanCreateSession) {
    GpuMemoryBudgetManager manager;
    if (!manager.isAvailable()) {
        // When NVML unavailable, all session types should be allowed
        EXPECT_TRUE(manager.canCreateSession(GpuSessionType::CT));
        EXPECT_TRUE(manager.canCreateSession(GpuSessionType::Flow4D));
        EXPECT_TRUE(manager.canCreateSession(GpuSessionType::Generic));
    }
}

TEST(GpuMemoryBudgetManagerTest, FallbackCheckAndEnforce) {
    GpuMemoryBudgetManager manager;
    if (!manager.isAvailable()) {
        EXPECT_EQ(manager.checkAndEnforce(), EnforcementAction::None);
    }
}

TEST(GpuMemoryBudgetManagerTest, FallbackMetrics) {
    GpuMemoryBudgetManager manager;
    if (!manager.isAvailable()) {
        auto m = manager.metrics();
        EXPECT_FALSE(m.available);
        EXPECT_EQ(m.totalBytes, 0u);
        EXPECT_EQ(m.usedBytes, 0u);
        EXPECT_EQ(m.freeBytes, 0u);
        EXPECT_DOUBLE_EQ(m.utilizationPercent, 0.0);
        EXPECT_TRUE(m.gpuName.empty());
    }
}

// =============================================================================
// Session registration and tracking
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, RegisterSession) {
    GpuMemoryBudgetManager manager;
    manager.registerSession("session-1", GpuSessionType::CT);

    auto m = manager.metrics();
    EXPECT_EQ(m.activeSessionCount, 1u);
}

TEST(GpuMemoryBudgetManagerTest, UnregisterSession) {
    GpuMemoryBudgetManager manager;
    manager.registerSession("session-1", GpuSessionType::CT);
    manager.registerSession("session-2", GpuSessionType::Flow4D);
    EXPECT_EQ(manager.metrics().activeSessionCount, 2u);

    manager.unregisterSession("session-1");
    EXPECT_EQ(manager.metrics().activeSessionCount, 1u);
}

TEST(GpuMemoryBudgetManagerTest, UnregisterNonexistentSession) {
    GpuMemoryBudgetManager manager;
    // Should not crash
    manager.unregisterSession("nonexistent");
    EXPECT_EQ(manager.metrics().activeSessionCount, 0u);
}

TEST(GpuMemoryBudgetManagerTest, MultipleSessions) {
    GpuMemoryBudgetManager manager;
    manager.registerSession("s1", GpuSessionType::CT);
    manager.registerSession("s2", GpuSessionType::Flow4D);
    manager.registerSession("s3", GpuSessionType::Generic);

    EXPECT_EQ(manager.metrics().activeSessionCount, 3u);

    manager.unregisterSession("s2");
    EXPECT_EQ(manager.metrics().activeSessionCount, 2u);

    manager.unregisterSession("s1");
    manager.unregisterSession("s3");
    EXPECT_EQ(manager.metrics().activeSessionCount, 0u);
}

// =============================================================================
// LRU session tracking
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, LruSessionIdEmpty) {
    GpuMemoryBudgetManager manager;
    EXPECT_TRUE(manager.lruSessionId().empty());
}

TEST(GpuMemoryBudgetManagerTest, LruSessionIdSingleSession) {
    GpuMemoryBudgetManager manager;
    manager.registerSession("only-one", GpuSessionType::CT);
    EXPECT_EQ(manager.lruSessionId(), "only-one");
}

TEST(GpuMemoryBudgetManagerTest, LruSessionIdOrderedByActivity) {
    GpuMemoryBudgetManager manager;

    manager.registerSession("old", GpuSessionType::CT);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    manager.registerSession("new", GpuSessionType::CT);

    // "old" was registered first, so it should be LRU
    EXPECT_EQ(manager.lruSessionId(), "old");
}

TEST(GpuMemoryBudgetManagerTest, TouchSessionUpdatesLru) {
    GpuMemoryBudgetManager manager;

    manager.registerSession("first", GpuSessionType::CT);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    manager.registerSession("second", GpuSessionType::CT);

    // Touch "first" to make it recent
    manager.touchSession("first");

    // Now "second" should be LRU (older last-active time)
    EXPECT_EQ(manager.lruSessionId(), "second");
}

// =============================================================================
// Terminate callback
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, TerminateCallbackNotCalledWithoutNvml) {
    GpuMemoryBudgetManager manager;
    bool called = false;
    manager.setTerminateCallback([&](const std::string&) {
        called = true;
    });

    if (!manager.isAvailable()) {
        manager.checkAndEnforce();
        EXPECT_FALSE(called);
    }
}

// =============================================================================
// EnforcementAction enum distinctness
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, EnforcementActionEnumValues) {
    EXPECT_NE(static_cast<int>(EnforcementAction::None),
              static_cast<int>(EnforcementAction::RejectNewSessions));
    EXPECT_NE(static_cast<int>(EnforcementAction::RejectNewSessions),
              static_cast<int>(EnforcementAction::DegradeQuality));
    EXPECT_NE(static_cast<int>(EnforcementAction::DegradeQuality),
              static_cast<int>(EnforcementAction::TerminateLRU));
}

// =============================================================================
// GpuSessionType enum distinctness
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, GpuSessionTypeEnumValues) {
    EXPECT_NE(static_cast<int>(GpuSessionType::CT),
              static_cast<int>(GpuSessionType::Flow4D));
    EXPECT_NE(static_cast<int>(GpuSessionType::Flow4D),
              static_cast<int>(GpuSessionType::Generic));
    EXPECT_NE(static_cast<int>(GpuSessionType::CT),
              static_cast<int>(GpuSessionType::Generic));
}

// =============================================================================
// GpuMemoryMetrics default values
// =============================================================================

TEST(GpuMemoryBudgetManagerTest, MetricsDefaultValues) {
    GpuMemoryMetrics m;
    EXPECT_FALSE(m.available);
    EXPECT_TRUE(m.gpuName.empty());
    EXPECT_EQ(m.totalBytes, 0u);
    EXPECT_EQ(m.usedBytes, 0u);
    EXPECT_EQ(m.freeBytes, 0u);
    EXPECT_DOUBLE_EQ(m.utilizationPercent, 0.0);
    EXPECT_EQ(m.activeSessionCount, 0u);
}

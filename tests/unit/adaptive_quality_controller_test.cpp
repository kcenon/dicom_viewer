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

#include "services/render/adaptive_quality_controller.hpp"

#include <chrono>
#include <thread>

using namespace dicom_viewer::services;

// =============================================================================
// Default construction and initial state
// =============================================================================

TEST(AdaptiveQualityControllerTest, DefaultConstruction) {
    AdaptiveQualityController controller;
    EXPECT_EQ(controller.state(), QualityState::Idle);
}

TEST(AdaptiveQualityControllerTest, DefaultConfigValues) {
    AdaptiveQualityController controller;
    const auto& cfg = controller.config();
    EXPECT_EQ(cfg.interactionQuality, 40);
    EXPECT_EQ(cfg.idleQuality, 85);
    EXPECT_EQ(cfg.interactionFps, 30u);
    EXPECT_EQ(cfg.idleFps, 1u);
    EXPECT_EQ(cfg.debounceMs, 100u);
}

TEST(AdaptiveQualityControllerTest, IdleStateQuality) {
    AdaptiveQualityController controller;
    EXPECT_EQ(controller.currentQuality(), 85);
    EXPECT_EQ(controller.currentTargetFps(), 1u);
}

TEST(AdaptiveQualityControllerTest, IdleDoesNotEmitFrame) {
    AdaptiveQualityController controller;
    EXPECT_FALSE(controller.shouldEmitFrame());
}

// =============================================================================
// Interaction start transitions
// =============================================================================

TEST(AdaptiveQualityControllerTest, InteractionStartTransitionsToInteracting) {
    AdaptiveQualityController controller;
    controller.onInteractionStart();
    EXPECT_EQ(controller.state(), QualityState::Interacting);
}

TEST(AdaptiveQualityControllerTest, InteractingStateQuality) {
    AdaptiveQualityController controller;
    controller.onInteractionStart();
    EXPECT_EQ(controller.currentQuality(), 40);
    EXPECT_EQ(controller.currentTargetFps(), 30u);
}

TEST(AdaptiveQualityControllerTest, InteractingEmitsFrames) {
    AdaptiveQualityController controller;
    controller.onInteractionStart();
    EXPECT_TRUE(controller.shouldEmitFrame());
    EXPECT_TRUE(controller.shouldEmitFrame());
}

// =============================================================================
// Interaction end transitions
// =============================================================================

TEST(AdaptiveQualityControllerTest, InteractionEndTransitionsToPostInteraction) {
    AdaptiveQualityController controller;
    controller.onInteractionStart();
    controller.onInteractionEnd();
    EXPECT_EQ(controller.state(), QualityState::PostInteraction);
}

TEST(AdaptiveQualityControllerTest, PostInteractionQualityIsHigh) {
    AdaptiveQualityController controller;
    controller.onInteractionStart();
    controller.onInteractionEnd();
    EXPECT_EQ(controller.currentQuality(), 85);
}

TEST(AdaptiveQualityControllerTest, InteractionEndFromIdleIsNoop) {
    AdaptiveQualityController controller;
    controller.onInteractionEnd();
    EXPECT_EQ(controller.state(), QualityState::Idle);
}

// =============================================================================
// Debounce and post-interaction frame emission
// =============================================================================

TEST(AdaptiveQualityControllerTest, PostInteractionDebounce) {
    AdaptiveQualityConfig config;
    config.debounceMs = 10; // Short debounce for testing
    AdaptiveQualityController controller(config);

    controller.onInteractionStart();
    controller.onInteractionEnd();

    // Immediately after end — within debounce, no frame
    EXPECT_FALSE(controller.shouldEmitFrame());

    // Wait for debounce to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    // First call after debounce — emit high-quality frame
    EXPECT_TRUE(controller.shouldEmitFrame());

    // Second call — transitions to Idle, no more frames
    EXPECT_FALSE(controller.shouldEmitFrame());
    EXPECT_EQ(controller.state(), QualityState::Idle);
}

// =============================================================================
// Re-interaction during post-interaction
// =============================================================================

TEST(AdaptiveQualityControllerTest, ReinteractionDuringPostInteraction) {
    AdaptiveQualityConfig config;
    config.debounceMs = 50;
    AdaptiveQualityController controller(config);

    controller.onInteractionStart();
    controller.onInteractionEnd();
    EXPECT_EQ(controller.state(), QualityState::PostInteraction);

    // Start interacting again before debounce expires
    controller.onInteractionStart();
    EXPECT_EQ(controller.state(), QualityState::Interacting);
    EXPECT_EQ(controller.currentQuality(), 40);
    EXPECT_TRUE(controller.shouldEmitFrame());
}

// =============================================================================
// Full cycle: Idle -> Interacting -> PostInteraction -> Idle
// =============================================================================

TEST(AdaptiveQualityControllerTest, FullCycle) {
    AdaptiveQualityConfig config;
    config.debounceMs = 5;
    AdaptiveQualityController controller(config);

    // Start idle
    EXPECT_EQ(controller.state(), QualityState::Idle);
    EXPECT_FALSE(controller.shouldEmitFrame());

    // Begin interaction
    controller.onInteractionStart();
    EXPECT_EQ(controller.state(), QualityState::Interacting);
    EXPECT_EQ(controller.currentQuality(), 40);
    EXPECT_TRUE(controller.shouldEmitFrame());

    // End interaction
    controller.onInteractionEnd();
    EXPECT_EQ(controller.state(), QualityState::PostInteraction);
    EXPECT_EQ(controller.currentQuality(), 85);

    // Wait for debounce
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Emit final high-quality frame
    EXPECT_TRUE(controller.shouldEmitFrame());

    // Transition to idle
    EXPECT_FALSE(controller.shouldEmitFrame());
    EXPECT_EQ(controller.state(), QualityState::Idle);
}

// =============================================================================
// Custom configuration
// =============================================================================

TEST(AdaptiveQualityControllerTest, CustomConfig) {
    AdaptiveQualityConfig config;
    config.interactionQuality = 30;
    config.idleQuality = 90;
    config.interactionFps = 60;
    config.idleFps = 5;
    config.debounceMs = 200;

    AdaptiveQualityController controller(config);

    EXPECT_EQ(controller.currentQuality(), 90);
    EXPECT_EQ(controller.currentTargetFps(), 5u);

    controller.onInteractionStart();
    EXPECT_EQ(controller.currentQuality(), 30);
    EXPECT_EQ(controller.currentTargetFps(), 60u);
}

TEST(AdaptiveQualityControllerTest, SetConfigUpdatesValues) {
    AdaptiveQualityController controller;
    EXPECT_EQ(controller.currentQuality(), 85);

    AdaptiveQualityConfig newConfig;
    newConfig.idleQuality = 95;
    controller.setConfig(newConfig);
    EXPECT_EQ(controller.currentQuality(), 95);
}

// =============================================================================
// Move semantics
// =============================================================================

TEST(AdaptiveQualityControllerTest, MoveConstruction) {
    AdaptiveQualityController a;
    a.onInteractionStart();
    EXPECT_EQ(a.state(), QualityState::Interacting);

    AdaptiveQualityController b(std::move(a));
    EXPECT_EQ(b.state(), QualityState::Interacting);
    EXPECT_EQ(b.currentQuality(), 40);
}

TEST(AdaptiveQualityControllerTest, MoveAssignment) {
    AdaptiveQualityController a;
    a.onInteractionStart();

    AdaptiveQualityController b;
    b = std::move(a);
    EXPECT_EQ(b.state(), QualityState::Interacting);
}

// =============================================================================
// QualityState enum distinctness
// =============================================================================

TEST(AdaptiveQualityControllerTest, QualityStateEnumValues) {
    EXPECT_NE(static_cast<int>(QualityState::Idle),
              static_cast<int>(QualityState::Interacting));
    EXPECT_NE(static_cast<int>(QualityState::Interacting),
              static_cast<int>(QualityState::PostInteraction));
    EXPECT_NE(static_cast<int>(QualityState::Idle),
              static_cast<int>(QualityState::PostInteraction));
}

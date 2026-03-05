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

#include "services/render/dirty_region_tracker.hpp"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <vector>

using namespace dicom_viewer::services;

namespace {

// Create a solid RGBA frame
std::vector<uint8_t> createSolidFrame(uint32_t w, uint32_t h,
                                       uint8_t r, uint8_t g, uint8_t b)
{
    std::vector<uint8_t> frame(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < frame.size(); i += 4) {
        frame[i + 0] = r;
        frame[i + 1] = g;
        frame[i + 2] = b;
        frame[i + 3] = 255;
    }
    return frame;
}

// Modify a rectangular region in a frame
void fillRect(std::vector<uint8_t>& frame, uint32_t frameW,
              uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh,
              uint8_t r, uint8_t g, uint8_t b)
{
    for (uint32_t y = ry; y < ry + rh; ++y) {
        for (uint32_t x = rx; x < rx + rw; ++x) {
            size_t idx = (static_cast<size_t>(y) * frameW + x) * 4;
            frame[idx + 0] = r;
            frame[idx + 1] = g;
            frame[idx + 2] = b;
        }
    }
}

} // anonymous namespace

// =============================================================================
// Default construction
// =============================================================================

TEST(DirtyRegionTrackerTest, DefaultConstruction) {
    DirtyRegionTracker tracker;
    EXPECT_EQ(tracker.config().mergeGap, 16u);
    EXPECT_DOUBLE_EQ(tracker.config().fullFrameThreshold, 0.60);
}

TEST(DirtyRegionTrackerTest, CustomConfig) {
    DirtyRegionConfig config;
    config.mergeGap = 32;
    config.fullFrameThreshold = 0.75;
    DirtyRegionTracker tracker(config);
    EXPECT_EQ(tracker.config().mergeGap, 32u);
    EXPECT_DOUBLE_EQ(tracker.config().fullFrameThreshold, 0.75);
}

TEST(DirtyRegionTrackerTest, SetConfig) {
    DirtyRegionTracker tracker;
    DirtyRegionConfig config;
    config.mergeGap = 8;
    tracker.setConfig(config);
    EXPECT_EQ(tracker.config().mergeGap, 8u);
}

// =============================================================================
// Identical frames — no dirty regions
// =============================================================================

TEST(DirtyRegionTrackerTest, IdenticalFramesNoDirtyRegions) {
    DirtyRegionTracker tracker;
    auto frame = createSolidFrame(64, 64, 128, 128, 128);
    auto result = tracker.detect(frame.data(), frame.data(), 64, 64);

    EXPECT_TRUE(result.regions.empty());
    EXPECT_DOUBLE_EQ(result.dirtyRatio, 0.0);
    EXPECT_FALSE(result.fullFrameRequired);
}

// =============================================================================
// Single dirty region detection
// =============================================================================

TEST(DirtyRegionTrackerTest, SingleDirtyRegion) {
    DirtyRegionTracker tracker;
    constexpr uint32_t W = 100, H = 100;

    auto prev = createSolidFrame(W, H, 0, 0, 0);
    auto curr = createSolidFrame(W, H, 0, 0, 0);

    // Change a 10x10 block at (20, 30)
    fillRect(curr, W, 20, 30, 10, 10, 255, 0, 0);

    auto result = tracker.detect(curr.data(), prev.data(), W, H);

    EXPECT_FALSE(result.fullFrameRequired);
    EXPECT_GT(result.dirtyRatio, 0.0);
    EXPECT_LE(result.dirtyRatio, 0.02); // 100 pixels out of 10000
    ASSERT_EQ(result.regions.size(), 1u);

    auto& r = result.regions[0];
    EXPECT_EQ(r.x, 20u);
    EXPECT_EQ(r.y, 30u);
    EXPECT_EQ(r.width, 10u);
    EXPECT_EQ(r.height, 10u);
}

// =============================================================================
// Multiple disjoint dirty regions
// =============================================================================

TEST(DirtyRegionTrackerTest, MultipleDisjointRegions) {
    DirtyRegionConfig config;
    config.mergeGap = 0; // Disable merging for this test
    DirtyRegionTracker tracker(config);
    constexpr uint32_t W = 200, H = 200;

    auto prev = createSolidFrame(W, H, 0, 0, 0);
    auto curr = createSolidFrame(W, H, 0, 0, 0);

    // Two distant dirty regions
    fillRect(curr, W, 10, 10, 5, 5, 255, 0, 0);     // Top-left
    fillRect(curr, W, 150, 150, 5, 5, 0, 255, 0);    // Bottom-right

    auto result = tracker.detect(curr.data(), prev.data(), W, H);

    EXPECT_FALSE(result.fullFrameRequired);
    EXPECT_EQ(result.regions.size(), 2u);
}

// =============================================================================
// Nearby regions merge
// =============================================================================

TEST(DirtyRegionTrackerTest, NearbyRegionsMerge) {
    DirtyRegionConfig config;
    config.mergeGap = 20;
    DirtyRegionTracker tracker(config);
    constexpr uint32_t W = 200, H = 200;

    auto prev = createSolidFrame(W, H, 0, 0, 0);
    auto curr = createSolidFrame(W, H, 0, 0, 0);

    // Two regions within merge gap (vertical gap = 10 < 20)
    fillRect(curr, W, 50, 10, 10, 10, 255, 0, 0);
    fillRect(curr, W, 50, 30, 10, 10, 0, 255, 0);

    auto result = tracker.detect(curr.data(), prev.data(), W, H);

    EXPECT_FALSE(result.fullFrameRequired);
    // Regions should be merged into one
    EXPECT_EQ(result.regions.size(), 1u);
}

// =============================================================================
// Full frame threshold
// =============================================================================

TEST(DirtyRegionTrackerTest, FullFrameThresholdTriggered) {
    DirtyRegionConfig config;
    config.fullFrameThreshold = 0.60;
    DirtyRegionTracker tracker(config);
    constexpr uint32_t W = 100, H = 100;

    auto prev = createSolidFrame(W, H, 0, 0, 0);
    auto curr = createSolidFrame(W, H, 0, 0, 0);

    // Change 70% of pixels (7000 out of 10000)
    fillRect(curr, W, 0, 0, 100, 70, 255, 255, 255);

    auto result = tracker.detect(curr.data(), prev.data(), W, H);

    EXPECT_TRUE(result.fullFrameRequired);
    EXPECT_GT(result.dirtyRatio, 0.60);
    EXPECT_EQ(result.regions.size(), 1u);
    // Full-frame region should cover entire frame
    EXPECT_EQ(result.regions[0].x, 0u);
    EXPECT_EQ(result.regions[0].y, 0u);
    EXPECT_EQ(result.regions[0].width, W);
    EXPECT_EQ(result.regions[0].height, H);
}

TEST(DirtyRegionTrackerTest, JustBelowThresholdNotFullFrame) {
    DirtyRegionConfig config;
    config.fullFrameThreshold = 0.60;
    DirtyRegionTracker tracker(config);
    constexpr uint32_t W = 100, H = 100;

    auto prev = createSolidFrame(W, H, 0, 0, 0);
    auto curr = createSolidFrame(W, H, 0, 0, 0);

    // Change 50% of pixels
    fillRect(curr, W, 0, 0, 100, 50, 255, 255, 255);

    auto result = tracker.detect(curr.data(), prev.data(), W, H);

    EXPECT_FALSE(result.fullFrameRequired);
    EXPECT_LT(result.dirtyRatio, 0.60);
}

// =============================================================================
// Extract and apply region
// =============================================================================

TEST(DirtyRegionTrackerTest, ExtractRegion) {
    constexpr uint32_t W = 10, H = 10;
    auto frame = createSolidFrame(W, H, 100, 100, 100);
    fillRect(frame, W, 2, 3, 4, 5, 200, 200, 200);

    DirtyRect rect;
    rect.x = 2;
    rect.y = 3;
    rect.width = 4;
    rect.height = 5;

    auto tile = DirtyRegionTracker::extractRegion(
        frame.data(), W, H, rect);

    ASSERT_EQ(tile.size(), static_cast<size_t>(4 * 5 * 4));

    // All pixels in the tile should be (200, 200, 200, 255)
    for (size_t i = 0; i < tile.size(); i += 4) {
        EXPECT_EQ(tile[i + 0], 200);
        EXPECT_EQ(tile[i + 1], 200);
        EXPECT_EQ(tile[i + 2], 200);
    }
}

TEST(DirtyRegionTrackerTest, ApplyRegion) {
    constexpr uint32_t W = 10, H = 10;
    auto base = createSolidFrame(W, H, 0, 0, 0);

    DirtyRect rect;
    rect.x = 3;
    rect.y = 4;
    rect.width = 2;
    rect.height = 2;

    // Create a tile with red pixels
    std::vector<uint8_t> tile(2 * 2 * 4);
    for (size_t i = 0; i < tile.size(); i += 4) {
        tile[i + 0] = 255;
        tile[i + 1] = 0;
        tile[i + 2] = 0;
        tile[i + 3] = 255;
    }

    DirtyRegionTracker::applyRegion(
        base.data(), W, H, tile.data(), rect);

    // Check that the tile was applied at (3,4)
    size_t idx = (4 * W + 3) * 4;
    EXPECT_EQ(base[idx + 0], 255);
    EXPECT_EQ(base[idx + 1], 0);
    EXPECT_EQ(base[idx + 2], 0);

    // Check that surrounding pixel is still black
    size_t idxOutside = (0 * W + 0) * 4;
    EXPECT_EQ(base[idxOutside + 0], 0);
    EXPECT_EQ(base[idxOutside + 1], 0);
    EXPECT_EQ(base[idxOutside + 2], 0);
}

TEST(DirtyRegionTrackerTest, ExtractAndApplyRoundtrip) {
    constexpr uint32_t W = 20, H = 20;
    auto original = createSolidFrame(W, H, 50, 50, 50);
    fillRect(original, W, 5, 5, 8, 8, 200, 100, 50);

    DirtyRect rect;
    rect.x = 5;
    rect.y = 5;
    rect.width = 8;
    rect.height = 8;

    auto tile = DirtyRegionTracker::extractRegion(
        original.data(), W, H, rect);

    // Apply to a blank frame
    auto target = createSolidFrame(W, H, 50, 50, 50);
    DirtyRegionTracker::applyRegion(
        target.data(), W, H, tile.data(), rect);

    // The target should now match the original in the rect area
    for (uint32_t y = 5; y < 13; ++y) {
        for (uint32_t x = 5; x < 13; ++x) {
            size_t idx = (static_cast<size_t>(y) * W + x) * 4;
            EXPECT_EQ(target[idx + 0], original[idx + 0]);
            EXPECT_EQ(target[idx + 1], original[idx + 1]);
            EXPECT_EQ(target[idx + 2], original[idx + 2]);
        }
    }
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(DirtyRegionTrackerTest, NullPointerReturnsEmpty) {
    DirtyRegionTracker tracker;
    auto frame = createSolidFrame(10, 10, 0, 0, 0);

    auto result = tracker.detect(nullptr, frame.data(), 10, 10);
    EXPECT_TRUE(result.regions.empty());

    result = tracker.detect(frame.data(), nullptr, 10, 10);
    EXPECT_TRUE(result.regions.empty());
}

TEST(DirtyRegionTrackerTest, ZeroDimensionsReturnsEmpty) {
    DirtyRegionTracker tracker;
    auto frame = createSolidFrame(10, 10, 0, 0, 0);

    auto result = tracker.detect(frame.data(), frame.data(), 0, 10);
    EXPECT_TRUE(result.regions.empty());

    result = tracker.detect(frame.data(), frame.data(), 10, 0);
    EXPECT_TRUE(result.regions.empty());
}

TEST(DirtyRegionTrackerTest, ExtractRegionNullReturnsEmpty) {
    DirtyRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = 5;
    rect.height = 5;

    auto tile = DirtyRegionTracker::extractRegion(nullptr, 10, 10, rect);
    EXPECT_TRUE(tile.empty());
}

TEST(DirtyRegionTrackerTest, ExtractRegionZeroSizeReturnsEmpty) {
    auto frame = createSolidFrame(10, 10, 0, 0, 0);
    DirtyRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = 0;
    rect.height = 5;

    auto tile = DirtyRegionTracker::extractRegion(
        frame.data(), 10, 10, rect);
    EXPECT_TRUE(tile.empty());
}

// =============================================================================
// Move semantics
// =============================================================================

TEST(DirtyRegionTrackerTest, MoveConstruction) {
    DirtyRegionConfig config;
    config.mergeGap = 32;
    DirtyRegionTracker a(config);

    DirtyRegionTracker b(std::move(a));
    EXPECT_EQ(b.config().mergeGap, 32u);
}

TEST(DirtyRegionTrackerTest, MoveAssignment) {
    DirtyRegionConfig config;
    config.mergeGap = 32;
    DirtyRegionTracker a(config);

    DirtyRegionTracker b;
    b = std::move(a);
    EXPECT_EQ(b.config().mergeGap, 32u);
}

// =============================================================================
// Dirty ratio accuracy
// =============================================================================

TEST(DirtyRegionTrackerTest, DirtyRatioAccuracy) {
    DirtyRegionTracker tracker;
    constexpr uint32_t W = 100, H = 100;

    auto prev = createSolidFrame(W, H, 0, 0, 0);
    auto curr = createSolidFrame(W, H, 0, 0, 0);

    // Change exactly 25% (a 50x50 block = 2500 pixels out of 10000)
    fillRect(curr, W, 0, 0, 50, 50, 255, 255, 255);

    auto result = tracker.detect(curr.data(), prev.data(), W, H);

    EXPECT_NEAR(result.dirtyRatio, 0.25, 0.01);
}

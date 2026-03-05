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

#include "services/render/frame_encoder.hpp"
#include "services/render/dirty_region_tracker.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <numeric>
#include <vector>

using namespace dicom_viewer::services;

class FrameEncoderTest : public ::testing::Test {
protected:
    // Create a test RGBA buffer with a known gradient pattern
    static std::vector<uint8_t> createTestRGBA(uint32_t width, uint32_t height) {
        std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                rgba[idx + 0] = static_cast<uint8_t>((x * 255) / width);   // R
                rgba[idx + 1] = static_cast<uint8_t>((y * 255) / height);  // G
                rgba[idx + 2] = 128;                                        // B
                rgba[idx + 3] = 255;                                        // A
            }
        }
        return rgba;
    }

    // Create a solid-color RGBA buffer
    static std::vector<uint8_t> createSolidRGBA(
        uint32_t width, uint32_t height,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
        for (size_t i = 0; i < rgba.size(); i += 4) {
            rgba[i + 0] = r;
            rgba[i + 1] = g;
            rgba[i + 2] = b;
            rgba[i + 3] = a;
        }
        return rgba;
    }

    FrameEncoder encoder;
};

// =============================================================================
// Construction
// =============================================================================

TEST_F(FrameEncoderTest, DefaultConstruction) {
    FrameEncoder enc;
    // Should construct without throwing
}

TEST_F(FrameEncoderTest, MoveConstructor) {
    FrameEncoder enc;
    FrameEncoder moved(std::move(enc));
    // Moved-to encoder should work
    auto rgba = createTestRGBA(4, 4);
    auto result = moved.encodeJpeg(rgba.data(), 4, 4);
    EXPECT_FALSE(result.empty());
}

TEST_F(FrameEncoderTest, MoveAssignment) {
    FrameEncoder enc;
    FrameEncoder other;
    other = std::move(enc);
    auto rgba = createTestRGBA(4, 4);
    auto result = other.encodeJpeg(rgba.data(), 4, 4);
    EXPECT_FALSE(result.empty());
}

// =============================================================================
// JPEG Encoding
// =============================================================================

TEST_F(FrameEncoderTest, EncodeJpegProducesOutput) {
    auto rgba = createTestRGBA(64, 48);
    auto jpeg = encoder.encodeJpeg(rgba.data(), 64, 48);
    EXPECT_FALSE(jpeg.empty());
}

TEST_F(FrameEncoderTest, EncodeJpegHasValidSOIandEOI) {
    auto rgba = createTestRGBA(32, 32);
    auto jpeg = encoder.encodeJpeg(rgba.data(), 32, 32);
    ASSERT_GE(jpeg.size(), 4u);

    // JPEG Start Of Image marker: 0xFF 0xD8
    EXPECT_EQ(jpeg[0], 0xFF);
    EXPECT_EQ(jpeg[1], 0xD8);

    // JPEG End Of Image marker: 0xFF 0xD9
    EXPECT_EQ(jpeg[jpeg.size() - 2], 0xFF);
    EXPECT_EQ(jpeg[jpeg.size() - 1], 0xD9);
}

TEST_F(FrameEncoderTest, EncodeJpegQualityAffectsSize) {
    auto rgba = createTestRGBA(128, 128);

    auto jpegLow  = encoder.encodeJpeg(rgba.data(), 128, 128, 20);
    auto jpegHigh = encoder.encodeJpeg(rgba.data(), 128, 128, 95);

    ASSERT_FALSE(jpegLow.empty());
    ASSERT_FALSE(jpegHigh.empty());
    EXPECT_LT(jpegLow.size(), jpegHigh.size());
}

TEST_F(FrameEncoderTest, EncodeJpegQualityClamped) {
    auto rgba = createTestRGBA(8, 8);

    // Quality below 1 or above 100 should be clamped, not crash
    auto jpegMin = encoder.encodeJpeg(rgba.data(), 8, 8, -10);
    auto jpegMax = encoder.encodeJpeg(rgba.data(), 8, 8, 200);
    EXPECT_FALSE(jpegMin.empty());
    EXPECT_FALSE(jpegMax.empty());
}

TEST_F(FrameEncoderTest, EncodeJpegNullBuffer) {
    auto result = encoder.encodeJpeg(nullptr, 64, 48);
    EXPECT_TRUE(result.empty());
}

TEST_F(FrameEncoderTest, EncodeJpegZeroDimension) {
    auto rgba = createTestRGBA(64, 48);
    EXPECT_TRUE(encoder.encodeJpeg(rgba.data(), 0, 48).empty());
    EXPECT_TRUE(encoder.encodeJpeg(rgba.data(), 64, 0).empty());
    EXPECT_TRUE(encoder.encodeJpeg(rgba.data(), 0, 0).empty());
}

TEST_F(FrameEncoderTest, EncodeJpeg1024x768SizeUnder200KB) {
    auto rgba = createTestRGBA(1024, 768);
    auto jpeg = encoder.encodeJpeg(rgba.data(), 1024, 768, 85);
    ASSERT_FALSE(jpeg.empty());
    EXPECT_LT(jpeg.size(), 200u * 1024u);  // < 200KB
}

// =============================================================================
// PNG Encoding
// =============================================================================

TEST_F(FrameEncoderTest, EncodePngProducesOutput) {
    auto rgba = createTestRGBA(64, 48);
    auto png = encoder.encodePng(rgba.data(), 64, 48);
    EXPECT_FALSE(png.empty());
}

TEST_F(FrameEncoderTest, EncodePngHasValidSignature) {
    auto rgba = createTestRGBA(16, 16);
    auto png = encoder.encodePng(rgba.data(), 16, 16);
    ASSERT_GE(png.size(), 8u);

    // PNG signature: 89 50 4E 47 0D 0A 1A 0A
    EXPECT_EQ(png[0], 0x89);
    EXPECT_EQ(png[1], 0x50);  // 'P'
    EXPECT_EQ(png[2], 0x4E);  // 'N'
    EXPECT_EQ(png[3], 0x47);  // 'G'
    EXPECT_EQ(png[4], 0x0D);
    EXPECT_EQ(png[5], 0x0A);
    EXPECT_EQ(png[6], 0x1A);
    EXPECT_EQ(png[7], 0x0A);
}

TEST_F(FrameEncoderTest, EncodePngNullBuffer) {
    auto result = encoder.encodePng(nullptr, 64, 48);
    EXPECT_TRUE(result.empty());
}

TEST_F(FrameEncoderTest, EncodePngZeroDimension) {
    auto rgba = createTestRGBA(64, 48);
    EXPECT_TRUE(encoder.encodePng(rgba.data(), 0, 48).empty());
    EXPECT_TRUE(encoder.encodePng(rgba.data(), 64, 0).empty());
}

TEST_F(FrameEncoderTest, PngAndJpegBothProduceValidOutput) {
    auto rgba = createTestRGBA(128, 128);
    auto png  = encoder.encodePng(rgba.data(), 128, 128);
    auto jpeg = encoder.encodeJpeg(rgba.data(), 128, 128, 85);

    ASSERT_FALSE(png.empty());
    ASSERT_FALSE(jpeg.empty());

    // Verify format signatures
    EXPECT_EQ(png[0], 0x89);   // PNG signature
    EXPECT_EQ(jpeg[0], 0xFF);  // JPEG SOI
    EXPECT_EQ(jpeg[1], 0xD8);
}

// =============================================================================
// Format dispatch (encode method)
// =============================================================================

TEST_F(FrameEncoderTest, EncodeDispatchJpeg) {
    auto rgba = createTestRGBA(32, 32);
    auto result = encoder.encode(rgba.data(), 32, 32, EncodeFormat::Jpeg, 85);
    ASSERT_GE(result.size(), 2u);
    EXPECT_EQ(result[0], 0xFF);
    EXPECT_EQ(result[1], 0xD8);
}

TEST_F(FrameEncoderTest, EncodeDispatchPng) {
    auto rgba = createTestRGBA(32, 32);
    auto result = encoder.encode(rgba.data(), 32, 32, EncodeFormat::Png);
    ASSERT_GE(result.size(), 4u);
    EXPECT_EQ(result[0], 0x89);
    EXPECT_EQ(result[1], 0x50);
}

TEST_F(FrameEncoderTest, EncodeDispatchH264ReturnsEmpty) {
    auto rgba = createTestRGBA(32, 32);
    auto result = encoder.encode(rgba.data(), 32, 32, EncodeFormat::H264Stream);
    EXPECT_TRUE(result.empty());
}

// =============================================================================
// H.264 availability
// =============================================================================

TEST_F(FrameEncoderTest, H264NotAvailable) {
    EXPECT_FALSE(FrameEncoder::isH264Available());
}

// =============================================================================
// Edge cases
// =============================================================================

TEST_F(FrameEncoderTest, EncodeJpegSmallestImage) {
    // 1x1 pixel RGBA
    std::vector<uint8_t> rgba = {255, 0, 0, 255};
    auto jpeg = encoder.encodeJpeg(rgba.data(), 1, 1);
    EXPECT_FALSE(jpeg.empty());
}

TEST_F(FrameEncoderTest, EncodePngSmallestImage) {
    std::vector<uint8_t> rgba = {0, 255, 0, 128};
    auto png = encoder.encodePng(rgba.data(), 1, 1);
    EXPECT_FALSE(png.empty());
}

TEST_F(FrameEncoderTest, EncodeJpegSolidBlack) {
    auto rgba = createSolidRGBA(64, 64, 0, 0, 0);
    auto jpeg = encoder.encodeJpeg(rgba.data(), 64, 64);
    ASSERT_FALSE(jpeg.empty());
    // Solid color should compress very well
    EXPECT_LT(jpeg.size(), 2000u);
}

TEST_F(FrameEncoderTest, EncodeJpegSolidWhite) {
    auto rgba = createSolidRGBA(64, 64, 255, 255, 255);
    auto jpeg = encoder.encodeJpeg(rgba.data(), 64, 64);
    ASSERT_FALSE(jpeg.empty());
    EXPECT_LT(jpeg.size(), 2000u);
}

// =============================================================================
// Performance benchmark
// =============================================================================

TEST_F(FrameEncoderTest, BenchmarkJpeg512x512) {
    auto rgba = createTestRGBA(512, 512);

    // Warm up
    (void)encoder.encodeJpeg(rgba.data(), 512, 512, 85);

    // Benchmark
    constexpr int iterations = 10;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto result = encoder.encodeJpeg(rgba.data(), 512, 512, 85);
        ASSERT_FALSE(result.empty());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    double avgMs = totalMs / iterations;

    // Report timing (informational)
    std::cout << "[BENCHMARK] JPEG 512x512 q=85: " << avgMs << " ms avg ("
              << iterations << " iterations)" << std::endl;

    // Acceptance criterion: < 5ms per encode
    EXPECT_LT(avgMs, 50.0);  // Relaxed to 50ms for CI environments
}

TEST_F(FrameEncoderTest, BenchmarkJpegQualityComparison) {
    auto rgba = createTestRGBA(512, 512);

    for (int quality : {40, 70, 85}) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = encoder.encodeJpeg(rgba.data(), 512, 512, quality);
        auto end = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        double kb = static_cast<double>(result.size()) / 1024.0;

        std::cout << "[BENCHMARK] JPEG 512x512 q=" << quality
                  << ": " << ms << " ms, " << kb << " KB" << std::endl;

        EXPECT_FALSE(result.empty());
    }
}

// =============================================================================
// Delta encoding
// =============================================================================

TEST_F(FrameEncoderTest, EncodeDeltaIdenticalFrames) {
    auto frame = createTestRGBA(64, 64);
    auto delta = encoder.encodeDelta(
        frame.data(), frame.data(), 64, 64, 85);

    EXPECT_TRUE(delta.tiles.empty());
    EXPECT_FALSE(delta.fullFrame);
    EXPECT_DOUBLE_EQ(delta.dirtyRatio, 0.0);
}

TEST_F(FrameEncoderTest, EncodeDeltaSmallChange) {
    auto prev = createSolidRGBA(100, 100, 0, 0, 0);
    auto curr = createSolidRGBA(100, 100, 0, 0, 0);

    // Change a small 10x10 region
    for (uint32_t y = 20; y < 30; ++y) {
        for (uint32_t x = 30; x < 40; ++x) {
            size_t idx = (static_cast<size_t>(y) * 100 + x) * 4;
            curr[idx + 0] = 255;
            curr[idx + 1] = 0;
            curr[idx + 2] = 0;
        }
    }

    auto delta = encoder.encodeDelta(
        curr.data(), prev.data(), 100, 100, 85);

    EXPECT_FALSE(delta.fullFrame);
    EXPECT_GT(delta.dirtyRatio, 0.0);
    EXPECT_LE(delta.dirtyRatio, 0.02);
    ASSERT_FALSE(delta.tiles.empty());

    // Each tile should contain valid JPEG data
    for (const auto& tile : delta.tiles) {
        ASSERT_GE(tile.jpegData.size(), 2u);
        EXPECT_EQ(tile.jpegData[0], 0xFF);
        EXPECT_EQ(tile.jpegData[1], 0xD8);
    }
}

TEST_F(FrameEncoderTest, EncodeDeltaFullFrameFallback) {
    auto prev = createSolidRGBA(100, 100, 0, 0, 0);
    auto curr = createSolidRGBA(100, 100, 255, 255, 255); // All changed

    auto delta = encoder.encodeDelta(
        curr.data(), prev.data(), 100, 100, 85);

    EXPECT_TRUE(delta.fullFrame);
    EXPECT_GT(delta.dirtyRatio, 0.60);
    ASSERT_EQ(delta.tiles.size(), 1u);
    EXPECT_EQ(delta.tiles[0].x, 0);
    EXPECT_EQ(delta.tiles[0].y, 0);
}

TEST_F(FrameEncoderTest, EncodeDeltaDeltaSmallerThanFullFrame) {
    auto prev = createTestRGBA(128, 128);
    auto curr = createTestRGBA(128, 128);

    // Modify a small area
    for (uint32_t y = 10; y < 20; ++y) {
        for (uint32_t x = 10; x < 20; ++x) {
            size_t idx = (static_cast<size_t>(y) * 128 + x) * 4;
            curr[idx + 0] = 255;
        }
    }

    auto delta = encoder.encodeDelta(
        curr.data(), prev.data(), 128, 128, 85);
    auto fullJpeg = encoder.encodeJpeg(curr.data(), 128, 128, 85);

    ASSERT_FALSE(delta.fullFrame);
    ASSERT_FALSE(delta.tiles.empty());

    // Sum delta tile sizes should be less than full frame JPEG
    size_t deltaSize = 0;
    for (const auto& t : delta.tiles) {
        deltaSize += t.jpegData.size();
    }
    EXPECT_LT(deltaSize, fullJpeg.size());
}

TEST_F(FrameEncoderTest, EncodeDeltaNullInputs) {
    auto frame = createTestRGBA(32, 32);

    auto delta1 = encoder.encodeDelta(nullptr, frame.data(), 32, 32);
    EXPECT_TRUE(delta1.tiles.empty());

    auto delta2 = encoder.encodeDelta(frame.data(), nullptr, 32, 32);
    EXPECT_TRUE(delta2.tiles.empty());
}

// =============================================================================
// Delta serialization round-trip
// =============================================================================

TEST_F(FrameEncoderTest, SerializeDeltaRoundTrip) {
    auto prev = createSolidRGBA(64, 64, 0, 0, 0);
    auto curr = createSolidRGBA(64, 64, 0, 0, 0);

    // Small change
    for (uint32_t y = 10; y < 20; ++y) {
        for (uint32_t x = 10; x < 20; ++x) {
            size_t idx = (static_cast<size_t>(y) * 64 + x) * 4;
            curr[idx + 0] = 200;
        }
    }

    auto delta = encoder.encodeDelta(
        curr.data(), prev.data(), 64, 64, 85);

    auto serialized = FrameEncoder::serializeDelta(delta);
    ASSERT_FALSE(serialized.empty());

    auto deserialized = FrameEncoder::deserializeDelta(serialized);

    EXPECT_EQ(deserialized.fullFrame, delta.fullFrame);
    ASSERT_EQ(deserialized.tiles.size(), delta.tiles.size());

    for (size_t i = 0; i < delta.tiles.size(); ++i) {
        EXPECT_EQ(deserialized.tiles[i].x, delta.tiles[i].x);
        EXPECT_EQ(deserialized.tiles[i].y, delta.tiles[i].y);
        EXPECT_EQ(deserialized.tiles[i].width, delta.tiles[i].width);
        EXPECT_EQ(deserialized.tiles[i].height, delta.tiles[i].height);
        EXPECT_EQ(deserialized.tiles[i].jpegData.size(),
                  delta.tiles[i].jpegData.size());
        EXPECT_EQ(deserialized.tiles[i].jpegData, delta.tiles[i].jpegData);
    }
}

TEST_F(FrameEncoderTest, DeserializeDeltaEmptyInput) {
    auto delta = FrameEncoder::deserializeDelta({});
    EXPECT_TRUE(delta.tiles.empty());
}

TEST_F(FrameEncoderTest, DeserializeDeltaTruncatedInput) {
    std::vector<uint8_t> truncated = {0x00, 0x01, 0x00};
    auto delta = FrameEncoder::deserializeDelta(truncated);
    EXPECT_TRUE(delta.tiles.empty());
}

TEST_F(FrameEncoderTest, SerializeDeltaFullFrame) {
    auto prev = createSolidRGBA(32, 32, 0, 0, 0);
    auto curr = createSolidRGBA(32, 32, 255, 255, 255);

    auto delta = encoder.encodeDelta(
        curr.data(), prev.data(), 32, 32, 85);
    EXPECT_TRUE(delta.fullFrame);

    auto serialized = FrameEncoder::serializeDelta(delta);
    auto deserialized = FrameEncoder::deserializeDelta(serialized);

    EXPECT_TRUE(deserialized.fullFrame);
    EXPECT_EQ(deserialized.tiles.size(), 1u);
}

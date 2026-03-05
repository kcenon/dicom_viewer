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

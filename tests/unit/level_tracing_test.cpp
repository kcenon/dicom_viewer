#include "services/segmentation/level_tracing_tool.hpp"

#include <cmath>

#include <gtest/gtest.h>
#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;
using FloatSlice2D = LevelTracingTool::FloatSlice2D;
using BinarySlice2D = LevelTracingTool::BinarySlice2D;
using IndexPoint = LevelTracingTool::IndexPoint;

namespace {

/// Create a 2D float image with uniform value
FloatSlice2D::Pointer createSlice(int width, int height,
                                   float value = 0.0f) {
    auto image = FloatSlice2D::New();
    FloatSlice2D::SizeType size = {{
        static_cast<unsigned long>(width),
        static_cast<unsigned long>(height)
    }};
    FloatSlice2D::IndexType start = {{0, 0}};
    FloatSlice2D::RegionType region{start, size};
    image->SetRegions(region);

    FloatSlice2D::SpacingType spacing;
    spacing.Fill(1.0);
    image->SetSpacing(spacing);

    FloatSlice2D::PointType origin;
    origin.Fill(0.0);
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(value);
    return image;
}

/// Draw a filled rectangle on the slice
void drawRect(FloatSlice2D::Pointer image,
              int x0, int y0, int x1, int y1,
              float intensity) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            FloatSlice2D::IndexType idx = {{x, y}};
            image->SetPixel(idx, intensity);
        }
    }
}

/// Draw a filled circle on the slice
void drawCircle(FloatSlice2D::Pointer image,
                int cx, int cy, int radius,
                float intensity) {
    auto size = image->GetLargestPossibleRegion().GetSize();
    int w = static_cast<int>(size[0]);
    int h = static_cast<int>(size[1]);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius) {
                FloatSlice2D::IndexType idx = {{x, y}};
                image->SetPixel(idx, intensity);
            }
        }
    }
}

/// Count foreground pixels in a binary slice
size_t countForeground(BinarySlice2D::Pointer mask) {
    size_t count = 0;
    itk::ImageRegionConstIterator<BinarySlice2D> it(
        mask, mask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0) ++count;
    }
    return count;
}

}  // anonymous namespace

// =============================================================================
// Input validation tests
// =============================================================================

TEST(LevelTracingToolTest, NullSliceReturnsError) {
    IndexPoint seed = {5, 5};
    auto result = LevelTracingTool::traceContour(nullptr, seed);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(LevelTracingToolTest, OutOfBoundsSeedReturnsError) {
    auto slice = createSlice(20, 20, 100.0f);
    IndexPoint seed = {-5, 10};
    auto result = LevelTracingTool::traceContour(slice.GetPointer(), seed);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(LevelTracingToolTest, OutOfBoundsSeedHighReturnsError) {
    auto slice = createSlice(20, 20, 100.0f);
    IndexPoint seed = {25, 10};
    auto result = LevelTracingTool::traceContour(slice.GetPointer(), seed);
    EXPECT_FALSE(result.has_value());
}

TEST(LevelTracingToolTest, UniformImageReturnsError) {
    auto slice = createSlice(20, 20, 100.0f);  // uniform
    IndexPoint seed = {10, 10};
    auto result = LevelTracingTool::traceContour(slice.GetPointer(), seed);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(LevelTracingToolTest, TraceAndFillNullSliceReturnsError) {
    IndexPoint seed = {5, 5};
    auto result = LevelTracingTool::traceAndFill(nullptr, seed);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(LevelTracingToolTest, ContourToMaskNullReferenceReturnsError) {
    std::vector<IndexPoint> contour = {{0, 0}, {10, 0}, {10, 10}};
    auto result = LevelTracingTool::contourToMask(contour, nullptr);
    EXPECT_FALSE(result.has_value());
}

TEST(LevelTracingToolTest, ContourToMaskTooFewPointsReturnsError) {
    auto ref = createSlice(20, 20);
    std::vector<IndexPoint> contour = {{5, 5}, {10, 5}};
    auto result = LevelTracingTool::contourToMask(contour, ref.GetPointer());
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Rectangle tracing tests
// =============================================================================

TEST(LevelTracingToolTest, TraceContourOnBrightRectangle) {
    // Background = 0, bright rectangle = 200
    auto slice = createSlice(30, 30, 0.0f);
    drawRect(slice, 5, 5, 20, 20, 200.0f);

    // Seed inside the rectangle
    IndexPoint seed = {12, 12};
    LevelTracingTool::Config config;
    config.tolerancePct = 5.0;

    auto result = LevelTracingTool::traceContour(
        slice.GetPointer(), seed, config);
    ASSERT_TRUE(result.has_value())
        << "Error: " << result.error().message;

    auto& contour = result.value();
    // Rectangle boundary should have multiple points
    EXPECT_GT(contour.size(), 10u);

    // All contour points should be within or on the rectangle boundary
    for (const auto& pt : contour) {
        EXPECT_GE(pt[0], 4) << "Contour point too far left";
        EXPECT_LE(pt[0], 21) << "Contour point too far right";
        EXPECT_GE(pt[1], 4) << "Contour point too far up";
        EXPECT_LE(pt[1], 21) << "Contour point too far down";
    }
}

TEST(LevelTracingToolTest, TraceAndFillBrightRectangle) {
    auto slice = createSlice(30, 30, 0.0f);
    drawRect(slice, 5, 5, 20, 20, 200.0f);

    IndexPoint seed = {12, 12};
    auto result = LevelTracingTool::traceAndFill(slice.GetPointer(), seed);
    ASSERT_TRUE(result.has_value())
        << "Error: " << result.error().message;

    size_t filledCount = countForeground(*result);
    // Expected area: 16 * 16 = 256 pixels (5..20 inclusive = 16 wide)
    size_t expectedArea = 16u * 16u;
    EXPECT_NEAR(static_cast<double>(filledCount),
                static_cast<double>(expectedArea), expectedArea * 0.1);
}

// =============================================================================
// Circle tracing tests
// =============================================================================

TEST(LevelTracingToolTest, TraceContourOnCircle) {
    auto slice = createSlice(40, 40, 0.0f);
    drawCircle(slice, 20, 20, 8, 150.0f);

    IndexPoint seed = {20, 20};
    auto result = LevelTracingTool::traceContour(slice.GetPointer(), seed);
    ASSERT_TRUE(result.has_value());

    auto& contour = result.value();
    EXPECT_GT(contour.size(), 15u);

    // Contour points should be near the circle boundary
    for (const auto& pt : contour) {
        double dist = std::sqrt((pt[0] - 20) * (pt[0] - 20) +
                                (pt[1] - 20) * (pt[1] - 20));
        EXPECT_LE(dist, 10.0) << "Contour point too far from center";
    }
}

TEST(LevelTracingToolTest, TraceAndFillCircle) {
    auto slice = createSlice(40, 40, 0.0f);
    drawCircle(slice, 20, 20, 8, 150.0f);

    IndexPoint seed = {20, 20};
    auto result = LevelTracingTool::traceAndFill(slice.GetPointer(), seed);
    ASSERT_TRUE(result.has_value());

    size_t filledCount = countForeground(*result);
    // Approximate area of circle with r=8: π*64 ≈ 201
    double expectedArea = M_PI * 8 * 8;
    EXPECT_NEAR(static_cast<double>(filledCount), expectedArea,
                expectedArea * 0.15);
}

// =============================================================================
// Tolerance band tests
// =============================================================================

TEST(LevelTracingToolTest, NarrowToleranceTracesLess) {
    auto slice = createSlice(30, 30, 50.0f);
    // Gradient region: intensity varies from 100 to 200
    for (int y = 5; y <= 25; ++y) {
        for (int x = 5; x <= 25; ++x) {
            float intensity = 100.0f + (x - 5) * 5.0f;
            FloatSlice2D::IndexType idx = {{x, y}};
            slice->SetPixel(idx, intensity);
        }
    }

    IndexPoint seed = {15, 15};  // ~150

    // Wide tolerance should capture more
    LevelTracingTool::Config wideConfig;
    wideConfig.tolerancePct = 30.0;
    auto wideResult = LevelTracingTool::traceAndFill(
        slice.GetPointer(), seed, wideConfig);
    ASSERT_TRUE(wideResult.has_value());
    size_t wideCount = countForeground(*wideResult);

    // Narrow tolerance should capture less
    LevelTracingTool::Config narrowConfig;
    narrowConfig.tolerancePct = 5.0;
    auto narrowResult = LevelTracingTool::traceAndFill(
        slice.GetPointer(), seed, narrowConfig);
    ASSERT_TRUE(narrowResult.has_value());
    size_t narrowCount = countForeground(*narrowResult);

    EXPECT_GT(wideCount, narrowCount)
        << "Wider tolerance should capture more pixels";
}

TEST(LevelTracingToolTest, ToleranceBandRespectsSeedIntensity) {
    auto slice = createSlice(30, 30, 0.0f);
    // Two separate bright regions with different intensities
    drawRect(slice, 2, 2, 10, 10, 100.0f);
    drawRect(slice, 15, 15, 25, 25, 200.0f);

    // Seed in the first region (intensity=100)
    IndexPoint seed = {6, 6};
    LevelTracingTool::Config config;
    config.tolerancePct = 10.0;  // tolerance = 0.1 * 200 = 20

    auto result = LevelTracingTool::traceAndFill(
        slice.GetPointer(), seed, config);
    ASSERT_TRUE(result.has_value());

    // Should only fill the first rectangle, not the second
    // Check a pixel in the second rectangle
    BinarySlice2D::IndexType checkIdx = {{20, 20}};
    EXPECT_EQ(result.value()->GetPixel(checkIdx), 0)
        << "Second rectangle should not be filled";

    // Check a pixel in the first rectangle
    BinarySlice2D::IndexType firstIdx = {{6, 6}};
    EXPECT_EQ(result.value()->GetPixel(firstIdx), 1)
        << "Seed region should be filled";
}

// =============================================================================
// contourToMask tests
// =============================================================================

TEST(LevelTracingToolTest, ContourToMaskSimpleTriangle) {
    auto ref = createSlice(20, 20);
    std::vector<IndexPoint> contour = {
        {5, 5}, {15, 5}, {10, 15}
    };

    auto result = LevelTracingTool::contourToMask(contour, ref.GetPointer());
    ASSERT_TRUE(result.has_value());

    size_t filledCount = countForeground(*result);
    // Triangle area ≈ 0.5 * 10 * 10 = 50
    EXPECT_GT(filledCount, 20u) << "Triangle should have significant area";
    EXPECT_LT(filledCount, 120u) << "Filled area should not exceed bounding box";
}

TEST(LevelTracingToolTest, ContourToMaskSquare) {
    auto ref = createSlice(30, 30);
    std::vector<IndexPoint> contour;

    // Build a square contour: bottom edge, right edge, top edge, left edge
    for (int x = 5; x <= 15; ++x) contour.push_back({x, 5});
    for (int y = 6; y <= 15; ++y) contour.push_back({15, y});
    for (int x = 14; x >= 5; --x) contour.push_back({x, 15});
    for (int y = 14; y >= 6; --y) contour.push_back({5, y});

    auto result = LevelTracingTool::contourToMask(contour, ref.GetPointer());
    ASSERT_TRUE(result.has_value());

    size_t filledCount = countForeground(*result);
    // Square area: 11 * 11 = 121
    EXPECT_GT(filledCount, 80u);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(LevelTracingToolTest, SinglePixelRegion) {
    auto slice = createSlice(10, 10, 0.0f);
    // Single bright pixel
    FloatSlice2D::IndexType idx = {{5, 5}};
    slice->SetPixel(idx, 200.0f);

    IndexPoint seed = {5, 5};
    auto result = LevelTracingTool::traceAndFill(slice.GetPointer(), seed);
    ASSERT_TRUE(result.has_value());

    size_t count = countForeground(*result);
    EXPECT_EQ(count, 1u) << "Single pixel should produce single-pixel mask";
}

TEST(LevelTracingToolTest, SeedOnBackgroundWithDefaultTolerance) {
    auto slice = createSlice(20, 20, 0.0f);
    drawRect(slice, 8, 8, 12, 12, 200.0f);

    // Seed on background (intensity=0), default tolerance=5% of 200=10
    // Band: [-10, 10] — doesn't overlap with 200, so only background floods
    IndexPoint seed = {2, 2};
    auto result = LevelTracingTool::traceAndFill(slice.GetPointer(), seed);
    ASSERT_TRUE(result.has_value());

    // The filled region should be the background, not the rectangle
    BinarySlice2D::IndexType rectCenter = {{10, 10}};
    EXPECT_EQ(result.value()->GetPixel(rectCenter), 0)
        << "Rectangle should not be in background flood fill";
}

TEST(LevelTracingToolTest, CustomForegroundValue) {
    auto slice = createSlice(20, 20, 0.0f);
    drawRect(slice, 5, 5, 15, 15, 200.0f);

    IndexPoint seed = {10, 10};
    LevelTracingTool::Config config;
    config.tolerancePct = 5.0;
    config.foregroundValue = 255;

    auto result = LevelTracingTool::traceAndFill(
        slice.GetPointer(), seed, config);
    ASSERT_TRUE(result.has_value());

    BinarySlice2D::IndexType centerIdx = {{10, 10}};
    EXPECT_EQ(result.value()->GetPixel(centerIdx), 255);
}

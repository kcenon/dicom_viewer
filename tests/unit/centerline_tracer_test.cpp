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

#include "services/segmentation/centerline_tracer.hpp"

#include <cmath>

#include <gtest/gtest.h>
#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;
using FloatImage3D = CenterlineTracer::FloatImage3D;
using BinaryMaskType = CenterlineTracer::BinaryMaskType;

namespace {

/// Create a float 3D image of given size with uniform value
FloatImage3D::Pointer createImage(int sx, int sy, int sz,
                                   double spacingMm = 1.0,
                                   float value = 0.0f) {
    auto image = FloatImage3D::New();
    FloatImage3D::SizeType size = {{
        static_cast<unsigned long>(sx),
        static_cast<unsigned long>(sy),
        static_cast<unsigned long>(sz)
    }};
    FloatImage3D::IndexType start = {{0, 0, 0}};
    FloatImage3D::RegionType region{start, size};
    image->SetRegions(region);

    FloatImage3D::SpacingType spacing;
    spacing.Fill(spacingMm);
    image->SetSpacing(spacing);

    FloatImage3D::PointType origin;
    origin.Fill(0.0);
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(value);
    return image;
}

/// Draw a straight tube (cylinder) along the X axis in a float image
/// Center of tube is at (any_x, cy, cz), radius in mm
void drawStraightTube(FloatImage3D::Pointer image,
                       double cy, double cz, double radiusMm,
                       float intensity = 200.0f) {
    auto region = image->GetLargestPossibleRegion();
    auto spacing = image->GetSpacing();
    itk::ImageRegionIterator<FloatImage3D> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        // Physical coordinates
        double py = idx[1] * spacing[1];
        double pz = idx[2] * spacing[2];
        double dy = py - cy;
        double dz = pz - cz;
        double dist = std::sqrt(dy * dy + dz * dz);
        if (dist <= radiusMm) {
            it.Set(intensity);
        }
    }
}

/// Draw a curved tube (quarter circle) in the XY plane
/// Center of curvature at (cx, cy, cz), bend radius in mm, tube radius in mm
void drawCurvedTube(FloatImage3D::Pointer image,
                     double cx, double cy, double cz,
                     double bendRadiusMm, double tubeRadiusMm,
                     float intensity = 200.0f) {
    auto region = image->GetLargestPossibleRegion();
    auto spacing = image->GetSpacing();
    itk::ImageRegionIterator<FloatImage3D> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        double px = idx[0] * spacing[0];
        double py = idx[1] * spacing[1];
        double pz = idx[2] * spacing[2];

        // Distance from center of curvature in XY plane
        double dxy = std::sqrt((px - cx) * (px - cx) +
                               (py - cy) * (py - cy));
        // Distance from the circular arc
        double distFromArc = std::abs(dxy - bendRadiusMm);
        double distFromZ = std::abs(pz - cz);
        double totalDist = std::sqrt(distFromArc * distFromArc +
                                     distFromZ * distFromZ);

        if (totalDist <= tubeRadiusMm) {
            it.Set(intensity);
        }
    }
}

/// Count foreground voxels in mask
size_t countMaskVoxels(BinaryMaskType::Pointer mask) {
    size_t count = 0;
    itk::ImageRegionConstIterator<BinaryMaskType> it(
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

TEST(CenterlineTracerTest, NullImageReturnsError) {
    Point3D start = {0.0, 0.0, 0.0};
    Point3D end = {10.0, 0.0, 0.0};
    auto result = CenterlineTracer::traceCenterline(nullptr, start, end);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(CenterlineTracerTest, OutOfBoundsStartReturnsError) {
    auto image = createImage(20, 20, 20);
    drawStraightTube(image, 10.0, 10.0, 3.0);

    Point3D start = {-100.0, 0.0, 0.0};  // way outside
    Point3D end = {10.0, 10.0, 10.0};
    auto result = CenterlineTracer::traceCenterline(image, start, end);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(CenterlineTracerTest, OutOfBoundsEndReturnsError) {
    auto image = createImage(20, 20, 20);
    drawStraightTube(image, 10.0, 10.0, 3.0);

    Point3D start = {10.0, 10.0, 10.0};
    Point3D end = {-100.0, 0.0, 0.0};
    auto result = CenterlineTracer::traceCenterline(image, start, end);
    EXPECT_FALSE(result.has_value());
}

TEST(CenterlineTracerTest, UniformImageReturnsError) {
    auto image = createImage(20, 20, 20, 1.0, 100.0f);  // uniform intensity
    Point3D start = {5.0, 5.0, 5.0};
    Point3D end = {15.0, 5.0, 5.0};
    auto result = CenterlineTracer::traceCenterline(image, start, end);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

// =============================================================================
// Straight tube path finding
// =============================================================================

TEST(CenterlineTracerTest, StraightTubePathFollowsVessel) {
    // Create 40x40x40 image with 1mm spacing, background = 10
    auto image = createImage(40, 40, 40, 1.0, 10.0f);
    // Draw bright tube along X axis centered at Y=20, Z=20, radius=4mm
    drawStraightTube(image, 20.0, 20.0, 4.0, 200.0f);

    Point3D start = {5.0, 20.0, 20.0};
    Point3D end = {35.0, 20.0, 20.0};

    CenterlineTracer::TraceConfig config;
    config.brightVessels = true;
    config.initialRadiusMm = 5.0;

    auto result = CenterlineTracer::traceCenterline(
        image.GetPointer(), start, end, config);
    ASSERT_TRUE(result.has_value())
        << "Error: " << result.error().message;

    auto& centerline = result.value();

    // Path should have multiple points
    EXPECT_GT(centerline.points.size(), 5u);

    // All points should stay close to the tube center (Y≈20, Z≈20)
    for (const auto& pt : centerline.points) {
        EXPECT_NEAR(pt[1], 20.0, 5.0)
            << "Y coordinate deviated from tube center";
        EXPECT_NEAR(pt[2], 20.0, 5.0)
            << "Z coordinate deviated from tube center";
    }

    // Total length should approximate the straight distance
    double expectedLength = 30.0;  // 35 - 5 = 30mm
    EXPECT_NEAR(centerline.totalLengthMm, expectedLength, 10.0);
}

TEST(CenterlineTracerTest, PathRadiiAreReasonable) {
    auto image = createImage(40, 40, 40, 1.0, 10.0f);
    drawStraightTube(image, 20.0, 20.0, 4.0, 200.0f);

    Point3D start = {5.0, 20.0, 20.0};
    Point3D end = {35.0, 20.0, 20.0};

    auto result = CenterlineTracer::traceCenterline(image.GetPointer(),
                                                     start, end);
    ASSERT_TRUE(result.has_value());

    // Radii should be roughly around the tube radius (4mm)
    for (double r : result->radii) {
        EXPECT_GT(r, 1.0) << "Radius too small";
        EXPECT_LT(r, 10.0) << "Radius too large";
    }
}

// =============================================================================
// Spline smoothing tests
// =============================================================================

TEST(CenterlineTracerTest, SmoothPathTooFewPointsReturnsOriginal) {
    std::vector<Point3D> twoPoints = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}};
    auto result = CenterlineTracer::smoothPath(twoPoints, 3);
    EXPECT_EQ(result.size(), twoPoints.size());
}

TEST(CenterlineTracerTest, SmoothPathIncreasesPointCount) {
    std::vector<Point3D> points = {
        {0.0, 0.0, 0.0},
        {5.0, 1.0, 0.0},
        {10.0, 0.0, 0.0},
        {15.0, -1.0, 0.0},
        {20.0, 0.0, 0.0}
    };

    auto smoothed = CenterlineTracer::smoothPath(points, 3);
    EXPECT_GT(smoothed.size(), points.size());
}

TEST(CenterlineTracerTest, SmoothPathPreservesEndpoints) {
    std::vector<Point3D> points = {
        {0.0, 0.0, 0.0},
        {5.0, 1.0, 0.0},
        {10.0, 0.0, 0.0},
        {15.0, 1.0, 0.0}
    };

    auto smoothed = CenterlineTracer::smoothPath(points, 3);
    ASSERT_GE(smoothed.size(), 2u);

    // First point should be close to original start
    EXPECT_NEAR(smoothed.front()[0], points.front()[0], 0.5);
    EXPECT_NEAR(smoothed.front()[1], points.front()[1], 0.5);

    // Last point should be original end
    EXPECT_NEAR(smoothed.back()[0], points.back()[0], 0.01);
    EXPECT_NEAR(smoothed.back()[1], points.back()[1], 0.01);
}

// =============================================================================
// Radius estimation tests
// =============================================================================

TEST(CenterlineTracerTest, EstimateRadiusNullImage) {
    Point3D center = {10.0, 10.0, 10.0};
    Point3D tangent = {1.0, 0.0, 0.0};
    double r = CenterlineTracer::estimateLocalRadius(nullptr, center, tangent);
    EXPECT_EQ(r, 1.0);  // default fallback
}

TEST(CenterlineTracerTest, EstimateRadiusOnStraightTube) {
    auto image = createImage(40, 40, 40, 1.0, 10.0f);
    drawStraightTube(image, 20.0, 20.0, 5.0, 200.0f);

    Point3D center = {20.0, 20.0, 20.0};
    Point3D tangent = {1.0, 0.0, 0.0};  // tube along X

    double r = CenterlineTracer::estimateLocalRadius(
        image.GetPointer(), center, tangent, 15.0);

    // Should be close to tube radius of 5mm
    EXPECT_NEAR(r, 5.0, 2.0);
}

// =============================================================================
// Mask generation tests
// =============================================================================

TEST(CenterlineTracerTest, GenerateMaskNullImage) {
    CenterlineTracer::CenterlineResult cl;
    cl.points = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}};
    cl.radii = {3.0, 3.0};

    auto result = CenterlineTracer::generateMask(cl, 3.0, nullptr);
    EXPECT_FALSE(result.has_value());
}

TEST(CenterlineTracerTest, GenerateMaskEmptyCenterline) {
    auto ref = createImage(20, 20, 20);
    CenterlineTracer::CenterlineResult cl;  // empty

    auto result = CenterlineTracer::generateMask(cl, 3.0, ref.GetPointer());
    EXPECT_FALSE(result.has_value());
}

TEST(CenterlineTracerTest, GenerateMaskProducesNonEmptyMask) {
    auto ref = createImage(30, 30, 30, 1.0, 0.0f);

    CenterlineTracer::CenterlineResult cl;
    // Straight centerline along X from (5,15,15) to (25,15,15)
    for (int x = 5; x <= 25; ++x) {
        cl.points.push_back({static_cast<double>(x), 15.0, 15.0});
        cl.radii.push_back(3.0);
    }

    auto result = CenterlineTracer::generateMask(cl, 3.0, ref.GetPointer());
    ASSERT_TRUE(result.has_value());

    size_t voxelCount = countMaskVoxels(*result);
    EXPECT_GT(voxelCount, 100u) << "Mask should contain tube voxels";
}

TEST(CenterlineTracerTest, GenerateMaskWithOverrideRadius) {
    auto ref = createImage(30, 30, 30, 1.0, 0.0f);

    CenterlineTracer::CenterlineResult cl;
    for (int x = 10; x <= 20; ++x) {
        cl.points.push_back({static_cast<double>(x), 15.0, 15.0});
        cl.radii.push_back(2.0);  // auto radius
    }

    // With small override radius
    auto small = CenterlineTracer::generateMask(cl, 1.0, ref.GetPointer());
    ASSERT_TRUE(small.has_value());
    size_t smallCount = countMaskVoxels(*small);

    // With large override radius
    auto large = CenterlineTracer::generateMask(cl, 5.0, ref.GetPointer());
    ASSERT_TRUE(large.has_value());
    size_t largeCount = countMaskVoxels(*large);

    // Larger radius → more voxels
    EXPECT_GT(largeCount, smallCount);
}

TEST(CenterlineTracerTest, GenerateMaskAutoRadius) {
    auto ref = createImage(30, 30, 30, 1.0, 0.0f);

    CenterlineTracer::CenterlineResult cl;
    for (int x = 10; x <= 20; ++x) {
        cl.points.push_back({static_cast<double>(x), 15.0, 15.0});
        cl.radii.push_back(3.0);
    }

    // Negative override → use per-point auto radius
    auto result = CenterlineTracer::generateMask(cl, -1.0, ref.GetPointer());
    ASSERT_TRUE(result.has_value());

    size_t voxelCount = countMaskVoxels(*result);
    EXPECT_GT(voxelCount, 50u);
}

// =============================================================================
// Physical ↔ voxel conversion tests
// =============================================================================

TEST(CenterlineTracerTest, PhysicalToIndexValidPoint) {
    auto image = createImage(20, 20, 20, 1.0);
    Point3D pt = {10.0, 10.0, 10.0};
    FloatImage3D::IndexType idx;

    EXPECT_TRUE(CenterlineTracer::physicalToIndex(image.GetPointer(), pt, idx));
    EXPECT_EQ(idx[0], 10);
    EXPECT_EQ(idx[1], 10);
    EXPECT_EQ(idx[2], 10);
}

TEST(CenterlineTracerTest, PhysicalToIndexOutOfBounds) {
    auto image = createImage(20, 20, 20, 1.0);
    Point3D pt = {-5.0, 10.0, 10.0};
    FloatImage3D::IndexType idx;

    EXPECT_FALSE(CenterlineTracer::physicalToIndex(image.GetPointer(), pt, idx));
}

TEST(CenterlineTracerTest, PhysicalToIndexNullImage) {
    Point3D pt = {10.0, 10.0, 10.0};
    FloatImage3D::IndexType idx;

    EXPECT_FALSE(CenterlineTracer::physicalToIndex(nullptr, pt, idx));
}

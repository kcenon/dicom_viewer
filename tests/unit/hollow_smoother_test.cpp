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

#include "services/segmentation/hollow_tool.hpp"
#include "services/segmentation/mask_smoother.hpp"

#include <cmath>

#include <gtest/gtest.h>
#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;
using BinaryMaskType = HollowTool::BinaryMaskType;

namespace {

/// Create a binary mask of given size with all zeros
BinaryMaskType::Pointer createMask(int sx, int sy, int sz,
                                    double spacingMm = 1.0) {
    auto image = BinaryMaskType::New();
    BinaryMaskType::SizeType size = {{
        static_cast<unsigned long>(sx),
        static_cast<unsigned long>(sy),
        static_cast<unsigned long>(sz)
    }};
    BinaryMaskType::IndexType start = {{0, 0, 0}};
    BinaryMaskType::RegionType region{start, size};
    image->SetRegions(region);

    BinaryMaskType::SpacingType spacing;
    spacing.Fill(spacingMm);
    image->SetSpacing(spacing);

    BinaryMaskType::PointType origin;
    origin.Fill(0.0);
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(0);
    return image;
}

/// Draw a filled sphere in a binary mask
void drawSphere(BinaryMaskType::Pointer image,
                double cx, double cy, double cz,
                double radius, uint8_t label = 1) {
    auto region = image->GetLargestPossibleRegion();
    itk::ImageRegionIterator<BinaryMaskType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        double dx = idx[0] - cx;
        double dy = idx[1] - cy;
        double dz = idx[2] - cz;
        if (dx * dx + dy * dy + dz * dz <= radius * radius) {
            it.Set(label);
        }
    }
}

/// Draw a filled cube in a binary mask
void drawCube(BinaryMaskType::Pointer image,
              int x0, int y0, int z0,
              int x1, int y1, int z1, uint8_t label = 1) {
    auto region = image->GetLargestPossibleRegion();
    itk::ImageRegionIterator<BinaryMaskType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        if (idx[0] >= x0 && idx[0] <= x1 &&
            idx[1] >= y0 && idx[1] <= y1 &&
            idx[2] >= z0 && idx[2] <= z1) {
            it.Set(label);
        }
    }
}

/// Count foreground voxels
size_t countVoxels(BinaryMaskType::Pointer image, uint8_t fg = 1) {
    return MaskSmoother::countForeground(image.GetPointer(), fg);
}

}  // anonymous namespace

// =============================================================================
// HollowTool tests
// =============================================================================

TEST(HollowToolTest, NullInputReturnsError) {
    auto result = HollowTool::makeHollow(nullptr, 1.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(HollowToolTest, ZeroThicknessReturnsError) {
    auto mask = createMask(10, 10, 10);
    auto result = HollowTool::makeHollow(mask, 0.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST(HollowToolTest, NegativeThicknessReturnsError) {
    auto mask = createMask(10, 10, 10);
    auto result = HollowTool::makeHollow(mask, -1.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST(HollowToolTest, InsideHollowReducesVolume) {
    auto mask = createMask(30, 30, 30);
    drawSphere(mask, 15.0, 15.0, 15.0, 10.0);

    size_t originalVolume = countVoxels(mask);
    ASSERT_GT(originalVolume, 100u);

    HollowTool::Config config;
    config.thicknessMm = 2.0;
    config.direction = HollowDirection::Inside;

    auto result = HollowTool::makeHollow(mask, config);
    ASSERT_TRUE(result.has_value());

    size_t shellVolume = countVoxels(*result);
    // Shell should be smaller than original (it's a subset)
    EXPECT_LT(shellVolume, originalVolume);
    // Shell should not be empty
    EXPECT_GT(shellVolume, 0u);
}

TEST(HollowToolTest, OutsideHollowExcludesOriginalInterior) {
    auto mask = createMask(30, 30, 30);
    drawSphere(mask, 15.0, 15.0, 15.0, 8.0);

    HollowTool::Config config;
    config.thicknessMm = 2.0;
    config.direction = HollowDirection::Outside;

    auto result = HollowTool::makeHollow(mask, config);
    ASSERT_TRUE(result.has_value());

    auto shell = *result;
    size_t shellVolume = countVoxels(shell);
    EXPECT_GT(shellVolume, 0u);

    // Center of original sphere should NOT be in the outside shell
    BinaryMaskType::IndexType center = {{15, 15, 15}};
    EXPECT_EQ(shell->GetPixel(center), 0);
}

TEST(HollowToolTest, BothDirectionShell) {
    auto mask = createMask(30, 30, 30);
    drawSphere(mask, 15.0, 15.0, 15.0, 10.0);

    HollowTool::Config config;
    config.thicknessMm = 2.0;
    config.direction = HollowDirection::Both;

    auto result = HollowTool::makeHollow(mask, config);
    ASSERT_TRUE(result.has_value());

    size_t shellVolume = countVoxels(*result);
    EXPECT_GT(shellVolume, 0u);

    // Center should be empty (deep interior)
    BinaryMaskType::IndexType center = {{15, 15, 15}};
    EXPECT_EQ((*result)->GetPixel(center), 0);
}

TEST(HollowToolTest, InsideShellIsSubsetOfOriginal) {
    auto mask = createMask(30, 30, 30);
    drawSphere(mask, 15.0, 15.0, 15.0, 10.0);

    HollowTool::Config config;
    config.thicknessMm = 2.0;
    config.direction = HollowDirection::Inside;

    auto result = HollowTool::makeHollow(mask, config);
    ASSERT_TRUE(result.has_value());

    // Every shell voxel should also be in the original mask
    auto shell = *result;
    auto region = shell->GetLargestPossibleRegion();
    itk::ImageRegionConstIterator<BinaryMaskType> itS(shell, region);
    itk::ImageRegionConstIterator<BinaryMaskType> itM(mask, region);
    for (itS.GoToBegin(), itM.GoToBegin(); !itS.IsAtEnd(); ++itS, ++itM) {
        if (itS.Get() == 1) {
            EXPECT_EQ(itM.Get(), 1);
        }
    }
}

TEST(HollowToolTest, EmptyMaskReturnsEmptyShell) {
    auto mask = createMask(10, 10, 10);
    // No foreground voxels

    auto result = HollowTool::makeHollow(mask, 1.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(countVoxels(*result), 0u);
}

TEST(HollowToolTest, MmToVoxelRadiusWithDefaultSpacing) {
    auto mask = createMask(10, 10, 10, 1.0);  // 1mm spacing
    EXPECT_EQ(HollowTool::mmToVoxelRadius(mask.GetPointer(), 2.0), 2);
    EXPECT_EQ(HollowTool::mmToVoxelRadius(mask.GetPointer(), 0.3), 1);
}

TEST(HollowToolTest, MmToVoxelRadiusWithFineSpacing) {
    auto mask = createMask(10, 10, 10, 0.5);  // 0.5mm spacing
    // 2mm / 0.5mm = 4 voxels
    EXPECT_EQ(HollowTool::mmToVoxelRadius(mask.GetPointer(), 2.0), 4);
}

TEST(HollowToolTest, MmToVoxelRadiusNullReturnsOne) {
    EXPECT_EQ(HollowTool::mmToVoxelRadius(nullptr, 2.0), 1);
}

TEST(HollowToolTest, CubeInsideHollowHasNoInterior) {
    auto mask = createMask(20, 20, 20);
    drawCube(mask, 5, 5, 5, 14, 14, 14);

    HollowTool::Config config;
    config.thicknessMm = 2.0;
    config.direction = HollowDirection::Inside;

    auto result = HollowTool::makeHollow(mask, config);
    ASSERT_TRUE(result.has_value());

    // Deep interior should be empty
    BinaryMaskType::IndexType interior = {{10, 10, 10}};
    EXPECT_EQ((*result)->GetPixel(interior), 0);

    // Surface should be filled
    BinaryMaskType::IndexType surface = {{5, 10, 10}};
    EXPECT_EQ((*result)->GetPixel(surface), 1);
}

// =============================================================================
// MaskSmoother tests
// =============================================================================

TEST(MaskSmootherTest, NullInputReturnsError) {
    auto result = MaskSmoother::smooth(nullptr, 1.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(MaskSmootherTest, ZeroSigmaReturnsError) {
    auto mask = createMask(10, 10, 10);
    auto result = MaskSmoother::smooth(mask, 0.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST(MaskSmootherTest, NegativeSigmaReturnsError) {
    auto mask = createMask(10, 10, 10);
    auto result = MaskSmoother::smooth(mask, -1.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST(MaskSmootherTest, EmptyMaskReturnsEmpty) {
    auto mask = createMask(10, 10, 10);
    auto result = MaskSmoother::smooth(mask, 1.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(countVoxels(*result), 0u);
}

TEST(MaskSmootherTest, VolumePreservation) {
    auto mask = createMask(30, 30, 30);
    drawSphere(mask, 15.0, 15.0, 15.0, 8.0);

    size_t originalVolume = countVoxels(mask);
    ASSERT_GT(originalVolume, 100u);

    MaskSmoother::Config config;
    config.sigmaMm = 1.0;
    config.volumeTolerance = 0.02;  // 2% tolerance for test

    auto result = MaskSmoother::smooth(mask, config);
    ASSERT_TRUE(result.has_value());

    size_t smoothedVolume = countVoxels(*result);
    double ratio = std::abs(static_cast<double>(smoothedVolume) -
                            static_cast<double>(originalVolume)) /
                   static_cast<double>(originalVolume);

    // Volume should be within tolerance
    EXPECT_LE(ratio, config.volumeTolerance)
        << "Original: " << originalVolume
        << " Smoothed: " << smoothedVolume
        << " Ratio: " << ratio;
}

TEST(MaskSmootherTest, SmoothedMaskIsNotIdentical) {
    auto mask = createMask(30, 30, 30);
    // Create a cube with sharp corners
    drawCube(mask, 10, 10, 10, 19, 19, 19);

    auto result = MaskSmoother::smooth(mask, 1.5);
    ASSERT_TRUE(result.has_value());

    // The smoothed mask should differ from original (corners smoothed)
    auto smoothed = *result;
    int diffCount = 0;
    auto region = mask->GetLargestPossibleRegion();
    itk::ImageRegionConstIterator<BinaryMaskType> itO(mask, region);
    itk::ImageRegionConstIterator<BinaryMaskType> itS(smoothed, region);
    for (itO.GoToBegin(), itS.GoToBegin(); !itO.IsAtEnd(); ++itO, ++itS) {
        if (itO.Get() != itS.Get()) {
            ++diffCount;
        }
    }
    EXPECT_GT(diffCount, 0)
        << "Smoothed mask should differ from original at corners";
}

TEST(MaskSmootherTest, LargeSigmaPreservesVolume) {
    auto mask = createMask(40, 40, 40);
    drawSphere(mask, 20.0, 20.0, 20.0, 10.0);

    size_t originalVolume = countVoxels(mask);

    MaskSmoother::Config config;
    config.sigmaMm = 3.0;
    config.volumeTolerance = 0.02;

    auto result = MaskSmoother::smooth(mask, config);
    ASSERT_TRUE(result.has_value());

    size_t smoothedVolume = countVoxels(*result);
    double ratio = std::abs(static_cast<double>(smoothedVolume) -
                            static_cast<double>(originalVolume)) /
                   static_cast<double>(originalVolume);

    EXPECT_LE(ratio, config.volumeTolerance)
        << "Original: " << originalVolume
        << " Smoothed: " << smoothedVolume;
}

TEST(MaskSmootherTest, CountForegroundWorks) {
    auto mask = createMask(10, 10, 10);
    EXPECT_EQ(MaskSmoother::countForeground(mask.GetPointer()), 0u);

    drawSphere(mask, 5.0, 5.0, 5.0, 3.0);
    size_t count = MaskSmoother::countForeground(mask.GetPointer());
    EXPECT_GT(count, 0u);
    EXPECT_LT(count, 1000u);
}

TEST(MaskSmootherTest, CountForegroundNullReturnsZero) {
    EXPECT_EQ(MaskSmoother::countForeground(nullptr), 0u);
}

TEST(MaskSmootherTest, CountAboveThresholdNullReturnsZero) {
    EXPECT_EQ(MaskSmoother::countAboveThreshold(nullptr, 0.5f), 0u);
}

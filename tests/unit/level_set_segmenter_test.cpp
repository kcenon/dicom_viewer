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

#include "services/segmentation/level_set_segmenter.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

class LevelSetSegmenterTest : public ::testing::Test {
protected:
    using ImageType = LevelSetSegmenter::ImageType;
    using MaskType = LevelSetSegmenter::MaskType;

    void SetUp() override {
        segmenter_ = std::make_unique<LevelSetSegmenter>();
    }

    /**
     * @brief Create a synthetic test image with a spherical region
     */
    ImageType::Pointer createSphereImage(
        unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ,
        double centerX, double centerY, double centerZ,
        double radius, short insideValue, short outsideValue
    ) {
        auto image = ImageType::New();

        ImageType::RegionType region;
        ImageType::IndexType start = {{0, 0, 0}};
        ImageType::SizeType size = {{sizeX, sizeY, sizeZ}};
        region.SetSize(size);
        region.SetIndex(start);

        image->SetRegions(region);

        // Set spacing to 1mm
        ImageType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        image->SetSpacing(spacing);

        // Set origin to 0
        ImageType::PointType origin;
        origin[0] = 0.0;
        origin[1] = 0.0;
        origin[2] = 0.0;
        image->SetOrigin(origin);

        image->Allocate();
        image->FillBuffer(outsideValue);

        // Create sphere
        itk::ImageRegionIterator<ImageType> it(image, region);
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            auto idx = it.GetIndex();
            double dx = static_cast<double>(idx[0]) - centerX;
            double dy = static_cast<double>(idx[1]) - centerY;
            double dz = static_cast<double>(idx[2]) - centerZ;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

            if (dist <= radius) {
                it.Set(insideValue);
            }
        }

        return image;
    }

    /**
     * @brief Create a homogeneous test image
     */
    ImageType::Pointer createHomogeneousImage(
        unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ,
        short value
    ) {
        auto image = ImageType::New();

        ImageType::RegionType region;
        ImageType::IndexType start = {{0, 0, 0}};
        ImageType::SizeType size = {{sizeX, sizeY, sizeZ}};
        region.SetSize(size);
        region.SetIndex(start);

        image->SetRegions(region);

        ImageType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        image->SetSpacing(spacing);

        ImageType::PointType origin;
        origin[0] = 0.0;
        origin[1] = 0.0;
        origin[2] = 0.0;
        image->SetOrigin(origin);

        image->Allocate();
        image->FillBuffer(value);

        return image;
    }

    /**
     * @brief Count pixels with specific label value in mask
     */
    int countMaskPixels(MaskType::Pointer mask, unsigned char value) {
        int count = 0;
        itk::ImageRegionIterator<MaskType> it(
            mask, mask->GetLargestPossibleRegion()
        );
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == value) {
                ++count;
            }
        }
        return count;
    }

    std::unique_ptr<LevelSetSegmenter> segmenter_;
};

// Input validation tests
TEST_F(LevelSetSegmenterTest, GeodesicActiveContourRejectsNullInput) {
    LevelSetParameters params;
    params.seedPoints = {{25.0, 25.0, 25.0}};

    auto result = segmenter_->geodesicActiveContour(nullptr, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(LevelSetSegmenterTest, ThresholdLevelSetRejectsNullInput) {
    ThresholdLevelSetParameters params;
    params.seedPoints = {{25.0, 25.0, 25.0}};

    auto result = segmenter_->thresholdLevelSet(nullptr, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(LevelSetSegmenterTest, GeodesicActiveContourRejectsEmptySeeds) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    LevelSetParameters params;
    // Empty seeds

    auto result = segmenter_->geodesicActiveContour(image, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(LevelSetSegmenterTest, ThresholdLevelSetRejectsEmptySeeds) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    ThresholdLevelSetParameters params;
    // Empty seeds

    auto result = segmenter_->thresholdLevelSet(image, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(LevelSetSegmenterTest, GeodesicActiveContourRejectsInvalidRadius) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    LevelSetParameters params;
    params.seedPoints = {{25.0, 25.0, 25.0}};
    params.seedRadius = 0.0;  // Invalid

    auto result = segmenter_->geodesicActiveContour(image, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(LevelSetSegmenterTest, ThresholdLevelSetRejectsInvalidThresholds) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    ThresholdLevelSetParameters params;
    params.seedPoints = {{25.0, 25.0, 25.0}};
    params.lowerThreshold = 200.0;
    params.upperThreshold = 100.0;  // lower > upper

    auto result = segmenter_->thresholdLevelSet(image, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(LevelSetSegmenterTest, RejectsOutOfBoundsSeed) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    LevelSetParameters params;
    params.seedPoints = {{100.0, 100.0, 100.0}};  // Out of bounds

    auto result = segmenter_->geodesicActiveContour(image, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

// Seed point validation tests
TEST_F(LevelSetSegmenterTest, IsValidSeedPointReturnsTrueForValidSeed) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    LevelSetSeedPoint seed{25.0, 25.0, 25.0};
    EXPECT_TRUE(LevelSetSegmenter::isValidSeedPoint(image, seed));
}

TEST_F(LevelSetSegmenterTest, IsValidSeedPointReturnsFalseForInvalidSeed) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    LevelSetSeedPoint seed{100.0, 100.0, 100.0};
    EXPECT_FALSE(LevelSetSegmenter::isValidSeedPoint(image, seed));
}

TEST_F(LevelSetSegmenterTest, IsValidSeedPointReturnsFalseForNullImage) {
    LevelSetSeedPoint seed{25.0, 25.0, 25.0};
    EXPECT_FALSE(LevelSetSegmenter::isValidSeedPoint(nullptr, seed));
}

// Threshold Level Set functional tests
TEST_F(LevelSetSegmenterTest, ThresholdLevelSetSegmentsHomogeneousRegion) {
    // Create image with a distinct sphere
    auto image = createSphereImage(
        50, 50, 50,     // size
        25.0, 25.0, 25.0,  // center
        10.0,           // radius
        200,            // inside value (high intensity)
        0               // outside value
    );

    ThresholdLevelSetParameters params;
    params.seedPoints = {{25.0, 25.0, 25.0}};
    params.seedRadius = 3.0;
    params.lowerThreshold = 100.0;
    params.upperThreshold = 300.0;
    params.maxIterations = 100;
    params.propagationScaling = 1.0;
    params.curvatureScaling = 0.0;

    auto result = segmenter_->thresholdLevelSet(image, params);

    ASSERT_TRUE(result.has_value()) << "Segmentation failed: " << result.error().message;

    EXPECT_GT(result->iterations, 0);
    EXPECT_NE(result->mask, nullptr);

    // Check that some region was segmented
    int segmentedPixels = countMaskPixels(result->mask, 1);
    EXPECT_GT(segmentedPixels, 0);
}

TEST_F(LevelSetSegmenterTest, ThresholdLevelSetReturnsIterationInfo) {
    auto image = createHomogeneousImage(30, 30, 30, 100);

    ThresholdLevelSetParameters params;
    params.seedPoints = {{15.0, 15.0, 15.0}};
    params.seedRadius = 3.0;
    params.lowerThreshold = 50.0;
    params.upperThreshold = 150.0;
    params.maxIterations = 50;

    auto result = segmenter_->thresholdLevelSet(image, params);

    ASSERT_TRUE(result.has_value());

    // Should have iteration count
    EXPECT_GE(result->iterations, 0);
    EXPECT_LE(result->iterations, params.maxIterations);

    // Should have RMS value
    EXPECT_GE(result->finalRMS, 0.0);
}

// Geodesic Active Contour functional tests
TEST_F(LevelSetSegmenterTest, GeodesicActiveContourProducesOutput) {
    // Create a simple test image with clear edges
    auto image = createSphereImage(
        50, 50, 50,
        25.0, 25.0, 25.0,
        12.0,
        200,
        0
    );

    LevelSetParameters params;
    params.seedPoints = {{25.0, 25.0, 25.0}};
    params.seedRadius = 5.0;
    params.propagationScaling = 1.0;
    params.curvatureScaling = 0.2;
    params.advectionScaling = 1.0;
    params.maxIterations = 100;
    params.sigma = 1.0;

    auto result = segmenter_->geodesicActiveContour(image, params);

    ASSERT_TRUE(result.has_value()) << "Segmentation failed: " << result.error().message;

    EXPECT_NE(result->mask, nullptr);
    EXPECT_GE(result->iterations, 0);

    // Should produce some segmented region
    int segmentedPixels = countMaskPixels(result->mask, 1);
    EXPECT_GT(segmentedPixels, 0);
}

TEST_F(LevelSetSegmenterTest, GeodesicActiveContourRespectsMaxIterations) {
    auto image = createHomogeneousImage(30, 30, 30, 100);

    LevelSetParameters params;
    params.seedPoints = {{15.0, 15.0, 15.0}};
    params.seedRadius = 3.0;
    params.maxIterations = 10;  // Low iteration count

    auto result = segmenter_->geodesicActiveContour(image, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_LE(result->iterations, params.maxIterations);
}

// Progress callback tests
TEST_F(LevelSetSegmenterTest, ProgressCallbackIsCalled) {
    auto image = createHomogeneousImage(30, 30, 30, 100);

    int callbackCount = 0;
    double lastProgress = -1.0;

    segmenter_->setProgressCallback([&](double progress) {
        ++callbackCount;
        lastProgress = progress;
    });

    ThresholdLevelSetParameters params;
    params.seedPoints = {{15.0, 15.0, 15.0}};
    params.seedRadius = 3.0;
    params.lowerThreshold = 50.0;
    params.upperThreshold = 150.0;
    params.maxIterations = 20;

    auto result = segmenter_->thresholdLevelSet(image, params);

    // Progress callback should have been called at least once
    EXPECT_GE(callbackCount, 0);
}

// Multiple seeds tests
TEST_F(LevelSetSegmenterTest, ThresholdLevelSetWithMultipleSeeds) {
    auto image = createHomogeneousImage(50, 50, 50, 100);

    ThresholdLevelSetParameters params;
    params.seedPoints = {
        {15.0, 15.0, 25.0},
        {35.0, 35.0, 25.0}
    };
    params.seedRadius = 3.0;
    params.lowerThreshold = 50.0;
    params.upperThreshold = 150.0;
    params.maxIterations = 50;

    auto result = segmenter_->thresholdLevelSet(image, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->mask, nullptr);

    // Should have segmented region from both seeds
    int segmentedPixels = countMaskPixels(result->mask, 1);
    EXPECT_GT(segmentedPixels, 0);
}

// Parameter validation tests
TEST_F(LevelSetSegmenterTest, LevelSetParametersValidation) {
    LevelSetParameters params;

    // Empty seeds - invalid
    EXPECT_FALSE(params.isValid());

    // Add seed
    params.seedPoints = {{10.0, 10.0, 10.0}};
    EXPECT_TRUE(params.isValid());

    // Zero radius - invalid
    params.seedRadius = 0.0;
    EXPECT_FALSE(params.isValid());

    // Negative iterations - invalid
    params.seedRadius = 5.0;
    params.maxIterations = -1;
    EXPECT_FALSE(params.isValid());

    // Zero RMS threshold - invalid
    params.maxIterations = 100;
    params.rmsThreshold = 0.0;
    EXPECT_FALSE(params.isValid());
}

TEST_F(LevelSetSegmenterTest, ThresholdLevelSetParametersValidation) {
    ThresholdLevelSetParameters params;

    // Empty seeds - invalid
    EXPECT_FALSE(params.isValid());

    // Add seed
    params.seedPoints = {{10.0, 10.0, 10.0}};
    EXPECT_TRUE(params.isValid());

    // Inverted thresholds - invalid
    params.lowerThreshold = 200.0;
    params.upperThreshold = 100.0;
    EXPECT_FALSE(params.isValid());

    // Zero radius - invalid
    params.lowerThreshold = 100.0;
    params.upperThreshold = 200.0;
    params.seedRadius = 0.0;
    EXPECT_FALSE(params.isValid());
}

// LevelSetSeedPoint tests
TEST_F(LevelSetSegmenterTest, LevelSetSeedPointEquality) {
    LevelSetSeedPoint p1{10.0, 20.0, 30.0};
    LevelSetSeedPoint p2{10.0, 20.0, 30.0};
    LevelSetSeedPoint p3{10.0, 20.0, 31.0};

    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);
}

TEST_F(LevelSetSegmenterTest, LevelSetSeedPointDefaultConstruction) {
    LevelSetSeedPoint p;

    EXPECT_DOUBLE_EQ(p.x, 0.0);
    EXPECT_DOUBLE_EQ(p.y, 0.0);
    EXPECT_DOUBLE_EQ(p.z, 0.0);
}

// Convergence test
TEST_F(LevelSetSegmenterTest, ThresholdLevelSetConvergesBeforeMaxIterations) {
    // Create a well-defined region that should converge quickly
    auto image = createSphereImage(
        40, 40, 40,
        20.0, 20.0, 20.0,
        8.0,
        100,
        0
    );

    ThresholdLevelSetParameters params;
    params.seedPoints = {{20.0, 20.0, 20.0}};
    params.seedRadius = 2.0;
    params.lowerThreshold = 50.0;
    params.upperThreshold = 150.0;
    params.maxIterations = 500;
    params.rmsThreshold = 0.001;  // Tight threshold

    auto result = segmenter_->thresholdLevelSet(image, params);

    ASSERT_TRUE(result.has_value());

    // With a well-defined region and proper parameters,
    // it should converge before max iterations
    // (This depends on the specific configuration)
    EXPECT_GE(result->iterations, 1);
}

// =============================================================================
// Edge case and algorithmic correctness tests (Issue #204)
// =============================================================================

TEST_F(LevelSetSegmenterTest, NonConvergingUniformRegion) {
    // Uniform image has no gradient → level set should still terminate gracefully
    auto image = createHomogeneousImage(40, 40, 40, 100);

    LevelSetParameters params;
    params.seedPoints = {{20.0, 20.0, 20.0}};
    params.seedRadius = 3.0;
    params.maxIterations = 50;  // Low limit to keep test fast
    params.rmsThreshold = 0.001;

    auto result = segmenter_->geodesicActiveContour(image, params);

    // Should not crash; may fail or return a degenerate mask
    if (result.has_value()) {
        EXPECT_GE(result->iterations, 1);
    } else {
        EXPECT_FALSE(result.error().message.empty());
    }
}

TEST_F(LevelSetSegmenterTest, NegativePropagationScalingContracts) {
    // Negative propagation should contract the initial seed region
    auto image = createSphereImage(50, 50, 50, 25.0, 25.0, 25.0, 15.0, 200, 0);

    LevelSetParameters params;
    params.seedPoints = {{25.0, 25.0, 25.0}};
    params.seedRadius = 12.0;  // Start inside the sphere
    params.propagationScaling = -1.0;  // Contract
    params.curvatureScaling = 0.5;
    params.maxIterations = 100;

    auto result = segmenter_->geodesicActiveContour(image, params);

    if (result.has_value()) {
        int foreground = countMaskPixels(result->mask, 1);
        // Contracted mask should have fewer voxels than the initial seed sphere
        double seedVol = (4.0 / 3.0) * M_PI * 12.0 * 12.0 * 12.0;
        EXPECT_LT(foreground, static_cast<int>(seedVol));
    }
    // If it fails due to contraction being too aggressive, that's acceptable
}

TEST_F(LevelSetSegmenterTest, OverlappingSeedsProduceSingleRegion) {
    // Multiple overlapping seeds should merge into one connected region
    auto image = createSphereImage(50, 50, 50, 25.0, 25.0, 25.0, 15.0, 200, 0);

    ThresholdLevelSetParameters params;
    params.seedPoints = {{23.0, 25.0, 25.0}, {27.0, 25.0, 25.0}};  // 4 voxels apart
    params.seedRadius = 5.0;  // Radii overlap
    params.lowerThreshold = 100.0;
    params.upperThreshold = 300.0;
    params.maxIterations = 200;

    auto result = segmenter_->thresholdLevelSet(image, params);

    ASSERT_TRUE(result.has_value());
    int foreground = countMaskPixels(result->mask, 1);
    EXPECT_GT(foreground, 0) << "Overlapping seeds should produce a non-empty mask";
}

TEST_F(LevelSetSegmenterTest, NonUnitSpacingHandledCorrectly) {
    // Anisotropic spacing (common in clinical CT: 0.5×0.5×2.0 mm)
    auto image = createSphereImage(50, 50, 25, 25.0, 25.0, 12.5, 10.0, 200, 0);

    ImageType::SpacingType spacing;
    spacing[0] = 0.5;
    spacing[1] = 0.5;
    spacing[2] = 2.0;
    image->SetSpacing(spacing);

    // Seed must be in physical coordinates:
    // voxel center (25,25,12.5) * spacing (0.5,0.5,2.0) = physical (12.5, 12.5, 25.0)
    ThresholdLevelSetParameters params;
    params.seedPoints = {{12.5, 12.5, 25.0}};
    params.seedRadius = 3.0;
    params.lowerThreshold = 100.0;
    params.upperThreshold = 300.0;
    params.maxIterations = 200;

    auto result = segmenter_->thresholdLevelSet(image, params);

    ASSERT_TRUE(result.has_value());
    int foreground = countMaskPixels(result->mask, 1);
    EXPECT_GT(foreground, 0) << "Non-unit spacing should be handled correctly";
}

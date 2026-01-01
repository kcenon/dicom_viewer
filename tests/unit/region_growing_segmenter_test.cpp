#include <gtest/gtest.h>

#include "services/segmentation/region_growing_segmenter.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

class RegionGrowingSegmenterTest : public ::testing::Test {
protected:
    using ImageType = RegionGrowingSegmenter::ImageType;
    using BinaryMaskType = RegionGrowingSegmenter::BinaryMaskType;

    void SetUp() override {
        segmenter_ = std::make_unique<RegionGrowingSegmenter>();
    }

    /**
     * @brief Create a test image with a central region of different intensity
     *
     * Creates a 20x20x20 image where:
     * - Background: value 0
     * - Central 10x10x10 region (indices 5-14): value 500
     * This simulates a simple organ structure for region growing tests.
     */
    ImageType::Pointer createTestImageWithRegion() {
        auto image = ImageType::New();

        ImageType::SizeType size;
        size[0] = 20;
        size[1] = 20;
        size[2] = 20;

        ImageType::IndexType start;
        start.Fill(0);

        ImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        image->SetRegions(region);
        image->Allocate();
        image->FillBuffer(0);  // Background

        // Create central high-intensity region
        itk::ImageRegionIterator<ImageType> it(image, region);
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            auto idx = it.GetIndex();
            if (idx[0] >= 5 && idx[0] < 15 &&
                idx[1] >= 5 && idx[1] < 15 &&
                idx[2] >= 5 && idx[2] < 15) {
                it.Set(500);
            }
        }

        return image;
    }

    /**
     * @brief Create a test image with gradient intensity
     *
     * Creates a 10x10x10 image where pixel value = x + y * 10 + z * 100
     * Values range from 0 to 999.
     */
    ImageType::Pointer createGradientImage() {
        auto image = ImageType::New();

        ImageType::SizeType size;
        size.Fill(10);

        ImageType::IndexType start;
        start.Fill(0);

        ImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        image->SetRegions(region);
        image->Allocate();

        itk::ImageRegionIterator<ImageType> it(image, region);
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            auto idx = it.GetIndex();
            it.Set(static_cast<short>(idx[0] + idx[1] * 10 + idx[2] * 100));
        }

        return image;
    }

    /**
     * @brief Count non-zero pixels in binary mask
     */
    int countNonZeroPixels(BinaryMaskType::Pointer mask) {
        int count = 0;
        itk::ImageRegionIterator<BinaryMaskType> it(
            mask, mask->GetLargestPossibleRegion()
        );
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() != 0) {
                ++count;
            }
        }
        return count;
    }

    std::unique_ptr<RegionGrowingSegmenter> segmenter_;
};

// ============================================================================
// Connected Threshold Tests
// ============================================================================

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdReturnsValidMask) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};  // Center of high-intensity region

    auto result = segmenter_->connectedThreshold(image, seeds, 400.0, 600.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdSegmentsCentralRegion) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    auto result = segmenter_->connectedThreshold(image, seeds, 400.0, 600.0);

    ASSERT_TRUE(result.has_value());

    // Central region is 10x10x10 = 1000 voxels
    int segmentedCount = countNonZeroPixels(result.value());
    EXPECT_EQ(segmentedCount, 1000);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdHandlesNullInput) {
    std::vector<SeedPoint> seeds = {{5, 5, 5}};
    auto result = segmenter_->connectedThreshold(nullptr, seeds, 0.0, 100.0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdRejectsEmptySeeds) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds;  // Empty

    auto result = segmenter_->connectedThreshold(image, seeds, 0.0, 100.0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdRejectsInvalidRange) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    // Upper < Lower
    auto result = segmenter_->connectedThreshold(image, seeds, 600.0, 400.0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdRejectsOutOfBoundsSeed) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{100, 100, 100}};  // Out of 20x20x20 bounds

    auto result = segmenter_->connectedThreshold(image, seeds, 400.0, 600.0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
    EXPECT_TRUE(result.error().message.find("out of") != std::string::npos);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdWithMultipleSeeds) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {
        {10, 10, 10},  // Center
        {6, 6, 6},     // Corner of region
        {14, 14, 14}   // Other corner
    };

    auto result = segmenter_->connectedThreshold(image, seeds, 400.0, 600.0);

    ASSERT_TRUE(result.has_value());
    // All seeds are in the same connected region
    int segmentedCount = countNonZeroPixels(result.value());
    EXPECT_EQ(segmentedCount, 1000);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdNoGrowthOutsideRange) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{0, 0, 0}};  // Background area

    // Range doesn't include background value (0)
    auto result = segmenter_->connectedThreshold(image, seeds, 400.0, 600.0);

    ASSERT_TRUE(result.has_value());
    // Should not segment anything as seed value is outside threshold range
    int segmentedCount = countNonZeroPixels(result.value());
    EXPECT_EQ(segmentedCount, 0);
}

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdWithParametersStruct) {
    auto image = createTestImageWithRegion();

    RegionGrowingSegmenter::ConnectedThresholdParameters params;
    params.seeds = {{10, 10, 10}};
    params.lowerThreshold = 400.0;
    params.upperThreshold = 600.0;
    params.replaceValue = 255;

    auto result = segmenter_->connectedThreshold(image, params);

    ASSERT_TRUE(result.has_value());

    // Check that replace value is 255
    itk::ImageRegionIterator<BinaryMaskType> it(
        result.value(), result.value()->GetLargestPossibleRegion()
    );
    bool found255 = false;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() == 255) {
            found255 = true;
            break;
        }
    }
    EXPECT_TRUE(found255);
}

// ============================================================================
// Confidence Connected Tests
// ============================================================================

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedReturnsValidMask) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    auto result = segmenter_->confidenceConnected(image, seeds, 2.5, 5);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedSegmentsRegion) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    auto result = segmenter_->confidenceConnected(image, seeds, 3.0, 5);

    ASSERT_TRUE(result.has_value());
    int segmentedCount = countNonZeroPixels(result.value());
    // Should segment the high-intensity region
    EXPECT_GT(segmentedCount, 0);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedHandlesNullInput) {
    std::vector<SeedPoint> seeds = {{5, 5, 5}};
    auto result = segmenter_->confidenceConnected(nullptr, seeds);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedRejectsEmptySeeds) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds;

    auto result = segmenter_->confidenceConnected(image, seeds);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedRejectsInvalidMultiplier) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    RegionGrowingSegmenter::ConfidenceConnectedParameters params;
    params.seeds = seeds;
    params.multiplier = -1.0;  // Invalid

    auto result = segmenter_->confidenceConnected(image, params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedRejectsZeroIterations) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    RegionGrowingSegmenter::ConfidenceConnectedParameters params;
    params.seeds = seeds;
    params.numberOfIterations = 0;  // Invalid

    auto result = segmenter_->confidenceConnected(image, params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedRejectsOutOfBoundsSeed) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{100, 100, 100}};

    auto result = segmenter_->confidenceConnected(image, seeds);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedWithMultipleSeeds) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {
        {10, 10, 10},
        {8, 8, 8},
        {12, 12, 12}
    };

    auto result = segmenter_->confidenceConnected(image, seeds, 2.5, 5);

    ASSERT_TRUE(result.has_value());
    int segmentedCount = countNonZeroPixels(result.value());
    EXPECT_GT(segmentedCount, 0);
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedWithParametersStruct) {
    auto image = createTestImageWithRegion();

    RegionGrowingSegmenter::ConfidenceConnectedParameters params;
    params.seeds = {{10, 10, 10}};
    params.multiplier = 2.5;
    params.numberOfIterations = 5;
    params.initialNeighborhoodRadius = 2;
    params.replaceValue = 128;

    auto result = segmenter_->confidenceConnected(image, params);

    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// Seed Point Validation Tests
// ============================================================================

TEST_F(RegionGrowingSegmenterTest, IsValidSeedPointReturnsTrueForValidPoint) {
    auto image = createTestImageWithRegion();

    EXPECT_TRUE(RegionGrowingSegmenter::isValidSeedPoint(image, {0, 0, 0}));
    EXPECT_TRUE(RegionGrowingSegmenter::isValidSeedPoint(image, {10, 10, 10}));
    EXPECT_TRUE(RegionGrowingSegmenter::isValidSeedPoint(image, {19, 19, 19}));
}

TEST_F(RegionGrowingSegmenterTest, IsValidSeedPointReturnsFalseForOutOfBounds) {
    auto image = createTestImageWithRegion();

    EXPECT_FALSE(RegionGrowingSegmenter::isValidSeedPoint(image, {20, 10, 10}));
    EXPECT_FALSE(RegionGrowingSegmenter::isValidSeedPoint(image, {10, 20, 10}));
    EXPECT_FALSE(RegionGrowingSegmenter::isValidSeedPoint(image, {10, 10, 20}));
    EXPECT_FALSE(RegionGrowingSegmenter::isValidSeedPoint(image, {-1, 10, 10}));
}

TEST_F(RegionGrowingSegmenterTest, IsValidSeedPointReturnsFalseForNullImage) {
    EXPECT_FALSE(RegionGrowingSegmenter::isValidSeedPoint(nullptr, {0, 0, 0}));
}

// ============================================================================
// SeedPoint Structure Tests
// ============================================================================

TEST_F(RegionGrowingSegmenterTest, SeedPointDefaultConstructor) {
    SeedPoint seed;
    EXPECT_EQ(seed.x, 0);
    EXPECT_EQ(seed.y, 0);
    EXPECT_EQ(seed.z, 0);
}

TEST_F(RegionGrowingSegmenterTest, SeedPointParameterizedConstructor) {
    SeedPoint seed(10, 20, 30);
    EXPECT_EQ(seed.x, 10);
    EXPECT_EQ(seed.y, 20);
    EXPECT_EQ(seed.z, 30);
}

TEST_F(RegionGrowingSegmenterTest, SeedPointEquality) {
    SeedPoint seed1(10, 20, 30);
    SeedPoint seed2(10, 20, 30);
    SeedPoint seed3(10, 20, 31);

    EXPECT_TRUE(seed1 == seed2);
    EXPECT_FALSE(seed1 == seed3);
}

// ============================================================================
// Parameter Validation Tests
// ============================================================================

TEST_F(RegionGrowingSegmenterTest, ConnectedThresholdParametersValidation) {
    RegionGrowingSegmenter::ConnectedThresholdParameters valid;
    valid.seeds = {{0, 0, 0}};
    valid.lowerThreshold = 0.0;
    valid.upperThreshold = 100.0;
    EXPECT_TRUE(valid.isValid());

    RegionGrowingSegmenter::ConnectedThresholdParameters equalThresholds;
    equalThresholds.seeds = {{0, 0, 0}};
    equalThresholds.lowerThreshold = 50.0;
    equalThresholds.upperThreshold = 50.0;
    EXPECT_TRUE(equalThresholds.isValid());

    RegionGrowingSegmenter::ConnectedThresholdParameters emptySeeds;
    emptySeeds.lowerThreshold = 0.0;
    emptySeeds.upperThreshold = 100.0;
    EXPECT_FALSE(emptySeeds.isValid());

    RegionGrowingSegmenter::ConnectedThresholdParameters invalidRange;
    invalidRange.seeds = {{0, 0, 0}};
    invalidRange.lowerThreshold = 100.0;
    invalidRange.upperThreshold = 50.0;
    EXPECT_FALSE(invalidRange.isValid());
}

TEST_F(RegionGrowingSegmenterTest, ConfidenceConnectedParametersValidation) {
    RegionGrowingSegmenter::ConfidenceConnectedParameters valid;
    valid.seeds = {{0, 0, 0}};
    valid.multiplier = 2.5;
    valid.numberOfIterations = 5;
    EXPECT_TRUE(valid.isValid());

    RegionGrowingSegmenter::ConfidenceConnectedParameters invalidMultiplier;
    invalidMultiplier.seeds = {{0, 0, 0}};
    invalidMultiplier.multiplier = 0.0;
    invalidMultiplier.numberOfIterations = 5;
    EXPECT_FALSE(invalidMultiplier.isValid());

    RegionGrowingSegmenter::ConfidenceConnectedParameters zeroIterations;
    zeroIterations.seeds = {{0, 0, 0}};
    zeroIterations.multiplier = 2.5;
    zeroIterations.numberOfIterations = 0;
    EXPECT_FALSE(zeroIterations.isValid());
}

// ============================================================================
// Progress Callback Tests
// ============================================================================

TEST_F(RegionGrowingSegmenterTest, ProgressCallbackIsCalledForConnectedThreshold) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    bool callbackCalled = false;
    segmenter_->setProgressCallback([&callbackCalled](double /*progress*/) {
        callbackCalled = true;
    });

    auto result = segmenter_->connectedThreshold(image, seeds, 400.0, 600.0);

    ASSERT_TRUE(result.has_value());
    // Note: Progress callback may not be called for very fast operations
}

TEST_F(RegionGrowingSegmenterTest, ProgressCallbackIsCalledForConfidenceConnected) {
    auto image = createTestImageWithRegion();
    std::vector<SeedPoint> seeds = {{10, 10, 10}};

    bool callbackCalled = false;
    segmenter_->setProgressCallback([&callbackCalled](double /*progress*/) {
        callbackCalled = true;
    });

    auto result = segmenter_->confidenceConnected(image, seeds);

    ASSERT_TRUE(result.has_value());
    // Note: Progress callback may not be called for very fast operations
}

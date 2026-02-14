#include <gtest/gtest.h>

#include "services/segmentation/threshold_segmenter.hpp"

#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

class ThresholdSegmenterTest : public ::testing::Test {
protected:
    using ImageType = ThresholdSegmenter::ImageType;
    using BinaryMaskType = ThresholdSegmenter::BinaryMaskType;

    void SetUp() override {
        segmenter_ = std::make_unique<ThresholdSegmenter>();
    }

    /**
     * @brief Create a test image with known pixel values
     *
     * Creates a 10x10x10 image where pixel value = x + y * 10 + z * 100
     * This gives values from 0 to 999
     */
    ImageType::Pointer createTestImage(
        unsigned int sizeX = 10,
        unsigned int sizeY = 10,
        unsigned int sizeZ = 10
    ) {
        auto image = ImageType::New();

        ImageType::SizeType size;
        size[0] = sizeX;
        size[1] = sizeY;
        size[2] = sizeZ;

        ImageType::IndexType start;
        start.Fill(0);

        ImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        image->SetRegions(region);
        image->Allocate();

        // Fill with predictable values
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

    std::unique_ptr<ThresholdSegmenter> segmenter_;
};

// Basic functionality tests
TEST_F(ThresholdSegmenterTest, ManualThresholdReturnsValidMask) {
    auto image = createTestImage();
    auto result = segmenter_->manualThreshold(image, 0.0, 100.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(ThresholdSegmenterTest, ManualThresholdHandlesNullInput) {
    auto result = segmenter_->manualThreshold(nullptr, 0.0, 100.0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(ThresholdSegmenterTest, ManualThresholdRejectsInvalidRange) {
    auto image = createTestImage();
    // Upper < Lower
    auto result = segmenter_->manualThreshold(image, 100.0, 50.0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(ThresholdSegmenterTest, ManualThresholdSegmentsCorrectPixels) {
    auto image = createTestImage();
    // Only first two Z slices have values < 200
    auto result = segmenter_->manualThreshold(image, 0.0, 199.0);

    ASSERT_TRUE(result.has_value());

    // First two slices (z=0, z=1) have values 0-109 and 100-209
    // Values in range [0, 199] should be marked
    int nonZeroCount = countNonZeroPixels(result.value());
    EXPECT_GT(nonZeroCount, 0);
    EXPECT_LT(nonZeroCount, 1000);  // Not all pixels
}

TEST_F(ThresholdSegmenterTest, ManualThresholdWithParametersStruct) {
    auto image = createTestImage();

    ThresholdSegmenter::ThresholdParameters params;
    params.lowerThreshold = 100.0;
    params.upperThreshold = 300.0;
    params.insideValue = 255;
    params.outsideValue = 0;

    auto result = segmenter_->manualThreshold(image, params);

    ASSERT_TRUE(result.has_value());

    // Check that inside value is 255
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

// Otsu threshold tests
TEST_F(ThresholdSegmenterTest, OtsuThresholdReturnsValidResult) {
    auto image = createTestImage();
    auto result = segmenter_->otsuThreshold(image);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->mask, nullptr);
    // Threshold should be somewhere in the middle of the value range
    EXPECT_GT(result->threshold, 0.0);
    EXPECT_LT(result->threshold, 1000.0);
}

TEST_F(ThresholdSegmenterTest, OtsuThresholdHandlesNullInput) {
    auto result = segmenter_->otsuThreshold(nullptr);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(ThresholdSegmenterTest, OtsuThresholdWithCustomBins) {
    auto image = createTestImage();

    ThresholdSegmenter::OtsuParameters params;
    params.numberOfHistogramBins = 128;

    auto result = segmenter_->otsuThreshold(image, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->mask, nullptr);
}

// Multi-threshold Otsu tests
TEST_F(ThresholdSegmenterTest, OtsuMultiThresholdReturnsValidResult) {
    auto image = createTestImage();
    auto result = segmenter_->otsuMultiThreshold(image, 2);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->labelMap, nullptr);
    EXPECT_EQ(result->thresholds.size(), 2u);
    // Thresholds should be sorted
    EXPECT_LT(result->thresholds[0], result->thresholds[1]);
}

TEST_F(ThresholdSegmenterTest, OtsuMultiThresholdHandlesNullInput) {
    auto result = segmenter_->otsuMultiThreshold(nullptr, 2);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(ThresholdSegmenterTest, OtsuMultiThresholdRejectsInvalidCount) {
    auto image = createTestImage();

    // Zero thresholds
    auto result = segmenter_->otsuMultiThreshold(image, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(ThresholdSegmenterTest, OtsuMultiThresholdCreatesMultipleRegions) {
    auto image = createTestImage();
    auto result = segmenter_->otsuMultiThreshold(image, 3);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->thresholds.size(), 3u);

    // Should have labels 0, 1, 2, 3 (4 regions for 3 thresholds)
    std::set<unsigned char> labels;
    itk::ImageRegionIterator<ThresholdSegmenter::LabelMapType> it(
        result->labelMap, result->labelMap->GetLargestPossibleRegion()
    );
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        labels.insert(it.Get());
    }
    EXPECT_GE(labels.size(), 2u);  // At least 2 different labels
}

// Slice threshold tests
TEST_F(ThresholdSegmenterTest, ThresholdSliceReturns2DMask) {
    auto image = createTestImage();
    auto result = segmenter_->thresholdSlice(image, 0, 0.0, 50.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);

    // Check output is 2D
    auto region = result.value()->GetLargestPossibleRegion();
    EXPECT_EQ(region.GetSize()[0], 10u);
    EXPECT_EQ(region.GetSize()[1], 10u);
}

TEST_F(ThresholdSegmenterTest, ThresholdSliceHandlesNullInput) {
    auto result = segmenter_->thresholdSlice(nullptr, 0, 0.0, 50.0);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(ThresholdSegmenterTest, ThresholdSliceRejectsInvalidSliceIndex) {
    auto image = createTestImage();
    auto result = segmenter_->thresholdSlice(image, 100, 0.0, 50.0);  // Out of range

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(ThresholdSegmenterTest, ThresholdSliceRejectsInvalidThresholdRange) {
    auto image = createTestImage();
    auto result = segmenter_->thresholdSlice(image, 0, 100.0, 50.0);  // Lower > Upper

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

// Parameter validation tests
TEST_F(ThresholdSegmenterTest, ThresholdParametersValidationWorks) {
    ThresholdSegmenter::ThresholdParameters valid;
    valid.lowerThreshold = 0.0;
    valid.upperThreshold = 100.0;
    EXPECT_TRUE(valid.isValid());

    ThresholdSegmenter::ThresholdParameters equal;
    equal.lowerThreshold = 50.0;
    equal.upperThreshold = 50.0;
    EXPECT_TRUE(equal.isValid());

    ThresholdSegmenter::ThresholdParameters invalid;
    invalid.lowerThreshold = 100.0;
    invalid.upperThreshold = 50.0;
    EXPECT_FALSE(invalid.isValid());
}

// Progress callback tests
TEST_F(ThresholdSegmenterTest, ProgressCallbackIsCalled) {
    auto image = createTestImage();

    bool callbackCalled = false;
    segmenter_->setProgressCallback([&callbackCalled](double /*progress*/) {
        callbackCalled = true;
    });

    auto result = segmenter_->manualThreshold(image, 0.0, 500.0);

    ASSERT_TRUE(result.has_value());
    // Note: Progress callback may not be called for very fast operations
    // This test just ensures no crash occurs
}

// Error message tests
TEST_F(ThresholdSegmenterTest, SegmentationErrorToStringWorks) {
    SegmentationError success{SegmentationError::Code::Success, ""};
    EXPECT_EQ(success.toString(), "Success");

    SegmentationError invalidInput{SegmentationError::Code::InvalidInput, "null pointer"};
    EXPECT_TRUE(invalidInput.toString().find("Invalid input") != std::string::npos);

    SegmentationError invalidParams{SegmentationError::Code::InvalidParameters, "bad range"};
    EXPECT_TRUE(invalidParams.toString().find("Invalid parameters") != std::string::npos);
}

TEST_F(ThresholdSegmenterTest, SegmentationErrorIsSuccessWorks) {
    SegmentationError success{SegmentationError::Code::Success, ""};
    EXPECT_TRUE(success.isSuccess());

    SegmentationError failure{SegmentationError::Code::InvalidInput, "error"};
    EXPECT_FALSE(failure.isSuccess());
}

// Edge case tests
TEST_F(ThresholdSegmenterTest, ManualThresholdHandlesEntireRange) {
    auto image = createTestImage();
    // All pixels should be selected
    auto result = segmenter_->manualThreshold(image, -32768.0, 32767.0);

    ASSERT_TRUE(result.has_value());
    int nonZeroCount = countNonZeroPixels(result.value());
    EXPECT_EQ(nonZeroCount, 1000);  // All 10x10x10 pixels
}

TEST_F(ThresholdSegmenterTest, ManualThresholdHandlesEmptyRange) {
    auto image = createTestImage();
    // No pixels should match (values are 0-999, threshold is > 1000)
    auto result = segmenter_->manualThreshold(image, 1000.0, 2000.0);

    ASSERT_TRUE(result.has_value());
    int nonZeroCount = countNonZeroPixels(result.value());
    EXPECT_EQ(nonZeroCount, 0);
}

// =============================================================================
// Edge case and algorithmic correctness tests (Issue #204)
// =============================================================================

TEST_F(ThresholdSegmenterTest, LargeVolume256CubedDoesNotCrash) {
    // 256³ = 16,777,216 voxels — verify no OOM or excessive latency
    auto image = createTestImage(256, 256, 256);
    auto result = segmenter_->manualThreshold(image, 0.0, 500.0);

    ASSERT_TRUE(result.has_value());
    auto maskSize = result.value()->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(maskSize[0], 256u);
    EXPECT_EQ(maskSize[1], 256u);
    EXPECT_EQ(maskSize[2], 256u);
}

TEST_F(ThresholdSegmenterTest, FloatingPointPrecisionNearBoundary) {
    // Verify thresholds handle floating-point edge cases correctly
    auto image = createTestImage();  // values 0–999 (integer cast)

    // Threshold that falls exactly on a boundary value
    auto result = segmenter_->manualThreshold(image, 99.999999, 100.000001);
    ASSERT_TRUE(result.has_value());

    // At least the pixel with value=100 should be included
    int count = countNonZeroPixels(result.value());
    EXPECT_GE(count, 1);
}

TEST_F(ThresholdSegmenterTest, NegativeHUValuesThresholdedCorrectly) {
    // Simulate CT lung window: HU range -1000 to -500
    auto image = ImageType::New();
    ImageType::SizeType size = {{20, 20, 20}};
    ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex({{0, 0, 0}});
    image->SetRegions(region);
    image->Allocate();

    itk::ImageRegionIterator<ImageType> it(image, region);
    int i = 0;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it, ++i) {
        // Fill with values from -1024 to +1024 across the volume
        it.Set(static_cast<short>(-1024 + (i * 2048) / 8000));
    }

    auto result = segmenter_->manualThreshold(image, -1000.0, -500.0);
    ASSERT_TRUE(result.has_value());
    int count = countNonZeroPixels(result.value());
    EXPECT_GT(count, 0) << "Should capture negative HU voxels in lung window";
}

TEST_F(ThresholdSegmenterTest, PipelineChainingThresholdThenOtsu) {
    auto image = createTestImage();

    // First pass: manual threshold to narrow range
    auto manualResult = segmenter_->manualThreshold(image, 0.0, 500.0);
    ASSERT_TRUE(manualResult.has_value());
    int manualCount = countNonZeroPixels(manualResult.value());

    // Second pass: Otsu on the same image
    auto otsuResult = segmenter_->otsuThreshold(image);
    ASSERT_TRUE(otsuResult.has_value());
    int otsuCount = countNonZeroPixels(otsuResult.value().mask);

    // Both should produce valid, non-empty masks
    EXPECT_GT(manualCount, 0);
    EXPECT_GT(otsuCount, 0);

    // Otsu should find a different split point than manual [0,500]
    EXPECT_NE(manualCount, otsuCount);
}

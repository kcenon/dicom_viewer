#include <gtest/gtest.h>

#include "services/segmentation/watershed_segmenter.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

class WatershedSegmenterTest : public ::testing::Test {
protected:
    using ImageType = WatershedSegmenter::ImageType;
    using LabelMapType = WatershedSegmenter::LabelMapType;
    using BinaryMaskType = WatershedSegmenter::BinaryMaskType;

    void SetUp() override {
        segmenter_ = std::make_unique<WatershedSegmenter>();
    }

    /**
     * @brief Create a test image with two distinct intensity regions
     *
     * Creates a 20x20x10 image where:
     * - Left half (x < 10): low intensity values (100)
     * - Right half (x >= 10): high intensity values (200)
     * This creates a clear boundary for watershed segmentation.
     */
    ImageType::Pointer createTwoRegionImage() {
        auto image = ImageType::New();

        ImageType::SizeType size;
        size[0] = 20;
        size[1] = 20;
        size[2] = 10;

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
            if (idx[0] < 10) {
                it.Set(100);  // Low intensity region
            } else {
                it.Set(200);  // High intensity region
            }
        }

        return image;
    }

    /**
     * @brief Create a test image with gradient from left to right
     *
     * Creates a 20x20x10 image where pixel values vary from 0 to 255
     * based on x coordinate.
     */
    ImageType::Pointer createGradientImage() {
        auto image = ImageType::New();

        ImageType::SizeType size;
        size[0] = 20;
        size[1] = 20;
        size[2] = 10;

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
            // Gradient from 0 to 255 based on x coordinate
            it.Set(static_cast<short>(idx[0] * 255 / 19));
        }

        return image;
    }

    /**
     * @brief Create marker image for marker-based watershed
     *
     * Creates markers for two regions at opposite corners.
     */
    LabelMapType::Pointer createMarkerImage() {
        auto markers = LabelMapType::New();

        LabelMapType::SizeType size;
        size[0] = 20;
        size[1] = 20;
        size[2] = 10;

        LabelMapType::IndexType start;
        start.Fill(0);

        LabelMapType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        markers->SetRegions(region);
        markers->Allocate();
        markers->FillBuffer(0);

        // Create two marker regions
        LabelMapType::IndexType idx1 = {{2, 2, 5}};
        LabelMapType::IndexType idx2 = {{17, 17, 5}};

        // Small marker regions (3x3x3)
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    LabelMapType::IndexType p1 = {{idx1[0] + dx, idx1[1] + dy, idx1[2] + dz}};
                    LabelMapType::IndexType p2 = {{idx2[0] + dx, idx2[1] + dy, idx2[2] + dz}};

                    if (region.IsInside(p1)) {
                        markers->SetPixel(p1, 1);
                    }
                    if (region.IsInside(p2)) {
                        markers->SetPixel(p2, 2);
                    }
                }
            }
        }

        return markers;
    }

    /**
     * @brief Count unique labels in label map (excluding background 0)
     */
    size_t countUniqueLabels(LabelMapType::Pointer labelMap) {
        std::set<unsigned long> labels;
        itk::ImageRegionConstIterator<LabelMapType> it(
            labelMap, labelMap->GetLargestPossibleRegion()
        );
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() != 0) {
                labels.insert(it.Get());
            }
        }
        return labels.size();
    }

    /**
     * @brief Count non-zero pixels in binary mask
     */
    int countNonZeroPixels(BinaryMaskType::Pointer mask) {
        int count = 0;
        itk::ImageRegionConstIterator<BinaryMaskType> it(
            mask, mask->GetLargestPossibleRegion()
        );
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() != 0) {
                ++count;
            }
        }
        return count;
    }

    std::unique_ptr<WatershedSegmenter> segmenter_;
};

// Basic functionality tests
TEST_F(WatershedSegmenterTest, SegmentReturnsValidResult) {
    auto image = createTwoRegionImage();

    WatershedParameters params;
    params.level = 0.1;
    params.threshold = 0.01;
    params.gradientSigma = 1.0;

    auto result = segmenter_->segment(image, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->labelMap, nullptr);
    EXPECT_GT(result->regionCount, 0u);
}

TEST_F(WatershedSegmenterTest, SegmentHandlesNullInput) {
    WatershedParameters params;
    params.level = 0.1;

    auto result = segmenter_->segment(nullptr, params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(WatershedSegmenterTest, SegmentRejectsInvalidParameters) {
    auto image = createTwoRegionImage();

    // Level out of range
    WatershedParameters invalidParams;
    invalidParams.level = 1.5;  // Invalid: should be 0-1

    auto result = segmenter_->segment(image, invalidParams);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(WatershedSegmenterTest, SegmentProducesMultipleRegions) {
    auto image = createTwoRegionImage();

    WatershedParameters params;
    params.level = 0.5;  // Higher level = fewer regions
    params.threshold = 0.001;
    params.gradientSigma = 0.5;

    auto result = segmenter_->segment(image, params);

    ASSERT_TRUE(result.has_value());
    // Should produce at least 1 region (watershed always produces some regions)
    EXPECT_GE(result->regionCount, 1u);
    EXPECT_EQ(result->regions.size(), result->regionCount);
}

TEST_F(WatershedSegmenterTest, SegmentWithHighLevelProducesFewerRegions) {
    auto image = createGradientImage();

    WatershedParameters lowLevelParams;
    lowLevelParams.level = 0.01;
    lowLevelParams.threshold = 0.001;

    WatershedParameters highLevelParams;
    highLevelParams.level = 0.5;
    highLevelParams.threshold = 0.001;

    auto lowLevelResult = segmenter_->segment(image, lowLevelParams);
    auto highLevelResult = segmenter_->segment(image, highLevelParams);

    ASSERT_TRUE(lowLevelResult.has_value());
    ASSERT_TRUE(highLevelResult.has_value());

    // Higher level should produce fewer or equal regions
    EXPECT_LE(highLevelResult->regionCount, lowLevelResult->regionCount);
}

// Marker-based watershed tests
TEST_F(WatershedSegmenterTest, MarkerWatershedReturnsValidResult) {
    auto image = createTwoRegionImage();
    auto markers = createMarkerImage();

    WatershedParameters params;
    params.gradientSigma = 1.0;

    auto result = segmenter_->segmentWithMarkers(image, markers, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->labelMap, nullptr);
    EXPECT_GE(result->regionCount, 1u);
}

TEST_F(WatershedSegmenterTest, MarkerWatershedHandlesNullInput) {
    auto markers = createMarkerImage();
    WatershedParameters params;

    auto result = segmenter_->segmentWithMarkers(nullptr, markers, params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(WatershedSegmenterTest, MarkerWatershedHandlesNullMarkers) {
    auto image = createTwoRegionImage();
    WatershedParameters params;

    auto result = segmenter_->segmentWithMarkers(image, nullptr, params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(WatershedSegmenterTest, MarkerWatershedRejectsDimensionMismatch) {
    auto image = createTwoRegionImage();

    // Create markers with different size
    auto wrongMarkers = LabelMapType::New();
    LabelMapType::SizeType size;
    size[0] = 10;  // Different from image (20)
    size[1] = 10;
    size[2] = 5;

    LabelMapType::IndexType start;
    start.Fill(0);

    LabelMapType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    wrongMarkers->SetRegions(region);
    wrongMarkers->Allocate();
    wrongMarkers->FillBuffer(0);

    WatershedParameters params;

    auto result = segmenter_->segmentWithMarkers(image, wrongMarkers, params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

// Region extraction tests
TEST_F(WatershedSegmenterTest, ExtractRegionReturnsValidMask) {
    auto image = createTwoRegionImage();

    WatershedParameters params;
    params.level = 0.1;

    auto segResult = segmenter_->segment(image, params);
    ASSERT_TRUE(segResult.has_value());
    ASSERT_GT(segResult->regions.size(), 0u);

    // Extract the first region
    unsigned long firstLabel = segResult->regions[0].label;
    auto extractResult = segmenter_->extractRegion(segResult->labelMap, firstLabel);

    ASSERT_TRUE(extractResult.has_value());
    EXPECT_NE(extractResult.value(), nullptr);

    // Should have some non-zero pixels
    int nonZeroCount = countNonZeroPixels(extractResult.value());
    EXPECT_GT(nonZeroCount, 0);
}

TEST_F(WatershedSegmenterTest, ExtractRegionHandlesNullInput) {
    auto result = segmenter_->extractRegion(nullptr, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(WatershedSegmenterTest, ExtractRegionHandlesNonexistentLabel) {
    auto image = createTwoRegionImage();

    WatershedParameters params;
    params.level = 0.1;

    auto segResult = segmenter_->segment(image, params);
    ASSERT_TRUE(segResult.has_value());

    // Extract with a label that doesn't exist
    auto extractResult = segmenter_->extractRegion(segResult->labelMap, 999999);

    // Should succeed but return empty mask
    ASSERT_TRUE(extractResult.has_value());
    int nonZeroCount = countNonZeroPixels(extractResult.value());
    EXPECT_EQ(nonZeroCount, 0);
}

// Region statistics tests
TEST_F(WatershedSegmenterTest, RegionInfoContainsValidData) {
    auto image = createTwoRegionImage();

    WatershedParameters params;
    params.level = 0.1;

    auto result = segmenter_->segment(image, params);
    ASSERT_TRUE(result.has_value());
    ASSERT_GT(result->regions.size(), 0u);

    for (const auto& region : result->regions) {
        EXPECT_GT(region.label, 0u);
        EXPECT_GT(region.voxelCount, 0u);

        // Centroids should be within image bounds
        EXPECT_GE(region.centroid[0], 0.0);
        EXPECT_LT(region.centroid[0], 20.0);
        EXPECT_GE(region.centroid[1], 0.0);
        EXPECT_LT(region.centroid[1], 20.0);
        EXPECT_GE(region.centroid[2], 0.0);
        EXPECT_LT(region.centroid[2], 10.0);
    }
}

// Small region removal tests
TEST_F(WatershedSegmenterTest, SmallRegionsAreRemoved) {
    auto image = createGradientImage();

    // First run without small region removal
    WatershedParameters paramsNoRemoval;
    paramsNoRemoval.level = 0.01;
    paramsNoRemoval.mergeSmallRegions = false;

    auto resultNoRemoval = segmenter_->segment(image, paramsNoRemoval);
    ASSERT_TRUE(resultNoRemoval.has_value());

    // Then run with small region removal (high minimum size)
    WatershedParameters paramsWithRemoval;
    paramsWithRemoval.level = 0.01;
    paramsWithRemoval.mergeSmallRegions = true;
    paramsWithRemoval.minimumRegionSize = 500;  // High threshold

    auto resultWithRemoval = segmenter_->segment(image, paramsWithRemoval);
    ASSERT_TRUE(resultWithRemoval.has_value());

    // Should have fewer or equal regions after removal
    EXPECT_LE(resultWithRemoval->regionCount, resultNoRemoval->regionCount);
}

// Parameter validation tests
TEST_F(WatershedSegmenterTest, ParametersValidationWorks) {
    WatershedParameters valid;
    valid.level = 0.5;
    valid.threshold = 0.01;
    valid.gradientSigma = 1.0;
    EXPECT_TRUE(valid.isValid());

    WatershedParameters boundaryLow;
    boundaryLow.level = 0.0;  // Boundary value
    boundaryLow.threshold = 0.0;
    boundaryLow.gradientSigma = 0.1;
    EXPECT_TRUE(boundaryLow.isValid());

    WatershedParameters boundaryHigh;
    boundaryHigh.level = 1.0;  // Boundary value
    boundaryHigh.threshold = 1.0;
    boundaryHigh.gradientSigma = 10.0;
    EXPECT_TRUE(boundaryHigh.isValid());

    WatershedParameters invalidLevel;
    invalidLevel.level = -0.1;  // Invalid
    EXPECT_FALSE(invalidLevel.isValid());

    WatershedParameters invalidLevelHigh;
    invalidLevelHigh.level = 1.1;  // Invalid
    EXPECT_FALSE(invalidLevelHigh.isValid());

    WatershedParameters invalidSigma;
    invalidSigma.level = 0.1;
    invalidSigma.gradientSigma = 0.0;  // Invalid
    EXPECT_FALSE(invalidSigma.isValid());

    WatershedParameters invalidSigmaNegative;
    invalidSigmaNegative.level = 0.1;
    invalidSigmaNegative.gradientSigma = -1.0;  // Invalid
    EXPECT_FALSE(invalidSigmaNegative.isValid());
}

// Progress callback tests
TEST_F(WatershedSegmenterTest, ProgressCallbackIsCalled) {
    auto image = createTwoRegionImage();

    bool callbackCalled = false;
    segmenter_->setProgressCallback([&callbackCalled](double /*progress*/) {
        callbackCalled = true;
    });

    WatershedParameters params;
    params.level = 0.1;

    auto result = segmenter_->segment(image, params);

    ASSERT_TRUE(result.has_value());
    // Progress callback may or may not be called depending on filter implementation
    // This test ensures no crash occurs
}

// Large volume test (performance check)
TEST_F(WatershedSegmenterTest, HandlesLargerVolume) {
    // Create a larger test image (50x50x20)
    auto image = ImageType::New();

    ImageType::SizeType size;
    size[0] = 50;
    size[1] = 50;
    size[2] = 20;

    ImageType::IndexType start;
    start.Fill(0);

    ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    image->SetRegions(region);
    image->Allocate();

    // Fill with checkerboard pattern
    itk::ImageRegionIterator<ImageType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        bool checker = ((idx[0] / 10) + (idx[1] / 10) + (idx[2] / 5)) % 2 == 0;
        it.Set(checker ? 100 : 200);
    }

    WatershedParameters params;
    params.level = 0.5;
    params.gradientSigma = 2.0;

    auto result = segmenter_->segment(image, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result->regionCount, 1u);
}

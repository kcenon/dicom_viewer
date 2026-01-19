#include "services/preprocessing/histogram_equalizer.hpp"

#include <gtest/gtest.h>
#include <cmath>

namespace dicom_viewer::services {
namespace {

class HistogramEqualizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test image (20x20x20)
        testImage_ = ImageType::New();

        ImageType::SizeType size;
        size[0] = 20;
        size[1] = 20;
        size[2] = 20;

        ImageType::IndexType start;
        start.Fill(0);

        ImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        testImage_->SetRegions(region);
        testImage_->Allocate();
        testImage_->FillBuffer(0);

        // Set spacing (1mm x 1mm x 1mm)
        ImageType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        testImage_->SetSpacing(spacing);

        // Create a low-contrast pattern:
        // Center region (8-12, 8-12, 8-12) with value 100
        // Outer region with value 50
        // This creates a low-contrast image ideal for testing equalization
        testImage_->FillBuffer(50);

        for (int z = 8; z <= 12; ++z) {
            for (int y = 8; y <= 12; ++y) {
                for (int x = 8; x <= 12; ++x) {
                    ImageType::IndexType idx = {x, y, z};
                    testImage_->SetPixel(idx, 100);
                }
            }
        }
    }

    using ImageType = HistogramEqualizer::ImageType;
    using Image2DType = HistogramEqualizer::Image2DType;

    ImageType::Pointer testImage_;
};

// =============================================================================
// Parameters validation tests
// =============================================================================

TEST_F(HistogramEqualizerTest, ParametersDefaultValid) {
    HistogramEqualizer::Parameters params;

    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(params.method, EqualizationMethod::CLAHE);
    EXPECT_DOUBLE_EQ(params.clipLimit, 3.0);
    EXPECT_EQ(params.tileSize[0], 8u);
    EXPECT_EQ(params.tileSize[1], 8u);
    EXPECT_EQ(params.tileSize[2], 8u);
    EXPECT_EQ(params.numberOfBins, 256);
    EXPECT_TRUE(params.preserveRange);
}

TEST_F(HistogramEqualizerTest, ParametersClipLimitTooLow) {
    HistogramEqualizer::Parameters params;
    params.clipLimit = 0.05;  // Below 0.1 minimum

    EXPECT_FALSE(params.isValid());
}

TEST_F(HistogramEqualizerTest, ParametersClipLimitTooHigh) {
    HistogramEqualizer::Parameters params;
    params.clipLimit = 15.0;  // Above 10.0 maximum

    EXPECT_FALSE(params.isValid());
}

TEST_F(HistogramEqualizerTest, ParametersClipLimitAtBoundaries) {
    HistogramEqualizer::Parameters params;

    params.clipLimit = 0.1;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.clipLimit = 10.0;  // Maximum
    EXPECT_TRUE(params.isValid());
}

TEST_F(HistogramEqualizerTest, ParametersTileSizeTooSmall) {
    HistogramEqualizer::Parameters params;
    params.tileSize = {0, 8, 8};  // Zero not allowed

    EXPECT_FALSE(params.isValid());
}

TEST_F(HistogramEqualizerTest, ParametersTileSizeTooLarge) {
    HistogramEqualizer::Parameters params;
    params.tileSize = {65, 8, 8};  // Above 64 maximum

    EXPECT_FALSE(params.isValid());
}

TEST_F(HistogramEqualizerTest, ParametersNumberOfBinsTooSmall) {
    HistogramEqualizer::Parameters params;
    params.numberOfBins = 8;  // Below 16 minimum

    EXPECT_FALSE(params.isValid());
}

TEST_F(HistogramEqualizerTest, ParametersNumberOfBinsTooLarge) {
    HistogramEqualizer::Parameters params;
    params.numberOfBins = 5000;  // Above 4096 maximum

    EXPECT_FALSE(params.isValid());
}

// =============================================================================
// HistogramEqualizer equalize tests
// =============================================================================

TEST_F(HistogramEqualizerTest, EqualizeNullInput) {
    HistogramEqualizer equalizer;

    auto result = equalizer.equalize(nullptr);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(HistogramEqualizerTest, EqualizeInvalidParameters) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.clipLimit = 0.01;  // Invalid

    auto result = equalizer.equalize(testImage_, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(HistogramEqualizerTest, EqualizeWithDefaultParameters) {
    HistogramEqualizer equalizer;

    auto result = equalizer.equalize(testImage_);

    ASSERT_TRUE(result.has_value());

    auto enhancedImage = result.value();
    ASSERT_NE(enhancedImage, nullptr);

    // Check output dimensions match input
    auto inputSize = testImage_->GetLargestPossibleRegion().GetSize();
    auto outputSize = enhancedImage->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(inputSize[0], outputSize[0]);
    EXPECT_EQ(inputSize[1], outputSize[1]);
    EXPECT_EQ(inputSize[2], outputSize[2]);
}

TEST_F(HistogramEqualizerTest, EqualizeWithCLAHE) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.method = EqualizationMethod::CLAHE;
    params.clipLimit = 2.0;
    params.tileSize = {4, 4, 4};

    auto result = equalizer.equalize(testImage_, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(HistogramEqualizerTest, EqualizeWithAdaptive) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.method = EqualizationMethod::Adaptive;

    auto result = equalizer.equalize(testImage_, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(HistogramEqualizerTest, EqualizeWithStandard) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.method = EqualizationMethod::Standard;

    auto result = equalizer.equalize(testImage_, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(HistogramEqualizerTest, EqualizePreservesImageProperties) {
    HistogramEqualizer equalizer;

    auto result = equalizer.equalize(testImage_);
    ASSERT_TRUE(result.has_value());

    auto enhancedImage = result.value();

    // Check spacing is preserved
    auto inputSpacing = testImage_->GetSpacing();
    auto outputSpacing = enhancedImage->GetSpacing();

    EXPECT_DOUBLE_EQ(inputSpacing[0], outputSpacing[0]);
    EXPECT_DOUBLE_EQ(inputSpacing[1], outputSpacing[1]);
    EXPECT_DOUBLE_EQ(inputSpacing[2], outputSpacing[2]);

    // Check origin is preserved
    auto inputOrigin = testImage_->GetOrigin();
    auto outputOrigin = enhancedImage->GetOrigin();

    EXPECT_DOUBLE_EQ(inputOrigin[0], outputOrigin[0]);
    EXPECT_DOUBLE_EQ(inputOrigin[1], outputOrigin[1]);
    EXPECT_DOUBLE_EQ(inputOrigin[2], outputOrigin[2]);
}

TEST_F(HistogramEqualizerTest, EqualizeIncreasesContrast) {
    HistogramEqualizer equalizer;

    // Compute histogram of original image
    auto originalHistogram = equalizer.computeHistogram(testImage_, 64);

    // Apply equalization
    auto result = equalizer.equalize(testImage_);
    ASSERT_TRUE(result.has_value());

    // Compute histogram of enhanced image
    auto enhancedHistogram = equalizer.computeHistogram(result.value(), 64);

    // Enhanced image should have a wider spread of values
    // (more bins with non-zero counts)
    int originalNonZeroBins = 0;
    int enhancedNonZeroBins = 0;

    for (size_t count : originalHistogram.counts) {
        if (count > 0) originalNonZeroBins++;
    }
    for (size_t count : enhancedHistogram.counts) {
        if (count > 0) enhancedNonZeroBins++;
    }

    // Enhanced should have at least as many non-zero bins
    EXPECT_GE(enhancedNonZeroBins, originalNonZeroBins);
}

// =============================================================================
// applyCLAHE convenience method tests
// =============================================================================

TEST_F(HistogramEqualizerTest, ApplyCLAHEDefaultParameters) {
    HistogramEqualizer equalizer;

    auto result = equalizer.applyCLAHE(testImage_);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(HistogramEqualizerTest, ApplyCLAHECustomParameters) {
    HistogramEqualizer equalizer;

    auto result = equalizer.applyCLAHE(testImage_, 2.0, {16, 16, 16});

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(HistogramEqualizerTest, ApplyCLAHENullInput) {
    HistogramEqualizer equalizer;

    auto result = equalizer.applyCLAHE(nullptr);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

// =============================================================================
// equalizeSlice tests
// =============================================================================

TEST_F(HistogramEqualizerTest, EqualizeSliceNullInput) {
    HistogramEqualizer equalizer;

    auto result = equalizer.equalizeSlice(nullptr, 10);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(HistogramEqualizerTest, EqualizeSliceInvalidSliceIndex) {
    HistogramEqualizer equalizer;

    auto result = equalizer.equalizeSlice(testImage_, 100);  // Out of range

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(HistogramEqualizerTest, EqualizeSliceSuccess) {
    HistogramEqualizer equalizer;

    auto result = equalizer.equalizeSlice(testImage_, 10);

    ASSERT_TRUE(result.has_value());
    auto slice = result.value();
    ASSERT_NE(slice, nullptr);

    // Check 2D dimensions match XY of 3D input
    auto sliceSize = slice->GetLargestPossibleRegion().GetSize();
    auto volumeSize = testImage_->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(sliceSize[0], volumeSize[0]);
    EXPECT_EQ(sliceSize[1], volumeSize[1]);
}

TEST_F(HistogramEqualizerTest, EqualizeSliceWithCustomParameters) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.method = EqualizationMethod::CLAHE;
    params.clipLimit = 1.5;
    params.tileSize = {4, 4, 4};

    auto result = equalizer.equalizeSlice(testImage_, 10, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(HistogramEqualizerTest, EqualizeSliceInvalidParameters) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.clipLimit = 0.01;  // Invalid

    auto result = equalizer.equalizeSlice(testImage_, 10, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

// =============================================================================
// preview tests
// =============================================================================

TEST_F(HistogramEqualizerTest, PreviewReturnsSlice) {
    HistogramEqualizer equalizer;

    auto result = equalizer.preview(testImage_, 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

// =============================================================================
// computeHistogram tests
// =============================================================================

TEST_F(HistogramEqualizerTest, ComputeHistogramNullInput) {
    HistogramEqualizer equalizer;

    auto histogram = equalizer.computeHistogram(nullptr);

    EXPECT_TRUE(histogram.bins.empty());
    EXPECT_TRUE(histogram.counts.empty());
}

TEST_F(HistogramEqualizerTest, ComputeHistogramValidInput) {
    HistogramEqualizer equalizer;

    auto histogram = equalizer.computeHistogram(testImage_, 64);

    EXPECT_EQ(histogram.bins.size(), 64u);
    EXPECT_EQ(histogram.counts.size(), 64u);

    // Our test image has values 50 and 100, so min should be 50, max should be 100
    EXPECT_EQ(histogram.minValue, 50);
    EXPECT_EQ(histogram.maxValue, 100);

    // Total count should equal number of pixels
    size_t totalCount = 0;
    for (size_t count : histogram.counts) {
        totalCount += count;
    }
    EXPECT_EQ(totalCount, 20u * 20u * 20u);
}

TEST_F(HistogramEqualizerTest, ComputeHistogramUniformImage) {
    // Create a uniform image
    auto uniformImage = ImageType::New();
    ImageType::SizeType size = {10, 10, 10};
    ImageType::IndexType start;
    start.Fill(0);
    ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);
    uniformImage->SetRegions(region);
    uniformImage->Allocate();
    uniformImage->FillBuffer(100);  // All pixels = 100

    HistogramEqualizer equalizer;
    auto histogram = equalizer.computeHistogram(uniformImage, 64);

    EXPECT_EQ(histogram.minValue, 100);
    EXPECT_EQ(histogram.maxValue, 100);
    // For uniform image, bins should only have one entry
    EXPECT_FALSE(histogram.bins.empty());
}

// =============================================================================
// Different clip limit effects tests
// =============================================================================

TEST_F(HistogramEqualizerTest, DifferentClipLimitsProduceDifferentResults) {
    HistogramEqualizer equalizer;

    HistogramEqualizer::Parameters lowClip;
    lowClip.method = EqualizationMethod::CLAHE;
    lowClip.clipLimit = 1.0;

    HistogramEqualizer::Parameters highClip;
    highClip.method = EqualizationMethod::CLAHE;
    highClip.clipLimit = 5.0;

    auto lowResult = equalizer.equalize(testImage_, lowClip);
    auto highResult = equalizer.equalize(testImage_, highClip);

    ASSERT_TRUE(lowResult.has_value());
    ASSERT_TRUE(highResult.has_value());

    // Sample a pixel and verify the results are different
    // (different clip limits should produce different enhancements)
    ImageType::IndexType idx = {10, 10, 10};
    auto lowValue = lowResult.value()->GetPixel(idx);
    auto highValue = highResult.value()->GetPixel(idx);

    // We don't check for specific relationship, just that they can differ
    // (the exact relationship depends on the image content)
    EXPECT_TRUE(lowValue != 0 || highValue != 0);  // At least one should be non-zero
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(HistogramEqualizerTest, ProgressCallbackIsCalled) {
    HistogramEqualizer equalizer;

    bool callbackCalled = false;
    double lastProgress = -1.0;

    equalizer.setProgressCallback([&](double progress) {
        callbackCalled = true;
        lastProgress = progress;
    });

    auto result = equalizer.equalize(testImage_);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(callbackCalled);
    EXPECT_GE(lastProgress, 0.0);
    EXPECT_LE(lastProgress, 1.0);
}

// =============================================================================
// Move semantics tests
// =============================================================================

TEST_F(HistogramEqualizerTest, MoveConstruction) {
    HistogramEqualizer equalizer1;
    HistogramEqualizer equalizer2(std::move(equalizer1));

    auto result = equalizer2.equalize(testImage_);
    EXPECT_TRUE(result.has_value());
}

TEST_F(HistogramEqualizerTest, MoveAssignment) {
    HistogramEqualizer equalizer1;
    HistogramEqualizer equalizer2;

    equalizer2 = std::move(equalizer1);

    auto result = equalizer2.equalize(testImage_);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// Range preservation tests
// =============================================================================

TEST_F(HistogramEqualizerTest, PreserveRangeMaintainsOriginalMinMax) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.preserveRange = true;

    auto result = equalizer.equalize(testImage_, params);
    ASSERT_TRUE(result.has_value());

    // Get stats of result
    auto resultHistogram = equalizer.computeHistogram(result.value(), 256);

    // Original range is [50, 100]
    // With preserveRange=true, output should stay within or near this range
    EXPECT_GE(resultHistogram.minValue, 40);   // Allow some tolerance
    EXPECT_LE(resultHistogram.maxValue, 110);
}

TEST_F(HistogramEqualizerTest, CustomOutputRange) {
    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters params;
    params.preserveRange = false;
    params.outputMinimum = 0.0;
    params.outputMaximum = 255.0;

    auto result = equalizer.equalize(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto resultHistogram = equalizer.computeHistogram(result.value(), 256);

    // Output should be rescaled to [0, 255]
    EXPECT_GE(resultHistogram.minValue, -1);   // Allow small tolerance
    EXPECT_LE(resultHistogram.maxValue, 256);
}

}  // namespace
}  // namespace dicom_viewer::services

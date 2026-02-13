#include "services/preprocessing/anisotropic_diffusion_filter.hpp"

#include <gtest/gtest.h>
#include <cmath>

namespace dicom_viewer::services {
namespace {

class AnisotropicDiffusionFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test image (20x20x20)
        testImage_ = InputImageType::New();

        InputImageType::SizeType size;
        size[0] = 20;
        size[1] = 20;
        size[2] = 20;

        InputImageType::IndexType start;
        start.Fill(0);

        InputImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        testImage_->SetRegions(region);
        testImage_->Allocate();
        testImage_->FillBuffer(0);

        // Set spacing (1mm x 1mm x 1mm)
        InputImageType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        testImage_->SetSpacing(spacing);

        // Create a high-contrast pattern with edge:
        // Center cube (8-12, 8-12, 8-12) with value 1000
        // Surrounding area with value 0
        // This tests edge-preserving behavior
        for (int z = 8; z <= 12; ++z) {
            for (int y = 8; y <= 12; ++y) {
                for (int x = 8; x <= 12; ++x) {
                    InputImageType::IndexType idx = {x, y, z};
                    testImage_->SetPixel(idx, 1000);
                }
            }
        }

        // Add some noise to surrounding area
        for (int z = 0; z < 8; ++z) {
            for (int y = 0; y < 20; ++y) {
                for (int x = 0; x < 20; ++x) {
                    if ((x + y + z) % 3 == 0) {
                        InputImageType::IndexType idx = {x, y, z};
                        testImage_->SetPixel(idx, 50);  // Small noise
                    }
                }
            }
        }
    }

    using InputImageType = AnisotropicDiffusionFilter::InputImageType;
    using Input2DImageType = AnisotropicDiffusionFilter::Input2DImageType;

    InputImageType::Pointer testImage_;
};

// =============================================================================
// Parameters validation tests
// =============================================================================

TEST_F(AnisotropicDiffusionFilterTest, ParametersDefaultValid) {
    AnisotropicDiffusionFilter::Parameters params;

    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(params.numberOfIterations, 10);
    EXPECT_DOUBLE_EQ(params.conductance, 3.0);
    EXPECT_DOUBLE_EQ(params.timeStep, 0.0);
    EXPECT_TRUE(params.useImageSpacing);
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersIterationsTooLow) {
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 0;  // Below 1 minimum

    EXPECT_FALSE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersIterationsTooHigh) {
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 51;  // Above 50 maximum

    EXPECT_FALSE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersIterationsAtBoundaries) {
    AnisotropicDiffusionFilter::Parameters params;

    params.numberOfIterations = 1;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.numberOfIterations = 50;  // Maximum
    EXPECT_TRUE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersConductanceTooLow) {
    AnisotropicDiffusionFilter::Parameters params;
    params.conductance = 0.3;  // Below 0.5 minimum

    EXPECT_FALSE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersConductanceTooHigh) {
    AnisotropicDiffusionFilter::Parameters params;
    params.conductance = 11.0;  // Above 10.0 maximum

    EXPECT_FALSE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersConductanceAtBoundaries) {
    AnisotropicDiffusionFilter::Parameters params;

    params.conductance = 0.5;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.conductance = 10.0;  // Maximum
    EXPECT_TRUE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersTimeStepNegative) {
    AnisotropicDiffusionFilter::Parameters params;
    params.timeStep = -0.1;

    EXPECT_FALSE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersTimeStepTooHigh) {
    AnisotropicDiffusionFilter::Parameters params;
    params.timeStep = 0.2;  // Above 0.125 maximum for 3D

    EXPECT_FALSE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, ParametersTimeStepValid) {
    AnisotropicDiffusionFilter::Parameters params;

    params.timeStep = 0.0;  // Automatic
    EXPECT_TRUE(params.isValid());

    params.timeStep = 0.0625;  // Default safe value
    EXPECT_TRUE(params.isValid());

    params.timeStep = 0.125;  // Maximum stable
    EXPECT_TRUE(params.isValid());
}

TEST_F(AnisotropicDiffusionFilterTest, GetDefaultTimeStep) {
    double defaultStep = AnisotropicDiffusionFilter::Parameters::getDefaultTimeStep();

    EXPECT_DOUBLE_EQ(defaultStep, 0.0625);
}

// =============================================================================
// AnisotropicDiffusionFilter apply tests
// =============================================================================

TEST_F(AnisotropicDiffusionFilterTest, ApplyNullInput) {
    AnisotropicDiffusionFilter filter;

    auto result = filter.apply(nullptr);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyInvalidParameters) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 0;  // Invalid

    auto result = filter.apply(testImage_, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyWithDefaultParameters) {
    AnisotropicDiffusionFilter filter;

    auto result = filter.apply(testImage_);

    ASSERT_TRUE(result.has_value());

    auto filteredImage = result.value();
    ASSERT_NE(filteredImage, nullptr);

    // Check output dimensions match input
    auto inputSize = testImage_->GetLargestPossibleRegion().GetSize();
    auto outputSize = filteredImage->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(inputSize[0], outputSize[0]);
    EXPECT_EQ(inputSize[1], outputSize[1]);
    EXPECT_EQ(inputSize[2], outputSize[2]);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyWithCustomParameters) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 15;
    params.conductance = 3.0;
    params.timeStep = 0.05;
    params.useImageSpacing = true;

    auto result = filter.apply(testImage_, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyPreservesImageProperties) {
    AnisotropicDiffusionFilter filter;

    auto result = filter.apply(testImage_);
    ASSERT_TRUE(result.has_value());

    auto filteredImage = result.value();

    // Check spacing is preserved
    auto inputSpacing = testImage_->GetSpacing();
    auto outputSpacing = filteredImage->GetSpacing();

    EXPECT_DOUBLE_EQ(inputSpacing[0], outputSpacing[0]);
    EXPECT_DOUBLE_EQ(inputSpacing[1], outputSpacing[1]);
    EXPECT_DOUBLE_EQ(inputSpacing[2], outputSpacing[2]);

    // Check origin is preserved
    auto inputOrigin = testImage_->GetOrigin();
    auto outputOrigin = filteredImage->GetOrigin();

    EXPECT_DOUBLE_EQ(inputOrigin[0], outputOrigin[0]);
    EXPECT_DOUBLE_EQ(inputOrigin[1], outputOrigin[1]);
    EXPECT_DOUBLE_EQ(inputOrigin[2], outputOrigin[2]);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyReducesNoise) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 10;
    params.conductance = 3.0;

    auto result = filter.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto filteredImage = result.value();

    // Check that noise in homogeneous region is reduced
    // Sample point in the noisy area (z < 8)
    double sumNoiseBefore = 0;
    double sumNoiseAfter = 0;
    int count = 0;

    for (int z = 2; z < 6; ++z) {
        for (int y = 2; y < 6; ++y) {
            for (int x = 2; x < 6; ++x) {
                InputImageType::IndexType idx = {x, y, z};
                sumNoiseBefore += std::abs(testImage_->GetPixel(idx));
                sumNoiseAfter += std::abs(filteredImage->GetPixel(idx));
                ++count;
            }
        }
    }

    double avgBefore = sumNoiseBefore / count;
    double avgAfter = sumNoiseAfter / count;

    // After diffusion, variations should be smoothed
    // The average should be closer to a uniform value
    EXPECT_LE(avgAfter, avgBefore + 10);  // Should not increase significantly
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyPreservesEdges) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 5;
    params.conductance = 1.0;  // Lower conductance = more edge preservation

    auto result = filter.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto filteredImage = result.value();

    // Center of high-contrast region should remain high
    InputImageType::IndexType centerIdx = {10, 10, 10};
    auto originalCenter = testImage_->GetPixel(centerIdx);
    auto filteredCenter = filteredImage->GetPixel(centerIdx);

    // Edge-preserving should keep center value relatively high
    EXPECT_GT(filteredCenter, originalCenter * 0.5);

    // Background should remain relatively low
    InputImageType::IndexType bgIdx = {0, 0, 0};
    auto filteredBg = filteredImage->GetPixel(bgIdx);

    EXPECT_LT(filteredBg, filteredCenter);
}

TEST_F(AnisotropicDiffusionFilterTest, MoreIterationsMoreSmoothing) {
    AnisotropicDiffusionFilter filter;

    AnisotropicDiffusionFilter::Parameters lowIter;
    lowIter.numberOfIterations = 2;
    lowIter.conductance = 3.0;

    AnisotropicDiffusionFilter::Parameters highIter;
    highIter.numberOfIterations = 20;
    highIter.conductance = 3.0;

    auto lowResult = filter.apply(testImage_, lowIter);
    auto highResult = filter.apply(testImage_, highIter);

    ASSERT_TRUE(lowResult.has_value());
    ASSERT_TRUE(highResult.has_value());

    // Measure variance in a homogeneous region
    double sumLow = 0, sumHigh = 0;
    double sumLowSq = 0, sumHighSq = 0;
    int count = 0;

    for (int z = 2; z < 6; ++z) {
        for (int y = 2; y < 6; ++y) {
            for (int x = 2; x < 6; ++x) {
                InputImageType::IndexType idx = {x, y, z};
                double lowVal = lowResult.value()->GetPixel(idx);
                double highVal = highResult.value()->GetPixel(idx);
                sumLow += lowVal;
                sumHigh += highVal;
                sumLowSq += lowVal * lowVal;
                sumHighSq += highVal * highVal;
                ++count;
            }
        }
    }

    double varLow = (sumLowSq / count) - (sumLow / count) * (sumLow / count);
    double varHigh = (sumHighSq / count) - (sumHigh / count) * (sumHigh / count);

    // Higher iterations should result in lower variance (more uniform)
    EXPECT_LE(varHigh, varLow + 1.0);
}

// =============================================================================
// applyToSlice tests
// =============================================================================

TEST_F(AnisotropicDiffusionFilterTest, ApplyToSliceNullInput) {
    AnisotropicDiffusionFilter filter;

    auto result = filter.applyToSlice(nullptr, 10);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyToSliceInvalidSliceIndex) {
    AnisotropicDiffusionFilter filter;

    auto result = filter.applyToSlice(testImage_, 100);  // Out of range

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyToSliceSuccess) {
    AnisotropicDiffusionFilter filter;

    auto result = filter.applyToSlice(testImage_, 10);

    ASSERT_TRUE(result.has_value());
    auto slice = result.value();
    ASSERT_NE(slice, nullptr);

    // Check 2D dimensions match XY of 3D input
    auto sliceSize = slice->GetLargestPossibleRegion().GetSize();
    auto volumeSize = testImage_->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(sliceSize[0], volumeSize[0]);
    EXPECT_EQ(sliceSize[1], volumeSize[1]);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyToSliceWithCustomParameters) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 5;
    params.conductance = 2.0;

    auto result = filter.applyToSlice(testImage_, 10, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(AnisotropicDiffusionFilterTest, ApplyToSliceInvalidParameters) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.conductance = 0.1;  // Invalid (below 0.5)

    auto result = filter.applyToSlice(testImage_, 10, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

// =============================================================================
// estimateProcessingTime tests
// =============================================================================

TEST_F(AnisotropicDiffusionFilterTest, EstimateProcessingTimeBasic) {
    std::array<unsigned int, 3> imageSize = {256, 256, 100};
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 10;

    double estimate = AnisotropicDiffusionFilter::estimateProcessingTime(imageSize, params);

    EXPECT_GT(estimate, 0.0);
}

TEST_F(AnisotropicDiffusionFilterTest, EstimateProcessingTimeScalesWithSize) {
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 10;

    std::array<unsigned int, 3> smallSize = {64, 64, 64};
    std::array<unsigned int, 3> largeSize = {256, 256, 256};

    double smallEstimate = AnisotropicDiffusionFilter::estimateProcessingTime(smallSize, params);
    double largeEstimate = AnisotropicDiffusionFilter::estimateProcessingTime(largeSize, params);

    // Large image should take longer
    EXPECT_GT(largeEstimate, smallEstimate);

    // Should scale roughly with volume
    double volumeRatio = (256.0 * 256.0 * 256.0) / (64.0 * 64.0 * 64.0);
    double timeRatio = largeEstimate / smallEstimate;

    EXPECT_NEAR(timeRatio, volumeRatio, volumeRatio * 0.1);
}

TEST_F(AnisotropicDiffusionFilterTest, EstimateProcessingTimeScalesWithIterations) {
    std::array<unsigned int, 3> imageSize = {128, 128, 128};

    AnisotropicDiffusionFilter::Parameters lowIter;
    lowIter.numberOfIterations = 5;

    AnisotropicDiffusionFilter::Parameters highIter;
    highIter.numberOfIterations = 20;

    double lowEstimate = AnisotropicDiffusionFilter::estimateProcessingTime(imageSize, lowIter);
    double highEstimate = AnisotropicDiffusionFilter::estimateProcessingTime(imageSize, highIter);

    // More iterations should take longer
    EXPECT_GT(highEstimate, lowEstimate);

    // Should scale linearly with iterations
    double iterRatio = 20.0 / 5.0;
    double timeRatio = highEstimate / lowEstimate;

    EXPECT_NEAR(timeRatio, iterRatio, iterRatio * 0.1);
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(AnisotropicDiffusionFilterTest, ProgressCallbackIsCalled) {
    AnisotropicDiffusionFilter filter;

    bool callbackCalled = false;
    double lastProgress = -1.0;

    filter.setProgressCallback([&](double progress) {
        callbackCalled = true;
        lastProgress = progress;
    });

    auto result = filter.apply(testImage_);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(callbackCalled);
    EXPECT_GE(lastProgress, 0.0);
    EXPECT_LE(lastProgress, 1.0);
}

// =============================================================================
// Move semantics tests
// =============================================================================

TEST_F(AnisotropicDiffusionFilterTest, MoveConstruction) {
    AnisotropicDiffusionFilter filter1;
    AnisotropicDiffusionFilter filter2(std::move(filter1));

    auto result = filter2.apply(testImage_);
    EXPECT_TRUE(result.has_value());
}

TEST_F(AnisotropicDiffusionFilterTest, MoveAssignment) {
    AnisotropicDiffusionFilter filter1;
    AnisotropicDiffusionFilter filter2;

    filter2 = std::move(filter1);

    auto result = filter2.apply(testImage_);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// Filter accuracy and edge case tests
// =============================================================================

TEST_F(AnisotropicDiffusionFilterTest, StepEdgeContrastPreservedAfterDiffusion) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 5;
    params.conductance = 3.0;

    auto result = filter.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto output = result.value();

    // The step edge between background (0) and cube (1000) should be preserved
    InputImageType::IndexType insideCube = {10, 10, 10};
    InputImageType::IndexType outsideCube = {2, 2, 2};

    short insideVal = output->GetPixel(insideCube);
    short outsideVal = output->GetPixel(outsideCube);

    // Edge-preserving: contrast should remain significant
    double contrast = static_cast<double>(insideVal - outsideVal);
    EXPECT_GT(contrast, 500.0);  // Original contrast 1000, expect >50% preserved
}

TEST_F(AnisotropicDiffusionFilterTest, HomogeneousRegionNoiseReduced) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 10;
    params.conductance = 3.0;

    auto result = filter.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto output = result.value();

    // Compute variance in the noisy region (z < 6, outside the cube area)
    auto computeVariance = [](InputImageType::Pointer image) {
        double sum = 0.0;
        int count = 0;
        for (int z = 0; z < 6; ++z) {
            for (int y = 0; y < 6; ++y) {
                for (int x = 0; x < 6; ++x) {
                    InputImageType::IndexType idx = {x, y, z};
                    sum += image->GetPixel(idx);
                    count++;
                }
            }
        }
        double mean = sum / count;
        double variance = 0.0;
        for (int z = 0; z < 6; ++z) {
            for (int y = 0; y < 6; ++y) {
                for (int x = 0; x < 6; ++x) {
                    InputImageType::IndexType idx = {x, y, z};
                    double diff = image->GetPixel(idx) - mean;
                    variance += diff * diff;
                }
            }
        }
        return variance / count;
    };

    double inputVariance = computeVariance(testImage_);
    double outputVariance = computeVariance(output);

    // Diffusion should reduce noise (lower variance in homogeneous region)
    EXPECT_LT(outputVariance, inputVariance);
}

TEST_F(AnisotropicDiffusionFilterTest, MoreIterationsProducesSmootherResult) {
    AnisotropicDiffusionFilter filter;

    auto computeVariance = [](InputImageType::Pointer image) {
        double sum = 0.0;
        int count = 0;
        for (int z = 0; z < 6; ++z) {
            for (int y = 0; y < 6; ++y) {
                for (int x = 0; x < 6; ++x) {
                    InputImageType::IndexType idx = {x, y, z};
                    sum += image->GetPixel(idx);
                    count++;
                }
            }
        }
        double mean = sum / count;
        double variance = 0.0;
        for (int z = 0; z < 6; ++z) {
            for (int y = 0; y < 6; ++y) {
                for (int x = 0; x < 6; ++x) {
                    InputImageType::IndexType idx = {x, y, z};
                    double diff = image->GetPixel(idx) - mean;
                    variance += diff * diff;
                }
            }
        }
        return variance / count;
    };

    // Apply with 3 iterations
    AnisotropicDiffusionFilter::Parameters paramsLow;
    paramsLow.numberOfIterations = 3;
    paramsLow.conductance = 3.0;
    auto resultLow = filter.apply(testImage_, paramsLow);
    ASSERT_TRUE(resultLow.has_value());

    // Apply with 20 iterations
    AnisotropicDiffusionFilter::Parameters paramsHigh;
    paramsHigh.numberOfIterations = 20;
    paramsHigh.conductance = 3.0;
    auto resultHigh = filter.apply(testImage_, paramsHigh);
    ASSERT_TRUE(resultHigh.has_value());

    double varianceLow = computeVariance(resultLow.value());
    double varianceHigh = computeVariance(resultHigh.value());

    // More iterations should produce smoother result (lower variance)
    EXPECT_LE(varianceHigh, varianceLow);
}

TEST_F(AnisotropicDiffusionFilterTest, LowConductancePreservesEdgesBetter) {
    AnisotropicDiffusionFilter filter;

    // Low conductance (strong edge preservation)
    AnisotropicDiffusionFilter::Parameters paramsLow;
    paramsLow.numberOfIterations = 10;
    paramsLow.conductance = 0.5;
    auto resultLow = filter.apply(testImage_, paramsLow);
    ASSERT_TRUE(resultLow.has_value());

    // High conductance (weaker edge preservation)
    AnisotropicDiffusionFilter::Parameters paramsHigh;
    paramsHigh.numberOfIterations = 10;
    paramsHigh.conductance = 10.0;
    auto resultHigh = filter.apply(testImage_, paramsHigh);
    ASSERT_TRUE(resultHigh.has_value());

    // Measure edge contrast: inside cube vs just outside
    InputImageType::IndexType inside = {10, 10, 10};
    InputImageType::IndexType outside = {7, 10, 10};

    double contrastLow = std::abs(
        static_cast<double>(resultLow.value()->GetPixel(inside))
        - resultLow.value()->GetPixel(outside));
    double contrastHigh = std::abs(
        static_cast<double>(resultHigh.value()->GetPixel(inside))
        - resultHigh.value()->GetPixel(outside));

    // Low conductance should preserve edges better (higher contrast)
    EXPECT_GE(contrastLow, contrastHigh);
}

}  // namespace
}  // namespace dicom_viewer::services

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

#include "services/preprocessing/n4_bias_corrector.hpp"

#include <gtest/gtest.h>
#include <cmath>

namespace dicom_viewer::services {
namespace {

class N4BiasCorrectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test image (16x16x16) - small for fast tests
        testImage_ = InputImageType::New();

        InputImageType::SizeType size;
        size[0] = 16;
        size[1] = 16;
        size[2] = 16;

        InputImageType::IndexType start;
        start.Fill(0);

        InputImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        testImage_->SetRegions(region);
        testImage_->Allocate();
        testImage_->FillBuffer(100);

        // Set spacing (1mm x 1mm x 1mm)
        InputImageType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        testImage_->SetSpacing(spacing);

        // Simulate bias field artifact: gradual intensity increase along X axis
        // This mimics real MRI bias field
        for (int z = 0; z < 16; ++z) {
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    InputImageType::IndexType idx = {x, y, z};
                    // Bias increases from 0.5 to 1.5 along X
                    double biasFactor = 0.5 + (static_cast<double>(x) / 15.0);
                    short value = static_cast<short>(100 * biasFactor);
                    testImage_->SetPixel(idx, value);
                }
            }
        }

        // Create a simple mask (center region)
        testMask_ = MaskImageType::New();
        testMask_->SetRegions(region);
        testMask_->Allocate();
        testMask_->FillBuffer(0);
        testMask_->SetSpacing(spacing);

        for (int z = 4; z < 12; ++z) {
            for (int y = 4; y < 12; ++y) {
                for (int x = 4; x < 12; ++x) {
                    MaskImageType::IndexType idx = {x, y, z};
                    testMask_->SetPixel(idx, 1);
                }
            }
        }
    }

    using InputImageType = N4BiasCorrector::InputImageType;
    using MaskImageType = N4BiasCorrector::MaskImageType;
    using FloatImageType = N4BiasCorrector::FloatImageType;

    InputImageType::Pointer testImage_;
    MaskImageType::Pointer testMask_;
};

// =============================================================================
// Parameters validation tests
// =============================================================================

TEST_F(N4BiasCorrectorTest, ParametersDefaultValid) {
    N4BiasCorrector::Parameters params;

    EXPECT_TRUE(params.isValid());
    EXPECT_EQ(params.shrinkFactor, 4);
    EXPECT_EQ(params.numberOfFittingLevels, 4);
    EXPECT_EQ(params.maxIterationsPerLevel.size(), 4);
    EXPECT_DOUBLE_EQ(params.convergenceThreshold, 0.001);
}

TEST_F(N4BiasCorrectorTest, ParametersShrinkFactorTooLow) {
    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 0;

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersShrinkFactorTooHigh) {
    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 9;

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersShrinkFactorAtBoundaries) {
    N4BiasCorrector::Parameters params;

    params.shrinkFactor = 1;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.shrinkFactor = 8;  // Maximum
    EXPECT_TRUE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersFittingLevelsTooLow) {
    N4BiasCorrector::Parameters params;
    params.numberOfFittingLevels = 0;
    params.maxIterationsPerLevel = {};

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersFittingLevelsTooHigh) {
    N4BiasCorrector::Parameters params;
    params.numberOfFittingLevels = 9;
    params.maxIterationsPerLevel = {50, 50, 50, 50, 50, 50, 50, 50, 50};

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersIterationsMismatch) {
    N4BiasCorrector::Parameters params;
    params.numberOfFittingLevels = 4;
    params.maxIterationsPerLevel = {50, 50};  // Only 2 elements, should be 4

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersIterationsTooLow) {
    N4BiasCorrector::Parameters params;
    params.maxIterationsPerLevel = {0, 50, 50, 50};  // First element invalid

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersIterationsTooHigh) {
    N4BiasCorrector::Parameters params;
    params.maxIterationsPerLevel = {50, 501, 50, 50};  // Second element too high

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersConvergenceTooLow) {
    N4BiasCorrector::Parameters params;
    params.convergenceThreshold = 1e-8;  // Below 1e-7

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersConvergenceTooHigh) {
    N4BiasCorrector::Parameters params;
    params.convergenceThreshold = 0.2;  // Above 1e-1

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersConvergenceAtBoundaries) {
    N4BiasCorrector::Parameters params;

    params.convergenceThreshold = 1e-7;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.convergenceThreshold = 1e-1;  // Maximum
    EXPECT_TRUE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersControlPointsTooLow) {
    N4BiasCorrector::Parameters params;
    params.numberOfControlPoints = 1;

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersControlPointsTooHigh) {
    N4BiasCorrector::Parameters params;
    params.numberOfControlPoints = 33;

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersSplineOrderTooLow) {
    N4BiasCorrector::Parameters params;
    params.splineOrder = 1;

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersSplineOrderTooHigh) {
    N4BiasCorrector::Parameters params;
    params.splineOrder = 5;

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersNegativeWienerNoise) {
    N4BiasCorrector::Parameters params;
    params.wienerFilterNoise = -0.1;

    EXPECT_FALSE(params.isValid());
}

TEST_F(N4BiasCorrectorTest, ParametersInvalidBiasFWHM) {
    N4BiasCorrector::Parameters params;
    params.biasFieldFullWidthAtHalfMaximum = 0.0;

    EXPECT_FALSE(params.isValid());
}

// =============================================================================
// N4BiasCorrector apply tests
// =============================================================================

TEST_F(N4BiasCorrectorTest, ApplyNullInput) {
    N4BiasCorrector corrector;

    auto result = corrector.apply(nullptr);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(N4BiasCorrectorTest, ApplyInvalidParameters) {
    N4BiasCorrector corrector;
    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 0;  // Invalid

    auto result = corrector.apply(testImage_, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(N4BiasCorrectorTest, ApplyWithDefaultParameters) {
    N4BiasCorrector corrector;

    // Use minimal parameters for faster test
    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 4;
    params.numberOfFittingLevels = 1;
    params.maxIterationsPerLevel = {2};  // Minimal iterations
    params.convergenceThreshold = 0.01;

    auto result = corrector.apply(testImage_, params);

    ASSERT_TRUE(result.has_value());

    auto& res = result.value();
    ASSERT_NE(res.correctedImage, nullptr);
    ASSERT_NE(res.biasField, nullptr);

    // Check output dimensions match input
    auto inputSize = testImage_->GetLargestPossibleRegion().GetSize();
    auto outputSize = res.correctedImage->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(inputSize[0], outputSize[0]);
    EXPECT_EQ(inputSize[1], outputSize[1]);
    EXPECT_EQ(inputSize[2], outputSize[2]);
}

TEST_F(N4BiasCorrectorTest, ApplyWithMask) {
    N4BiasCorrector corrector;

    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 4;
    params.numberOfFittingLevels = 1;
    params.maxIterationsPerLevel = {2};
    params.convergenceThreshold = 0.01;

    auto result = corrector.apply(testImage_, params, testMask_);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().correctedImage, nullptr);
    EXPECT_NE(result.value().biasField, nullptr);
}

TEST_F(N4BiasCorrectorTest, ApplyPreservesImageProperties) {
    N4BiasCorrector corrector;

    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 4;
    params.numberOfFittingLevels = 1;
    params.maxIterationsPerLevel = {2};

    auto result = corrector.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto& correctedImage = result.value().correctedImage;

    // Check spacing is preserved
    auto inputSpacing = testImage_->GetSpacing();
    auto outputSpacing = correctedImage->GetSpacing();

    EXPECT_DOUBLE_EQ(inputSpacing[0], outputSpacing[0]);
    EXPECT_DOUBLE_EQ(inputSpacing[1], outputSpacing[1]);
    EXPECT_DOUBLE_EQ(inputSpacing[2], outputSpacing[2]);

    // Check origin is preserved
    auto inputOrigin = testImage_->GetOrigin();
    auto outputOrigin = correctedImage->GetOrigin();

    EXPECT_DOUBLE_EQ(inputOrigin[0], outputOrigin[0]);
    EXPECT_DOUBLE_EQ(inputOrigin[1], outputOrigin[1]);
    EXPECT_DOUBLE_EQ(inputOrigin[2], outputOrigin[2]);
}

TEST_F(N4BiasCorrectorTest, ApplyBiasFieldHasCorrectDimensions) {
    N4BiasCorrector corrector;

    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 4;
    params.numberOfFittingLevels = 1;
    params.maxIterationsPerLevel = {2};

    auto result = corrector.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto& biasField = result.value().biasField;

    // Bias field should have same dimensions as input
    auto inputSize = testImage_->GetLargestPossibleRegion().GetSize();
    auto biasSize = biasField->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(inputSize[0], biasSize[0]);
    EXPECT_EQ(inputSize[1], biasSize[1]);
    EXPECT_EQ(inputSize[2], biasSize[2]);
}

// =============================================================================
// estimateProcessingTime tests
// =============================================================================

TEST_F(N4BiasCorrectorTest, EstimateProcessingTimeBasic) {
    std::array<unsigned int, 3> imageSize = {256, 256, 100};
    N4BiasCorrector::Parameters params;

    double estimate = N4BiasCorrector::estimateProcessingTime(imageSize, params);

    EXPECT_GT(estimate, 0.0);
}

TEST_F(N4BiasCorrectorTest, EstimateProcessingTimeScalesWithSize) {
    N4BiasCorrector::Parameters params;

    std::array<unsigned int, 3> smallSize = {64, 64, 64};
    std::array<unsigned int, 3> largeSize = {256, 256, 256};

    double smallEstimate = N4BiasCorrector::estimateProcessingTime(smallSize, params);
    double largeEstimate = N4BiasCorrector::estimateProcessingTime(largeSize, params);

    // Large image should take longer
    EXPECT_GT(largeEstimate, smallEstimate);
}

TEST_F(N4BiasCorrectorTest, EstimateProcessingTimeScalesWithIterations) {
    std::array<unsigned int, 3> imageSize = {128, 128, 128};

    N4BiasCorrector::Parameters lowIter;
    lowIter.maxIterationsPerLevel = {10, 10, 10, 10};

    N4BiasCorrector::Parameters highIter;
    highIter.maxIterationsPerLevel = {100, 100, 100, 100};

    double lowEstimate = N4BiasCorrector::estimateProcessingTime(imageSize, lowIter);
    double highEstimate = N4BiasCorrector::estimateProcessingTime(imageSize, highIter);

    // More iterations should take longer
    EXPECT_GT(highEstimate, lowEstimate);
}

TEST_F(N4BiasCorrectorTest, EstimateProcessingTimeWithShrinkFactor) {
    std::array<unsigned int, 3> imageSize = {256, 256, 256};

    N4BiasCorrector::Parameters lowShrink;
    lowShrink.shrinkFactor = 2;

    N4BiasCorrector::Parameters highShrink;
    highShrink.shrinkFactor = 8;

    double lowShrinkEstimate = N4BiasCorrector::estimateProcessingTime(
        imageSize, lowShrink);
    double highShrinkEstimate = N4BiasCorrector::estimateProcessingTime(
        imageSize, highShrink);

    // Higher shrink factor should be faster (lower estimate)
    EXPECT_LT(highShrinkEstimate, lowShrinkEstimate);
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(N4BiasCorrectorTest, ProgressCallbackCanBeSet) {
    N4BiasCorrector corrector;

    bool callbackCalled = false;
    double lastProgress = -1.0;

    corrector.setProgressCallback([&](double progress) {
        callbackCalled = true;
        lastProgress = progress;
    });

    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 4;
    params.numberOfFittingLevels = 1;
    params.maxIterationsPerLevel = {5};

    auto result = corrector.apply(testImage_, params);

    // The callback may or may not be called depending on processing speed
    // With very small test images, processing completes too fast for callback
    EXPECT_TRUE(result.has_value());

    // If callback was called, verify progress was valid
    if (callbackCalled) {
        EXPECT_GE(lastProgress, 0.0);
        EXPECT_LE(lastProgress, 1.0);
    }
}

// =============================================================================
// Move semantics tests
// =============================================================================

TEST_F(N4BiasCorrectorTest, MoveConstruction) {
    N4BiasCorrector corrector1;
    N4BiasCorrector corrector2(std::move(corrector1));

    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 4;
    params.numberOfFittingLevels = 1;
    params.maxIterationsPerLevel = {2};

    auto result = corrector2.apply(testImage_, params);
    EXPECT_TRUE(result.has_value());
}

TEST_F(N4BiasCorrectorTest, MoveAssignment) {
    N4BiasCorrector corrector1;
    N4BiasCorrector corrector2;

    corrector2 = std::move(corrector1);

    N4BiasCorrector::Parameters params;
    params.shrinkFactor = 4;
    params.numberOfFittingLevels = 1;
    params.maxIterationsPerLevel = {2};

    auto result = corrector2.apply(testImage_, params);
    EXPECT_TRUE(result.has_value());
}

}  // namespace
}  // namespace dicom_viewer::services

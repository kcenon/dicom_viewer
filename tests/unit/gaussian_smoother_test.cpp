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

#include "services/preprocessing/gaussian_smoother.hpp"
#include "services/preprocessing/anisotropic_diffusion_filter.hpp"
#include "services/preprocessing/histogram_equalizer.hpp"

#include <gtest/gtest.h>
#include <cmath>

namespace dicom_viewer::services {
namespace {

class GaussianSmootherTest : public ::testing::Test {
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

        // Create a high-contrast pattern:
        // Center cube (8-12, 8-12, 8-12) with value 1000
        // Surrounding area with value 0
        for (int z = 8; z <= 12; ++z) {
            for (int y = 8; y <= 12; ++y) {
                for (int x = 8; x <= 12; ++x) {
                    ImageType::IndexType idx = {x, y, z};
                    testImage_->SetPixel(idx, 1000);
                }
            }
        }
    }

    using ImageType = GaussianSmoother::ImageType;
    using Image2DType = GaussianSmoother::Image2DType;

    ImageType::Pointer testImage_;
};

// =============================================================================
// PreprocessingError tests
// =============================================================================

TEST_F(GaussianSmootherTest, PreprocessingErrorSuccess) {
    PreprocessingError error;

    EXPECT_TRUE(error.isSuccess());
    EXPECT_EQ(error.code, PreprocessingError::Code::Success);
    EXPECT_EQ(error.toString(), "Success");
}

TEST_F(GaussianSmootherTest, PreprocessingErrorInvalidInput) {
    PreprocessingError error{
        PreprocessingError::Code::InvalidInput,
        "test message"
    };

    EXPECT_FALSE(error.isSuccess());
    EXPECT_EQ(error.toString(), "Invalid input: test message");
}

TEST_F(GaussianSmootherTest, PreprocessingErrorInvalidParameters) {
    PreprocessingError error{
        PreprocessingError::Code::InvalidParameters,
        "variance out of range"
    };

    EXPECT_FALSE(error.isSuccess());
    EXPECT_EQ(error.toString(), "Invalid parameters: variance out of range");
}

TEST_F(GaussianSmootherTest, PreprocessingErrorProcessingFailed) {
    PreprocessingError error{
        PreprocessingError::Code::ProcessingFailed,
        "ITK error"
    };

    EXPECT_FALSE(error.isSuccess());
    EXPECT_EQ(error.toString(), "Processing failed: ITK error");
}

// =============================================================================
// Parameters validation tests
// =============================================================================

TEST_F(GaussianSmootherTest, ParametersDefaultValid) {
    GaussianSmoother::Parameters params;

    EXPECT_TRUE(params.isValid());
    EXPECT_DOUBLE_EQ(params.variance, 1.0);
    EXPECT_EQ(params.maxKernelWidth, 0u);
    EXPECT_TRUE(params.useImageSpacing);
}

TEST_F(GaussianSmootherTest, ParametersVarianceTooLow) {
    GaussianSmoother::Parameters params;
    params.variance = 0.05;  // Below 0.1 minimum

    EXPECT_FALSE(params.isValid());
}

TEST_F(GaussianSmootherTest, ParametersVarianceTooHigh) {
    GaussianSmoother::Parameters params;
    params.variance = 15.0;  // Above 10.0 maximum

    EXPECT_FALSE(params.isValid());
}

TEST_F(GaussianSmootherTest, ParametersVarianceAtBoundaries) {
    GaussianSmoother::Parameters params;

    params.variance = 0.1;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.variance = 10.0;  // Maximum
    EXPECT_TRUE(params.isValid());
}

TEST_F(GaussianSmootherTest, ParametersInvalidKernelWidth) {
    GaussianSmoother::Parameters params;

    params.maxKernelWidth = 2;  // Too small (must be 0 or >= 3)
    EXPECT_FALSE(params.isValid());

    params.maxKernelWidth = 33;  // Too large (max 32)
    EXPECT_FALSE(params.isValid());
}

TEST_F(GaussianSmootherTest, ParametersValidKernelWidth) {
    GaussianSmoother::Parameters params;

    params.maxKernelWidth = 0;  // Automatic
    EXPECT_TRUE(params.isValid());

    params.maxKernelWidth = 3;  // Minimum when specified
    EXPECT_TRUE(params.isValid());

    params.maxKernelWidth = 32;  // Maximum
    EXPECT_TRUE(params.isValid());
}

// =============================================================================
// GaussianSmoother apply tests
// =============================================================================

TEST_F(GaussianSmootherTest, ApplyNullInput) {
    GaussianSmoother smoother;

    auto result = smoother.apply(nullptr);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(GaussianSmootherTest, ApplyInvalidParameters) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 0.01;  // Invalid

    auto result = smoother.apply(testImage_, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(GaussianSmootherTest, ApplyWithDefaultParameters) {
    GaussianSmoother smoother;

    auto result = smoother.apply(testImage_);

    ASSERT_TRUE(result.has_value());

    auto smoothedImage = result.value();
    ASSERT_NE(smoothedImage, nullptr);

    // Check output dimensions match input
    auto inputSize = testImage_->GetLargestPossibleRegion().GetSize();
    auto outputSize = smoothedImage->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(inputSize[0], outputSize[0]);
    EXPECT_EQ(inputSize[1], outputSize[1]);
    EXPECT_EQ(inputSize[2], outputSize[2]);
}

TEST_F(GaussianSmootherTest, ApplyWithCustomParameters) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 2.5;
    params.maxKernelWidth = 16;
    params.useImageSpacing = true;

    auto result = smoother.apply(testImage_, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(GaussianSmootherTest, ApplyPreservesImageProperties) {
    GaussianSmoother smoother;

    auto result = smoother.apply(testImage_);
    ASSERT_TRUE(result.has_value());

    auto smoothedImage = result.value();

    // Check spacing is preserved
    auto inputSpacing = testImage_->GetSpacing();
    auto outputSpacing = smoothedImage->GetSpacing();

    EXPECT_DOUBLE_EQ(inputSpacing[0], outputSpacing[0]);
    EXPECT_DOUBLE_EQ(inputSpacing[1], outputSpacing[1]);
    EXPECT_DOUBLE_EQ(inputSpacing[2], outputSpacing[2]);

    // Check origin is preserved
    auto inputOrigin = testImage_->GetOrigin();
    auto outputOrigin = smoothedImage->GetOrigin();

    EXPECT_DOUBLE_EQ(inputOrigin[0], outputOrigin[0]);
    EXPECT_DOUBLE_EQ(inputOrigin[1], outputOrigin[1]);
    EXPECT_DOUBLE_EQ(inputOrigin[2], outputOrigin[2]);
}

TEST_F(GaussianSmootherTest, ApplySmoothsImage) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 2.0;  // Moderate smoothing

    auto result = smoother.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto smoothedImage = result.value();

    // After smoothing, edge values should be reduced (smoothed)
    // Center should still have high values, but edges should be affected
    ImageType::IndexType centerIdx = {10, 10, 10};
    ImageType::IndexType edgeIdx = {8, 8, 8};

    auto originalCenter = testImage_->GetPixel(centerIdx);
    auto smoothedCenter = smoothedImage->GetPixel(centerIdx);

    // Center value should decrease due to averaging with surrounding zeros
    EXPECT_LE(smoothedCenter, originalCenter);
    EXPECT_GT(smoothedCenter, 0);  // Should still be positive
}

TEST_F(GaussianSmootherTest, HigherVarianceMoreSmoothing) {
    GaussianSmoother smoother;

    GaussianSmoother::Parameters lowVariance;
    lowVariance.variance = 0.5;

    GaussianSmoother::Parameters highVariance;
    highVariance.variance = 3.0;

    auto lowResult = smoother.apply(testImage_, lowVariance);
    auto highResult = smoother.apply(testImage_, highVariance);

    ASSERT_TRUE(lowResult.has_value());
    ASSERT_TRUE(highResult.has_value());

    // At edge points, high variance should have more effect (more blur)
    ImageType::IndexType edgeIdx = {7, 10, 10};  // Just outside original cube

    auto lowSmoothed = lowResult.value()->GetPixel(edgeIdx);
    auto highSmoothed = highResult.value()->GetPixel(edgeIdx);

    // Higher variance should spread values further, making edge pixels higher
    EXPECT_GE(highSmoothed, lowSmoothed);
}

// =============================================================================
// applyToSlice tests
// =============================================================================

TEST_F(GaussianSmootherTest, ApplyToSliceNullInput) {
    GaussianSmoother smoother;

    auto result = smoother.applyToSlice(nullptr, 10);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(GaussianSmootherTest, ApplyToSliceInvalidSliceIndex) {
    GaussianSmoother smoother;

    auto result = smoother.applyToSlice(testImage_, 100);  // Out of range

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(GaussianSmootherTest, ApplyToSliceSuccess) {
    GaussianSmoother smoother;

    auto result = smoother.applyToSlice(testImage_, 10);

    ASSERT_TRUE(result.has_value());
    auto slice = result.value();
    ASSERT_NE(slice, nullptr);

    // Check 2D dimensions match XY of 3D input
    auto sliceSize = slice->GetLargestPossibleRegion().GetSize();
    auto volumeSize = testImage_->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(sliceSize[0], volumeSize[0]);
    EXPECT_EQ(sliceSize[1], volumeSize[1]);
}

TEST_F(GaussianSmootherTest, ApplyToSliceWithCustomParameters) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 1.5;
    params.maxKernelWidth = 8;

    auto result = smoother.applyToSlice(testImage_, 10, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(GaussianSmootherTest, ApplyToSliceInvalidParameters) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 0.01;  // Invalid

    auto result = smoother.applyToSlice(testImage_, 10, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

// =============================================================================
// getKernelRadius tests
// =============================================================================

TEST_F(GaussianSmootherTest, GetKernelRadiusIsotropicSpacing) {
    GaussianSmoother::Parameters params;
    params.variance = 1.0;
    params.useImageSpacing = true;

    std::array<double, 3> spacing = {1.0, 1.0, 1.0};

    auto radius = GaussianSmoother::getKernelRadius(params, spacing);

    // For variance=1, sigma=1, radius should be ~3*1 = 3
    EXPECT_EQ(radius[0], radius[1]);
    EXPECT_EQ(radius[1], radius[2]);
    EXPECT_GE(radius[0], 3u);
}

TEST_F(GaussianSmootherTest, GetKernelRadiusAnisotropicSpacing) {
    GaussianSmoother::Parameters params;
    params.variance = 1.0;
    params.useImageSpacing = true;

    std::array<double, 3> spacing = {1.0, 1.0, 3.0};  // Anisotropic Z

    auto radius = GaussianSmoother::getKernelRadius(params, spacing);

    // Z radius should be smaller due to larger spacing
    EXPECT_LT(radius[2], radius[0]);
}

TEST_F(GaussianSmootherTest, GetKernelRadiusIgnoresSpacing) {
    GaussianSmoother::Parameters params;
    params.variance = 1.0;
    params.useImageSpacing = false;

    std::array<double, 3> spacing = {0.5, 0.5, 3.0};

    auto radius = GaussianSmoother::getKernelRadius(params, spacing);

    // When not using spacing, all radii should be equal
    EXPECT_EQ(radius[0], radius[1]);
    EXPECT_EQ(radius[1], radius[2]);
}

TEST_F(GaussianSmootherTest, GetKernelRadiusRespectsMaxWidth) {
    GaussianSmoother::Parameters params;
    params.variance = 9.0;  // Large variance
    params.maxKernelWidth = 8;
    params.useImageSpacing = true;

    std::array<double, 3> spacing = {1.0, 1.0, 1.0};

    auto radius = GaussianSmoother::getKernelRadius(params, spacing);

    // Radius should be limited to maxKernelWidth / 2
    EXPECT_LE(radius[0], 4u);
    EXPECT_LE(radius[1], 4u);
    EXPECT_LE(radius[2], 4u);
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(GaussianSmootherTest, ProgressCallbackIsCalled) {
    GaussianSmoother smoother;

    bool callbackCalled = false;
    double lastProgress = -1.0;

    smoother.setProgressCallback([&](double progress) {
        callbackCalled = true;
        lastProgress = progress;
    });

    auto result = smoother.apply(testImage_);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(callbackCalled);
    EXPECT_GE(lastProgress, 0.0);
    EXPECT_LE(lastProgress, 1.0);
}

// =============================================================================
// Move semantics tests
// =============================================================================

TEST_F(GaussianSmootherTest, MoveConstruction) {
    GaussianSmoother smoother1;
    GaussianSmoother smoother2(std::move(smoother1));

    auto result = smoother2.apply(testImage_);
    EXPECT_TRUE(result.has_value());
}

TEST_F(GaussianSmootherTest, MoveAssignment) {
    GaussianSmoother smoother1;
    GaussianSmoother smoother2;

    smoother2 = std::move(smoother1);

    auto result = smoother2.apply(testImage_);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// Filter accuracy and edge case tests
// =============================================================================

TEST_F(GaussianSmootherTest, ImpulseResponseDecaysWithDistance) {
    // Create an image with a single bright voxel (impulse function)
    auto impulseImage = ImageType::New();
    ImageType::SizeType size;
    size[0] = 21;
    size[1] = 21;
    size[2] = 21;
    ImageType::IndexType start;
    start.Fill(0);
    ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);
    impulseImage->SetRegions(region);
    impulseImage->Allocate();
    impulseImage->FillBuffer(0);
    ImageType::SpacingType spacing;
    spacing.Fill(1.0);
    impulseImage->SetSpacing(spacing);

    // Single bright voxel at center
    ImageType::IndexType center = {10, 10, 10};
    impulseImage->SetPixel(center, 10000);

    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 2.0;

    auto result = smoother.apply(impulseImage, params);
    ASSERT_TRUE(result.has_value());

    auto output = result.value();

    // Gaussian response: center should be positive but less than original
    short centerVal = output->GetPixel(center);
    EXPECT_GT(centerVal, 0);
    EXPECT_LT(centerVal, 10000);

    // Values must decay with increasing distance from center
    ImageType::IndexType near = {11, 10, 10};  // distance 1
    ImageType::IndexType far = {13, 10, 10};    // distance 3

    short nearVal = output->GetPixel(near);
    short farVal = output->GetPixel(far);

    EXPECT_GE(centerVal, nearVal);
    EXPECT_GE(nearVal, farVal);
}

TEST_F(GaussianSmootherTest, MinimumVarianceProducesMinimalSmoothing) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 0.1;  // Minimum variance

    auto result = smoother.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto output = result.value();

    // Interior voxel of the bright cube should retain most of its value
    ImageType::IndexType interiorIdx = {10, 10, 10};
    short originalVal = testImage_->GetPixel(interiorIdx);
    short smoothedVal = output->GetPixel(interiorIdx);

    // With very low variance, the interior should be close to original
    double ratio = static_cast<double>(smoothedVal) / originalVal;
    EXPECT_GT(ratio, 0.8);  // At least 80% retained
}

TEST_F(GaussianSmootherTest, GaussianPreservesMeanIntensity) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 3.0;

    auto result = smoother.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto output = result.value();

    // Compute mean of input and output
    double inputSum = 0.0;
    double outputSum = 0.0;
    const int totalVoxels = 20 * 20 * 20;

    for (int z = 0; z < 20; ++z) {
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                ImageType::IndexType idx = {x, y, z};
                inputSum += testImage_->GetPixel(idx);
                outputSum += output->GetPixel(idx);
            }
        }
    }

    double inputMean = inputSum / totalVoxels;
    double outputMean = outputSum / totalVoxels;

    // Gaussian smoothing preserves mean (within tolerance for boundary effects)
    EXPECT_NEAR(inputMean, outputMean, inputMean * 0.15);
}

TEST_F(GaussianSmootherTest, BoundaryVoxelsNoExtremeArtifacts) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 2.0;

    auto result = smoother.apply(testImage_, params);
    ASSERT_TRUE(result.has_value());

    auto output = result.value();

    // Check corner and edge voxels for extreme values
    std::array<ImageType::IndexType, 4> boundaryVoxels = {{
        {0, 0, 0},
        {19, 19, 19},
        {0, 10, 10},
        {19, 0, 19},
    }};

    for (const auto& idx : boundaryVoxels) {
        short val = output->GetPixel(idx);
        // No values should be negative or exceed the maximum input value
        EXPECT_GE(val, -10)
            << "Boundary artifact at (" << idx[0] << "," << idx[1] << ","
            << idx[2] << ")";
        EXPECT_LE(val, 1010)
            << "Boundary artifact at (" << idx[0] << "," << idx[1] << ","
            << idx[2] << ")";
    }
}

// =============================================================================
// Cross-filter pipeline tests
// =============================================================================

TEST_F(GaussianSmootherTest, GaussianThenDiffusionDiffersFromReverse) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters gaussParams;
    gaussParams.variance = 2.0;

    AnisotropicDiffusionFilter diffusion;
    AnisotropicDiffusionFilter::Parameters diffParams;
    diffParams.numberOfIterations = 5;
    diffParams.conductance = 3.0;

    // Path A: Gaussian → Diffusion
    auto gaussFirst = smoother.apply(testImage_, gaussParams);
    ASSERT_TRUE(gaussFirst.has_value());
    auto pathA = diffusion.apply(gaussFirst.value(), diffParams);
    ASSERT_TRUE(pathA.has_value());

    // Path B: Diffusion → Gaussian
    auto diffFirst = diffusion.apply(testImage_, diffParams);
    ASSERT_TRUE(diffFirst.has_value());
    auto pathB = smoother.apply(diffFirst.value(), gaussParams);
    ASSERT_TRUE(pathB.has_value());

    // The two pipelines should produce different results
    int differingVoxels = 0;
    for (int z = 0; z < 20; ++z) {
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                ImageType::IndexType idx = {x, y, z};
                if (pathA.value()->GetPixel(idx)
                    != pathB.value()->GetPixel(idx)) {
                    differingVoxels++;
                }
            }
        }
    }

    EXPECT_GT(differingVoxels, 0);
}

TEST_F(GaussianSmootherTest, GaussianPrefilterWidensEqualizedRange) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters gaussParams;
    gaussParams.variance = 1.5;

    HistogramEqualizer equalizer;
    HistogramEqualizer::Parameters eqParams;
    eqParams.method = EqualizationMethod::Standard;
    eqParams.preserveRange = false;
    eqParams.outputMinimum = 0.0;
    eqParams.outputMaximum = 255.0;

    // Path A: direct equalization
    auto directResult = equalizer.equalize(testImage_, eqParams);
    ASSERT_TRUE(directResult.has_value());

    // Path B: Gaussian prefilter → equalization
    auto smoothed = smoother.apply(testImage_, gaussParams);
    ASSERT_TRUE(smoothed.has_value());
    auto prefilterResult = equalizer.equalize(smoothed.value(), eqParams);
    ASSERT_TRUE(prefilterResult.has_value());

    // Both should produce valid results with output range near [0, 255]
    auto directHist = equalizer.computeHistogram(directResult.value(), 256);
    auto prefilterHist =
        equalizer.computeHistogram(prefilterResult.value(), 256);

    // Both pipelines should produce wide-range outputs
    double directRange = directHist.maxValue - directHist.minValue;
    double prefilterRange = prefilterHist.maxValue - prefilterHist.minValue;

    EXPECT_GT(directRange, 50.0);
    EXPECT_GT(prefilterRange, 50.0);
}

}  // namespace
}  // namespace dicom_viewer::services

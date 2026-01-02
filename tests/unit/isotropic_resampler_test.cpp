#include "services/preprocessing/isotropic_resampler.hpp"

#include <gtest/gtest.h>
#include <cmath>

#include <itkImageRegionConstIterator.h>

namespace dicom_viewer::services {
namespace {

class IsotropicResamplerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create an anisotropic test image (16x16x8) with spacing (1.0, 1.0, 2.5)mm
        anisotropicImage_ = ImageType::New();

        ImageType::SizeType size;
        size[0] = 16;
        size[1] = 16;
        size[2] = 8;

        ImageType::IndexType start;
        start.Fill(0);

        ImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        anisotropicImage_->SetRegions(region);
        anisotropicImage_->Allocate();
        anisotropicImage_->FillBuffer(100);

        // Set anisotropic spacing (typical MRI or CT with thick slices)
        ImageType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 2.5;  // Anisotropic: slice thickness > in-plane resolution
        anisotropicImage_->SetSpacing(spacing);

        // Set origin
        ImageType::PointType origin;
        origin.Fill(0.0);
        anisotropicImage_->SetOrigin(origin);

        // Create gradient pattern for interpolation verification
        for (int z = 0; z < 8; ++z) {
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    ImageType::IndexType idx = {x, y, z};
                    // Linear gradient along Z for interpolation testing
                    short value = static_cast<short>(50 + z * 20);
                    anisotropicImage_->SetPixel(idx, value);
                }
            }
        }

        // Create isotropic test image (16x16x16) with spacing 1.0mm
        isotropicImage_ = ImageType::New();

        ImageType::SizeType isoSize;
        isoSize.Fill(16);

        ImageType::RegionType isoRegion;
        isoRegion.SetSize(isoSize);
        isoRegion.SetIndex(start);

        isotropicImage_->SetRegions(isoRegion);
        isotropicImage_->Allocate();
        isotropicImage_->FillBuffer(100);

        ImageType::SpacingType isoSpacing;
        isoSpacing.Fill(1.0);  // Isotropic
        isotropicImage_->SetSpacing(isoSpacing);

        // Create label map
        labelMap_ = LabelMapType::New();
        labelMap_->SetRegions(region);
        labelMap_->Allocate();
        labelMap_->FillBuffer(0);
        labelMap_->SetSpacing(spacing);

        // Set some labeled regions
        for (int z = 2; z < 6; ++z) {
            for (int y = 4; y < 12; ++y) {
                for (int x = 4; x < 12; ++x) {
                    LabelMapType::IndexType idx = {x, y, z};
                    labelMap_->SetPixel(idx, 1);  // Foreground label
                }
            }
        }
    }

    using ImageType = IsotropicResampler::ImageType;
    using LabelMapType = IsotropicResampler::LabelMapType;

    ImageType::Pointer anisotropicImage_;
    ImageType::Pointer isotropicImage_;
    LabelMapType::Pointer labelMap_;
};

// =============================================================================
// Parameters validation tests
// =============================================================================

TEST_F(IsotropicResamplerTest, ParametersDefaultValid) {
    IsotropicResampler::Parameters params;

    EXPECT_TRUE(params.isValid());
    EXPECT_DOUBLE_EQ(params.targetSpacing, 1.0);
    EXPECT_EQ(params.interpolation, IsotropicResampler::Interpolation::Linear);
    EXPECT_DOUBLE_EQ(params.defaultValue, 0.0);
    EXPECT_EQ(params.splineOrder, 3u);
}

TEST_F(IsotropicResamplerTest, ParametersTargetSpacingTooLow) {
    IsotropicResampler::Parameters params;
    params.targetSpacing = 0.05;  // Below 0.1

    EXPECT_FALSE(params.isValid());
}

TEST_F(IsotropicResamplerTest, ParametersTargetSpacingTooHigh) {
    IsotropicResampler::Parameters params;
    params.targetSpacing = 15.0;  // Above 10.0

    EXPECT_FALSE(params.isValid());
}

TEST_F(IsotropicResamplerTest, ParametersTargetSpacingAtBoundaries) {
    IsotropicResampler::Parameters params;

    params.targetSpacing = 0.1;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.targetSpacing = 10.0;  // Maximum
    EXPECT_TRUE(params.isValid());
}

TEST_F(IsotropicResamplerTest, ParametersSplineOrderTooLow) {
    IsotropicResampler::Parameters params;
    params.splineOrder = 1;  // Below 2

    EXPECT_FALSE(params.isValid());
}

TEST_F(IsotropicResamplerTest, ParametersSplineOrderTooHigh) {
    IsotropicResampler::Parameters params;
    params.splineOrder = 6;  // Above 5

    EXPECT_FALSE(params.isValid());
}

TEST_F(IsotropicResamplerTest, ParametersSplineOrderAtBoundaries) {
    IsotropicResampler::Parameters params;

    params.splineOrder = 2;  // Minimum
    EXPECT_TRUE(params.isValid());

    params.splineOrder = 5;  // Maximum
    EXPECT_TRUE(params.isValid());
}

// =============================================================================
// needsResampling tests
// =============================================================================

TEST_F(IsotropicResamplerTest, NeedsResamplingForAnisotropicImage) {
    EXPECT_TRUE(IsotropicResampler::needsResampling(anisotropicImage_));
}

TEST_F(IsotropicResamplerTest, NeedsResamplingForIsotropicImage) {
    EXPECT_FALSE(IsotropicResampler::needsResampling(isotropicImage_));
}

TEST_F(IsotropicResamplerTest, NeedsResamplingNullInput) {
    EXPECT_FALSE(IsotropicResampler::needsResampling(nullptr));
}

// =============================================================================
// resample tests
// =============================================================================

TEST_F(IsotropicResamplerTest, ResampleNullInput) {
    IsotropicResampler resampler;

    auto result = resampler.resample(nullptr);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(IsotropicResamplerTest, ResampleInvalidParameters) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 0.01;  // Invalid (below 0.1)

    auto result = resampler.resample(anisotropicImage_, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(IsotropicResamplerTest, ResampleWithDefaultParameters) {
    IsotropicResampler resampler;

    auto result = resampler.resample(anisotropicImage_);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result.value(), nullptr);

    // Check output spacing is isotropic (1.0mm)
    auto outputSpacing = result.value()->GetSpacing();
    EXPECT_DOUBLE_EQ(outputSpacing[0], 1.0);
    EXPECT_DOUBLE_EQ(outputSpacing[1], 1.0);
    EXPECT_DOUBLE_EQ(outputSpacing[2], 1.0);
}

TEST_F(IsotropicResamplerTest, ResampleWithLinearInterpolation) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 1.0;
    params.interpolation = IsotropicResampler::Interpolation::Linear;

    auto result = resampler.resample(anisotropicImage_, params);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result.value(), nullptr);
}

TEST_F(IsotropicResamplerTest, ResampleWithNearestNeighborInterpolation) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 1.0;
    params.interpolation = IsotropicResampler::Interpolation::NearestNeighbor;

    auto result = resampler.resample(anisotropicImage_, params);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result.value(), nullptr);
}

TEST_F(IsotropicResamplerTest, ResampleWithBSplineInterpolation) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 1.0;
    params.interpolation = IsotropicResampler::Interpolation::BSpline;
    params.splineOrder = 3;

    auto result = resampler.resample(anisotropicImage_, params);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result.value(), nullptr);
}

TEST_F(IsotropicResamplerTest, ResampleWithWindowedSincInterpolation) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 1.0;
    params.interpolation = IsotropicResampler::Interpolation::WindowedSinc;

    auto result = resampler.resample(anisotropicImage_, params);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result.value(), nullptr);
}

TEST_F(IsotropicResamplerTest, ResamplePreservesOrigin) {
    IsotropicResampler resampler;

    // Set a custom origin
    ImageType::PointType customOrigin;
    customOrigin[0] = 10.0;
    customOrigin[1] = 20.0;
    customOrigin[2] = 30.0;
    anisotropicImage_->SetOrigin(customOrigin);

    auto result = resampler.resample(anisotropicImage_);

    ASSERT_TRUE(result.has_value());

    auto outputOrigin = result.value()->GetOrigin();
    EXPECT_DOUBLE_EQ(outputOrigin[0], customOrigin[0]);
    EXPECT_DOUBLE_EQ(outputOrigin[1], customOrigin[1]);
    EXPECT_DOUBLE_EQ(outputOrigin[2], customOrigin[2]);
}

TEST_F(IsotropicResamplerTest, ResamplePreservesDirection) {
    IsotropicResampler resampler;

    auto inputDirection = anisotropicImage_->GetDirection();

    auto result = resampler.resample(anisotropicImage_);

    ASSERT_TRUE(result.has_value());

    auto outputDirection = result.value()->GetDirection();
    for (unsigned int i = 0; i < 3; ++i) {
        for (unsigned int j = 0; j < 3; ++j) {
            EXPECT_DOUBLE_EQ(inputDirection[i][j], outputDirection[i][j]);
        }
    }
}

TEST_F(IsotropicResamplerTest, ResampleChangesZDimension) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 1.0;

    auto result = resampler.resample(anisotropicImage_, params);

    ASSERT_TRUE(result.has_value());

    // Original: 8 slices with 2.5mm spacing = 20mm
    // Resampled: ~20 slices with 1.0mm spacing
    auto inputSize = anisotropicImage_->GetLargestPossibleRegion().GetSize();
    auto outputSize = result.value()->GetLargestPossibleRegion().GetSize();

    // X and Y should remain same (same spacing)
    EXPECT_EQ(outputSize[0], inputSize[0]);
    EXPECT_EQ(outputSize[1], inputSize[1]);

    // Z should increase (2.5 / 1.0 = 2.5x more slices)
    EXPECT_GT(outputSize[2], inputSize[2]);
    EXPECT_EQ(outputSize[2], 20u);  // 8 * 2.5 / 1.0 = 20
}

TEST_F(IsotropicResamplerTest, ResampleWithCustomSpacing) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 0.5;

    auto result = resampler.resample(anisotropicImage_, params);

    ASSERT_TRUE(result.has_value());

    auto outputSpacing = result.value()->GetSpacing();
    EXPECT_DOUBLE_EQ(outputSpacing[0], 0.5);
    EXPECT_DOUBLE_EQ(outputSpacing[1], 0.5);
    EXPECT_DOUBLE_EQ(outputSpacing[2], 0.5);

    // Output size should be doubled in X and Y (1.0/0.5 = 2x)
    auto outputSize = result.value()->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(outputSize[0], 32u);  // 16 * 1.0 / 0.5 = 32
    EXPECT_EQ(outputSize[1], 32u);
    EXPECT_EQ(outputSize[2], 40u);  // 8 * 2.5 / 0.5 = 40
}

// =============================================================================
// resampleLabels tests
// =============================================================================

TEST_F(IsotropicResamplerTest, ResampleLabelsNullInput) {
    IsotropicResampler resampler;

    auto result = resampler.resampleLabels(nullptr, 1.0);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(IsotropicResamplerTest, ResampleLabelsInvalidSpacing) {
    IsotropicResampler resampler;

    auto result = resampler.resampleLabels(labelMap_, 0.01);  // Invalid

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(IsotropicResamplerTest, ResampleLabelsSuccess) {
    IsotropicResampler resampler;

    auto result = resampler.resampleLabels(labelMap_, 1.0);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result.value(), nullptr);

    // Check output spacing is isotropic
    auto outputSpacing = result.value()->GetSpacing();
    EXPECT_DOUBLE_EQ(outputSpacing[0], 1.0);
    EXPECT_DOUBLE_EQ(outputSpacing[1], 1.0);
    EXPECT_DOUBLE_EQ(outputSpacing[2], 1.0);
}

TEST_F(IsotropicResamplerTest, ResampleLabelsPreservesLabelValues) {
    IsotropicResampler resampler;

    auto result = resampler.resampleLabels(labelMap_, 1.0);

    ASSERT_TRUE(result.has_value());

    // With nearest neighbor interpolation, label values should be preserved
    // Check that we have both 0 and 1 labels in output
    bool hasBackground = false;
    bool hasForeground = false;

    auto output = result.value();
    using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
    IteratorType it(output, output->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() == 0) hasBackground = true;
        if (it.Get() == 1) hasForeground = true;
        // No other values should exist (interpolation artifacts)
        EXPECT_TRUE(it.Get() == 0 || it.Get() == 1);
    }

    EXPECT_TRUE(hasBackground);
    EXPECT_TRUE(hasForeground);
}

// =============================================================================
// previewDimensions tests
// =============================================================================

TEST_F(IsotropicResamplerTest, PreviewDimensionsNullInput) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;

    auto result = resampler.previewDimensions(nullptr, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidInput);
}

TEST_F(IsotropicResamplerTest, PreviewDimensionsInvalidParameters) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 0.01;  // Invalid

    auto result = resampler.previewDimensions(anisotropicImage_, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PreprocessingError::Code::InvalidParameters);
}

TEST_F(IsotropicResamplerTest, PreviewDimensionsSuccess) {
    IsotropicResampler resampler;
    IsotropicResampler::Parameters params;
    params.targetSpacing = 1.0;

    auto result = resampler.previewDimensions(anisotropicImage_, params);

    ASSERT_TRUE(result.has_value());

    auto info = result.value();

    // Check original size
    EXPECT_EQ(info.originalSize[0], 16u);
    EXPECT_EQ(info.originalSize[1], 16u);
    EXPECT_EQ(info.originalSize[2], 8u);

    // Check original spacing
    EXPECT_DOUBLE_EQ(info.originalSpacing[0], 1.0);
    EXPECT_DOUBLE_EQ(info.originalSpacing[1], 1.0);
    EXPECT_DOUBLE_EQ(info.originalSpacing[2], 2.5);

    // Check resampled size
    EXPECT_EQ(info.resampledSize[0], 16u);
    EXPECT_EQ(info.resampledSize[1], 16u);
    EXPECT_EQ(info.resampledSize[2], 20u);

    // Check resampled spacing
    EXPECT_DOUBLE_EQ(info.resampledSpacing, 1.0);

    // Check memory estimate
    EXPECT_GT(info.estimatedMemoryBytes, 0u);
    EXPECT_EQ(info.estimatedMemoryBytes, 16u * 16u * 20u * sizeof(short));
}

// =============================================================================
// interpolationToString tests
// =============================================================================

TEST_F(IsotropicResamplerTest, InterpolationToStringNearestNeighbor) {
    auto str = IsotropicResampler::interpolationToString(
        IsotropicResampler::Interpolation::NearestNeighbor);
    EXPECT_EQ(str, "Nearest Neighbor");
}

TEST_F(IsotropicResamplerTest, InterpolationToStringLinear) {
    auto str = IsotropicResampler::interpolationToString(
        IsotropicResampler::Interpolation::Linear);
    EXPECT_EQ(str, "Linear");
}

TEST_F(IsotropicResamplerTest, InterpolationToStringBSpline) {
    auto str = IsotropicResampler::interpolationToString(
        IsotropicResampler::Interpolation::BSpline);
    EXPECT_EQ(str, "B-Spline");
}

TEST_F(IsotropicResamplerTest, InterpolationToStringWindowedSinc) {
    auto str = IsotropicResampler::interpolationToString(
        IsotropicResampler::Interpolation::WindowedSinc);
    EXPECT_EQ(str, "Windowed Sinc");
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(IsotropicResamplerTest, ProgressCallbackCanBeSet) {
    IsotropicResampler resampler;

    bool callbackCalled = false;
    double lastProgress = -1.0;

    resampler.setProgressCallback([&](double progress) {
        callbackCalled = true;
        lastProgress = progress;
    });

    auto result = resampler.resample(anisotropicImage_);

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

TEST_F(IsotropicResamplerTest, MoveConstruction) {
    IsotropicResampler resampler1;
    IsotropicResampler resampler2(std::move(resampler1));

    auto result = resampler2.resample(anisotropicImage_);
    EXPECT_TRUE(result.has_value());
}

TEST_F(IsotropicResamplerTest, MoveAssignment) {
    IsotropicResampler resampler1;
    IsotropicResampler resampler2;

    resampler2 = std::move(resampler1);

    auto result = resampler2.resample(anisotropicImage_);
    EXPECT_TRUE(result.has_value());
}

}  // namespace
}  // namespace dicom_viewer::services

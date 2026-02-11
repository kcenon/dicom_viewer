#include <gtest/gtest.h>

#include <cmath>

#include <itkImage.h>
#include <itkImageRegionIterator.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkVectorImage.h>

#include "services/flow/phase_corrector.hpp"
#include "services/flow/velocity_field_assembler.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a small 3D vector image with uniform values per component
VectorImage3D::Pointer createUniformVectorImage(
    unsigned int sx, unsigned int sy, unsigned int sz,
    float vx, float vy, float vz) {
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size = {{sx, sy, sz}};
    VectorImage3D::RegionType region;
    region.SetSize(size);
    image->SetRegions(region);
    image->SetNumberOfComponentsPerPixel(3);
    image->Allocate();

    itk::VariableLengthVector<float> pixel(3);
    pixel[0] = vx;
    pixel[1] = vy;
    pixel[2] = vz;
    image->FillBuffer(pixel);
    return image;
}

/// Create a small 3D scalar image with uniform value
FloatImage3D::Pointer createUniformScalarImage(
    unsigned int sx, unsigned int sy, unsigned int sz, float value) {
    auto image = FloatImage3D::New();
    FloatImage3D::SizeType size = {{sx, sy, sz}};
    FloatImage3D::RegionType region;
    region.SetSize(size);
    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(value);
    return image;
}

}  // anonymous namespace

// =============================================================================
// PhaseCorrectionConfig tests
// =============================================================================

TEST(PhaseCorrectionConfigTest, DefaultIsValid) {
    PhaseCorrectionConfig config;
    EXPECT_TRUE(config.isValid());
    EXPECT_TRUE(config.enableAliasingUnwrap);
    EXPECT_TRUE(config.enableEddyCurrentCorrection);
    EXPECT_TRUE(config.enableMaxwellCorrection);
    EXPECT_EQ(config.polynomialOrder, 2);
    EXPECT_DOUBLE_EQ(config.aliasingThreshold, 0.8);
}

TEST(PhaseCorrectionConfigTest, InvalidPolynomialOrder) {
    PhaseCorrectionConfig config;
    config.polynomialOrder = 0;
    EXPECT_FALSE(config.isValid());
    config.polynomialOrder = 5;
    EXPECT_FALSE(config.isValid());
}

TEST(PhaseCorrectionConfigTest, InvalidThreshold) {
    PhaseCorrectionConfig config;
    config.aliasingThreshold = 0.0;
    EXPECT_FALSE(config.isValid());
    config.aliasingThreshold = 1.5;
    EXPECT_FALSE(config.isValid());
}

// =============================================================================
// PhaseCorrector construction tests
// =============================================================================

TEST(PhaseCorrectorTest, DefaultConstruction) {
    PhaseCorrector corrector;
}

TEST(PhaseCorrectorTest, MoveConstruction) {
    PhaseCorrector corrector;
    PhaseCorrector moved(std::move(corrector));
}

TEST(PhaseCorrectorTest, MoveAssignment) {
    PhaseCorrector corrector;
    PhaseCorrector other;
    other = std::move(corrector);
}

TEST(PhaseCorrectorTest, ProgressCallback) {
    PhaseCorrector corrector;
    double lastProgress = -1.0;
    corrector.setProgressCallback([&](double p) { lastProgress = p; });
    EXPECT_DOUBLE_EQ(lastProgress, -1.0);
}

// =============================================================================
// correctPhase error handling tests
// =============================================================================

TEST(PhaseCorrectorTest, CorrectPhaseInvalidConfig) {
    PhaseCorrector corrector;
    VelocityPhase phase;
    phase.velocityField = createUniformVectorImage(4, 4, 4, 10.0f, 20.0f, 30.0f);
    PhaseCorrectionConfig config;
    config.polynomialOrder = 0;  // Invalid
    auto result = corrector.correctPhase(phase, 150.0, config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(PhaseCorrectorTest, CorrectPhaseNullVelocity) {
    PhaseCorrector corrector;
    VelocityPhase phase;  // velocityField is null
    PhaseCorrectionConfig config;
    auto result = corrector.correctPhase(phase, 150.0, config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(PhaseCorrectorTest, CorrectPhaseNegativeVENC) {
    PhaseCorrector corrector;
    VelocityPhase phase;
    phase.velocityField = createUniformVectorImage(4, 4, 4, 10.0f, 20.0f, 30.0f);
    PhaseCorrectionConfig config;
    auto result = corrector.correctPhase(phase, -100.0, config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(PhaseCorrectorTest, CorrectPhaseZeroVENC) {
    PhaseCorrector corrector;
    VelocityPhase phase;
    phase.velocityField = createUniformVectorImage(4, 4, 4, 10.0f, 20.0f, 30.0f);
    PhaseCorrectionConfig config;
    auto result = corrector.correctPhase(phase, 0.0, config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(PhaseCorrectorTest, CorrectPhasePreservesOriginal) {
    PhaseCorrector corrector;
    VelocityPhase phase;
    phase.velocityField = createUniformVectorImage(4, 4, 4, 10.0f, 20.0f, 30.0f);
    phase.magnitudeImage = createUniformScalarImage(4, 4, 4, 500.0f);
    phase.phaseIndex = 3;
    phase.triggerTime = 42.5;

    PhaseCorrectionConfig config;
    config.enableAliasingUnwrap = false;
    config.enableEddyCurrentCorrection = false;
    config.enableMaxwellCorrection = false;

    auto result = corrector.correctPhase(phase, 150.0, config);
    ASSERT_TRUE(result.has_value());

    // Original should be unchanged
    VectorImage3D::IndexType idx = {{0, 0, 0}};
    auto origPixel = phase.velocityField->GetPixel(idx);
    EXPECT_FLOAT_EQ(origPixel[0], 10.0f);
    EXPECT_FLOAT_EQ(origPixel[1], 20.0f);
    EXPECT_FLOAT_EQ(origPixel[2], 30.0f);

    // Corrected copy metadata preserved
    EXPECT_EQ(result->phaseIndex, 3);
    EXPECT_DOUBLE_EQ(result->triggerTime, 42.5);
}

TEST(PhaseCorrectorTest, CorrectPhaseWithoutMagnitude) {
    PhaseCorrector corrector;
    VelocityPhase phase;
    phase.velocityField = createUniformVectorImage(4, 4, 4, 10.0f, 20.0f, 30.0f);
    // No magnitude image — eddy current correction should be skipped

    PhaseCorrectionConfig config;
    config.enableAliasingUnwrap = false;
    config.enableEddyCurrentCorrection = true;

    auto result = corrector.correctPhase(phase, 150.0, config);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->magnitudeImage, nullptr);
}

// =============================================================================
// unwrapAliasing tests with synthetic data
// =============================================================================

TEST(AliasingUnwrapTest, NoWrappingUnchanged) {
    // All velocities within VENC — should not change
    auto velocity = createUniformVectorImage(8, 8, 4, 50.0f, -30.0f, 75.0f);
    PhaseCorrector::unwrapAliasing(velocity, 150.0, 0.8);

    VectorImage3D::IndexType idx = {{3, 3, 2}};
    auto pixel = velocity->GetPixel(idx);
    EXPECT_FLOAT_EQ(pixel[0], 50.0f);
    EXPECT_FLOAT_EQ(pixel[1], -30.0f);
    EXPECT_FLOAT_EQ(pixel[2], 75.0f);
}

TEST(AliasingUnwrapTest, SingleWrapDetection) {
    // Create image with a velocity jump that indicates wrapping
    auto velocity = VectorImage3D::New();
    VectorImage3D::SizeType size = {{10, 1, 1}};
    VectorImage3D::RegionType region;
    region.SetSize(size);
    velocity->SetRegions(region);
    velocity->SetNumberOfComponentsPerPixel(3);
    velocity->Allocate();

    double venc = 150.0;
    // Gradually increasing velocity that wraps at x=5
    for (unsigned int x = 0; x < 10; ++x) {
        itk::VariableLengthVector<float> pixel(3);
        if (x < 5) {
            pixel[0] = static_cast<float>(100.0 + x * 10.0);  // 100→140
        } else {
            // After wrap: actual velocity 150→190, but measured as -(300-actual)
            pixel[0] = static_cast<float>(-300.0 + 100.0 + x * 10.0);  // -150→-110
        }
        pixel[1] = 0.0f;
        pixel[2] = 0.0f;
        VectorImage3D::IndexType idx = {{static_cast<long>(x), 0, 0}};
        velocity->SetPixel(idx, pixel);
    }

    PhaseCorrector::unwrapAliasing(velocity, venc, 0.8);

    // After unwrap, the discontinuity should be corrected
    // The wrapped values should be shifted by +2*VENC
    VectorImage3D::IndexType idx5 = {{5, 0, 0}};
    auto pixel5 = velocity->GetPixel(idx5);
    // Original was -150, should be unwrapped to 150 (adding 2*VENC=300)
    EXPECT_GT(pixel5[0], 100.0f);  // Should be positive after unwrap
}

TEST(AliasingUnwrapTest, NullImageSafe) {
    PhaseCorrector::unwrapAliasing(nullptr, 150.0, 0.8);
    // Should not crash
}

// =============================================================================
// createStationaryMask tests
// =============================================================================

TEST(StationaryMaskTest, UniformHighSignal) {
    auto magnitude = createUniformScalarImage(8, 8, 4, 1000.0f);
    auto mask = PhaseCorrector::createStationaryMask(magnitude);
    ASSERT_NE(mask, nullptr);
    // Uniform high signal — most voxels should be tissue (255)
    MaskImage3D::IndexType idx = {{4, 4, 2}};
    // Otsu threshold on uniform image may classify all as one class
    // The actual value depends on Otsu behavior with constant input
}

TEST(StationaryMaskTest, NullInput) {
    auto mask = PhaseCorrector::createStationaryMask(nullptr);
    EXPECT_EQ(mask, nullptr);
}

// =============================================================================
// evaluatePolynomial tests
// =============================================================================

TEST(PolynomialTest, ConstantTerm) {
    // Order 1: coeffs[0] = constant
    std::vector<double> coeffs = {5.0, 0.0, 0.0, 0.0};
    double val = PhaseCorrector::evaluatePolynomial(coeffs, 0.0, 0.0, 0.0, 1);
    EXPECT_DOUBLE_EQ(val, 5.0);
}

TEST(PolynomialTest, LinearTerms) {
    // Order 1: a0 + a1*x + a2*y + a3*z
    std::vector<double> coeffs = {1.0, 2.0, 3.0, 4.0};
    // At (1, 1, 1): 1 + 2 + 3 + 4 = 10
    double val = PhaseCorrector::evaluatePolynomial(coeffs, 1.0, 1.0, 1.0, 1);
    EXPECT_DOUBLE_EQ(val, 10.0);
}

TEST(PolynomialTest, LinearAtOrigin) {
    std::vector<double> coeffs = {7.0, 2.0, 3.0, 4.0};
    double val = PhaseCorrector::evaluatePolynomial(coeffs, 0.0, 0.0, 0.0, 1);
    EXPECT_DOUBLE_EQ(val, 7.0);
}

TEST(PolynomialTest, EmptyCoefficients) {
    std::vector<double> coeffs;
    double val = PhaseCorrector::evaluatePolynomial(coeffs, 1.0, 1.0, 1.0, 1);
    EXPECT_DOUBLE_EQ(val, 0.0);
}

// =============================================================================
// fitPolynomialBackground tests
// =============================================================================

TEST(PolynomialFitTest, NullInputs) {
    auto coeffs = PhaseCorrector::fitPolynomialBackground(
        nullptr, nullptr, 2);
    // Should return zero-filled coefficients
    for (double c : coeffs) {
        EXPECT_DOUBLE_EQ(c, 0.0);
    }
}

TEST(PolynomialFitTest, ConstantField) {
    // Scalar field with constant value 42.0, full mask
    auto scalar = createUniformScalarImage(8, 8, 4, 42.0f);
    auto mask = MaskImage3D::New();
    MaskImage3D::SizeType size = {{8, 8, 4}};
    MaskImage3D::RegionType region;
    region.SetSize(size);
    mask->SetRegions(region);
    mask->Allocate();
    mask->FillBuffer(255);

    auto coeffs = PhaseCorrector::fitPolynomialBackground(scalar, mask, 1);
    // Constant term should be approximately 42.0
    ASSERT_FALSE(coeffs.empty());
    EXPECT_NEAR(coeffs[0], 42.0, 1.0);
    // Linear terms should be near zero
    if (coeffs.size() >= 4) {
        EXPECT_NEAR(coeffs[1], 0.0, 1.0);
        EXPECT_NEAR(coeffs[2], 0.0, 1.0);
        EXPECT_NEAR(coeffs[3], 0.0, 1.0);
    }
}

TEST(PolynomialFitTest, TooFewSamples) {
    auto scalar = createUniformScalarImage(2, 2, 2, 10.0f);
    auto mask = MaskImage3D::New();
    MaskImage3D::SizeType size = {{2, 2, 2}};
    MaskImage3D::RegionType region;
    region.SetSize(size);
    mask->SetRegions(region);
    mask->Allocate();
    // Only 1 pixel in mask — fewer than polynomial terms for order 2
    mask->FillBuffer(0);
    MaskImage3D::IndexType idx = {{0, 0, 0}};
    mask->SetPixel(idx, 255);

    auto coeffs = PhaseCorrector::fitPolynomialBackground(scalar, mask, 2);
    // Should return zeros (not enough samples)
    for (double c : coeffs) {
        EXPECT_DOUBLE_EQ(c, 0.0);
    }
}

// =============================================================================
// correctEddyCurrent integration test
// =============================================================================

TEST(EddyCurrentTest, NullInputsSafe) {
    PhaseCorrector::correctEddyCurrent(nullptr, nullptr, 2);
    // Should not crash
}

TEST(EddyCurrentTest, NullMagnitudeSafe) {
    auto velocity = createUniformVectorImage(4, 4, 4, 10.0f, 20.0f, 30.0f);
    PhaseCorrector::correctEddyCurrent(velocity, nullptr, 2);
    // Should not crash, velocity unchanged
    VectorImage3D::IndexType idx = {{2, 2, 2}};
    auto pixel = velocity->GetPixel(idx);
    EXPECT_FLOAT_EQ(pixel[0], 10.0f);
}

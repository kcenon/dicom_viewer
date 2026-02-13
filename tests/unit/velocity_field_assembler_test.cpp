#include <gtest/gtest.h>

#include <cmath>

#include <itkImage.h>
#include <itkImageRegionIterator.h>
#include <itkVectorImage.h>

#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/velocity_field_assembler.hpp"

using namespace dicom_viewer::services;

// =============================================================================
// VelocityPhase default tests
// =============================================================================

TEST(VelocityPhaseTest, Defaults) {
    VelocityPhase phase;
    EXPECT_EQ(phase.phaseIndex, 0);
    EXPECT_DOUBLE_EQ(phase.triggerTime, 0.0);
    EXPECT_EQ(phase.velocityField, nullptr);
    EXPECT_EQ(phase.magnitudeImage, nullptr);
}

// =============================================================================
// VelocityFieldAssembler construction tests
// =============================================================================

TEST(VelocityFieldAssemblerTest, DefaultConstruction) {
    VelocityFieldAssembler assembler;
    // Should not throw
}

TEST(VelocityFieldAssemblerTest, MoveConstruction) {
    VelocityFieldAssembler assembler;
    VelocityFieldAssembler moved(std::move(assembler));
    // Should not throw
}

TEST(VelocityFieldAssemblerTest, MoveAssignment) {
    VelocityFieldAssembler assembler;
    VelocityFieldAssembler other;
    other = std::move(assembler);
    // Should not throw
}

TEST(VelocityFieldAssemblerTest, ProgressCallback) {
    VelocityFieldAssembler assembler;
    double lastProgress = -1.0;
    assembler.setProgressCallback([&](double p) { lastProgress = p; });
    EXPECT_DOUBLE_EQ(lastProgress, -1.0);
}

// =============================================================================
// VENC Scaling tests (static method, no I/O needed)
// =============================================================================

TEST(VENCScalingTest, SignedZeroIsZeroVelocity) {
    // pixel=0 with signed → velocity=0
    float v = VelocityFieldAssembler::applyVENCScaling(0.0f, 150.0, 2048, true);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(VENCScalingTest, SignedMaxIsVENC) {
    // pixel=max → velocity=VENC
    float v = VelocityFieldAssembler::applyVENCScaling(2048.0f, 150.0, 2048, true);
    EXPECT_FLOAT_EQ(v, 150.0f);
}

TEST(VENCScalingTest, SignedNegMaxIsNegVENC) {
    // pixel=-max → velocity=-VENC
    float v = VelocityFieldAssembler::applyVENCScaling(-2048.0f, 150.0, 2048, true);
    EXPECT_FLOAT_EQ(v, -150.0f);
}

TEST(VENCScalingTest, SignedHalfIsHalfVENC) {
    // pixel=max/2 → velocity=VENC/2
    float v = VelocityFieldAssembler::applyVENCScaling(1024.0f, 200.0, 2048, true);
    EXPECT_FLOAT_EQ(v, 100.0f);
}

TEST(VENCScalingTest, UnsignedMidpointIsZeroVelocity) {
    // pixel=midpoint → velocity=0
    float v = VelocityFieldAssembler::applyVENCScaling(2048.0f, 150.0, 4096, false);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(VENCScalingTest, UnsignedMaxIsVENC) {
    // pixel=max → velocity=VENC
    float v = VelocityFieldAssembler::applyVENCScaling(4096.0f, 150.0, 4096, false);
    EXPECT_FLOAT_EQ(v, 150.0f);
}

TEST(VENCScalingTest, UnsignedZeroIsNegVENC) {
    // pixel=0 → velocity=-VENC
    float v = VelocityFieldAssembler::applyVENCScaling(0.0f, 150.0, 4096, false);
    EXPECT_FLOAT_EQ(v, -150.0f);
}

TEST(VENCScalingTest, UnsignedQuarterIsNegHalfVENC) {
    // pixel=max/4 → velocity=-VENC/2
    float v = VelocityFieldAssembler::applyVENCScaling(1024.0f, 200.0, 4096, false);
    EXPECT_FLOAT_EQ(v, -100.0f);
}

TEST(VENCScalingTest, ZeroMaxPixelReturnsZero) {
    float v = VelocityFieldAssembler::applyVENCScaling(100.0f, 150.0, 0, true);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(VENCScalingTest, ZeroVENCReturnsZero) {
    float v = VelocityFieldAssembler::applyVENCScaling(2048.0f, 0.0, 4096, true);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(VENCScalingTest, TypicalSiemens12Bit) {
    // 12-bit signed: max = 2047, VENC = 150 cm/s
    // pixel = 1024 → velocity = (1024/2047) × 150 ≈ 75.037
    float v = VelocityFieldAssembler::applyVENCScaling(1024.0f, 150.0, 2047, true);
    EXPECT_NEAR(v, 75.037f, 0.1f);
}

TEST(VENCScalingTest, TypicalPhilips12BitUnsigned) {
    // 12-bit unsigned: max = 4095, VENC = 100 cm/s
    // pixel = 3072 → velocity = ((3072-2047.5)/2047.5) × 100 ≈ 50.012
    float v = VelocityFieldAssembler::applyVENCScaling(3072.0f, 100.0, 4095, false);
    EXPECT_NEAR(v, 50.012f, 0.1f);
}

// =============================================================================
// assembleAllPhases error handling tests
// =============================================================================

TEST(VelocityFieldAssemblerTest, AssembleAllPhasesEmptyFrameMatrix) {
    VelocityFieldAssembler assembler;
    FlowSeriesInfo info;
    // frameMatrix is empty
    auto result = assembler.assembleAllPhases(info);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

// =============================================================================
// assemblePhase error handling tests
// =============================================================================

TEST(VelocityFieldAssemblerTest, AssemblePhaseNegativeIndex) {
    VelocityFieldAssembler assembler;
    FlowSeriesInfo info;
    info.frameMatrix.resize(1);
    auto result = assembler.assemblePhase(info, -1);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(VelocityFieldAssemblerTest, AssemblePhaseOutOfRange) {
    VelocityFieldAssembler assembler;
    FlowSeriesInfo info;
    info.frameMatrix.resize(3);
    auto result = assembler.assemblePhase(info, 5);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(VelocityFieldAssemblerTest, AssemblePhaseMissingComponents) {
    VelocityFieldAssembler assembler;
    FlowSeriesInfo info;
    info.frameMatrix.resize(1);
    // Only add Vx, missing Vy and Vz
    info.frameMatrix[0][VelocityComponent::Vx] = {"/fake/vx.dcm"};
    auto result = assembler.assemblePhase(info, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InconsistentData);
}

TEST(VelocityFieldAssemblerTest, AssemblePhaseMissingVz) {
    VelocityFieldAssembler assembler;
    FlowSeriesInfo info;
    info.frameMatrix.resize(1);
    info.frameMatrix[0][VelocityComponent::Vx] = {"/fake/vx.dcm"};
    info.frameMatrix[0][VelocityComponent::Vy] = {"/fake/vy.dcm"};
    // Vz missing
    auto result = assembler.assemblePhase(info, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InconsistentData);
}

TEST(VelocityFieldAssemblerTest, AssemblePhaseNonexistentFiles) {
    VelocityFieldAssembler assembler;
    FlowSeriesInfo info;
    info.frameMatrix.resize(1);
    info.frameMatrix[0][VelocityComponent::Vx] = {"/nonexistent/vx.dcm"};
    info.frameMatrix[0][VelocityComponent::Vy] = {"/nonexistent/vy.dcm"};
    info.frameMatrix[0][VelocityComponent::Vz] = {"/nonexistent/vz.dcm"};
    info.venc = {150.0, 150.0, 150.0};
    auto result = assembler.assemblePhase(info, 0);
    ASSERT_FALSE(result.has_value());
    // Should fail at ITK file reading
    EXPECT_EQ(result.error().code, FlowError::Code::ParseFailed);
}

// =============================================================================
// Scanner bit depth variation tests (Issue #202)
// =============================================================================

TEST(VENCScalingTest, Signed10BitRange) {
    // 10-bit signed: max pixel = 511, VENC = 150 cm/s
    // pixel=511 → velocity = VENC
    float v = VelocityFieldAssembler::applyVENCScaling(511.0f, 150.0, 511, true);
    EXPECT_FLOAT_EQ(v, 150.0f);

    // pixel=-511 → velocity = -VENC
    v = VelocityFieldAssembler::applyVENCScaling(-511.0f, 150.0, 511, true);
    EXPECT_FLOAT_EQ(v, -150.0f);
}

TEST(VENCScalingTest, Signed14BitRange) {
    // 14-bit signed: max pixel = 8191, VENC = 200 cm/s
    float v = VelocityFieldAssembler::applyVENCScaling(8191.0f, 200.0, 8191, true);
    EXPECT_FLOAT_EQ(v, 200.0f);

    // Half value
    v = VelocityFieldAssembler::applyVENCScaling(4096.0f, 200.0, 8191, true);
    EXPECT_NEAR(v, 100.0f, 0.1f);
}

TEST(VENCScalingTest, Signed16BitRange) {
    // 16-bit signed: max pixel = 32767, VENC = 300 cm/s
    float v = VelocityFieldAssembler::applyVENCScaling(32767.0f, 300.0, 32767, true);
    EXPECT_FLOAT_EQ(v, 300.0f);

    v = VelocityFieldAssembler::applyVENCScaling(-16384.0f, 300.0, 32767, true);
    EXPECT_NEAR(v, -150.0f, 0.1f);
}

TEST(VENCScalingTest, AsymmetricVENCPerComponent) {
    // Asymmetric VENC: Vx=150, Vy=200, Vz=100 cm/s
    // Max pixel for same max pixel value, different VENC per axis
    int maxPixel = 2047;
    float pixelVal = 1024.0f;

    // Vx: VENC=150 → (1024/2047)*150 ≈ 75.037
    float vx = VelocityFieldAssembler::applyVENCScaling(pixelVal, 150.0, maxPixel, true);
    EXPECT_NEAR(vx, 75.037f, 0.1f);

    // Vy: VENC=200 → (1024/2047)*200 ≈ 100.049
    float vy = VelocityFieldAssembler::applyVENCScaling(pixelVal, 200.0, maxPixel, true);
    EXPECT_NEAR(vy, 100.049f, 0.1f);

    // Vz: VENC=100 → (1024/2047)*100 ≈ 50.024
    float vz = VelocityFieldAssembler::applyVENCScaling(pixelVal, 100.0, maxPixel, true);
    EXPECT_NEAR(vz, 50.024f, 0.1f);
}

TEST(VENCScalingTest, Unsigned10BitRange) {
    // 10-bit unsigned: max pixel = 1023, VENC = 150 cm/s
    // midpoint (511.5) → 0 velocity
    float v = VelocityFieldAssembler::applyVENCScaling(512.0f, 150.0, 1023, false);
    EXPECT_NEAR(v, 0.0f, 0.5f);

    // max → +VENC
    v = VelocityFieldAssembler::applyVENCScaling(1023.0f, 150.0, 1023, false);
    EXPECT_FLOAT_EQ(v, 150.0f);
}

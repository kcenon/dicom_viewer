#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include <itkVectorImage.h>

#include "services/flow/flow_quantifier.hpp"
#include "services/flow/vessel_analyzer.hpp"

#include "../test_utils/flow_phantom_generator.hpp"

using namespace dicom_viewer::services;
namespace phantom = dicom_viewer::test_utils;

// =============================================================================
// Kinetic Energy — Error handling
// =============================================================================

TEST(KineticEnergyTest, NullVelocityFieldReturnsError) {
    VesselAnalyzer analyzer;
    VelocityPhase phase;  // null velocity field
    auto result = analyzer.computeKineticEnergy(phase);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(KineticEnergyTest, WrongComponentCountReturnsError) {
    VesselAnalyzer analyzer;

    // Create 2-component image instead of 3
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size;
    size[0] = 8; size[1] = 8; size[2] = 8;
    image->SetRegions(VectorImage3D::RegionType(VectorImage3D::IndexType(), size));
    image->SetNumberOfComponentsPerPixel(2);
    image->Allocate(true);

    VelocityPhase phase;
    phase.velocityField = image;

    auto result = analyzer.computeKineticEnergy(phase);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

// =============================================================================
// Kinetic Energy — Analytical verification
// =============================================================================

TEST(KineticEnergyTest, UniformVelocityFieldAnalytical) {
    // Known velocity → compute KE analytically
    // V = (0, 0, 100) cm/s at all voxels
    // |u| = 100 cm/s = 1.0 m/s
    // KE_density = 0.5 * 1060 * 1.0^2 = 530 J/m^3
    // Voxel volume = 1mm^3 = 1e-9 m^3
    // Total KE = 530 * numVoxels * 1e-9

    constexpr int kDim = 16;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    int numPixels = kDim * kDim * kDim;
    for (int i = 0; i < numPixels; ++i) {
        buf[i * 3]     = 0.0f;
        buf[i * 3 + 1] = 0.0f;
        buf[i * 3 + 2] = 100.0f;  // 100 cm/s along Z
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    VesselAnalyzer analyzer;
    auto result = analyzer.computeKineticEnergy(phase);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // Per-voxel KE = 0.5 * 1060 * 1.0^2 = 530.0 J/m^3
    double expectedDensity = 0.5 * 1060.0 * 1.0 * 1.0;
    EXPECT_NEAR(result->meanKE, expectedDensity, 0.1);

    // Total KE = 530.0 * 4096 * 1e-9 = ~2.1709e-3 J
    double expectedTotal = expectedDensity * numPixels * 1e-9;
    EXPECT_NEAR(result->totalKE, expectedTotal, expectedTotal * 0.01);

    EXPECT_EQ(result->voxelCount, numPixels);
}

TEST(KineticEnergyTest, ZeroVelocityFieldHasZeroKE) {
    constexpr int kDim = 8;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    // All zeros by default

    VelocityPhase phase;
    phase.velocityField = velocity;

    VesselAnalyzer analyzer;
    auto result = analyzer.computeKineticEnergy(phase);
    ASSERT_TRUE(result.has_value());

    EXPECT_NEAR(result->totalKE, 0.0, 1e-15);
    EXPECT_NEAR(result->meanKE, 0.0, 1e-15);
}

TEST(KineticEnergyTest, KEScalesWithDensity) {
    constexpr int kDim = 8;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    int numPixels = kDim * kDim * kDim;
    for (int i = 0; i < numPixels; ++i) {
        buf[i * 3 + 2] = 50.0f;  // 50 cm/s
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    // Default density
    VesselAnalyzer analyzer1;
    auto result1 = analyzer1.computeKineticEnergy(phase);
    ASSERT_TRUE(result1.has_value());

    // Recreate phase for second computation
    auto velocity2 = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf2 = velocity2->GetBufferPointer();
    for (int i = 0; i < numPixels; ++i) {
        buf2[i * 3 + 2] = 50.0f;
    }
    VelocityPhase phase2;
    phase2.velocityField = velocity2;

    // Double density
    VesselAnalyzer analyzer2;
    analyzer2.setBloodDensity(2120.0);
    auto result2 = analyzer2.computeKineticEnergy(phase2);
    ASSERT_TRUE(result2.has_value());

    EXPECT_NEAR(result2->totalKE / result1->totalKE, 2.0, 0.01)
        << "KE should scale linearly with density";
}

TEST(KineticEnergyTest, MultiComponentVelocity) {
    // V = (30, 40, 0) cm/s → |u| = 50 cm/s = 0.5 m/s
    // KE_density = 0.5 * 1060 * 0.25 = 132.5 J/m^3
    constexpr int kDim = 8;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    int numPixels = kDim * kDim * kDim;
    for (int i = 0; i < numPixels; ++i) {
        buf[i * 3]     = 30.0f;
        buf[i * 3 + 1] = 40.0f;
        buf[i * 3 + 2] = 0.0f;
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    VesselAnalyzer analyzer;
    auto result = analyzer.computeKineticEnergy(phase);
    ASSERT_TRUE(result.has_value());

    double expectedDensity = 0.5 * 1060.0 * 0.25;  // 132.5
    EXPECT_NEAR(result->meanKE, expectedDensity, 0.1);
}

// =============================================================================
// Kinetic Energy — Mask support
// =============================================================================

TEST(KineticEnergyTest, MaskRestrictsComputation) {
    constexpr int kDim = 8;
    int numPixels = kDim * kDim * kDim;

    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    for (int i = 0; i < numPixels; ++i) {
        buf[i * 3 + 2] = 100.0f;
    }

    // Mask: only first half of voxels
    auto mask = phantom::createScalarImage(kDim, kDim, kDim);
    auto* mBuf = mask->GetBufferPointer();
    for (int i = 0; i < numPixels / 2; ++i) {
        mBuf[i] = 1.0f;
    }
    // Second half stays 0.0 (masked out)

    VelocityPhase phase;
    phase.velocityField = velocity;

    VesselAnalyzer analyzer;

    // Without mask
    auto fullResult = analyzer.computeKineticEnergy(phase);
    ASSERT_TRUE(fullResult.has_value());

    // Recreate phase for masked run
    auto velocity2 = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf2 = velocity2->GetBufferPointer();
    for (int i = 0; i < numPixels; ++i) {
        buf2[i * 3 + 2] = 100.0f;
    }
    VelocityPhase phase2;
    phase2.velocityField = velocity2;

    // With mask (half voxels)
    auto maskedResult = analyzer.computeKineticEnergy(phase2, mask);
    ASSERT_TRUE(maskedResult.has_value());

    EXPECT_EQ(maskedResult->voxelCount, numPixels / 2);
    EXPECT_NEAR(maskedResult->totalKE, fullResult->totalKE / 2.0,
                fullResult->totalKE * 0.01);
}

TEST(KineticEnergyTest, MaskDimensionMismatchReturnsError) {
    constexpr int kDim = 8;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    for (int i = 0; i < kDim * kDim * kDim; ++i) {
        buf[i * 3 + 2] = 50.0f;
    }

    // Mask with wrong dimensions
    auto mask = phantom::createScalarImage(4, 4, 4);

    VelocityPhase phase;
    phase.velocityField = velocity;

    VesselAnalyzer analyzer;
    auto result = analyzer.computeKineticEnergy(phase, mask);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

// =============================================================================
// Kinetic Energy — Output image
// =============================================================================

TEST(KineticEnergyTest, OutputImageDimensionsMatch) {
    constexpr int kDim = 16;
    auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, 100.0, 6.0);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeKineticEnergy(phase);
    ASSERT_TRUE(result.has_value());

    auto outSize = result->keField->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(outSize[0], static_cast<unsigned>(kDim));
    EXPECT_EQ(outSize[1], static_cast<unsigned>(kDim));
    EXPECT_EQ(outSize[2], static_cast<unsigned>(kDim));
}

TEST(KineticEnergyTest, PoiseuilleFlowKEProfile) {
    // Poiseuille: V(r) = Vmax*(1 - r^2/R^2) along Z
    // KE at center = 0.5*rho*(Vmax*0.01)^2 (max)
    // KE at wall = 0 (velocity = 0)
    constexpr int kDim = 32;
    constexpr double kVMax = 80.0;  // cm/s
    auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, kVMax, 10.0);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeKineticEnergy(phase);
    ASSERT_TRUE(result.has_value());

    auto* keBuf = result->keField->GetBufferPointer();
    int center = kDim / 2;
    int centerIdx = center * kDim * kDim + center * kDim + center;

    // KE at pipe center should be maximum
    double expectedMaxKE = 0.5 * 1060.0 * (kVMax * 0.01) * (kVMax * 0.01);
    EXPECT_NEAR(keBuf[centerIdx], expectedMaxKE, expectedMaxKE * 0.01);

    // KE at corner (outside pipe) should be zero
    int cornerIdx = 0;
    EXPECT_NEAR(keBuf[cornerIdx], 0.0, 0.01);
}

// =============================================================================
// Heart Rate — Error handling
// =============================================================================

TEST(HeartRateTest, TooFewPhasesReturnsError) {
    std::vector<VelocityPhase> onePhase(1);
    auto result = FlowQuantifier::extractHeartRate(onePhase);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(HeartRateTest, NoTriggerOrTemporalResolutionReturnsError) {
    // Phases with zero trigger times and no temporal resolution
    std::vector<VelocityPhase> phases(5);
    for (int i = 0; i < 5; ++i) {
        phases[i].phaseIndex = 0;
        phases[i].triggerTime = 0.0;
    }
    auto result = FlowQuantifier::extractHeartRate(phases, 0.0);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Heart Rate — Trigger time based
// =============================================================================

TEST(HeartRateTest, FromTriggerTimes) {
    // 20 phases, trigger times spanning 0..950 ms (50 ms intervals)
    // RR interval = 950 * 20/19 = 1000 ms → HR = 60 BPM
    std::vector<VelocityPhase> phases;
    for (int i = 0; i < 20; ++i) {
        VelocityPhase phase;
        phase.phaseIndex = i;
        phase.triggerTime = i * 50.0;  // 0, 50, 100, ..., 950 ms
        phases.push_back(phase);
    }

    auto result = FlowQuantifier::extractHeartRate(phases);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_NEAR(*result, 60.0, 0.1)
        << "20 phases over 1000ms cycle should give 60 BPM";
}

TEST(HeartRateTest, FastHeartRate) {
    // 10 phases, trigger times spanning 0..360 ms (40ms intervals)
    // RR = 360 * 10/9 = 400 ms → HR = 150 BPM
    std::vector<VelocityPhase> phases;
    for (int i = 0; i < 10; ++i) {
        VelocityPhase phase;
        phase.phaseIndex = i;
        phase.triggerTime = i * 40.0;
        phases.push_back(phase);
    }

    auto result = FlowQuantifier::extractHeartRate(phases);
    ASSERT_TRUE(result.has_value());

    EXPECT_NEAR(*result, 150.0, 0.5);
}

TEST(HeartRateTest, SlowHeartRate) {
    // 30 phases, trigger times spanning 0..1450 ms (50ms intervals)
    // RR = 1450 * 30/29 = 1500 ms → HR = 40 BPM
    std::vector<VelocityPhase> phases;
    for (int i = 0; i < 30; ++i) {
        VelocityPhase phase;
        phase.phaseIndex = i;
        phase.triggerTime = i * 50.0;
        phases.push_back(phase);
    }

    auto result = FlowQuantifier::extractHeartRate(phases);
    ASSERT_TRUE(result.has_value());

    EXPECT_NEAR(*result, 40.0, 0.5);
}

// =============================================================================
// Heart Rate — Temporal resolution fallback
// =============================================================================

TEST(HeartRateTest, FromTemporalResolutionFallback) {
    // Phases without trigger time data, but with temporal resolution
    // 20 phases × 50 ms = 1000 ms RR → 60 BPM
    std::vector<VelocityPhase> phases;
    for (int i = 0; i < 20; ++i) {
        VelocityPhase phase;
        phase.phaseIndex = 0;      // no meaningful phase index
        phase.triggerTime = 0.0;   // no trigger time
        phases.push_back(phase);
    }

    auto result = FlowQuantifier::extractHeartRate(phases, 50.0);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_NEAR(*result, 60.0, 0.1);
}

// =============================================================================
// Heart Rate — Pulsatile phantom integration
// =============================================================================

TEST(HeartRateTest, PulsatilePhantomIntegration) {
    // Generate pulsatile flow with known temporal resolution
    constexpr int kDim = 8;
    constexpr int kPhases = 25;
    constexpr double kTemporalRes = 32.0;  // ms

    auto [phases, truth] = phantom::generatePulsatileFlow(
        kDim, kPhases, 50.0, 20.0, kTemporalRes);

    auto result = FlowQuantifier::extractHeartRate(phases, kTemporalRes);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // RR = 25 * 32 = 800 ms → HR = 75 BPM
    double expectedHR = 60000.0 / (kPhases * kTemporalRes);
    EXPECT_NEAR(*result, expectedHR, 0.5);
}

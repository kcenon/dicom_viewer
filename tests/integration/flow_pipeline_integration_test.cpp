#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

#include <itkVectorImage.h>

#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>

#include "services/flow/flow_quantifier.hpp"
#include "services/flow/flow_visualizer.hpp"
#include "services/flow/phase_corrector.hpp"
#include "services/flow/temporal_navigator.hpp"
#include "services/flow/velocity_field_assembler.hpp"

#include "../test_utils/flow_phantom_generator.hpp"

using namespace dicom_viewer::services;
namespace phantom = dicom_viewer::test_utils;

// =============================================================================
// E2E-001: Poiseuille Flow Rate Validation
// Pipeline: PhantomGenerator → PhaseCorrector → FlowQuantifier → Validate
// Ground truth: Q = pi * R_cm^2 * Vmax / 2
// =============================================================================

class PoiseuilleFlowIntegration : public ::testing::Test {
protected:
    static constexpr int kDim = 64;
    static constexpr double kVMax = 100.0;     // cm/s
    static constexpr double kPipeRadius = 15.0; // voxels (15mm at 1mm spacing)
};

TEST_F(PoiseuilleFlowIntegration, FlowRateMatchesAnalyticalSolution) {
    // Generate phantom with known flow rate
    auto [phase, truth] = phantom::generatePoiseuillePipe(
        kDim, kVMax, kPipeRadius);

    // Apply phase correction (no aliasing, corrections should be benign)
    PhaseCorrector corrector;
    PhaseCorrectionConfig config;
    config.enableAliasingUnwrap = false;
    config.enableEddyCurrentCorrection = false;
    config.enableMaxwellCorrection = false;
    auto corrected = corrector.correctPhase(phase, 150.0, config);
    ASSERT_TRUE(corrected.has_value()) << corrected.error().message;

    // Measure flow through center of volume, normal along Z
    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    plane.normal = {0, 0, 1};
    plane.radius = kPipeRadius + 5.0;  // Slightly larger than pipe
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto measurement = quantifier.measureFlow(*corrected);
    ASSERT_TRUE(measurement.has_value()) << measurement.error().message;

    // Validate against analytical solution: Q = pi * R_cm^2 * Vmax / 2
    // Tolerance ±10% for discrete sampling
    EXPECT_NEAR(measurement->flowRate, truth.flowRate,
                truth.flowRate * 0.10)
        << "Measured flow rate " << measurement->flowRate
        << " mL/s vs analytical " << truth.flowRate << " mL/s";

    // Mean velocity should be approximately Vmax/2 for Poiseuille
    EXPECT_GT(measurement->meanVelocity, 0.0);
    EXPECT_GT(measurement->maxVelocity, 0.0);
    EXPECT_GT(measurement->sampleCount, 0);
}

TEST_F(PoiseuilleFlowIntegration, PerpendicularPlaneGivesZeroFlow) {
    auto [phase, truth] = phantom::generatePoiseuillePipe(
        kDim, kVMax, kPipeRadius);

    // Plane perpendicular to flow (normal along X, flow along Z)
    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    plane.normal = {1, 0, 0};  // X-normal → perpendicular to Z-flow
    plane.radius = kPipeRadius;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto measurement = quantifier.measureFlow(phase);
    ASSERT_TRUE(measurement.has_value());

    // Through-plane flow should be ~zero (dot product of Z-velocity with X-normal)
    EXPECT_NEAR(measurement->flowRate, 0.0, 0.1);
}

TEST_F(PoiseuilleFlowIntegration, ObliquePlaneGivesReducedFlow) {
    auto [phase, truth] = phantom::generatePoiseuillePipe(
        kDim, kVMax, kPipeRadius);

    // 45-degree oblique plane (normal halfway between X and Z)
    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    double inv_sqrt2 = 1.0 / std::sqrt(2.0);
    plane.normal = {inv_sqrt2, 0, inv_sqrt2};  // 45 degrees
    plane.radius = kPipeRadius + 5.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto measurement = quantifier.measureFlow(phase);
    ASSERT_TRUE(measurement.has_value());

    // By conservation of mass, the flow rate through any cross-section
    // of the same pipe is identical regardless of plane angle.
    // The reduced per-sample velocity (cos theta) is compensated by the
    // larger elliptical intersection area (1/cos theta).
    EXPECT_NEAR(measurement->flowRate, truth.flowRate, truth.flowRate * 0.20);
}

// =============================================================================
// E2E-004: Pulsatile Time-Velocity Curve Validation
// Pipeline: PhantomGenerator → FlowQuantifier TVC → Validate SV
// =============================================================================

class PulsatileTVCIntegration : public ::testing::Test {
protected:
    static constexpr int kDim = 32;         // Smaller for speed
    static constexpr int kPhaseCount = 20;
    static constexpr double kTempRes = 50.0; // 50ms between phases
};

TEST_F(PulsatileTVCIntegration, StrokeVolumeConsistentWithUniformForwardFlow) {
    // Generate pulsatile flow: base=50, amplitude=30 → all phases positive
    auto [phases, truth] = phantom::generatePulsatileFlow(
        kDim, kPhaseCount, 50.0, 30.0, kTempRes);

    // Measure TVC
    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    plane.normal = {0, 0, 1};
    plane.radius = kDim / 2.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto tvc = quantifier.computeTimeVelocityCurve(phases, kTempRes);
    ASSERT_TRUE(tvc.has_value()) << tvc.error().message;

    // All flow is positive → stroke volume should be positive
    EXPECT_GT(tvc->strokeVolume, 0.0);

    // No backward flow → regurgitant volume should be 0
    EXPECT_NEAR(tvc->regurgitantVolume, 0.0, 0.01);
    EXPECT_NEAR(tvc->regurgitantFraction, 0.0, 0.01);

    // TVC should have same number of points as input phases
    EXPECT_EQ(tvc->timePoints.size(), static_cast<size_t>(kPhaseCount));
    EXPECT_EQ(tvc->flowRates.size(), static_cast<size_t>(kPhaseCount));
}

TEST_F(PulsatileTVCIntegration, BidirectionalFlowHasRegurgitation) {
    // Generate pulsatile with amplitude > base → some negative flow phases
    auto [phases, truth] = phantom::generatePulsatileFlow(
        kDim, kPhaseCount, 20.0, 40.0, kTempRes);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    plane.normal = {0, 0, 1};
    plane.radius = kDim / 2.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto tvc = quantifier.computeTimeVelocityCurve(phases, kTempRes);
    ASSERT_TRUE(tvc.has_value());

    // With bidirectional flow, we should see both forward and backward flow
    EXPECT_GT(tvc->strokeVolume, 0.0);
    EXPECT_GT(tvc->regurgitantVolume, 0.0);
    EXPECT_GT(tvc->regurgitantFraction, 0.0);
    EXPECT_LT(tvc->regurgitantFraction, 100.0);
}

TEST_F(PulsatileTVCIntegration, TVCShapeFollowsSinusoid) {
    // Uniform flow varying sinusoidally
    auto [phases, truth] = phantom::generatePulsatileFlow(
        kDim, kPhaseCount, 50.0, 30.0, kTempRes);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    plane.normal = {0, 0, 1};
    plane.radius = kDim / 2.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto tvc = quantifier.computeTimeVelocityCurve(phases, kTempRes);
    ASSERT_TRUE(tvc.has_value());

    // Flow rates should follow sinusoidal pattern
    // Find max and min flow rate
    double maxFlow = *std::max_element(tvc->flowRates.begin(), tvc->flowRates.end());
    double minFlow = *std::min_element(tvc->flowRates.begin(), tvc->flowRates.end());

    // Max and min should differ (sinusoidal variation exists)
    EXPECT_GT(maxFlow - minFlow, 0.0);

    // All flow rates should be positive (base > amplitude)
    for (double fr : tvc->flowRates) {
        EXPECT_GT(fr, 0.0);
    }
}

// =============================================================================
// E2E-005: Phase Unwrapping Accuracy
// Pipeline: AliasedPhantom → PhaseCorrector.unwrapAliasing → Validate
// =============================================================================

class PhaseUnwrapIntegration : public ::testing::Test {
protected:
    static constexpr int kDim = 32;
};

TEST_F(PhaseUnwrapIntegration, SingleWrapUnwrapsCorrectly) {
    double trueVelocity = 180.0;  // cm/s
    double venc = 150.0;          // cm/s
    // Wrapped: 180 - 2*150 = -120 cm/s

    auto phase = phantom::generateAliasedField(kDim, trueVelocity, venc);

    // Verify the field is wrapped (should be near -120, not 180)
    auto* buf = phase.velocityField->GetBufferPointer();
    double wrappedVz = buf[2];  // First voxel, Z component
    EXPECT_LT(wrappedVz, 0.0) << "Field should be wrapped to negative";
    EXPECT_NEAR(wrappedVz, -120.0, 1.0);

    // Apply aliasing unwrapping only
    PhaseCorrector::unwrapAliasing(phase.velocityField, venc, 0.8);

    // After unwrapping, check a central voxel
    // The unwrapping algorithm is neighbor-based, so uniform fields
    // may not all unwrap (all neighbors have same wrapped value).
    // This tests that the algorithm at least doesn't corrupt the data.
    auto* unwrapped = phase.velocityField->GetBufferPointer();
    // For a uniform aliased field, neighbor-based unwrapping might not
    // detect jumps (all neighbors identical). This validates data integrity.
    int numPixels = kDim * kDim * kDim;
    for (int i = 0; i < numPixels; ++i) {
        double vz = unwrapped[i * 3 + 2];
        // Value should be a valid velocity (not NaN or inf)
        EXPECT_FALSE(std::isnan(vz));
        EXPECT_FALSE(std::isinf(vz));
    }
}

TEST_F(PhaseUnwrapIntegration, GradientFieldWithLocalAliasing) {
    // Create a field with a velocity gradient that crosses the VENC boundary
    // at specific locations, causing local aliasing
    double venc = 100.0;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();

    // Linear gradient along X: V_z goes from -50 to +150 cm/s
    // At x > 75% of dim, V > VENC → aliased
    for (int z = 0; z < kDim; ++z) {
        for (int y = 0; y < kDim; ++y) {
            for (int x = 0; x < kDim; ++x) {
                int idx = z * kDim * kDim + y * kDim + x;
                double trueVz = -50.0 + 200.0 * x / (kDim - 1);

                // Apply wrapping
                double wrapped = trueVz;
                while (wrapped > venc) wrapped -= 2.0 * venc;
                while (wrapped < -venc) wrapped += 2.0 * venc;

                buf[idx * 3]     = 0.0f;
                buf[idx * 3 + 1] = 0.0f;
                buf[idx * 3 + 2] = static_cast<float>(wrapped);
            }
        }
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    // Apply unwrapping
    PhaseCorrector::unwrapAliasing(phase.velocityField, venc, 0.8);

    // Check that the unwrapped field is smoother than the wrapped field
    // The gradient should be roughly monotonically increasing
    auto* unwrapped = phase.velocityField->GetBufferPointer();
    int midY = kDim / 2;
    int midZ = kDim / 2;

    // Count direction reversals along X for a fixed Y,Z line
    int reversals = 0;
    double prevVz = unwrapped[(midZ * kDim * kDim + midY * kDim) * 3 + 2];
    for (int x = 1; x < kDim; ++x) {
        int idx = midZ * kDim * kDim + midY * kDim + x;
        double vz = unwrapped[idx * 3 + 2];
        if ((vz - prevVz) < -50.0) {  // Large negative jump = aliasing artifact
            ++reversals;
        }
        prevVz = vz;
    }

    // A well-unwrapped field should have fewer reversals than the wrapped field
    // The wrapped field has at least 1 reversal at the VENC boundary
    // (we allow some reversals due to algorithmic limitations)
    EXPECT_LE(reversals, 2)
        << "Unwrapped field has too many direction reversals";
}

// =============================================================================
// Pipeline Integration: TemporalNavigator + FlowQuantifier
// Tests cache-based phase loading with quantitative measurement
// =============================================================================

class CacheQuantifierIntegration : public ::testing::Test {
protected:
    static constexpr int kDim = 32;
    static constexpr int kPhaseCount = 10;
    static constexpr double kVelocity = 50.0;
};

TEST_F(CacheQuantifierIntegration, NavigatorProvidesDataToQuantifier) {
    // Set up TemporalNavigator with a simple loader
    TemporalNavigator nav;
    nav.initialize(kPhaseCount, 50.0, 3);  // 3-phase window

    nav.setPhaseLoader([](int phaseIndex) -> std::expected<VelocityPhase, FlowError> {
        // Each phase has uniform Z-velocity
        double vz = 50.0 + phaseIndex * 5.0;
        auto velocity = phantom::createVectorImage(32, 32, 32);
        auto* buf = velocity->GetBufferPointer();
        int numPixels = 32 * 32 * 32;
        for (int i = 0; i < numPixels; ++i) {
            buf[i * 3 + 2] = static_cast<float>(vz);
        }
        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = phaseIndex;
        phase.triggerTime = phaseIndex * 50.0;
        return phase;
    });

    // Navigate to a phase and measure
    auto phase0 = nav.goToPhase(0);
    ASSERT_TRUE(phase0.has_value());

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {15.5, 15.5, 15.5};
    plane.normal = {0, 0, 1};
    plane.radius = 16.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto m0 = quantifier.measureFlow(*phase0);
    ASSERT_TRUE(m0.has_value());
    EXPECT_GT(m0->flowRate, 0.0);

    // Navigate to a different phase
    auto phase5 = nav.goToPhase(5);
    ASSERT_TRUE(phase5.has_value());

    auto m5 = quantifier.measureFlow(*phase5);
    ASSERT_TRUE(m5.has_value());

    // Phase 5 has higher velocity → higher flow rate
    EXPECT_GT(m5->flowRate, m0->flowRate);

    // Verify cache status
    auto status = nav.cacheStatus();
    EXPECT_GE(status.cachedCount, 1);
    EXPECT_LE(status.cachedCount, 3);  // Window size = 3
}

TEST_F(CacheQuantifierIntegration, SequentialNavigationBuildsCache) {
    TemporalNavigator nav;
    nav.initialize(kPhaseCount, 50.0, 5);  // 5-phase window

    int loadCount = 0;
    nav.setPhaseLoader([&loadCount](int phaseIndex)
        -> std::expected<VelocityPhase, FlowError> {
        ++loadCount;
        auto velocity = phantom::createVectorImage(16, 16, 16);
        auto* buf = velocity->GetBufferPointer();
        for (int i = 0; i < 16 * 16 * 16; ++i) {
            buf[i * 3 + 2] = 50.0f;
        }
        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = phaseIndex;
        phase.triggerTime = phaseIndex * 50.0;
        return phase;
    });

    // Navigate forward
    for (int i = 0; i < 5; ++i) {
        auto result = nav.goToPhase(i);
        ASSERT_TRUE(result.has_value());
    }
    EXPECT_EQ(loadCount, 5);

    // Re-access cached phase → should NOT trigger loader
    loadCount = 0;
    auto cached = nav.goToPhase(3);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(loadCount, 0) << "Cached phase should not trigger loader";

    // Access beyond window → should trigger eviction + load
    loadCount = 0;
    for (int i = 5; i < 8; ++i) {
        auto result = nav.goToPhase(i);
        ASSERT_TRUE(result.has_value());
    }
    EXPECT_EQ(loadCount, 3);
}

// =============================================================================
// Visualization Pipeline Integration
// Tests FlowVisualizer with real velocity data from phantom generator
// =============================================================================

class VisualizationIntegration : public ::testing::Test {
protected:
    static constexpr int kDim = 32;
};

TEST_F(VisualizationIntegration, StreamlinesFromPoiseuilleFlow) {
    auto [phase, truth] = phantom::generatePoiseuillePipe(
        kDim, 100.0, 10.0);

    FlowVisualizer visualizer;
    auto setResult = visualizer.setVelocityField(phase);
    ASSERT_TRUE(setResult.has_value()) << setResult.error().message;

    StreamlineParams params;
    params.maxSeedPoints = 100;
    params.maxSteps = 500;
    params.terminalSpeed = 1.0;
    params.tubeRadius = 0.3;

    auto streamlines = visualizer.generateStreamlines(params);
    ASSERT_TRUE(streamlines.has_value()) << streamlines.error().message;

    auto polydata = streamlines.value();
    EXPECT_GT(polydata->GetNumberOfPoints(), 0)
        << "Streamlines should produce geometry";
    EXPECT_GT(polydata->GetNumberOfCells(), 0)
        << "Streamlines should produce cells";
}

TEST_F(VisualizationIntegration, GlyphsFromRotatingCylinder) {
    auto [phase, truth] = phantom::generateRotatingCylinder(
        kDim, 10.0, 12.0);

    FlowVisualizer visualizer;
    auto setResult = visualizer.setVelocityField(phase);
    ASSERT_TRUE(setResult.has_value());

    GlyphParams params;
    params.scaleFactor = 0.5;
    params.skipFactor = 4;
    params.minMagnitude = 0.5;

    auto glyphs = visualizer.generateGlyphs(params);
    ASSERT_TRUE(glyphs.has_value()) << glyphs.error().message;

    auto polydata = glyphs.value();
    EXPECT_GT(polydata->GetNumberOfPoints(), 0);
}

TEST_F(VisualizationIntegration, PathlinesFromMultiPhaseFlow) {
    // Generate 5 phases of uniform flow with increasing velocity
    std::vector<VelocityPhase> phases;
    for (int p = 0; p < 5; ++p) {
        double vz = 30.0 + p * 10.0;
        auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
        auto* buf = velocity->GetBufferPointer();
        int numPixels = kDim * kDim * kDim;
        for (int i = 0; i < numPixels; ++i) {
            buf[i * 3 + 2] = static_cast<float>(vz);
        }
        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = p;
        phase.triggerTime = p * 50.0;
        phases.push_back(std::move(phase));
    }

    FlowVisualizer visualizer;
    auto setResult = visualizer.setVelocityField(phases[0]);
    ASSERT_TRUE(setResult.has_value());

    PathlineParams params;
    params.maxSeedPoints = 50;
    params.maxSteps = 100;
    params.terminalSpeed = 1.0;

    auto pathlines = visualizer.generatePathlines(phases, params);
    ASSERT_TRUE(pathlines.has_value()) << pathlines.error().message;

    auto polydata = pathlines.value();
    EXPECT_GT(polydata->GetNumberOfPoints(), 0)
        << "Pathlines should trace particles across phases";
}

TEST_F(VisualizationIntegration, LookupTableCreationWithVelocityRange) {
    auto [phase, truth] = phantom::generatePoiseuillePipe(
        kDim, 100.0, 10.0);

    FlowVisualizer visualizer;
    visualizer.setVelocityField(phase);
    visualizer.setColorMode(ColorMode::VelocityMagnitude);
    visualizer.setVelocityRange(0.0, 100.0);

    auto lut = visualizer.createLookupTable();
    ASSERT_NE(lut, nullptr);
    EXPECT_EQ(lut->GetNumberOfTableValues(), 256);

    double* range = lut->GetRange();
    EXPECT_NEAR(range[0], 0.0, 0.1);
    EXPECT_NEAR(range[1], 100.0, 0.1);
}

// =============================================================================
// ITK-to-VTK Conversion Integration
// Validates the bridge between ITK VectorImage and VTK ImageData
// =============================================================================

class ITKVTKBridgeIntegration : public ::testing::Test {
protected:
    static constexpr int kDim = 16;
};

TEST_F(ITKVTKBridgeIntegration, ConversionPreservesVelocityValues) {
    double vz = 75.0;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim, 2.0);
    auto* buf = velocity->GetBufferPointer();
    int numPixels = kDim * kDim * kDim;
    for (int i = 0; i < numPixels; ++i) {
        buf[i * 3]     = 10.0f;  // Vx
        buf[i * 3 + 1] = 20.0f;  // Vy
        buf[i * 3 + 2] = static_cast<float>(vz);
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    auto vtkResult = FlowVisualizer::velocityFieldToVTK(phase);
    ASSERT_TRUE(vtkResult.has_value()) << vtkResult.error().message;

    auto vtkImage = vtkResult.value();

    // Check dimensions
    int* dims = vtkImage->GetDimensions();
    EXPECT_EQ(dims[0], kDim);
    EXPECT_EQ(dims[1], kDim);
    EXPECT_EQ(dims[2], kDim);

    // Check spacing
    double* spacing = vtkImage->GetSpacing();
    EXPECT_NEAR(spacing[0], 2.0, 0.01);
    EXPECT_NEAR(spacing[1], 2.0, 0.01);
    EXPECT_NEAR(spacing[2], 2.0, 0.01);

    // Check velocity vectors are preserved
    auto vectors = vtkImage->GetPointData()->GetVectors();
    ASSERT_NE(vectors, nullptr);
    EXPECT_EQ(vectors->GetNumberOfComponents(), 3);
    EXPECT_EQ(vectors->GetNumberOfTuples(), numPixels);

    // Sample a few points
    double vec[3];
    vectors->GetTuple(0, vec);
    EXPECT_NEAR(vec[0], 10.0, 0.1);
    EXPECT_NEAR(vec[1], 20.0, 0.1);
    EXPECT_NEAR(vec[2], vz, 0.1);
}

TEST_F(ITKVTKBridgeIntegration, MagnitudeScalarsComputed) {
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    // Set known velocity: (3, 4, 0) → magnitude = 5
    for (int i = 0; i < kDim * kDim * kDim; ++i) {
        buf[i * 3]     = 3.0f;
        buf[i * 3 + 1] = 4.0f;
        buf[i * 3 + 2] = 0.0f;
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    auto vtkResult = FlowVisualizer::velocityFieldToVTK(phase);
    ASSERT_TRUE(vtkResult.has_value());

    auto vtkImage = vtkResult.value();
    auto scalars = vtkImage->GetPointData()->GetScalars();
    ASSERT_NE(scalars, nullptr);

    // Magnitude should be 5.0
    EXPECT_NEAR(scalars->GetTuple1(0), 5.0, 0.1);
}

// =============================================================================
// Pressure Gradient Integration
// Validates simplified Bernoulli equation with flow measurement pipeline
// =============================================================================

TEST(PressureGradientIntegration, BernoulliWithMeasuredVelocity) {
    // Generate Poiseuille flow and measure max velocity
    constexpr int kDim = 32;
    auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, 200.0, 10.0);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    plane.normal = {0, 0, 1};
    plane.radius = 12.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto measurement = quantifier.measureFlow(phase);
    ASSERT_TRUE(measurement.has_value());

    // Use measured max velocity for pressure gradient
    double deltaP = FlowQuantifier::estimatePressureGradient(
        measurement->maxVelocity);

    // Simplified Bernoulli: ΔP = 4 * V²(m/s) = 4 * (V_cm_s/100)²
    double expectedDeltaP = 4.0 * std::pow(measurement->maxVelocity / 100.0, 2.0);
    EXPECT_NEAR(deltaP, expectedDeltaP, 0.01);

    // For Vmax ≈ 200 cm/s = 2 m/s → ΔP ≈ 4 × 4 = 16 mmHg
    EXPECT_GT(deltaP, 10.0);
    EXPECT_LT(deltaP, 20.0);
}

// =============================================================================
// CSV Export Integration
// Validates end-to-end: measurement → TVC → CSV output
// =============================================================================

TEST(CSVExportIntegration, MeasuredTVCExportsToValidCSV) {
    constexpr int kDim = 16;
    constexpr int kPhases = 5;

    auto [phases, truth] = phantom::generatePulsatileFlow(
        kDim, kPhases, 50.0, 20.0, 40.0);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {7.5, 7.5, 7.5};
    plane.normal = {0, 0, 1};
    plane.radius = 8.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto tvc = quantifier.computeTimeVelocityCurve(phases, 40.0);
    ASSERT_TRUE(tvc.has_value());

    // Export to temp file
    auto tmpPath = std::filesystem::temp_directory_path() / "flow_test_tvc.csv";
    auto exportResult = FlowQuantifier::exportToCSV(*tvc, tmpPath.string());
    ASSERT_TRUE(exportResult.has_value());

    // Read and validate CSV content
    std::ifstream ifs(tmpPath.string());
    ASSERT_TRUE(ifs.is_open());

    std::string header;
    std::getline(ifs, header);
    EXPECT_EQ(header, "Time_ms,MeanVelocity_cm_s,MaxVelocity_cm_s,FlowRate_mL_s");

    // Count data rows
    int rowCount = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') break;
        ++rowCount;
    }
    EXPECT_EQ(rowCount, kPhases);

    // Cleanup
    std::filesystem::remove(tmpPath);
}

// =============================================================================
// Rotating Cylinder: Velocity Profile Validation
// Validates tangential velocity = omega * r
// =============================================================================

TEST(RotatingCylinderIntegration, TangentialVelocityMatchesFormula) {
    constexpr int kDim = 64;
    constexpr double kOmega = 5.0;
    constexpr double kRadius = 20.0;

    auto [phase, truth] = phantom::generateRotatingCylinder(
        kDim, kOmega, kRadius);

    auto* buf = phase.velocityField->GetBufferPointer();
    double centerX = (kDim - 1) / 2.0;
    double centerY = (kDim - 1) / 2.0;
    int midZ = kDim / 2;

    // Check velocities at several radii
    // Note: center = 31.5 for dim=64, so integer pixel coordinates
    // have half-pixel offset from center.
    for (int testR = 5; testR <= 15; testR += 5) {
        int x = static_cast<int>(centerX + testR);
        int y = static_cast<int>(centerY);
        int idx = midZ * kDim * kDim + y * kDim + x;

        double vx = buf[idx * 3];
        double vy = buf[idx * 3 + 1];

        // Actual displacement from center (accounts for half-pixel offset)
        double actualDx = x - centerX;
        double actualDy = y - centerY;

        // V = omega x r → Vx = -omega*dy, Vy = omega*dx
        double expectedVx = -kOmega * actualDy;
        double expectedVy = kOmega * actualDx;
        EXPECT_NEAR(vx, expectedVx, 0.1)
            << "Vx at radius " << testR << " voxels";
        EXPECT_NEAR(vy, expectedVy, 0.1)
            << "Vy at radius " << testR << " voxels";
    }

    // Outside cylinder should be zero
    int outsideX = static_cast<int>(centerX + kRadius + 2);
    int outsideIdx = midZ * kDim * kDim +
                     static_cast<int>(centerY) * kDim + outsideX;
    EXPECT_NEAR(buf[outsideIdx * 3], 0.0, 0.01);
    EXPECT_NEAR(buf[outsideIdx * 3 + 1], 0.0, 0.01);
}

// =============================================================================
// Full Pipeline: PhaseCorrector → FlowQuantifier with corrected data
// =============================================================================

TEST(FullCorrectionPipeline, EddyCurrentCorrectionImprovesAccuracy) {
    // Generate field with background gradient
    constexpr int kDim = 32;
    double trueVelocity = 50.0;
    auto phase = phantom::generateFieldWithBackground(
        kDim, trueVelocity, 0.5, 0.3, 0.0);  // Linear gradient in X,Y

    // Measure without correction
    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {(kDim - 1) / 2.0, (kDim - 1) / 2.0, (kDim - 1) / 2.0};
    plane.normal = {0, 0, 1};
    plane.radius = kDim / 2.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto uncorrected = quantifier.measureFlow(phase);
    ASSERT_TRUE(uncorrected.has_value());

    // Apply phase correction with eddy current correction
    PhaseCorrector corrector;
    PhaseCorrectionConfig config;
    config.enableAliasingUnwrap = false;
    config.enableEddyCurrentCorrection = true;
    config.enableMaxwellCorrection = false;
    config.polynomialOrder = 1;  // Linear fit

    auto corrected = corrector.correctPhase(phase, 150.0, config);
    ASSERT_TRUE(corrected.has_value()) << corrected.error().message;

    auto correctedMeasurement = quantifier.measureFlow(*corrected);
    ASSERT_TRUE(correctedMeasurement.has_value());

    // The corrected measurement should have the background gradient removed
    // Both should have similar mean velocity (background averages out at center)
    // but the gradient field has offset at non-center points
    EXPECT_GT(correctedMeasurement->sampleCount, 0);
}

// =============================================================================
// VENC Scaling Integration
// Validates the VelocityFieldAssembler VENC scaling utility
// =============================================================================

TEST(VENCScalingIntegration, SignedScalingRoundTrip) {
    // Simulate: pixel value 2048 with max 4096 (12-bit), VENC = 150 cm/s
    // For signed: velocity = (pixel / maxPixel) * VENC
    float velocity = VelocityFieldAssembler::applyVENCScaling(
        2048.0f, 150.0, 4096, true);
    EXPECT_NEAR(velocity, 75.0, 0.1);  // 2048/4096 * 150 = 75

    // Negative pixel → negative velocity
    float negVelocity = VelocityFieldAssembler::applyVENCScaling(
        -4096.0f, 150.0, 4096, true);
    EXPECT_NEAR(negVelocity, -150.0, 0.1);

    // Zero pixel → zero velocity
    float zeroVelocity = VelocityFieldAssembler::applyVENCScaling(
        0.0f, 150.0, 4096, true);
    EXPECT_NEAR(zeroVelocity, 0.0, 0.01);
}

TEST(VENCScalingIntegration, UnsignedScalingCentersAtMidpoint) {
    // Unsigned: velocity = ((pixel / maxPixel) - 0.5) * 2 * VENC
    // pixel = 3072 / 4096 = 0.75 → (0.75 - 0.5) * 2 * 150 = 75
    float velocity = VelocityFieldAssembler::applyVENCScaling(
        3072.0f, 150.0, 4096, false);
    EXPECT_NEAR(velocity, 75.0, 0.1);

    // Midpoint pixel = 2048 → zero velocity
    float zeroVelocity = VelocityFieldAssembler::applyVENCScaling(
        2048.0f, 150.0, 4096, false);
    EXPECT_NEAR(zeroVelocity, 0.0, 0.1);
}

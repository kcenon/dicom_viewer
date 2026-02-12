#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#include <itkVectorImage.h>

#include "services/flow/flow_quantifier.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a synthetic velocity field with uniform flow along Z axis
VelocityPhase createUniformZFlow(int dim, float velocityZ, int phaseIndex = 0) {
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size;
    size[0] = dim; size[1] = dim; size[2] = dim;
    VectorImage3D::IndexType start;
    start[0] = 0; start[1] = 0; start[2] = 0;
    image->SetRegions(VectorImage3D::RegionType(start, size));
    image->SetNumberOfComponentsPerPixel(3);

    VectorImage3D::SpacingType spacing;
    spacing[0] = 1.0; spacing[1] = 1.0; spacing[2] = 1.0;
    image->SetSpacing(spacing);

    VectorImage3D::PointType origin;
    origin[0] = 0.0; origin[1] = 0.0; origin[2] = 0.0;
    image->SetOrigin(origin);

    image->Allocate();

    auto* buffer = image->GetBufferPointer();
    int numPixels = dim * dim * dim;
    for (int i = 0; i < numPixels; ++i) {
        buffer[i * 3]     = 0.0f;   // Vx
        buffer[i * 3 + 1] = 0.0f;   // Vy
        buffer[i * 3 + 2] = velocityZ;  // Vz
    }

    VelocityPhase phase;
    phase.velocityField = image;
    phase.phaseIndex = phaseIndex;
    phase.triggerTime = phaseIndex * 40.0;
    return phase;
}

/// Create a parabolic pipe flow along Z axis (Poiseuille profile)
/// v(r) = vMax × (1 - r²/R²) for r < R, 0 otherwise
VelocityPhase createPoiseuillePipeFlow(int dim, float vMax,
                                       double pipeRadius,
                                       int phaseIndex = 0) {
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size;
    size[0] = dim; size[1] = dim; size[2] = dim;
    VectorImage3D::IndexType start;
    start[0] = 0; start[1] = 0; start[2] = 0;
    image->SetRegions(VectorImage3D::RegionType(start, size));
    image->SetNumberOfComponentsPerPixel(3);

    VectorImage3D::SpacingType spacing;
    spacing[0] = 1.0; spacing[1] = 1.0; spacing[2] = 1.0;
    image->SetSpacing(spacing);

    VectorImage3D::PointType origin;
    origin[0] = 0.0; origin[1] = 0.0; origin[2] = 0.0;
    image->SetOrigin(origin);

    image->Allocate();

    auto* buffer = image->GetBufferPointer();
    double centerX = (dim - 1) / 2.0;
    double centerY = (dim - 1) / 2.0;

    for (int z = 0; z < dim; ++z) {
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                int idx = x + dim * (y + dim * z);
                double dx = x - centerX;
                double dy = y - centerY;
                double r = std::sqrt(dx * dx + dy * dy);

                float vz = 0.0f;
                if (r < pipeRadius) {
                    vz = static_cast<float>(
                        vMax * (1.0 - (r * r) / (pipeRadius * pipeRadius)));
                }
                buffer[idx * 3]     = 0.0f;
                buffer[idx * 3 + 1] = 0.0f;
                buffer[idx * 3 + 2] = vz;
            }
        }
    }

    VelocityPhase phase;
    phase.velocityField = image;
    phase.phaseIndex = phaseIndex;
    phase.triggerTime = phaseIndex * 40.0;
    return phase;
}

const std::string kTestCSVPath = "/tmp/claude/flow_quantifier_test.csv";

}  // anonymous namespace

// =============================================================================
// Struct default tests
// =============================================================================

TEST(FlowMeasurementTest, Defaults) {
    FlowMeasurement m;
    EXPECT_EQ(m.phaseIndex, 0);
    EXPECT_DOUBLE_EQ(m.flowRate, 0.0);
    EXPECT_DOUBLE_EQ(m.meanVelocity, 0.0);
    EXPECT_DOUBLE_EQ(m.maxVelocity, 0.0);
    EXPECT_DOUBLE_EQ(m.crossSectionArea, 0.0);
    EXPECT_EQ(m.sampleCount, 0);
}

TEST(MeasurementPlaneTest, Defaults) {
    MeasurementPlane p;
    EXPECT_DOUBLE_EQ(p.center[0], 0.0);
    EXPECT_DOUBLE_EQ(p.normal[2], 1.0);
    EXPECT_DOUBLE_EQ(p.radius, 50.0);
    EXPECT_DOUBLE_EQ(p.sampleSpacing, 1.0);
}

TEST(TimeVelocityCurveTest, Defaults) {
    TimeVelocityCurve tvc;
    EXPECT_TRUE(tvc.timePoints.empty());
    EXPECT_DOUBLE_EQ(tvc.strokeVolume, 0.0);
    EXPECT_DOUBLE_EQ(tvc.regurgitantVolume, 0.0);
    EXPECT_DOUBLE_EQ(tvc.regurgitantFraction, 0.0);
}

// =============================================================================
// FlowQuantifier construction tests
// =============================================================================

TEST(FlowQuantifierTest, DefaultConstruction) {
    FlowQuantifier q;
    auto plane = q.measurementPlane();
    EXPECT_DOUBLE_EQ(plane.normal[2], 1.0);
}

TEST(FlowQuantifierTest, MoveConstruction) {
    FlowQuantifier q;
    FlowQuantifier moved(std::move(q));
}

TEST(FlowQuantifierTest, MoveAssignment) {
    FlowQuantifier q;
    FlowQuantifier other;
    other = std::move(q);
}

// =============================================================================
// Vector math utility tests
// =============================================================================

TEST(FlowQuantifierTest, DotProduct) {
    EXPECT_DOUBLE_EQ(FlowQuantifier::dotProduct({1, 0, 0}, {1, 0, 0}), 1.0);
    EXPECT_DOUBLE_EQ(FlowQuantifier::dotProduct({1, 0, 0}, {0, 1, 0}), 0.0);
    EXPECT_DOUBLE_EQ(FlowQuantifier::dotProduct({1, 2, 3}, {4, 5, 6}), 32.0);
    EXPECT_DOUBLE_EQ(FlowQuantifier::dotProduct({3, -2, 7}, {0, 4, -1}), -15.0);
}

TEST(FlowQuantifierTest, CrossProduct) {
    auto c = FlowQuantifier::crossProduct({1, 0, 0}, {0, 1, 0});
    EXPECT_DOUBLE_EQ(c[0], 0.0);
    EXPECT_DOUBLE_EQ(c[1], 0.0);
    EXPECT_DOUBLE_EQ(c[2], 1.0);

    c = FlowQuantifier::crossProduct({0, 1, 0}, {0, 0, 1});
    EXPECT_DOUBLE_EQ(c[0], 1.0);
    EXPECT_DOUBLE_EQ(c[1], 0.0);
    EXPECT_DOUBLE_EQ(c[2], 0.0);
}

TEST(FlowQuantifierTest, Normalize) {
    auto n = FlowQuantifier::normalize({3, 4, 0});
    EXPECT_NEAR(n[0], 0.6, 1e-10);
    EXPECT_NEAR(n[1], 0.8, 1e-10);
    EXPECT_NEAR(n[2], 0.0, 1e-10);

    // Zero vector
    auto z = FlowQuantifier::normalize({0, 0, 0});
    EXPECT_DOUBLE_EQ(z[0], 0.0);
    EXPECT_DOUBLE_EQ(z[1], 0.0);
    EXPECT_DOUBLE_EQ(z[2], 0.0);
}

// =============================================================================
// Measurement plane tests
// =============================================================================

TEST(FlowQuantifierTest, SetMeasurementPlane) {
    FlowQuantifier q;
    MeasurementPlane plane;
    plane.center = {10, 20, 30};
    plane.normal = {0, 0, 2};  // Not unit — should be normalized
    plane.radius = 25.0;
    plane.sampleSpacing = 0.5;
    q.setMeasurementPlane(plane);

    auto retrieved = q.measurementPlane();
    EXPECT_DOUBLE_EQ(retrieved.center[0], 10.0);
    EXPECT_NEAR(retrieved.normal[2], 1.0, 1e-10);  // Normalized
    EXPECT_DOUBLE_EQ(retrieved.radius, 25.0);
    EXPECT_DOUBLE_EQ(retrieved.sampleSpacing, 0.5);
}

TEST(FlowQuantifierTest, SetMeasurementPlaneFrom3Points) {
    FlowQuantifier q;
    // XY plane at z=5
    q.setMeasurementPlaneFrom3Points(
        {0, 0, 5}, {10, 0, 5}, {0, 10, 5});

    auto plane = q.measurementPlane();
    // Center is centroid
    EXPECT_NEAR(plane.center[0], 10.0 / 3.0, 1e-10);
    EXPECT_NEAR(plane.center[1], 10.0 / 3.0, 1e-10);
    EXPECT_NEAR(plane.center[2], 5.0, 1e-10);
    // Normal should be along Z
    EXPECT_NEAR(std::abs(plane.normal[2]), 1.0, 1e-10);
}

// =============================================================================
// Flow measurement tests
// =============================================================================

TEST(FlowQuantifierTest, MeasureFlow_NullField) {
    FlowQuantifier q;
    VelocityPhase phase;
    auto result = q.measureFlow(phase);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(FlowQuantifierTest, MeasureFlow_UniformZFlow) {
    FlowQuantifier q;

    // 20x20x20 grid, spacing 1mm, uniform flow Vz=10 cm/s
    auto phase = createUniformZFlow(20, 10.0f);

    // Measurement plane at center, normal along Z, radius 5mm
    MeasurementPlane plane;
    plane.center = {10, 10, 10};
    plane.normal = {0, 0, 1};
    plane.radius = 5.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    auto result = q.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    // All samples should have through-plane velocity = 10 cm/s
    EXPECT_NEAR(result->meanVelocity, 10.0, 0.1);
    EXPECT_NEAR(result->maxVelocity, 10.0, 0.1);
    EXPECT_GT(result->sampleCount, 0);
    EXPECT_GT(result->flowRate, 0.0);
    EXPECT_GT(result->crossSectionArea, 0.0);
}

TEST(FlowQuantifierTest, MeasureFlow_PerpendicularFlow) {
    FlowQuantifier q;

    // Flow along Z, but measure plane has normal along X
    auto phase = createUniformZFlow(20, 10.0f);

    MeasurementPlane plane;
    plane.center = {10, 10, 10};
    plane.normal = {1, 0, 0};  // Normal perpendicular to flow
    plane.radius = 5.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    auto result = q.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    // Through-plane velocity should be ~0 (flow is perpendicular to normal)
    EXPECT_NEAR(result->meanVelocity, 0.0, 0.01);
    EXPECT_NEAR(result->flowRate, 0.0, 0.01);
}

TEST(FlowQuantifierTest, MeasureFlow_PoiseuillePipeFlow) {
    FlowQuantifier q;

    // Poiseuille flow: v(r) = vMax × (1 - r²/R²)
    // Mean velocity = vMax / 2
    // Flow rate = π × R² × vMax / 2
    int dim = 40;
    float vMax = 100.0f;  // cm/s
    double pipeRadius = 8.0;  // mm (in grid units since spacing=1mm)

    auto phase = createPoiseuillePipeFlow(dim, vMax, pipeRadius);

    // Measurement plane at pipe center
    MeasurementPlane plane;
    plane.center = {(dim - 1) / 2.0, (dim - 1) / 2.0, (dim - 1) / 2.0};
    plane.normal = {0, 0, 1};
    plane.radius = pipeRadius + 2.0;  // Slightly larger than pipe
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    auto result = q.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    // Analytical: mean velocity over pipe cross-section = vMax / 2
    // But we're sampling over a larger area than the pipe, so mean velocity
    // over the full sample area will be lower. Max velocity should be vMax.
    EXPECT_NEAR(result->maxVelocity, vMax, 1.0);

    // Analytical flow rate = π × R² × vMax / 2
    // R = 8mm = 0.8cm, vMax = 100 cm/s
    // Q = π × 0.64 × 50 = 100.53 mL/s
    // But discrete sampling introduces error, accept ±10%
    double expectedFlowRate = M_PI * (pipeRadius / 10.0) * (pipeRadius / 10.0)
                              * vMax / 2.0;
    EXPECT_NEAR(result->flowRate, expectedFlowRate, expectedFlowRate * 0.10);
}

TEST(FlowQuantifierTest, MeasureFlow_ObliqueNormal) {
    FlowQuantifier q;

    // Flow along Z = 10 cm/s
    auto phase = createUniformZFlow(20, 10.0f);

    // Measurement plane with 45-degree tilted normal
    MeasurementPlane plane;
    plane.center = {10, 10, 10};
    plane.normal = {0, 1, 1};  // Will be normalized to {0, 1/√2, 1/√2}
    plane.radius = 5.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    auto result = q.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    // Through-plane = dot({0,0,10}, {0, 1/√2, 1/√2}) = 10/√2 ≈ 7.07
    EXPECT_NEAR(result->meanVelocity, 10.0 / std::sqrt(2.0), 0.5);
}

// =============================================================================
// Time-velocity curve tests
// =============================================================================

TEST(FlowQuantifierTest, ComputeTVC_EmptyPhases) {
    FlowQuantifier q;
    std::vector<VelocityPhase> empty;
    auto result = q.computeTimeVelocityCurve(empty, 40.0);
    ASSERT_FALSE(result.has_value());
}

TEST(FlowQuantifierTest, ComputeTVC_InvalidResolution) {
    FlowQuantifier q;
    std::vector<VelocityPhase> phases;
    phases.push_back(createUniformZFlow(10, 10.0f, 0));
    auto result = q.computeTimeVelocityCurve(phases, 0.0);
    ASSERT_FALSE(result.has_value());
}

TEST(FlowQuantifierTest, ComputeTVC_UniformFlow) {
    FlowQuantifier q;

    MeasurementPlane plane;
    plane.center = {5, 5, 5};
    plane.normal = {0, 0, 1};
    plane.radius = 3.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    // 5 phases with constant flow
    std::vector<VelocityPhase> phases;
    for (int i = 0; i < 5; ++i) {
        phases.push_back(createUniformZFlow(10, 10.0f, i));
    }

    auto result = q.computeTimeVelocityCurve(phases, 40.0);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->timePoints.size(), 5);
    EXPECT_EQ(result->flowRates.size(), 5);

    // All phases should have same flow rate
    for (size_t i = 1; i < result->flowRates.size(); ++i) {
        EXPECT_NEAR(result->flowRates[i], result->flowRates[0], 0.01);
    }

    // Stroke volume = sum of flowRates × dt (40ms = 0.04s)
    // All forward flow, no regurgitation
    EXPECT_GT(result->strokeVolume, 0.0);
    EXPECT_DOUBLE_EQ(result->regurgitantVolume, 0.0);
    EXPECT_DOUBLE_EQ(result->regurgitantFraction, 0.0);
}

TEST(FlowQuantifierTest, ComputeTVC_WithRegurgitation) {
    FlowQuantifier q;

    MeasurementPlane plane;
    plane.center = {5, 5, 5};
    plane.normal = {0, 0, 1};
    plane.radius = 3.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    // 4 phases: 2 forward, 2 backward (regurgitant)
    std::vector<VelocityPhase> phases;
    phases.push_back(createUniformZFlow(10, 10.0f, 0));   // Forward
    phases.push_back(createUniformZFlow(10, 10.0f, 1));   // Forward
    phases.push_back(createUniformZFlow(10, -5.0f, 2));   // Backward
    phases.push_back(createUniformZFlow(10, -5.0f, 3));   // Backward

    auto result = q.computeTimeVelocityCurve(phases, 40.0);
    ASSERT_TRUE(result.has_value());

    EXPECT_GT(result->strokeVolume, 0.0);
    EXPECT_GT(result->regurgitantVolume, 0.0);
    EXPECT_GT(result->regurgitantFraction, 0.0);
    EXPECT_LT(result->regurgitantFraction, 100.0);
}

// =============================================================================
// Pressure gradient tests
// =============================================================================

TEST(FlowQuantifierTest, PressureGradient_Zero) {
    EXPECT_DOUBLE_EQ(FlowQuantifier::estimatePressureGradient(0.0), 0.0);
}

TEST(FlowQuantifierTest, PressureGradient_Typical) {
    // V = 100 cm/s = 1 m/s → ΔP = 4 × 1² = 4 mmHg
    EXPECT_NEAR(FlowQuantifier::estimatePressureGradient(100.0), 4.0, 1e-10);

    // V = 200 cm/s = 2 m/s → ΔP = 4 × 4 = 16 mmHg
    EXPECT_NEAR(FlowQuantifier::estimatePressureGradient(200.0), 16.0, 1e-10);

    // V = 300 cm/s = 3 m/s → ΔP = 4 × 9 = 36 mmHg
    EXPECT_NEAR(FlowQuantifier::estimatePressureGradient(300.0), 36.0, 1e-10);

    // V = 50 cm/s = 0.5 m/s → ΔP = 4 × 0.25 = 1 mmHg
    EXPECT_NEAR(FlowQuantifier::estimatePressureGradient(50.0), 1.0, 1e-10);
}

// =============================================================================
// CSV export tests
// =============================================================================

TEST(FlowQuantifierTest, ExportToCSV_EmptyPath) {
    TimeVelocityCurve tvc;
    auto result = FlowQuantifier::exportToCSV(tvc, "");
    ASSERT_FALSE(result.has_value());
}

TEST(FlowQuantifierTest, ExportToCSV_ValidData) {
    TimeVelocityCurve tvc;
    tvc.timePoints = {0, 40, 80};
    tvc.meanVelocities = {10.0, 20.0, 15.0};
    tvc.maxVelocities = {15.0, 30.0, 22.0};
    tvc.flowRates = {5.0, 10.0, 7.5};
    tvc.strokeVolume = 50.0;
    tvc.regurgitantVolume = 5.0;
    tvc.regurgitantFraction = 9.09;

    auto result = FlowQuantifier::exportToCSV(tvc, kTestCSVPath);
    ASSERT_TRUE(result.has_value());

    // Verify file exists and has content
    std::ifstream ifs(kTestCSVPath);
    ASSERT_TRUE(ifs.is_open());

    std::string line;
    std::getline(ifs, line);
    EXPECT_EQ(line, "Time_ms,MeanVelocity_cm_s,MaxVelocity_cm_s,FlowRate_mL_s");

    // Check data rows
    std::getline(ifs, line);
    EXPECT_FALSE(line.empty());

    // Clean up
    std::filesystem::remove(kTestCSVPath);
}

// =============================================================================
// Non-perpendicular measurement plane tests (Issue #202)
// =============================================================================

TEST(FlowQuantifierTest, MeasureFlow_30DegreeAngle) {
    FlowQuantifier q;

    // Flow along Z = 10 cm/s
    auto phase = createUniformZFlow(20, 10.0f);

    // Measurement plane tilted 30° from Z axis
    // Normal = (0, sin30°, cos30°) = (0, 0.5, 0.866)
    MeasurementPlane plane;
    plane.center = {10, 10, 10};
    plane.normal = {0, std::sin(M_PI / 6.0), std::cos(M_PI / 6.0)};
    plane.radius = 5.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    auto result = q.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    // Through-plane velocity = dot({0,0,10}, normalized_normal)
    // = 10 × cos(30°) ≈ 8.66 cm/s
    EXPECT_NEAR(result->meanVelocity, 10.0 * std::cos(M_PI / 6.0), 0.5);
}

TEST(FlowQuantifierTest, MeasureFlow_60DegreeAngle) {
    FlowQuantifier q;

    auto phase = createUniformZFlow(20, 10.0f);

    // 60° tilted plane: normal = (0, sin60°, cos60°) = (0, 0.866, 0.5)
    MeasurementPlane plane;
    plane.center = {10, 10, 10};
    plane.normal = {0, std::sin(M_PI / 3.0), std::cos(M_PI / 3.0)};
    plane.radius = 5.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    auto result = q.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    // Through-plane = 10 × cos(60°) = 5.0 cm/s
    EXPECT_NEAR(result->meanVelocity, 10.0 * std::cos(M_PI / 3.0), 0.5);
}

// =============================================================================
// Temporal resolution edge case tests (Issue #202)
// =============================================================================

TEST(FlowQuantifierTest, ComputeTVC_SinglePhase) {
    FlowQuantifier q;

    MeasurementPlane plane;
    plane.center = {5, 5, 5};
    plane.normal = {0, 0, 1};
    plane.radius = 3.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    // Single phase — edge case for integration
    std::vector<VelocityPhase> phases;
    phases.push_back(createUniformZFlow(10, 10.0f, 0));

    auto result = q.computeTimeVelocityCurve(phases, 40.0);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->timePoints.size(), 1);
    EXPECT_EQ(result->flowRates.size(), 1);
    // Single point: stroke volume should still be computed
    EXPECT_GE(result->strokeVolume, 0.0);
}

TEST(FlowQuantifierTest, ComputeTVC_HighRegurgitation) {
    FlowQuantifier q;

    MeasurementPlane plane;
    plane.center = {5, 5, 5};
    plane.normal = {0, 0, 1};
    plane.radius = 3.0;
    plane.sampleSpacing = 1.0;
    q.setMeasurementPlane(plane);

    // 6 phases: 1 forward, 5 backward → high regurgitant fraction (>50%)
    std::vector<VelocityPhase> phases;
    phases.push_back(createUniformZFlow(10, 20.0f, 0));   // Forward
    for (int i = 1; i <= 5; ++i) {
        phases.push_back(createUniformZFlow(10, -10.0f, i));  // Backward
    }

    auto result = q.computeTimeVelocityCurve(phases, 40.0);
    ASSERT_TRUE(result.has_value());

    EXPECT_GT(result->regurgitantFraction, 50.0)
        << "5 backward phases vs 1 forward should yield >50% regurgitation";
}

// =============================================================================
// Pressure gradient edge cases (Issue #202)
// =============================================================================

TEST(FlowQuantifierTest, PressureGradient_NegativeVelocity) {
    // Modified Bernoulli uses absolute velocity or squared
    double p = FlowQuantifier::estimatePressureGradient(-100.0);
    // ΔP = 4 × V²; with V = -100 cm/s = -1 m/s → ΔP = 4 × 1 = 4 mmHg
    EXPECT_NEAR(p, 4.0, 1e-10);
}

TEST(FlowQuantifierTest, PressureGradient_VeryHighVelocity) {
    // V = 500 cm/s = 5 m/s → ΔP = 4 × 25 = 100 mmHg (severe stenosis)
    double p = FlowQuantifier::estimatePressureGradient(500.0);
    EXPECT_NEAR(p, 100.0, 1e-10);
}

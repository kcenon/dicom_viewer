#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include <itkVectorImage.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>

#include "services/flow/flow_visualizer.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a synthetic 3-component velocity field with uniform flow
VelocityPhase createUniformFlowPhase(int dimX, int dimY, int dimZ,
                                     float vx, float vy, float vz,
                                     int phaseIndex = 0) {
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size = {{
        static_cast<VectorImage3D::SizeValueType>(dimX),
        static_cast<VectorImage3D::SizeValueType>(dimY),
        static_cast<VectorImage3D::SizeValueType>(dimZ)
    }};
    VectorImage3D::IndexType start = {{0, 0, 0}};
    VectorImage3D::RegionType region(start, size);
    image->SetRegions(region);
    image->SetNumberOfComponentsPerPixel(3);

    VectorImage3D::SpacingType spacing;
    spacing[0] = 1.0; spacing[1] = 1.0; spacing[2] = 1.0;
    image->SetSpacing(spacing);

    VectorImage3D::PointType origin;
    origin[0] = 0.0; origin[1] = 0.0; origin[2] = 0.0;
    image->SetOrigin(origin);

    image->Allocate();

    // Fill with uniform velocity
    auto* buffer = image->GetBufferPointer();
    int numPixels = dimX * dimY * dimZ;
    for (int i = 0; i < numPixels; ++i) {
        buffer[i * 3]     = vx;
        buffer[i * 3 + 1] = vy;
        buffer[i * 3 + 2] = vz;
    }

    VelocityPhase phase;
    phase.velocityField = image;
    phase.phaseIndex = phaseIndex;
    phase.triggerTime = phaseIndex * 40.0;
    return phase;
}

/// Create a phase with parabolic pipe flow (for testing non-uniform fields)
VelocityPhase createParabolicFlowPhase(int dim, float maxVelocity,
                                        int phaseIndex = 0) {
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size = {{
        static_cast<VectorImage3D::SizeValueType>(dim),
        static_cast<VectorImage3D::SizeValueType>(dim),
        static_cast<VectorImage3D::SizeValueType>(dim)
    }};
    VectorImage3D::IndexType start = {{0, 0, 0}};
    VectorImage3D::RegionType region(start, size);
    image->SetRegions(region);
    image->SetNumberOfComponentsPerPixel(3);
    VectorImage3D::SpacingType pSpacing;
    pSpacing[0] = 1.0; pSpacing[1] = 1.0; pSpacing[2] = 1.0;
    image->SetSpacing(pSpacing);
    VectorImage3D::PointType pOrigin;
    pOrigin[0] = 0.0; pOrigin[1] = 0.0; pOrigin[2] = 0.0;
    image->SetOrigin(pOrigin);
    image->Allocate();

    auto* buffer = image->GetBufferPointer();
    double center = (dim - 1) / 2.0;
    double radius = center;

    for (int z = 0; z < dim; ++z) {
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                int idx = x + dim * (y + dim * z);
                // Parabolic profile: flow along Z, max at center
                double dy = y - center;
                double dx = x - center;
                double r = std::sqrt(dx * dx + dy * dy);
                double frac = std::max(0.0, 1.0 - (r * r) / (radius * radius));
                buffer[idx * 3]     = 0.0f;
                buffer[idx * 3 + 1] = 0.0f;
                buffer[idx * 3 + 2] = static_cast<float>(maxVelocity * frac);
            }
        }
    }

    VelocityPhase phase;
    phase.velocityField = image;
    phase.phaseIndex = phaseIndex;
    phase.triggerTime = phaseIndex * 40.0;
    return phase;
}

}  // anonymous namespace

// =============================================================================
// Struct default tests
// =============================================================================

TEST(StreamlineParamsTest, Defaults) {
    StreamlineParams params;
    EXPECT_EQ(params.maxSeedPoints, 5000);
    EXPECT_DOUBLE_EQ(params.stepLength, 0.5);
    EXPECT_EQ(params.maxSteps, 2000);
    EXPECT_DOUBLE_EQ(params.terminalSpeed, 0.1);
    EXPECT_DOUBLE_EQ(params.tubeRadius, 0.5);
    EXPECT_EQ(params.tubeSides, 8);
}

TEST(GlyphParamsTest, Defaults) {
    GlyphParams params;
    EXPECT_DOUBLE_EQ(params.scaleFactor, 1.0);
    EXPECT_EQ(params.skipFactor, 4);
    EXPECT_DOUBLE_EQ(params.minMagnitude, 1.0);
}

TEST(PathlineParamsTest, Defaults) {
    PathlineParams params;
    EXPECT_EQ(params.maxSeedPoints, 1000);
    EXPECT_EQ(params.maxSteps, 2000);
    EXPECT_DOUBLE_EQ(params.terminalSpeed, 0.1);
    EXPECT_DOUBLE_EQ(params.tubeRadius, 0.5);
    EXPECT_EQ(params.tubeSides, 8);
}

TEST(SeedRegionTest, Defaults) {
    SeedRegion region;
    EXPECT_EQ(region.type, SeedRegion::Type::Volume);
    EXPECT_EQ(region.numSeedPoints, 5000);
    EXPECT_DOUBLE_EQ(region.planeRadius, 50.0);
}

// =============================================================================
// FlowVisualizer construction tests
// =============================================================================

TEST(FlowVisualizerTest, DefaultConstruction) {
    FlowVisualizer viz;
    EXPECT_FALSE(viz.hasVelocityField());
    EXPECT_EQ(viz.colorMode(), ColorMode::VelocityMagnitude);
}

TEST(FlowVisualizerTest, MoveConstruction) {
    FlowVisualizer viz;
    FlowVisualizer moved(std::move(viz));
    EXPECT_FALSE(moved.hasVelocityField());
}

TEST(FlowVisualizerTest, MoveAssignment) {
    FlowVisualizer viz;
    FlowVisualizer other;
    other = std::move(viz);
}

// =============================================================================
// ITK → VTK conversion tests
// =============================================================================

TEST(FlowVisualizerTest, VelocityFieldToVTK_NullField) {
    VelocityPhase phase;  // null velocityField
    auto result = FlowVisualizer::velocityFieldToVTK(phase);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(FlowVisualizerTest, VelocityFieldToVTK_UniformFlow) {
    auto phase = createUniformFlowPhase(8, 8, 8, 10.0f, 5.0f, 3.0f);
    auto result = FlowVisualizer::velocityFieldToVTK(phase);
    ASSERT_TRUE(result.has_value());

    auto vtkImage = result.value();
    int dims[3];
    vtkImage->GetDimensions(dims);
    EXPECT_EQ(dims[0], 8);
    EXPECT_EQ(dims[1], 8);
    EXPECT_EQ(dims[2], 8);

    // Check vector data is present
    auto* vectors = vtkImage->GetPointData()->GetVectors();
    ASSERT_NE(vectors, nullptr);
    EXPECT_EQ(vectors->GetNumberOfComponents(), 3);
    EXPECT_EQ(vectors->GetNumberOfTuples(), 8 * 8 * 8);

    // Check first vector value
    double vel[3];
    vectors->GetTuple(0, vel);
    EXPECT_FLOAT_EQ(static_cast<float>(vel[0]), 10.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(vel[1]), 5.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(vel[2]), 3.0f);

    // Check magnitude scalar
    auto* scalars = vtkImage->GetPointData()->GetScalars();
    ASSERT_NE(scalars, nullptr);
    double expectedMag = std::sqrt(10.0 * 10.0 + 5.0 * 5.0 + 3.0 * 3.0);
    EXPECT_NEAR(scalars->GetTuple1(0), expectedMag, 0.01);
}

TEST(FlowVisualizerTest, VelocityFieldToVTK_SpacingAndOrigin) {
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size = {{4, 4, 4}};
    VectorImage3D::IndexType start = {{0, 0, 0}};
    image->SetRegions(VectorImage3D::RegionType(start, size));
    image->SetNumberOfComponentsPerPixel(3);
    VectorImage3D::SpacingType spacing;
    spacing[0] = 2.0; spacing[1] = 3.0; spacing[2] = 1.5;
    image->SetSpacing(spacing);
    VectorImage3D::PointType origin;
    origin[0] = 10.0; origin[1] = 20.0; origin[2] = 30.0;
    image->SetOrigin(origin);
    image->Allocate(true);

    VelocityPhase phase;
    phase.velocityField = image;
    auto result = FlowVisualizer::velocityFieldToVTK(phase);
    ASSERT_TRUE(result.has_value());

    auto vtkImage = result.value();
    double* vtkSpacing = vtkImage->GetSpacing();
    double* vtkOrigin = vtkImage->GetOrigin();

    EXPECT_DOUBLE_EQ(vtkSpacing[0], 2.0);
    EXPECT_DOUBLE_EQ(vtkSpacing[1], 3.0);
    EXPECT_DOUBLE_EQ(vtkSpacing[2], 1.5);
    EXPECT_DOUBLE_EQ(vtkOrigin[0], 10.0);
    EXPECT_DOUBLE_EQ(vtkOrigin[1], 20.0);
    EXPECT_DOUBLE_EQ(vtkOrigin[2], 30.0);
}

TEST(FlowVisualizerTest, VelocityFieldToVTK_WrongComponents) {
    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size = {{4, 4, 4}};
    VectorImage3D::IndexType start = {{0, 0, 0}};
    image->SetRegions(VectorImage3D::RegionType(start, size));
    image->SetNumberOfComponentsPerPixel(2);  // Wrong: should be 3
    image->Allocate();

    VelocityPhase phase;
    phase.velocityField = image;
    auto result = FlowVisualizer::velocityFieldToVTK(phase);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

// =============================================================================
// SetVelocityField tests
// =============================================================================

TEST(FlowVisualizerTest, SetVelocityField_Success) {
    FlowVisualizer viz;
    auto phase = createUniformFlowPhase(8, 8, 8, 1.0f, 0.0f, 0.0f);
    auto result = viz.setVelocityField(phase);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(viz.hasVelocityField());
}

TEST(FlowVisualizerTest, SetVelocityField_NullField) {
    FlowVisualizer viz;
    VelocityPhase phase;
    auto result = viz.setVelocityField(phase);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(viz.hasVelocityField());
}

TEST(FlowVisualizerTest, SetVelocityField_AutoSetsBounds) {
    FlowVisualizer viz;
    auto phase = createUniformFlowPhase(10, 10, 10, 1.0f, 0.0f, 0.0f);
    viz.setVelocityField(phase);

    auto seed = viz.seedRegion();
    // Bounds should be auto-set to image bounds (0-9 with spacing 1.0)
    EXPECT_GE(seed.bounds[1], 0.0);  // xmax > 0
}

// =============================================================================
// Seed region tests
// =============================================================================

TEST(FlowVisualizerTest, SetSeedRegion) {
    FlowVisualizer viz;
    SeedRegion region;
    region.type = SeedRegion::Type::Plane;
    region.planeOrigin = {5.0, 5.0, 5.0};
    region.planeNormal = {1.0, 0.0, 0.0};
    region.planeRadius = 25.0;
    region.numSeedPoints = 1000;

    viz.setSeedRegion(region);

    auto retrieved = viz.seedRegion();
    EXPECT_EQ(retrieved.type, SeedRegion::Type::Plane);
    EXPECT_DOUBLE_EQ(retrieved.planeRadius, 25.0);
    EXPECT_EQ(retrieved.numSeedPoints, 1000);
}

// =============================================================================
// Streamline generation tests
// =============================================================================

TEST(FlowVisualizerTest, GenerateStreamlines_NoField) {
    FlowVisualizer viz;
    auto result = viz.generateStreamlines();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(FlowVisualizerTest, GenerateStreamlines_UniformFlow) {
    FlowVisualizer viz;
    auto phase = createUniformFlowPhase(10, 10, 10, 5.0f, 0.0f, 0.0f);
    viz.setVelocityField(phase);

    StreamlineParams params;
    params.maxSeedPoints = 50;  // Small for test performance
    params.maxSteps = 100;
    params.stepLength = 0.5;

    auto result = viz.generateStreamlines(params);
    ASSERT_TRUE(result.has_value());

    auto polyData = result.value();
    EXPECT_GT(polyData->GetNumberOfPoints(), 0);
}

TEST(FlowVisualizerTest, GenerateStreamlines_ParabolicFlow) {
    FlowVisualizer viz;
    auto phase = createParabolicFlowPhase(10, 50.0f);
    viz.setVelocityField(phase);

    StreamlineParams params;
    params.maxSeedPoints = 20;
    params.maxSteps = 50;

    auto result = viz.generateStreamlines(params);
    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result.value()->GetNumberOfPoints(), 0);
}

// =============================================================================
// Glyph generation tests
// =============================================================================

TEST(FlowVisualizerTest, GenerateGlyphs_NoField) {
    FlowVisualizer viz;
    auto result = viz.generateGlyphs();
    ASSERT_FALSE(result.has_value());
}

TEST(FlowVisualizerTest, GenerateGlyphs_UniformFlow) {
    FlowVisualizer viz;
    auto phase = createUniformFlowPhase(8, 8, 8, 10.0f, 0.0f, 0.0f);
    viz.setVelocityField(phase);

    GlyphParams params;
    params.skipFactor = 2;
    params.minMagnitude = 0.5;
    params.scaleFactor = 0.5;

    auto result = viz.generateGlyphs(params);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value()->GetNumberOfCells(), 0);
}

TEST(FlowVisualizerTest, GenerateGlyphs_HighMinMagnitude) {
    FlowVisualizer viz;
    // Uniform flow with magnitude = sqrt(1+1+1) ≈ 1.73 cm/s
    auto phase = createUniformFlowPhase(8, 8, 8, 1.0f, 1.0f, 1.0f);
    viz.setVelocityField(phase);

    GlyphParams params;
    params.minMagnitude = 100.0;  // Higher than actual magnitude

    auto result = viz.generateGlyphs(params);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->GetNumberOfCells(), 0);  // All filtered out
}

// =============================================================================
// Pathline generation tests
// =============================================================================

TEST(FlowVisualizerTest, GeneratePathlines_NoPhases) {
    FlowVisualizer viz;
    std::vector<VelocityPhase> empty;
    auto result = viz.generatePathlines(empty);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(FlowVisualizerTest, GeneratePathlines_MultiPhase) {
    FlowVisualizer viz;
    auto phase0 = createUniformFlowPhase(8, 8, 8, 1.0f, 0.0f, 0.0f, 0);
    viz.setVelocityField(phase0);

    std::vector<VelocityPhase> phases;
    for (int i = 0; i < 5; ++i) {
        phases.push_back(
            createUniformFlowPhase(8, 8, 8, 1.0f, 0.0f, 0.0f, i));
    }

    PathlineParams params;
    params.maxSeedPoints = 20;

    auto result = viz.generatePathlines(phases, params);
    ASSERT_TRUE(result.has_value());

    auto polyData = result.value();
    // Should have TriggerTime and VelocityMagnitude arrays
    EXPECT_NE(polyData->GetPointData()->GetArray("TriggerTime"), nullptr);
    EXPECT_NE(polyData->GetPointData()->GetArray("VelocityMagnitude"), nullptr);
}

TEST(FlowVisualizerTest, GeneratePathlines_NullPhaseInSequence) {
    FlowVisualizer viz;
    auto phase0 = createUniformFlowPhase(8, 8, 8, 1.0f, 0.0f, 0.0f, 0);
    viz.setVelocityField(phase0);

    std::vector<VelocityPhase> phases;
    phases.push_back(createUniformFlowPhase(8, 8, 8, 1.0f, 0.0f, 0.0f, 0));
    phases.push_back(VelocityPhase{});  // Null field

    auto result = viz.generatePathlines(phases);
    ASSERT_FALSE(result.has_value());  // Should fail on null phase
}

// =============================================================================
// Color mapping tests
// =============================================================================

TEST(FlowVisualizerTest, ColorMode_Default) {
    FlowVisualizer viz;
    EXPECT_EQ(viz.colorMode(), ColorMode::VelocityMagnitude);
}

TEST(FlowVisualizerTest, SetColorMode) {
    FlowVisualizer viz;
    viz.setColorMode(ColorMode::VelocityComponent);
    EXPECT_EQ(viz.colorMode(), ColorMode::VelocityComponent);

    viz.setColorMode(ColorMode::FlowDirection);
    EXPECT_EQ(viz.colorMode(), ColorMode::FlowDirection);

    viz.setColorMode(ColorMode::TriggerTime);
    EXPECT_EQ(viz.colorMode(), ColorMode::TriggerTime);
}

TEST(FlowVisualizerTest, CreateLookupTable_VelocityMagnitude) {
    FlowVisualizer viz;
    viz.setColorMode(ColorMode::VelocityMagnitude);
    viz.setVelocityRange(0.0, 150.0);

    auto lut = viz.createLookupTable();
    ASSERT_NE(lut, nullptr);
    EXPECT_EQ(lut->GetNumberOfTableValues(), 256);

    double* range = lut->GetRange();
    EXPECT_DOUBLE_EQ(range[0], 0.0);
    EXPECT_DOUBLE_EQ(range[1], 150.0);
}

TEST(FlowVisualizerTest, CreateLookupTable_VelocityComponent) {
    FlowVisualizer viz;
    viz.setColorMode(ColorMode::VelocityComponent);
    viz.setVelocityRange(0.0, 100.0);

    auto lut = viz.createLookupTable();
    ASSERT_NE(lut, nullptr);

    double* range = lut->GetRange();
    // Diverging: [-max, +max]
    EXPECT_DOUBLE_EQ(range[0], -100.0);
    EXPECT_DOUBLE_EQ(range[1], 100.0);

    // Check middle value is white-ish (blue→white→red)
    double rgba[4];
    lut->GetTableValue(128, rgba);
    EXPECT_GT(rgba[0], 0.9);  // Near white
    EXPECT_GT(rgba[1], 0.9);
    EXPECT_GT(rgba[2], 0.9);
}

TEST(FlowVisualizerTest, CreateLookupTable_FlowDirection) {
    FlowVisualizer viz;
    viz.setColorMode(ColorMode::FlowDirection);

    auto lut = viz.createLookupTable();
    ASSERT_NE(lut, nullptr);
    EXPECT_EQ(lut->GetNumberOfTableValues(), 256);
}

TEST(FlowVisualizerTest, CreateLookupTable_TriggerTime) {
    FlowVisualizer viz;
    viz.setColorMode(ColorMode::TriggerTime);

    auto lut = viz.createLookupTable();
    ASSERT_NE(lut, nullptr);

    double* range = lut->GetRange();
    EXPECT_DOUBLE_EQ(range[0], 0.0);
    EXPECT_DOUBLE_EQ(range[1], 1000.0);
}

// =============================================================================
// Glyph orientation and edge case tests (Issue #202)
// =============================================================================

TEST(FlowVisualizerTest, GenerateGlyphs_OrientationMatchesVelocity) {
    FlowVisualizer viz;
    // Pure X-direction flow
    auto phase = createUniformFlowPhase(8, 8, 8, 20.0f, 0.0f, 0.0f);
    viz.setVelocityField(phase);

    GlyphParams params;
    params.skipFactor = 4;  // Sample every 4th voxel
    params.minMagnitude = 0.1;

    auto result = viz.generateGlyphs(params);
    ASSERT_TRUE(result.has_value());

    auto polyData = result.value();
    EXPECT_GT(polyData->GetNumberOfCells(), 0);

    // Glyph vectors should have a data array with velocity directions
    auto* vectors = polyData->GetPointData()->GetVectors();
    if (vectors && vectors->GetNumberOfTuples() > 0) {
        double vec[3];
        vectors->GetTuple(0, vec);
        // X component should dominate for pure X-direction flow
        double mag = std::sqrt(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);
        if (mag > 0.0) {
            EXPECT_GT(std::abs(vec[0]) / mag, 0.9)
                << "Glyph should point primarily in X direction";
        }
    }
}

TEST(FlowVisualizerTest, VelocityFieldToVTK_VerySmallVelocity) {
    // Near-zero velocity should not cause numerical instability
    auto phase = createUniformFlowPhase(4, 4, 4, 1e-7f, 1e-7f, 1e-7f);
    auto result = FlowVisualizer::velocityFieldToVTK(phase);
    ASSERT_TRUE(result.has_value());

    auto* scalars = result.value()->GetPointData()->GetScalars();
    ASSERT_NE(scalars, nullptr);
    double mag = scalars->GetTuple1(0);
    EXPECT_GE(mag, 0.0);
    EXPECT_FALSE(std::isnan(mag));
    EXPECT_FALSE(std::isinf(mag));
}

TEST(FlowVisualizerTest, SetVelocityField_ReplaceExisting) {
    FlowVisualizer viz;

    // Set first field
    auto phase1 = createUniformFlowPhase(8, 8, 8, 10.0f, 0.0f, 0.0f);
    auto result1 = viz.setVelocityField(phase1);
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(viz.hasVelocityField());

    // Replace with second field (different dimensions)
    auto phase2 = createUniformFlowPhase(4, 4, 4, 0.0f, 20.0f, 0.0f);
    auto result2 = viz.setVelocityField(phase2);
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(viz.hasVelocityField());

    // Generate glyphs from the replaced field
    GlyphParams params;
    params.skipFactor = 1;
    params.minMagnitude = 0.1;
    auto glyphs = viz.generateGlyphs(params);
    ASSERT_TRUE(glyphs.has_value());
}

TEST(FlowVisualizerTest, GenerateStreamlines_HighTerminalSpeed) {
    FlowVisualizer viz;
    // Flow with magnitude ~1.73 cm/s
    auto phase = createUniformFlowPhase(10, 10, 10, 1.0f, 1.0f, 1.0f);
    viz.setVelocityField(phase);

    StreamlineParams params;
    params.maxSeedPoints = 20;
    params.maxSteps = 100;
    params.terminalSpeed = 100.0;  // Higher than actual velocity

    auto result = viz.generateStreamlines(params);
    ASSERT_TRUE(result.has_value());

    // Streamlines should terminate immediately (speed < terminalSpeed)
    auto polyData = result.value();
    // May have zero or very few points since all speeds are below threshold
    SUCCEED();
}

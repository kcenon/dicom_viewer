#include <gtest/gtest.h>

#include <cmath>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

#include "services/render/hemodynamic_overlay_renderer.hpp"
#include "services/mpr_renderer.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a 3D scalar field (e.g., velocity magnitude) for testing
vtkSmartPointer<vtkImageData> createScalarField(
    int dimX, int dimY, int dimZ,
    double spacing = 1.0) {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(dimX, dimY, dimZ);
    image->SetSpacing(spacing, spacing, spacing);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);

    auto* ptr = static_cast<float*>(image->GetScalarPointer());
    int total = dimX * dimY * dimZ;
    for (int i = 0; i < total; ++i) {
        // Gradient pattern: value increases with voxel index
        ptr[i] = static_cast<float>(i) / static_cast<float>(total) * 100.0f;
    }
    return image;
}

/// Create a 3D vector field (Vx, Vy, Vz) for testing computeVelocityMagnitude
vtkSmartPointer<vtkImageData> createVectorField(
    int dimX, int dimY, int dimZ,
    double vx, double vy, double vz) {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(dimX, dimY, dimZ);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 3);

    auto* ptr = static_cast<float*>(image->GetScalarPointer());
    int total = dimX * dimY * dimZ;
    for (int i = 0; i < total; ++i) {
        ptr[i * 3 + 0] = static_cast<float>(vx);
        ptr[i * 3 + 1] = static_cast<float>(vy);
        ptr[i * 3 + 2] = static_cast<float>(vz);
    }
    return image;
}

/// Create a non-uniform vector field with varying magnitudes
vtkSmartPointer<vtkImageData> createGradientVectorField(
    int dimX, int dimY, int dimZ) {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(dimX, dimY, dimZ);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 3);

    auto* ptr = static_cast<float*>(image->GetScalarPointer());
    int idx = 0;
    for (int z = 0; z < dimZ; ++z) {
        for (int y = 0; y < dimY; ++y) {
            for (int x = 0; x < dimX; ++x) {
                ptr[idx * 3 + 0] = static_cast<float>(x);  // Vx
                ptr[idx * 3 + 1] = static_cast<float>(y);  // Vy
                ptr[idx * 3 + 2] = static_cast<float>(z);  // Vz
                ++idx;
            }
        }
    }
    return image;
}

} // anonymous namespace

// =============================================================================
// Construction and Default State
// =============================================================================

TEST(HemodynamicOverlayRendererTest, DefaultState) {
    HemodynamicOverlayRenderer renderer;
    EXPECT_FALSE(renderer.hasScalarField());
    EXPECT_TRUE(renderer.isVisible());
    EXPECT_DOUBLE_EQ(renderer.opacity(), 0.5);
    EXPECT_EQ(renderer.overlayType(), OverlayType::VelocityMagnitude);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::Jet);

    auto [minVal, maxVal] = renderer.scalarRange();
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 100.0);
}

TEST(HemodynamicOverlayRendererTest, MoveConstructor) {
    HemodynamicOverlayRenderer r1;
    r1.setOpacity(0.8);
    r1.setOverlayType(OverlayType::VelocityX);

    HemodynamicOverlayRenderer r2(std::move(r1));
    EXPECT_DOUBLE_EQ(r2.opacity(), 0.8);
    EXPECT_EQ(r2.overlayType(), OverlayType::VelocityX);
}

// =============================================================================
// Scalar Field Input
// =============================================================================

TEST(HemodynamicOverlayRendererTest, SetScalarField) {
    HemodynamicOverlayRenderer renderer;
    auto field = createScalarField(16, 16, 16);

    renderer.setScalarField(field);
    EXPECT_TRUE(renderer.hasScalarField());

    renderer.setScalarField(nullptr);
    EXPECT_FALSE(renderer.hasScalarField());
}

// =============================================================================
// Visibility and Opacity
// =============================================================================

TEST(HemodynamicOverlayRendererTest, VisibilityToggle) {
    HemodynamicOverlayRenderer renderer;
    EXPECT_TRUE(renderer.isVisible());

    renderer.setVisible(false);
    EXPECT_FALSE(renderer.isVisible());

    renderer.setVisible(true);
    EXPECT_TRUE(renderer.isVisible());
}

TEST(HemodynamicOverlayRendererTest, OpacityClamping) {
    HemodynamicOverlayRenderer renderer;

    renderer.setOpacity(0.75);
    EXPECT_DOUBLE_EQ(renderer.opacity(), 0.75);

    renderer.setOpacity(-0.5);
    EXPECT_DOUBLE_EQ(renderer.opacity(), 0.0);

    renderer.setOpacity(1.5);
    EXPECT_DOUBLE_EQ(renderer.opacity(), 1.0);
}

// =============================================================================
// Overlay Type
// =============================================================================

TEST(HemodynamicOverlayRendererTest, OverlayTypeSettings) {
    HemodynamicOverlayRenderer renderer;

    renderer.setOverlayType(OverlayType::VelocityX);
    EXPECT_EQ(renderer.overlayType(), OverlayType::VelocityX);

    renderer.setOverlayType(OverlayType::VelocityZ);
    EXPECT_EQ(renderer.overlayType(), OverlayType::VelocityZ);

    renderer.setOverlayType(OverlayType::Vorticity);
    EXPECT_EQ(renderer.overlayType(), OverlayType::Vorticity);

    renderer.setOverlayType(OverlayType::EnergyLoss);
    EXPECT_EQ(renderer.overlayType(), OverlayType::EnergyLoss);
}

// =============================================================================
// Colormap
// =============================================================================

TEST(HemodynamicOverlayRendererTest, ColormapPresetSwitch) {
    HemodynamicOverlayRenderer renderer;

    renderer.setColormapPreset(ColormapPreset::HotMetal);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::HotMetal);

    renderer.setColormapPreset(ColormapPreset::CoolWarm);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::CoolWarm);

    renderer.setColormapPreset(ColormapPreset::Viridis);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::Viridis);
}

TEST(HemodynamicOverlayRendererTest, ScalarRangeControl) {
    HemodynamicOverlayRenderer renderer;

    renderer.setScalarRange(10.0, 200.0);
    auto [minVal, maxVal] = renderer.scalarRange();
    EXPECT_DOUBLE_EQ(minVal, 10.0);
    EXPECT_DOUBLE_EQ(maxVal, 200.0);
}

TEST(HemodynamicOverlayRendererTest, LookupTableCreated) {
    HemodynamicOverlayRenderer renderer;
    renderer.setScalarRange(0.0, 50.0);

    auto lut = renderer.getLookupTable();
    ASSERT_NE(lut, nullptr);
    EXPECT_EQ(lut->GetNumberOfTableValues(), 256);

    double range[2];
    lut->GetTableRange(range);
    EXPECT_DOUBLE_EQ(range[0], 0.0);
    EXPECT_DOUBLE_EQ(range[1], 50.0);
}

// =============================================================================
// Renderer Attachment
// =============================================================================

TEST(HemodynamicOverlayRendererTest, SetRenderers) {
    HemodynamicOverlayRenderer renderer;

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();

    renderer.setRenderers(axial, coronal, sagittal);

    // After setRenderers, actors should be added to renderers
    EXPECT_GT(axial->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(coronal->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(sagittal->GetViewProps()->GetNumberOfItems(), 0);
}

TEST(HemodynamicOverlayRendererTest, RendererReattachment) {
    HemodynamicOverlayRenderer renderer;

    auto r1 = vtkSmartPointer<vtkRenderer>::New();
    auto r2 = vtkSmartPointer<vtkRenderer>::New();
    auto r3 = vtkSmartPointer<vtkRenderer>::New();

    renderer.setRenderers(r1, r2, r3);
    int countBefore = r1->GetViewProps()->GetNumberOfItems();

    // Re-attach to new renderers
    auto r4 = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(r4, r2, r3);

    // Old renderer should have actors removed
    EXPECT_EQ(r1->GetViewProps()->GetNumberOfItems(), countBefore - 1);
    // New renderer should have actors added
    EXPECT_GT(r4->GetViewProps()->GetNumberOfItems(), 0);
}

// =============================================================================
// Slice Position
// =============================================================================

TEST(HemodynamicOverlayRendererTest, SetSlicePositionWithoutField) {
    HemodynamicOverlayRenderer renderer;
    auto result = renderer.setSlicePosition(MPRPlane::Axial, 50.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

TEST(HemodynamicOverlayRendererTest, SetSlicePositionSuccess) {
    HemodynamicOverlayRenderer renderer;
    renderer.setScalarField(createScalarField(32, 32, 32));

    auto result = renderer.setSlicePosition(MPRPlane::Axial, 15.0);
    EXPECT_TRUE(result.has_value());

    result = renderer.setSlicePosition(MPRPlane::Coronal, 10.0);
    EXPECT_TRUE(result.has_value());

    result = renderer.setSlicePosition(MPRPlane::Sagittal, 5.0);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// Update Pipeline
// =============================================================================

TEST(HemodynamicOverlayRendererTest, UpdateWithScalarField) {
    HemodynamicOverlayRenderer renderer;
    auto field = createScalarField(16, 16, 16);
    renderer.setScalarField(field);

    // Should not crash
    renderer.update();
    renderer.updatePlane(MPRPlane::Axial);
    renderer.updatePlane(MPRPlane::Coronal);
    renderer.updatePlane(MPRPlane::Sagittal);
}

TEST(HemodynamicOverlayRendererTest, UpdateWithoutScalarField) {
    HemodynamicOverlayRenderer renderer;
    // Should not crash when no data is set
    renderer.update();
    renderer.updatePlane(MPRPlane::Axial);
}

// =============================================================================
// Velocity Magnitude Computation
// =============================================================================

TEST(HemodynamicOverlayRendererTest, ComputeVelocityMagnitudeUniform) {
    // V = (3, 4, 0) → |V| = 5.0
    auto vecField = createVectorField(8, 8, 8, 3.0, 4.0, 0.0);
    auto result = HemodynamicOverlayRenderer::computeVelocityMagnitude(vecField);
    ASSERT_TRUE(result.has_value());

    auto& mag = *result;
    EXPECT_EQ(mag->GetNumberOfScalarComponents(), 1);

    int* dims = mag->GetDimensions();
    EXPECT_EQ(dims[0], 8);
    EXPECT_EQ(dims[1], 8);
    EXPECT_EQ(dims[2], 8);

    // Check all voxels have magnitude 5.0
    auto* ptr = static_cast<float*>(mag->GetScalarPointer());
    for (int i = 0; i < 8 * 8 * 8; ++i) {
        EXPECT_NEAR(ptr[i], 5.0f, 1e-5f);
    }
}

TEST(HemodynamicOverlayRendererTest, ComputeVelocityMagnitudeGradient) {
    // Gradient field: V(x,y,z) = (x, y, z) at each voxel
    auto vecField = createGradientVectorField(4, 4, 4);
    auto result = HemodynamicOverlayRenderer::computeVelocityMagnitude(vecField);
    ASSERT_TRUE(result.has_value());

    auto& mag = *result;
    auto* ptr = static_cast<float*>(mag->GetScalarPointer());

    // Check voxel at (0,0,0): |V| = 0
    EXPECT_NEAR(ptr[0], 0.0f, 1e-5f);

    // Check voxel at (3,3,3): |V| = sqrt(9+9+9) = sqrt(27) ≈ 5.196
    int idx = 3 * 4 * 4 + 3 * 4 + 3;
    float expected = std::sqrt(9.0f + 9.0f + 9.0f);
    EXPECT_NEAR(ptr[idx], expected, 1e-4f);
}

TEST(HemodynamicOverlayRendererTest, ComputeVelocityMagnitudeNullInput) {
    auto result = HemodynamicOverlayRenderer::computeVelocityMagnitude(nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

TEST(HemodynamicOverlayRendererTest, ComputeVelocityMagnitudeInvalidComponents) {
    // Scalar image with 1 component (not a vector field)
    auto scalar = createScalarField(4, 4, 4);
    auto result = HemodynamicOverlayRenderer::computeVelocityMagnitude(scalar);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

// =============================================================================
// Component Extraction
// =============================================================================

TEST(HemodynamicOverlayRendererTest, ExtractComponentX) {
    auto vecField = createVectorField(4, 4, 4, 10.0, 20.0, 30.0);
    auto result = HemodynamicOverlayRenderer::extractComponent(vecField, 0);
    ASSERT_TRUE(result.has_value());

    auto& comp = *result;
    auto* ptr = static_cast<float*>(comp->GetScalarPointer());
    for (int i = 0; i < 4 * 4 * 4; ++i) {
        EXPECT_NEAR(ptr[i], 10.0f, 1e-5f);
    }
}

TEST(HemodynamicOverlayRendererTest, ExtractComponentY) {
    auto vecField = createVectorField(4, 4, 4, 10.0, 20.0, 30.0);
    auto result = HemodynamicOverlayRenderer::extractComponent(vecField, 1);
    ASSERT_TRUE(result.has_value());

    auto& comp = *result;
    auto* ptr = static_cast<float*>(comp->GetScalarPointer());
    for (int i = 0; i < 4 * 4 * 4; ++i) {
        EXPECT_NEAR(ptr[i], 20.0f, 1e-5f);
    }
}

TEST(HemodynamicOverlayRendererTest, ExtractComponentZ) {
    auto vecField = createVectorField(4, 4, 4, 10.0, 20.0, 30.0);
    auto result = HemodynamicOverlayRenderer::extractComponent(vecField, 2);
    ASSERT_TRUE(result.has_value());

    auto& comp = *result;
    auto* ptr = static_cast<float*>(comp->GetScalarPointer());
    for (int i = 0; i < 4 * 4 * 4; ++i) {
        EXPECT_NEAR(ptr[i], 30.0f, 1e-5f);
    }
}

TEST(HemodynamicOverlayRendererTest, ExtractComponentInvalidIndex) {
    auto vecField = createVectorField(4, 4, 4, 1.0, 2.0, 3.0);
    auto result = HemodynamicOverlayRenderer::extractComponent(vecField, 5);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

TEST(HemodynamicOverlayRendererTest, ExtractComponentNullInput) {
    auto result = HemodynamicOverlayRenderer::extractComponent(nullptr, 0);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Full Pipeline Integration
// =============================================================================

TEST(HemodynamicOverlayRendererTest, FullPipelineEndToEnd) {
    // Create a velocity vector field
    auto vecField = createVectorField(16, 16, 16, 30.0, 40.0, 0.0);

    // Compute magnitude
    auto magResult = HemodynamicOverlayRenderer::computeVelocityMagnitude(vecField);
    ASSERT_TRUE(magResult.has_value());

    // Set up overlay renderer
    HemodynamicOverlayRenderer renderer;
    renderer.setScalarField(*magResult);
    renderer.setScalarRange(0.0, 100.0);
    renderer.setColormapPreset(ColormapPreset::Jet);
    renderer.setOpacity(0.6);

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    // Set slice positions
    auto r1 = renderer.setSlicePosition(MPRPlane::Axial, 8.0);
    EXPECT_TRUE(r1.has_value());

    auto r2 = renderer.setSlicePosition(MPRPlane::Coronal, 8.0);
    EXPECT_TRUE(r2.has_value());

    auto r3 = renderer.setSlicePosition(MPRPlane::Sagittal, 8.0);
    EXPECT_TRUE(r3.has_value());

    // Update pipeline - should not crash
    renderer.update();

    // Verify overlay actors are in renderers
    EXPECT_GT(axial->GetViewProps()->GetNumberOfItems(), 0);
}

// =============================================================================
// Colormap Preset Validation
// =============================================================================

TEST(HemodynamicOverlayRendererTest, AllColormapsProduceValidLUT) {
    HemodynamicOverlayRenderer renderer;

    for (auto preset : {ColormapPreset::Jet, ColormapPreset::HotMetal,
                        ColormapPreset::CoolWarm, ColormapPreset::Viridis}) {
        renderer.setColormapPreset(preset);
        auto lut = renderer.getLookupTable();
        ASSERT_NE(lut, nullptr);
        EXPECT_EQ(lut->GetNumberOfTableValues(), 256);

        // Verify all table values are in [0, 1] range
        for (int i = 0; i < 256; ++i) {
            double rgba[4];
            lut->GetTableValue(i, rgba);
            EXPECT_GE(rgba[0], 0.0);
            EXPECT_LE(rgba[0], 1.0);
            EXPECT_GE(rgba[1], 0.0);
            EXPECT_LE(rgba[1], 1.0);
            EXPECT_GE(rgba[2], 0.0);
            EXPECT_LE(rgba[2], 1.0);
        }
    }
}

// =============================================================================
// Geometry Preservation
// =============================================================================

TEST(HemodynamicOverlayRendererTest, MagnitudePreservesGeometry) {
    auto vecField = createVectorField(8, 12, 16, 1.0, 0.0, 0.0);
    vecField->SetSpacing(0.5, 0.75, 1.25);
    vecField->SetOrigin(10.0, 20.0, 30.0);

    auto result = HemodynamicOverlayRenderer::computeVelocityMagnitude(vecField);
    ASSERT_TRUE(result.has_value());

    auto& mag = *result;
    int* dims = mag->GetDimensions();
    double* spacing = mag->GetSpacing();
    double* origin = mag->GetOrigin();

    EXPECT_EQ(dims[0], 8);
    EXPECT_EQ(dims[1], 12);
    EXPECT_EQ(dims[2], 16);

    EXPECT_DOUBLE_EQ(spacing[0], 0.5);
    EXPECT_DOUBLE_EQ(spacing[1], 0.75);
    EXPECT_DOUBLE_EQ(spacing[2], 1.25);

    EXPECT_DOUBLE_EQ(origin[0], 10.0);
    EXPECT_DOUBLE_EQ(origin[1], 20.0);
    EXPECT_DOUBLE_EQ(origin[2], 30.0);
}

// =============================================================================
// Default Colormap For Overlay Type
// =============================================================================

TEST(HemodynamicOverlayRendererTest, DefaultColormapForVelocityMagnitude) {
    auto preset = HemodynamicOverlayRenderer::defaultColormapForType(
        OverlayType::VelocityMagnitude);
    EXPECT_EQ(preset, ColormapPreset::Jet);
}

TEST(HemodynamicOverlayRendererTest, DefaultColormapForVelocityComponents) {
    EXPECT_EQ(HemodynamicOverlayRenderer::defaultColormapForType(OverlayType::VelocityX),
              ColormapPreset::CoolWarm);
    EXPECT_EQ(HemodynamicOverlayRenderer::defaultColormapForType(OverlayType::VelocityY),
              ColormapPreset::CoolWarm);
    EXPECT_EQ(HemodynamicOverlayRenderer::defaultColormapForType(OverlayType::VelocityZ),
              ColormapPreset::CoolWarm);
}

TEST(HemodynamicOverlayRendererTest, DefaultColormapForVorticity) {
    auto preset = HemodynamicOverlayRenderer::defaultColormapForType(
        OverlayType::Vorticity);
    EXPECT_EQ(preset, ColormapPreset::CoolWarm);
}

TEST(HemodynamicOverlayRendererTest, DefaultColormapForEnergyLoss) {
    auto preset = HemodynamicOverlayRenderer::defaultColormapForType(
        OverlayType::EnergyLoss);
    EXPECT_EQ(preset, ColormapPreset::HotMetal);
}

// =============================================================================
// Overlay Type Auto-Applies Colormap
// =============================================================================

TEST(HemodynamicOverlayRendererTest, SetOverlayTypeAppliesDefaultColormap) {
    HemodynamicOverlayRenderer renderer;

    renderer.setOverlayType(OverlayType::Vorticity);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::CoolWarm);

    renderer.setOverlayType(OverlayType::EnergyLoss);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::HotMetal);

    renderer.setOverlayType(OverlayType::VelocityMagnitude);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::Jet);
}

// =============================================================================
// Vorticity Overlay Pipeline
// =============================================================================

TEST(HemodynamicOverlayRendererTest, VorticityOverlayEndToEnd) {
    // Simulate a vorticity magnitude scalar field
    auto vorticityField = createScalarField(16, 16, 16);

    HemodynamicOverlayRenderer renderer;
    renderer.setOverlayType(OverlayType::Vorticity);
    renderer.setScalarField(vorticityField);
    renderer.setScalarRange(0.0, 50.0);

    EXPECT_EQ(renderer.overlayType(), OverlayType::Vorticity);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::CoolWarm);

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    auto r = renderer.setSlicePosition(MPRPlane::Axial, 8.0);
    EXPECT_TRUE(r.has_value());

    // Pipeline should execute without crash
    renderer.update();

    EXPECT_GT(axial->GetViewProps()->GetNumberOfItems(), 0);
}

// =============================================================================
// Energy Loss Overlay Pipeline
// =============================================================================

TEST(HemodynamicOverlayRendererTest, EnergyLossOverlayEndToEnd) {
    // Simulate a dissipation rate scalar field
    auto dissipationField = createScalarField(16, 16, 16);

    HemodynamicOverlayRenderer renderer;
    renderer.setOverlayType(OverlayType::EnergyLoss);
    renderer.setScalarField(dissipationField);
    renderer.setScalarRange(0.0, 1000.0);

    EXPECT_EQ(renderer.overlayType(), OverlayType::EnergyLoss);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::HotMetal);

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    auto r1 = renderer.setSlicePosition(MPRPlane::Axial, 8.0);
    EXPECT_TRUE(r1.has_value());
    auto r2 = renderer.setSlicePosition(MPRPlane::Coronal, 8.0);
    EXPECT_TRUE(r2.has_value());
    auto r3 = renderer.setSlicePosition(MPRPlane::Sagittal, 8.0);
    EXPECT_TRUE(r3.has_value());

    renderer.update();

    EXPECT_GT(axial->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(coronal->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(sagittal->GetViewProps()->GetNumberOfItems(), 0);
}

// =============================================================================
// Vorticity/EnergyLoss Opacity and Visibility
// =============================================================================

TEST(HemodynamicOverlayRendererTest, VorticityOpacityControl) {
    HemodynamicOverlayRenderer renderer;
    renderer.setOverlayType(OverlayType::Vorticity);
    renderer.setOpacity(0.3);

    EXPECT_DOUBLE_EQ(renderer.opacity(), 0.3);
}

TEST(HemodynamicOverlayRendererTest, EnergyLossVisibilityToggle) {
    HemodynamicOverlayRenderer renderer;
    renderer.setOverlayType(OverlayType::EnergyLoss);

    renderer.setVisible(false);
    EXPECT_FALSE(renderer.isVisible());

    renderer.setVisible(true);
    EXPECT_TRUE(renderer.isVisible());
}

// =============================================================================
// Colormap Override After setOverlayType
// =============================================================================

TEST(HemodynamicOverlayRendererTest, ColormapOverrideAfterTypeSet) {
    HemodynamicOverlayRenderer renderer;

    // setOverlayType auto-applies CoolWarm for Vorticity
    renderer.setOverlayType(OverlayType::Vorticity);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::CoolWarm);

    // User can override the colormap after setting type
    renderer.setColormapPreset(ColormapPreset::Viridis);
    EXPECT_EQ(renderer.colormapPreset(), ColormapPreset::Viridis);

    // Verify LUT reflects the override
    auto lut = renderer.getLookupTable();
    ASSERT_NE(lut, nullptr);
    EXPECT_EQ(lut->GetNumberOfTableValues(), 256);
}

// =============================================================================
// Mask Overlay Type
// =============================================================================

TEST(HemodynamicOverlayRendererTest, MaskOverlayType) {
    HemodynamicOverlayRenderer renderer;
    renderer.setOverlayType(OverlayType::Mask);
    EXPECT_EQ(renderer.overlayType(), OverlayType::Mask);
}

TEST(HemodynamicOverlayRendererTest, DefaultColormapForMask) {
    auto preset = HemodynamicOverlayRenderer::defaultColormapForType(
        OverlayType::Mask);
    EXPECT_EQ(preset, ColormapPreset::Jet);
}

// =============================================================================
// Performance Timing
// =============================================================================

TEST(HemodynamicOverlayRendererTest, LastRenderTimeMs_InitiallyZero) {
    HemodynamicOverlayRenderer renderer;
    EXPECT_DOUBLE_EQ(renderer.lastRenderTimeMs(), 0.0);
}

TEST(HemodynamicOverlayRendererTest, LastRenderTimeMs_MeasuredAfterUpdate) {
    HemodynamicOverlayRenderer renderer;
    auto field = createScalarField(32, 32, 32);
    renderer.setScalarField(field);

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    renderer.setSlicePosition(MPRPlane::Axial, 16.0);
    renderer.setSlicePosition(MPRPlane::Coronal, 16.0);
    renderer.setSlicePosition(MPRPlane::Sagittal, 16.0);

    renderer.update();

    double ms = renderer.lastRenderTimeMs();
    EXPECT_GT(ms, 0.0);
    // Performance requirement: overlay rendering < 50ms per frame
    EXPECT_LT(ms, 50.0);
}

TEST(HemodynamicOverlayRendererTest, PerformanceLargeField) {
    // Test with a larger field (64^3) to verify performance under load
    HemodynamicOverlayRenderer renderer;
    auto field = createScalarField(64, 64, 64);
    renderer.setScalarField(field);

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    renderer.setSlicePosition(MPRPlane::Axial, 32.0);
    renderer.setSlicePosition(MPRPlane::Coronal, 32.0);
    renderer.setSlicePosition(MPRPlane::Sagittal, 32.0);

    renderer.update();

    double ms = renderer.lastRenderTimeMs();
    EXPECT_GT(ms, 0.0);
    EXPECT_LT(ms, 50.0);
}

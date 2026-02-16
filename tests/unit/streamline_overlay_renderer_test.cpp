#include <gtest/gtest.h>

#include <cmath>

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

#include "services/render/streamline_overlay_renderer.hpp"
#include "services/render/hemodynamic_overlay_renderer.hpp"
#include "services/mpr_renderer.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a 3D uniform velocity field (Vx, Vy, Vz) at each voxel
vtkSmartPointer<vtkImageData> createUniformVelocityField(
    int dimX, int dimY, int dimZ,
    double vx, double vy, double vz,
    double spacing = 1.0) {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(dimX, dimY, dimZ);
    image->SetSpacing(spacing, spacing, spacing);
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

/// Create a 3D velocity field with a circular vortex pattern in the XY plane
vtkSmartPointer<vtkImageData> createVortexVelocityField(
    int dim, double spacing = 1.0) {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(dim, dim, dim);
    image->SetSpacing(spacing, spacing, spacing);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 3);

    auto* ptr = static_cast<float*>(image->GetScalarPointer());
    double center = (dim - 1) * spacing / 2.0;

    int idx = 0;
    for (int z = 0; z < dim; ++z) {
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                double dx = x * spacing - center;
                double dy = y * spacing - center;
                double r = std::sqrt(dx * dx + dy * dy);
                double speed = (r > 0.1) ? 10.0 / (1.0 + r) : 0.0;
                // Tangential velocity: (-dy, dx) / r
                ptr[idx * 3 + 0] = static_cast<float>((r > 0.1) ? -dy / r * speed : 0.0);
                ptr[idx * 3 + 1] = static_cast<float>((r > 0.1) ? dx / r * speed : 0.0);
                ptr[idx * 3 + 2] = 0.0f;
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

TEST(StreamlineOverlayRendererTest, DefaultState) {
    StreamlineOverlayRenderer renderer;
    EXPECT_FALSE(renderer.hasVelocityField());
    EXPECT_TRUE(renderer.isVisible());
    EXPECT_DOUBLE_EQ(renderer.opacity(), 0.6);
    EXPECT_EQ(renderer.mode(), StreamlineOverlayRenderer::Mode::Streamline);
}

TEST(StreamlineOverlayRendererTest, MoveConstructor) {
    StreamlineOverlayRenderer r1;
    r1.setOpacity(0.8);
    r1.setMode(StreamlineOverlayRenderer::Mode::LIC);

    StreamlineOverlayRenderer r2(std::move(r1));
    EXPECT_DOUBLE_EQ(r2.opacity(), 0.8);
    EXPECT_EQ(r2.mode(), StreamlineOverlayRenderer::Mode::LIC);
}

// =============================================================================
// Velocity Field Input
// =============================================================================

TEST(StreamlineOverlayRendererTest, SetVelocityField) {
    StreamlineOverlayRenderer renderer;
    auto field = createUniformVelocityField(8, 8, 8, 1.0, 0.0, 0.0);

    renderer.setVelocityField(field);
    EXPECT_TRUE(renderer.hasVelocityField());

    renderer.setVelocityField(nullptr);
    EXPECT_FALSE(renderer.hasVelocityField());
}

// =============================================================================
// Settings
// =============================================================================

TEST(StreamlineOverlayRendererTest, ModeSwitch) {
    StreamlineOverlayRenderer renderer;
    renderer.setMode(StreamlineOverlayRenderer::Mode::LIC);
    EXPECT_EQ(renderer.mode(), StreamlineOverlayRenderer::Mode::LIC);

    renderer.setMode(StreamlineOverlayRenderer::Mode::Streamline);
    EXPECT_EQ(renderer.mode(), StreamlineOverlayRenderer::Mode::Streamline);
}

TEST(StreamlineOverlayRendererTest, OpacityClamping) {
    StreamlineOverlayRenderer renderer;
    renderer.setOpacity(-0.5);
    EXPECT_DOUBLE_EQ(renderer.opacity(), 0.0);

    renderer.setOpacity(1.5);
    EXPECT_DOUBLE_EQ(renderer.opacity(), 1.0);

    renderer.setOpacity(0.7);
    EXPECT_DOUBLE_EQ(renderer.opacity(), 0.7);
}

TEST(StreamlineOverlayRendererTest, VisibilityToggle) {
    StreamlineOverlayRenderer renderer;
    EXPECT_TRUE(renderer.isVisible());

    renderer.setVisible(false);
    EXPECT_FALSE(renderer.isVisible());

    renderer.setVisible(true);
    EXPECT_TRUE(renderer.isVisible());
}

// =============================================================================
// Renderer Attachment
// =============================================================================

TEST(StreamlineOverlayRendererTest, SetRenderers) {
    StreamlineOverlayRenderer renderer;

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();

    renderer.setRenderers(axial, coronal, sagittal);

    // Actors should be added (2 per plane: streamline + LIC)
    EXPECT_GT(axial->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(coronal->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(sagittal->GetViewProps()->GetNumberOfItems(), 0);
}

// =============================================================================
// Slice Position
// =============================================================================

TEST(StreamlineOverlayRendererTest, SetSlicePositionWithoutField) {
    StreamlineOverlayRenderer renderer;
    auto result = renderer.setSlicePosition(MPRPlane::Axial, 5.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

TEST(StreamlineOverlayRendererTest, SetSlicePositionSuccess) {
    StreamlineOverlayRenderer renderer;
    renderer.setVelocityField(createUniformVelocityField(16, 16, 16, 1.0, 0.0, 0.0));

    auto r = renderer.setSlicePosition(MPRPlane::Axial, 8.0);
    EXPECT_TRUE(r.has_value());

    r = renderer.setSlicePosition(MPRPlane::Coronal, 8.0);
    EXPECT_TRUE(r.has_value());

    r = renderer.setSlicePosition(MPRPlane::Sagittal, 8.0);
    EXPECT_TRUE(r.has_value());
}

// =============================================================================
// Extract Slice Velocity
// =============================================================================

TEST(StreamlineOverlayRendererTest, ExtractAxialSliceVelocity) {
    auto field = createUniformVelocityField(8, 8, 8, 5.0, 10.0, 15.0);
    auto result = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Axial, 4.0);
    ASSERT_TRUE(result.has_value());

    auto& slice = *result;
    int* dims = slice->GetDimensions();
    EXPECT_EQ(dims[0], 8);  // X preserved
    EXPECT_EQ(dims[1], 8);  // Y preserved
    EXPECT_EQ(dims[2], 1);  // 2D

    // Axial extracts (Vx, Vy) = (5.0, 10.0)
    auto* ptr = static_cast<float*>(slice->GetScalarPointer());
    EXPECT_NEAR(ptr[0], 5.0f, 1e-5f);   // Vx
    EXPECT_NEAR(ptr[1], 10.0f, 1e-5f);  // Vy
    EXPECT_NEAR(ptr[2], 0.0f, 1e-5f);   // Z = 0

    // Should have vectors set for vtkStreamTracer
    EXPECT_NE(slice->GetPointData()->GetVectors(), nullptr);
}

TEST(StreamlineOverlayRendererTest, ExtractCoronalSliceVelocity) {
    auto field = createUniformVelocityField(8, 8, 8, 5.0, 10.0, 15.0);
    auto result = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Coronal, 4.0);
    ASSERT_TRUE(result.has_value());

    auto& slice = *result;
    int* dims = slice->GetDimensions();
    EXPECT_EQ(dims[0], 8);  // X preserved
    EXPECT_EQ(dims[1], 8);  // Z mapped to Y dimension

    // Coronal extracts (Vx, Vz) = (5.0, 15.0)
    auto* ptr = static_cast<float*>(slice->GetScalarPointer());
    EXPECT_NEAR(ptr[0], 5.0f, 1e-5f);   // Vx
    EXPECT_NEAR(ptr[1], 15.0f, 1e-5f);  // Vz
}

TEST(StreamlineOverlayRendererTest, ExtractSagittalSliceVelocity) {
    auto field = createUniformVelocityField(8, 8, 8, 5.0, 10.0, 15.0);
    auto result = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Sagittal, 4.0);
    ASSERT_TRUE(result.has_value());

    auto& slice = *result;
    int* dims = slice->GetDimensions();
    EXPECT_EQ(dims[0], 8);  // Y mapped to X
    EXPECT_EQ(dims[1], 8);  // Z mapped to Y

    // Sagittal extracts (Vy, Vz) = (10.0, 15.0)
    auto* ptr = static_cast<float*>(slice->GetScalarPointer());
    EXPECT_NEAR(ptr[0], 10.0f, 1e-5f);  // Vy
    EXPECT_NEAR(ptr[1], 15.0f, 1e-5f);  // Vz
}

TEST(StreamlineOverlayRendererTest, ExtractSliceNullInput) {
    auto result = StreamlineOverlayRenderer::extractSliceVelocity(
        nullptr, MPRPlane::Axial, 0.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

TEST(StreamlineOverlayRendererTest, ExtractSliceScalarInput) {
    // Scalar field (1 component) should fail
    auto scalar = vtkSmartPointer<vtkImageData>::New();
    scalar->SetDimensions(4, 4, 4);
    scalar->AllocateScalars(VTK_FLOAT, 1);

    auto result = StreamlineOverlayRenderer::extractSliceVelocity(
        scalar, MPRPlane::Axial, 0.0);
    EXPECT_FALSE(result.has_value());
}

TEST(StreamlineOverlayRendererTest, ExtractSliceVelocityMagnitudeArray) {
    auto field = createUniformVelocityField(8, 8, 8, 3.0, 4.0, 0.0);
    auto result = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Axial, 4.0);
    ASSERT_TRUE(result.has_value());

    // Should have VelocityMagnitude array
    auto* magArray = (*result)->GetPointData()->GetArray("VelocityMagnitude");
    ASSERT_NE(magArray, nullptr);

    // |V| = sqrt(3^2 + 4^2) = 5.0 for axial (Vx, Vy)
    EXPECT_NEAR(magArray->GetComponent(0, 0), 5.0, 1e-4);
}

// =============================================================================
// 2D Streamline Generation
// =============================================================================

TEST(StreamlineOverlayRendererTest, GenerateStreamlinesFromUniformField) {
    auto field = createUniformVelocityField(16, 16, 16, 10.0, 0.0, 0.0);
    auto sliceResult = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Axial, 8.0);
    ASSERT_TRUE(sliceResult.has_value());

    Streamline2DParams params;
    params.numSeedPoints = 25;
    params.maxSteps = 100;

    auto result = StreamlineOverlayRenderer::generateStreamlines2D(
        *sliceResult, params);
    ASSERT_TRUE(result.has_value());

    auto& polyData = *result;
    // Should produce some streamline geometry
    EXPECT_GT(polyData->GetNumberOfPoints(), 0);
    EXPECT_GT(polyData->GetNumberOfCells(), 0);

    // Should have velocity magnitude scalars for color coding
    EXPECT_NE(polyData->GetPointData()->GetScalars(), nullptr);
}

TEST(StreamlineOverlayRendererTest, GenerateStreamlinesFromVortexField) {
    auto field = createVortexVelocityField(16);
    auto sliceResult = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Axial, 8.0);
    ASSERT_TRUE(sliceResult.has_value());

    Streamline2DParams params;
    params.numSeedPoints = 36;
    params.maxSteps = 200;

    auto result = StreamlineOverlayRenderer::generateStreamlines2D(
        *sliceResult, params);
    ASSERT_TRUE(result.has_value());

    EXPECT_GT((*result)->GetNumberOfPoints(), 0);
}

TEST(StreamlineOverlayRendererTest, GenerateStreamlinesNullInput) {
    auto result = StreamlineOverlayRenderer::generateStreamlines2D(nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

TEST(StreamlineOverlayRendererTest, GenerateStreamlinesNoVectors) {
    // Create an image without vectors set
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(8, 8, 1);
    image->AllocateScalars(VTK_FLOAT, 3);

    // No SetVectors() call
    auto result = StreamlineOverlayRenderer::generateStreamlines2D(image);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// LIC Texture Computation
// =============================================================================

TEST(StreamlineOverlayRendererTest, ComputeLICFromUniformField) {
    auto field = createUniformVelocityField(16, 16, 16, 10.0, 0.0, 0.0);
    auto sliceResult = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Axial, 8.0);
    ASSERT_TRUE(sliceResult.has_value());

    LICParams params;
    params.kernelLength = 10;

    auto result = StreamlineOverlayRenderer::computeLIC(*sliceResult, params);
    ASSERT_TRUE(result.has_value());

    auto& licTexture = *result;
    int* dims = licTexture->GetDimensions();
    EXPECT_EQ(dims[0], 16);
    EXPECT_EQ(dims[1], 16);
    EXPECT_EQ(dims[2], 1);

    // Should be unsigned char (grayscale)
    EXPECT_EQ(licTexture->GetScalarType(), VTK_UNSIGNED_CHAR);
    EXPECT_EQ(licTexture->GetNumberOfScalarComponents(), 1);

    // LIC values should be in [0, 255]
    auto* ptr = static_cast<unsigned char*>(licTexture->GetScalarPointer());
    bool hasNonZero = false;
    for (int i = 0; i < 16 * 16; ++i) {
        EXPECT_GE(ptr[i], 0);
        EXPECT_LE(ptr[i], 255);
        if (ptr[i] > 0) hasNonZero = true;
    }
    EXPECT_TRUE(hasNonZero);
}

TEST(StreamlineOverlayRendererTest, ComputeLICFromVortexField) {
    auto field = createVortexVelocityField(16);
    auto sliceResult = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Axial, 8.0);
    ASSERT_TRUE(sliceResult.has_value());

    auto result = StreamlineOverlayRenderer::computeLIC(*sliceResult);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ((*result)->GetDimensions()[0], 16);
    EXPECT_EQ((*result)->GetDimensions()[1], 16);
}

TEST(StreamlineOverlayRendererTest, ComputeLICNullInput) {
    auto result = StreamlineOverlayRenderer::computeLIC(nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OverlayError::NoScalarField);
}

TEST(StreamlineOverlayRendererTest, ComputeLICReproducible) {
    auto field = createVortexVelocityField(8);
    auto sliceResult = StreamlineOverlayRenderer::extractSliceVelocity(
        field, MPRPlane::Axial, 4.0);
    ASSERT_TRUE(sliceResult.has_value());

    LICParams params;
    params.noiseSeed = 123;

    auto r1 = StreamlineOverlayRenderer::computeLIC(*sliceResult, params);
    auto r2 = StreamlineOverlayRenderer::computeLIC(*sliceResult, params);
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());

    auto* p1 = static_cast<unsigned char*>((*r1)->GetScalarPointer());
    auto* p2 = static_cast<unsigned char*>((*r2)->GetScalarPointer());

    // Same seed should produce identical results
    for (int i = 0; i < 8 * 8; ++i) {
        EXPECT_EQ(p1[i], p2[i]);
    }
}

// =============================================================================
// OverlayType Enum Integration
// =============================================================================

TEST(StreamlineOverlayRendererTest, OverlayTypeEnumValues) {
    // Verify the new enum values exist and are distinct
    EXPECT_NE(static_cast<int>(OverlayType::Streamline),
              static_cast<int>(OverlayType::VelocityMagnitude));
    EXPECT_NE(static_cast<int>(OverlayType::VelocityTexture),
              static_cast<int>(OverlayType::Streamline));
}

TEST(StreamlineOverlayRendererTest, DefaultColormapForStreamline) {
    auto preset = HemodynamicOverlayRenderer::defaultColormapForType(
        OverlayType::Streamline);
    EXPECT_EQ(preset, ColormapPreset::Jet);
}

TEST(StreamlineOverlayRendererTest, DefaultColormapForVelocityTexture) {
    auto preset = HemodynamicOverlayRenderer::defaultColormapForType(
        OverlayType::VelocityTexture);
    EXPECT_EQ(preset, ColormapPreset::Viridis);
}

// =============================================================================
// Full Pipeline Integration
// =============================================================================

TEST(StreamlineOverlayRendererTest, FullStreamlinePipeline) {
    StreamlineOverlayRenderer renderer;
    auto field = createVortexVelocityField(16);

    renderer.setVelocityField(field);
    renderer.setMode(StreamlineOverlayRenderer::Mode::Streamline);

    Streamline2DParams params;
    params.numSeedPoints = 16;
    renderer.setStreamlineParams(params);

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    auto r = renderer.setSlicePosition(MPRPlane::Axial, 8.0);
    EXPECT_TRUE(r.has_value());

    // Should not crash
    renderer.updatePlane(MPRPlane::Axial);
}

TEST(StreamlineOverlayRendererTest, FullLICPipeline) {
    StreamlineOverlayRenderer renderer;
    auto field = createVortexVelocityField(16);

    renderer.setVelocityField(field);
    renderer.setMode(StreamlineOverlayRenderer::Mode::LIC);

    LICParams params;
    params.kernelLength = 5;
    renderer.setLICParams(params);

    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    auto r = renderer.setSlicePosition(MPRPlane::Axial, 8.0);
    EXPECT_TRUE(r.has_value());

    // Should not crash
    renderer.updatePlane(MPRPlane::Axial);
}

TEST(StreamlineOverlayRendererTest, UpdateAllPlanes) {
    StreamlineOverlayRenderer renderer;
    auto field = createUniformVelocityField(8, 8, 8, 5.0, 5.0, 0.0);

    renderer.setVelocityField(field);
    auto axial = vtkSmartPointer<vtkRenderer>::New();
    auto coronal = vtkSmartPointer<vtkRenderer>::New();
    auto sagittal = vtkSmartPointer<vtkRenderer>::New();
    renderer.setRenderers(axial, coronal, sagittal);

    renderer.setSlicePosition(MPRPlane::Axial, 4.0);
    renderer.setSlicePosition(MPRPlane::Coronal, 4.0);
    renderer.setSlicePosition(MPRPlane::Sagittal, 4.0);

    // Should not crash
    renderer.update();
}

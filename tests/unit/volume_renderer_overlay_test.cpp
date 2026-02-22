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

#include <gtest/gtest.h>

#include <cmath>

#include <vtkColorTransferFunction.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include "services/volume_renderer.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a scalar volume for testing
vtkSmartPointer<vtkImageData> createTestVolume(int dim, float maxVal = 100.0f) {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(dim, dim, dim);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);

    auto* ptr = static_cast<float*>(image->GetScalarPointer());
    int total = dim * dim * dim;
    for (int i = 0; i < total; ++i) {
        ptr[i] = (static_cast<float>(i) / static_cast<float>(total)) * maxVal;
    }
    return image;
}

/// Create a simple color transfer function
vtkSmartPointer<vtkColorTransferFunction> createColorTF(double maxVal) {
    auto tf = vtkSmartPointer<vtkColorTransferFunction>::New();
    tf->AddRGBPoint(0.0, 0.0, 0.0, 1.0);    // Blue at min
    tf->AddRGBPoint(maxVal, 1.0, 0.0, 0.0);  // Red at max
    return tf;
}

/// Create a simple opacity transfer function
vtkSmartPointer<vtkPiecewiseFunction> createOpacityTF(double maxVal) {
    auto tf = vtkSmartPointer<vtkPiecewiseFunction>::New();
    tf->AddPoint(0.0, 0.0);
    tf->AddPoint(maxVal, 0.5);
    return tf;
}

} // anonymous namespace

// =============================================================================
// Overlay Management
// =============================================================================

TEST(VolumeRendererOverlayTest, NoOverlaysByDefault) {
    VolumeRenderer renderer;
    EXPECT_TRUE(renderer.overlayNames().empty());
    EXPECT_FALSE(renderer.hasOverlay("test"));
}

TEST(VolumeRendererOverlayTest, AddOverlay) {
    VolumeRenderer renderer;
    auto vol = createTestVolume(8);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol, ctf, otf);

    EXPECT_TRUE(renderer.hasOverlay("velocity"));
    EXPECT_FALSE(renderer.hasOverlay("vorticity"));
    EXPECT_EQ(renderer.overlayNames().size(), 1);
    EXPECT_EQ(renderer.overlayNames()[0], "velocity");
}

TEST(VolumeRendererOverlayTest, AddMultipleOverlays) {
    VolumeRenderer renderer;
    auto vol1 = createTestVolume(8, 100.0f);
    auto vol2 = createTestVolume(8, 50.0f);
    auto ctf1 = createColorTF(100.0);
    auto ctf2 = createColorTF(50.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol1, ctf1, otf);
    renderer.addScalarOverlay("vorticity", vol2, ctf2, otf);

    EXPECT_EQ(renderer.overlayNames().size(), 2);
    EXPECT_TRUE(renderer.hasOverlay("velocity"));
    EXPECT_TRUE(renderer.hasOverlay("vorticity"));
}

TEST(VolumeRendererOverlayTest, AddDuplicateNameReplaces) {
    VolumeRenderer renderer;
    auto vol1 = createTestVolume(8, 100.0f);
    auto vol2 = createTestVolume(8, 200.0f);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol1, ctf, otf);
    renderer.addScalarOverlay("velocity", vol2, ctf, otf);

    EXPECT_EQ(renderer.overlayNames().size(), 1);
    EXPECT_TRUE(renderer.hasOverlay("velocity"));
}

TEST(VolumeRendererOverlayTest, RemoveOverlay) {
    VolumeRenderer renderer;
    auto vol = createTestVolume(8);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol, ctf, otf);
    EXPECT_TRUE(renderer.hasOverlay("velocity"));

    bool removed = renderer.removeScalarOverlay("velocity");
    EXPECT_TRUE(removed);
    EXPECT_FALSE(renderer.hasOverlay("velocity"));
    EXPECT_TRUE(renderer.overlayNames().empty());
}

TEST(VolumeRendererOverlayTest, RemoveNonexistentOverlay) {
    VolumeRenderer renderer;
    bool removed = renderer.removeScalarOverlay("nonexistent");
    EXPECT_FALSE(removed);
}

TEST(VolumeRendererOverlayTest, RemoveAllOverlays) {
    VolumeRenderer renderer;
    auto vol = createTestVolume(8);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("a", vol, ctf, otf);
    renderer.addScalarOverlay("b", vol, ctf, otf);
    renderer.addScalarOverlay("c", vol, ctf, otf);
    EXPECT_EQ(renderer.overlayNames().size(), 3);

    renderer.removeAllScalarOverlays();
    EXPECT_TRUE(renderer.overlayNames().empty());
}

// =============================================================================
// Overlay Volume Actor
// =============================================================================

TEST(VolumeRendererOverlayTest, GetOverlayVolume) {
    VolumeRenderer renderer;
    auto vol = createTestVolume(8);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol, ctf, otf);

    auto overlayVol = renderer.getOverlayVolume("velocity");
    ASSERT_NE(overlayVol, nullptr);

    // The overlay volume should have its own mapper set
    EXPECT_NE(overlayVol->GetMapper(), nullptr);
    EXPECT_NE(overlayVol->GetProperty(), nullptr);
}

TEST(VolumeRendererOverlayTest, GetOverlayVolumeNotFound) {
    VolumeRenderer renderer;
    auto vol = renderer.getOverlayVolume("nonexistent");
    EXPECT_EQ(vol, nullptr);
}

TEST(VolumeRendererOverlayTest, OverlayVolumeIndependentFromMain) {
    VolumeRenderer renderer;
    auto mainVol = createTestVolume(16, 1000.0f);
    auto overlayVol = createTestVolume(8, 50.0f);
    auto ctf = createColorTF(50.0);
    auto otf = createOpacityTF(50.0);

    renderer.setInputData(mainVol);
    renderer.addScalarOverlay("velocity", overlayVol, ctf, otf);

    auto mainActor = renderer.getVolume();
    auto overlayActor = renderer.getOverlayVolume("velocity");

    ASSERT_NE(mainActor, nullptr);
    ASSERT_NE(overlayActor, nullptr);
    EXPECT_NE(mainActor.Get(), overlayActor.Get());
}

// =============================================================================
// Overlay Visibility
// =============================================================================

TEST(VolumeRendererOverlayTest, OverlayVisibilityToggle) {
    VolumeRenderer renderer;
    auto vol = createTestVolume(8);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol, ctf, otf);

    auto overlayVol = renderer.getOverlayVolume("velocity");
    ASSERT_NE(overlayVol, nullptr);

    renderer.setOverlayVisible("velocity", false);
    EXPECT_EQ(overlayVol->GetVisibility(), 0);

    renderer.setOverlayVisible("velocity", true);
    EXPECT_EQ(overlayVol->GetVisibility(), 1);
}

// =============================================================================
// Transfer Function Update
// =============================================================================

TEST(VolumeRendererOverlayTest, UpdateTransferFunctions) {
    VolumeRenderer renderer;
    auto vol = createTestVolume(8);
    auto ctf1 = createColorTF(100.0);
    auto otf1 = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol, ctf1, otf1);

    // Update with new transfer functions
    auto ctf2 = createColorTF(200.0);
    auto otf2 = createOpacityTF(200.0);
    bool updated = renderer.updateOverlayTransferFunctions("velocity", ctf2, otf2);
    EXPECT_TRUE(updated);
}

TEST(VolumeRendererOverlayTest, UpdateTransferFunctionsNotFound) {
    VolumeRenderer renderer;
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);
    bool updated = renderer.updateOverlayTransferFunctions("nonexistent", ctf, otf);
    EXPECT_FALSE(updated);
}

// =============================================================================
// Velocity Convenience Functions
// =============================================================================

TEST(VolumeRendererOverlayTest, CreateVelocityColorFunction) {
    auto ctf = VolumeRenderer::createVelocityColorFunction(150.0);
    ASSERT_NE(ctf, nullptr);

    // Check that key points exist
    EXPECT_GE(ctf->GetSize(), 5);

    // Verify color at different velocities
    double rgb[3];
    ctf->GetColor(0.0, rgb);
    // At zero velocity, should be blue-ish
    EXPECT_LT(rgb[0], 0.5);  // Low red
    EXPECT_GT(rgb[2], 0.0);  // Some blue

    ctf->GetColor(150.0, rgb);
    // At max velocity, should be red
    EXPECT_DOUBLE_EQ(rgb[0], 1.0);
    EXPECT_DOUBLE_EQ(rgb[1], 0.0);
    EXPECT_DOUBLE_EQ(rgb[2], 0.0);
}

TEST(VolumeRendererOverlayTest, CreateVelocityOpacityFunction) {
    auto otf = VolumeRenderer::createVelocityOpacityFunction(100.0, 0.5);
    ASSERT_NE(otf, nullptr);

    // Below 10% of max → should be transparent
    EXPECT_DOUBLE_EQ(otf->GetValue(0.0), 0.0);

    // At max velocity → should be at base opacity
    EXPECT_NEAR(otf->GetValue(100.0), 0.5, 0.01);
}

TEST(VolumeRendererOverlayTest, VelocityOverlayEndToEnd) {
    VolumeRenderer renderer;

    // Create velocity magnitude field
    auto velocityMag = createTestVolume(16, 150.0f);

    // Create velocity transfer functions
    auto ctf = VolumeRenderer::createVelocityColorFunction(150.0);
    auto otf = VolumeRenderer::createVelocityOpacityFunction(150.0, 0.4);

    // Add as overlay
    renderer.addScalarOverlay("velocity_magnitude", velocityMag, ctf, otf);

    EXPECT_TRUE(renderer.hasOverlay("velocity_magnitude"));
    auto vol = renderer.getOverlayVolume("velocity_magnitude");
    ASSERT_NE(vol, nullptr);
    EXPECT_NE(vol->GetMapper(), nullptr);
    EXPECT_NE(vol->GetProperty(), nullptr);

    // Verify the overlay uses the correct transfer functions
    auto* prop = vol->GetProperty();
    EXPECT_NE(prop->GetRGBTransferFunction(), nullptr);
    EXPECT_NE(prop->GetScalarOpacity(), nullptr);
}

// =============================================================================
// Main Volume Unaffected
// =============================================================================

TEST(VolumeRendererOverlayTest, MainVolumeUnaffectedByOverlays) {
    VolumeRenderer renderer;
    auto mainVol = createTestVolume(16, 1000.0f);
    renderer.setInputData(mainVol);

    auto mainActor = renderer.getVolume();
    auto mainMapper = mainActor->GetMapper();
    ASSERT_NE(mainMapper, nullptr);

    // Add overlays
    auto overlayVol = createTestVolume(8, 100.0f);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);
    renderer.addScalarOverlay("overlay1", overlayVol, ctf, otf);

    // Main volume should still have the same mapper
    EXPECT_EQ(mainActor->GetMapper(), mainMapper);

    // Remove overlay - main volume unaffected
    renderer.removeScalarOverlay("overlay1");
    EXPECT_EQ(mainActor->GetMapper(), mainMapper);
}

// =============================================================================
// Overlay Property Configuration
// =============================================================================

TEST(VolumeRendererOverlayTest, OverlayPropertyNoShading) {
    VolumeRenderer renderer;
    auto vol = createTestVolume(8);
    auto ctf = createColorTF(100.0);
    auto otf = createOpacityTF(100.0);

    renderer.addScalarOverlay("velocity", vol, ctf, otf);

    auto overlayVol = renderer.getOverlayVolume("velocity");
    ASSERT_NE(overlayVol, nullptr);

    // Overlay should have shading off (scalar overlays don't need lighting)
    auto* prop = overlayVol->GetProperty();
    EXPECT_EQ(prop->GetShade(), 0);
}

// =============================================================================
// Vorticity Convenience Functions
// =============================================================================

TEST(VolumeRendererOverlayTest, CreateVorticityColorFunction) {
    auto ctf = VolumeRenderer::createVorticityColorFunction(50.0);
    ASSERT_NE(ctf, nullptr);
    EXPECT_GE(ctf->GetSize(), 5);

    // At zero vorticity → dark blue
    double rgb[3];
    ctf->GetColor(0.0, rgb);
    EXPECT_LT(rgb[0], 0.5);
    EXPECT_GT(rgb[2], 0.0);

    // At mid vorticity → near white
    ctf->GetColor(25.0, rgb);
    EXPECT_GT(rgb[0], 0.5);
    EXPECT_GT(rgb[1], 0.5);
    EXPECT_GT(rgb[2], 0.5);

    // At max vorticity → dark red
    ctf->GetColor(50.0, rgb);
    EXPECT_GT(rgb[0], 0.0);
    EXPECT_LT(rgb[1], 0.5);
    EXPECT_LT(rgb[2], 0.5);
}

TEST(VolumeRendererOverlayTest, CreateVorticityOpacityFunction) {
    auto otf = VolumeRenderer::createVorticityOpacityFunction(50.0, 0.5);
    ASSERT_NE(otf, nullptr);

    // Zero vorticity → transparent
    EXPECT_DOUBLE_EQ(otf->GetValue(0.0), 0.0);

    // Below 10% → still transparent
    EXPECT_DOUBLE_EQ(otf->GetValue(50.0 * 0.05), 0.0);

    // At max vorticity → base opacity
    EXPECT_NEAR(otf->GetValue(50.0), 0.5, 0.01);
}

TEST(VolumeRendererOverlayTest, VorticityOverlayEndToEnd) {
    VolumeRenderer renderer;
    auto vorticityField = createTestVolume(16, 50.0f);
    auto ctf = VolumeRenderer::createVorticityColorFunction(50.0);
    auto otf = VolumeRenderer::createVorticityOpacityFunction(50.0, 0.4);

    renderer.addScalarOverlay("vorticity", vorticityField, ctf, otf);

    EXPECT_TRUE(renderer.hasOverlay("vorticity"));
    auto vol = renderer.getOverlayVolume("vorticity");
    ASSERT_NE(vol, nullptr);
    EXPECT_NE(vol->GetMapper(), nullptr);
    EXPECT_NE(vol->GetProperty(), nullptr);

    auto* prop = vol->GetProperty();
    EXPECT_NE(prop->GetRGBTransferFunction(), nullptr);
    EXPECT_NE(prop->GetScalarOpacity(), nullptr);
}

// =============================================================================
// Energy Loss Convenience Functions
// =============================================================================

TEST(VolumeRendererOverlayTest, CreateEnergyLossColorFunction) {
    auto ctf = VolumeRenderer::createEnergyLossColorFunction(1000.0);
    ASSERT_NE(ctf, nullptr);
    EXPECT_GE(ctf->GetSize(), 5);

    // At zero → black
    double rgb[3];
    ctf->GetColor(0.0, rgb);
    EXPECT_DOUBLE_EQ(rgb[0], 0.0);
    EXPECT_DOUBLE_EQ(rgb[1], 0.0);
    EXPECT_DOUBLE_EQ(rgb[2], 0.0);

    // At mid → red
    ctf->GetColor(500.0, rgb);
    EXPECT_DOUBLE_EQ(rgb[0], 1.0);
    EXPECT_LT(rgb[1], 0.5);

    // At max → near white (hot)
    ctf->GetColor(1000.0, rgb);
    EXPECT_DOUBLE_EQ(rgb[0], 1.0);
    EXPECT_DOUBLE_EQ(rgb[1], 1.0);
    EXPECT_GT(rgb[2], 0.5);
}

TEST(VolumeRendererOverlayTest, CreateEnergyLossOpacityFunction) {
    auto otf = VolumeRenderer::createEnergyLossOpacityFunction(1000.0, 0.5);
    ASSERT_NE(otf, nullptr);

    // Zero energy loss → transparent
    EXPECT_DOUBLE_EQ(otf->GetValue(0.0), 0.0);

    // Below 5% → still transparent
    EXPECT_DOUBLE_EQ(otf->GetValue(1000.0 * 0.025), 0.0);

    // At max → base opacity
    EXPECT_NEAR(otf->GetValue(1000.0), 0.5, 0.01);
}

TEST(VolumeRendererOverlayTest, EnergyLossOverlayEndToEnd) {
    VolumeRenderer renderer;
    auto energyLossField = createTestVolume(16, 1000.0f);
    auto ctf = VolumeRenderer::createEnergyLossColorFunction(1000.0);
    auto otf = VolumeRenderer::createEnergyLossOpacityFunction(1000.0, 0.3);

    renderer.addScalarOverlay("energy_loss", energyLossField, ctf, otf);

    EXPECT_TRUE(renderer.hasOverlay("energy_loss"));
    auto vol = renderer.getOverlayVolume("energy_loss");
    ASSERT_NE(vol, nullptr);
    EXPECT_NE(vol->GetMapper(), nullptr);
    EXPECT_NE(vol->GetProperty(), nullptr);

    auto* prop = vol->GetProperty();
    EXPECT_NE(prop->GetRGBTransferFunction(), nullptr);
    EXPECT_NE(prop->GetScalarOpacity(), nullptr);
}

// =============================================================================
// Multiple Hemodynamic Overlays Coexistence
// =============================================================================

TEST(VolumeRendererOverlayTest, MultipleHemodynamicOverlays) {
    VolumeRenderer renderer;

    auto velField = createTestVolume(8, 150.0f);
    auto vorField = createTestVolume(8, 50.0f);
    auto elField  = createTestVolume(8, 1000.0f);

    renderer.addScalarOverlay("velocity",
        velField,
        VolumeRenderer::createVelocityColorFunction(150.0),
        VolumeRenderer::createVelocityOpacityFunction(150.0));

    renderer.addScalarOverlay("vorticity",
        vorField,
        VolumeRenderer::createVorticityColorFunction(50.0),
        VolumeRenderer::createVorticityOpacityFunction(50.0));

    renderer.addScalarOverlay("energy_loss",
        elField,
        VolumeRenderer::createEnergyLossColorFunction(1000.0),
        VolumeRenderer::createEnergyLossOpacityFunction(1000.0));

    EXPECT_EQ(renderer.overlayNames().size(), 3);
    EXPECT_TRUE(renderer.hasOverlay("velocity"));
    EXPECT_TRUE(renderer.hasOverlay("vorticity"));
    EXPECT_TRUE(renderer.hasOverlay("energy_loss"));

    // Each overlay has independent volume actor
    auto v1 = renderer.getOverlayVolume("velocity");
    auto v2 = renderer.getOverlayVolume("vorticity");
    auto v3 = renderer.getOverlayVolume("energy_loss");
    EXPECT_NE(v1.Get(), v2.Get());
    EXPECT_NE(v2.Get(), v3.Get());
}

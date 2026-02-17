#include <gtest/gtest.h>

#include "ui/display_3d_controller.hpp"
#include "services/hemodynamic_surface_manager.hpp"
#include "services/surface_renderer.hpp"
#include "services/volume_renderer.hpp"

#include <vtkActor.h>
#include <vtkColorTransferFunction.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

using namespace dicom_viewer::ui;
using namespace dicom_viewer::services;

namespace {

/// Create a scalar volume for overlay testing
vtkSmartPointer<vtkImageData> createTestVolume(int dim = 8, float maxVal = 100.0f)
{
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

vtkSmartPointer<vtkColorTransferFunction> createColorTF(double maxVal)
{
    auto tf = vtkSmartPointer<vtkColorTransferFunction>::New();
    tf->AddRGBPoint(0.0, 0.0, 0.0, 1.0);
    tf->AddRGBPoint(maxVal, 1.0, 0.0, 0.0);
    return tf;
}

vtkSmartPointer<vtkPiecewiseFunction> createOpacityTF(double maxVal)
{
    auto tf = vtkSmartPointer<vtkPiecewiseFunction>::New();
    tf->AddPoint(0.0, 0.0);
    tf->AddPoint(maxVal, 0.5);
    return tf;
}

/// Create a sphere mesh with a named per-vertex scalar array
vtkSmartPointer<vtkPolyData> createMeshWithArray(
    const std::string& arrayName, double maxVal)
{
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetRadius(20.0);
    sphere->SetThetaResolution(12);
    sphere->SetPhiResolution(12);
    sphere->Update();

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->DeepCopy(sphere->GetOutput());

    auto nPts = polyData->GetNumberOfPoints();
    auto scalars = vtkSmartPointer<vtkFloatArray>::New();
    scalars->SetName(arrayName.c_str());
    scalars->SetNumberOfTuples(nPts);
    for (vtkIdType i = 0; i < nPts; ++i) {
        scalars->SetValue(i, static_cast<float>(i) / static_cast<float>(nPts) * maxVal);
    }

    polyData->GetPointData()->AddArray(scalars);
    polyData->GetPointData()->SetActiveScalars(arrayName.c_str());
    return polyData;
}

}  // anonymous namespace

// =============================================================================
// Construction and defaults
// =============================================================================

TEST(Display3DControllerTest, DefaultConstruction) {
    Display3DController ctrl;
    for (int i = 0; i < 13; ++i) {
        EXPECT_FALSE(ctrl.isEnabled(static_cast<Display3DItem>(i)));
    }
}

TEST(Display3DControllerTest, MoveConstruction) {
    Display3DController ctrl;
    ctrl.handleToggle(Display3DItem::Velocity, true);
    EXPECT_TRUE(ctrl.isEnabled(Display3DItem::Velocity));

    Display3DController moved(std::move(ctrl));
    EXPECT_TRUE(moved.isEnabled(Display3DItem::Velocity));
}

TEST(Display3DControllerTest, EnabledStatesArray) {
    Display3DController ctrl;
    auto states = ctrl.enabledStates();
    for (bool s : states) {
        EXPECT_FALSE(s);
    }

    ctrl.handleToggle(Display3DItem::WSS, true);
    ctrl.handleToggle(Display3DItem::Vorticity, true);
    states = ctrl.enabledStates();
    EXPECT_TRUE(states[static_cast<int>(Display3DItem::WSS)]);
    EXPECT_TRUE(states[static_cast<int>(Display3DItem::Vorticity)]);
    EXPECT_FALSE(states[static_cast<int>(Display3DItem::OSI)]);
}

// =============================================================================
// Safe no-op when renderers not set
// =============================================================================

TEST(Display3DControllerTest, ToggleWithoutRenderers_NoOp) {
    Display3DController ctrl;
    // Should not crash when no renderers are set
    ctrl.handleToggle(Display3DItem::WSS, true);
    ctrl.handleToggle(Display3DItem::Velocity, true);
    ctrl.handleToggle(Display3DItem::Streamline, true);
    ctrl.handleToggle(Display3DItem::MaskVolume, true);
    ctrl.handleToggle(Display3DItem::Surface, true);

    // State is still tracked even without renderers
    EXPECT_TRUE(ctrl.isEnabled(Display3DItem::WSS));
    EXPECT_TRUE(ctrl.isEnabled(Display3DItem::Velocity));
}

// =============================================================================
// Volume overlay visibility (Velocity, Vorticity, EnergyLoss, Magnitude)
// =============================================================================

class Display3DControllerVolumeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctrl = std::make_unique<Display3DController>();
        volumeRenderer = std::make_unique<VolumeRenderer>();
        ctrl->setVolumeRenderer(volumeRenderer.get());

        auto vol = createTestVolume();
        auto ctf = createColorTF(100.0);
        auto otf = createOpacityTF(100.0);

        // Add all four overlay types
        volumeRenderer->addScalarOverlay("velocity", vol, ctf, otf);
        volumeRenderer->addScalarOverlay("vorticity", vol, ctf, otf);
        volumeRenderer->addScalarOverlay("energy_loss", vol, ctf, otf);
        volumeRenderer->addScalarOverlay("magnitude", vol, ctf, otf);
    }

    std::unique_ptr<Display3DController> ctrl;
    std::unique_ptr<VolumeRenderer> volumeRenderer;
};

TEST_F(Display3DControllerVolumeTest, ToggleVelocity) {
    ctrl->handleToggle(Display3DItem::Velocity, false);
    EXPECT_FALSE(ctrl->isEnabled(Display3DItem::Velocity));

    ctrl->handleToggle(Display3DItem::Velocity, true);
    EXPECT_TRUE(ctrl->isEnabled(Display3DItem::Velocity));
}

TEST_F(Display3DControllerVolumeTest, ToggleVorticity) {
    ctrl->handleToggle(Display3DItem::Vorticity, true);
    EXPECT_TRUE(ctrl->isEnabled(Display3DItem::Vorticity));

    ctrl->handleToggle(Display3DItem::Vorticity, false);
    EXPECT_FALSE(ctrl->isEnabled(Display3DItem::Vorticity));
}

TEST_F(Display3DControllerVolumeTest, ToggleEnergyLoss) {
    ctrl->handleToggle(Display3DItem::EnergyLoss, true);
    EXPECT_TRUE(ctrl->isEnabled(Display3DItem::EnergyLoss));
}

TEST_F(Display3DControllerVolumeTest, ToggleMagnitude) {
    ctrl->handleToggle(Display3DItem::Magnitude, true);
    EXPECT_TRUE(ctrl->isEnabled(Display3DItem::Magnitude));
}

TEST_F(Display3DControllerVolumeTest, IndependentVolumeOverlays) {
    ctrl->handleToggle(Display3DItem::Velocity, true);
    ctrl->handleToggle(Display3DItem::Vorticity, true);
    ctrl->handleToggle(Display3DItem::EnergyLoss, false);

    EXPECT_TRUE(ctrl->isEnabled(Display3DItem::Velocity));
    EXPECT_TRUE(ctrl->isEnabled(Display3DItem::Vorticity));
    EXPECT_FALSE(ctrl->isEnabled(Display3DItem::EnergyLoss));
    EXPECT_FALSE(ctrl->isEnabled(Display3DItem::Magnitude));
}

// =============================================================================
// Hemodynamic surface visibility (WSS, OSI, AFI, RRT)
// =============================================================================

class Display3DControllerSurfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctrl = std::make_unique<Display3DController>();
        surfaceRenderer = std::make_unique<SurfaceRenderer>();
        hemoManager = std::make_unique<HemodynamicSurfaceManager>();
        ctrl->setSurfaceRenderer(surfaceRenderer.get());
        ctrl->setHemodynamicManager(hemoManager.get());

        // Add hemodynamic surfaces
        auto wssMesh = createMeshWithArray("WSS", 5.0);
        hemoManager->showWSS(*surfaceRenderer, wssMesh, 5.0);

        auto osiMesh = createMeshWithArray("OSI", 0.5);
        hemoManager->showOSI(*surfaceRenderer, osiMesh);

        auto tawssMesh = createMeshWithArray("TAWSS", 4.0);
        hemoManager->showAFI(*surfaceRenderer, tawssMesh);

        auto rrtMesh = createMeshWithArray("RRT", 100.0);
        hemoManager->showRRT(*surfaceRenderer, rrtMesh, 100.0);
    }

    std::unique_ptr<Display3DController> ctrl;
    std::unique_ptr<SurfaceRenderer> surfaceRenderer;
    std::unique_ptr<HemodynamicSurfaceManager> hemoManager;
};

TEST_F(Display3DControllerSurfaceTest, ToggleWSS) {
    ctrl->handleToggle(Display3DItem::WSS, false);
    EXPECT_FALSE(ctrl->isEnabled(Display3DItem::WSS));

    auto wssIdx = hemoManager->wssIndex();
    ASSERT_TRUE(wssIdx.has_value());
    auto config = surfaceRenderer->getSurfaceConfig(*wssIdx);
    EXPECT_FALSE(config.visible);

    ctrl->handleToggle(Display3DItem::WSS, true);
    config = surfaceRenderer->getSurfaceConfig(*wssIdx);
    EXPECT_TRUE(config.visible);
}

TEST_F(Display3DControllerSurfaceTest, ToggleOSI) {
    ctrl->handleToggle(Display3DItem::OSI, false);
    auto osiIdx = hemoManager->osiIndex();
    ASSERT_TRUE(osiIdx.has_value());
    auto config = surfaceRenderer->getSurfaceConfig(*osiIdx);
    EXPECT_FALSE(config.visible);
}

TEST_F(Display3DControllerSurfaceTest, ToggleAFI) {
    ctrl->handleToggle(Display3DItem::AFI, false);
    auto afiIdx = hemoManager->afiIndex();
    ASSERT_TRUE(afiIdx.has_value());
    auto config = surfaceRenderer->getSurfaceConfig(*afiIdx);
    EXPECT_FALSE(config.visible);
}

TEST_F(Display3DControllerSurfaceTest, ToggleRRT) {
    ctrl->handleToggle(Display3DItem::RRT, false);
    auto rrtIdx = hemoManager->rrtIndex();
    ASSERT_TRUE(rrtIdx.has_value());
    auto config = surfaceRenderer->getSurfaceConfig(*rrtIdx);
    EXPECT_FALSE(config.visible);
}

TEST_F(Display3DControllerSurfaceTest, IndependentSurfaces) {
    ctrl->handleToggle(Display3DItem::WSS, false);
    ctrl->handleToggle(Display3DItem::OSI, true);
    ctrl->handleToggle(Display3DItem::AFI, false);
    ctrl->handleToggle(Display3DItem::RRT, true);

    auto wssConfig = surfaceRenderer->getSurfaceConfig(*hemoManager->wssIndex());
    auto osiConfig = surfaceRenderer->getSurfaceConfig(*hemoManager->osiIndex());
    auto afiConfig = surfaceRenderer->getSurfaceConfig(*hemoManager->afiIndex());
    auto rrtConfig = surfaceRenderer->getSurfaceConfig(*hemoManager->rrtIndex());

    EXPECT_FALSE(wssConfig.visible);
    EXPECT_TRUE(osiConfig.visible);
    EXPECT_FALSE(afiConfig.visible);
    EXPECT_TRUE(rrtConfig.visible);
}

// =============================================================================
// Actor visibility (Streamline, MaskVolume, Surface)
// =============================================================================

TEST(Display3DControllerTest, ToggleStreamlineActor) {
    Display3DController ctrl;
    auto actor = vtkSmartPointer<vtkActor>::New();
    ctrl.setStreamlineActor(actor);

    EXPECT_EQ(actor->GetVisibility(), 1);  // VTK default

    ctrl.handleToggle(Display3DItem::Streamline, false);
    EXPECT_EQ(actor->GetVisibility(), 0);

    ctrl.handleToggle(Display3DItem::Streamline, true);
    EXPECT_EQ(actor->GetVisibility(), 1);
}

TEST(Display3DControllerTest, ToggleMaskVolumeActor) {
    Display3DController ctrl;
    auto actor = vtkSmartPointer<vtkActor>::New();
    ctrl.setMaskVolumeActor(actor);

    ctrl.handleToggle(Display3DItem::MaskVolume, false);
    EXPECT_EQ(actor->GetVisibility(), 0);

    ctrl.handleToggle(Display3DItem::MaskVolume, true);
    EXPECT_EQ(actor->GetVisibility(), 1);
}

TEST(Display3DControllerTest, ToggleSurfaceActor) {
    Display3DController ctrl;
    auto actor = vtkSmartPointer<vtkActor>::New();
    ctrl.setSurfaceActor(actor);

    ctrl.handleToggle(Display3DItem::Surface, false);
    EXPECT_EQ(actor->GetVisibility(), 0);

    ctrl.handleToggle(Display3DItem::Surface, true);
    EXPECT_EQ(actor->GetVisibility(), 1);
}

// =============================================================================
// Stub items (Cine, ASC) — should not crash, just track state
// =============================================================================

TEST(Display3DControllerTest, StubItems_Cine) {
    Display3DController ctrl;
    ctrl.handleToggle(Display3DItem::Cine, true);
    EXPECT_TRUE(ctrl.isEnabled(Display3DItem::Cine));

    ctrl.handleToggle(Display3DItem::Cine, false);
    EXPECT_FALSE(ctrl.isEnabled(Display3DItem::Cine));
}

TEST(Display3DControllerTest, StubItems_ASC) {
    Display3DController ctrl;
    ctrl.handleToggle(Display3DItem::ASC, true);
    EXPECT_TRUE(ctrl.isEnabled(Display3DItem::ASC));
}

// =============================================================================
// All 13 items independent toggling
// =============================================================================

TEST(Display3DControllerTest, AllItemsToggleIndependently) {
    Display3DController ctrl;

    // Enable all
    for (int i = 0; i < 13; ++i) {
        ctrl.handleToggle(static_cast<Display3DItem>(i), true);
    }
    for (int i = 0; i < 13; ++i) {
        EXPECT_TRUE(ctrl.isEnabled(static_cast<Display3DItem>(i)));
    }

    // Disable odd indices only
    for (int i = 0; i < 13; ++i) {
        if (i % 2 == 1) {
            ctrl.handleToggle(static_cast<Display3DItem>(i), false);
        }
    }
    for (int i = 0; i < 13; ++i) {
        if (i % 2 == 0) {
            EXPECT_TRUE(ctrl.isEnabled(static_cast<Display3DItem>(i)));
        } else {
            EXPECT_FALSE(ctrl.isEnabled(static_cast<Display3DItem>(i)));
        }
    }
}

// =============================================================================
// hasColormapRange — static classification
// =============================================================================

TEST(Display3DControllerTest, HasColormapRange_ColormapItems) {
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::WSS));
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::OSI));
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::AFI));
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::RRT));
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::Velocity));
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::Vorticity));
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::EnergyLoss));
    EXPECT_TRUE(Display3DController::hasColormapRange(Display3DItem::Magnitude));
}

TEST(Display3DControllerTest, HasColormapRange_NonColormapItems) {
    EXPECT_FALSE(Display3DController::hasColormapRange(Display3DItem::MaskVolume));
    EXPECT_FALSE(Display3DController::hasColormapRange(Display3DItem::Surface));
    EXPECT_FALSE(Display3DController::hasColormapRange(Display3DItem::Cine));
    EXPECT_FALSE(Display3DController::hasColormapRange(Display3DItem::ASC));
    EXPECT_FALSE(Display3DController::hasColormapRange(Display3DItem::Streamline));
}

// =============================================================================
// Scalar range — state tracking
// =============================================================================

TEST(Display3DControllerTest, ScalarRange_DefaultZero) {
    Display3DController ctrl;
    auto [min, max] = ctrl.scalarRange(Display3DItem::WSS);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 0.0);
}

TEST(Display3DControllerTest, ScalarRange_StoresValue) {
    Display3DController ctrl;
    ctrl.setScalarRange(Display3DItem::WSS, 0.5, 4.0);
    auto [min, max] = ctrl.scalarRange(Display3DItem::WSS);
    EXPECT_DOUBLE_EQ(min, 0.5);
    EXPECT_DOUBLE_EQ(max, 4.0);
}

TEST(Display3DControllerTest, ScalarRange_IndependentPerItem) {
    Display3DController ctrl;
    ctrl.setScalarRange(Display3DItem::WSS, 0.0, 5.0);
    ctrl.setScalarRange(Display3DItem::OSI, 0.0, 0.5);
    ctrl.setScalarRange(Display3DItem::Velocity, 0.0, 120.0);

    auto wss = ctrl.scalarRange(Display3DItem::WSS);
    auto osi = ctrl.scalarRange(Display3DItem::OSI);
    auto vel = ctrl.scalarRange(Display3DItem::Velocity);

    EXPECT_DOUBLE_EQ(wss.second, 5.0);
    EXPECT_DOUBLE_EQ(osi.second, 0.5);
    EXPECT_DOUBLE_EQ(vel.second, 120.0);
}

TEST(Display3DControllerTest, ScalarRange_IgnoredForNonColormap) {
    Display3DController ctrl;
    ctrl.setScalarRange(Display3DItem::Streamline, 1.0, 10.0);
    // Non-colormap items are rejected by setScalarRange
    auto [min, max] = ctrl.scalarRange(Display3DItem::Streamline);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 0.0);
}

TEST(Display3DControllerTest, ScalarRange_OverwritePrevious) {
    Display3DController ctrl;
    ctrl.setScalarRange(Display3DItem::RRT, 0.0, 50.0);
    ctrl.setScalarRange(Display3DItem::RRT, 10.0, 200.0);
    auto [min, max] = ctrl.scalarRange(Display3DItem::RRT);
    EXPECT_DOUBLE_EQ(min, 10.0);
    EXPECT_DOUBLE_EQ(max, 200.0);
}

// =============================================================================
// Scalar range — surface renderer integration
// =============================================================================

TEST_F(Display3DControllerSurfaceTest, SetScalarRange_WSS) {
    ctrl->setScalarRange(Display3DItem::WSS, 0.0, 3.0);
    auto [min, max] = ctrl->scalarRange(Display3DItem::WSS);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 3.0);
    // Verify no crash when routing to SurfaceRenderer
    ASSERT_TRUE(hemoManager->wssIndex().has_value());
}

TEST_F(Display3DControllerSurfaceTest, SetScalarRange_OSI) {
    ctrl->setScalarRange(Display3DItem::OSI, 0.0, 0.3);
    auto [min, max] = ctrl->scalarRange(Display3DItem::OSI);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 0.3);
    ASSERT_TRUE(hemoManager->osiIndex().has_value());
}

TEST_F(Display3DControllerSurfaceTest, SetScalarRange_AFI) {
    ctrl->setScalarRange(Display3DItem::AFI, 0.0, 1.5);
    auto [min, max] = ctrl->scalarRange(Display3DItem::AFI);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 1.5);
    ASSERT_TRUE(hemoManager->afiIndex().has_value());
}

TEST_F(Display3DControllerSurfaceTest, SetScalarRange_RRT) {
    ctrl->setScalarRange(Display3DItem::RRT, 5.0, 80.0);
    auto [min, max] = ctrl->scalarRange(Display3DItem::RRT);
    EXPECT_DOUBLE_EQ(min, 5.0);
    EXPECT_DOUBLE_EQ(max, 80.0);
    ASSERT_TRUE(hemoManager->rrtIndex().has_value());
}

// =============================================================================
// Scalar range — volume renderer integration
// =============================================================================

TEST_F(Display3DControllerVolumeTest, SetScalarRange_Velocity) {
    ctrl->setScalarRange(Display3DItem::Velocity, 0.0, 80.0);
    auto [min, max] = ctrl->scalarRange(Display3DItem::Velocity);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 80.0);
}

TEST_F(Display3DControllerVolumeTest, SetScalarRange_Vorticity) {
    ctrl->setScalarRange(Display3DItem::Vorticity, 0.0, 50.0);
    auto [min, max] = ctrl->scalarRange(Display3DItem::Vorticity);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 50.0);
}

TEST_F(Display3DControllerVolumeTest, SetScalarRange_EnergyLoss) {
    ctrl->setScalarRange(Display3DItem::EnergyLoss, 0.0, 75.0);
    auto [min, max] = ctrl->scalarRange(Display3DItem::EnergyLoss);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 75.0);
}

TEST_F(Display3DControllerVolumeTest, SetScalarRange_Magnitude) {
    ctrl->setScalarRange(Display3DItem::Magnitude, 10.0, 90.0);
    auto [min, max] = ctrl->scalarRange(Display3DItem::Magnitude);
    EXPECT_DOUBLE_EQ(min, 10.0);
    EXPECT_DOUBLE_EQ(max, 90.0);
}

TEST_F(Display3DControllerVolumeTest, SetScalarRange_WithoutRenderer_NoOp) {
    Display3DController detached;
    // Should not crash when no renderer is set
    detached.setScalarRange(Display3DItem::Velocity, 0.0, 100.0);
    auto [min, max] = detached.scalarRange(Display3DItem::Velocity);
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 100.0);
}

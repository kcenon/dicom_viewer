#include <gtest/gtest.h>

#include "services/mpr_renderer.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace dicom_viewer::services;

class MPRRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = std::make_unique<MPRRenderer>();
    }

    void TearDown() override {
        renderer.reset();
    }

    vtkSmartPointer<vtkImageData> createTestVolume(int dims = 64) {
        auto imageData = vtkSmartPointer<vtkImageData>::New();
        imageData->SetDimensions(dims, dims, dims);
        imageData->SetSpacing(1.0, 1.0, 1.0);
        imageData->SetOrigin(0.0, 0.0, 0.0);
        imageData->AllocateScalars(VTK_SHORT, 1);

        // Fill with gradient test data
        short* ptr = static_cast<short*>(imageData->GetScalarPointer());
        for (int z = 0; z < dims; ++z) {
            for (int y = 0; y < dims; ++y) {
                for (int x = 0; x < dims; ++x) {
                    int idx = z * dims * dims + y * dims + x;
                    ptr[idx] = static_cast<short>((x + y + z) % 1000);
                }
            }
        }

        return imageData;
    }

    std::unique_ptr<MPRRenderer> renderer;
};

// Test construction
TEST_F(MPRRendererTest, DefaultConstruction) {
    EXPECT_NE(renderer, nullptr);
}

// Test move semantics
TEST_F(MPRRendererTest, MoveConstructor) {
    MPRRenderer moved(std::move(*renderer));
    EXPECT_NE(moved.getRenderer(MPRPlane::Axial), nullptr);
}

TEST_F(MPRRendererTest, MoveAssignment) {
    MPRRenderer other;
    other = std::move(*renderer);
    EXPECT_NE(other.getRenderer(MPRPlane::Axial), nullptr);
}

// Test renderer retrieval for each plane
TEST_F(MPRRendererTest, GetRendererAxial) {
    auto axialRenderer = renderer->getRenderer(MPRPlane::Axial);
    EXPECT_NE(axialRenderer, nullptr);
}

TEST_F(MPRRendererTest, GetRendererCoronal) {
    auto coronalRenderer = renderer->getRenderer(MPRPlane::Coronal);
    EXPECT_NE(coronalRenderer, nullptr);
}

TEST_F(MPRRendererTest, GetRendererSagittal) {
    auto sagittalRenderer = renderer->getRenderer(MPRPlane::Sagittal);
    EXPECT_NE(sagittalRenderer, nullptr);
}

TEST_F(MPRRendererTest, AllRenderersAreDifferent) {
    auto axial = renderer->getRenderer(MPRPlane::Axial);
    auto coronal = renderer->getRenderer(MPRPlane::Coronal);
    auto sagittal = renderer->getRenderer(MPRPlane::Sagittal);

    EXPECT_NE(axial, coronal);
    EXPECT_NE(axial, sagittal);
    EXPECT_NE(coronal, sagittal);
}

// Test input data
TEST_F(MPRRendererTest, SetInputDataAcceptsValidVolume) {
    auto volume = createTestVolume();
    EXPECT_NO_THROW(renderer->setInputData(volume));
}

TEST_F(MPRRendererTest, SetInputDataAcceptsNullptr) {
    EXPECT_NO_THROW(renderer->setInputData(nullptr));
}

// Test slice position
TEST_F(MPRRendererTest, GetSliceRangeWithoutData) {
    auto [min, max] = renderer->getSliceRange(MPRPlane::Axial);
    EXPECT_EQ(min, 0.0);
    EXPECT_EQ(max, 0.0);
}

TEST_F(MPRRendererTest, GetSliceRangeWithData) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    auto [min, max] = renderer->getSliceRange(MPRPlane::Axial);
    EXPECT_EQ(min, 0.0);
    EXPECT_EQ(max, 63.0);
}

TEST_F(MPRRendererTest, SetSlicePositionValid) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSlicePosition(MPRPlane::Axial, 32.0);
    EXPECT_EQ(renderer->getSlicePosition(MPRPlane::Axial), 32.0);
}

TEST_F(MPRRendererTest, SetSlicePositionClampsToRange) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Try to set position beyond range
    renderer->setSlicePosition(MPRPlane::Axial, 100.0);
    EXPECT_LE(renderer->getSlicePosition(MPRPlane::Axial), 63.0);

    renderer->setSlicePosition(MPRPlane::Axial, -10.0);
    EXPECT_GE(renderer->getSlicePosition(MPRPlane::Axial), 0.0);
}

// Test scroll slice
TEST_F(MPRRendererTest, ScrollSliceForward) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    double initialPos = renderer->getSlicePosition(MPRPlane::Axial);
    renderer->scrollSlice(MPRPlane::Axial, 5);
    EXPECT_GT(renderer->getSlicePosition(MPRPlane::Axial), initialPos);
}

TEST_F(MPRRendererTest, ScrollSliceBackward) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSlicePosition(MPRPlane::Axial, 32.0);
    double initialPos = renderer->getSlicePosition(MPRPlane::Axial);
    renderer->scrollSlice(MPRPlane::Axial, -5);
    EXPECT_LT(renderer->getSlicePosition(MPRPlane::Axial), initialPos);
}

// Test window/level
TEST_F(MPRRendererTest, SetWindowLevelValidValues) {
    EXPECT_NO_THROW(renderer->setWindowLevel(400.0, 40.0));

    auto [width, center] = renderer->getWindowLevel();
    EXPECT_EQ(width, 400.0);
    EXPECT_EQ(center, 40.0);
}

TEST_F(MPRRendererTest, SetWindowLevelNegativeValues) {
    EXPECT_NO_THROW(renderer->setWindowLevel(1500.0, -600.0));

    auto [width, center] = renderer->getWindowLevel();
    EXPECT_EQ(width, 1500.0);
    EXPECT_EQ(center, -600.0);
}

// Test crosshair
TEST_F(MPRRendererTest, SetCrosshairPosition) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setCrosshairPosition(10.0, 20.0, 30.0);
    auto pos = renderer->getCrosshairPosition();

    EXPECT_EQ(pos[0], 10.0);
    EXPECT_EQ(pos[1], 20.0);
    EXPECT_EQ(pos[2], 30.0);
}

TEST_F(MPRRendererTest, CrosshairSynchronizesSlicePositions) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setCrosshairPosition(15.0, 25.0, 35.0);

    // Crosshair should update slice positions
    EXPECT_EQ(renderer->getSlicePosition(MPRPlane::Sagittal), 15.0);  // X
    EXPECT_EQ(renderer->getSlicePosition(MPRPlane::Coronal), 25.0);   // Y
    EXPECT_EQ(renderer->getSlicePosition(MPRPlane::Axial), 35.0);     // Z
}

TEST_F(MPRRendererTest, SetCrosshairVisible) {
    EXPECT_NO_THROW(renderer->setCrosshairVisible(true));
    EXPECT_NO_THROW(renderer->setCrosshairVisible(false));
}

// Test slab mode
TEST_F(MPRRendererTest, SetSlabModeNone) {
    EXPECT_NO_THROW(renderer->setSlabMode(SlabMode::None));
}

TEST_F(MPRRendererTest, SetSlabModeMIP) {
    EXPECT_NO_THROW(renderer->setSlabMode(SlabMode::MIP, 10.0));
}

TEST_F(MPRRendererTest, SetSlabModeMinIP) {
    EXPECT_NO_THROW(renderer->setSlabMode(SlabMode::MinIP, 5.0));
}

TEST_F(MPRRendererTest, SetSlabModeAverage) {
    EXPECT_NO_THROW(renderer->setSlabMode(SlabMode::Average, 8.0));
}

// Test callbacks
TEST_F(MPRRendererTest, SetSlicePositionCallback) {
    bool callbackCalled = false;
    MPRPlane callbackPlane = MPRPlane::Axial;
    double callbackPosition = 0.0;

    renderer->setSlicePositionCallback(
        [&](MPRPlane plane, double position) {
            callbackCalled = true;
            callbackPlane = plane;
            callbackPosition = position;
        });

    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSlicePosition(MPRPlane::Coronal, 20.0);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(callbackPlane, MPRPlane::Coronal);
    EXPECT_EQ(callbackPosition, 20.0);
}

TEST_F(MPRRendererTest, SetCrosshairCallback) {
    bool callbackCalled = false;
    double cbX = 0, cbY = 0, cbZ = 0;

    renderer->setCrosshairCallback(
        [&](double x, double y, double z) {
            callbackCalled = true;
            cbX = x;
            cbY = y;
            cbZ = z;
        });

    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setCrosshairPosition(10.0, 20.0, 30.0);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(cbX, 10.0);
    EXPECT_EQ(cbY, 20.0);
    EXPECT_EQ(cbZ, 30.0);
}

// Test update
TEST_F(MPRRendererTest, UpdateDoesNotThrow) {
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(MPRRendererTest, UpdateWithData) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);
    EXPECT_NO_THROW(renderer->update());
}

// Test reset views
TEST_F(MPRRendererTest, ResetViewsWithoutData) {
    EXPECT_NO_THROW(renderer->resetViews());
}

TEST_F(MPRRendererTest, ResetViewsCentersSlices) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Move slices away from center
    renderer->setSlicePosition(MPRPlane::Axial, 10.0);
    renderer->setSlicePosition(MPRPlane::Coronal, 10.0);
    renderer->setSlicePosition(MPRPlane::Sagittal, 10.0);

    // Reset should center all slices
    renderer->resetViews();

    // Center of 64x64x64 volume (0-63) is 31.5
    EXPECT_NEAR(renderer->getSlicePosition(MPRPlane::Axial), 31.5, 0.5);
    EXPECT_NEAR(renderer->getSlicePosition(MPRPlane::Coronal), 31.5, 0.5);
    EXPECT_NEAR(renderer->getSlicePosition(MPRPlane::Sagittal), 31.5, 0.5);
}

// Test slice range for all planes
TEST_F(MPRRendererTest, SliceRangeAxial) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    auto [min, max] = renderer->getSliceRange(MPRPlane::Axial);
    EXPECT_EQ(min, 0.0);
    EXPECT_EQ(max, 63.0);
}

TEST_F(MPRRendererTest, SliceRangeCoronal) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    auto [min, max] = renderer->getSliceRange(MPRPlane::Coronal);
    EXPECT_EQ(min, 0.0);
    EXPECT_EQ(max, 63.0);
}

TEST_F(MPRRendererTest, SliceRangeSagittal) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    auto [min, max] = renderer->getSliceRange(MPRPlane::Sagittal);
    EXPECT_EQ(min, 0.0);
    EXPECT_EQ(max, 63.0);
}

// Test with anisotropic spacing
TEST_F(MPRRendererTest, AnisotropicSpacing) {
    auto imageData = vtkSmartPointer<vtkImageData>::New();
    imageData->SetDimensions(64, 64, 32);
    imageData->SetSpacing(0.5, 0.5, 2.0);  // Different Z spacing
    imageData->SetOrigin(0.0, 0.0, 0.0);
    imageData->AllocateScalars(VTK_SHORT, 1);

    renderer->setInputData(imageData);

    // Axial should have range 0-62 (31 slices * 2.0 spacing)
    auto [minZ, maxZ] = renderer->getSliceRange(MPRPlane::Axial);
    EXPECT_NEAR(maxZ, 62.0, 0.1);

    // X and Y should have range 0-31.5 (64 pixels * 0.5 spacing)
    auto [minX, maxX] = renderer->getSliceRange(MPRPlane::Sagittal);
    EXPECT_NEAR(maxX, 31.5, 0.1);
}

// ==================== Comprehensive Thick Slab Tests ====================

// Test slab mode getter after setter
TEST_F(MPRRendererTest, GetSlabModeReturnsSetValue) {
    renderer->setSlabMode(SlabMode::MIP, 15.0);
    EXPECT_EQ(renderer->getSlabMode(), SlabMode::MIP);

    renderer->setSlabMode(SlabMode::MinIP, 20.0);
    EXPECT_EQ(renderer->getSlabMode(), SlabMode::MinIP);

    renderer->setSlabMode(SlabMode::Average, 10.0);
    EXPECT_EQ(renderer->getSlabMode(), SlabMode::Average);

    renderer->setSlabMode(SlabMode::None);
    EXPECT_EQ(renderer->getSlabMode(), SlabMode::None);
}

// Test slab thickness getter
TEST_F(MPRRendererTest, GetSlabThicknessReturnsSetValue) {
    renderer->setSlabMode(SlabMode::MIP, 25.0);
    EXPECT_DOUBLE_EQ(renderer->getSlabThickness(), 25.0);

    renderer->setSlabMode(SlabMode::MinIP, 5.5);
    EXPECT_DOUBLE_EQ(renderer->getSlabThickness(), 5.5);
}

// Test thickness clamping (min/max 1-100mm)
TEST_F(MPRRendererTest, SlabThicknessClampedToRange) {
    renderer->setSlabMode(SlabMode::MIP, 0.5);  // Below minimum
    EXPECT_GE(renderer->getSlabThickness(), 1.0);

    renderer->setSlabMode(SlabMode::MIP, 150.0);  // Above maximum
    EXPECT_LE(renderer->getSlabThickness(), 100.0);
}

// Test plane-specific slab mode
TEST_F(MPRRendererTest, SetPlaneSlabModeIndependent) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Set different modes for each plane
    renderer->setPlaneSlabMode(MPRPlane::Axial, SlabMode::MIP, 10.0);
    renderer->setPlaneSlabMode(MPRPlane::Coronal, SlabMode::MinIP, 15.0);
    renderer->setPlaneSlabMode(MPRPlane::Sagittal, SlabMode::Average, 20.0);

    EXPECT_EQ(renderer->getPlaneSlabMode(MPRPlane::Axial), SlabMode::MIP);
    EXPECT_EQ(renderer->getPlaneSlabMode(MPRPlane::Coronal), SlabMode::MinIP);
    EXPECT_EQ(renderer->getPlaneSlabMode(MPRPlane::Sagittal), SlabMode::Average);
}

// Test plane-specific thickness
TEST_F(MPRRendererTest, GetPlaneSlabThicknessIndependent) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setPlaneSlabMode(MPRPlane::Axial, SlabMode::MIP, 10.0);
    renderer->setPlaneSlabMode(MPRPlane::Coronal, SlabMode::MIP, 15.0);
    renderer->setPlaneSlabMode(MPRPlane::Sagittal, SlabMode::MIP, 20.0);

    EXPECT_DOUBLE_EQ(renderer->getPlaneSlabThickness(MPRPlane::Axial), 10.0);
    EXPECT_DOUBLE_EQ(renderer->getPlaneSlabThickness(MPRPlane::Coronal), 15.0);
    EXPECT_DOUBLE_EQ(renderer->getPlaneSlabThickness(MPRPlane::Sagittal), 20.0);
}

// Test global mode overrides plane-specific
TEST_F(MPRRendererTest, GlobalSlabModeResetsPlaneSpecific) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Set plane-specific first
    renderer->setPlaneSlabMode(MPRPlane::Axial, SlabMode::MIP, 10.0);

    // Then set global mode
    renderer->setSlabMode(SlabMode::MinIP, 25.0);

    // Plane should now use global mode
    EXPECT_EQ(renderer->getPlaneSlabMode(MPRPlane::Axial), SlabMode::MinIP);
    EXPECT_DOUBLE_EQ(renderer->getPlaneSlabThickness(MPRPlane::Axial), 25.0);
}

// Test effective slice count with uniform spacing
TEST_F(MPRRendererTest, EffectiveSliceCountUniformSpacing) {
    auto volume = createTestVolume(64);  // 1.0 spacing
    renderer->setInputData(volume);

    renderer->setSlabMode(SlabMode::MIP, 10.0);

    // With 1.0mm spacing and 10.0mm thickness, expect 10 slices
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Axial), 10);
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Coronal), 10);
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Sagittal), 10);
}

// Test effective slice count with anisotropic spacing
TEST_F(MPRRendererTest, EffectiveSliceCountAnisotropicSpacing) {
    auto imageData = vtkSmartPointer<vtkImageData>::New();
    imageData->SetDimensions(64, 64, 32);
    imageData->SetSpacing(1.0, 1.0, 2.0);  // Z has 2.0mm spacing
    imageData->SetOrigin(0.0, 0.0, 0.0);
    imageData->AllocateScalars(VTK_SHORT, 1);

    renderer->setInputData(imageData);
    renderer->setSlabMode(SlabMode::MIP, 10.0);

    // Axial (Z-axis): 10mm / 2.0mm = 5 slices
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Axial), 5);
    // Coronal (Y-axis): 10mm / 1.0mm = 10 slices
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Coronal), 10);
    // Sagittal (X-axis): 10mm / 1.0mm = 10 slices
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Sagittal), 10);
}

// Test effective slice count minimum is 1
TEST_F(MPRRendererTest, EffectiveSliceCountMinimumIsOne) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // With SlabMode::None, always 1 slice
    renderer->setSlabMode(SlabMode::None);
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Axial), 1);

    // Even with very small thickness, minimum is 1
    renderer->setSlabMode(SlabMode::MIP, 0.1);  // Will be clamped to 1.0
    EXPECT_GE(renderer->getEffectiveSliceCount(MPRPlane::Axial), 1);
}

// Test slab mode with different volume dimensions
TEST_F(MPRRendererTest, SlabModeWithLargeVolume) {
    // Simulate typical CT volume: 512x512x300
    auto imageData = vtkSmartPointer<vtkImageData>::New();
    imageData->SetDimensions(128, 128, 75);  // Reduced for test performance
    imageData->SetSpacing(0.5, 0.5, 1.5);  // Typical CT spacing
    imageData->SetOrigin(0.0, 0.0, 0.0);
    imageData->AllocateScalars(VTK_SHORT, 1);

    renderer->setInputData(imageData);

    // Test MIP with 20mm thickness (common for CT angio)
    renderer->setSlabMode(SlabMode::MIP, 20.0);
    EXPECT_EQ(renderer->getSlabMode(), SlabMode::MIP);
    EXPECT_DOUBLE_EQ(renderer->getSlabThickness(), 20.0);

    // Axial: 20mm / 1.5mm ≈ 13 slices
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Axial), 13);
    // Coronal/Sagittal: 20mm / 0.5mm = 40 slices
    EXPECT_EQ(renderer->getEffectiveSliceCount(MPRPlane::Coronal), 40);
}

// Test update still works after slab mode changes
TEST_F(MPRRendererTest, UpdateAfterSlabModeChange) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSlabMode(SlabMode::MIP, 10.0);
    EXPECT_NO_THROW(renderer->update());

    renderer->setSlabMode(SlabMode::MinIP, 5.0);
    EXPECT_NO_THROW(renderer->update());

    renderer->setSlabMode(SlabMode::Average, 15.0);
    EXPECT_NO_THROW(renderer->update());

    renderer->setSlabMode(SlabMode::None);
    EXPECT_NO_THROW(renderer->update());
}

// Test default values
TEST_F(MPRRendererTest, DefaultSlabModeIsNone) {
    EXPECT_EQ(renderer->getSlabMode(), SlabMode::None);
}

TEST_F(MPRRendererTest, DefaultSlabThickness) {
    EXPECT_DOUBLE_EQ(renderer->getSlabThickness(), 1.0);
}

// Test slab mode persistence across slice changes
TEST_F(MPRRendererTest, SlabModePersistsAfterSliceChange) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSlabMode(SlabMode::MIP, 12.0);
    renderer->setSlicePosition(MPRPlane::Axial, 32.0);
    renderer->scrollSlice(MPRPlane::Axial, 5);

    EXPECT_EQ(renderer->getSlabMode(), SlabMode::MIP);
    EXPECT_DOUBLE_EQ(renderer->getSlabThickness(), 12.0);
}

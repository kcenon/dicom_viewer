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

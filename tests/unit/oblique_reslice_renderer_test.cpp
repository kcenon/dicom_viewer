#include <gtest/gtest.h>

#include "services/render/oblique_reslice_renderer.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <cmath>

using namespace dicom_viewer::services;

class ObliqueResliceRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = std::make_unique<ObliqueResliceRenderer>();
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

    std::unique_ptr<ObliqueResliceRenderer> renderer;
};

// ==================== Construction Tests ====================

TEST_F(ObliqueResliceRendererTest, DefaultConstruction) {
    EXPECT_NE(renderer, nullptr);
}

TEST_F(ObliqueResliceRendererTest, MoveConstructor) {
    ObliqueResliceRenderer moved(std::move(*renderer));
    EXPECT_EQ(moved.getInputData(), nullptr);
}

TEST_F(ObliqueResliceRendererTest, MoveAssignment) {
    ObliqueResliceRenderer other;
    other = std::move(*renderer);
    EXPECT_EQ(other.getInputData(), nullptr);
}

// ==================== Input Data Tests ====================

TEST_F(ObliqueResliceRendererTest, SetInputDataAcceptsValidVolume) {
    auto volume = createTestVolume();
    EXPECT_NO_THROW(renderer->setInputData(volume));
    EXPECT_EQ(renderer->getInputData(), volume);
}

TEST_F(ObliqueResliceRendererTest, SetInputDataAcceptsNullptr) {
    EXPECT_NO_THROW(renderer->setInputData(nullptr));
    EXPECT_EQ(renderer->getInputData(), nullptr);
}

// ==================== Plane Definition by Rotation Tests ====================

TEST_F(ObliqueResliceRendererTest, SetPlaneByRotationZeroAngles) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    EXPECT_NO_THROW(renderer->setPlaneByRotation(0.0, 0.0, 0.0));

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationX, 0.0);
    EXPECT_DOUBLE_EQ(plane.rotationY, 0.0);
    EXPECT_DOUBLE_EQ(plane.rotationZ, 0.0);
}

TEST_F(ObliqueResliceRendererTest, SetPlaneByRotation45Degrees) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    renderer->setPlaneByRotation(45.0, 0.0, 0.0);

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationX, 45.0);
    EXPECT_DOUBLE_EQ(plane.rotationY, 0.0);
    EXPECT_DOUBLE_EQ(plane.rotationZ, 0.0);
}

TEST_F(ObliqueResliceRendererTest, SetPlaneByRotationCombined) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    renderer->setPlaneByRotation(15.0, -8.5, 0.0);

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationX, 15.0);
    EXPECT_DOUBLE_EQ(plane.rotationY, -8.5);
    EXPECT_DOUBLE_EQ(plane.rotationZ, 0.0);
}

// ==================== Plane Definition by Three Points Tests ====================

TEST_F(ObliqueResliceRendererTest, SetPlaneByThreePointsXYPlane) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    // Three points defining XY plane (Z=0)
    Point3D p1{0.0, 0.0, 0.0};
    Point3D p2{1.0, 0.0, 0.0};
    Point3D p3{0.0, 1.0, 0.0};

    EXPECT_NO_THROW(renderer->setPlaneByThreePoints(p1, p2, p3));

    // Normal should be approximately (0, 0, 1)
    auto normal = renderer->getPlaneNormal();
    EXPECT_NEAR(normal.z, 1.0, 0.01);
    EXPECT_NEAR(std::abs(normal.x), 0.0, 0.01);
    EXPECT_NEAR(std::abs(normal.y), 0.0, 0.01);
}

TEST_F(ObliqueResliceRendererTest, SetPlaneByThreePointsXZPlane) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    // Three points defining XZ plane (Y=0)
    Point3D p1{0.0, 0.0, 0.0};
    Point3D p2{1.0, 0.0, 0.0};
    Point3D p3{0.0, 0.0, 1.0};

    EXPECT_NO_THROW(renderer->setPlaneByThreePoints(p1, p2, p3));

    // Normal should be approximately (0, -1, 0)
    auto normal = renderer->getPlaneNormal();
    EXPECT_NEAR(std::abs(normal.y), 1.0, 0.01);
}

// ==================== Plane Definition by Normal Tests ====================

TEST_F(ObliqueResliceRendererTest, SetPlaneByNormalZAxis) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    Vector3D normal{0.0, 0.0, 1.0};
    Point3D center{32.0, 32.0, 32.0};

    EXPECT_NO_THROW(renderer->setPlaneByNormal(normal, center));

    auto resultNormal = renderer->getPlaneNormal();
    EXPECT_NEAR(resultNormal.z, 1.0, 0.01);
}

TEST_F(ObliqueResliceRendererTest, SetPlaneByNormalDiagonal) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    // Normalized diagonal vector
    double inv_sqrt3 = 1.0 / std::sqrt(3.0);
    Vector3D normal{inv_sqrt3, inv_sqrt3, inv_sqrt3};
    Point3D center{32.0, 32.0, 32.0};

    EXPECT_NO_THROW(renderer->setPlaneByNormal(normal, center));

    auto resultNormal = renderer->getPlaneNormal();
    double length = resultNormal.length();
    EXPECT_NEAR(length, 1.0, 0.01);
}

// ==================== Center Point Tests ====================

TEST_F(ObliqueResliceRendererTest, SetAndGetCenter) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    Point3D center{10.0, 20.0, 30.0};
    renderer->setCenter(center);

    auto result = renderer->getCenter();
    EXPECT_DOUBLE_EQ(result.x, 10.0);
    EXPECT_DOUBLE_EQ(result.y, 20.0);
    EXPECT_DOUBLE_EQ(result.z, 30.0);
}

// ==================== Slice Navigation Tests ====================

TEST_F(ObliqueResliceRendererTest, GetSliceRangeWithoutData) {
    auto [min, max] = renderer->getSliceRange();
    EXPECT_DOUBLE_EQ(min, 0.0);
    EXPECT_DOUBLE_EQ(max, 0.0);
}

TEST_F(ObliqueResliceRendererTest, GetSliceRangeWithData) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    auto [min, max] = renderer->getSliceRange();
    // Range should be approximately half the diagonal
    EXPECT_LT(min, 0.0);
    EXPECT_GT(max, 0.0);
    EXPECT_DOUBLE_EQ(min, -max);  // Symmetric range
}

TEST_F(ObliqueResliceRendererTest, SetSliceOffsetValid) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSliceOffset(10.0);
    EXPECT_DOUBLE_EQ(renderer->getSliceOffset(), 10.0);
}

TEST_F(ObliqueResliceRendererTest, SetSliceOffsetClampsToRange) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    auto [min, max] = renderer->getSliceRange();

    // Try to set beyond max
    renderer->setSliceOffset(max + 100.0);
    EXPECT_LE(renderer->getSliceOffset(), max);

    // Try to set beyond min
    renderer->setSliceOffset(min - 100.0);
    EXPECT_GE(renderer->getSliceOffset(), min);
}

TEST_F(ObliqueResliceRendererTest, ScrollSliceForward) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSliceOffset(0.0);
    double initial = renderer->getSliceOffset();

    renderer->scrollSlice(5);
    EXPECT_GT(renderer->getSliceOffset(), initial);
}

TEST_F(ObliqueResliceRendererTest, ScrollSliceBackward) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    renderer->setSliceOffset(10.0);
    double initial = renderer->getSliceOffset();

    renderer->scrollSlice(-5);
    EXPECT_LT(renderer->getSliceOffset(), initial);
}

// ==================== Preset Planes Tests ====================

TEST_F(ObliqueResliceRendererTest, SetAxial) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    EXPECT_NO_THROW(renderer->setAxial());

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationX, 0.0);
    EXPECT_DOUBLE_EQ(plane.rotationY, 0.0);
    EXPECT_DOUBLE_EQ(plane.rotationZ, 0.0);
}

TEST_F(ObliqueResliceRendererTest, SetCoronal) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    EXPECT_NO_THROW(renderer->setCoronal());

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationX, -90.0);
}

TEST_F(ObliqueResliceRendererTest, SetSagittal) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    EXPECT_NO_THROW(renderer->setSagittal());

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationY, 90.0);
}

// ==================== Interactive Rotation Tests ====================

TEST_F(ObliqueResliceRendererTest, InteractiveRotationNotActiveByDefault) {
    EXPECT_FALSE(renderer->isInteractiveRotationActive());
}

TEST_F(ObliqueResliceRendererTest, StartInteractiveRotation) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    renderer->startInteractiveRotation(100, 100);
    EXPECT_TRUE(renderer->isInteractiveRotationActive());
}

TEST_F(ObliqueResliceRendererTest, EndInteractiveRotation) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    renderer->startInteractiveRotation(100, 100);
    renderer->endInteractiveRotation();
    EXPECT_FALSE(renderer->isInteractiveRotationActive());
}

TEST_F(ObliqueResliceRendererTest, UpdateInteractiveRotation) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    renderer->setPlaneByRotation(0.0, 0.0, 0.0);
    renderer->startInteractiveRotation(100, 100);

    // Move 20 pixels to the right (should change Y rotation)
    renderer->updateInteractiveRotation(120, 100);

    auto plane = renderer->getCurrentPlane();
    EXPECT_NE(plane.rotationY, 0.0);

    renderer->endInteractiveRotation();
}

// ==================== Window/Level Tests ====================

TEST_F(ObliqueResliceRendererTest, SetWindowLevelValidValues) {
    EXPECT_NO_THROW(renderer->setWindowLevel(400.0, 40.0));

    auto [width, center] = renderer->getWindowLevel();
    EXPECT_DOUBLE_EQ(width, 400.0);
    EXPECT_DOUBLE_EQ(center, 40.0);
}

TEST_F(ObliqueResliceRendererTest, SetWindowLevelNegativeCenter) {
    EXPECT_NO_THROW(renderer->setWindowLevel(1500.0, -600.0));

    auto [width, center] = renderer->getWindowLevel();
    EXPECT_DOUBLE_EQ(width, 1500.0);
    EXPECT_DOUBLE_EQ(center, -600.0);
}

// ==================== Options Tests ====================

TEST_F(ObliqueResliceRendererTest, SetAndGetOptions) {
    ObliqueResliceOptions options;
    options.interpolation = InterpolationMode::Cubic;
    options.outputDimensions = {256, 256};
    options.backgroundValue = -2000.0;

    renderer->setOptions(options);

    auto result = renderer->getOptions();
    EXPECT_EQ(result.interpolation, InterpolationMode::Cubic);
    EXPECT_EQ(result.outputDimensions[0], 256);
    EXPECT_EQ(result.outputDimensions[1], 256);
    EXPECT_DOUBLE_EQ(result.backgroundValue, -2000.0);
}

// ==================== Reslice Matrix Tests ====================

TEST_F(ObliqueResliceRendererTest, GetResliceMatrixNotNull) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    auto matrix = renderer->getResliceMatrix();
    EXPECT_NE(matrix, nullptr);
}

TEST_F(ObliqueResliceRendererTest, GetPlaneNormalZAxisForAxial) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);
    renderer->setAxial();

    auto normal = renderer->getPlaneNormal();
    EXPECT_NEAR(normal.x, 0.0, 0.01);
    EXPECT_NEAR(normal.y, 0.0, 0.01);
    EXPECT_NEAR(normal.z, 1.0, 0.01);
}

// ==================== Update and Reset Tests ====================

TEST_F(ObliqueResliceRendererTest, UpdateDoesNotThrow) {
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(ObliqueResliceRendererTest, UpdateWithData) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(ObliqueResliceRendererTest, ResetViewWithoutData) {
    EXPECT_NO_THROW(renderer->resetView());
}

TEST_F(ObliqueResliceRendererTest, ResetViewCentersAndResetsRotation) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Change rotation and offset
    renderer->setPlaneByRotation(45.0, 30.0, 0.0);
    renderer->setSliceOffset(20.0);

    // Reset should restore defaults
    renderer->resetView();

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationX, 0.0);
    EXPECT_DOUBLE_EQ(plane.rotationY, 0.0);
    EXPECT_DOUBLE_EQ(plane.rotationZ, 0.0);
    EXPECT_DOUBLE_EQ(plane.sliceOffset, 0.0);
}

// ==================== Callback Tests ====================

TEST_F(ObliqueResliceRendererTest, PlaneChangedCallback) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    bool callbackCalled = false;
    ObliquePlaneDefinition receivedPlane;

    renderer->setPlaneChangedCallback([&](const ObliquePlaneDefinition& plane) {
        callbackCalled = true;
        receivedPlane = plane;
    });

    renderer->setPlaneByRotation(30.0, 0.0, 0.0);

    EXPECT_TRUE(callbackCalled);
    EXPECT_DOUBLE_EQ(receivedPlane.rotationX, 30.0);
}

TEST_F(ObliqueResliceRendererTest, SliceChangedCallback) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    bool callbackCalled = false;
    double receivedOffset = 0.0;

    renderer->setSliceChangedCallback([&](double offset) {
        callbackCalled = true;
        receivedOffset = offset;
    });

    renderer->setSliceOffset(15.0);

    EXPECT_TRUE(callbackCalled);
    EXPECT_DOUBLE_EQ(receivedOffset, 15.0);
}

// ==================== Vector3D Tests ====================

TEST_F(ObliqueResliceRendererTest, Vector3DLength) {
    Vector3D v{3.0, 4.0, 0.0};
    EXPECT_DOUBLE_EQ(v.length(), 5.0);
}

TEST_F(ObliqueResliceRendererTest, Vector3DNormalized) {
    Vector3D v{3.0, 4.0, 0.0};
    auto normalized = v.normalized();

    EXPECT_NEAR(normalized.length(), 1.0, 0.0001);
    EXPECT_DOUBLE_EQ(normalized.x, 0.6);
    EXPECT_DOUBLE_EQ(normalized.y, 0.8);
}

TEST_F(ObliqueResliceRendererTest, Vector3DNormalizedZeroVector) {
    Vector3D v{0.0, 0.0, 0.0};
    auto normalized = v.normalized();

    // Should return default unit vector
    EXPECT_NEAR(normalized.length(), 1.0, 0.0001);
}

// ==================== Renderer Assignment Tests ====================

TEST_F(ObliqueResliceRendererTest, GetRendererReturnsNullByDefault) {
    EXPECT_EQ(renderer->getRenderer(), nullptr);
}

// ==================== Anisotropic Spacing Tests ====================

TEST_F(ObliqueResliceRendererTest, AnisotropicSpacing) {
    auto imageData = vtkSmartPointer<vtkImageData>::New();
    imageData->SetDimensions(64, 64, 32);
    imageData->SetSpacing(0.5, 0.5, 2.0);  // Different Z spacing
    imageData->SetOrigin(0.0, 0.0, 0.0);
    imageData->AllocateScalars(VTK_SHORT, 1);

    renderer->setInputData(imageData);

    // Slice range should account for anisotropic spacing
    auto [min, max] = renderer->getSliceRange();
    EXPECT_GT(max, 0.0);
}

// ==================== Edge Cases ====================

TEST_F(ObliqueResliceRendererTest, RotationNear90Degrees) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    // Should not cause gimbal lock issues
    EXPECT_NO_THROW(renderer->setPlaneByRotation(89.0, 0.0, 0.0));

    auto plane = renderer->getCurrentPlane();
    EXPECT_DOUBLE_EQ(plane.rotationX, 89.0);
}

TEST_F(ObliqueResliceRendererTest, LargeRotationValues) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    // Large rotation values should work
    EXPECT_NO_THROW(renderer->setPlaneByRotation(0.0, 180.0, 0.0));
}

TEST_F(ObliqueResliceRendererTest, SmallVolume) {
    auto imageData = vtkSmartPointer<vtkImageData>::New();
    imageData->SetDimensions(4, 4, 4);
    imageData->SetSpacing(1.0, 1.0, 1.0);
    imageData->SetOrigin(0.0, 0.0, 0.0);
    imageData->AllocateScalars(VTK_SHORT, 1);

    EXPECT_NO_THROW(renderer->setInputData(imageData));
    EXPECT_NO_THROW(renderer->setPlaneByRotation(45.0, 45.0, 0.0));
}

// =============================================================================
// Error recovery and boundary tests (Issue #205)
// =============================================================================

TEST_F(ObliqueResliceRendererTest, ReslicePlaneEntirelyOutsideVolume) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Move center far outside the volume extent
    Point3D farCenter(1000.0, 1000.0, 1000.0);
    EXPECT_NO_THROW(renderer->setCenter(farCenter));
    EXPECT_NO_THROW(renderer->update());

    // Set plane by normal pointing away from volume
    Vector3D normal(0.0, 0.0, 1.0);
    EXPECT_NO_THROW(renderer->setPlaneByNormal(normal, farCenter));
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(ObliqueResliceRendererTest, ResliceAtVolumeCornerMinimalOverlap) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Position at volume corner (origin)
    Point3D corner(0.0, 0.0, 0.0);
    renderer->setCenter(corner);

    // Oblique plane at 45° — only a tiny corner of the volume intersects
    EXPECT_NO_THROW(renderer->setPlaneByRotation(45.0, 45.0, 45.0));
    EXPECT_NO_THROW(renderer->update());

    // Verify plane state is valid
    auto plane = renderer->getCurrentPlane();
    EXPECT_NEAR(plane.center.x, 0.0, 0.1);
    EXPECT_NEAR(plane.center.y, 0.0, 0.1);
    EXPECT_NEAR(plane.center.z, 0.0, 0.1);
}

TEST_F(ObliqueResliceRendererTest, InterpolationModeSwitch) {
    auto volume = createTestVolume(64);
    renderer->setInputData(volume);

    // Test all interpolation modes
    ObliqueResliceOptions opts;

    opts.interpolation = InterpolationMode::NearestNeighbor;
    EXPECT_NO_THROW(renderer->setOptions(opts));
    EXPECT_NO_THROW(renderer->update());
    EXPECT_EQ(renderer->getOptions().interpolation, InterpolationMode::NearestNeighbor);

    opts.interpolation = InterpolationMode::Linear;
    EXPECT_NO_THROW(renderer->setOptions(opts));
    EXPECT_NO_THROW(renderer->update());
    EXPECT_EQ(renderer->getOptions().interpolation, InterpolationMode::Linear);

    opts.interpolation = InterpolationMode::Cubic;
    EXPECT_NO_THROW(renderer->setOptions(opts));
    EXPECT_NO_THROW(renderer->update());
    EXPECT_EQ(renderer->getOptions().interpolation, InterpolationMode::Cubic);
}

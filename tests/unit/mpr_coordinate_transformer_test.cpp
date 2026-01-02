#include <gtest/gtest.h>

#include "services/segmentation/mpr_coordinate_transformer.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace dicom_viewer::services;

class MPRCoordinateTransformerTest : public ::testing::Test {
protected:
    void SetUp() override {
        transformer = std::make_unique<MPRCoordinateTransformer>();

        // Create test image data (100x100x50 volume)
        testImage = vtkSmartPointer<vtkImageData>::New();
        testImage->SetDimensions(100, 100, 50);
        testImage->SetSpacing(1.0, 1.0, 2.0);  // Non-isotropic spacing
        testImage->SetOrigin(0.0, 0.0, 0.0);
        testImage->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

        transformer->setImageData(testImage);
    }

    std::unique_ptr<MPRCoordinateTransformer> transformer;
    vtkSmartPointer<vtkImageData> testImage;
};

// ==================== Basic Property Tests ====================

TEST_F(MPRCoordinateTransformerTest, GetDimensions) {
    auto dims = transformer->getDimensions();
    EXPECT_EQ(dims[0], 100);
    EXPECT_EQ(dims[1], 100);
    EXPECT_EQ(dims[2], 50);
}

TEST_F(MPRCoordinateTransformerTest, GetSpacing) {
    auto spacing = transformer->getSpacing();
    EXPECT_DOUBLE_EQ(spacing[0], 1.0);
    EXPECT_DOUBLE_EQ(spacing[1], 1.0);
    EXPECT_DOUBLE_EQ(spacing[2], 2.0);
}

TEST_F(MPRCoordinateTransformerTest, GetOrigin) {
    auto origin = transformer->getOrigin();
    EXPECT_DOUBLE_EQ(origin[0], 0.0);
    EXPECT_DOUBLE_EQ(origin[1], 0.0);
    EXPECT_DOUBLE_EQ(origin[2], 0.0);
}

// ==================== World to Index Conversion ====================

TEST_F(MPRCoordinateTransformerTest, WorldToIndex_Origin) {
    auto index = transformer->worldToIndex(0.0, 0.0, 0.0);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index->x, 0);
    EXPECT_EQ(index->y, 0);
    EXPECT_EQ(index->z, 0);
}

TEST_F(MPRCoordinateTransformerTest, WorldToIndex_Center) {
    // World position (50, 50, 50) should be index (50, 50, 25) due to spacing
    auto index = transformer->worldToIndex(50.0, 50.0, 50.0);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index->x, 50);
    EXPECT_EQ(index->y, 50);
    EXPECT_EQ(index->z, 25);  // 50.0 / 2.0 = 25
}

TEST_F(MPRCoordinateTransformerTest, WorldToIndex_OutOfBounds) {
    // Negative position should be out of bounds
    auto index = transformer->worldToIndex(-1.0, 0.0, 0.0);
    EXPECT_FALSE(index.has_value());

    // Beyond dimensions should be out of bounds
    index = transformer->worldToIndex(100.0, 0.0, 0.0);  // Exactly at bounds
    EXPECT_FALSE(index.has_value());
}

TEST_F(MPRCoordinateTransformerTest, WorldToIndex_WithNonZeroOrigin) {
    // Set non-zero origin
    testImage->SetOrigin(10.0, 20.0, 30.0);
    transformer->setImageData(testImage);

    // World position (10, 20, 30) should map to index (0, 0, 0)
    auto index = transformer->worldToIndex(10.0, 20.0, 30.0);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index->x, 0);
    EXPECT_EQ(index->y, 0);
    EXPECT_EQ(index->z, 0);

    // World position (20, 30, 50) should map to index (10, 10, 10)
    index = transformer->worldToIndex(20.0, 30.0, 50.0);
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index->x, 10);
    EXPECT_EQ(index->y, 10);
    EXPECT_EQ(index->z, 10);
}

// ==================== Index to World Conversion ====================

TEST_F(MPRCoordinateTransformerTest, IndexToWorld_Origin) {
    auto world = transformer->indexToWorld({0, 0, 0});
    EXPECT_DOUBLE_EQ(world.x, 0.0);
    EXPECT_DOUBLE_EQ(world.y, 0.0);
    EXPECT_DOUBLE_EQ(world.z, 0.0);
}

TEST_F(MPRCoordinateTransformerTest, IndexToWorld_WithSpacing) {
    auto world = transformer->indexToWorld({10, 20, 5});
    EXPECT_DOUBLE_EQ(world.x, 10.0);  // 10 * 1.0
    EXPECT_DOUBLE_EQ(world.y, 20.0);  // 20 * 1.0
    EXPECT_DOUBLE_EQ(world.z, 10.0);  // 5 * 2.0
}

TEST_F(MPRCoordinateTransformerTest, IndexToWorld_RoundTrip) {
    MPRCoordinateTransformer::Index3D originalIndex{25, 30, 15};
    auto world = transformer->indexToWorld(originalIndex);
    auto recoveredIndex = transformer->worldToIndex(world.x, world.y, world.z);

    ASSERT_TRUE(recoveredIndex.has_value());
    EXPECT_EQ(recoveredIndex->x, originalIndex.x);
    EXPECT_EQ(recoveredIndex->y, originalIndex.y);
    EXPECT_EQ(recoveredIndex->z, originalIndex.z);
}

// ==================== Plane Coordinate to Index Conversion ====================

TEST_F(MPRCoordinateTransformerTest, PlaneCoordToIndex_Axial) {
    // Axial: X maps to X, Y maps to Y, slice is Z
    double slicePos = 20.0;  // World Z position = 20.0 -> index Z = 10
    auto index = transformer->planeCoordToIndex(MPRPlane::Axial, 50, 50, slicePos);

    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index->x, 50);
    EXPECT_EQ(index->y, 50);
    EXPECT_EQ(index->z, 10);  // 20.0 / 2.0 = 10
}

TEST_F(MPRCoordinateTransformerTest, PlaneCoordToIndex_Coronal) {
    // Coronal: X maps to X, Y maps to Z, slice is Y
    double slicePos = 30.0;  // World Y position = 30.0 -> index Y = 30
    auto index = transformer->planeCoordToIndex(MPRPlane::Coronal, 40, 20, slicePos);

    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index->x, 40);
    EXPECT_EQ(index->y, 30);  // Slice position
    EXPECT_EQ(index->z, 20);  // View Y maps to volume Z
}

TEST_F(MPRCoordinateTransformerTest, PlaneCoordToIndex_Sagittal) {
    // Sagittal: X maps to Y, Y maps to Z, slice is X
    double slicePos = 25.0;  // World X position = 25.0 -> index X = 25
    auto index = transformer->planeCoordToIndex(MPRPlane::Sagittal, 40, 15, slicePos);

    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index->x, 25);  // Slice position
    EXPECT_EQ(index->y, 40);  // View X maps to volume Y
    EXPECT_EQ(index->z, 15);  // View Y maps to volume Z
}

// ==================== Index to Plane Coordinate Conversion ====================

TEST_F(MPRCoordinateTransformerTest, IndexToPlaneCoord_Axial) {
    MPRCoordinateTransformer::Index3D index{50, 30, 10};
    auto coord = transformer->indexToPlaneCoord(MPRPlane::Axial, index);

    ASSERT_TRUE(coord.has_value());
    EXPECT_EQ(coord->x, 50);  // X -> X
    EXPECT_EQ(coord->y, 30);  // Y -> Y
}

TEST_F(MPRCoordinateTransformerTest, IndexToPlaneCoord_Coronal) {
    MPRCoordinateTransformer::Index3D index{50, 30, 10};
    auto coord = transformer->indexToPlaneCoord(MPRPlane::Coronal, index);

    ASSERT_TRUE(coord.has_value());
    EXPECT_EQ(coord->x, 50);  // X -> X
    EXPECT_EQ(coord->y, 10);  // Z -> Y
}

TEST_F(MPRCoordinateTransformerTest, IndexToPlaneCoord_Sagittal) {
    MPRCoordinateTransformer::Index3D index{50, 30, 10};
    auto coord = transformer->indexToPlaneCoord(MPRPlane::Sagittal, index);

    ASSERT_TRUE(coord.has_value());
    EXPECT_EQ(coord->x, 30);  // Y -> X
    EXPECT_EQ(coord->y, 10);  // Z -> Y
}

// ==================== Slice Index Conversion ====================

TEST_F(MPRCoordinateTransformerTest, WorldPositionToSliceIndex_Axial) {
    // Axial slice at Z world position
    int sliceIdx = transformer->worldPositionToSliceIndex(MPRPlane::Axial, 20.0);
    EXPECT_EQ(sliceIdx, 10);  // 20.0 / 2.0 spacing = 10
}

TEST_F(MPRCoordinateTransformerTest, WorldPositionToSliceIndex_Coronal) {
    // Coronal slice at Y world position
    int sliceIdx = transformer->worldPositionToSliceIndex(MPRPlane::Coronal, 30.0);
    EXPECT_EQ(sliceIdx, 30);  // 30.0 / 1.0 spacing = 30
}

TEST_F(MPRCoordinateTransformerTest, WorldPositionToSliceIndex_Sagittal) {
    // Sagittal slice at X world position
    int sliceIdx = transformer->worldPositionToSliceIndex(MPRPlane::Sagittal, 50.0);
    EXPECT_EQ(sliceIdx, 50);  // 50.0 / 1.0 spacing = 50
}

TEST_F(MPRCoordinateTransformerTest, SliceIndexToWorldPosition_RoundTrip) {
    for (auto plane : {MPRPlane::Axial, MPRPlane::Coronal, MPRPlane::Sagittal}) {
        int originalSlice = 25;
        double worldPos = transformer->sliceIndexToWorldPosition(plane, originalSlice);
        int recoveredSlice = transformer->worldPositionToSliceIndex(plane, worldPos);

        EXPECT_EQ(recoveredSlice, originalSlice)
            << "Round trip failed for plane " << static_cast<int>(plane);
    }
}

// ==================== Segmentation Coordinate Transform ====================

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_Axial) {
    double slicePos = 40.0;  // Z = 40 -> index Z = 20
    auto coords = transformer->transformForSegmentation(
        MPRPlane::Axial, 30, 40, slicePos);

    ASSERT_TRUE(coords.has_value());
    EXPECT_EQ(coords->point2D.x, 30);
    EXPECT_EQ(coords->point2D.y, 40);
    EXPECT_EQ(coords->sliceIndex, 20);
    EXPECT_EQ(coords->index3D.x, 30);
    EXPECT_EQ(coords->index3D.y, 40);
    EXPECT_EQ(coords->index3D.z, 20);
}

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_Coronal) {
    double slicePos = 50.0;  // Y = 50 -> index Y = 50
    auto coords = transformer->transformForSegmentation(
        MPRPlane::Coronal, 30, 15, slicePos);

    ASSERT_TRUE(coords.has_value());
    // For coronal view, the controller uses X, Z as 2D coords
    EXPECT_EQ(coords->point2D.x, 30);
    EXPECT_EQ(coords->point2D.y, 15);
    EXPECT_EQ(coords->sliceIndex, 50);  // Y slice
}

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_Sagittal) {
    double slicePos = 25.0;  // X = 25 -> index X = 25
    auto coords = transformer->transformForSegmentation(
        MPRPlane::Sagittal, 40, 10, slicePos);

    ASSERT_TRUE(coords.has_value());
    // For sagittal view, the controller uses Y, Z as 2D coords
    EXPECT_EQ(coords->point2D.x, 40);
    EXPECT_EQ(coords->point2D.y, 10);
    EXPECT_EQ(coords->sliceIndex, 25);  // X slice
}

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_OutOfBounds) {
    auto coords = transformer->transformForSegmentation(
        MPRPlane::Axial, 150, 50, 0.0);  // X = 150 is out of bounds

    EXPECT_FALSE(coords.has_value());
}

// ==================== Slice Range Tests ====================

TEST_F(MPRCoordinateTransformerTest, GetSliceRange_Axial) {
    auto range = transformer->getSliceRange(MPRPlane::Axial);
    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 49);  // Z dimension - 1
}

TEST_F(MPRCoordinateTransformerTest, GetSliceRange_Coronal) {
    auto range = transformer->getSliceRange(MPRPlane::Coronal);
    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 99);  // Y dimension - 1
}

TEST_F(MPRCoordinateTransformerTest, GetSliceRange_Sagittal) {
    auto range = transformer->getSliceRange(MPRPlane::Sagittal);
    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 99);  // X dimension - 1
}

// ==================== Validity Check Tests ====================

TEST_F(MPRCoordinateTransformerTest, IsValidIndex_Valid) {
    EXPECT_TRUE(transformer->isValidIndex({0, 0, 0}));
    EXPECT_TRUE(transformer->isValidIndex({50, 50, 25}));
    EXPECT_TRUE(transformer->isValidIndex({99, 99, 49}));
}

TEST_F(MPRCoordinateTransformerTest, IsValidIndex_Invalid) {
    EXPECT_FALSE(transformer->isValidIndex({-1, 0, 0}));
    EXPECT_FALSE(transformer->isValidIndex({100, 0, 0}));
    EXPECT_FALSE(transformer->isValidIndex({0, 100, 0}));
    EXPECT_FALSE(transformer->isValidIndex({0, 0, 50}));
}

// ==================== Axis Mapping Tests ====================

TEST_F(MPRCoordinateTransformerTest, GetPlaneAxisMapping_Axial) {
    auto mapping = transformer->getPlaneAxisMapping(MPRPlane::Axial);
    EXPECT_EQ(mapping[0], 0);  // H = X
    EXPECT_EQ(mapping[1], 1);  // V = Y
    EXPECT_EQ(mapping[2], 2);  // Slice = Z
}

TEST_F(MPRCoordinateTransformerTest, GetPlaneAxisMapping_Coronal) {
    auto mapping = transformer->getPlaneAxisMapping(MPRPlane::Coronal);
    EXPECT_EQ(mapping[0], 0);  // H = X
    EXPECT_EQ(mapping[1], 2);  // V = Z
    EXPECT_EQ(mapping[2], 1);  // Slice = Y
}

TEST_F(MPRCoordinateTransformerTest, GetPlaneAxisMapping_Sagittal) {
    auto mapping = transformer->getPlaneAxisMapping(MPRPlane::Sagittal);
    EXPECT_EQ(mapping[0], 1);  // H = Y
    EXPECT_EQ(mapping[1], 2);  // V = Z
    EXPECT_EQ(mapping[2], 0);  // Slice = X
}

// ==================== Edge Cases ====================

TEST_F(MPRCoordinateTransformerTest, NoImageData) {
    MPRCoordinateTransformer emptyTransformer;

    auto dims = emptyTransformer.getDimensions();
    EXPECT_EQ(dims[0], 0);
    EXPECT_EQ(dims[1], 0);
    EXPECT_EQ(dims[2], 0);

    auto index = emptyTransformer.worldToIndex(0, 0, 0);
    EXPECT_FALSE(index.has_value());
}

TEST_F(MPRCoordinateTransformerTest, Index3D_Equality) {
    MPRCoordinateTransformer::Index3D a{1, 2, 3};
    MPRCoordinateTransformer::Index3D b{1, 2, 3};
    MPRCoordinateTransformer::Index3D c{1, 2, 4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST_F(MPRCoordinateTransformerTest, Index3D_IsValid) {
    MPRCoordinateTransformer::Index3D validIndex{0, 0, 0};
    EXPECT_TRUE(validIndex.isValid());

    MPRCoordinateTransformer::Index3D invalidIndex{-1, 0, 0};
    EXPECT_FALSE(invalidIndex.isValid());
}

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

#include "services/coordinate/mpr_coordinate_transformer.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace dicom_viewer::services::coordinate;

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

// ==================== World to Voxel Conversion ====================

TEST_F(MPRCoordinateTransformerTest, WorldToVoxel_Origin) {
    auto voxel = transformer->worldToVoxel(0.0, 0.0, 0.0);
    ASSERT_TRUE(voxel.has_value());
    EXPECT_EQ(voxel->i, 0);
    EXPECT_EQ(voxel->j, 0);
    EXPECT_EQ(voxel->k, 0);
}

TEST_F(MPRCoordinateTransformerTest, WorldToVoxel_Center) {
    // World position (50, 50, 50) should be voxel (50, 50, 25) due to spacing
    auto voxel = transformer->worldToVoxel(50.0, 50.0, 50.0);
    ASSERT_TRUE(voxel.has_value());
    EXPECT_EQ(voxel->i, 50);
    EXPECT_EQ(voxel->j, 50);
    EXPECT_EQ(voxel->k, 25);  // 50.0 / 2.0 = 25
}

TEST_F(MPRCoordinateTransformerTest, WorldToVoxel_OutOfBounds) {
    // Negative position should be out of bounds
    auto voxel = transformer->worldToVoxel(-1.0, 0.0, 0.0);
    EXPECT_FALSE(voxel.has_value());

    // Beyond dimensions should be out of bounds
    voxel = transformer->worldToVoxel(100.0, 0.0, 0.0);  // Exactly at bounds
    EXPECT_FALSE(voxel.has_value());
}

TEST_F(MPRCoordinateTransformerTest, WorldToVoxel_WithNonZeroOrigin) {
    // Set non-zero origin
    testImage->SetOrigin(10.0, 20.0, 30.0);
    transformer->setImageData(testImage);

    // World position (10, 20, 30) should map to voxel (0, 0, 0)
    auto voxel = transformer->worldToVoxel(10.0, 20.0, 30.0);
    ASSERT_TRUE(voxel.has_value());
    EXPECT_EQ(voxel->i, 0);
    EXPECT_EQ(voxel->j, 0);
    EXPECT_EQ(voxel->k, 0);

    // World position (20, 30, 50) should map to voxel (10, 10, 10)
    voxel = transformer->worldToVoxel(20.0, 30.0, 50.0);
    ASSERT_TRUE(voxel.has_value());
    EXPECT_EQ(voxel->i, 10);
    EXPECT_EQ(voxel->j, 10);
    EXPECT_EQ(voxel->k, 10);
}

// ==================== Voxel to World Conversion ====================

TEST_F(MPRCoordinateTransformerTest, VoxelToWorld_Origin) {
    auto world = transformer->voxelToWorld({0, 0, 0});
    EXPECT_DOUBLE_EQ(world.x, 0.0);
    EXPECT_DOUBLE_EQ(world.y, 0.0);
    EXPECT_DOUBLE_EQ(world.z, 0.0);
}

TEST_F(MPRCoordinateTransformerTest, VoxelToWorld_WithSpacing) {
    auto world = transformer->voxelToWorld({10, 20, 5});
    EXPECT_DOUBLE_EQ(world.x, 10.0);  // 10 * 1.0
    EXPECT_DOUBLE_EQ(world.y, 20.0);  // 20 * 1.0
    EXPECT_DOUBLE_EQ(world.z, 10.0);  // 5 * 2.0
}

TEST_F(MPRCoordinateTransformerTest, VoxelToWorld_RoundTrip) {
    VoxelIndex originalVoxel{25, 30, 15};
    auto world = transformer->voxelToWorld(originalVoxel);
    auto recoveredVoxel = transformer->worldToVoxel(world.x, world.y, world.z);

    ASSERT_TRUE(recoveredVoxel.has_value());
    EXPECT_EQ(recoveredVoxel->i, originalVoxel.i);
    EXPECT_EQ(recoveredVoxel->j, originalVoxel.j);
    EXPECT_EQ(recoveredVoxel->k, originalVoxel.k);
}

// ==================== Plane Coordinate to Voxel Conversion ====================

TEST_F(MPRCoordinateTransformerTest, PlaneCoordToVoxel_Axial) {
    // Axial: X maps to X, Y maps to Y, slice is Z
    double slicePos = 20.0;  // World Z position = 20.0 -> voxel Z = 10
    auto voxel = transformer->planeCoordToVoxel(dicom_viewer::services::MPRPlane::Axial, 50, 50, slicePos);

    ASSERT_TRUE(voxel.has_value());
    EXPECT_EQ(voxel->i, 50);
    EXPECT_EQ(voxel->j, 50);
    EXPECT_EQ(voxel->k, 10);  // 20.0 / 2.0 = 10
}

TEST_F(MPRCoordinateTransformerTest, PlaneCoordToVoxel_Coronal) {
    // Coronal: X maps to X, Y maps to Z, slice is Y
    double slicePos = 30.0;  // World Y position = 30.0 -> voxel Y = 30
    auto voxel = transformer->planeCoordToVoxel(dicom_viewer::services::MPRPlane::Coronal, 40, 20, slicePos);

    ASSERT_TRUE(voxel.has_value());
    EXPECT_EQ(voxel->i, 40);
    EXPECT_EQ(voxel->j, 30);  // Slice position
    EXPECT_EQ(voxel->k, 20);  // View Y maps to volume Z
}

TEST_F(MPRCoordinateTransformerTest, PlaneCoordToVoxel_Sagittal) {
    // Sagittal: X maps to Y, Y maps to Z, slice is X
    double slicePos = 25.0;  // World X position = 25.0 -> voxel X = 25
    auto voxel = transformer->planeCoordToVoxel(dicom_viewer::services::MPRPlane::Sagittal, 40, 15, slicePos);

    ASSERT_TRUE(voxel.has_value());
    EXPECT_EQ(voxel->i, 25);  // Slice position
    EXPECT_EQ(voxel->j, 40);  // View X maps to volume Y
    EXPECT_EQ(voxel->k, 15);  // View Y maps to volume Z
}

// ==================== Voxel to Plane Coordinate Conversion ====================

TEST_F(MPRCoordinateTransformerTest, VoxelToPlaneCoord_Axial) {
    VoxelIndex voxel{50, 30, 10};
    auto coord = transformer->voxelToPlaneCoord(dicom_viewer::services::MPRPlane::Axial, voxel);

    ASSERT_TRUE(coord.has_value());
    EXPECT_EQ(coord->x, 50);  // X -> X
    EXPECT_EQ(coord->y, 30);  // Y -> Y
}

TEST_F(MPRCoordinateTransformerTest, VoxelToPlaneCoord_Coronal) {
    VoxelIndex voxel{50, 30, 10};
    auto coord = transformer->voxelToPlaneCoord(dicom_viewer::services::MPRPlane::Coronal, voxel);

    ASSERT_TRUE(coord.has_value());
    EXPECT_EQ(coord->x, 50);  // X -> X
    EXPECT_EQ(coord->y, 10);  // Z -> Y
}

TEST_F(MPRCoordinateTransformerTest, VoxelToPlaneCoord_Sagittal) {
    VoxelIndex voxel{50, 30, 10};
    auto coord = transformer->voxelToPlaneCoord(dicom_viewer::services::MPRPlane::Sagittal, voxel);

    ASSERT_TRUE(coord.has_value());
    EXPECT_EQ(coord->x, 30);  // Y -> X
    EXPECT_EQ(coord->y, 10);  // Z -> Y
}

// ==================== Slice Index Conversion ====================

TEST_F(MPRCoordinateTransformerTest, GetSliceIndex_Axial) {
    // Axial slice at Z world position
    int sliceIdx = transformer->getSliceIndex(dicom_viewer::services::MPRPlane::Axial, 20.0);
    EXPECT_EQ(sliceIdx, 10);  // 20.0 / 2.0 spacing = 10
}

TEST_F(MPRCoordinateTransformerTest, GetSliceIndex_Coronal) {
    // Coronal slice at Y world position
    int sliceIdx = transformer->getSliceIndex(dicom_viewer::services::MPRPlane::Coronal, 30.0);
    EXPECT_EQ(sliceIdx, 30);  // 30.0 / 1.0 spacing = 30
}

TEST_F(MPRCoordinateTransformerTest, GetSliceIndex_Sagittal) {
    // Sagittal slice at X world position
    int sliceIdx = transformer->getSliceIndex(dicom_viewer::services::MPRPlane::Sagittal, 50.0);
    EXPECT_EQ(sliceIdx, 50);  // 50.0 / 1.0 spacing = 50
}

TEST_F(MPRCoordinateTransformerTest, GetWorldPosition_RoundTrip) {
    for (auto plane : {dicom_viewer::services::MPRPlane::Axial,
                       dicom_viewer::services::MPRPlane::Coronal,
                       dicom_viewer::services::MPRPlane::Sagittal}) {
        int originalSlice = 25;
        double worldPos = transformer->getWorldPosition(plane, originalSlice);
        int recoveredSlice = transformer->getSliceIndex(plane, worldPos);

        EXPECT_EQ(recoveredSlice, originalSlice)
            << "Round trip failed for plane " << static_cast<int>(plane);
    }
}

// ==================== Segmentation Coordinate Transform ====================

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_Axial) {
    double slicePos = 40.0;  // Z = 40 -> voxel Z = 20
    auto coords = transformer->transformForSegmentation(
        dicom_viewer::services::MPRPlane::Axial, 30, 40, slicePos);

    ASSERT_TRUE(coords.has_value());
    EXPECT_EQ(coords->point2D.x, 30);
    EXPECT_EQ(coords->point2D.y, 40);
    EXPECT_EQ(coords->sliceIndex, 20);
    EXPECT_EQ(coords->index3D.i, 30);
    EXPECT_EQ(coords->index3D.j, 40);
    EXPECT_EQ(coords->index3D.k, 20);
}

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_Coronal) {
    double slicePos = 50.0;  // Y = 50 -> voxel Y = 50
    auto coords = transformer->transformForSegmentation(
        dicom_viewer::services::MPRPlane::Coronal, 30, 15, slicePos);

    ASSERT_TRUE(coords.has_value());
    // For coronal view, the controller uses X, Z as 2D coords
    EXPECT_EQ(coords->point2D.x, 30);
    EXPECT_EQ(coords->point2D.y, 15);
    EXPECT_EQ(coords->sliceIndex, 50);  // Y slice
}

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_Sagittal) {
    double slicePos = 25.0;  // X = 25 -> voxel X = 25
    auto coords = transformer->transformForSegmentation(
        dicom_viewer::services::MPRPlane::Sagittal, 40, 10, slicePos);

    ASSERT_TRUE(coords.has_value());
    // For sagittal view, the controller uses Y, Z as 2D coords
    EXPECT_EQ(coords->point2D.x, 40);
    EXPECT_EQ(coords->point2D.y, 10);
    EXPECT_EQ(coords->sliceIndex, 25);  // X slice
}

TEST_F(MPRCoordinateTransformerTest, TransformForSegmentation_OutOfBounds) {
    auto coords = transformer->transformForSegmentation(
        dicom_viewer::services::MPRPlane::Axial, 150, 50, 0.0);  // X = 150 is out of bounds

    EXPECT_FALSE(coords.has_value());
}

// ==================== Slice Range Tests ====================

TEST_F(MPRCoordinateTransformerTest, GetSliceRange_Axial) {
    auto range = transformer->getSliceRange(dicom_viewer::services::MPRPlane::Axial);
    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 49);  // Z dimension - 1
}

TEST_F(MPRCoordinateTransformerTest, GetSliceRange_Coronal) {
    auto range = transformer->getSliceRange(dicom_viewer::services::MPRPlane::Coronal);
    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 99);  // Y dimension - 1
}

TEST_F(MPRCoordinateTransformerTest, GetSliceRange_Sagittal) {
    auto range = transformer->getSliceRange(dicom_viewer::services::MPRPlane::Sagittal);
    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 99);  // X dimension - 1
}

// ==================== Validity Check Tests ====================

TEST_F(MPRCoordinateTransformerTest, IsValidVoxel_Valid) {
    EXPECT_TRUE(transformer->isValidVoxel({0, 0, 0}));
    EXPECT_TRUE(transformer->isValidVoxel({50, 50, 25}));
    EXPECT_TRUE(transformer->isValidVoxel({99, 99, 49}));
}

TEST_F(MPRCoordinateTransformerTest, IsValidVoxel_Invalid) {
    EXPECT_FALSE(transformer->isValidVoxel({-1, 0, 0}));
    EXPECT_FALSE(transformer->isValidVoxel({100, 0, 0}));
    EXPECT_FALSE(transformer->isValidVoxel({0, 100, 0}));
    EXPECT_FALSE(transformer->isValidVoxel({0, 0, 50}));
}

// ==================== Axis Mapping Tests ====================

TEST_F(MPRCoordinateTransformerTest, GetPlaneAxisMapping_Axial) {
    auto mapping = transformer->getPlaneAxisMapping(dicom_viewer::services::MPRPlane::Axial);
    EXPECT_EQ(mapping[0], 0);  // H = X
    EXPECT_EQ(mapping[1], 1);  // V = Y
    EXPECT_EQ(mapping[2], 2);  // Slice = Z
}

TEST_F(MPRCoordinateTransformerTest, GetPlaneAxisMapping_Coronal) {
    auto mapping = transformer->getPlaneAxisMapping(dicom_viewer::services::MPRPlane::Coronal);
    EXPECT_EQ(mapping[0], 0);  // H = X
    EXPECT_EQ(mapping[1], 2);  // V = Z
    EXPECT_EQ(mapping[2], 1);  // Slice = Y
}

TEST_F(MPRCoordinateTransformerTest, GetPlaneAxisMapping_Sagittal) {
    auto mapping = transformer->getPlaneAxisMapping(dicom_viewer::services::MPRPlane::Sagittal);
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

    auto voxel = emptyTransformer.worldToVoxel(0, 0, 0);
    EXPECT_FALSE(voxel.has_value());
}

TEST_F(MPRCoordinateTransformerTest, VoxelIndex_Equality) {
    VoxelIndex a{1, 2, 3};
    VoxelIndex b{1, 2, 3};
    VoxelIndex c{1, 2, 4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST_F(MPRCoordinateTransformerTest, VoxelIndex_IsValid) {
    VoxelIndex validVoxel{0, 0, 0};
    EXPECT_TRUE(validVoxel.isValid());

    VoxelIndex invalidVoxel{-1, 0, 0};
    EXPECT_FALSE(invalidVoxel.isValid());
}

#include <gtest/gtest.h>

#include "services/segmentation/mask_boolean_operations.hpp"

using namespace dicom_viewer::services;
using LabelMapType = MaskBooleanOperations::LabelMapType;

namespace {

/**
 * @brief Create a label map filled with zeros
 */
LabelMapType::Pointer createEmptyMap(int nx, int ny, int nz) {
    auto map = LabelMapType::New();
    LabelMapType::RegionType region;
    LabelMapType::SizeType size;
    size[0] = nx; size[1] = ny; size[2] = nz;
    region.SetSize(size);
    map->SetRegions(region);

    LabelMapType::SpacingType spacing;
    spacing[0] = 1.0; spacing[1] = 1.0; spacing[2] = 1.0;
    map->SetSpacing(spacing);

    map->Allocate(true);
    return map;
}

/**
 * @brief Count non-zero voxels in a label map
 */
int countNonZero(LabelMapType::Pointer map) {
    auto* buf = map->GetBufferPointer();
    auto size = map->GetLargestPossibleRegion().GetSize();
    size_t total = size[0] * size[1] * size[2];
    int count = 0;
    for (size_t i = 0; i < total; ++i) {
        if (buf[i] != 0) ++count;
    }
    return count;
}

/**
 * @brief Count voxels with a specific label
 */
int countLabel(LabelMapType::Pointer map, uint8_t label) {
    auto* buf = map->GetBufferPointer();
    auto size = map->GetLargestPossibleRegion().GetSize();
    size_t total = size[0] * size[1] * size[2];
    int count = 0;
    for (size_t i = 0; i < total; ++i) {
        if (buf[i] == label) ++count;
    }
    return count;
}

}  // namespace

// =============================================================================
// Validation tests
// =============================================================================

TEST(MaskBooleanOperations, NullInputReturnsError) {
    auto mapA = createEmptyMap(10, 10, 1);

    auto r1 = MaskBooleanOperations::computeUnion(nullptr, mapA);
    EXPECT_FALSE(r1.has_value());

    auto r2 = MaskBooleanOperations::computeUnion(mapA, nullptr);
    EXPECT_FALSE(r2.has_value());

    auto r3 = MaskBooleanOperations::computeDifference(nullptr, nullptr);
    EXPECT_FALSE(r3.has_value());
}

TEST(MaskBooleanOperations, DimensionMismatchReturnsError) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(20, 10, 1);

    auto result = MaskBooleanOperations::computeUnion(mapA, mapB);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("Dimension mismatch"), std::string::npos);
}

TEST(MaskBooleanOperations, SpacingMismatchReturnsError) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);

    LabelMapType::SpacingType differentSpacing;
    differentSpacing[0] = 2.0; differentSpacing[1] = 1.0; differentSpacing[2] = 1.0;
    mapB->SetSpacing(differentSpacing);

    auto result = MaskBooleanOperations::computeIntersection(mapA, mapB);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("Spacing mismatch"), std::string::npos);
}

// =============================================================================
// Union tests
// =============================================================================

TEST(MaskBooleanOperations, UnionNonOverlapping) {
    // A: left half labeled 1, B: right half labeled 2
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);
    auto* bufA = mapA->GetBufferPointer();
    auto* bufB = mapB->GetBufferPointer();

    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 5; ++x) {
            bufA[y * 10 + x] = 1;
        }
        for (int x = 5; x < 10; ++x) {
            bufB[y * 10 + x] = 2;
        }
    }

    auto result = MaskBooleanOperations::computeUnion(mapA, mapB);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(countNonZero(*result), 100);
    EXPECT_EQ(countLabel(*result, 1), 50);
    EXPECT_EQ(countLabel(*result, 2), 50);
}

TEST(MaskBooleanOperations, UnionOverlappingAPriority) {
    // Both A and B cover entire map with different labels
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);
    auto* bufA = mapA->GetBufferPointer();
    auto* bufB = mapB->GetBufferPointer();

    for (int i = 0; i < 100; ++i) {
        bufA[i] = 1;
        bufB[i] = 2;
    }

    auto result = MaskBooleanOperations::computeUnion(mapA, mapB);
    ASSERT_TRUE(result.has_value());

    // A takes priority → all should be label 1
    EXPECT_EQ(countLabel(*result, 1), 100);
    EXPECT_EQ(countLabel(*result, 2), 0);
}

TEST(MaskBooleanOperations, UnionBothEmpty) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);

    auto result = MaskBooleanOperations::computeUnion(mapA, mapB);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(countNonZero(*result), 0);
}

TEST(MaskBooleanOperations, UnionPreservesOriginals) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);
    mapA->GetBufferPointer()[0] = 1;

    auto result = MaskBooleanOperations::computeUnion(mapA, mapB);
    ASSERT_TRUE(result.has_value());

    // Original maps unmodified
    EXPECT_EQ(mapA->GetBufferPointer()[0], 1);
    EXPECT_EQ(mapB->GetBufferPointer()[0], 0);
    // Result is a different pointer
    EXPECT_NE(result->GetPointer(), mapA.GetPointer());
}

// =============================================================================
// Difference tests
// =============================================================================

TEST(MaskBooleanOperations, DifferenceRemovesOverlap) {
    // A: entire row y=0 labeled 1
    // B: first 5 columns labeled 2
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);
    auto* bufA = mapA->GetBufferPointer();
    auto* bufB = mapB->GetBufferPointer();

    for (int x = 0; x < 10; ++x) {
        bufA[x] = 1;  // y=0, all x
    }
    for (int x = 0; x < 5; ++x) {
        bufB[x] = 2;  // y=0, x=0..4
    }

    auto result = MaskBooleanOperations::computeDifference(mapA, mapB);
    ASSERT_TRUE(result.has_value());

    auto* bufOut = result->GetPointer()->GetBufferPointer();

    // x=0..4 removed (overlap), x=5..9 remain
    for (int x = 0; x < 5; ++x) {
        EXPECT_EQ(bufOut[x], 0) << "Overlapping voxel at x=" << x << " should be removed";
    }
    for (int x = 5; x < 10; ++x) {
        EXPECT_EQ(bufOut[x], 1) << "Non-overlapping voxel at x=" << x << " should remain";
    }
}

TEST(MaskBooleanOperations, DifferenceNoOverlap) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);

    // A: left, B: right → no overlap
    for (int i = 0; i < 50; ++i) mapA->GetBufferPointer()[i] = 1;
    for (int i = 50; i < 100; ++i) mapB->GetBufferPointer()[i] = 2;

    auto result = MaskBooleanOperations::computeDifference(mapA, mapB);
    ASSERT_TRUE(result.has_value());

    // A preserved entirely since no overlap
    EXPECT_EQ(countLabel(*result, 1), 50);
}

TEST(MaskBooleanOperations, DifferenceCompleteSubtraction) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);

    // Both cover all voxels
    for (int i = 0; i < 100; ++i) {
        mapA->GetBufferPointer()[i] = 1;
        mapB->GetBufferPointer()[i] = 2;
    }

    auto result = MaskBooleanOperations::computeDifference(mapA, mapB);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(countNonZero(*result), 0)
        << "Complete overlap should produce empty result";
}

// =============================================================================
// Intersection tests
// =============================================================================

TEST(MaskBooleanOperations, IntersectionKeepsOverlapOnly) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);
    auto* bufA = mapA->GetBufferPointer();
    auto* bufB = mapB->GetBufferPointer();

    // A: x=0..6, B: x=3..9 → overlap x=3..6
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x <= 6; ++x) bufA[y * 10 + x] = 1;
        for (int x = 3; x <= 9; ++x) bufB[y * 10 + x] = 2;
    }

    auto result = MaskBooleanOperations::computeIntersection(mapA, mapB);
    ASSERT_TRUE(result.has_value());

    // Overlap: x=3..6 → 4 columns × 10 rows = 40 voxels
    EXPECT_EQ(countNonZero(*result), 40);
    // Label from A
    EXPECT_EQ(countLabel(*result, 1), 40);
}

TEST(MaskBooleanOperations, IntersectionNoOverlapProducesEmpty) {
    auto mapA = createEmptyMap(10, 10, 1);
    auto mapB = createEmptyMap(10, 10, 1);

    for (int i = 0; i < 50; ++i) mapA->GetBufferPointer()[i] = 1;
    for (int i = 50; i < 100; ++i) mapB->GetBufferPointer()[i] = 2;

    auto result = MaskBooleanOperations::computeIntersection(mapA, mapB);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(countNonZero(*result), 0);
}

// =============================================================================
// Multi-mask union tests
// =============================================================================

TEST(MaskBooleanOperations, UnionMultipleTooFewReturnsError) {
    std::vector<LabelMapType::Pointer> masks;
    masks.push_back(createEmptyMap(10, 10, 1));

    auto result = MaskBooleanOperations::computeUnionMultiple(masks);
    EXPECT_FALSE(result.has_value());
}

TEST(MaskBooleanOperations, UnionMultipleThreeMasks) {
    auto m1 = createEmptyMap(10, 1, 1);
    auto m2 = createEmptyMap(10, 1, 1);
    auto m3 = createEmptyMap(10, 1, 1);

    // m1: x=0..2, m2: x=3..5, m3: x=6..8
    for (int x = 0; x <= 2; ++x) m1->GetBufferPointer()[x] = 1;
    for (int x = 3; x <= 5; ++x) m2->GetBufferPointer()[x] = 2;
    for (int x = 6; x <= 8; ++x) m3->GetBufferPointer()[x] = 3;

    auto result = MaskBooleanOperations::computeUnionMultiple({m1, m2, m3});
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(countNonZero(*result), 9);
    EXPECT_EQ(countLabel(*result, 1), 3);
    EXPECT_EQ(countLabel(*result, 2), 3);
    EXPECT_EQ(countLabel(*result, 3), 3);
}

// =============================================================================
// 3D volume test
// =============================================================================

TEST(MaskBooleanOperations, ThreeDimensionalVolume) {
    // Test with an actual 3D volume
    auto mapA = createEmptyMap(10, 10, 10);
    auto mapB = createEmptyMap(10, 10, 10);

    // A: slice 0-4 filled, B: slice 3-7 filled
    auto* bufA = mapA->GetBufferPointer();
    auto* bufB = mapB->GetBufferPointer();
    int sliceSize = 10 * 10;

    for (int z = 0; z < 5; ++z) {
        for (int i = 0; i < sliceSize; ++i) {
            bufA[z * sliceSize + i] = 1;
        }
    }
    for (int z = 3; z < 8; ++z) {
        for (int i = 0; i < sliceSize; ++i) {
            bufB[z * sliceSize + i] = 2;
        }
    }

    // Union: slices 0-7 = 800 voxels
    auto unionResult = MaskBooleanOperations::computeUnion(mapA, mapB);
    ASSERT_TRUE(unionResult.has_value());
    EXPECT_EQ(countNonZero(*unionResult), 800);

    // Intersection: slices 3-4 = 200 voxels
    auto interResult = MaskBooleanOperations::computeIntersection(mapA, mapB);
    ASSERT_TRUE(interResult.has_value());
    EXPECT_EQ(countNonZero(*interResult), 200);

    // Difference A\B: slices 0-2 = 300 voxels
    auto diffResult = MaskBooleanOperations::computeDifference(mapA, mapB);
    ASSERT_TRUE(diffResult.has_value());
    EXPECT_EQ(countNonZero(*diffResult), 300);
}

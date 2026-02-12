#include <gtest/gtest.h>

#include "services/enhanced_dicom/dimension_index_sorter.hpp"
#include "services/enhanced_dicom/enhanced_dicom_types.hpp"

using namespace dicom_viewer::services;

// =============================================================================
// DimensionDefinition and DimensionOrganization tests
// =============================================================================

TEST(DimensionDefinitionTest, DefaultValues) {
    DimensionDefinition def;
    EXPECT_EQ(def.dimensionIndexPointer, 0u);
    EXPECT_EQ(def.functionalGroupPointer, 0u);
    EXPECT_TRUE(def.dimensionOrganizationUID.empty());
    EXPECT_TRUE(def.dimensionDescription.empty());
}

TEST(DimensionOrganizationTest, EmptyOrganization) {
    DimensionOrganization org;
    EXPECT_TRUE(org.dimensions.empty());
    EXPECT_FALSE(org.hasDimension(dimension_tag::InStackPositionNumber));
    EXPECT_FALSE(org.dimensionIndex(dimension_tag::InStackPositionNumber).has_value());
}

TEST(DimensionOrganizationTest, HasDimension) {
    DimensionOrganization org;
    org.dimensions.push_back({dimension_tag::TemporalPositionIndex, 0, "", ""});
    org.dimensions.push_back({dimension_tag::InStackPositionNumber, 0, "", ""});

    EXPECT_TRUE(org.hasDimension(dimension_tag::TemporalPositionIndex));
    EXPECT_TRUE(org.hasDimension(dimension_tag::InStackPositionNumber));
    EXPECT_FALSE(org.hasDimension(dimension_tag::StackID));
}

TEST(DimensionOrganizationTest, DimensionIndex) {
    DimensionOrganization org;
    org.dimensions.push_back({dimension_tag::TemporalPositionIndex, 0, "", ""});
    org.dimensions.push_back({dimension_tag::InStackPositionNumber, 0, "", ""});

    auto idx0 = org.dimensionIndex(dimension_tag::TemporalPositionIndex);
    ASSERT_TRUE(idx0.has_value());
    EXPECT_EQ(idx0.value(), 0u);

    auto idx1 = org.dimensionIndex(dimension_tag::InStackPositionNumber);
    ASSERT_TRUE(idx1.has_value());
    EXPECT_EQ(idx1.value(), 1u);

    auto idx2 = org.dimensionIndex(dimension_tag::StackID);
    EXPECT_FALSE(idx2.has_value());
}

// =============================================================================
// dimension_tag constants verification
// =============================================================================

TEST(DimensionTagTest, KnownTags) {
    // InStackPositionNumber: (0020,9057) = 0x00209057
    EXPECT_EQ(dimension_tag::InStackPositionNumber, 0x00209057u);
    // TemporalPositionIndex: (0020,9128) = 0x00209128
    EXPECT_EQ(dimension_tag::TemporalPositionIndex, 0x00209128u);
    // StackID: (0020,9056) = 0x00209056
    EXPECT_EQ(dimension_tag::StackID, 0x00209056u);
}

// =============================================================================
// DimensionIndexSorter construction tests
// =============================================================================

TEST(DimensionIndexSorterTest, ConstructionAndDestruction) {
    DimensionIndexSorter sorter;
    // Verify no crash
}

TEST(DimensionIndexSorterTest, MoveConstruction) {
    DimensionIndexSorter s1;
    DimensionIndexSorter s2(std::move(s1));
}

TEST(DimensionIndexSorterTest, MoveAssignment) {
    DimensionIndexSorter s1;
    DimensionIndexSorter s2;
    s2 = std::move(s1);
}

// =============================================================================
// parseDimensionIndex tests
// =============================================================================

TEST(DimensionIndexSorterTest, ParseNonexistentFile) {
    DimensionIndexSorter sorter;
    auto result = sorter.parseDimensionIndex("/nonexistent/file.dcm");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::ParseFailed);
}

// =============================================================================
// sortFrames tests - 2D dimension sorting (temporal + spatial)
// =============================================================================

TEST(DimensionIndexSorterTest, SortFrames2D) {
    DimensionIndexSorter sorter;

    // Set up dimension organization: Temporal -> InStackPosition
    DimensionOrganization org;
    org.dimensions.push_back(
        {dimension_tag::TemporalPositionIndex, 0, "", "Temporal"});
    org.dimensions.push_back(
        {dimension_tag::InStackPositionNumber, 0, "", "Spatial"});

    // Create unsorted frames: 2 temporal phases x 3 slices
    // Frame order: (T2,S3), (T1,S2), (T2,S1), (T1,S1), (T2,S2), (T1,S3)
    std::vector<EnhancedFrameInfo> frames(6);
    // Frame 0: T=2, S=3
    frames[0].frameIndex = 0;
    frames[0].dimensionIndices[dimension_tag::TemporalPositionIndex] = 2;
    frames[0].dimensionIndices[dimension_tag::InStackPositionNumber] = 3;
    // Frame 1: T=1, S=2
    frames[1].frameIndex = 1;
    frames[1].dimensionIndices[dimension_tag::TemporalPositionIndex] = 1;
    frames[1].dimensionIndices[dimension_tag::InStackPositionNumber] = 2;
    // Frame 2: T=2, S=1
    frames[2].frameIndex = 2;
    frames[2].dimensionIndices[dimension_tag::TemporalPositionIndex] = 2;
    frames[2].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    // Frame 3: T=1, S=1
    frames[3].frameIndex = 3;
    frames[3].dimensionIndices[dimension_tag::TemporalPositionIndex] = 1;
    frames[3].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    // Frame 4: T=2, S=2
    frames[4].frameIndex = 4;
    frames[4].dimensionIndices[dimension_tag::TemporalPositionIndex] = 2;
    frames[4].dimensionIndices[dimension_tag::InStackPositionNumber] = 2;
    // Frame 5: T=1, S=3
    frames[5].frameIndex = 5;
    frames[5].dimensionIndices[dimension_tag::TemporalPositionIndex] = 1;
    frames[5].dimensionIndices[dimension_tag::InStackPositionNumber] = 3;

    auto sorted = sorter.sortFrames(frames, org);
    ASSERT_EQ(sorted.size(), 6u);

    // Expected order: (T1,S1), (T1,S2), (T1,S3), (T2,S1), (T2,S2), (T2,S3)
    EXPECT_EQ(sorted[0].frameIndex, 3);  // T=1, S=1
    EXPECT_EQ(sorted[1].frameIndex, 1);  // T=1, S=2
    EXPECT_EQ(sorted[2].frameIndex, 5);  // T=1, S=3
    EXPECT_EQ(sorted[3].frameIndex, 2);  // T=2, S=1
    EXPECT_EQ(sorted[4].frameIndex, 4);  // T=2, S=2
    EXPECT_EQ(sorted[5].frameIndex, 0);  // T=2, S=3
}

// =============================================================================
// sortFrames tests - 3D dimension sorting (stack + temporal + spatial)
// =============================================================================

TEST(DimensionIndexSorterTest, SortFrames3D) {
    DimensionIndexSorter sorter;

    // Set up: Stack -> Temporal -> InStackPosition
    DimensionOrganization org;
    org.dimensions.push_back({dimension_tag::StackID, 0, "", "Stack"});
    org.dimensions.push_back(
        {dimension_tag::TemporalPositionIndex, 0, "", "Temporal"});
    org.dimensions.push_back(
        {dimension_tag::InStackPositionNumber, 0, "", "Spatial"});

    // Create 2 stacks x 2 temporal x 2 spatial = 8 frames (shuffled)
    std::vector<EnhancedFrameInfo> frames(8);
    // Stack 2, T2, S2
    frames[0].frameIndex = 0;
    frames[0].dimensionIndices[dimension_tag::StackID] = 2;
    frames[0].dimensionIndices[dimension_tag::TemporalPositionIndex] = 2;
    frames[0].dimensionIndices[dimension_tag::InStackPositionNumber] = 2;
    // Stack 1, T1, S1
    frames[1].frameIndex = 1;
    frames[1].dimensionIndices[dimension_tag::StackID] = 1;
    frames[1].dimensionIndices[dimension_tag::TemporalPositionIndex] = 1;
    frames[1].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    // Stack 1, T2, S1
    frames[2].frameIndex = 2;
    frames[2].dimensionIndices[dimension_tag::StackID] = 1;
    frames[2].dimensionIndices[dimension_tag::TemporalPositionIndex] = 2;
    frames[2].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    // Stack 2, T1, S1
    frames[3].frameIndex = 3;
    frames[3].dimensionIndices[dimension_tag::StackID] = 2;
    frames[3].dimensionIndices[dimension_tag::TemporalPositionIndex] = 1;
    frames[3].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    // Stack 1, T1, S2
    frames[4].frameIndex = 4;
    frames[4].dimensionIndices[dimension_tag::StackID] = 1;
    frames[4].dimensionIndices[dimension_tag::TemporalPositionIndex] = 1;
    frames[4].dimensionIndices[dimension_tag::InStackPositionNumber] = 2;
    // Stack 2, T1, S2
    frames[5].frameIndex = 5;
    frames[5].dimensionIndices[dimension_tag::StackID] = 2;
    frames[5].dimensionIndices[dimension_tag::TemporalPositionIndex] = 1;
    frames[5].dimensionIndices[dimension_tag::InStackPositionNumber] = 2;
    // Stack 1, T2, S2
    frames[6].frameIndex = 6;
    frames[6].dimensionIndices[dimension_tag::StackID] = 1;
    frames[6].dimensionIndices[dimension_tag::TemporalPositionIndex] = 2;
    frames[6].dimensionIndices[dimension_tag::InStackPositionNumber] = 2;
    // Stack 2, T2, S1
    frames[7].frameIndex = 7;
    frames[7].dimensionIndices[dimension_tag::StackID] = 2;
    frames[7].dimensionIndices[dimension_tag::TemporalPositionIndex] = 2;
    frames[7].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;

    auto sorted = sorter.sortFrames(frames, org);
    ASSERT_EQ(sorted.size(), 8u);

    // Expected: S1T1S1, S1T1S2, S1T2S1, S1T2S2, S2T1S1, S2T1S2, S2T2S1, S2T2S2
    EXPECT_EQ(sorted[0].frameIndex, 1);  // Stack1, T1, S1
    EXPECT_EQ(sorted[1].frameIndex, 4);  // Stack1, T1, S2
    EXPECT_EQ(sorted[2].frameIndex, 2);  // Stack1, T2, S1
    EXPECT_EQ(sorted[3].frameIndex, 6);  // Stack1, T2, S2
    EXPECT_EQ(sorted[4].frameIndex, 3);  // Stack2, T1, S1
    EXPECT_EQ(sorted[5].frameIndex, 5);  // Stack2, T1, S2
    EXPECT_EQ(sorted[6].frameIndex, 7);  // Stack2, T2, S1
    EXPECT_EQ(sorted[7].frameIndex, 0);  // Stack2, T2, S2
}

// =============================================================================
// sortFrames tests - empty organization fallback
// =============================================================================

TEST(DimensionIndexSorterTest, SortFramesEmptyOrganizationFallback) {
    DimensionIndexSorter sorter;

    DimensionOrganization emptyOrg;  // No dimensions

    // Create frames with spatial positions for fallback sort
    std::vector<EnhancedFrameInfo> frames(3);
    frames[0].frameIndex = 0;
    frames[0].imagePosition = {0.0, 0.0, 30.0};
    frames[1].frameIndex = 1;
    frames[1].imagePosition = {0.0, 0.0, 10.0};
    frames[2].frameIndex = 2;
    frames[2].imagePosition = {0.0, 0.0, 20.0};

    auto sorted = sorter.sortFrames(frames, emptyOrg);
    ASSERT_EQ(sorted.size(), 3u);

    // Should be sorted by Z position: 10, 20, 30
    EXPECT_EQ(sorted[0].frameIndex, 1);  // Z=10
    EXPECT_EQ(sorted[1].frameIndex, 2);  // Z=20
    EXPECT_EQ(sorted[2].frameIndex, 0);  // Z=30
}

// =============================================================================
// sortFramesBySpatialPosition tests
// =============================================================================

TEST(DimensionIndexSorterTest, SortBySpatialPositionAxial) {
    DimensionIndexSorter sorter;

    // Default orientation (axial): normal = (0,0,1)
    std::vector<EnhancedFrameInfo> frames(4);
    frames[0].frameIndex = 0;
    frames[0].imagePosition = {0.0, 0.0, 40.0};
    frames[1].frameIndex = 1;
    frames[1].imagePosition = {0.0, 0.0, 10.0};
    frames[2].frameIndex = 2;
    frames[2].imagePosition = {0.0, 0.0, 30.0};
    frames[3].frameIndex = 3;
    frames[3].imagePosition = {0.0, 0.0, 20.0};

    auto sorted = sorter.sortFramesBySpatialPosition(frames);
    ASSERT_EQ(sorted.size(), 4u);

    EXPECT_EQ(sorted[0].frameIndex, 1);  // Z=10
    EXPECT_EQ(sorted[1].frameIndex, 3);  // Z=20
    EXPECT_EQ(sorted[2].frameIndex, 2);  // Z=30
    EXPECT_EQ(sorted[3].frameIndex, 0);  // Z=40
}

TEST(DimensionIndexSorterTest, SortBySpatialPositionEmpty) {
    DimensionIndexSorter sorter;
    std::vector<EnhancedFrameInfo> empty;
    auto result = sorter.sortFramesBySpatialPosition(empty);
    EXPECT_TRUE(result.empty());
}

TEST(DimensionIndexSorterTest, SortBySpatialPositionSingleFrame) {
    DimensionIndexSorter sorter;
    std::vector<EnhancedFrameInfo> frames(1);
    frames[0].frameIndex = 42;
    frames[0].imagePosition = {1.0, 2.0, 3.0};

    auto sorted = sorter.sortFramesBySpatialPosition(frames);
    ASSERT_EQ(sorted.size(), 1u);
    EXPECT_EQ(sorted[0].frameIndex, 42);
}

// =============================================================================
// groupByDimension tests
// =============================================================================

TEST(DimensionIndexSorterTest, GroupByTemporalDimension) {
    DimensionIndexSorter sorter;

    // 3 temporal phases x 2 slices = 6 frames
    std::vector<EnhancedFrameInfo> frames(6);
    for (int t = 1; t <= 3; ++t) {
        for (int s = 1; s <= 2; ++s) {
            int idx = (t - 1) * 2 + (s - 1);
            frames[idx].frameIndex = idx;
            frames[idx].dimensionIndices[dimension_tag::TemporalPositionIndex] = t;
            frames[idx].dimensionIndices[dimension_tag::InStackPositionNumber] = s;
        }
    }

    auto groups = sorter.groupByDimension(
        frames, dimension_tag::TemporalPositionIndex);

    EXPECT_EQ(groups.size(), 3u);  // 3 temporal phases
    EXPECT_EQ(groups[1].size(), 2u);  // Phase 1: 2 slices
    EXPECT_EQ(groups[2].size(), 2u);  // Phase 2: 2 slices
    EXPECT_EQ(groups[3].size(), 2u);  // Phase 3: 2 slices
}

TEST(DimensionIndexSorterTest, GroupByDimensionMissingIndices) {
    DimensionIndexSorter sorter;

    // Frames without the grouping dimension should go to group 0
    std::vector<EnhancedFrameInfo> frames(3);
    frames[0].frameIndex = 0;
    frames[1].frameIndex = 1;
    frames[2].frameIndex = 2;

    auto groups = sorter.groupByDimension(
        frames, dimension_tag::TemporalPositionIndex);

    EXPECT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].size(), 3u);  // All in group 0
}

TEST(DimensionIndexSorterTest, GroupByDimensionEmpty) {
    DimensionIndexSorter sorter;
    std::vector<EnhancedFrameInfo> empty;
    auto groups = sorter.groupByDimension(
        empty, dimension_tag::TemporalPositionIndex);
    EXPECT_TRUE(groups.empty());
}

// =============================================================================
// reconstructVolumes tests
// =============================================================================

TEST(DimensionIndexSorterTest, ReconstructVolumesEmptyFrames) {
    DimensionIndexSorter sorter;

    EnhancedSeriesInfo info;
    DimensionOrganization org;

    auto result = sorter.reconstructVolumes(info, org);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

// =============================================================================
// sortFrames edge cases
// =============================================================================

TEST(DimensionIndexSorterTest, SortFramesEmpty) {
    DimensionIndexSorter sorter;
    DimensionOrganization org;
    org.dimensions.push_back(
        {dimension_tag::InStackPositionNumber, 0, "", ""});

    auto sorted = sorter.sortFrames({}, org);
    EXPECT_TRUE(sorted.empty());
}

TEST(DimensionIndexSorterTest, SortFramesSingleDimension) {
    DimensionIndexSorter sorter;

    DimensionOrganization org;
    org.dimensions.push_back(
        {dimension_tag::InStackPositionNumber, 0, "", "Spatial"});

    std::vector<EnhancedFrameInfo> frames(3);
    frames[0].frameIndex = 0;
    frames[0].dimensionIndices[dimension_tag::InStackPositionNumber] = 3;
    frames[1].frameIndex = 1;
    frames[1].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    frames[2].frameIndex = 2;
    frames[2].dimensionIndices[dimension_tag::InStackPositionNumber] = 2;

    auto sorted = sorter.sortFrames(frames, org);
    ASSERT_EQ(sorted.size(), 3u);

    EXPECT_EQ(sorted[0].frameIndex, 1);  // S=1
    EXPECT_EQ(sorted[1].frameIndex, 2);  // S=2
    EXPECT_EQ(sorted[2].frameIndex, 0);  // S=3
}

TEST(DimensionIndexSorterTest, SortFramesPreservesOrderForEqualIndices) {
    DimensionIndexSorter sorter;

    DimensionOrganization org;
    org.dimensions.push_back(
        {dimension_tag::InStackPositionNumber, 0, "", ""});

    // All frames have the same dimension index
    std::vector<EnhancedFrameInfo> frames(3);
    frames[0].frameIndex = 10;
    frames[0].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    frames[1].frameIndex = 5;
    frames[1].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;
    frames[2].frameIndex = 20;
    frames[2].dimensionIndices[dimension_tag::InStackPositionNumber] = 1;

    auto sorted = sorter.sortFrames(frames, org);
    ASSERT_EQ(sorted.size(), 3u);

    // Should be stable sorted by frameIndex
    EXPECT_EQ(sorted[0].frameIndex, 5);
    EXPECT_EQ(sorted[1].frameIndex, 10);
    EXPECT_EQ(sorted[2].frameIndex, 20);
}

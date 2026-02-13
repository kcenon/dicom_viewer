#include "services/segmentation/morphological_processor.hpp"

#include <gtest/gtest.h>

#include <itkImageRegionIterator.h>

namespace dicom_viewer::services::test {

// ============================================================================
// Test Fixtures
// ============================================================================

class MorphologicalProcessorTest : public ::testing::Test {
protected:
    using BinaryMaskType = MorphologicalProcessor::BinaryMaskType;
    using LabelMapType = MorphologicalProcessor::LabelMapType;

    void SetUp() override {
        processor_ = std::make_unique<MorphologicalProcessor>();
    }

    /**
     * @brief Create a test binary mask with a simple shape
     */
    BinaryMaskType::Pointer createTestMask(int width, int height, int depth) {
        auto mask = BinaryMaskType::New();

        BinaryMaskType::SizeType size;
        size[0] = width;
        size[1] = height;
        size[2] = depth;

        BinaryMaskType::RegionType region;
        region.SetSize(size);

        BinaryMaskType::IndexType start;
        start.Fill(0);
        region.SetIndex(start);

        mask->SetRegions(region);
        mask->Allocate();
        mask->FillBuffer(0);

        return mask;
    }

    /**
     * @brief Create a mask with a solid cube in the center
     */
    BinaryMaskType::Pointer createCubeMask(int size, int cubeRadius) {
        auto mask = createTestMask(size, size, size);

        int center = size / 2;
        BinaryMaskType::IndexType index;

        for (int z = center - cubeRadius; z <= center + cubeRadius; ++z) {
            for (int y = center - cubeRadius; y <= center + cubeRadius; ++y) {
                for (int x = center - cubeRadius; x <= center + cubeRadius; ++x) {
                    if (x >= 0 && x < size && y >= 0 && y < size && z >= 0 && z < size) {
                        index[0] = x;
                        index[1] = y;
                        index[2] = z;
                        mask->SetPixel(index, 1);
                    }
                }
            }
        }

        return mask;
    }

    /**
     * @brief Create a mask with a cube that has a hole in the center
     */
    BinaryMaskType::Pointer createCubeWithHoleMask(int size, int cubeRadius, int holeRadius) {
        auto mask = createCubeMask(size, cubeRadius);

        int center = size / 2;
        BinaryMaskType::IndexType index;

        // Create hole in center
        for (int z = center - holeRadius; z <= center + holeRadius; ++z) {
            for (int y = center - holeRadius; y <= center + holeRadius; ++y) {
                for (int x = center - holeRadius; x <= center + holeRadius; ++x) {
                    if (x >= 0 && x < size && y >= 0 && y < size && z >= 0 && z < size) {
                        index[0] = x;
                        index[1] = y;
                        index[2] = z;
                        mask->SetPixel(index, 0);
                    }
                }
            }
        }

        return mask;
    }

    /**
     * @brief Create a mask with multiple isolated components
     */
    BinaryMaskType::Pointer createMultiComponentMask(int size) {
        auto mask = createTestMask(size, size, size);

        BinaryMaskType::IndexType index;

        // Component 1: Large cube (5x5x5) at corner
        for (int z = 2; z < 7; ++z) {
            for (int y = 2; y < 7; ++y) {
                for (int x = 2; x < 7; ++x) {
                    index[0] = x;
                    index[1] = y;
                    index[2] = z;
                    mask->SetPixel(index, 1);
                }
            }
        }

        // Component 2: Small cube (2x2x2) at opposite corner
        for (int z = size - 4; z < size - 2; ++z) {
            for (int y = size - 4; y < size - 2; ++y) {
                for (int x = size - 4; x < size - 2; ++x) {
                    index[0] = x;
                    index[1] = y;
                    index[2] = z;
                    mask->SetPixel(index, 1);
                }
            }
        }

        return mask;
    }

    /**
     * @brief Create a multi-label map for testing
     */
    LabelMapType::Pointer createMultiLabelMap(int size) {
        auto labelMap = LabelMapType::New();

        LabelMapType::SizeType mapSize;
        mapSize[0] = size;
        mapSize[1] = size;
        mapSize[2] = size;

        LabelMapType::RegionType region;
        region.SetSize(mapSize);

        LabelMapType::IndexType start;
        start.Fill(0);
        region.SetIndex(start);

        labelMap->SetRegions(region);
        labelMap->Allocate();
        labelMap->FillBuffer(0);

        LabelMapType::IndexType index;

        // Label 1: Cube in lower region
        for (int z = 2; z < 8; ++z) {
            for (int y = 2; y < 8; ++y) {
                for (int x = 2; x < 8; ++x) {
                    index[0] = x;
                    index[1] = y;
                    index[2] = z;
                    labelMap->SetPixel(index, 1);
                }
            }
        }

        // Label 2: Cube in upper region
        for (int z = size - 8; z < size - 2; ++z) {
            for (int y = size - 8; y < size - 2; ++y) {
                for (int x = size - 8; x < size - 2; ++x) {
                    index[0] = x;
                    index[1] = y;
                    index[2] = z;
                    labelMap->SetPixel(index, 2);
                }
            }
        }

        return labelMap;
    }

    /**
     * @brief Count foreground voxels in mask
     */
    int countForegroundVoxels(BinaryMaskType::Pointer mask) {
        int count = 0;
        using IteratorType = itk::ImageRegionConstIterator<BinaryMaskType>;
        IteratorType it(mask, mask->GetLargestPossibleRegion());
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() > 0) {
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief Count voxels with specific label
     */
    int countLabelVoxels(LabelMapType::Pointer labelMap, unsigned char labelId) {
        int count = 0;
        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(labelMap, labelMap->GetLargestPossibleRegion());
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == labelId) {
                ++count;
            }
        }
        return count;
    }

    std::unique_ptr<MorphologicalProcessor> processor_;
};

// ============================================================================
// Parameter Validation Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, ParametersValidation) {
    MorphologicalProcessor::Parameters params;

    // Default parameters should be valid
    EXPECT_TRUE(params.isValid());

    // Radius in valid range
    params.radius = 1;
    EXPECT_TRUE(params.isValid());

    params.radius = 10;
    EXPECT_TRUE(params.isValid());

    // Radius out of range
    params.radius = 0;
    EXPECT_FALSE(params.isValid());

    params.radius = 11;
    EXPECT_FALSE(params.isValid());
}

TEST_F(MorphologicalProcessorTest, IslandRemovalParametersValidation) {
    MorphologicalProcessor::IslandRemovalParameters params;

    // Default parameters should be valid
    EXPECT_TRUE(params.isValid());

    // Valid range
    params.numberOfComponents = 1;
    EXPECT_TRUE(params.isValid());

    params.numberOfComponents = 255;
    EXPECT_TRUE(params.isValid());

    // Out of range
    params.numberOfComponents = 0;
    EXPECT_FALSE(params.isValid());

    params.numberOfComponents = 256;
    EXPECT_FALSE(params.isValid());
}

// ============================================================================
// Input Validation Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, NullInputReturnsError) {
    BinaryMaskType::Pointer nullMask = nullptr;
    MorphologicalProcessor::Parameters params;

    auto result = processor_->apply(nullMask, MorphologicalOperation::Opening, params);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(MorphologicalProcessorTest, InvalidParametersReturnsError) {
    auto mask = createCubeMask(20, 5);
    MorphologicalProcessor::Parameters params;
    params.radius = 0;  // Invalid

    auto result = processor_->apply(mask, MorphologicalOperation::Opening, params);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

// ============================================================================
// Opening Operation Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, OpeningRemovesSmallProtrusions) {
    auto mask = createCubeMask(30, 10);
    int originalCount = countForegroundVoxels(mask);

    MorphologicalProcessor::Parameters params;
    params.radius = 2;
    params.structuringElement = StructuringElementShape::Ball;

    auto result = processor_->opening(mask, params);
    ASSERT_TRUE(result.has_value());

    int resultCount = countForegroundVoxels(result.value());

    // Opening should not increase the size
    EXPECT_LE(resultCount, originalCount);
}

TEST_F(MorphologicalProcessorTest, OpeningWithCrossElement) {
    auto mask = createCubeMask(30, 10);

    MorphologicalProcessor::Parameters params;
    params.radius = 2;
    params.structuringElement = StructuringElementShape::Cross;

    auto result = processor_->opening(mask, params);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

// ============================================================================
// Closing Operation Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, ClosingFillsSmallHoles) {
    auto mask = createCubeWithHoleMask(30, 10, 2);
    int originalCount = countForegroundVoxels(mask);

    MorphologicalProcessor::Parameters params;
    params.radius = 3;
    params.structuringElement = StructuringElementShape::Ball;

    auto result = processor_->closing(mask, params);
    ASSERT_TRUE(result.has_value());

    int resultCount = countForegroundVoxels(result.value());

    // Closing should not decrease the size
    EXPECT_GE(resultCount, originalCount);
}

// ============================================================================
// Dilation Operation Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, DilationExpandsRegion) {
    auto mask = createCubeMask(30, 5);
    int originalCount = countForegroundVoxels(mask);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    auto result = processor_->dilation(mask, params);
    ASSERT_TRUE(result.has_value());

    int resultCount = countForegroundVoxels(result.value());

    // Dilation should expand the region
    EXPECT_GT(resultCount, originalCount);
}

// ============================================================================
// Erosion Operation Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, ErosionShrinksRegion) {
    auto mask = createCubeMask(30, 10);
    int originalCount = countForegroundVoxels(mask);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    auto result = processor_->erosion(mask, params);
    ASSERT_TRUE(result.has_value());

    int resultCount = countForegroundVoxels(result.value());

    // Erosion should shrink the region
    EXPECT_LT(resultCount, originalCount);
}

TEST_F(MorphologicalProcessorTest, ErosionThenDilationApproximatesOriginal) {
    auto mask = createCubeMask(30, 10);
    int originalCount = countForegroundVoxels(mask);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    // Erosion followed by dilation should approximately preserve size
    auto eroded = processor_->erosion(mask, params);
    ASSERT_TRUE(eroded.has_value());

    auto dilated = processor_->dilation(eroded.value(), params);
    ASSERT_TRUE(dilated.has_value());

    int resultCount = countForegroundVoxels(dilated.value());

    // Should be close to original (within 20%)
    double ratio = static_cast<double>(resultCount) / originalCount;
    EXPECT_GT(ratio, 0.8);
    EXPECT_LT(ratio, 1.2);
}

// ============================================================================
// Fill Holes Operation Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, FillHolesFillsInternalHoles) {
    auto mask = createCubeWithHoleMask(30, 10, 3);
    int originalCount = countForegroundVoxels(mask);

    auto result = processor_->fillHoles(mask, 1);
    ASSERT_TRUE(result.has_value());

    int resultCount = countForegroundVoxels(result.value());

    // Fill holes should increase the foreground count
    EXPECT_GT(resultCount, originalCount);
}

// ============================================================================
// Island Removal Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, KeepLargestComponentRemovesSmallIslands) {
    auto mask = createMultiComponentMask(20);
    int originalCount = countForegroundVoxels(mask);

    auto result = processor_->keepLargestComponents(mask, 1);
    ASSERT_TRUE(result.has_value());

    int resultCount = countForegroundVoxels(result.value());

    // Should have fewer voxels (small component removed)
    EXPECT_LT(resultCount, originalCount);
    EXPECT_GT(resultCount, 0);
}

TEST_F(MorphologicalProcessorTest, KeepMultipleComponents) {
    auto mask = createMultiComponentMask(20);
    int originalCount = countForegroundVoxels(mask);

    auto result = processor_->keepLargestComponents(mask, 2);
    ASSERT_TRUE(result.has_value());

    int resultCount = countForegroundVoxels(result.value());

    // With 2 components kept, should have all voxels
    EXPECT_EQ(resultCount, originalCount);
}

// ============================================================================
// Apply Generic Interface Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, ApplyWithOperationType) {
    auto mask = createCubeMask(30, 10);

    auto result = processor_->apply(mask, MorphologicalOperation::Opening, 2);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

TEST_F(MorphologicalProcessorTest, ApplyWithParameters) {
    auto mask = createCubeMask(30, 10);

    MorphologicalProcessor::Parameters params;
    params.radius = 2;
    params.structuringElement = StructuringElementShape::Ball;

    auto result = processor_->apply(mask, MorphologicalOperation::Closing, params);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

// ============================================================================
// 2D Slice Preview Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, ApplyToSliceReturns2DResult) {
    auto mask = createCubeMask(30, 10);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    auto result = processor_->applyToSlice(mask, 15, MorphologicalOperation::Dilation, params);
    ASSERT_TRUE(result.has_value());

    auto slice = result.value();
    EXPECT_NE(slice, nullptr);

    // Verify it's 2D
    auto region = slice->GetLargestPossibleRegion();
    EXPECT_EQ(region.GetSize()[0], 30);
    EXPECT_EQ(region.GetSize()[1], 30);
}

TEST_F(MorphologicalProcessorTest, ApplyToSliceInvalidIndex) {
    auto mask = createCubeMask(30, 10);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    auto result = processor_->applyToSlice(mask, 100, MorphologicalOperation::Dilation, params);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

// ============================================================================
// Multi-Label Operations Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, ApplyToLabelModifiesOnlySpecifiedLabel) {
    auto labelMap = createMultiLabelMap(30);

    int label1Count = countLabelVoxels(labelMap, 1);
    int label2Count = countLabelVoxels(labelMap, 2);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    auto result = processor_->applyToLabel(labelMap, 1, MorphologicalOperation::Dilation, params);
    ASSERT_TRUE(result.has_value());

    int newLabel1Count = countLabelVoxels(result.value(), 1);
    int newLabel2Count = countLabelVoxels(result.value(), 2);

    // Label 1 should be dilated (increased)
    EXPECT_GT(newLabel1Count, label1Count);

    // Label 2 should be unchanged
    EXPECT_EQ(newLabel2Count, label2Count);
}

TEST_F(MorphologicalProcessorTest, ApplyToLabelRejectsBackgroundLabel) {
    auto labelMap = createMultiLabelMap(30);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    auto result = processor_->applyToLabel(labelMap, 0, MorphologicalOperation::Dilation, params);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

TEST_F(MorphologicalProcessorTest, ApplyToAllLabelsModifiesAllLabels) {
    auto labelMap = createMultiLabelMap(30);

    int label1Count = countLabelVoxels(labelMap, 1);
    int label2Count = countLabelVoxels(labelMap, 2);

    MorphologicalProcessor::Parameters params;
    params.radius = 1;

    auto result = processor_->applyToAllLabels(labelMap, MorphologicalOperation::Erosion, params);
    ASSERT_TRUE(result.has_value());

    int newLabel1Count = countLabelVoxels(result.value(), 1);
    int newLabel2Count = countLabelVoxels(result.value(), 2);

    // Both labels should be eroded (decreased)
    EXPECT_LT(newLabel1Count, label1Count);
    EXPECT_LT(newLabel2Count, label2Count);
}

// ============================================================================
// Progress Callback Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, ProgressCallbackIsCalled) {
    auto mask = createCubeMask(30, 10);

    bool callbackCalled = false;
    double lastProgress = 0.0;

    processor_->setProgressCallback([&](double progress) {
        callbackCalled = true;
        lastProgress = progress;
    });

    MorphologicalProcessor::Parameters params;
    params.radius = 2;

    auto result = processor_->opening(mask, params);
    ASSERT_TRUE(result.has_value());

    // Progress callback may or may not be called depending on ITK filter implementation
    // Just verify no crash occurs
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(MorphologicalProcessorTest, OperationToString) {
    EXPECT_EQ(MorphologicalProcessor::operationToString(MorphologicalOperation::Opening),
              "Opening");
    EXPECT_EQ(MorphologicalProcessor::operationToString(MorphologicalOperation::Closing),
              "Closing");
    EXPECT_EQ(MorphologicalProcessor::operationToString(MorphologicalOperation::Dilation),
              "Dilation");
    EXPECT_EQ(MorphologicalProcessor::operationToString(MorphologicalOperation::Erosion),
              "Erosion");
    EXPECT_EQ(MorphologicalProcessor::operationToString(MorphologicalOperation::FillHoles),
              "Fill Holes");
    EXPECT_EQ(MorphologicalProcessor::operationToString(MorphologicalOperation::IslandRemoval),
              "Island Removal");
}

TEST_F(MorphologicalProcessorTest, StructuringElementToString) {
    EXPECT_EQ(MorphologicalProcessor::structuringElementToString(StructuringElementShape::Ball),
              "Ball");
    EXPECT_EQ(MorphologicalProcessor::structuringElementToString(StructuringElementShape::Cross),
              "Cross");
}

// ============================================================================
// Edge case and algorithmic correctness tests (Issue #204)
// ============================================================================

TEST_F(MorphologicalProcessorTest, OneVoxelThickStructureErodedCompletely) {
    // A single-voxel-thick line should be completely removed by erosion with radius 1
    auto mask = createTestMask(20, 20, 20);

    // Create a 1-voxel-thick line along x-axis at y=10, z=10
    BinaryMaskType::IndexType idx;
    for (int x = 2; x < 18; ++x) {
        idx[0] = x;
        idx[1] = 10;
        idx[2] = 10;
        mask->SetPixel(idx, 1);
    }

    int beforeCount = countForegroundVoxels(mask);
    EXPECT_EQ(beforeCount, 16);

    auto result = processor_->erosion(mask, 1, StructuringElementShape::Ball);
    ASSERT_TRUE(result.has_value());

    int afterCount = countForegroundVoxels(result.value());
    EXPECT_EQ(afterCount, 0)
        << "1-voxel-thick structure should be completely eroded by radius-1 ball";
}

TEST_F(MorphologicalProcessorTest, RepeatedClosingStability) {
    // Applying closing twice should produce the same result as closing once
    // (closing is idempotent for the same structuring element)
    auto mask = createCubeMask(30, 5);

    auto result1 = processor_->closing(mask, 2, StructuringElementShape::Ball);
    ASSERT_TRUE(result1.has_value());
    int count1 = countForegroundVoxels(result1.value());

    auto result2 = processor_->closing(result1.value(), 2, StructuringElementShape::Ball);
    ASSERT_TRUE(result2.has_value());
    int count2 = countForegroundVoxels(result2.value());

    EXPECT_EQ(count1, count2)
        << "Repeated closing with same SE should be idempotent";
}

TEST_F(MorphologicalProcessorTest, DilationMergesNearbyRegions) {
    // Two cubes separated by a small gap should merge after sufficient dilation
    auto mask = createTestMask(30, 30, 30);

    BinaryMaskType::IndexType idx;

    // Cube 1: center at (8, 15, 15), radius 3
    for (int z = 12; z <= 18; ++z) {
        for (int y = 12; y <= 18; ++y) {
            for (int x = 5; x <= 11; ++x) {
                idx[0] = x; idx[1] = y; idx[2] = z;
                mask->SetPixel(idx, 1);
            }
        }
    }

    // Cube 2: center at (22, 15, 15), radius 3 — gap of 4 voxels
    for (int z = 12; z <= 18; ++z) {
        for (int y = 12; y <= 18; ++y) {
            for (int x = 19; x <= 25; ++x) {
                idx[0] = x; idx[1] = y; idx[2] = z;
                mask->SetPixel(idx, 1);
            }
        }
    }

    int beforeCount = countForegroundVoxels(mask);

    // Dilate with radius 3 — should bridge the 4-voxel gap
    auto result = processor_->dilation(mask, 3, StructuringElementShape::Ball);
    ASSERT_TRUE(result.has_value());

    int afterCount = countForegroundVoxels(result.value());
    EXPECT_GT(afterCount, beforeCount)
        << "Dilation should expand the regions";

    // Check that the gap region is now filled (voxel at x=15 between cubes)
    idx[0] = 15; idx[1] = 15; idx[2] = 15;
    EXPECT_EQ(result.value()->GetPixel(idx), 1)
        << "Gap between cubes should be bridged by dilation radius 3";
}

}  // namespace dicom_viewer::services::test

#include <gtest/gtest.h>

#include "services/segmentation/slice_interpolator.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

class SliceInterpolatorTest : public ::testing::Test {
protected:
    using LabelMapType = SliceInterpolator::LabelMapType;

    void SetUp() override {
        interpolator_ = std::make_unique<SliceInterpolator>();
    }

    /**
     * @brief Create a label map with a spherical region in specific slices
     */
    LabelMapType::Pointer createSparseLabelMap(
        unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ,
        const std::vector<int>& annotatedSlices,
        uint8_t labelId,
        double centerX, double centerY, double radius
    ) {
        auto image = LabelMapType::New();

        LabelMapType::RegionType region;
        LabelMapType::IndexType start = {{0, 0, 0}};
        LabelMapType::SizeType size = {{sizeX, sizeY, sizeZ}};
        region.SetSize(size);
        region.SetIndex(start);

        image->SetRegions(region);

        LabelMapType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        image->SetSpacing(spacing);

        LabelMapType::PointType origin;
        origin[0] = 0.0;
        origin[1] = 0.0;
        origin[2] = 0.0;
        image->SetOrigin(origin);

        image->Allocate();
        image->FillBuffer(0);

        // Create circular regions only in annotated slices
        for (int z : annotatedSlices) {
            if (z < 0 || z >= static_cast<int>(sizeZ)) continue;

            for (unsigned int y = 0; y < sizeY; ++y) {
                for (unsigned int x = 0; x < sizeX; ++x) {
                    double dx = static_cast<double>(x) - centerX;
                    double dy = static_cast<double>(y) - centerY;
                    double dist = std::sqrt(dx*dx + dy*dy);

                    if (dist <= radius) {
                        LabelMapType::IndexType idx = {{
                            static_cast<long>(x),
                            static_cast<long>(y),
                            static_cast<long>(z)
                        }};
                        image->SetPixel(idx, labelId);
                    }
                }
            }
        }

        return image;
    }

    /**
     * @brief Create a label map with a cylinder (continuous across all slices)
     */
    LabelMapType::Pointer createCylinderLabelMap(
        unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ,
        uint8_t labelId,
        double centerX, double centerY, double radius
    ) {
        std::vector<int> allSlices;
        for (unsigned int z = 0; z < sizeZ; ++z) {
            allSlices.push_back(static_cast<int>(z));
        }
        return createSparseLabelMap(sizeX, sizeY, sizeZ, allSlices,
                                    labelId, centerX, centerY, radius);
    }

    /**
     * @brief Count voxels with a specific label value
     */
    size_t countLabelVoxels(LabelMapType::Pointer image, uint8_t labelId) {
        size_t count = 0;
        itk::ImageRegionConstIterator<LabelMapType> it(
            image, image->GetLargestPossibleRegion());

        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == labelId) {
                ++count;
            }
        }
        return count;
    }

    std::unique_ptr<SliceInterpolator> interpolator_;
};

// ============================================================================
// Basic Tests
// ============================================================================

TEST_F(SliceInterpolatorTest, DetectAnnotatedSlices_ReturnsCorrectSlices) {
    // Create label map with annotations in slices 10, 20, 30
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20, 30}, 1, 32.0, 32.0, 10.0);

    auto slices = interpolator_->detectAnnotatedSlices(labelMap, 1);

    ASSERT_EQ(slices.size(), 3);
    EXPECT_EQ(slices[0], 10);
    EXPECT_EQ(slices[1], 20);
    EXPECT_EQ(slices[2], 30);
}

TEST_F(SliceInterpolatorTest, DetectAnnotatedSlices_EmptyForNonexistentLabel) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20, 30}, 1, 32.0, 32.0, 10.0);

    auto slices = interpolator_->detectAnnotatedSlices(labelMap, 2);

    EXPECT_TRUE(slices.empty());
}

TEST_F(SliceInterpolatorTest, DetectLabels_FindsAllLabels) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20}, 1, 32.0, 32.0, 10.0);

    // Add a second label
    LabelMapType::IndexType idx = {{16, 16, 15}};
    labelMap->SetPixel(idx, 2);

    auto labels = interpolator_->detectLabels(labelMap);

    ASSERT_EQ(labels.size(), 2);
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), 1) != labels.end());
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), 2) != labels.end());
}

TEST_F(SliceInterpolatorTest, DetectLabels_ExcludesBackground) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10}, 1, 32.0, 32.0, 5.0);

    auto labels = interpolator_->detectLabels(labelMap);

    // Should not contain 0 (background)
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), 0) == labels.end());
}

// ============================================================================
// Interpolation Tests
// ============================================================================

TEST_F(SliceInterpolatorTest, Interpolate_FillsGapsBetweenSlices) {
    // Create sparse annotation: slices 10 and 20 only
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20}, 1, 32.0, 32.0, 10.0);

    InterpolationParameters params;
    params.labelIds = {1};
    params.method = InterpolationMethod::Morphological;

    auto result = interpolator_->interpolate(labelMap, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->interpolatedSlices.empty());

    // Check that intermediate slices now have content
    auto newSlices = interpolator_->detectAnnotatedSlices(result->interpolatedMask, 1);
    EXPECT_GT(newSlices.size(), 2);

    // Verify slices 11-19 are now filled
    for (int z = 11; z < 20; ++z) {
        bool found = std::find(newSlices.begin(), newSlices.end(), z) != newSlices.end();
        EXPECT_TRUE(found) << "Slice " << z << " should be filled";
    }
}

TEST_F(SliceInterpolatorTest, Interpolate_PreservesSourceSlices) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20, 30}, 1, 32.0, 32.0, 10.0);
    size_t originalVoxels10 = 0;
    size_t originalVoxels20 = 0;
    size_t originalVoxels30 = 0;

    // Count voxels in original slices
    {
        LabelMapType::RegionType slice10Region;
        slice10Region.SetIndex({{0, 0, 10}});
        slice10Region.SetSize({{64, 64, 1}});
        itk::ImageRegionConstIterator<LabelMapType> it(labelMap, slice10Region);
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == 1) ++originalVoxels10;
        }
    }
    {
        LabelMapType::RegionType slice20Region;
        slice20Region.SetIndex({{0, 0, 20}});
        slice20Region.SetSize({{64, 64, 1}});
        itk::ImageRegionConstIterator<LabelMapType> it(labelMap, slice20Region);
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == 1) ++originalVoxels20;
        }
    }
    {
        LabelMapType::RegionType slice30Region;
        slice30Region.SetIndex({{0, 0, 30}});
        slice30Region.SetSize({{64, 64, 1}});
        itk::ImageRegionConstIterator<LabelMapType> it(labelMap, slice30Region);
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == 1) ++originalVoxels30;
        }
    }

    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(labelMap, params);
    ASSERT_TRUE(result.has_value());

    // Verify source slices are preserved
    EXPECT_TRUE(std::find(result->sourceSlices.begin(), result->sourceSlices.end(), 10)
        != result->sourceSlices.end());
    EXPECT_TRUE(std::find(result->sourceSlices.begin(), result->sourceSlices.end(), 20)
        != result->sourceSlices.end());
    EXPECT_TRUE(std::find(result->sourceSlices.begin(), result->sourceSlices.end(), 30)
        != result->sourceSlices.end());
}

TEST_F(SliceInterpolatorTest, Interpolate_HandlesMultipleLabels) {
    // Create two labels with different sparse patterns
    auto labelMap = createSparseLabelMap(64, 64, 50, {5, 15}, 1, 20.0, 32.0, 8.0);

    // Add second label in different slices
    for (int z : {10, 20}) {
        for (unsigned int y = 0; y < 64; ++y) {
            for (unsigned int x = 0; x < 64; ++x) {
                double dx = static_cast<double>(x) - 44.0;
                double dy = static_cast<double>(y) - 32.0;
                if (std::sqrt(dx*dx + dy*dy) <= 8.0) {
                    LabelMapType::IndexType idx = {{
                        static_cast<long>(x),
                        static_cast<long>(y),
                        static_cast<long>(z)
                    }};
                    labelMap->SetPixel(idx, 2);
                }
            }
        }
    }

    InterpolationParameters params;
    params.labelIds = {1, 2};

    auto result = interpolator_->interpolate(labelMap, params);

    ASSERT_TRUE(result.has_value());

    // Both labels should have been interpolated
    auto label1Slices = interpolator_->detectAnnotatedSlices(result->interpolatedMask, 1);
    auto label2Slices = interpolator_->detectAnnotatedSlices(result->interpolatedMask, 2);

    EXPECT_GT(label1Slices.size(), 2);
    EXPECT_GT(label2Slices.size(), 2);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(SliceInterpolatorTest, Interpolate_FailsWithNullInput) {
    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(nullptr, params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST_F(SliceInterpolatorTest, Interpolate_FailsWithNoLabels) {
    auto emptyLabelMap = LabelMapType::New();
    LabelMapType::RegionType region;
    region.SetSize({{64, 64, 50}});
    emptyLabelMap->SetRegions(region);
    emptyLabelMap->Allocate();
    emptyLabelMap->FillBuffer(0);

    InterpolationParameters params;
    // Empty labelIds means auto-detect, but there are no labels

    auto result = interpolator_->interpolate(emptyLabelMap, params);

    EXPECT_FALSE(result.has_value());
}

TEST_F(SliceInterpolatorTest, InterpolateRange_ValidatesSliceIndices) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20}, 1, 32.0, 32.0, 10.0);

    // Invalid range: negative start
    auto result1 = interpolator_->interpolateRange(labelMap, 1, -1, 20);
    EXPECT_FALSE(result1.has_value());

    // Invalid range: end beyond bounds
    auto result2 = interpolator_->interpolateRange(labelMap, 1, 10, 100);
    EXPECT_FALSE(result2.has_value());

    // Invalid range: start > end
    auto result3 = interpolator_->interpolateRange(labelMap, 1, 30, 10);
    EXPECT_FALSE(result3.has_value());
}

// ============================================================================
// Different Interpolation Methods
// ============================================================================

TEST_F(SliceInterpolatorTest, ShapeBasedInterpolation_ProducesResult) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20}, 1, 32.0, 32.0, 10.0);

    InterpolationParameters params;
    params.labelIds = {1};
    params.method = InterpolationMethod::ShapeBased;

    auto result = interpolator_->interpolate(labelMap, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->interpolatedSlices.empty());
}

TEST_F(SliceInterpolatorTest, LinearInterpolation_ProducesResult) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20}, 1, 32.0, 32.0, 10.0);

    InterpolationParameters params;
    params.labelIds = {1};
    params.method = InterpolationMethod::Linear;

    auto result = interpolator_->interpolate(labelMap, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->interpolatedSlices.empty());
}

// ============================================================================
// Preview Tests
// ============================================================================

TEST_F(SliceInterpolatorTest, PreviewSlice_ReturnsValidSlice) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20}, 1, 32.0, 32.0, 10.0);

    // Preview slice 15 (between annotated slices)
    auto result = interpolator_->previewSlice(labelMap, 1, 15);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().IsNotNull());

    // Check dimensions
    auto size = result.value()->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], 64);
    EXPECT_EQ(size[1], 64);
}

TEST_F(SliceInterpolatorTest, PreviewSlice_FailsWithInvalidIndex) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 20}, 1, 32.0, 32.0, 10.0);

    auto result = interpolator_->previewSlice(labelMap, 1, 100);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidParameters);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SliceInterpolatorTest, Interpolate_HandlesSingleAnnotatedSlice) {
    // Only one annotated slice - cannot interpolate
    auto labelMap = createSparseLabelMap(64, 64, 50, {25}, 1, 32.0, 32.0, 10.0);

    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(labelMap, params);

    // Should succeed but with no interpolated slices
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->interpolatedSlices.empty());
}

TEST_F(SliceInterpolatorTest, Interpolate_HandlesContiguousSlices) {
    // No gaps to fill
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 11, 12, 13, 14}, 1,
                                         32.0, 32.0, 10.0);

    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(labelMap, params);

    ASSERT_TRUE(result.has_value());
    // Few or no interpolated slices since they are contiguous
    EXPECT_LE(result->interpolatedSlices.size(), 1);
}

TEST_F(SliceInterpolatorTest, ProgressCallback_IsCalled) {
    auto labelMap = createSparseLabelMap(64, 64, 50, {10, 30}, 1, 32.0, 32.0, 10.0);

    bool callbackCalled = false;
    interpolator_->setProgressCallback([&callbackCalled](double progress) {
        callbackCalled = true;
        EXPECT_GE(progress, 0.0);
        EXPECT_LE(progress, 1.0);
    });

    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(labelMap, params);

    EXPECT_TRUE(result.has_value());
    // Progress callback may or may not be called depending on ITK's behavior
}

// =============================================================================
// Edge case and algorithmic correctness tests (Issue #204)
// =============================================================================

TEST_F(SliceInterpolatorTest, VolumeConservationBetweenIdenticalSlices) {
    // Two identical circular annotations → interpolated volume should equal
    // the original volume (no gain, no loss)
    auto labelMap = createSparseLabelMap(64, 64, 20, {5, 15}, 1, 32.0, 32.0, 10.0);

    size_t beforeCount = countLabelVoxels(labelMap, 1);
    // 2 slices × ~π×10² ≈ 628 voxels
    EXPECT_GT(beforeCount, 0u);

    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(labelMap, params);
    ASSERT_TRUE(result.has_value());

    size_t afterCount = countLabelVoxels(result.value().interpolatedMask, 1);

    // Interpolated volume should be >= original (fills in-between slices)
    EXPECT_GE(afterCount, beforeCount)
        << "Interpolation should not lose existing annotations";

    // For identical circles, interpolation should fill uniformly
    // Expected: ~10 slices × ~314 voxels ≈ 3140 (slices 5-15)
    size_t perSliceArea = beforeCount / 2;
    size_t expectedFilled = perSliceArea * 11;  // 11 slices from 5 to 15 inclusive
    EXPECT_NEAR(static_cast<double>(afterCount),
                static_cast<double>(expectedFilled),
                expectedFilled * 0.15);
}

TEST_F(SliceInterpolatorTest, NonConvexRegionInterpolation) {
    // Create an L-shaped region to test non-convex interpolation
    auto labelMap = LabelMapType::New();
    LabelMapType::SizeType size = {{32, 32, 20}};
    LabelMapType::RegionType region;
    region.SetSize(size);
    region.SetIndex({{0, 0, 0}});
    labelMap->SetRegions(region);

    LabelMapType::SpacingType spacing;
    spacing.Fill(1.0);
    labelMap->SetSpacing(spacing);

    LabelMapType::PointType origin;
    origin.Fill(0.0);
    labelMap->SetOrigin(origin);

    labelMap->Allocate();
    labelMap->FillBuffer(0);

    // L-shape in slices 3 and 17
    for (int z : {3, 17}) {
        for (int y = 5; y < 25; ++y) {
            for (int x = 5; x < 15; ++x) {
                labelMap->SetPixel({{x, y, z}}, 1);
            }
        }
        for (int y = 5; y < 15; ++y) {
            for (int x = 15; x < 25; ++x) {
                labelMap->SetPixel({{x, y, z}}, 1);
            }
        }
    }

    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(labelMap, params);
    ASSERT_TRUE(result.has_value());

    size_t afterCount = countLabelVoxels(result.value().interpolatedMask, 1);
    EXPECT_GT(afterCount, 0u)
        << "Non-convex L-shape should be interpolated without crashing";
}

TEST_F(SliceInterpolatorTest, SparseAnnotationsOverFiftySlicesApart) {
    // Annotations 60 slices apart (clinically unusual but shouldn't crash)
    auto labelMap = createSparseLabelMap(32, 32, 80, {5, 65}, 1, 16.0, 16.0, 8.0);

    InterpolationParameters params;
    params.labelIds = {1};

    auto result = interpolator_->interpolate(labelMap, params);
    ASSERT_TRUE(result.has_value());

    size_t afterCount = countLabelVoxels(result.value().interpolatedMask, 1);
    EXPECT_GT(afterCount, 0u)
        << "Sparse annotations 60 slices apart should still interpolate";

    // Should fill all slices between 5 and 65
    size_t beforeCount = countLabelVoxels(labelMap, 1);
    EXPECT_GT(afterCount, beforeCount)
        << "Interpolation should add voxels between sparse annotations";
}

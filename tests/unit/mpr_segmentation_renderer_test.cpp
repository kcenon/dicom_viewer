#include <gtest/gtest.h>

#include "services/segmentation/mpr_segmentation_renderer.hpp"
#include "services/segmentation/segmentation_label.hpp"
#include "services/mpr_renderer.hpp"

#include <cstdint>
#include <vector>

#include <itkImage.h>
#include <itkImageRegionIterator.h>

#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

using namespace dicom_viewer::services;

// =============================================================================
// Helper: create a synthetic 3D label map with known label values
// =============================================================================

namespace {

/// Create a 3D label map where each voxel's label = (z % numLabels)
/// This gives each axial slice a uniform label, useful for testing
/// slice extraction.
MPRSegmentationRenderer::LabelMapType::Pointer
createTestLabelMap(int width, int height, int depth, int numLabels = 3) {
    using LabelMapType = MPRSegmentationRenderer::LabelMapType;
    auto labelMap = LabelMapType::New();

    LabelMapType::SizeType size;
    size[0] = width;
    size[1] = height;
    size[2] = depth;

    LabelMapType::IndexType start;
    start.Fill(0);

    LabelMapType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    labelMap->SetRegions(region);
    labelMap->Allocate();
    labelMap->FillBuffer(0);

    // Set spacing
    LabelMapType::SpacingType spacing;
    spacing[0] = 1.0;
    spacing[1] = 1.0;
    spacing[2] = 1.0;
    labelMap->SetSpacing(spacing);

    // Fill: label = (z % numLabels) + 1 for z < numLabels slices,
    // 0 (background) for remaining slices
    itk::ImageRegionIterator<LabelMapType> it(labelMap, region);
    while (!it.IsAtEnd()) {
        auto idx = it.GetIndex();
        int z = idx[2];
        if (z < numLabels) {
            it.Set(static_cast<uint8_t>((z % numLabels) + 1));
        } else {
            it.Set(0);
        }
        ++it;
    }

    return labelMap;
}

/// Create a label map with a single labeled voxel at given position
MPRSegmentationRenderer::LabelMapType::Pointer
createSingleVoxelLabelMap(int width, int height, int depth,
                          int vx, int vy, int vz, uint8_t label) {
    using LabelMapType = MPRSegmentationRenderer::LabelMapType;
    auto labelMap = LabelMapType::New();

    LabelMapType::SizeType size;
    size[0] = width;
    size[1] = height;
    size[2] = depth;

    LabelMapType::IndexType start;
    start.Fill(0);

    LabelMapType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    labelMap->SetRegions(region);
    labelMap->Allocate();
    labelMap->FillBuffer(0);

    LabelMapType::IndexType idx;
    idx[0] = vx;
    idx[1] = vy;
    idx[2] = vz;
    labelMap->SetPixel(idx, label);

    return labelMap;
}

/// Create a label map filled entirely with a single label value
MPRSegmentationRenderer::LabelMapType::Pointer
createUniformLabelMap(int width, int height, int depth, uint8_t label) {
    using LabelMapType = MPRSegmentationRenderer::LabelMapType;
    auto labelMap = LabelMapType::New();

    LabelMapType::SizeType size;
    size[0] = width;
    size[1] = height;
    size[2] = depth;

    LabelMapType::IndexType start;
    start.Fill(0);

    LabelMapType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    labelMap->SetRegions(region);
    labelMap->Allocate();
    labelMap->FillBuffer(label);

    return labelMap;
}

}  // anonymous namespace

// =============================================================================
// Test fixture
// =============================================================================

class MPRSegmentationRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create offscreen VTK renderers for testing
        renderWindow_ = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow_->SetOffScreenRendering(1);
        renderWindow_->SetSize(64, 64);

        for (int i = 0; i < 3; ++i) {
            renderers_[i] = vtkSmartPointer<vtkRenderer>::New();
            renderWindow_->AddRenderer(renderers_[i]);
        }
    }

    MPRSegmentationRenderer renderer_;
    vtkSmartPointer<vtkRenderWindow> renderWindow_;
    std::array<vtkSmartPointer<vtkRenderer>, 3> renderers_;
};

// =============================================================================
// Construction & Lifecycle tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, DefaultConstruction) {
    MPRSegmentationRenderer renderer;
    // Verify no crash on construction/destruction
}

TEST_F(MPRSegmentationRendererTest, MoveConstruction) {
    MPRSegmentationRenderer renderer1;
    MPRSegmentationRenderer renderer2(std::move(renderer1));
    // Verify no crash on move construction
}

TEST_F(MPRSegmentationRendererTest, MoveAssignment) {
    MPRSegmentationRenderer renderer1;
    MPRSegmentationRenderer renderer2;
    renderer2 = std::move(renderer1);
    // Verify no crash on move assignment
}

TEST_F(MPRSegmentationRendererTest, InitialState) {
    // Initial visibility should be true
    EXPECT_TRUE(renderer_.isVisible());

    // Initial opacity should be 0.5
    EXPECT_DOUBLE_EQ(renderer_.getOpacity(), 0.5);

    // No label map set initially
    EXPECT_EQ(renderer_.getLabelMap(), nullptr);

    // Slice indices should be 0
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Axial), 0);
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Coronal), 0);
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Sagittal), 0);
}

// =============================================================================
// Label Map Integration tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, SetLabelMapAndRetrieve) {
    auto labelMap = createTestLabelMap(8, 8, 4);

    renderer_.setLabelMap(labelMap);
    EXPECT_EQ(renderer_.getLabelMap(), labelMap);
}

TEST_F(MPRSegmentationRendererTest, SetNullLabelMap) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);
    EXPECT_NE(renderer_.getLabelMap(), nullptr);

    renderer_.setLabelMap(nullptr);
    EXPECT_EQ(renderer_.getLabelMap(), nullptr);
}

TEST_F(MPRSegmentationRendererTest, SetLabelMapTriggersCallback) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);
    EXPECT_EQ(callbackCount, 1);

    // Setting null also triggers callback
    renderer_.setLabelMap(nullptr);
    EXPECT_EQ(callbackCount, 2);
}

TEST_F(MPRSegmentationRendererTest, UpdateLabelMapTriggersReRender) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);
    int afterSet = callbackCount;

    // update() should trigger callback
    renderer_.update();
    EXPECT_GT(callbackCount, afterSet);
}

TEST_F(MPRSegmentationRendererTest, ClearLabelMap) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);
    EXPECT_NE(renderer_.getLabelMap(), nullptr);

    renderer_.clear();
    EXPECT_EQ(renderer_.getLabelMap(), nullptr);
}

TEST_F(MPRSegmentationRendererTest, ClearTriggersCallback) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);
    int afterSet = callbackCount;

    renderer_.clear();
    EXPECT_GT(callbackCount, afterSet);
}

// =============================================================================
// Overlay Rendering: slice extraction for each plane
// =============================================================================

TEST_F(MPRSegmentationRendererTest, SetSliceIndexAxial) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    renderer_.setSliceIndex(MPRPlane::Axial, 2);
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Axial), 2);
}

TEST_F(MPRSegmentationRendererTest, SetSliceIndexCoronal) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    renderer_.setSliceIndex(MPRPlane::Coronal, 3);
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Coronal), 3);
}

TEST_F(MPRSegmentationRendererTest, SetSliceIndexSagittal) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    renderer_.setSliceIndex(MPRPlane::Sagittal, 5);
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Sagittal), 5);
}

TEST_F(MPRSegmentationRendererTest, SetSliceIndexTriggersCallback) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    renderer_.setSliceIndex(MPRPlane::Axial, 1);
    EXPECT_EQ(callbackCount, 1);

    renderer_.setSliceIndex(MPRPlane::Coronal, 2);
    EXPECT_EQ(callbackCount, 2);

    renderer_.setSliceIndex(MPRPlane::Sagittal, 3);
    EXPECT_EQ(callbackCount, 3);
}

TEST_F(MPRSegmentationRendererTest, SliceExtractionAllPlanes) {
    // Verify that slice extraction works for all 3 planes without crashing
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 6, 4);
    renderer_.setLabelMap(labelMap);

    // Axial: extract XY at Z=1
    renderer_.setSliceIndex(MPRPlane::Axial, 1);
    // Coronal: extract XZ at Y=3
    renderer_.setSliceIndex(MPRPlane::Coronal, 3);
    // Sagittal: extract YZ at X=5
    renderer_.setSliceIndex(MPRPlane::Sagittal, 5);
}

TEST_F(MPRSegmentationRendererTest, UpdatePlaneSpecific) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    renderer_.updatePlane(MPRPlane::Axial);
    EXPECT_EQ(callbackCount, 1);

    renderer_.updatePlane(MPRPlane::Coronal);
    EXPECT_EQ(callbackCount, 2);
}

// =============================================================================
// Overlay Visibility tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, SetVisibleTrue) {
    renderer_.setVisible(true);
    EXPECT_TRUE(renderer_.isVisible());
}

TEST_F(MPRSegmentationRendererTest, SetVisibleFalse) {
    renderer_.setVisible(false);
    EXPECT_FALSE(renderer_.isVisible());
}

TEST_F(MPRSegmentationRendererTest, SetVisibleTriggersCallback) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    renderer_.setVisible(false);
    EXPECT_EQ(callbackCount, 1);

    renderer_.setVisible(true);
    EXPECT_EQ(callbackCount, 2);
}

TEST_F(MPRSegmentationRendererTest, SetLabelVisible) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    renderer_.setLabelVisible(1, false);
    EXPECT_EQ(callbackCount, 1);

    renderer_.setLabelVisible(1, true);
    EXPECT_EQ(callbackCount, 2);
}

TEST_F(MPRSegmentationRendererTest, SetLabelColor) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    LabelColor red(1.0f, 0.0f, 0.0f, 1.0f);
    renderer_.setLabelColor(1, red);
    EXPECT_EQ(callbackCount, 1);

    LabelColor blue(0.0f, 0.0f, 1.0f, 0.8f);
    renderer_.setLabelColor(2, blue);
    EXPECT_EQ(callbackCount, 2);
}

// =============================================================================
// Opacity tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, SetOpacity) {
    renderer_.setOpacity(0.7);
    EXPECT_DOUBLE_EQ(renderer_.getOpacity(), 0.7);
}

TEST_F(MPRSegmentationRendererTest, OpacityClampedToRange) {
    renderer_.setOpacity(-0.5);
    EXPECT_DOUBLE_EQ(renderer_.getOpacity(), 0.0);

    renderer_.setOpacity(1.5);
    EXPECT_DOUBLE_EQ(renderer_.getOpacity(), 1.0);
}

TEST_F(MPRSegmentationRendererTest, SetOpacityTriggersCallback) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    renderer_.setOpacity(0.3);
    EXPECT_EQ(callbackCount, 1);
}

// =============================================================================
// Renderer management tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, SetRenderersTriple) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    // Setting renderers should add actors
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);

    // Actors should be added to renderers
    EXPECT_GT(renderers_[0]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_GT(renderers_[1]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_GT(renderers_[2]->GetActors()->GetNumberOfItems(), 0);
}

TEST_F(MPRSegmentationRendererTest, SetRendererSinglePlane) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    auto singleRenderer = vtkSmartPointer<vtkRenderer>::New();
    renderer_.setRenderer(MPRPlane::Axial, singleRenderer);

    EXPECT_GT(singleRenderer->GetActors()->GetNumberOfItems(), 0);
}

TEST_F(MPRSegmentationRendererTest, RemoveFromRenderers) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);

    int actorsBefore = renderers_[0]->GetActors()->GetNumberOfItems();
    EXPECT_GT(actorsBefore, 0);

    renderer_.removeFromRenderers();

    EXPECT_EQ(renderers_[0]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_EQ(renderers_[1]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_EQ(renderers_[2]->GetActors()->GetNumberOfItems(), 0);
}

TEST_F(MPRSegmentationRendererTest, SetRenderersReplacePrevious) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);

    // Create new renderers and replace
    auto newAxial = vtkSmartPointer<vtkRenderer>::New();
    auto newCoronal = vtkSmartPointer<vtkRenderer>::New();
    auto newSagittal = vtkSmartPointer<vtkRenderer>::New();

    renderer_.setRenderers(newAxial, newCoronal, newSagittal);

    // Old renderers should have actors removed
    EXPECT_EQ(renderers_[0]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_EQ(renderers_[1]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_EQ(renderers_[2]->GetActors()->GetNumberOfItems(), 0);

    // New renderers should have actors added
    EXPECT_GT(newAxial->GetActors()->GetNumberOfItems(), 0);
}

// =============================================================================
// Label Manager integration tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, SetLabelManagerNull) {
    // Setting null label manager should not crash
    renderer_.setLabelManager(nullptr);
}

// =============================================================================
// Update without label map tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, UpdateWithoutLabelMapNoOp) {
    // update() without a label map should not crash
    renderer_.update();
}

TEST_F(MPRSegmentationRendererTest, UpdatePlaneWithoutLabelMapNoOp) {
    // updatePlane() without a label map should not crash
    renderer_.updatePlane(MPRPlane::Axial);
    renderer_.updatePlane(MPRPlane::Coronal);
    renderer_.updatePlane(MPRPlane::Sagittal);
}

TEST_F(MPRSegmentationRendererTest, UpdateWithoutLabelMapNoCallback) {
    int callbackCount = 0;
    renderer_.setUpdateCallback([&callbackCount]() {
        ++callbackCount;
    });

    renderer_.update();
    EXPECT_EQ(callbackCount, 0);

    renderer_.updatePlane(MPRPlane::Axial);
    EXPECT_EQ(callbackCount, 0);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST_F(MPRSegmentationRendererTest, EmptyLabelMapTransparent) {
    // Label map with all zeros (background) should work without crash
    auto labelMap = createUniformLabelMap(8, 8, 4, 0);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    renderer_.setLabelMap(labelMap);

    renderer_.setSliceIndex(MPRPlane::Axial, 0);
    renderer_.setSliceIndex(MPRPlane::Coronal, 0);
    renderer_.setSliceIndex(MPRPlane::Sagittal, 0);
}

TEST_F(MPRSegmentationRendererTest, SingleVoxelLabel) {
    // Label map with single labeled voxel at center
    auto labelMap = createSingleVoxelLabelMap(8, 8, 4, 4, 4, 2, 1);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    renderer_.setLabelMap(labelMap);

    // Navigate to the slice containing the label
    renderer_.setSliceIndex(MPRPlane::Axial, 2);   // Z=2
    renderer_.setSliceIndex(MPRPlane::Coronal, 4);  // Y=4
    renderer_.setSliceIndex(MPRPlane::Sagittal, 4); // X=4
}

TEST_F(MPRSegmentationRendererTest, LabelAtBoundary) {
    // Label at image boundary (corner voxel)
    auto labelMap = createSingleVoxelLabelMap(8, 8, 4, 0, 0, 0, 5);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    renderer_.setLabelMap(labelMap);

    renderer_.setSliceIndex(MPRPlane::Axial, 0);
    renderer_.setSliceIndex(MPRPlane::Coronal, 0);
    renderer_.setSliceIndex(MPRPlane::Sagittal, 0);
}

TEST_F(MPRSegmentationRendererTest, MaxLabelValue) {
    // Use label value 255 (maximum)
    auto labelMap = createUniformLabelMap(4, 4, 2, 255);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    renderer_.setLabelMap(labelMap);

    renderer_.setSliceIndex(MPRPlane::Axial, 0);
}

TEST_F(MPRSegmentationRendererTest, SliceIndexClampedToValidRange) {
    // The implementation clamps slice indices via std::clamp
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    renderer_.setLabelMap(labelMap);

    // Out-of-range index should not crash (clamped internally)
    renderer_.setSliceIndex(MPRPlane::Axial, 100);
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Axial), 100);
    // The stored index is 100, but extractSlice clamps it to [0, depth-1]
}

TEST_F(MPRSegmentationRendererTest, NegativeSliceIndex) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    renderer_.setLabelMap(labelMap);

    // Negative index should not crash (clamped to 0 in extractSlice)
    renderer_.setSliceIndex(MPRPlane::Axial, -5);
    EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Axial), -5);
    // extractSlice uses std::clamp to handle this safely
}

// =============================================================================
// Non-isotropic spacing tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, NonIsotropicSpacing) {
    using LabelMapType = MPRSegmentationRenderer::LabelMapType;
    auto labelMap = LabelMapType::New();

    LabelMapType::SizeType size;
    size[0] = 8;
    size[1] = 8;
    size[2] = 4;

    LabelMapType::IndexType start;
    start.Fill(0);

    LabelMapType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    labelMap->SetRegions(region);
    labelMap->Allocate();
    labelMap->FillBuffer(1);

    // Non-isotropic spacing (common in CT)
    LabelMapType::SpacingType spacing;
    spacing[0] = 0.5;
    spacing[1] = 0.5;
    spacing[2] = 2.5;
    labelMap->SetSpacing(spacing);

    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    renderer_.setLabelMap(labelMap);

    // Extraction should work for all planes
    renderer_.setSliceIndex(MPRPlane::Axial, 2);
    renderer_.setSliceIndex(MPRPlane::Coronal, 4);
    renderer_.setSliceIndex(MPRPlane::Sagittal, 4);
}

// =============================================================================
// Callback management tests
// =============================================================================

TEST_F(MPRSegmentationRendererTest, SetUpdateCallbackNull) {
    // Setting null callback should not crash when triggering
    renderer_.setUpdateCallback(nullptr);
    renderer_.setVisible(false);
}

TEST_F(MPRSegmentationRendererTest, CallbackReplacedOnSecondSet) {
    int count1 = 0;
    int count2 = 0;

    renderer_.setUpdateCallback([&count1]() { ++count1; });
    renderer_.setVisible(false);
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 0);

    renderer_.setUpdateCallback([&count2]() { ++count2; });
    renderer_.setVisible(true);
    EXPECT_EQ(count1, 1);  // Old callback not called
    EXPECT_EQ(count2, 1);  // New callback called
}

// =============================================================================
// Full pipeline integration: set label map with renderers and navigate
// =============================================================================

TEST_F(MPRSegmentationRendererTest, FullPipelineAxialNavigation) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4, 3);
    renderer_.setLabelMap(labelMap);

    // Navigate through all axial slices
    for (int z = 0; z < 4; ++z) {
        renderer_.setSliceIndex(MPRPlane::Axial, z);
        EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Axial), z);
    }
}

TEST_F(MPRSegmentationRendererTest, FullPipelineCoronalNavigation) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4, 3);
    renderer_.setLabelMap(labelMap);

    for (int y = 0; y < 8; ++y) {
        renderer_.setSliceIndex(MPRPlane::Coronal, y);
        EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Coronal), y);
    }
}

TEST_F(MPRSegmentationRendererTest, FullPipelineSagittalNavigation) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4, 3);
    renderer_.setLabelMap(labelMap);

    for (int x = 0; x < 8; ++x) {
        renderer_.setSliceIndex(MPRPlane::Sagittal, x);
        EXPECT_EQ(renderer_.getSliceIndex(MPRPlane::Sagittal), x);
    }
}

TEST_F(MPRSegmentationRendererTest, FullPipelineVisibilityToggle) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    renderer_.setVisible(false);
    EXPECT_FALSE(renderer_.isVisible());

    renderer_.setVisible(true);
    EXPECT_TRUE(renderer_.isVisible());
}

TEST_F(MPRSegmentationRendererTest, FullPipelineOpacityChange) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    renderer_.setOpacity(0.3);
    EXPECT_DOUBLE_EQ(renderer_.getOpacity(), 0.3);

    renderer_.setOpacity(0.9);
    EXPECT_DOUBLE_EQ(renderer_.getOpacity(), 0.9);
}

TEST_F(MPRSegmentationRendererTest, FullPipelineLabelColorChange) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    // Change label colors
    LabelColor red(1.0f, 0.0f, 0.0f, 1.0f);
    LabelColor green(0.0f, 1.0f, 0.0f, 0.5f);
    renderer_.setLabelColor(1, red);
    renderer_.setLabelColor(2, green);

    // Should not crash and overlay should reflect new colors
    renderer_.update();
}

TEST_F(MPRSegmentationRendererTest, FullPipelineLabelVisibilityToggle) {
    renderer_.setRenderers(renderers_[0], renderers_[1], renderers_[2]);
    auto labelMap = createTestLabelMap(8, 8, 4);
    renderer_.setLabelMap(labelMap);

    // Hide label 1
    renderer_.setLabelVisible(1, false);

    // Show label 1
    renderer_.setLabelVisible(1, true);

    renderer_.update();
}

// =============================================================================
// Destruction with active renderers
// =============================================================================

TEST_F(MPRSegmentationRendererTest, DestructionCleansUpActors) {
    {
        MPRSegmentationRenderer localRenderer;
        auto labelMap = createTestLabelMap(8, 8, 4);
        localRenderer.setLabelMap(labelMap);
        localRenderer.setRenderers(renderers_[0], renderers_[1], renderers_[2]);

        EXPECT_GT(renderers_[0]->GetActors()->GetNumberOfItems(), 0);
    }
    // After destruction, actors should be removed
    EXPECT_EQ(renderers_[0]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_EQ(renderers_[1]->GetActors()->GetNumberOfItems(), 0);
    EXPECT_EQ(renderers_[2]->GetActors()->GetNumberOfItems(), 0);
}

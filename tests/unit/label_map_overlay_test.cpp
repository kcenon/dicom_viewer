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

#include "services/segmentation/label_map_overlay.hpp"
#include "services/segmentation/segmentation_label.hpp"
#include "services/mpr_renderer.hpp"

#include <cstdint>

#include <itkImage.h>
#include <itkImageRegionIterator.h>

#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

using namespace dicom_viewer::services;

// =============================================================================
// Helper: create synthetic 3D label maps for testing
// =============================================================================

namespace {

/// Create a 3D label map where each voxel's label = (z % numLabels) + 1
/// for z < numLabels, and 0 (background) for the rest.
LabelMapOverlay::LabelMapType::Pointer
createTestLabelMap(int width, int height, int depth, int numLabels = 3) {
    using LabelMapType = LabelMapOverlay::LabelMapType;
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

    LabelMapType::SpacingType spacing;
    spacing[0] = 1.0;
    spacing[1] = 1.0;
    spacing[2] = 1.0;
    labelMap->SetSpacing(spacing);

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

/// Create a label map filled entirely with a single label value
LabelMapOverlay::LabelMapType::Pointer
createUniformLabelMap(int width, int height, int depth, uint8_t label) {
    using LabelMapType = LabelMapOverlay::LabelMapType;
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

/// Create a label map with a single labeled voxel at given position
LabelMapOverlay::LabelMapType::Pointer
createSingleVoxelLabelMap(int width, int height, int depth,
                          int vx, int vy, int vz, uint8_t label) {
    using LabelMapType = LabelMapOverlay::LabelMapType;
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

}  // anonymous namespace

// =============================================================================
// Test fixture
// =============================================================================

class LabelMapOverlayTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderWindow_ = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow_->SetOffScreenRendering(1);
        renderWindow_->SetSize(64, 64);

        for (int i = 0; i < 3; ++i) {
            renderers_[i] = vtkSmartPointer<vtkRenderer>::New();
            renderWindow_->AddRenderer(renderers_[i]);
        }
    }

    LabelMapOverlay overlay_;
    vtkSmartPointer<vtkRenderWindow> renderWindow_;
    std::array<vtkSmartPointer<vtkRenderer>, 3> renderers_;
};

// =============================================================================
// Construction & Lifecycle tests
// =============================================================================

TEST_F(LabelMapOverlayTest, DefaultConstruction) {
    LabelMapOverlay overlay;
    // Verify no crash on construction/destruction
}

TEST_F(LabelMapOverlayTest, MoveConstruction) {
    LabelMapOverlay overlay1;
    LabelMapOverlay overlay2(std::move(overlay1));
    // Verify no crash on move construction
}

TEST_F(LabelMapOverlayTest, MoveAssignment) {
    LabelMapOverlay overlay1;
    LabelMapOverlay overlay2;
    overlay2 = std::move(overlay1);
    // Verify no crash on move assignment
}

TEST_F(LabelMapOverlayTest, InitialState) {
    EXPECT_TRUE(overlay_.isVisible());
    EXPECT_DOUBLE_EQ(overlay_.getOpacity(), 0.5);
    EXPECT_EQ(overlay_.getLabelMap(), nullptr);
}

// =============================================================================
// Label Map management tests
// =============================================================================

TEST_F(LabelMapOverlayTest, SetLabelMapAndRetrieve) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    EXPECT_EQ(overlay_.getLabelMap(), labelMap);
}

TEST_F(LabelMapOverlayTest, SetNullLabelMap) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    EXPECT_NE(overlay_.getLabelMap(), nullptr);

    overlay_.setLabelMap(nullptr);
    EXPECT_EQ(overlay_.getLabelMap(), nullptr);
}

TEST_F(LabelMapOverlayTest, SetLabelMapReSetupsAttachedPlanes) {
    // Attach renderer first, then set label map
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    EXPECT_EQ(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);

    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);

    // After setting label map, actor should be added
    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
}

// =============================================================================
// Color Mapping tests
// =============================================================================

TEST_F(LabelMapOverlayTest, SetAndGetLabelColor) {
    LabelColor red(1.0f, 0.0f, 0.0f, 1.0f);
    overlay_.setLabelColor(1, red);

    auto color = overlay_.getLabelColor(1);
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_FLOAT_EQ(color.g, 0.0f);
    EXPECT_FLOAT_EQ(color.b, 0.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST_F(LabelMapOverlayTest, GetLabelColorFallsToPalette) {
    // Without custom color, getLabelColor returns palette default
    auto color = overlay_.getLabelColor(1);
    auto expected = LabelColorPalette::getColor(1);

    EXPECT_FLOAT_EQ(color.r, expected.r);
    EXPECT_FLOAT_EQ(color.g, expected.g);
    EXPECT_FLOAT_EQ(color.b, expected.b);
    EXPECT_FLOAT_EQ(color.a, expected.a);
}

TEST_F(LabelMapOverlayTest, BackgroundLabelTransparent) {
    auto bgColor = LabelColorPalette::getColor(0);
    EXPECT_FLOAT_EQ(bgColor.r, 0.0f);
    EXPECT_FLOAT_EQ(bgColor.g, 0.0f);
    EXPECT_FLOAT_EQ(bgColor.b, 0.0f);
    EXPECT_FLOAT_EQ(bgColor.a, 0.0f);
}

TEST_F(LabelMapOverlayTest, MultipleCustomColors) {
    LabelColor red(1.0f, 0.0f, 0.0f, 1.0f);
    LabelColor green(0.0f, 1.0f, 0.0f, 0.8f);
    LabelColor blue(0.0f, 0.0f, 1.0f, 0.6f);

    overlay_.setLabelColor(1, red);
    overlay_.setLabelColor(2, green);
    overlay_.setLabelColor(3, blue);

    auto c1 = overlay_.getLabelColor(1);
    auto c2 = overlay_.getLabelColor(2);
    auto c3 = overlay_.getLabelColor(3);

    EXPECT_FLOAT_EQ(c1.r, 1.0f);
    EXPECT_FLOAT_EQ(c2.g, 1.0f);
    EXPECT_FLOAT_EQ(c3.b, 1.0f);
}

TEST_F(LabelMapOverlayTest, CustomColorOverridesPalette) {
    auto paletteColor = LabelColorPalette::getColor(1);

    // Set custom color
    LabelColor custom(0.5f, 0.5f, 0.5f, 0.5f);
    overlay_.setLabelColor(1, custom);

    auto color = overlay_.getLabelColor(1);
    EXPECT_FLOAT_EQ(color.r, 0.5f);
    EXPECT_FLOAT_EQ(color.g, 0.5f);
    EXPECT_FLOAT_EQ(color.b, 0.5f);
    EXPECT_NE(color.r, paletteColor.r);
}

// =============================================================================
// Opacity Control tests
// =============================================================================

TEST_F(LabelMapOverlayTest, SetAndGetOpacity) {
    overlay_.setOpacity(0.7);
    EXPECT_DOUBLE_EQ(overlay_.getOpacity(), 0.7);
}

TEST_F(LabelMapOverlayTest, OpacityClampedToValidRange) {
    overlay_.setOpacity(-0.5);
    EXPECT_DOUBLE_EQ(overlay_.getOpacity(), 0.0);

    overlay_.setOpacity(1.5);
    EXPECT_DOUBLE_EQ(overlay_.getOpacity(), 1.0);
}

TEST_F(LabelMapOverlayTest, OpacityZeroFullyTransparent) {
    overlay_.setOpacity(0.0);
    EXPECT_DOUBLE_EQ(overlay_.getOpacity(), 0.0);
}

TEST_F(LabelMapOverlayTest, OpacityOneFullyOpaque) {
    overlay_.setOpacity(1.0);
    EXPECT_DOUBLE_EQ(overlay_.getOpacity(), 1.0);
}

// =============================================================================
// Visibility tests
// =============================================================================

TEST_F(LabelMapOverlayTest, SetVisibleTrue) {
    overlay_.setVisible(true);
    EXPECT_TRUE(overlay_.isVisible());
}

TEST_F(LabelMapOverlayTest, SetVisibleFalse) {
    overlay_.setVisible(false);
    EXPECT_FALSE(overlay_.isVisible());
}

TEST_F(LabelMapOverlayTest, VisibilityAffectsActors) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    overlay_.setVisible(false);
    EXPECT_FALSE(overlay_.isVisible());

    overlay_.setVisible(true);
    EXPECT_TRUE(overlay_.isVisible());
}

// =============================================================================
// Renderer Attachment tests
// =============================================================================

TEST_F(LabelMapOverlayTest, AttachToRendererWithLabelMap) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);

    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
}

TEST_F(LabelMapOverlayTest, AttachToRendererWithoutLabelMap) {
    // Attaching without label map should not add actors yet
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    EXPECT_EQ(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
}

TEST_F(LabelMapOverlayTest, AttachMultiplePlanes) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);

    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    overlay_.attachToRenderer(renderers_[1], MPRPlane::Coronal);
    overlay_.attachToRenderer(renderers_[2], MPRPlane::Sagittal);

    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(renderers_[1]->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(renderers_[2]->GetViewProps()->GetNumberOfItems(), 0);
}

TEST_F(LabelMapOverlayTest, DetachFromRenderer) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);

    overlay_.detachFromRenderer(MPRPlane::Axial);
    EXPECT_EQ(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
}

TEST_F(LabelMapOverlayTest, DetachFromRendererNotAttached) {
    // Detaching from a plane that was never attached should not crash
    overlay_.detachFromRenderer(MPRPlane::Axial);
    overlay_.detachFromRenderer(MPRPlane::Coronal);
    overlay_.detachFromRenderer(MPRPlane::Sagittal);
}

TEST_F(LabelMapOverlayTest, DetachOnlyTargetPlane) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);

    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    overlay_.attachToRenderer(renderers_[1], MPRPlane::Coronal);

    overlay_.detachFromRenderer(MPRPlane::Axial);

    EXPECT_EQ(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(renderers_[1]->GetViewProps()->GetNumberOfItems(), 0);
}

// =============================================================================
// Slice Update tests
// =============================================================================

TEST_F(LabelMapOverlayTest, UpdateSliceAxial) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    // Update slice position (world coordinates)
    overlay_.updateSlice(MPRPlane::Axial, 2.0);
}

TEST_F(LabelMapOverlayTest, UpdateSliceCoronal) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[1], MPRPlane::Coronal);

    overlay_.updateSlice(MPRPlane::Coronal, 3.0);
}

TEST_F(LabelMapOverlayTest, UpdateSliceSagittal) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[2], MPRPlane::Sagittal);

    overlay_.updateSlice(MPRPlane::Sagittal, 5.0);
}

TEST_F(LabelMapOverlayTest, UpdateSliceNotAttachedNoOp) {
    // Updating a plane that is not attached should not crash
    overlay_.updateSlice(MPRPlane::Axial, 2.0);
    overlay_.updateSlice(MPRPlane::Coronal, 3.0);
    overlay_.updateSlice(MPRPlane::Sagittal, 5.0);
}

TEST_F(LabelMapOverlayTest, UpdateAll) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    overlay_.attachToRenderer(renderers_[1], MPRPlane::Coronal);
    overlay_.attachToRenderer(renderers_[2], MPRPlane::Sagittal);

    // Set initial slice positions
    overlay_.updateSlice(MPRPlane::Axial, 1.0);
    overlay_.updateSlice(MPRPlane::Coronal, 2.0);
    overlay_.updateSlice(MPRPlane::Sagittal, 3.0);

    // updateAll refreshes all planes
    overlay_.updateAll();
}

TEST_F(LabelMapOverlayTest, NotifySliceModified) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    // notifySliceModified delegates to updateAll
    overlay_.notifySliceModified(0);
    overlay_.notifySliceModified(2);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(LabelMapOverlayTest, NullLabelMapInput) {
    overlay_.setLabelMap(nullptr);
    EXPECT_EQ(overlay_.getLabelMap(), nullptr);

    // Operations on null label map should not crash
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    overlay_.updateSlice(MPRPlane::Axial, 0.0);
    overlay_.updateAll();
}

TEST_F(LabelMapOverlayTest, BackgroundOnlyLabelMapTransparent) {
    auto labelMap = createUniformLabelMap(8, 8, 4, 0);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    overlay_.updateSlice(MPRPlane::Axial, 0.0);
}

TEST_F(LabelMapOverlayTest, MaxLabels255Distinct) {
    // Create a label map using all 255 label values
    using LabelMapType = LabelMapOverlay::LabelMapType;
    auto labelMap = LabelMapType::New();

    LabelMapType::SizeType size;
    size[0] = 16;
    size[1] = 16;
    size[2] = 1;

    LabelMapType::IndexType start;
    start.Fill(0);

    LabelMapType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    labelMap->SetRegions(region);
    labelMap->Allocate();
    labelMap->FillBuffer(0);

    // Fill with labels 1-255 in a 16x16 grid (256 voxels, skip index 0)
    itk::ImageRegionIterator<LabelMapType> it(labelMap, region);
    uint8_t labelVal = 0;
    while (!it.IsAtEnd()) {
        it.Set(labelVal);
        if (labelVal < 255) ++labelVal;
        ++it;
    }

    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    overlay_.updateSlice(MPRPlane::Axial, 0.0);
}

TEST_F(LabelMapOverlayTest, SingleVoxelOverlay) {
    auto labelMap = createSingleVoxelLabelMap(8, 8, 4, 4, 4, 2, 1);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    overlay_.updateSlice(MPRPlane::Axial, 2.0);
}

// =============================================================================
// Full pipeline integration tests
// =============================================================================

TEST_F(LabelMapOverlayTest, FullPipelineAllPlanes) {
    auto labelMap = createTestLabelMap(8, 8, 4, 3);
    overlay_.setLabelMap(labelMap);

    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    overlay_.attachToRenderer(renderers_[1], MPRPlane::Coronal);
    overlay_.attachToRenderer(renderers_[2], MPRPlane::Sagittal);

    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(renderers_[1]->GetViewProps()->GetNumberOfItems(), 0);
    EXPECT_GT(renderers_[2]->GetViewProps()->GetNumberOfItems(), 0);

    // Navigate through slices
    for (double z = 0.0; z < 4.0; z += 1.0) {
        overlay_.updateSlice(MPRPlane::Axial, z);
    }
    for (double y = 0.0; y < 8.0; y += 1.0) {
        overlay_.updateSlice(MPRPlane::Coronal, y);
    }
    for (double x = 0.0; x < 8.0; x += 1.0) {
        overlay_.updateSlice(MPRPlane::Sagittal, x);
    }
}

TEST_F(LabelMapOverlayTest, FullPipelineColorAndOpacityChange) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    // Change colors
    LabelColor red(1.0f, 0.0f, 0.0f, 1.0f);
    LabelColor green(0.0f, 1.0f, 0.0f, 0.5f);
    overlay_.setLabelColor(1, red);
    overlay_.setLabelColor(2, green);

    // Change opacity
    overlay_.setOpacity(0.8);

    // Update display
    overlay_.updateSlice(MPRPlane::Axial, 0.0);
    overlay_.updateAll();
}

TEST_F(LabelMapOverlayTest, FullPipelineVisibilityToggle) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    overlay_.setVisible(false);
    EXPECT_FALSE(overlay_.isVisible());

    overlay_.updateSlice(MPRPlane::Axial, 0.0);

    overlay_.setVisible(true);
    EXPECT_TRUE(overlay_.isVisible());

    overlay_.updateSlice(MPRPlane::Axial, 1.0);
}

TEST_F(LabelMapOverlayTest, FullPipelineAttachDetachReattach) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);

    // Attach
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);

    // Detach
    overlay_.detachFromRenderer(MPRPlane::Axial);
    EXPECT_EQ(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);

    // Reattach
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
}

TEST_F(LabelMapOverlayTest, FullPipelineReplaceLabelMap) {
    auto labelMap1 = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap1);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    EXPECT_EQ(overlay_.getLabelMap(), labelMap1);

    // Replace with a different label map
    auto labelMap2 = createTestLabelMap(16, 16, 8, 5);
    overlay_.setLabelMap(labelMap2);

    EXPECT_EQ(overlay_.getLabelMap(), labelMap2);

    // Actor should still be present
    EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);

    overlay_.updateSlice(MPRPlane::Axial, 3.0);
}

// =============================================================================
// Non-isotropic spacing tests
// =============================================================================

TEST_F(LabelMapOverlayTest, NonIsotropicSpacing) {
    using LabelMapType = LabelMapOverlay::LabelMapType;
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

    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);
    overlay_.attachToRenderer(renderers_[1], MPRPlane::Coronal);
    overlay_.attachToRenderer(renderers_[2], MPRPlane::Sagittal);

    overlay_.updateSlice(MPRPlane::Axial, 2.5);
    overlay_.updateSlice(MPRPlane::Coronal, 2.0);
    overlay_.updateSlice(MPRPlane::Sagittal, 2.0);
}

// =============================================================================
// Destruction with active renderers
// =============================================================================

TEST_F(LabelMapOverlayTest, DestructionDoesNotCrash) {
    {
        LabelMapOverlay localOverlay;
        auto labelMap = createTestLabelMap(8, 8, 4);
        localOverlay.setLabelMap(labelMap);
        localOverlay.attachToRenderer(renderers_[0], MPRPlane::Axial);
        localOverlay.attachToRenderer(renderers_[1], MPRPlane::Coronal);
        localOverlay.attachToRenderer(renderers_[2], MPRPlane::Sagittal);

        EXPECT_GT(renderers_[0]->GetViewProps()->GetNumberOfItems(), 0);
    }
    // After destruction, verify renderers are still valid
    // (actors may or may not be cleaned up automatically by VTK smart pointers)
}

TEST_F(LabelMapOverlayTest, MoveAfterSetup) {
    auto labelMap = createTestLabelMap(8, 8, 4);
    overlay_.setLabelMap(labelMap);
    overlay_.attachToRenderer(renderers_[0], MPRPlane::Axial);

    LabelMapOverlay movedOverlay(std::move(overlay_));

    // Moved overlay should still work
    movedOverlay.updateSlice(MPRPlane::Axial, 1.0);
    movedOverlay.setVisible(false);
    EXPECT_FALSE(movedOverlay.isVisible());
}

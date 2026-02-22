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

#include "services/render/asc_view_controller.hpp"

#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

using namespace dicom_viewer::services;

namespace {

vtkSmartPointer<vtkImageData> createTestVolume(int dimX = 16, int dimY = 16, int dimZ = 16)
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(dimX, dimY, dimZ);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);

    auto* ptr = static_cast<float*>(image->GetScalarPointer());
    int total = dimX * dimY * dimZ;
    for (int i = 0; i < total; ++i) {
        ptr[i] = static_cast<float>(i % 256);
    }
    return image;
}

}  // anonymous namespace

// =============================================================================
// Construction and defaults
// =============================================================================

TEST(AscViewControllerTest, DefaultConstruction) {
    AscViewController ctrl;
    EXPECT_FALSE(ctrl.isVisible());
    EXPECT_EQ(ctrl.getInputData(), nullptr);
    EXPECT_EQ(ctrl.getRenderer(), nullptr);
    EXPECT_EQ(ctrl.axialSlice(), 0);
    EXPECT_EQ(ctrl.coronalSlice(), 0);
    EXPECT_EQ(ctrl.sagittalSlice(), 0);
}

TEST(AscViewControllerTest, MoveConstruction) {
    AscViewController ctrl;
    ctrl.setVisible(true);
    EXPECT_TRUE(ctrl.isVisible());

    AscViewController moved(std::move(ctrl));
    EXPECT_TRUE(moved.isVisible());
}

TEST(AscViewControllerTest, Dimensions_NoData) {
    AscViewController ctrl;
    auto dims = ctrl.dimensions();
    EXPECT_EQ(dims[0], 0);
    EXPECT_EQ(dims[1], 0);
    EXPECT_EQ(dims[2], 0);
}

// =============================================================================
// Input data and auto-centering
// =============================================================================

TEST(AscViewControllerTest, SetInputData_CentersSlices) {
    AscViewController ctrl;
    auto vol = createTestVolume(20, 30, 40);
    ctrl.setInputData(vol);

    EXPECT_EQ(ctrl.getInputData(), vol);

    auto dims = ctrl.dimensions();
    EXPECT_EQ(dims[0], 20);
    EXPECT_EQ(dims[1], 30);
    EXPECT_EQ(dims[2], 40);

    // Slices should be centered at dim/2
    EXPECT_EQ(ctrl.axialSlice(), 20);    // 40/2
    EXPECT_EQ(ctrl.coronalSlice(), 15);  // 30/2
    EXPECT_EQ(ctrl.sagittalSlice(), 10); // 20/2
}

// =============================================================================
// Visibility
// =============================================================================

TEST(AscViewControllerTest, Visibility_Toggle) {
    AscViewController ctrl;
    EXPECT_FALSE(ctrl.isVisible());

    ctrl.setVisible(true);
    EXPECT_TRUE(ctrl.isVisible());

    ctrl.setVisible(false);
    EXPECT_FALSE(ctrl.isVisible());
}

// =============================================================================
// Slice positioning
// =============================================================================

TEST(AscViewControllerTest, SlicePositioning_Individual) {
    AscViewController ctrl;
    auto vol = createTestVolume();
    ctrl.setInputData(vol);

    ctrl.setAxialSlice(5);
    EXPECT_EQ(ctrl.axialSlice(), 5);

    ctrl.setCoronalSlice(10);
    EXPECT_EQ(ctrl.coronalSlice(), 10);

    ctrl.setSagittalSlice(3);
    EXPECT_EQ(ctrl.sagittalSlice(), 3);
}

TEST(AscViewControllerTest, SlicePositioning_AllAtOnce) {
    AscViewController ctrl;
    auto vol = createTestVolume();
    ctrl.setInputData(vol);

    ctrl.setSlicePositions(7, 11, 2);
    EXPECT_EQ(ctrl.axialSlice(), 7);
    EXPECT_EQ(ctrl.coronalSlice(), 11);
    EXPECT_EQ(ctrl.sagittalSlice(), 2);
}

// =============================================================================
// Window/Level
// =============================================================================

TEST(AscViewControllerTest, WindowLevel_Default) {
    AscViewController ctrl;
    auto [w, c] = ctrl.getWindowLevel();
    EXPECT_DOUBLE_EQ(w, 400.0);
    EXPECT_DOUBLE_EQ(c, 40.0);
}

TEST(AscViewControllerTest, WindowLevel_Set) {
    AscViewController ctrl;
    ctrl.setWindowLevel(1500.0, 300.0);
    auto [w, c] = ctrl.getWindowLevel();
    EXPECT_DOUBLE_EQ(w, 1500.0);
    EXPECT_DOUBLE_EQ(c, 300.0);
}

// =============================================================================
// Opacity
// =============================================================================

TEST(AscViewControllerTest, Opacity_Default) {
    AscViewController ctrl;
    EXPECT_DOUBLE_EQ(ctrl.getOpacity(), 1.0);
}

TEST(AscViewControllerTest, Opacity_Set) {
    AscViewController ctrl;
    ctrl.setOpacity(0.5);
    EXPECT_DOUBLE_EQ(ctrl.getOpacity(), 0.5);
}

// =============================================================================
// Renderer integration
// =============================================================================

// Note: vtkImageSlice + vtkImageSliceMapper require an OpenGL context.
// Adding them to a bare vtkRenderer without a render window crashes VTK.
// Renderer integration tests are deferred to integration test suite.
// Here we test only state management (getRenderer pointer tracking).

TEST(AscViewControllerTest, Renderer_GetInitiallyNull) {
    AscViewController ctrl;
    EXPECT_EQ(ctrl.getRenderer(), nullptr);
}

TEST(AscViewControllerTest, DataWithoutRenderer) {
    AscViewController ctrl;
    auto vol = createTestVolume();

    ctrl.setInputData(vol);
    EXPECT_EQ(ctrl.getInputData(), vol);
    EXPECT_EQ(ctrl.getRenderer(), nullptr);

    // All state operations should work without renderer
    ctrl.setVisible(true);
    EXPECT_TRUE(ctrl.isVisible());
    ctrl.setSlicePositions(5, 5, 5);
    EXPECT_EQ(ctrl.axialSlice(), 5);
}

// =============================================================================
// Integration with Display3DController
// =============================================================================

TEST(AscViewControllerTest, FullWorkflow) {
    AscViewController ctrl;
    auto vol = createTestVolume(32, 32, 32);

    ctrl.setInputData(vol);
    ctrl.setWindowLevel(2000.0, 400.0);
    ctrl.setOpacity(0.8);

    auto [w, c] = ctrl.getWindowLevel();
    EXPECT_DOUBLE_EQ(w, 2000.0);
    EXPECT_DOUBLE_EQ(c, 400.0);
    EXPECT_DOUBLE_EQ(ctrl.getOpacity(), 0.8);

    // Initially hidden
    EXPECT_FALSE(ctrl.isVisible());

    // Show ASC planes
    ctrl.setVisible(true);
    EXPECT_TRUE(ctrl.isVisible());

    // Position planes
    ctrl.setSlicePositions(16, 16, 16);
    EXPECT_EQ(ctrl.axialSlice(), 16);
    EXPECT_EQ(ctrl.coronalSlice(), 16);
    EXPECT_EQ(ctrl.sagittalSlice(), 16);

    // Hide again
    ctrl.setVisible(false);
    EXPECT_FALSE(ctrl.isVisible());
}

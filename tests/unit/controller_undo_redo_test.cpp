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

#include <itkImage.h>

#include "services/segmentation/manual_segmentation_controller.hpp"

using namespace dicom_viewer::services;

namespace {

/// Create a controller initialized with a 10x10x1 label map
ManualSegmentationController createTestController() {
    ManualSegmentationController ctrl;
    auto result = ctrl.initializeLabelMap(10, 10, 1);
    // Ensure initialization succeeded
    if (!result) {
        throw std::runtime_error("Failed to initialize label map");
    }
    return ctrl;
}

/// Read a voxel from the controller's label map
uint8_t readVoxel(ManualSegmentationController& ctrl, int x, int y, int z = 0) {
    auto labelMap = ctrl.getLabelMap();
    ManualSegmentationController::LabelMapType::IndexType idx;
    idx[0] = x;
    idx[1] = y;
    idx[2] = z;
    return labelMap->GetPixel(idx);
}

} // anonymous namespace

// =============================================================================
// Brush stroke undo/redo
// =============================================================================

TEST(ControllerUndoRedoTest, BrushStrokeUndoRestoresVoxels) {
    auto ctrl = createTestController();
    ctrl.setActiveTool(SegmentationTool::Brush);
    ctrl.setBrushSize(1);
    ctrl.setBrushShape(BrushShape::Square);
    ctrl.setActiveLabel(1);

    // Draw at (5, 5)
    ctrl.onMousePress(Point2D{5, 5}, 0);
    ctrl.onMouseRelease(Point2D{5, 5}, 0);

    EXPECT_EQ(readVoxel(ctrl, 5, 5), 1);
    EXPECT_TRUE(ctrl.canUndo());
    EXPECT_FALSE(ctrl.canRedo());

    // Undo
    EXPECT_TRUE(ctrl.undo());
    EXPECT_EQ(readVoxel(ctrl, 5, 5), 0);
    EXPECT_FALSE(ctrl.canUndo());
    EXPECT_TRUE(ctrl.canRedo());

    // Redo
    EXPECT_TRUE(ctrl.redo());
    EXPECT_EQ(readVoxel(ctrl, 5, 5), 1);
    EXPECT_TRUE(ctrl.canUndo());
    EXPECT_FALSE(ctrl.canRedo());
}

// =============================================================================
// Eraser undo/redo
// =============================================================================

TEST(ControllerUndoRedoTest, EraserUndoRestoresVoxels) {
    auto ctrl = createTestController();
    ctrl.setActiveTool(SegmentationTool::Brush);
    ctrl.setBrushSize(1);
    ctrl.setBrushShape(BrushShape::Square);
    ctrl.setActiveLabel(2);

    // Paint label 2 at (3, 3)
    ctrl.onMousePress(Point2D{3, 3}, 0);
    ctrl.onMouseRelease(Point2D{3, 3}, 0);
    EXPECT_EQ(readVoxel(ctrl, 3, 3), 2);

    // Erase at (3, 3)
    ctrl.setActiveTool(SegmentationTool::Eraser);
    ctrl.onMousePress(Point2D{3, 3}, 0);
    ctrl.onMouseRelease(Point2D{3, 3}, 0);
    EXPECT_EQ(readVoxel(ctrl, 3, 3), 0);

    // Undo eraser → label 2 restored
    EXPECT_TRUE(ctrl.undo());
    EXPECT_EQ(readVoxel(ctrl, 3, 3), 2);
}

// =============================================================================
// Fill undo/redo
// =============================================================================

TEST(ControllerUndoRedoTest, FillUndoRestoresRegion) {
    auto ctrl = createTestController();
    ctrl.setActiveTool(SegmentationTool::Fill);
    ctrl.setActiveLabel(3);

    // Fill from (0, 0) — fills entire blank label map
    ctrl.onMousePress(Point2D{0, 0}, 0);

    // Verify fill happened
    EXPECT_EQ(readVoxel(ctrl, 0, 0), 3);
    EXPECT_EQ(readVoxel(ctrl, 9, 9), 3);
    EXPECT_TRUE(ctrl.canUndo());

    // Undo → entire map back to 0
    EXPECT_TRUE(ctrl.undo());
    EXPECT_EQ(readVoxel(ctrl, 0, 0), 0);
    EXPECT_EQ(readVoxel(ctrl, 9, 9), 0);
}

// =============================================================================
// Multiple undo/redo
// =============================================================================

TEST(ControllerUndoRedoTest, MultipleUndoRedo) {
    auto ctrl = createTestController();
    ctrl.setActiveTool(SegmentationTool::Brush);
    ctrl.setBrushSize(1);
    ctrl.setBrushShape(BrushShape::Square);
    ctrl.setActiveLabel(1);

    // Stroke 1: draw at (2, 2)
    ctrl.onMousePress(Point2D{2, 2}, 0);
    ctrl.onMouseRelease(Point2D{2, 2}, 0);

    // Stroke 2: draw at (4, 4)
    ctrl.onMousePress(Point2D{4, 4}, 0);
    ctrl.onMouseRelease(Point2D{4, 4}, 0);

    EXPECT_EQ(readVoxel(ctrl, 2, 2), 1);
    EXPECT_EQ(readVoxel(ctrl, 4, 4), 1);

    // Undo stroke 2
    EXPECT_TRUE(ctrl.undo());
    EXPECT_EQ(readVoxel(ctrl, 2, 2), 1);
    EXPECT_EQ(readVoxel(ctrl, 4, 4), 0);

    // Undo stroke 1
    EXPECT_TRUE(ctrl.undo());
    EXPECT_EQ(readVoxel(ctrl, 2, 2), 0);
    EXPECT_EQ(readVoxel(ctrl, 4, 4), 0);

    // Redo stroke 1
    EXPECT_TRUE(ctrl.redo());
    EXPECT_EQ(readVoxel(ctrl, 2, 2), 1);
    EXPECT_EQ(readVoxel(ctrl, 4, 4), 0);

    // Redo stroke 2
    EXPECT_TRUE(ctrl.redo());
    EXPECT_EQ(readVoxel(ctrl, 2, 2), 1);
    EXPECT_EQ(readVoxel(ctrl, 4, 4), 1);
}

// =============================================================================
// New command clears redo stack
// =============================================================================

TEST(ControllerUndoRedoTest, NewCommandClearsRedoStack) {
    auto ctrl = createTestController();
    ctrl.setActiveTool(SegmentationTool::Brush);
    ctrl.setBrushSize(1);
    ctrl.setBrushShape(BrushShape::Square);
    ctrl.setActiveLabel(1);

    // Draw, undo, draw again → redo gone
    ctrl.onMousePress(Point2D{1, 1}, 0);
    ctrl.onMouseRelease(Point2D{1, 1}, 0);

    EXPECT_TRUE(ctrl.undo());
    EXPECT_TRUE(ctrl.canRedo());

    // New stroke at (2, 2) should clear redo
    ctrl.onMousePress(Point2D{2, 2}, 0);
    ctrl.onMouseRelease(Point2D{2, 2}, 0);

    EXPECT_FALSE(ctrl.canRedo());
}

// =============================================================================
// clearAll resets command stack
// =============================================================================

TEST(ControllerUndoRedoTest, ClearAllResetsCommandStack) {
    auto ctrl = createTestController();
    ctrl.setActiveTool(SegmentationTool::Brush);
    ctrl.setBrushSize(1);
    ctrl.setBrushShape(BrushShape::Square);
    ctrl.setActiveLabel(1);

    ctrl.onMousePress(Point2D{5, 5}, 0);
    ctrl.onMouseRelease(Point2D{5, 5}, 0);
    EXPECT_TRUE(ctrl.canUndo());

    ctrl.clearAll();
    // clearAll is an undoable operation (users can recover from accidental clear)
    EXPECT_TRUE(ctrl.canUndo());
    EXPECT_FALSE(ctrl.canRedo());
}

// =============================================================================
// No-op when nothing to undo/redo
// =============================================================================

TEST(ControllerUndoRedoTest, UndoRedoReturnFalseWhenEmpty) {
    auto ctrl = createTestController();
    EXPECT_FALSE(ctrl.canUndo());
    EXPECT_FALSE(ctrl.canRedo());
    EXPECT_FALSE(ctrl.undo());
    EXPECT_FALSE(ctrl.redo());
}

// =============================================================================
// UndoRedo callback
// =============================================================================

TEST(ControllerUndoRedoTest, UndoRedoCallbackNotified) {
    auto ctrl = createTestController();

    bool lastCanUndo = false;
    bool lastCanRedo = false;
    int callCount = 0;
    ctrl.setUndoRedoCallback([&](bool canUndoVal, bool canRedoVal) {
        lastCanUndo = canUndoVal;
        lastCanRedo = canRedoVal;
        ++callCount;
    });

    ctrl.setActiveTool(SegmentationTool::Brush);
    ctrl.setBrushSize(1);
    ctrl.setBrushShape(BrushShape::Square);
    ctrl.setActiveLabel(1);

    // Draw → callback should fire
    ctrl.onMousePress(Point2D{5, 5}, 0);
    ctrl.onMouseRelease(Point2D{5, 5}, 0);
    EXPECT_GT(callCount, 0);
    EXPECT_TRUE(lastCanUndo);
    EXPECT_FALSE(lastCanRedo);

    int prevCount = callCount;
    // Undo → callback should fire
    ctrl.undo();
    EXPECT_GT(callCount, prevCount);
    EXPECT_FALSE(lastCanUndo);
    EXPECT_TRUE(lastCanRedo);
}

// =============================================================================
// Brush stroke with drag records all changes
// =============================================================================

TEST(ControllerUndoRedoTest, BrushDragStrokeUndoAll) {
    auto ctrl = createTestController();
    ctrl.setActiveTool(SegmentationTool::Brush);
    ctrl.setBrushSize(1);
    ctrl.setBrushShape(BrushShape::Square);
    ctrl.setActiveLabel(1);

    // Drag stroke: press at (1,1), move to (3,1), release
    ctrl.onMousePress(Point2D{1, 1}, 0);
    ctrl.onMouseMove(Point2D{2, 1}, 0);
    ctrl.onMouseMove(Point2D{3, 1}, 0);
    ctrl.onMouseRelease(Point2D{3, 1}, 0);

    EXPECT_EQ(readVoxel(ctrl, 1, 1), 1);
    EXPECT_EQ(readVoxel(ctrl, 2, 1), 1);
    EXPECT_EQ(readVoxel(ctrl, 3, 1), 1);

    // Single undo reverts entire stroke
    EXPECT_TRUE(ctrl.undo());
    EXPECT_EQ(readVoxel(ctrl, 1, 1), 0);
    EXPECT_EQ(readVoxel(ctrl, 2, 1), 0);
    EXPECT_EQ(readVoxel(ctrl, 3, 1), 0);
}

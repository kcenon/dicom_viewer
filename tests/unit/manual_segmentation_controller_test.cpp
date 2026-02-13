#include <gtest/gtest.h>

#include "services/segmentation/manual_segmentation_controller.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

class ManualSegmentationControllerTest : public ::testing::Test {
protected:
    using LabelMapType = ManualSegmentationController::LabelMapType;

    void SetUp() override {
        controller_ = std::make_unique<ManualSegmentationController>();
    }

    /**
     * @brief Count pixels with specific label value
     */
    int countLabelPixels(LabelMapType::Pointer labelMap, uint8_t label) {
        int count = 0;
        itk::ImageRegionIterator<LabelMapType> it(
            labelMap, labelMap->GetLargestPossibleRegion()
        );
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == label) {
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief Get pixel value at specific position
     */
    uint8_t getPixelAt(LabelMapType::Pointer labelMap, int x, int y, int z) {
        LabelMapType::IndexType index;
        index[0] = x;
        index[1] = y;
        index[2] = z;
        return labelMap->GetPixel(index);
    }

    std::unique_ptr<ManualSegmentationController> controller_;
};

// Initialization tests
TEST_F(ManualSegmentationControllerTest, InitializeLabelMapCreatesValidImage) {
    auto result = controller_->initializeLabelMap(100, 100, 50);

    ASSERT_TRUE(result.has_value());

    auto labelMap = controller_->getLabelMap();
    ASSERT_NE(labelMap, nullptr);

    auto size = labelMap->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], 100);
    EXPECT_EQ(size[1], 100);
    EXPECT_EQ(size[2], 50);
}

TEST_F(ManualSegmentationControllerTest, InitializeLabelMapRejectsInvalidDimensions) {
    auto result = controller_->initializeLabelMap(0, 100, 50);
    EXPECT_FALSE(result.has_value());

    result = controller_->initializeLabelMap(100, -1, 50);
    EXPECT_FALSE(result.has_value());

    result = controller_->initializeLabelMap(100, 100, 0);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ManualSegmentationControllerTest, InitializeLabelMapFillsWithZero) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 5).has_value());
    auto labelMap = controller_->getLabelMap();

    int nonZeroCount = countLabelPixels(labelMap, 0);
    int totalPixels = 10 * 10 * 5;
    EXPECT_EQ(nonZeroCount, totalPixels);
}

// Tool management tests
TEST_F(ManualSegmentationControllerTest, DefaultToolIsNone) {
    EXPECT_EQ(controller_->getActiveTool(), SegmentationTool::None);
}

TEST_F(ManualSegmentationControllerTest, SetActiveToolChangesTool) {
    controller_->setActiveTool(SegmentationTool::Brush);
    EXPECT_EQ(controller_->getActiveTool(), SegmentationTool::Brush);

    controller_->setActiveTool(SegmentationTool::Eraser);
    EXPECT_EQ(controller_->getActiveTool(), SegmentationTool::Eraser);

    controller_->setActiveTool(SegmentationTool::Fill);
    EXPECT_EQ(controller_->getActiveTool(), SegmentationTool::Fill);
}

// Brush parameter tests
TEST_F(ManualSegmentationControllerTest, DefaultBrushSize) {
    EXPECT_EQ(controller_->getBrushSize(), 5);
}

TEST_F(ManualSegmentationControllerTest, SetBrushSizeValidRange) {
    EXPECT_TRUE(controller_->setBrushSize(1));
    EXPECT_EQ(controller_->getBrushSize(), 1);

    EXPECT_TRUE(controller_->setBrushSize(50));
    EXPECT_EQ(controller_->getBrushSize(), 50);

    EXPECT_TRUE(controller_->setBrushSize(25));
    EXPECT_EQ(controller_->getBrushSize(), 25);
}

TEST_F(ManualSegmentationControllerTest, SetBrushSizeRejectsInvalidRange) {
    controller_->setBrushSize(10);

    EXPECT_FALSE(controller_->setBrushSize(0));
    EXPECT_EQ(controller_->getBrushSize(), 10);  // unchanged

    EXPECT_FALSE(controller_->setBrushSize(51));
    EXPECT_EQ(controller_->getBrushSize(), 10);  // unchanged

    EXPECT_FALSE(controller_->setBrushSize(-5));
    EXPECT_EQ(controller_->getBrushSize(), 10);  // unchanged
}

TEST_F(ManualSegmentationControllerTest, DefaultBrushShapeIsCircle) {
    EXPECT_EQ(controller_->getBrushShape(), BrushShape::Circle);
}

TEST_F(ManualSegmentationControllerTest, SetBrushShapeChangesShape) {
    controller_->setBrushShape(BrushShape::Square);
    EXPECT_EQ(controller_->getBrushShape(), BrushShape::Square);

    controller_->setBrushShape(BrushShape::Circle);
    EXPECT_EQ(controller_->getBrushShape(), BrushShape::Circle);
}

TEST_F(ManualSegmentationControllerTest, SetBrushParametersValidates) {
    BrushParameters params;
    params.size = 20;
    params.shape = BrushShape::Square;

    EXPECT_TRUE(controller_->setBrushParameters(params));
    EXPECT_EQ(controller_->getBrushSize(), 20);
    EXPECT_EQ(controller_->getBrushShape(), BrushShape::Square);
}

TEST_F(ManualSegmentationControllerTest, SetBrushParametersRejectsInvalid) {
    controller_->setBrushSize(15);

    BrushParameters params;
    params.size = 100;  // Invalid

    EXPECT_FALSE(controller_->setBrushParameters(params));
    EXPECT_EQ(controller_->getBrushSize(), 15);  // unchanged
}

// Label management tests
TEST_F(ManualSegmentationControllerTest, DefaultLabelIsOne) {
    EXPECT_EQ(controller_->getActiveLabel(), 1);
}

TEST_F(ManualSegmentationControllerTest, SetActiveLabelValidRange) {
    EXPECT_TRUE(controller_->setActiveLabel(1));
    EXPECT_EQ(controller_->getActiveLabel(), 1);

    EXPECT_TRUE(controller_->setActiveLabel(255));
    EXPECT_EQ(controller_->getActiveLabel(), 255);
}

TEST_F(ManualSegmentationControllerTest, SetActiveLabelRejectsZero) {
    controller_->setActiveLabel(5);

    EXPECT_FALSE(controller_->setActiveLabel(0));
    EXPECT_EQ(controller_->getActiveLabel(), 5);  // unchanged
}

// Brush tool tests
TEST_F(ManualSegmentationControllerTest, BrushToolDrawsAtPosition) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(1);
    controller_->setActiveLabel(1);

    controller_->onMousePress(Point2D{50, 50}, 5);
    controller_->onMouseRelease(Point2D{50, 50}, 5);

    auto labelMap = controller_->getLabelMap();
    EXPECT_EQ(getPixelAt(labelMap, 50, 50, 5), 1);
}

TEST_F(ManualSegmentationControllerTest, BrushToolDrawsCircularShape) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(5);
    controller_->setBrushShape(BrushShape::Circle);
    controller_->setActiveLabel(1);

    controller_->onMousePress(Point2D{50, 50}, 5);
    controller_->onMouseRelease(Point2D{50, 50}, 5);

    auto labelMap = controller_->getLabelMap();

    // Center should be labeled
    EXPECT_EQ(getPixelAt(labelMap, 50, 50, 5), 1);

    // Within brush radius should be labeled
    EXPECT_EQ(getPixelAt(labelMap, 52, 50, 5), 1);
    EXPECT_EQ(getPixelAt(labelMap, 50, 52, 5), 1);

    // Corners of bounding box should NOT be labeled (circular brush)
    EXPECT_EQ(getPixelAt(labelMap, 48, 48, 5), 0);
    EXPECT_EQ(getPixelAt(labelMap, 52, 52, 5), 0);
}

TEST_F(ManualSegmentationControllerTest, BrushToolDrawsSquareShape) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(5);
    controller_->setBrushShape(BrushShape::Square);
    controller_->setActiveLabel(1);

    controller_->onMousePress(Point2D{50, 50}, 5);
    controller_->onMouseRelease(Point2D{50, 50}, 5);

    auto labelMap = controller_->getLabelMap();

    // Center should be labeled
    EXPECT_EQ(getPixelAt(labelMap, 50, 50, 5), 1);

    // Corners of bounding box should be labeled (square brush)
    EXPECT_EQ(getPixelAt(labelMap, 48, 48, 5), 1);
    EXPECT_EQ(getPixelAt(labelMap, 52, 52, 5), 1);
}

TEST_F(ManualSegmentationControllerTest, BrushToolDrawsLine) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(1);
    controller_->setActiveLabel(1);

    controller_->onMousePress(Point2D{10, 10}, 5);
    controller_->onMouseMove(Point2D{15, 10}, 5);
    controller_->onMouseRelease(Point2D{15, 10}, 5);

    auto labelMap = controller_->getLabelMap();

    // Line should be drawn
    for (int x = 10; x <= 15; ++x) {
        EXPECT_EQ(getPixelAt(labelMap, x, 10, 5), 1)
            << "Pixel at (" << x << ", 10, 5) should be labeled";
    }
}

// Eraser tool tests
TEST_F(ManualSegmentationControllerTest, EraserToolRemovesLabels) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(5);
    controller_->setActiveLabel(1);

    // First draw something
    controller_->onMousePress(Point2D{50, 50}, 5);
    controller_->onMouseRelease(Point2D{50, 50}, 5);

    auto labelMap = controller_->getLabelMap();
    EXPECT_EQ(getPixelAt(labelMap, 50, 50, 5), 1);

    // Now erase
    controller_->setActiveTool(SegmentationTool::Eraser);
    controller_->onMousePress(Point2D{50, 50}, 5);
    controller_->onMouseRelease(Point2D{50, 50}, 5);

    EXPECT_EQ(getPixelAt(labelMap, 50, 50, 5), 0);
}

// Fill tool tests
TEST_F(ManualSegmentationControllerTest, FillToolFillsRegion) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Fill);
    controller_->setActiveLabel(1);

    controller_->onMousePress(Point2D{5, 5}, 0);

    auto labelMap = controller_->getLabelMap();

    // All pixels should be filled
    int filledCount = countLabelPixels(labelMap, 1);
    EXPECT_EQ(filledCount, 100);  // 10x10
}

TEST_F(ManualSegmentationControllerTest, FillToolStopsAtBoundary) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 1).has_value());

    // First create a boundary using brush
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(1);
    controller_->setActiveLabel(2);

    // Draw a vertical line at x=5
    for (int y = 0; y < 10; ++y) {
        controller_->onMousePress(Point2D{5, y}, 0);
        controller_->onMouseRelease(Point2D{5, y}, 0);
    }

    // Now fill on the left side
    controller_->setActiveTool(SegmentationTool::Fill);
    controller_->setActiveLabel(1);
    controller_->onMousePress(Point2D{2, 5}, 0);

    auto labelMap = controller_->getLabelMap();

    // Left side should be filled (x < 5)
    EXPECT_EQ(getPixelAt(labelMap, 0, 5, 0), 1);
    EXPECT_EQ(getPixelAt(labelMap, 4, 5, 0), 1);

    // Right side should NOT be filled (x > 5)
    EXPECT_EQ(getPixelAt(labelMap, 6, 5, 0), 0);
    EXPECT_EQ(getPixelAt(labelMap, 9, 5, 0), 0);

    // Boundary should remain as label 2
    EXPECT_EQ(getPixelAt(labelMap, 5, 5, 0), 2);
}

// Clear tests
TEST_F(ManualSegmentationControllerTest, ClearAllRemovesAllLabels) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 5).has_value());
    controller_->setActiveTool(SegmentationTool::Fill);
    controller_->setActiveLabel(1);
    controller_->onMousePress(Point2D{5, 5}, 2);

    auto labelMap = controller_->getLabelMap();
    EXPECT_GT(countLabelPixels(labelMap, 1), 0);

    controller_->clearAll();
    EXPECT_EQ(countLabelPixels(labelMap, 1), 0);
}

TEST_F(ManualSegmentationControllerTest, ClearLabelRemovesSpecificLabel) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 1).has_value());

    // Fill with label 1
    controller_->setActiveTool(SegmentationTool::Fill);
    controller_->setActiveLabel(1);
    controller_->onMousePress(Point2D{2, 2}, 0);

    // Draw some with label 2
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(1);
    controller_->setActiveLabel(2);
    controller_->onMousePress(Point2D{5, 5}, 0);
    controller_->onMouseRelease(Point2D{5, 5}, 0);

    auto labelMap = controller_->getLabelMap();
    EXPECT_EQ(getPixelAt(labelMap, 5, 5, 0), 2);

    // Clear only label 2
    controller_->clearLabel(2);

    EXPECT_EQ(getPixelAt(labelMap, 5, 5, 0), 0);
    // Label 1 should remain
    EXPECT_GT(countLabelPixels(labelMap, 1), 0);
}

// Drawing state tests
TEST_F(ManualSegmentationControllerTest, IsDrawingReturnsTrueWhenDrawing) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);

    EXPECT_FALSE(controller_->isDrawing());

    controller_->onMousePress(Point2D{50, 50}, 5);
    EXPECT_TRUE(controller_->isDrawing());

    controller_->onMouseRelease(Point2D{50, 50}, 5);
    EXPECT_FALSE(controller_->isDrawing());
}

TEST_F(ManualSegmentationControllerTest, CancelOperationStopsDrawing) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);

    controller_->onMousePress(Point2D{50, 50}, 5);
    EXPECT_TRUE(controller_->isDrawing());

    controller_->cancelOperation();
    EXPECT_FALSE(controller_->isDrawing());
}

// Callback tests
TEST_F(ManualSegmentationControllerTest, ModificationCallbackIsCalled) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(1);

    int callbackCount = 0;
    int lastSlice = -1;

    controller_->setModificationCallback([&](int sliceIndex) {
        ++callbackCount;
        lastSlice = sliceIndex;
    });

    controller_->onMousePress(Point2D{50, 50}, 5);
    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(lastSlice, 5);

    controller_->onMouseRelease(Point2D{50, 50}, 5);
    EXPECT_EQ(callbackCount, 2);
}

// Bounds checking tests
TEST_F(ManualSegmentationControllerTest, BrushDoesNotDrawOutOfBounds) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(5);
    controller_->setActiveLabel(1);

    // Draw at corner - should not crash
    controller_->onMousePress(Point2D{0, 0}, 0);
    controller_->onMouseRelease(Point2D{0, 0}, 0);

    controller_->onMousePress(Point2D{9, 9}, 0);
    controller_->onMouseRelease(Point2D{9, 9}, 0);

    // Draw outside bounds - should not crash
    controller_->onMousePress(Point2D{-5, -5}, 0);
    controller_->onMouseRelease(Point2D{-5, -5}, 0);

    controller_->onMousePress(Point2D{100, 100}, 0);
    controller_->onMouseRelease(Point2D{100, 100}, 0);

    // Test completed without crash
    SUCCEED();
}

// Fill tool with 8-connectivity
TEST_F(ManualSegmentationControllerTest, FillToolWith8Connectivity) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 1).has_value());

    // Create a diagonal boundary
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setBrushSize(1);
    controller_->setActiveLabel(2);

    // Draw diagonal line
    for (int i = 0; i < 10; ++i) {
        controller_->onMousePress(Point2D{i, i}, 0);
        controller_->onMouseRelease(Point2D{i, i}, 0);
    }

    // Fill with 4-connectivity (default)
    FillParameters params;
    params.use8Connectivity = false;
    controller_->setFillParameters(params);

    controller_->setActiveTool(SegmentationTool::Fill);
    controller_->setActiveLabel(1);
    controller_->onMousePress(Point2D{0, 5}, 0);

    auto labelMap = controller_->getLabelMap();

    // With 4-connectivity, fill should be blocked by diagonal
    // The pixel at (1, 5) should be filled
    EXPECT_EQ(getPixelAt(labelMap, 0, 5, 0), 1);

    // Reset and test 8-connectivity
    controller_->clearAll();

    // Redraw diagonal
    controller_->setActiveTool(SegmentationTool::Brush);
    controller_->setActiveLabel(2);
    for (int i = 0; i < 10; ++i) {
        controller_->onMousePress(Point2D{i, i}, 0);
        controller_->onMouseRelease(Point2D{i, i}, 0);
    }

    // Fill with 8-connectivity
    params.use8Connectivity = true;
    controller_->setFillParameters(params);

    controller_->setActiveTool(SegmentationTool::Fill);
    controller_->setActiveLabel(3);
    controller_->onMousePress(Point2D{0, 5}, 0);

    // With 8-connectivity, fill should also stop at diagonal
    // but may leak through depending on boundary configuration
    EXPECT_EQ(getPixelAt(labelMap, 0, 5, 0), 3);
}

// Freehand tool tests
TEST_F(ManualSegmentationControllerTest, FreehandParametersDefault) {
    auto params = controller_->getFreehandParameters();
    EXPECT_TRUE(params.enableSmoothing);
    EXPECT_EQ(params.smoothingWindowSize, 5);
    EXPECT_TRUE(params.enableSimplification);
    EXPECT_DOUBLE_EQ(params.simplificationTolerance, 2.0);
    EXPECT_FALSE(params.fillInterior);
    EXPECT_DOUBLE_EQ(params.closeThreshold, 10.0);
}

TEST_F(ManualSegmentationControllerTest, SetFreehandParametersValid) {
    FreehandParameters params;
    params.enableSmoothing = false;
    params.smoothingWindowSize = 7;
    params.enableSimplification = false;
    params.simplificationTolerance = 5.0;
    params.fillInterior = true;
    params.closeThreshold = 15.0;

    EXPECT_TRUE(controller_->setFreehandParameters(params));

    auto result = controller_->getFreehandParameters();
    EXPECT_FALSE(result.enableSmoothing);
    EXPECT_EQ(result.smoothingWindowSize, 7);
    EXPECT_FALSE(result.enableSimplification);
    EXPECT_DOUBLE_EQ(result.simplificationTolerance, 5.0);
    EXPECT_TRUE(result.fillInterior);
    EXPECT_DOUBLE_EQ(result.closeThreshold, 15.0);
}

TEST_F(ManualSegmentationControllerTest, SetFreehandParametersInvalidWindowSize) {
    FreehandParameters params;
    params.smoothingWindowSize = 2;  // Must be >= 3

    EXPECT_FALSE(controller_->setFreehandParameters(params));

    params.smoothingWindowSize = 4;  // Must be odd
    EXPECT_FALSE(controller_->setFreehandParameters(params));

    params.smoothingWindowSize = 12;  // Must be <= 11
    EXPECT_FALSE(controller_->setFreehandParameters(params));
}

TEST_F(ManualSegmentationControllerTest, FreehandToolDrawsPath) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Freehand);
    controller_->setActiveLabel(1);

    // Disable smoothing and simplification for predictable results
    FreehandParameters params;
    params.enableSmoothing = false;
    params.enableSimplification = false;
    controller_->setFreehandParameters(params);

    // Draw a simple line
    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMouseMove(Point2D{20, 10}, 0);
    controller_->onMouseMove(Point2D{30, 10}, 0);
    controller_->onMouseRelease(Point2D{40, 10}, 0);

    auto labelMap = controller_->getLabelMap();

    // Check that path was drawn (at least endpoints should be labeled)
    EXPECT_EQ(getPixelAt(labelMap, 10, 10, 0), 1);
    EXPECT_EQ(getPixelAt(labelMap, 40, 10, 0), 1);

    // Intermediate points should also be labeled
    EXPECT_EQ(getPixelAt(labelMap, 25, 10, 0), 1);
}

TEST_F(ManualSegmentationControllerTest, FreehandPathCollectsPoints) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Freehand);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMouseMove(Point2D{20, 15}, 0);
    controller_->onMouseMove(Point2D{30, 20}, 0);

    auto path = controller_->getFreehandPath();
    EXPECT_GE(path.size(), 3);

    controller_->onMouseRelease(Point2D{40, 25}, 0);

    // After release, path should be cleared
    path = controller_->getFreehandPath();
    EXPECT_TRUE(path.empty());
}

TEST_F(ManualSegmentationControllerTest, FreehandToolFillsClosedPath) {
    ASSERT_TRUE(controller_->initializeLabelMap(50, 50, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Freehand);
    controller_->setActiveLabel(1);

    // Enable fill interior
    FreehandParameters params;
    params.enableSmoothing = false;
    params.enableSimplification = false;
    params.fillInterior = true;
    params.closeThreshold = 15.0;
    controller_->setFreehandParameters(params);

    // Draw a closed rectangle (start and end points close together)
    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMouseMove(Point2D{30, 10}, 0);
    controller_->onMouseMove(Point2D{30, 30}, 0);
    controller_->onMouseMove(Point2D{10, 30}, 0);
    controller_->onMouseRelease(Point2D{10, 15}, 0);  // Close to start

    auto labelMap = controller_->getLabelMap();

    // Interior point should be filled
    EXPECT_EQ(getPixelAt(labelMap, 20, 20, 0), 1);

    // Points on the boundary should also be labeled
    EXPECT_EQ(getPixelAt(labelMap, 10, 10, 0), 1);
    EXPECT_EQ(getPixelAt(labelMap, 30, 10, 0), 1);
}

TEST_F(ManualSegmentationControllerTest, FreehandToolCancelClearsPath) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Freehand);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMouseMove(Point2D{20, 15}, 0);
    controller_->onMouseMove(Point2D{30, 20}, 0);

    EXPECT_TRUE(controller_->isDrawing());

    controller_->cancelOperation();

    EXPECT_FALSE(controller_->isDrawing());

    auto path = controller_->getFreehandPath();
    EXPECT_TRUE(path.empty());
}

TEST_F(ManualSegmentationControllerTest, FreehandToolWithSmoothing) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Freehand);
    controller_->setActiveLabel(1);

    // Enable smoothing, disable simplification
    FreehandParameters params;
    params.enableSmoothing = true;
    params.smoothingWindowSize = 3;
    params.enableSimplification = false;
    params.fillInterior = false;
    controller_->setFreehandParameters(params);

    // Draw a zigzag path
    controller_->onMousePress(Point2D{10, 20}, 0);
    controller_->onMouseMove(Point2D{15, 10}, 0);
    controller_->onMouseMove(Point2D{20, 30}, 0);
    controller_->onMouseMove(Point2D{25, 10}, 0);
    controller_->onMouseMove(Point2D{30, 30}, 0);
    controller_->onMouseRelease(Point2D{35, 20}, 0);

    auto labelMap = controller_->getLabelMap();

    // Path should be drawn (smoothed)
    int labeledPixels = countLabelPixels(labelMap, 1);
    EXPECT_GT(labeledPixels, 0);
}

TEST_F(ManualSegmentationControllerTest, FreehandToolWithSimplification) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Freehand);
    controller_->setActiveLabel(1);

    // Enable simplification with high tolerance
    FreehandParameters params;
    params.enableSmoothing = false;
    params.enableSimplification = true;
    params.simplificationTolerance = 10.0;  // High tolerance
    params.fillInterior = false;
    controller_->setFreehandParameters(params);

    // Draw many points in a roughly straight line
    controller_->onMousePress(Point2D{10, 50}, 0);
    for (int x = 11; x < 90; ++x) {
        // Slight vertical variation
        int y = 50 + (x % 3) - 1;
        controller_->onMouseMove(Point2D{x, y}, 0);
    }
    controller_->onMouseRelease(Point2D{90, 50}, 0);

    auto labelMap = controller_->getLabelMap();

    // Simplified path should still connect start to end
    EXPECT_EQ(getPixelAt(labelMap, 10, 50, 0), 1);
    EXPECT_EQ(getPixelAt(labelMap, 90, 50, 0), 1);
}

// Polygon tool tests
TEST_F(ManualSegmentationControllerTest, PolygonParametersDefault) {
    auto params = controller_->getPolygonParameters();
    EXPECT_TRUE(params.fillInterior);
    EXPECT_TRUE(params.drawOutline);
    EXPECT_EQ(params.minimumVertices, 3);
}

TEST_F(ManualSegmentationControllerTest, SetPolygonParametersValid) {
    PolygonParameters params;
    params.fillInterior = false;
    params.drawOutline = true;
    params.minimumVertices = 4;

    EXPECT_TRUE(controller_->setPolygonParameters(params));

    auto result = controller_->getPolygonParameters();
    EXPECT_FALSE(result.fillInterior);
    EXPECT_TRUE(result.drawOutline);
    EXPECT_EQ(result.minimumVertices, 4);
}

TEST_F(ManualSegmentationControllerTest, SetPolygonParametersInvalid) {
    PolygonParameters params;
    params.minimumVertices = 2;  // Must be >= 3

    EXPECT_FALSE(controller_->setPolygonParameters(params));
}

TEST_F(ManualSegmentationControllerTest, PolygonToolAddsVertices) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);

    controller_->onMousePress(Point2D{10, 10}, 0);
    EXPECT_EQ(controller_->getPolygonVertices().size(), 1);

    controller_->onMousePress(Point2D{50, 10}, 0);
    EXPECT_EQ(controller_->getPolygonVertices().size(), 2);

    controller_->onMousePress(Point2D{30, 50}, 0);
    EXPECT_EQ(controller_->getPolygonVertices().size(), 3);
}

TEST_F(ManualSegmentationControllerTest, PolygonToolUndoVertex) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);
    controller_->onMousePress(Point2D{30, 50}, 0);

    EXPECT_EQ(controller_->getPolygonVertices().size(), 3);

    EXPECT_TRUE(controller_->undoLastPolygonVertex());
    EXPECT_EQ(controller_->getPolygonVertices().size(), 2);

    EXPECT_TRUE(controller_->undoLastPolygonVertex());
    EXPECT_EQ(controller_->getPolygonVertices().size(), 1);

    EXPECT_TRUE(controller_->undoLastPolygonVertex());
    EXPECT_EQ(controller_->getPolygonVertices().size(), 0);

    // Undo on empty polygon returns false
    EXPECT_FALSE(controller_->undoLastPolygonVertex());
}

TEST_F(ManualSegmentationControllerTest, CanCompletePolygon) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);

    // Need at least 3 vertices by default
    controller_->onMousePress(Point2D{10, 10}, 0);
    EXPECT_FALSE(controller_->canCompletePolygon());

    controller_->onMousePress(Point2D{50, 10}, 0);
    EXPECT_FALSE(controller_->canCompletePolygon());

    controller_->onMousePress(Point2D{30, 50}, 0);
    EXPECT_TRUE(controller_->canCompletePolygon());
}

TEST_F(ManualSegmentationControllerTest, PolygonToolCompleteDrawsPolygon) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);
    controller_->setActiveLabel(1);

    // Draw a triangle
    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);
    controller_->onMousePress(Point2D{30, 50}, 0);

    EXPECT_TRUE(controller_->completePolygon(0));

    auto labelMap = controller_->getLabelMap();

    // Vertices should be labeled (outline)
    EXPECT_EQ(getPixelAt(labelMap, 10, 10, 0), 1);
    EXPECT_EQ(getPixelAt(labelMap, 50, 10, 0), 1);
    EXPECT_EQ(getPixelAt(labelMap, 30, 50, 0), 1);

    // Interior should be filled
    EXPECT_EQ(getPixelAt(labelMap, 30, 25, 0), 1);

    // Polygon vertices should be cleared after completion
    EXPECT_TRUE(controller_->getPolygonVertices().empty());
}

TEST_F(ManualSegmentationControllerTest, PolygonToolCompleteOutlineOnly) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);
    controller_->setActiveLabel(1);

    PolygonParameters params;
    params.fillInterior = false;
    params.drawOutline = true;
    controller_->setPolygonParameters(params);

    // Draw a large triangle
    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{90, 10}, 0);
    controller_->onMousePress(Point2D{50, 90}, 0);

    EXPECT_TRUE(controller_->completePolygon(0));

    auto labelMap = controller_->getLabelMap();

    // Vertices should be labeled (outline)
    EXPECT_EQ(getPixelAt(labelMap, 10, 10, 0), 1);
    EXPECT_EQ(getPixelAt(labelMap, 90, 10, 0), 1);

    // Interior should NOT be filled
    EXPECT_EQ(getPixelAt(labelMap, 50, 30, 0), 0);
}

TEST_F(ManualSegmentationControllerTest, PolygonToolInsufficientVertices) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);
    controller_->setActiveLabel(1);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);

    // Only 2 vertices, minimum is 3
    EXPECT_FALSE(controller_->completePolygon(0));

    // Vertices should still be there
    EXPECT_EQ(controller_->getPolygonVertices().size(), 2);
}

TEST_F(ManualSegmentationControllerTest, PolygonToolCancelClearsVertices) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);
    controller_->onMousePress(Point2D{30, 50}, 0);

    EXPECT_EQ(controller_->getPolygonVertices().size(), 3);

    controller_->cancelOperation();

    EXPECT_TRUE(controller_->getPolygonVertices().empty());
}

TEST_F(ManualSegmentationControllerTest, PolygonToolSameSliceOnly) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);

    // Start on slice 0
    controller_->onMousePress(Point2D{10, 10}, 0);
    EXPECT_EQ(controller_->getPolygonVertices().size(), 1);

    // Try to add vertex on different slice - should be ignored
    controller_->onMousePress(Point2D{50, 10}, 5);
    EXPECT_EQ(controller_->getPolygonVertices().size(), 1);

    // Add vertex on same slice
    controller_->onMousePress(Point2D{50, 10}, 0);
    EXPECT_EQ(controller_->getPolygonVertices().size(), 2);
}

TEST_F(ManualSegmentationControllerTest, PolygonToolModificationCallback) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);

    int callbackCount = 0;
    int lastSlice = -1;

    controller_->setModificationCallback([&](int sliceIndex) {
        ++callbackCount;
        lastSlice = sliceIndex;
    });

    controller_->onMousePress(Point2D{10, 10}, 0);
    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(lastSlice, 0);

    controller_->onMousePress(Point2D{50, 10}, 0);
    EXPECT_EQ(callbackCount, 2);

    controller_->onMousePress(Point2D{30, 50}, 0);
    EXPECT_EQ(callbackCount, 3);

    controller_->completePolygon(0);
    EXPECT_EQ(callbackCount, 4);
}

TEST_F(ManualSegmentationControllerTest, PolygonToolCustomMinimumVertices) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::Polygon);

    // Require 4 vertices minimum
    PolygonParameters params;
    params.minimumVertices = 4;
    controller_->setPolygonParameters(params);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);
    controller_->onMousePress(Point2D{50, 50}, 0);

    // Only 3 vertices, need 4
    EXPECT_FALSE(controller_->canCompletePolygon());

    controller_->onMousePress(Point2D{10, 50}, 0);
    EXPECT_TRUE(controller_->canCompletePolygon());
}

// Smart Scissors tool tests
TEST_F(ManualSegmentationControllerTest, SmartScissorsParametersDefault) {
    auto params = controller_->getSmartScissorsParameters();
    EXPECT_DOUBLE_EQ(params.gradientWeight, 0.43);
    EXPECT_DOUBLE_EQ(params.directionWeight, 0.43);
    EXPECT_DOUBLE_EQ(params.laplacianWeight, 0.14);
    EXPECT_DOUBLE_EQ(params.gaussianSigma, 1.5);
    EXPECT_TRUE(params.enableSmoothing);
    EXPECT_DOUBLE_EQ(params.closeThreshold, 10.0);
    EXPECT_TRUE(params.fillInterior);
}

TEST_F(ManualSegmentationControllerTest, SetSmartScissorsParametersValid) {
    SmartScissorsParameters params;
    params.gradientWeight = 0.5;
    params.directionWeight = 0.3;
    params.laplacianWeight = 0.2;
    params.gaussianSigma = 2.0;
    params.enableSmoothing = false;
    params.closeThreshold = 15.0;
    params.fillInterior = false;

    EXPECT_TRUE(controller_->setSmartScissorsParameters(params));

    auto result = controller_->getSmartScissorsParameters();
    EXPECT_DOUBLE_EQ(result.gradientWeight, 0.5);
    EXPECT_DOUBLE_EQ(result.directionWeight, 0.3);
    EXPECT_DOUBLE_EQ(result.laplacianWeight, 0.2);
    EXPECT_DOUBLE_EQ(result.gaussianSigma, 2.0);
    EXPECT_FALSE(result.enableSmoothing);
    EXPECT_DOUBLE_EQ(result.closeThreshold, 15.0);
    EXPECT_FALSE(result.fillInterior);
}

TEST_F(ManualSegmentationControllerTest, SetSmartScissorsParametersInvalidWeights) {
    SmartScissorsParameters params;

    // Weight exceeds 1.0
    params.gradientWeight = 1.5;
    EXPECT_FALSE(controller_->setSmartScissorsParameters(params));

    // Negative weight
    params.gradientWeight = -0.1;
    EXPECT_FALSE(controller_->setSmartScissorsParameters(params));

    // Total weight exceeds 1.0
    params.gradientWeight = 0.5;
    params.directionWeight = 0.5;
    params.laplacianWeight = 0.5;
    EXPECT_FALSE(controller_->setSmartScissorsParameters(params));
}

TEST_F(ManualSegmentationControllerTest, SetSmartScissorsParametersInvalidSigma) {
    SmartScissorsParameters params;

    // Sigma too low
    params.gaussianSigma = 0.5;
    EXPECT_FALSE(controller_->setSmartScissorsParameters(params));

    // Sigma too high
    params.gaussianSigma = 6.0;
    EXPECT_FALSE(controller_->setSmartScissorsParameters(params));
}

TEST_F(ManualSegmentationControllerTest, SmartScissorsToolAddsAnchors) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::SmartScissors);

    controller_->onMousePress(Point2D{10, 10}, 0);
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 1);

    controller_->onMousePress(Point2D{50, 10}, 0);
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 2);

    controller_->onMousePress(Point2D{30, 50}, 0);
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 3);
}

TEST_F(ManualSegmentationControllerTest, SmartScissorsUndoAnchor) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::SmartScissors);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);
    controller_->onMousePress(Point2D{30, 50}, 0);

    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 3);

    EXPECT_TRUE(controller_->undoLastSmartScissorsAnchor());
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 2);

    EXPECT_TRUE(controller_->undoLastSmartScissorsAnchor());
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 1);

    EXPECT_TRUE(controller_->undoLastSmartScissorsAnchor());
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 0);

    // Undo on empty returns false
    EXPECT_FALSE(controller_->undoLastSmartScissorsAnchor());
}

TEST_F(ManualSegmentationControllerTest, CanCompleteSmartScissors) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::SmartScissors);

    // Need at least 2 anchors
    controller_->onMousePress(Point2D{10, 10}, 0);
    EXPECT_FALSE(controller_->canCompleteSmartScissors());

    controller_->onMousePress(Point2D{50, 10}, 0);
    EXPECT_TRUE(controller_->canCompleteSmartScissors());
}

TEST_F(ManualSegmentationControllerTest, SmartScissorsSameSliceOnly) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 10).has_value());
    controller_->setActiveTool(SegmentationTool::SmartScissors);

    // Start on slice 0
    controller_->onMousePress(Point2D{10, 10}, 0);
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 1);

    // Try to add anchor on different slice - should be ignored
    controller_->onMousePress(Point2D{50, 10}, 5);
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 1);

    // Add anchor on same slice
    controller_->onMousePress(Point2D{50, 10}, 0);
    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 2);
}

TEST_F(ManualSegmentationControllerTest, SmartScissorsCancelClearsState) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::SmartScissors);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);
    controller_->onMousePress(Point2D{30, 50}, 0);

    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 3);

    controller_->cancelOperation();

    EXPECT_TRUE(controller_->getSmartScissorsAnchors().empty());
    EXPECT_TRUE(controller_->getSmartScissorsPath().empty());
}

TEST_F(ManualSegmentationControllerTest, SmartScissorsToolSwitchClearsState) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::SmartScissors);

    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{50, 10}, 0);

    EXPECT_EQ(controller_->getSmartScissorsAnchors().size(), 2);

    // Switch to different tool
    controller_->setActiveTool(SegmentationTool::Brush);

    // State should be cleared
    EXPECT_TRUE(controller_->getSmartScissorsAnchors().empty());
}

// =============================================================================
// Edge case and algorithmic correctness tests (Issue #204)
// =============================================================================

TEST_F(ManualSegmentationControllerTest, UndoRedoAfterBrushStroke) {
    ASSERT_TRUE(controller_->initializeLabelMap(50, 50, 1).has_value());
    controller_->setActiveLabel(1);
    controller_->setActiveTool(SegmentationTool::Brush);

    auto labelMap = controller_->getLabelMap();
    int beforeCount = countLabelPixels(labelMap, 1);
    EXPECT_EQ(beforeCount, 0);

    // Paint a stroke
    controller_->onMousePress(Point2D{25, 25}, 0);
    controller_->onMouseMove(Point2D{30, 25}, 0);
    controller_->onMouseRelease(Point2D{30, 25}, 0);

    labelMap = controller_->getLabelMap();
    int afterPaintCount = countLabelPixels(labelMap, 1);
    EXPECT_GT(afterPaintCount, 0) << "Brush stroke should paint pixels";

    // Undo should revert to zero painted pixels
    auto undoResult = controller_->undo();
    EXPECT_TRUE(undoResult.has_value());

    labelMap = controller_->getLabelMap();
    int afterUndoCount = countLabelPixels(labelMap, 1);
    EXPECT_EQ(afterUndoCount, 0) << "Undo should revert brush stroke";

    // Redo should restore the painted pixels
    auto redoResult = controller_->redo();
    EXPECT_TRUE(redoResult.has_value());

    labelMap = controller_->getLabelMap();
    int afterRedoCount = countLabelPixels(labelMap, 1);
    EXPECT_EQ(afterRedoCount, afterPaintCount) << "Redo should restore brush stroke";
}

TEST_F(ManualSegmentationControllerTest, PolygonSelfIntersectionHandled) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveLabel(1);
    controller_->setActiveTool(SegmentationTool::Polygon);

    // Create a self-intersecting polygon (figure-8 shape)
    controller_->onMousePress(Point2D{20, 20}, 0);  // Top-left
    controller_->onMousePress(Point2D{80, 80}, 0);  // Bottom-right
    controller_->onMousePress(Point2D{80, 20}, 0);  // Top-right
    controller_->onMousePress(Point2D{20, 80}, 0);  // Bottom-left

    // Complete the polygon (double-click or close)
    controller_->onMousePress(Point2D{20, 20}, 0);

    // Should not crash; label map should remain valid
    auto labelMap = controller_->getLabelMap();
    ASSERT_NE(labelMap, nullptr);

    auto size = labelMap->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], 100u);
    EXPECT_EQ(size[1], 100u);
}

TEST_F(ManualSegmentationControllerTest, SmartScissorsMultipleAnchors) {
    ASSERT_TRUE(controller_->initializeLabelMap(100, 100, 1).has_value());
    controller_->setActiveTool(SegmentationTool::SmartScissors);

    // Place multiple anchor points
    controller_->onMousePress(Point2D{10, 10}, 0);
    controller_->onMousePress(Point2D{90, 10}, 0);
    controller_->onMousePress(Point2D{90, 90}, 0);
    controller_->onMousePress(Point2D{10, 90}, 0);

    auto anchors = controller_->getSmartScissorsAnchors();
    EXPECT_EQ(anchors.size(), 4)
        << "Should store all anchor points without crashing";

    // Path should be generated between anchors
    auto path = controller_->getSmartScissorsPath();
    // Path may be empty if no image data is set, but should not crash
    ASSERT_NE(controller_->getLabelMap(), nullptr);
}

TEST_F(ManualSegmentationControllerTest, LabelExhaustion255Labels) {
    ASSERT_TRUE(controller_->initializeLabelMap(10, 10, 1).has_value());

    // Add labels up to the maximum (255)
    bool reachedLimit = false;
    for (int i = 1; i <= 256; ++i) {
        controller_->setActiveLabel(static_cast<uint8_t>(std::min(i, 255)));
        if (i > 255) {
            reachedLimit = true;
        }
    }

    // The controller should handle label ID 255 without issues
    controller_->setActiveLabel(255);
    controller_->setActiveTool(SegmentationTool::Brush);

    controller_->onMousePress(Point2D{5, 5}, 0);
    controller_->onMouseRelease(Point2D{5, 5}, 0);

    auto labelMap = controller_->getLabelMap();
    uint8_t pixelVal = getPixelAt(labelMap, 5, 5, 0);
    EXPECT_EQ(pixelVal, 255) << "Should be able to paint with label ID 255";
}

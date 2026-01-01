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

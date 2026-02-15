#include <gtest/gtest.h>

#include <itkImage.h>

#include "services/segmentation/segmentation_command.hpp"
#include "services/segmentation/brush_stroke_command.hpp"

using namespace dicom_viewer::services;

namespace {

/// Simple concrete command for testing the stack
class TestCommand : public ISegmentationCommand {
public:
    TestCommand(int& counter, int delta, std::string desc = "Test")
        : counter_(counter), delta_(delta), desc_(std::move(desc)) {}

    void execute() override { counter_ += delta_; }
    void undo() override { counter_ -= delta_; }
    std::string description() const override { return desc_; }
    size_t memoryUsage() const override { return sizeof(*this); }

private:
    int& counter_;
    int delta_;
    std::string desc_;
};

/// Create a 10x10x1 label map for testing
BrushStrokeCommand::LabelMapType::Pointer createTestLabelMap() {
    auto image = BrushStrokeCommand::LabelMapType::New();
    BrushStrokeCommand::LabelMapType::SizeType size;
    size[0] = 10; size[1] = 10; size[2] = 1;
    BrushStrokeCommand::LabelMapType::IndexType start;
    start[0] = 0; start[1] = 0; start[2] = 0;
    image->SetRegions(
        BrushStrokeCommand::LabelMapType::RegionType(start, size));
    image->Allocate(true);  // Initialize to zero
    return image;
}

}  // anonymous namespace

// =============================================================================
// SegmentationCommandStack — Construction
// =============================================================================

TEST(SegmentationCommandStackTest, DefaultConstruction) {
    SegmentationCommandStack stack;
    EXPECT_FALSE(stack.canUndo());
    EXPECT_FALSE(stack.canRedo());
    EXPECT_EQ(stack.undoCount(), 0);
    EXPECT_EQ(stack.redoCount(), 0);
    EXPECT_EQ(stack.maxHistorySize(), 20);
}

TEST(SegmentationCommandStackTest, CustomHistorySize) {
    SegmentationCommandStack stack(50);
    EXPECT_EQ(stack.maxHistorySize(), 50);
}

TEST(SegmentationCommandStackTest, MinimumHistorySize) {
    SegmentationCommandStack stack(0);
    EXPECT_EQ(stack.maxHistorySize(), 1);
}

// =============================================================================
// SegmentationCommandStack — Execute / Undo / Redo
// =============================================================================

TEST(SegmentationCommandStackTest, ExecuteAndUndo) {
    int counter = 0;
    SegmentationCommandStack stack;

    stack.execute(std::make_unique<TestCommand>(counter, 5));
    EXPECT_EQ(counter, 5);
    EXPECT_TRUE(stack.canUndo());
    EXPECT_FALSE(stack.canRedo());

    stack.undo();
    EXPECT_EQ(counter, 0);
    EXPECT_FALSE(stack.canUndo());
    EXPECT_TRUE(stack.canRedo());
}

TEST(SegmentationCommandStackTest, UndoAndRedo) {
    int counter = 0;
    SegmentationCommandStack stack;

    stack.execute(std::make_unique<TestCommand>(counter, 10));
    stack.execute(std::make_unique<TestCommand>(counter, 20));
    EXPECT_EQ(counter, 30);

    stack.undo();
    EXPECT_EQ(counter, 10);

    stack.redo();
    EXPECT_EQ(counter, 30);
}

TEST(SegmentationCommandStackTest, RedoClearedOnNewCommand) {
    int counter = 0;
    SegmentationCommandStack stack;

    stack.execute(std::make_unique<TestCommand>(counter, 10));
    stack.execute(std::make_unique<TestCommand>(counter, 20));
    EXPECT_EQ(counter, 30);

    stack.undo();
    EXPECT_EQ(counter, 10);
    EXPECT_TRUE(stack.canRedo());

    // New command should clear redo
    stack.execute(std::make_unique<TestCommand>(counter, 5));
    EXPECT_EQ(counter, 15);
    EXPECT_FALSE(stack.canRedo());
}

TEST(SegmentationCommandStackTest, MultipleUndoRedo) {
    int counter = 0;
    SegmentationCommandStack stack;

    stack.execute(std::make_unique<TestCommand>(counter, 1));
    stack.execute(std::make_unique<TestCommand>(counter, 2));
    stack.execute(std::make_unique<TestCommand>(counter, 3));
    EXPECT_EQ(counter, 6);
    EXPECT_EQ(stack.undoCount(), 3);

    stack.undo();
    stack.undo();
    stack.undo();
    EXPECT_EQ(counter, 0);
    EXPECT_EQ(stack.redoCount(), 3);

    stack.redo();
    stack.redo();
    EXPECT_EQ(counter, 3);
    EXPECT_EQ(stack.undoCount(), 2);
    EXPECT_EQ(stack.redoCount(), 1);
}

// =============================================================================
// SegmentationCommandStack — History limit
// =============================================================================

TEST(SegmentationCommandStackTest, HistoryLimitRespected) {
    int counter = 0;
    SegmentationCommandStack stack(5);

    for (int i = 0; i < 10; ++i) {
        stack.execute(std::make_unique<TestCommand>(counter, 1));
    }
    EXPECT_EQ(counter, 10);
    EXPECT_EQ(stack.undoCount(), 5);

    // Can only undo 5 steps
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(stack.undo());
    }
    EXPECT_EQ(counter, 5);
    EXPECT_FALSE(stack.undo());
}

TEST(SegmentationCommandStackTest, DefaultHistoryAtLeast20) {
    int counter = 0;
    SegmentationCommandStack stack;

    for (int i = 0; i < 25; ++i) {
        stack.execute(std::make_unique<TestCommand>(counter, 1));
    }
    EXPECT_GE(stack.undoCount(), 20);
}

// =============================================================================
// SegmentationCommandStack — Clear and descriptions
// =============================================================================

TEST(SegmentationCommandStackTest, Clear) {
    int counter = 0;
    SegmentationCommandStack stack;

    stack.execute(std::make_unique<TestCommand>(counter, 1));
    stack.execute(std::make_unique<TestCommand>(counter, 2));
    stack.undo();

    stack.clear();
    EXPECT_FALSE(stack.canUndo());
    EXPECT_FALSE(stack.canRedo());
    EXPECT_EQ(stack.undoCount(), 0);
    EXPECT_EQ(stack.redoCount(), 0);
}

TEST(SegmentationCommandStackTest, Descriptions) {
    int counter = 0;
    SegmentationCommandStack stack;

    EXPECT_TRUE(stack.undoDescription().empty());
    EXPECT_TRUE(stack.redoDescription().empty());

    stack.execute(std::make_unique<TestCommand>(counter, 1, "Step 1"));
    stack.execute(std::make_unique<TestCommand>(counter, 2, "Step 2"));

    EXPECT_EQ(stack.undoDescription(), "Step 2");

    stack.undo();
    EXPECT_EQ(stack.undoDescription(), "Step 1");
    EXPECT_EQ(stack.redoDescription(), "Step 2");
}

// =============================================================================
// SegmentationCommandStack — Availability callback
// =============================================================================

TEST(SegmentationCommandStackTest, AvailabilityCallback) {
    int counter = 0;
    SegmentationCommandStack stack;

    bool lastCanUndo = false;
    bool lastCanRedo = false;
    int callCount = 0;

    stack.setAvailabilityCallback(
        [&](bool canUndo, bool canRedo) {
            lastCanUndo = canUndo;
            lastCanRedo = canRedo;
            ++callCount;
        });

    stack.execute(std::make_unique<TestCommand>(counter, 1));
    EXPECT_TRUE(lastCanUndo);
    EXPECT_FALSE(lastCanRedo);
    EXPECT_EQ(callCount, 1);

    stack.undo();
    EXPECT_FALSE(lastCanUndo);
    EXPECT_TRUE(lastCanRedo);
    EXPECT_EQ(callCount, 2);
}

// =============================================================================
// SegmentationCommandStack — Edge cases
// =============================================================================

TEST(SegmentationCommandStackTest, UndoOnEmptyReturnsFalse) {
    SegmentationCommandStack stack;
    EXPECT_FALSE(stack.undo());
}

TEST(SegmentationCommandStackTest, RedoOnEmptyReturnsFalse) {
    SegmentationCommandStack stack;
    EXPECT_FALSE(stack.redo());
}

TEST(SegmentationCommandStackTest, NullCommandIgnored) {
    SegmentationCommandStack stack;
    stack.execute(nullptr);
    EXPECT_FALSE(stack.canUndo());
}

// =============================================================================
// BrushStrokeCommand
// =============================================================================

TEST(BrushStrokeCommandTest, RecordAndUndo) {
    auto labelMap = createTestLabelMap();
    auto* buffer = labelMap->GetBufferPointer();

    // Simulate a brush stroke: paint label 1 on voxels 0-4
    auto cmd = std::make_unique<BrushStrokeCommand>(labelMap, "Brush stroke");
    for (int i = 0; i < 5; ++i) {
        cmd->recordChange(i, buffer[i], 1);
        buffer[i] = 1;  // Apply during drawing
    }
    EXPECT_EQ(cmd->changeCount(), 5);
    EXPECT_TRUE(cmd->hasChanges());

    // Verify painted
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(buffer[i], 1);
    }

    // Undo should restore to 0
    cmd->undo();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(buffer[i], 0);
    }

    // Execute (redo) should re-apply
    cmd->execute();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(buffer[i], 1);
    }
}

TEST(BrushStrokeCommandTest, SkipsDuplicateLabels) {
    auto labelMap = createTestLabelMap();

    auto cmd = std::make_unique<BrushStrokeCommand>(labelMap, "No-op");
    // Recording same old/new label should be ignored
    cmd->recordChange(0, 0, 0);
    cmd->recordChange(1, 1, 1);
    EXPECT_EQ(cmd->changeCount(), 0);
    EXPECT_FALSE(cmd->hasChanges());
}

TEST(BrushStrokeCommandTest, MemoryUsage) {
    auto labelMap = createTestLabelMap();

    auto cmd = std::make_unique<BrushStrokeCommand>(labelMap, "Brush");
    size_t baseMemory = cmd->memoryUsage();

    cmd->recordChange(0, 0, 1);
    cmd->recordChange(1, 0, 1);
    EXPECT_GT(cmd->memoryUsage(), baseMemory);
    EXPECT_EQ(cmd->memoryUsage(),
              2 * sizeof(VoxelChange) + std::string("Brush").size());
}

TEST(BrushStrokeCommandTest, Description) {
    auto labelMap = createTestLabelMap();
    BrushStrokeCommand cmd(labelMap, "Circle brush size 10");
    EXPECT_EQ(cmd.description(), "Circle brush size 10");
}

TEST(BrushStrokeCommandTest, IntegrationWithCommandStack) {
    auto labelMap = createTestLabelMap();
    auto* buffer = labelMap->GetBufferPointer();

    SegmentationCommandStack stack;

    // Stroke 1: paint label 1 on voxels 0-2
    {
        auto cmd = std::make_unique<BrushStrokeCommand>(labelMap, "Stroke 1");
        for (int i = 0; i < 3; ++i) {
            cmd->recordChange(i, buffer[i], 1);
            buffer[i] = 1;
        }
        // execute() is a no-op since we already applied during drawing,
        // but the stack calls it — re-applying same values is idempotent
        stack.execute(std::move(cmd));
    }

    // Stroke 2: paint label 2 on voxels 3-5
    {
        auto cmd = std::make_unique<BrushStrokeCommand>(labelMap, "Stroke 2");
        for (int i = 3; i < 6; ++i) {
            cmd->recordChange(i, buffer[i], 2);
            buffer[i] = 2;
        }
        stack.execute(std::move(cmd));
    }

    // Verify state: [1,1,1,2,2,2,0,0,0,0...]
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[3], 2);
    EXPECT_EQ(buffer[6], 0);

    // Undo stroke 2
    stack.undo();
    EXPECT_EQ(buffer[3], 0);
    EXPECT_EQ(buffer[0], 1);

    // Undo stroke 1
    stack.undo();
    EXPECT_EQ(buffer[0], 0);

    // Redo both
    stack.redo();
    EXPECT_EQ(buffer[0], 1);
    stack.redo();
    EXPECT_EQ(buffer[3], 2);
}

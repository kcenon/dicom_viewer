#include <gtest/gtest.h>

#include <cstring>
#include <numeric>
#include <random>
#include <vector>

#include "services/segmentation/segmentation_command.hpp"
#include "services/segmentation/snapshot_command.hpp"

using namespace dicom_viewer::services;
using LabelMapType = SnapshotCommand::LabelMapType;

namespace {

/**
 * @brief Create a label map filled with zeros
 */
LabelMapType::Pointer createLabelMap(int nx, int ny, int nz) {
    auto map = LabelMapType::New();
    LabelMapType::RegionType region;
    LabelMapType::SizeType size;
    size[0] = nx; size[1] = ny; size[2] = nz;
    region.SetSize(size);
    map->SetRegions(region);

    LabelMapType::SpacingType spacing;
    spacing[0] = 1.0; spacing[1] = 1.0; spacing[2] = 1.0;
    map->SetSpacing(spacing);

    map->Allocate(true);
    return map;
}

/**
 * @brief Count voxels with a specific label
 */
int countLabel(LabelMapType::Pointer map, uint8_t label) {
    auto* buf = map->GetBufferPointer();
    auto size = map->GetLargestPossibleRegion().GetSize();
    size_t total = size[0] * size[1] * size[2];
    int count = 0;
    for (size_t i = 0; i < total; ++i) {
        if (buf[i] == label) ++count;
    }
    return count;
}

}  // namespace

// =============================================================================
// RLE compression tests
// =============================================================================

TEST(SnapshotCommand, RLERoundtripAllZeros) {
    std::vector<uint8_t> data(1000, 0);
    auto compressed = SnapshotCommand::compressRLE(data.data(), data.size());

    std::vector<uint8_t> output(data.size());
    SnapshotCommand::decompressRLE(compressed, output.data(), output.size());

    EXPECT_EQ(data, output);
}

TEST(SnapshotCommand, RLERoundtripMixedData) {
    // Create data with several distinct runs
    std::vector<uint8_t> data(500);
    std::fill_n(data.data(), 100, 0);
    std::fill_n(data.data() + 100, 150, 1);
    std::fill_n(data.data() + 250, 50, 2);
    std::fill_n(data.data() + 300, 200, 0);

    auto compressed = SnapshotCommand::compressRLE(data.data(), data.size());

    std::vector<uint8_t> output(data.size());
    SnapshotCommand::decompressRLE(compressed, output.data(), output.size());

    EXPECT_EQ(data, output);
}

TEST(SnapshotCommand, RLERoundtripAlternating) {
    // Worst case: alternating values → no compression benefit
    std::vector<uint8_t> data(256);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 2);
    }

    auto compressed = SnapshotCommand::compressRLE(data.data(), data.size());

    std::vector<uint8_t> output(data.size());
    SnapshotCommand::decompressRLE(compressed, output.data(), output.size());

    EXPECT_EQ(data, output);
}

TEST(SnapshotCommand, RLECompressionRatio) {
    // All zeros: should compress to a single 5-byte run
    std::vector<uint8_t> data(100000, 0);
    auto compressed = SnapshotCommand::compressRLE(data.data(), data.size());

    // One run: 5 bytes (1 value + 4 count)
    EXPECT_EQ(compressed.size(), 5u);
}

TEST(SnapshotCommand, RLECompressionMostlyZero) {
    // Realistic label map: mostly zero with a few labeled regions
    size_t total = 128 * 128 * 128;  // 2M voxels
    std::vector<uint8_t> data(total, 0);

    // Label a small sphere (~5% of voxels)
    int center = 64;
    int radius = 20;
    for (int z = 0; z < 128; ++z) {
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
                int dx = x - center;
                int dy = y - center;
                int dz = z - center;
                if (dx*dx + dy*dy + dz*dz < radius*radius) {
                    data[z * 128 * 128 + y * 128 + x] = 1;
                }
            }
        }
    }

    auto compressed = SnapshotCommand::compressRLE(data.data(), data.size());

    // Compressed should be much smaller than raw (2M bytes)
    double ratio = static_cast<double>(compressed.size()) / total;
    EXPECT_LT(ratio, 0.01) << "Mostly-zero label map should compress to <1% of raw size";

    // Verify roundtrip
    std::vector<uint8_t> output(total);
    SnapshotCommand::decompressRLE(compressed, output.data(), output.size());
    EXPECT_EQ(data, output);
}

TEST(SnapshotCommand, RLESingleElement) {
    uint8_t data = 42;
    auto compressed = SnapshotCommand::compressRLE(&data, 1);
    EXPECT_EQ(compressed.size(), 5u);

    uint8_t output = 0;
    SnapshotCommand::decompressRLE(compressed, &output, 1);
    EXPECT_EQ(output, 42);
}

// =============================================================================
// Undo/Redo tests
// =============================================================================

TEST(SnapshotCommand, UndoRestoresBeforeState) {
    auto labelMap = createLabelMap(32, 32, 32);

    // Create command (captures empty state)
    auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Test operation");

    // Simulate a bulk operation: fill first 1000 voxels with label 1
    auto* buf = labelMap->GetBufferPointer();
    for (int i = 0; i < 1000; ++i) {
        buf[i] = 1;
    }

    cmd->captureAfterState();
    EXPECT_TRUE(cmd->isComplete());

    // Verify label map has the changes
    EXPECT_EQ(countLabel(labelMap, 1), 1000);

    // Undo → should restore all zeros
    cmd->undo();
    EXPECT_EQ(countLabel(labelMap, 1), 0);
    EXPECT_EQ(countLabel(labelMap, 0), 32 * 32 * 32);
}

TEST(SnapshotCommand, RedoRestoresAfterState) {
    auto labelMap = createLabelMap(32, 32, 32);

    auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Test redo");

    auto* buf = labelMap->GetBufferPointer();
    for (int i = 0; i < 500; ++i) {
        buf[i] = 3;
    }

    cmd->captureAfterState();

    // Undo
    cmd->undo();
    EXPECT_EQ(countLabel(labelMap, 3), 0);

    // Redo (execute)
    cmd->execute();
    EXPECT_EQ(countLabel(labelMap, 3), 500);
}

TEST(SnapshotCommand, MultipleUndoRedoCycles) {
    auto labelMap = createLabelMap(16, 16, 16);

    auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Cycle test");

    auto* buf = labelMap->GetBufferPointer();
    for (int i = 0; i < 100; ++i) {
        buf[i] = 2;
    }
    cmd->captureAfterState();

    // Cycle undo/redo 5 times
    for (int cycle = 0; cycle < 5; ++cycle) {
        cmd->undo();
        EXPECT_EQ(countLabel(labelMap, 2), 0)
            << "Undo cycle " << cycle;

        cmd->execute();
        EXPECT_EQ(countLabel(labelMap, 2), 100)
            << "Redo cycle " << cycle;
    }
}

TEST(SnapshotCommand, DescriptionAndMetadata) {
    auto labelMap = createLabelMap(10, 10, 10);
    auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Threshold [100, 500]");

    EXPECT_EQ(cmd->description(), "Threshold [100, 500]");
    EXPECT_GT(cmd->memoryUsage(), 0u);
}

TEST(SnapshotCommand, IncompleteCommandUndoStillWorks) {
    auto labelMap = createLabelMap(16, 16, 16);

    auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Incomplete");
    EXPECT_FALSE(cmd->isComplete());

    // Modify label map
    labelMap->GetBufferPointer()[0] = 5;

    // Undo without captureAfterState → restores before state
    cmd->undo();
    EXPECT_EQ(labelMap->GetBufferPointer()[0], 0);
}

// =============================================================================
// Integration with SegmentationCommandStack
// =============================================================================

TEST(SnapshotCommand, WorksWithCommandStack) {
    auto labelMap = createLabelMap(32, 32, 32);
    SegmentationCommandStack stack;

    // Operation 1: Fill with label 1
    {
        auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Fill label 1");
        auto* buf = labelMap->GetBufferPointer();
        for (int i = 0; i < 500; ++i) buf[i] = 1;
        cmd->captureAfterState();
        stack.execute(std::move(cmd));
    }

    // Operation 2: Fill with label 2
    {
        auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Fill label 2");
        auto* buf = labelMap->GetBufferPointer();
        for (int i = 500; i < 1000; ++i) buf[i] = 2;
        cmd->captureAfterState();
        stack.execute(std::move(cmd));
    }

    EXPECT_EQ(countLabel(labelMap, 1), 500);
    EXPECT_EQ(countLabel(labelMap, 2), 500);

    // Undo operation 2
    EXPECT_TRUE(stack.undo());
    EXPECT_EQ(countLabel(labelMap, 1), 500);
    EXPECT_EQ(countLabel(labelMap, 2), 0);

    // Undo operation 1
    EXPECT_TRUE(stack.undo());
    EXPECT_EQ(countLabel(labelMap, 1), 0);
    EXPECT_EQ(countLabel(labelMap, 0), 32 * 32 * 32);

    // Redo both
    EXPECT_TRUE(stack.redo());
    EXPECT_EQ(countLabel(labelMap, 1), 500);

    EXPECT_TRUE(stack.redo());
    EXPECT_EQ(countLabel(labelMap, 2), 500);
}

TEST(SnapshotCommand, MixedWithBrushStrokeCommands) {
    // Verify snapshot and diff-based commands interoperate in the same stack
    auto labelMap = createLabelMap(32, 32, 32);
    SegmentationCommandStack stack;

    // Snapshot command: bulk fill
    {
        auto cmd = std::make_unique<SnapshotCommand>(labelMap, "Threshold");
        auto* buf = labelMap->GetBufferPointer();
        for (int i = 0; i < 200; ++i) buf[i] = 1;
        cmd->captureAfterState();
        stack.execute(std::move(cmd));
    }

    EXPECT_EQ(countLabel(labelMap, 1), 200);
    EXPECT_EQ(stack.undoCount(), 1u);

    // Undo the snapshot command
    EXPECT_TRUE(stack.undo());
    EXPECT_EQ(countLabel(labelMap, 1), 0);

    // Redo
    EXPECT_TRUE(stack.redo());
    EXPECT_EQ(countLabel(labelMap, 1), 200);
}

// =============================================================================
// Memory budget test
// =============================================================================

TEST(SnapshotCommand, TwentyStepHistoryWithinMemoryBudget) {
    // Simulate 20 snapshot operations on a 128^3 label map
    // Each operation labels ~5% of voxels differently
    auto labelMap = createLabelMap(128, 128, 128);
    SegmentationCommandStack stack(20);

    size_t totalCommandMemory = 0;

    for (int step = 0; step < 20; ++step) {
        auto cmd = std::make_unique<SnapshotCommand>(
            labelMap, "Step " + std::to_string(step));

        // Simulate a segmentation operation: fill a different sphere each step
        auto* buf = labelMap->GetBufferPointer();
        int cx = 32 + (step % 4) * 20;
        int cy = 32 + ((step / 4) % 4) * 20;
        int cz = 64;
        int radius = 15;

        for (int z = 0; z < 128; ++z) {
            for (int y = 0; y < 128; ++y) {
                for (int x = 0; x < 128; ++x) {
                    int dx = x - cx;
                    int dy = y - cy;
                    int dz = z - cz;
                    if (dx*dx + dy*dy + dz*dz < radius*radius) {
                        buf[z * 128 * 128 + y * 128 + x] =
                            static_cast<uint8_t>((step % 5) + 1);
                    }
                }
            }
        }

        cmd->captureAfterState();
        totalCommandMemory += cmd->memoryUsage();
        stack.execute(std::move(cmd));
    }

    EXPECT_EQ(stack.undoCount(), 20u);

    // Total memory for 20 commands should be well under 100MB
    // Each command stores ~2 compressed snapshots of a mostly-zero 128^3 map
    // Raw size: 2M bytes, compressed: typically <50KB each → 20 * 2 * 50KB = 2MB
    EXPECT_LT(totalCommandMemory, 100 * 1024 * 1024)
        << "20-step snapshot history should stay under 100MB budget"
        << "\n  Actual memory: " << totalCommandMemory / 1024 << " KB";

    // Verify undo all 20 steps restores to empty
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(stack.undo());
    }
    EXPECT_EQ(countLabel(labelMap, 0), 128 * 128 * 128);
    EXPECT_FALSE(stack.canUndo());
}

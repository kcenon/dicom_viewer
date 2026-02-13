#include "services/segmentation/label_manager.hpp"
#include "services/segmentation/segmentation_label.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include <itkImageRegionIterator.h>

namespace dicom_viewer::services::test {

// ============================================================================
// SegmentationLabel Tests
// ============================================================================

TEST(SegmentationLabelTest, DefaultConstruction) {
    SegmentationLabel label;

    EXPECT_EQ(label.id, 0);
    EXPECT_TRUE(label.name.empty());
    EXPECT_EQ(label.opacity, 1.0);
    EXPECT_TRUE(label.visible);
    EXPECT_FALSE(label.isValid());
}

TEST(SegmentationLabelTest, ParameterizedConstruction) {
    SegmentationLabel label{1, "Liver", LabelColor(0.8f, 0.2f, 0.2f)};

    EXPECT_EQ(label.id, 1);
    EXPECT_EQ(label.name, "Liver");
    EXPECT_FLOAT_EQ(label.color.r, 0.8f);
    EXPECT_FLOAT_EQ(label.color.g, 0.2f);
    EXPECT_FLOAT_EQ(label.color.b, 0.2f);
    EXPECT_TRUE(label.isValid());
}

TEST(SegmentationLabelTest, ClearStatistics) {
    SegmentationLabel label{1, "Test", LabelColor()};
    label.volumeML = 100.0;
    label.meanHU = 50.0;
    label.voxelCount = 1000;

    EXPECT_TRUE(label.volumeML.has_value());
    EXPECT_TRUE(label.meanHU.has_value());
    EXPECT_TRUE(label.voxelCount.has_value());

    label.clearStatistics();

    EXPECT_FALSE(label.volumeML.has_value());
    EXPECT_FALSE(label.meanHU.has_value());
    EXPECT_FALSE(label.voxelCount.has_value());
}

// ============================================================================
// LabelColor Tests
// ============================================================================

TEST(LabelColorTest, DefaultConstruction) {
    LabelColor color;

    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_FLOAT_EQ(color.g, 0.0f);
    EXPECT_FLOAT_EQ(color.b, 0.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST(LabelColorTest, RGBAConstruction) {
    LabelColor color(0.5f, 0.6f, 0.7f, 0.8f);

    EXPECT_FLOAT_EQ(color.r, 0.5f);
    EXPECT_FLOAT_EQ(color.g, 0.6f);
    EXPECT_FLOAT_EQ(color.b, 0.7f);
    EXPECT_FLOAT_EQ(color.a, 0.8f);
}

TEST(LabelColorTest, FromRGBA8) {
    LabelColor color = LabelColor::fromRGBA8(255, 128, 0, 255);

    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_NEAR(color.g, 0.502f, 0.01f);
    EXPECT_FLOAT_EQ(color.b, 0.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST(LabelColorTest, ToRGBA8) {
    LabelColor color(1.0f, 0.5f, 0.0f, 1.0f);
    auto rgba = color.toRGBA8();

    EXPECT_EQ(rgba[0], 255);
    EXPECT_EQ(rgba[1], 127);
    EXPECT_EQ(rgba[2], 0);
    EXPECT_EQ(rgba[3], 255);
}

TEST(LabelColorTest, ClampValues) {
    LabelColor color(-0.5f, 1.5f, 0.5f, 2.0f);

    EXPECT_FLOAT_EQ(color.r, 0.0f);
    EXPECT_FLOAT_EQ(color.g, 1.0f);
    EXPECT_FLOAT_EQ(color.b, 0.5f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

// ============================================================================
// LabelColorPalette Tests
// ============================================================================

TEST(LabelColorPaletteTest, BackgroundIsTransparent) {
    LabelColor bg = LabelColorPalette::getColor(0);

    EXPECT_FLOAT_EQ(bg.r, 0.0f);
    EXPECT_FLOAT_EQ(bg.g, 0.0f);
    EXPECT_FLOAT_EQ(bg.b, 0.0f);
    EXPECT_FLOAT_EQ(bg.a, 0.0f);
}

TEST(LabelColorPaletteTest, DistinctColorsForDifferentLabels) {
    LabelColor color1 = LabelColorPalette::getColor(1);
    LabelColor color2 = LabelColorPalette::getColor(2);
    LabelColor color3 = LabelColorPalette::getColor(3);

    EXPECT_FALSE(color1 == color2);
    EXPECT_FALSE(color2 == color3);
    EXPECT_FALSE(color1 == color3);
}

TEST(LabelColorPaletteTest, CyclesAfter20Labels) {
    LabelColor color1 = LabelColorPalette::getColor(1);
    LabelColor color21 = LabelColorPalette::getColor(21);

    EXPECT_EQ(color1, color21);
}

// ============================================================================
// LabelManager Tests
// ============================================================================

class LabelManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<LabelManager>();
    }

    std::unique_ptr<LabelManager> manager_;
};

TEST_F(LabelManagerTest, DefaultState) {
    EXPECT_FALSE(manager_->hasLabelMap());
    EXPECT_EQ(manager_->getLabelCount(), 0);
    EXPECT_EQ(manager_->getActiveLabel(), 0);
}

TEST_F(LabelManagerTest, InitializeLabelMap) {
    auto result = manager_->initializeLabelMap(512, 512, 100);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(manager_->hasLabelMap());

    auto labelMap = manager_->getLabelMap();
    EXPECT_NE(labelMap, nullptr);

    auto size = labelMap->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], 512);
    EXPECT_EQ(size[1], 512);
    EXPECT_EQ(size[2], 100);
}

TEST_F(LabelManagerTest, InitializeLabelMapInvalidDimensions) {
    auto result = manager_->initializeLabelMap(0, 512, 100);
    EXPECT_FALSE(result.has_value());

    result = manager_->initializeLabelMap(512, -1, 100);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LabelManagerTest, AddLabel) {
    auto result = manager_->addLabel("Liver");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->get().id, 1);
    EXPECT_EQ(result->get().name, "Liver");
    EXPECT_EQ(manager_->getLabelCount(), 1);
}

TEST_F(LabelManagerTest, AddLabelWithColor) {
    LabelColor color(0.5f, 0.5f, 0.5f);
    auto result = manager_->addLabel("Kidney", color);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->get().color.r, 0.5f);
}

TEST_F(LabelManagerTest, AddLabelWithId) {
    LabelColor color(0.8f, 0.2f, 0.2f);
    auto result = manager_->addLabel(10, "Spleen", color);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->get().id, 10);
    EXPECT_EQ(result->get().name, "Spleen");
}

TEST_F(LabelManagerTest, AddLabelIdZeroFails) {
    auto result = manager_->addLabel(0, "Background", LabelColor());
    EXPECT_FALSE(result.has_value());
}

TEST_F(LabelManagerTest, AddDuplicateLabelIdFails) {
    (void)manager_->addLabel(5, "First", LabelColor());
    auto result = manager_->addLabel(5, "Second", LabelColor());

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(manager_->getLabelCount(), 1);
}

TEST_F(LabelManagerTest, RemoveLabel) {
    (void)manager_->addLabel("Liver");
    (void)manager_->addLabel("Kidney");

    EXPECT_EQ(manager_->getLabelCount(), 2);

    auto result = manager_->removeLabel(1, false);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager_->getLabelCount(), 1);
    EXPECT_FALSE(manager_->hasLabel(1));
    EXPECT_TRUE(manager_->hasLabel(2));
}

TEST_F(LabelManagerTest, RemoveNonexistentLabelFails) {
    auto result = manager_->removeLabel(99, false);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LabelManagerTest, GetLabel) {
    (void)manager_->addLabel(5, "Test", LabelColor(0.1f, 0.2f, 0.3f));

    auto* label = manager_->getLabel(5);
    EXPECT_NE(label, nullptr);
    EXPECT_EQ(label->name, "Test");

    auto* nonexistent = manager_->getLabel(99);
    EXPECT_EQ(nonexistent, nullptr);
}

TEST_F(LabelManagerTest, GetAllLabels) {
    (void)manager_->addLabel(3, "C", LabelColor());
    (void)manager_->addLabel(1, "A", LabelColor());
    (void)manager_->addLabel(2, "B", LabelColor());

    auto labels = manager_->getAllLabels();

    EXPECT_EQ(labels.size(), 3);
    EXPECT_EQ(labels[0].id, 1);
    EXPECT_EQ(labels[1].id, 2);
    EXPECT_EQ(labels[2].id, 3);
}

TEST_F(LabelManagerTest, SetActiveLabel) {
    (void)manager_->addLabel("Liver");

    auto result = manager_->setActiveLabel(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager_->getActiveLabel(), 1);
}

TEST_F(LabelManagerTest, SetActiveLabelNonexistentFails) {
    auto result = manager_->setActiveLabel(99);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LabelManagerTest, SetActiveLabelToZero) {
    (void)manager_->addLabel("Liver");
    (void)manager_->setActiveLabel(1);

    auto result = manager_->setActiveLabel(0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager_->getActiveLabel(), 0);
}

TEST_F(LabelManagerTest, RemoveActiveLabelResetsActive) {
    (void)manager_->addLabel("Liver");
    (void)manager_->setActiveLabel(1);
    EXPECT_EQ(manager_->getActiveLabel(), 1);

    (void)manager_->removeLabel(1, false);
    EXPECT_EQ(manager_->getActiveLabel(), 0);
}

TEST_F(LabelManagerTest, SetLabelName) {
    (void)manager_->addLabel("OldName");

    auto result = manager_->setLabelName(1, "NewName");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager_->getLabel(1)->name, "NewName");
}

TEST_F(LabelManagerTest, SetLabelColor) {
    (void)manager_->addLabel("Test");
    LabelColor newColor(0.9f, 0.8f, 0.7f);

    auto result = manager_->setLabelColor(1, newColor);
    EXPECT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(manager_->getLabel(1)->color.r, 0.9f);
}

TEST_F(LabelManagerTest, SetLabelOpacity) {
    (void)manager_->addLabel("Test");

    auto result = manager_->setLabelOpacity(1, 0.5);
    EXPECT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(manager_->getLabel(1)->opacity, 0.5);
}

TEST_F(LabelManagerTest, SetLabelOpacityClamped) {
    (void)manager_->addLabel("Test");

    (void)manager_->setLabelOpacity(1, 1.5);
    EXPECT_DOUBLE_EQ(manager_->getLabel(1)->opacity, 1.0);

    (void)manager_->setLabelOpacity(1, -0.5);
    EXPECT_DOUBLE_EQ(manager_->getLabel(1)->opacity, 0.0);
}

TEST_F(LabelManagerTest, SetLabelVisibility) {
    (void)manager_->addLabel("Test");

    auto result = manager_->setLabelVisibility(1, false);
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(manager_->getLabel(1)->visible);
}

TEST_F(LabelManagerTest, ToggleLabelVisibility) {
    (void)manager_->addLabel("Test");
    EXPECT_TRUE(manager_->getLabel(1)->visible);

    auto result = manager_->toggleLabelVisibility(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
    EXPECT_FALSE(manager_->getLabel(1)->visible);

    result = manager_->toggleLabelVisibility(1);
    EXPECT_TRUE(*result);
    EXPECT_TRUE(manager_->getLabel(1)->visible);
}

TEST_F(LabelManagerTest, ClearAllLabels) {
    (void)manager_->addLabel("A");
    (void)manager_->addLabel("B");
    (void)manager_->addLabel("C");
    (void)manager_->setActiveLabel(2);

    manager_->clearAllLabels(false);

    EXPECT_EQ(manager_->getLabelCount(), 0);
    EXPECT_EQ(manager_->getActiveLabel(), 0);
}

TEST_F(LabelManagerTest, LabelChangeCallback) {
    int callbackCount = 0;
    manager_->setLabelChangeCallback([&callbackCount]() {
        ++callbackCount;
    });

    (void)manager_->addLabel("Test");
    EXPECT_EQ(callbackCount, 1);

    (void)manager_->setLabelName(1, "NewName");
    EXPECT_EQ(callbackCount, 2);

    (void)manager_->removeLabel(1, false);
    EXPECT_EQ(callbackCount, 3);
}

TEST_F(LabelManagerTest, MoveConstruction) {
    (void)manager_->addLabel("Test");
    (void)manager_->setActiveLabel(1);

    LabelManager moved = std::move(*manager_);

    EXPECT_EQ(moved.getLabelCount(), 1);
    EXPECT_EQ(moved.getActiveLabel(), 1);
    EXPECT_EQ(moved.getLabel(1)->name, "Test");
}

// ============================================================================
// Import/Export Tests
// ============================================================================

class LabelManagerIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<LabelManager>();
        (void)manager_->initializeLabelMap(64, 64, 10);

        tempDir_ = std::filesystem::temp_directory_path() / "label_manager_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    std::unique_ptr<LabelManager> manager_;
    std::filesystem::path tempDir_;
};

TEST_F(LabelManagerIOTest, ExportLabelMetadata) {
    (void)manager_->addLabel(1, "Liver", LabelColor(0.8f, 0.2f, 0.2f));
    (void)manager_->addLabel(2, "Kidney", LabelColor(0.2f, 0.8f, 0.2f));

    auto path = tempDir_ / "labels.json";
    auto result = manager_->exportLabelMetadata(path);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(path));

    std::ifstream file(path);
    EXPECT_TRUE(file.good());
}

TEST_F(LabelManagerIOTest, ImportLabelMetadata) {
    // Create a JSON file
    auto path = tempDir_ / "labels.json";
    {
        std::ofstream file(path);
        file << R"({
            "version": "1.0",
            "labels": [
                {
                    "id": 1,
                    "name": "Liver",
                    "color": {"r": 0.8, "g": 0.2, "b": 0.2, "a": 1.0},
                    "opacity": 0.7,
                    "visible": true
                }
            ]
        })";
    }

    auto result = manager_->importLabelMetadata(path);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager_->getLabelCount(), 1);

    auto* label = manager_->getLabel(1);
    EXPECT_NE(label, nullptr);
    EXPECT_EQ(label->name, "Liver");
    EXPECT_DOUBLE_EQ(label->opacity, 0.7);
}

TEST_F(LabelManagerIOTest, ImportNonexistentFileFails) {
    auto result = manager_->importLabelMetadata(tempDir_ / "nonexistent.json");
    EXPECT_FALSE(result.has_value());
}

TEST_F(LabelManagerIOTest, ExportSegmentationNIfTI) {
    auto path = tempDir_ / "segmentation.nii.gz";
    auto result = manager_->exportSegmentation(path, SegmentationFormat::NIfTI);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(LabelManagerIOTest, ExportSegmentationNRRD) {
    auto path = tempDir_ / "segmentation.nrrd";
    auto result = manager_->exportSegmentation(path, SegmentationFormat::NRRD);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(LabelManagerIOTest, ImportSegmentation) {
    // First export
    auto path = tempDir_ / "segmentation.nii.gz";
    (void)manager_->exportSegmentation(path, SegmentationFormat::NIfTI);

    // Create new manager and import
    LabelManager newManager;
    auto result = newManager.importSegmentation(path);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(newManager.hasLabelMap());
}

TEST_F(LabelManagerIOTest, ExportWithoutLabelMapFails) {
    LabelManager emptyManager;
    auto result = emptyManager.exportSegmentation(
        tempDir_ / "test.nii.gz",
        SegmentationFormat::NIfTI
    );

    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Edge case and algorithmic correctness tests (Issue #204)
// ============================================================================

TEST_F(LabelManagerTest, ComputeLabelStatisticsWithSourceImage) {
    // Initialize label map and source image
    ASSERT_TRUE(manager_->initializeLabelMap(10, 10, 10).has_value());
    (void)manager_->addLabel("Liver");

    // Create a source image with known HU values
    auto sourceImage = LabelManager::SourceImageType::New();
    LabelManager::SourceImageType::SizeType size = {{10, 10, 10}};
    LabelManager::SourceImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex({{0, 0, 0}});
    sourceImage->SetRegions(region);
    sourceImage->Allocate();

    // Fill source with value 50 HU everywhere
    itk::ImageRegionIterator<LabelManager::SourceImageType> srcIt(sourceImage, region);
    for (srcIt.GoToBegin(); !srcIt.IsAtEnd(); ++srcIt) {
        srcIt.Set(50);
    }

    // Paint label 1 into some voxels
    auto labelMap = manager_->getLabelMap();
    itk::ImageRegionIterator<LabelManager::LabelMapType> lblIt(labelMap, region);
    int painted = 0;
    for (lblIt.GoToBegin(); !lblIt.IsAtEnd(); ++lblIt) {
        auto idx = lblIt.GetIndex();
        if (idx[0] >= 2 && idx[0] < 8 && idx[1] >= 2 && idx[1] < 8 && idx[2] >= 2 && idx[2] < 8) {
            lblIt.Set(1);
            ++painted;
        }
    }

    auto result = manager_->computeLabelStatistics(1, sourceImage);
    ASSERT_TRUE(result.has_value());

    // Verify the label now has statistics populated
    auto* label = manager_->getLabel(1);
    ASSERT_NE(label, nullptr);
    EXPECT_TRUE(label->voxelCount.has_value());
    if (label->voxelCount.has_value()) {
        EXPECT_EQ(label->voxelCount.value(), painted);
    }
    if (label->meanHU.has_value()) {
        EXPECT_NEAR(label->meanHU.value(), 50.0, 1.0);
    }
}

TEST_F(LabelManagerTest, MaxLabelsCapacity255) {
    // Verify LabelManager can hold up to 255 labels (MAX_LABELS)
    int addedCount = 0;
    for (int i = 1; i <= 255; ++i) {
        auto result = manager_->addLabel("Label_" + std::to_string(i));
        if (result.has_value()) {
            ++addedCount;
        } else {
            break;
        }
    }

    EXPECT_EQ(addedCount, 255) << "Should support exactly 255 labels";
    EXPECT_EQ(manager_->getLabelCount(), 255);

    // Attempting to add label 256 should fail
    auto overflow = manager_->addLabel("Overflow");
    EXPECT_FALSE(overflow.has_value())
        << "Adding label beyond 255 should fail";
}

TEST_F(LabelManagerIOTest, ImportCorruptedJsonFails) {
    // Write invalid JSON to file
    auto path = tempDir_ / "corrupt.json";
    {
        std::ofstream file(path);
        file << "{ this is not valid json !!!";
    }

    auto result = manager_->importLabelMetadata(path);
    EXPECT_FALSE(result.has_value())
        << "Corrupted JSON should fail gracefully";
}

}  // namespace dicom_viewer::services::test

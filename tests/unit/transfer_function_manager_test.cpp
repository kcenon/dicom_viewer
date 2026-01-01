#include <gtest/gtest.h>

#include "services/transfer_function_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace dicom_viewer::services;

class TransferFunctionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<TransferFunctionManager>();
        testDir = std::filesystem::temp_directory_path() / "dicom_viewer_test";
        std::filesystem::create_directories(testDir);
    }

    void TearDown() override {
        manager.reset();
        std::filesystem::remove_all(testDir);
    }

    TransferFunctionPreset createTestPreset(const std::string& name = "TestPreset") {
        return TransferFunctionManager::createPreset(
            name,
            500.0,
            100.0,
            {{0, 0.0, 0.0, 0.0}, {500, 1.0, 0.8, 0.6}, {1000, 1.0, 1.0, 1.0}},
            {{0, 0.0}, {500, 0.5}, {1000, 1.0}},
            {{0, 0.0}, {100, 1.0}}
        );
    }

    std::unique_ptr<TransferFunctionManager> manager;
    std::filesystem::path testDir;
};

// Test construction
TEST_F(TransferFunctionManagerTest, DefaultConstruction) {
    EXPECT_NE(manager, nullptr);
}

// Test move semantics
TEST_F(TransferFunctionManagerTest, MoveConstructor) {
    auto names = manager->getPresetNames();
    TransferFunctionManager moved(std::move(*manager));
    EXPECT_EQ(moved.getPresetNames(), names);
}

TEST_F(TransferFunctionManagerTest, MoveAssignment) {
    auto names = manager->getPresetNames();
    TransferFunctionManager other;
    other = std::move(*manager);
    EXPECT_EQ(other.getPresetNames(), names);
}

// Test built-in presets
TEST_F(TransferFunctionManagerTest, GetBuiltInPresetNames) {
    auto names = manager->getBuiltInPresetNames();
    EXPECT_EQ(names.size(), 6);
}

TEST_F(TransferFunctionManagerTest, BuiltInPresetsContainExpectedNames) {
    auto names = manager->getBuiltInPresetNames();
    std::vector<std::string> expected = {
        "CT Abdomen", "CT Angio", "CT Bone", "CT Lung", "CT Soft Tissue", "MRI Default"
    };
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(names, expected);
}

TEST_F(TransferFunctionManagerTest, GetPresetCTBone) {
    auto result = manager->getPreset("CT Bone");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "CT Bone");
    EXPECT_EQ(result->windowWidth, 2000);
    EXPECT_EQ(result->windowCenter, 400);
}

TEST_F(TransferFunctionManagerTest, GetPresetMRIDefault) {
    auto result = manager->getPreset("MRI Default");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "MRI Default");
}

TEST_F(TransferFunctionManagerTest, IsBuiltInPresetReturnsTrue) {
    EXPECT_TRUE(manager->isBuiltInPreset("CT Bone"));
    EXPECT_TRUE(manager->isBuiltInPreset("CT Lung"));
    EXPECT_TRUE(manager->isBuiltInPreset("MRI Default"));
}

TEST_F(TransferFunctionManagerTest, IsBuiltInPresetReturnsFalse) {
    EXPECT_FALSE(manager->isBuiltInPreset("Custom Preset"));
    EXPECT_FALSE(manager->isBuiltInPreset("NonExistent"));
}

// Test custom presets
TEST_F(TransferFunctionManagerTest, AddCustomPreset) {
    auto preset = createTestPreset();
    auto result = manager->addCustomPreset(preset);
    ASSERT_TRUE(result.has_value());
}

TEST_F(TransferFunctionManagerTest, GetCustomPreset) {
    auto preset = createTestPreset();
    manager->addCustomPreset(preset);

    auto result = manager->getPreset("TestPreset");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "TestPreset");
    EXPECT_EQ(result->windowWidth, 500.0);
    EXPECT_EQ(result->windowCenter, 100.0);
}

TEST_F(TransferFunctionManagerTest, GetCustomPresetNames) {
    manager->addCustomPreset(createTestPreset("Custom1"));
    manager->addCustomPreset(createTestPreset("Custom2"));

    auto names = manager->getCustomPresetNames();
    EXPECT_EQ(names.size(), 2);
}

TEST_F(TransferFunctionManagerTest, AddDuplicatePresetFails) {
    auto preset = createTestPreset();
    manager->addCustomPreset(preset);

    auto result = manager->addCustomPreset(preset);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::DuplicatePreset);
}

TEST_F(TransferFunctionManagerTest, AddDuplicatePresetWithOverwrite) {
    auto preset = createTestPreset();
    manager->addCustomPreset(preset);

    preset.windowWidth = 1000.0;
    auto result = manager->addCustomPreset(preset, true);
    ASSERT_TRUE(result.has_value());

    auto loaded = manager->getPreset("TestPreset");
    EXPECT_EQ(loaded->windowWidth, 1000.0);
}

TEST_F(TransferFunctionManagerTest, CannotOverwriteBuiltInPreset) {
    TransferFunctionPreset preset{
        .name = "CT Bone",
        .windowWidth = 1000,
        .windowCenter = 200
    };

    auto result = manager->addCustomPreset(preset, true);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::DuplicatePreset);
}

TEST_F(TransferFunctionManagerTest, RemoveCustomPreset) {
    manager->addCustomPreset(createTestPreset());
    auto result = manager->removeCustomPreset("TestPreset");
    ASSERT_TRUE(result.has_value());

    auto loaded = manager->getPreset("TestPreset");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(TransferFunctionManagerTest, RemoveNonExistentPresetFails) {
    auto result = manager->removeCustomPreset("NonExistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::PresetNotFound);
}

TEST_F(TransferFunctionManagerTest, CannotRemoveBuiltInPreset) {
    auto result = manager->removeCustomPreset("CT Bone");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::PresetNotFound);
}

// Test preset not found
TEST_F(TransferFunctionManagerTest, GetNonExistentPresetFails) {
    auto result = manager->getPreset("NonExistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::PresetNotFound);
}

// Test save/load
TEST_F(TransferFunctionManagerTest, SaveCustomPresets) {
    manager->addCustomPreset(createTestPreset("Save1"));
    manager->addCustomPreset(createTestPreset("Save2"));

    auto filePath = testDir / "presets.json";
    auto result = manager->saveCustomPresets(filePath);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(filePath));
}

TEST_F(TransferFunctionManagerTest, LoadCustomPresets) {
    manager->addCustomPreset(createTestPreset("Load1"));
    manager->addCustomPreset(createTestPreset("Load2"));

    auto filePath = testDir / "presets.json";
    manager->saveCustomPresets(filePath);

    // Create a new manager and load
    TransferFunctionManager newManager;
    auto result = newManager.loadCustomPresets(filePath);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2);

    EXPECT_TRUE(newManager.getPreset("Load1").has_value());
    EXPECT_TRUE(newManager.getPreset("Load2").has_value());
}

TEST_F(TransferFunctionManagerTest, LoadCustomPresetsWithMerge) {
    manager->addCustomPreset(createTestPreset("Existing"));

    // Create a file with different presets
    auto filePath = testDir / "presets.json";
    {
        TransferFunctionManager temp;
        temp.addCustomPreset(createTestPreset("New1"));
        temp.addCustomPreset(createTestPreset("New2"));
        temp.saveCustomPresets(filePath);
    }

    auto result = manager->loadCustomPresets(filePath, true);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2);

    // Original preset should still exist
    EXPECT_TRUE(manager->getPreset("Existing").has_value());
    EXPECT_TRUE(manager->getPreset("New1").has_value());
    EXPECT_TRUE(manager->getPreset("New2").has_value());
}

TEST_F(TransferFunctionManagerTest, LoadCustomPresetsWithReplace) {
    manager->addCustomPreset(createTestPreset("Existing"));

    auto filePath = testDir / "presets.json";
    {
        TransferFunctionManager temp;
        temp.addCustomPreset(createTestPreset("New1"));
        temp.saveCustomPresets(filePath);
    }

    auto result = manager->loadCustomPresets(filePath, false);
    ASSERT_TRUE(result.has_value());

    // Original preset should be replaced
    EXPECT_FALSE(manager->getPreset("Existing").has_value());
    EXPECT_TRUE(manager->getPreset("New1").has_value());
}

TEST_F(TransferFunctionManagerTest, LoadNonExistentFileFails) {
    auto result = manager->loadCustomPresets(testDir / "nonexistent.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::FileNotFound);
}

// Test export/import single preset
TEST_F(TransferFunctionManagerTest, ExportPreset) {
    auto filePath = testDir / "ct_bone.json";
    auto result = manager->exportPreset("CT Bone", filePath);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(filePath));
}

TEST_F(TransferFunctionManagerTest, ExportNonExistentPresetFails) {
    auto result = manager->exportPreset("NonExistent", testDir / "preset.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::PresetNotFound);
}

TEST_F(TransferFunctionManagerTest, ImportPreset) {
    auto filePath = testDir / "custom.json";
    {
        TransferFunctionManager temp;
        temp.addCustomPreset(createTestPreset("ImportTest"));
        temp.saveCustomPresets(filePath);
    }

    auto result = manager->importPreset(filePath);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "ImportTest");
    EXPECT_TRUE(manager->getPreset("ImportTest").has_value());
}

TEST_F(TransferFunctionManagerTest, ImportNonExistentFileFails) {
    auto result = manager->importPreset(testDir / "nonexistent.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferFunctionError::FileNotFound);
}

// Test createPreset static method
TEST_F(TransferFunctionManagerTest, CreatePresetStatic) {
    auto preset = TransferFunctionManager::createPreset(
        "StaticTest",
        800.0,
        200.0,
        {{0, 0, 0, 0}},
        {{0, 0}},
        {{0, 0}}
    );

    EXPECT_EQ(preset.name, "StaticTest");
    EXPECT_EQ(preset.windowWidth, 800.0);
    EXPECT_EQ(preset.windowCenter, 200.0);
    EXPECT_EQ(preset.colorPoints.size(), 1);
    EXPECT_EQ(preset.opacityPoints.size(), 1);
    EXPECT_EQ(preset.gradientOpacityPoints.size(), 1);
}

// Test getDefaultPresetsDirectory
TEST_F(TransferFunctionManagerTest, GetDefaultPresetsDirectory) {
    auto path = TransferFunctionManager::getDefaultPresetsDirectory();
    EXPECT_FALSE(path.empty());
}

// Test all preset names combined
TEST_F(TransferFunctionManagerTest, GetPresetNamesIncludesAll) {
    manager->addCustomPreset(createTestPreset("Custom1"));

    auto names = manager->getPresetNames();
    EXPECT_EQ(names.size(), 7); // 6 built-in + 1 custom

    // Should be sorted
    EXPECT_TRUE(std::is_sorted(names.begin(), names.end()));
}

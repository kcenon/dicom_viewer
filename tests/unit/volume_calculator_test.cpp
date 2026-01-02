#include "services/measurement/volume_calculator.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace dicom_viewer::services {
namespace {

class VolumeCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple label map (10x10x10)
        testLabelMap_ = LabelMapType::New();

        LabelMapType::SizeType size;
        size[0] = 10;
        size[1] = 10;
        size[2] = 10;

        LabelMapType::IndexType start;
        start.Fill(0);

        LabelMapType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        testLabelMap_->SetRegions(region);
        testLabelMap_->Allocate();
        testLabelMap_->FillBuffer(0);

        // Set spacing (1mm x 1mm x 1mm)
        LabelMapType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        testLabelMap_->SetSpacing(spacing);

        // Fill label 1: center cube (3-7, 3-7, 3-7) = 5x5x5 = 125 voxels
        for (int z = 3; z <= 7; ++z) {
            for (int y = 3; y <= 7; ++y) {
                for (int x = 3; x <= 7; ++x) {
                    LabelMapType::IndexType idx = {x, y, z};
                    testLabelMap_->SetPixel(idx, 1);
                }
            }
        }

        // Fill label 2: corner cube (0-1, 0-1, 0-1) = 2x2x2 = 8 voxels
        for (int z = 0; z <= 1; ++z) {
            for (int y = 0; y <= 1; ++y) {
                for (int x = 0; x <= 1; ++x) {
                    LabelMapType::IndexType idx = {x, y, z};
                    testLabelMap_->SetPixel(idx, 2);
                }
            }
        }
    }

    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(testCsvPath_)) {
            std::filesystem::remove(testCsvPath_);
        }
        if (std::filesystem::exists(testTrackingCsvPath_)) {
            std::filesystem::remove(testTrackingCsvPath_);
        }
    }

    using LabelMapType = VolumeCalculator::LabelMapType;
    using SpacingType = VolumeCalculator::SpacingType;

    LabelMapType::Pointer testLabelMap_;
    SpacingType testSpacing_ = {1.0, 1.0, 1.0};
    std::filesystem::path testCsvPath_ = std::filesystem::temp_directory_path() / "test_volume.csv";
    std::filesystem::path testTrackingCsvPath_ = std::filesystem::temp_directory_path() / "test_tracking.csv";
};

// =============================================================================
// VolumeResult struct tests
// =============================================================================

TEST_F(VolumeCalculatorTest, VolumeResultDefaultValues) {
    VolumeResult result;

    EXPECT_EQ(result.labelId, 0);
    EXPECT_TRUE(result.labelName.empty());
    EXPECT_EQ(result.voxelCount, 0);
    EXPECT_DOUBLE_EQ(result.volumeMm3, 0.0);
    EXPECT_DOUBLE_EQ(result.volumeCm3, 0.0);
    EXPECT_DOUBLE_EQ(result.volumeML, 0.0);
    EXPECT_FALSE(result.surfaceAreaMm2.has_value());
    EXPECT_FALSE(result.sphericity.has_value());
}

TEST_F(VolumeCalculatorTest, VolumeResultToString) {
    VolumeResult result;
    result.labelId = 1;
    result.labelName = "Liver";
    result.voxelCount = 1000;
    result.volumeMm3 = 1000.0;
    result.volumeCm3 = 1.0;
    result.volumeML = 1.0;
    result.surfaceAreaMm2 = 600.0;
    result.sphericity = 0.85;
    result.boundingBoxMm = {10.0, 10.0, 10.0};

    std::string str = result.toString();

    EXPECT_NE(str.find("Liver"), std::string::npos);
    EXPECT_NE(str.find("1000"), std::string::npos);
    EXPECT_NE(str.find("mm^3"), std::string::npos);
    EXPECT_NE(str.find("cm^3"), std::string::npos);
    EXPECT_NE(str.find("mL"), std::string::npos);
    EXPECT_NE(str.find("Surface"), std::string::npos);
    EXPECT_NE(str.find("Sphericity"), std::string::npos);
}

TEST_F(VolumeCalculatorTest, VolumeResultGetCsvHeader) {
    auto header = VolumeResult::getCsvHeader();

    EXPECT_FALSE(header.empty());
    EXPECT_EQ(header[0], "LabelID");
    EXPECT_EQ(header[1], "LabelName");
    EXPECT_EQ(header[2], "VoxelCount");
}

TEST_F(VolumeCalculatorTest, VolumeResultGetCsvRow) {
    VolumeResult result;
    result.labelId = 1;
    result.labelName = "TestLabel";
    result.voxelCount = 125;
    result.volumeMm3 = 125.0;
    result.volumeCm3 = 0.125;
    result.volumeML = 0.125;

    auto row = result.getCsvRow();

    EXPECT_FALSE(row.empty());
    EXPECT_EQ(row[0], "1");
    EXPECT_EQ(row[1], "TestLabel");
    EXPECT_EQ(row[2], "125");
}

// =============================================================================
// VolumeError tests
// =============================================================================

TEST_F(VolumeCalculatorTest, VolumeErrorSuccess) {
    VolumeError error;
    EXPECT_TRUE(error.isSuccess());
    EXPECT_EQ(error.code, VolumeError::Code::Success);
}

TEST_F(VolumeCalculatorTest, VolumeErrorToString) {
    VolumeError error;
    error.code = VolumeError::Code::InvalidLabelMap;
    error.message = "test message";

    std::string result = error.toString();
    EXPECT_NE(result.find("Invalid label map"), std::string::npos);
    EXPECT_NE(result.find("test message"), std::string::npos);
}

// =============================================================================
// VolumeCalculator basic tests
// =============================================================================

TEST_F(VolumeCalculatorTest, CalculatorDefaultConstruction) {
    VolumeCalculator calculator;
    // Should not crash
}

TEST_F(VolumeCalculatorTest, CalculatorMoveConstruction) {
    VolumeCalculator calculator1;
    VolumeCalculator calculator2(std::move(calculator1));
    // Should not crash
}

TEST_F(VolumeCalculatorTest, CalculatorNullLabelMapError) {
    VolumeCalculator calculator;

    auto result = calculator.calculate(nullptr, 1, testSpacing_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, VolumeError::Code::InvalidLabelMap);
}

TEST_F(VolumeCalculatorTest, CalculatorBackgroundLabelError) {
    VolumeCalculator calculator;

    auto result = calculator.calculate(testLabelMap_, 0, testSpacing_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, VolumeError::Code::LabelNotFound);
}

TEST_F(VolumeCalculatorTest, CalculatorInvalidSpacingError) {
    VolumeCalculator calculator;
    SpacingType invalidSpacing = {0.0, 1.0, 1.0};

    auto result = calculator.calculate(testLabelMap_, 1, invalidSpacing);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, VolumeError::Code::InvalidSpacing);
}

TEST_F(VolumeCalculatorTest, CalculatorLabelNotFoundError) {
    VolumeCalculator calculator;

    auto result = calculator.calculate(testLabelMap_, 99, testSpacing_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, VolumeError::Code::LabelNotFound);
}

// =============================================================================
// Volume calculation tests
// =============================================================================

TEST_F(VolumeCalculatorTest, CalculateSingleLabelVolume) {
    VolumeCalculator calculator;

    auto result = calculator.calculate(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_EQ(result->labelId, 1);
        EXPECT_EQ(result->voxelCount, 125);  // 5x5x5 cube
        EXPECT_DOUBLE_EQ(result->volumeMm3, 125.0);  // 125 mm^3
        EXPECT_DOUBLE_EQ(result->volumeCm3, 0.125);  // 0.125 cm^3
        EXPECT_DOUBLE_EQ(result->volumeML, 0.125);   // 0.125 mL
    }
}

TEST_F(VolumeCalculatorTest, CalculateSingleLabelWithDifferentSpacing) {
    VolumeCalculator calculator;
    SpacingType spacing = {0.5, 0.5, 2.0};  // 0.5 mm^3 voxel volume

    auto result = calculator.calculate(testLabelMap_, 1, spacing);
    EXPECT_TRUE(result.has_value());

    if (result) {
        // Voxel volume = 0.5 * 0.5 * 2.0 = 0.5 mm^3
        // Total volume = 125 * 0.5 = 62.5 mm^3
        EXPECT_EQ(result->voxelCount, 125);
        EXPECT_DOUBLE_EQ(result->volumeMm3, 62.5);
        EXPECT_DOUBLE_EQ(result->volumeCm3, 0.0625);
    }
}

TEST_F(VolumeCalculatorTest, CalculateSecondLabel) {
    VolumeCalculator calculator;

    auto result = calculator.calculate(testLabelMap_, 2, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_EQ(result->labelId, 2);
        EXPECT_EQ(result->voxelCount, 8);  // 2x2x2 cube
        EXPECT_DOUBLE_EQ(result->volumeMm3, 8.0);
        EXPECT_DOUBLE_EQ(result->volumeCm3, 0.008);
    }
}

TEST_F(VolumeCalculatorTest, CalculateWithSurfaceArea) {
    VolumeCalculator calculator;

    auto result = calculator.calculate(testLabelMap_, 1, testSpacing_, true);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_TRUE(result->surfaceAreaMm2.has_value());
        EXPECT_GT(result->surfaceAreaMm2.value(), 0.0);
        // Theoretical surface area of 5x5x5 cube = 6 * 5^2 = 150 mm^2
        // Marching cubes gives slightly different due to mesh approximation
        EXPECT_GT(result->surfaceAreaMm2.value(), 100.0);
        EXPECT_LT(result->surfaceAreaMm2.value(), 200.0);

        EXPECT_TRUE(result->sphericity.has_value());
        // Cube has lower sphericity than sphere
        EXPECT_GT(result->sphericity.value(), 0.0);
        EXPECT_LE(result->sphericity.value(), 1.0);
    }
}

TEST_F(VolumeCalculatorTest, CalculateBoundingBox) {
    VolumeCalculator calculator;

    auto result = calculator.calculate(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        // Bounding box should be 5x5x5 mm (indices 3-7 = 5 voxels)
        EXPECT_DOUBLE_EQ(result->boundingBoxMm[0], 5.0);
        EXPECT_DOUBLE_EQ(result->boundingBoxMm[1], 5.0);
        EXPECT_DOUBLE_EQ(result->boundingBoxMm[2], 5.0);
    }
}

// =============================================================================
// CalculateAll tests
// =============================================================================

TEST_F(VolumeCalculatorTest, CalculateAllLabels) {
    VolumeCalculator calculator;

    auto results = calculator.calculateAll(testLabelMap_, testSpacing_);
    EXPECT_EQ(results.size(), 2);  // Labels 1 and 2

    int successCount = 0;
    for (const auto& result : results) {
        if (result.has_value()) {
            ++successCount;
        }
    }
    EXPECT_EQ(successCount, 2);
}

TEST_F(VolumeCalculatorTest, CalculateAllWithSurfaceArea) {
    VolumeCalculator calculator;

    auto results = calculator.calculateAll(testLabelMap_, testSpacing_, true);
    EXPECT_EQ(results.size(), 2);

    for (const auto& result : results) {
        if (result.has_value()) {
            EXPECT_TRUE(result->surfaceAreaMm2.has_value());
        }
    }
}

TEST_F(VolumeCalculatorTest, CalculateAllEmptyLabelMap) {
    VolumeCalculator calculator;

    // Create empty label map
    auto emptyLabelMap = LabelMapType::New();
    emptyLabelMap->SetRegions(testLabelMap_->GetLargestPossibleRegion());
    emptyLabelMap->Allocate();
    emptyLabelMap->FillBuffer(0);

    auto results = calculator.calculateAll(emptyLabelMap, testSpacing_);
    EXPECT_TRUE(results.empty());
}

// =============================================================================
// Comparison table tests
// =============================================================================

TEST_F(VolumeCalculatorTest, CreateComparisonTable) {
    std::vector<VolumeResult> results;

    VolumeResult r1;
    r1.labelId = 1;
    r1.labelName = "Liver";
    r1.volumeMm3 = 1000.0;
    results.push_back(r1);

    VolumeResult r2;
    r2.labelId = 2;
    r2.labelName = "Kidney";
    r2.volumeMm3 = 500.0;
    results.push_back(r2);

    auto table = VolumeCalculator::createComparisonTable(results);

    EXPECT_EQ(table.results.size(), 2);
    EXPECT_DOUBLE_EQ(table.totalVolumeMm3, 1500.0);
    EXPECT_EQ(table.percentages.size(), 2);
    EXPECT_NEAR(table.percentages[0], 66.67, 0.1);  // 1000/1500 * 100
    EXPECT_NEAR(table.percentages[1], 33.33, 0.1);  // 500/1500 * 100
}

TEST_F(VolumeCalculatorTest, ComparisonTableToString) {
    std::vector<VolumeResult> results;

    VolumeResult r1;
    r1.labelId = 1;
    r1.labelName = "Liver";
    r1.volumeMm3 = 1000.0;
    r1.volumeML = 1.0;
    results.push_back(r1);

    auto table = VolumeCalculator::createComparisonTable(results);
    std::string str = table.toString();

    EXPECT_NE(str.find("Liver"), std::string::npos);
    EXPECT_NE(str.find("Total"), std::string::npos);
    EXPECT_NE(str.find("100"), std::string::npos);  // 100%
}

TEST_F(VolumeCalculatorTest, ComparisonTableExportToCsv) {
    std::vector<VolumeResult> results;

    VolumeResult r1;
    r1.labelId = 1;
    r1.labelName = "Test1";
    r1.volumeMm3 = 100.0;
    results.push_back(r1);

    auto table = VolumeCalculator::createComparisonTable(results);
    auto exportResult = table.exportToCsv(testCsvPath_);

    EXPECT_TRUE(exportResult.has_value());
    EXPECT_TRUE(std::filesystem::exists(testCsvPath_));
}

// =============================================================================
// Volume change calculation tests
// =============================================================================

TEST_F(VolumeCalculatorTest, CalculateVolumeChange) {
    VolumeResult current;
    current.labelId = 1;
    current.labelName = "Tumor";
    current.volumeMm3 = 1200.0;

    VolumeResult previous;
    previous.labelId = 1;
    previous.labelName = "Tumor";
    previous.volumeMm3 = 1000.0;

    auto timePoint = VolumeCalculator::calculateChange(
        current, previous, "20250101", "Follow-up"
    );

    EXPECT_EQ(timePoint.studyDate, "20250101");
    EXPECT_EQ(timePoint.studyDescription, "Follow-up");
    EXPECT_DOUBLE_EQ(timePoint.volume.volumeMm3, 1200.0);
    EXPECT_TRUE(timePoint.changeFromPreviousMm3.has_value());
    EXPECT_DOUBLE_EQ(timePoint.changeFromPreviousMm3.value(), 200.0);
    EXPECT_TRUE(timePoint.changePercentage.has_value());
    EXPECT_DOUBLE_EQ(timePoint.changePercentage.value(), 20.0);
}

TEST_F(VolumeCalculatorTest, CalculateNegativeVolumeChange) {
    VolumeResult current;
    current.volumeMm3 = 800.0;

    VolumeResult previous;
    previous.volumeMm3 = 1000.0;

    auto timePoint = VolumeCalculator::calculateChange(
        current, previous, "20250101"
    );

    EXPECT_DOUBLE_EQ(timePoint.changeFromPreviousMm3.value(), -200.0);
    EXPECT_DOUBLE_EQ(timePoint.changePercentage.value(), -20.0);
}

// =============================================================================
// Export tests
// =============================================================================

TEST_F(VolumeCalculatorTest, ExportToCsv) {
    std::vector<VolumeResult> results;

    VolumeResult r1;
    r1.labelId = 1;
    r1.labelName = "Label1";
    r1.voxelCount = 100;
    r1.volumeMm3 = 100.0;
    r1.volumeCm3 = 0.1;
    r1.volumeML = 0.1;
    results.push_back(r1);

    VolumeResult r2;
    r2.labelId = 2;
    r2.labelName = "Label2";
    r2.voxelCount = 200;
    r2.volumeMm3 = 200.0;
    r2.volumeCm3 = 0.2;
    r2.volumeML = 0.2;
    results.push_back(r2);

    auto result = VolumeCalculator::exportToCsv(results, testCsvPath_);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testCsvPath_));

    // Verify file content
    std::ifstream file(testCsvPath_);
    int lineCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lineCount;
    }
    EXPECT_EQ(lineCount, 3);  // Header + 2 data rows
}

TEST_F(VolumeCalculatorTest, ExportToCsvInvalidPath) {
    std::vector<VolumeResult> results;
    VolumeResult r1;
    results.push_back(r1);

    auto result = VolumeCalculator::exportToCsv(results, "/invalid/path/file.csv");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, VolumeError::Code::ExportFailed);
}

TEST_F(VolumeCalculatorTest, ExportTrackingToCsv) {
    std::vector<VolumeTimePoint> timePoints;

    VolumeResult r1;
    r1.labelId = 1;
    r1.labelName = "Tumor";
    r1.volumeMm3 = 1000.0;
    r1.volumeCm3 = 1.0;
    r1.volumeML = 1.0;

    VolumeTimePoint tp1;
    tp1.studyDate = "20240101";
    tp1.studyDescription = "Baseline";
    tp1.volume = r1;
    timePoints.push_back(tp1);

    VolumeResult r2;
    r2.labelId = 1;
    r2.labelName = "Tumor";
    r2.volumeMm3 = 1200.0;
    r2.volumeCm3 = 1.2;
    r2.volumeML = 1.2;

    VolumeTimePoint tp2;
    tp2.studyDate = "20250101";
    tp2.studyDescription = "Follow-up";
    tp2.volume = r2;
    tp2.changeFromPreviousMm3 = 200.0;
    tp2.changePercentage = 20.0;
    timePoints.push_back(tp2);

    auto result = VolumeCalculator::exportTrackingToCsv(timePoints, testTrackingCsvPath_);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testTrackingCsvPath_));

    // Verify file content
    std::ifstream file(testTrackingCsvPath_);
    std::string header;
    std::getline(file, header);
    EXPECT_NE(header.find("StudyDate"), std::string::npos);
    EXPECT_NE(header.find("ChangePercent"), std::string::npos);
}

// =============================================================================
// Progress callback test
// =============================================================================

TEST_F(VolumeCalculatorTest, ProgressCallback) {
    VolumeCalculator calculator;

    int callCount = 0;
    double lastProgress = 0.0;

    calculator.setProgressCallback([&](double progress) {
        ++callCount;
        lastProgress = progress;
    });

    auto unused = calculator.calculateAll(testLabelMap_, testSpacing_);
    (void)unused;

    EXPECT_EQ(callCount, 2);  // Two labels
    EXPECT_DOUBLE_EQ(lastProgress, 1.0);
}

}  // namespace
}  // namespace dicom_viewer::services

#include "services/measurement/roi_statistics.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace dicom_viewer::services {
namespace {

class RoiStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test image (10x10x5)
        testImage_ = ImageType::New();

        ImageType::SizeType size;
        size[0] = 10;
        size[1] = 10;
        size[2] = 5;

        ImageType::IndexType start;
        start.Fill(0);

        ImageType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        testImage_->SetRegions(region);
        testImage_->Allocate();

        // Set spacing
        ImageType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        testImage_->SetSpacing(spacing);

        // Fill with known values
        // Center region (3-7, 3-7) has value 100
        // Rest has value 0
        testImage_->FillBuffer(0);

        for (int z = 0; z < 5; ++z) {
            for (int y = 3; y <= 7; ++y) {
                for (int x = 3; x <= 7; ++x) {
                    ImageType::IndexType idx = {x, y, z};
                    testImage_->SetPixel(idx, 100);
                }
            }
        }
    }

    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(testCsvPath_)) {
            std::filesystem::remove(testCsvPath_);
        }
    }

    using ImageType = RoiStatisticsCalculator::ImageType;
    using LabelMapType = RoiStatisticsCalculator::LabelMapType;

    ImageType::Pointer testImage_;
    std::filesystem::path testCsvPath_ = std::filesystem::temp_directory_path() / "test_stats.csv";
};

// =============================================================================
// RoiStatistics struct tests
// =============================================================================

TEST_F(RoiStatisticsTest, RoiStatisticsDefaultValues) {
    RoiStatistics stats;

    EXPECT_EQ(stats.roiId, 0);
    EXPECT_TRUE(stats.roiLabel.empty());
    EXPECT_DOUBLE_EQ(stats.mean, 0.0);
    EXPECT_DOUBLE_EQ(stats.stdDev, 0.0);
    EXPECT_DOUBLE_EQ(stats.min, 0.0);
    EXPECT_DOUBLE_EQ(stats.max, 0.0);
    EXPECT_EQ(stats.voxelCount, 0);
}

TEST_F(RoiStatisticsTest, RoiStatisticsToString) {
    RoiStatistics stats;
    stats.roiLabel = "TestROI";
    stats.mean = 50.0;
    stats.stdDev = 10.0;
    stats.min = 0.0;
    stats.max = 100.0;
    stats.median = 50.0;
    stats.voxelCount = 1000;
    stats.areaMm2 = 100.0;

    std::string result = stats.toString();

    EXPECT_NE(result.find("TestROI"), std::string::npos);
    EXPECT_NE(result.find("Mean"), std::string::npos);
    EXPECT_NE(result.find("50.00"), std::string::npos);
    EXPECT_NE(result.find("Area"), std::string::npos);
}

TEST_F(RoiStatisticsTest, RoiStatisticsGetCsvHeader) {
    auto header = RoiStatistics::getCsvHeader();

    EXPECT_FALSE(header.empty());
    EXPECT_EQ(header[0], "ROI_ID");
    EXPECT_EQ(header[1], "Label");
    EXPECT_EQ(header[2], "Mean");
}

TEST_F(RoiStatisticsTest, RoiStatisticsGetCsvRow) {
    RoiStatistics stats;
    stats.roiId = 1;
    stats.roiLabel = "TestROI";
    stats.mean = 50.0;

    auto row = stats.getCsvRow();

    EXPECT_FALSE(row.empty());
    EXPECT_EQ(row[0], "1");
    EXPECT_EQ(row[1], "TestROI");
}

TEST_F(RoiStatisticsTest, RoiStatisticsExportToCsv) {
    RoiStatistics stats;
    stats.roiId = 1;
    stats.roiLabel = "TestROI";
    stats.mean = 50.0;
    stats.stdDev = 10.0;
    stats.min = 0.0;
    stats.max = 100.0;

    auto result = stats.exportToCsv(testCsvPath_);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testCsvPath_));

    // Verify file content
    std::ifstream file(testCsvPath_);
    std::string header;
    std::getline(file, header);
    EXPECT_NE(header.find("ROI_ID"), std::string::npos);

    std::string data;
    std::getline(file, data);
    EXPECT_NE(data.find("TestROI"), std::string::npos);
}

// =============================================================================
// StatisticsError tests
// =============================================================================

TEST_F(RoiStatisticsTest, StatisticsErrorSuccess) {
    StatisticsError error;
    EXPECT_TRUE(error.isSuccess());
    EXPECT_EQ(error.code, StatisticsError::Code::Success);
}

TEST_F(RoiStatisticsTest, StatisticsErrorToString) {
    StatisticsError error;
    error.code = StatisticsError::Code::InvalidImage;
    error.message = "test message";

    std::string result = error.toString();
    EXPECT_NE(result.find("Invalid image"), std::string::npos);
    EXPECT_NE(result.find("test message"), std::string::npos);
}

// =============================================================================
// RoiStatisticsCalculator tests
// =============================================================================

TEST_F(RoiStatisticsTest, CalculatorDefaultConstruction) {
    RoiStatisticsCalculator calculator;
    // Should not crash
}

TEST_F(RoiStatisticsTest, CalculatorSetImage) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);
    // Should not crash
}

TEST_F(RoiStatisticsTest, CalculatorSetPixelSpacing) {
    RoiStatisticsCalculator calculator;
    calculator.setPixelSpacing(0.5, 0.5, 1.0);
    // Should not crash
}

TEST_F(RoiStatisticsTest, CalculatorSetHistogramParameters) {
    RoiStatisticsCalculator calculator;
    calculator.setHistogramParameters(-1024.0, 3071.0, 512);
    // Should not crash
}

TEST_F(RoiStatisticsTest, CalculatorNoImageError) {
    RoiStatisticsCalculator calculator;
    // Don't set image

    AreaMeasurement roi;
    roi.id = 1;
    roi.type = RoiType::Rectangle;
    roi.points = {{3.0, 3.0, 0.0}, {7.0, 7.0, 0.0}};

    auto result = calculator.calculate(roi, 0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, StatisticsError::Code::InvalidImage);
}

TEST_F(RoiStatisticsTest, CalculatorRectangleRoi) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);

    AreaMeasurement roi;
    roi.id = 1;
    roi.type = RoiType::Rectangle;
    roi.label = "TestRect";
    // Rectangle covering the center region (3-7, 3-7)
    roi.points = {{3.0, 3.0, 0.0}, {7.0, 7.0, 0.0}};

    auto result = calculator.calculate(roi, 2);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_EQ(result->roiId, 1);
        EXPECT_EQ(result->roiLabel, "TestRect");
        // All pixels in this region should be 100
        EXPECT_DOUBLE_EQ(result->mean, 100.0);
        EXPECT_DOUBLE_EQ(result->stdDev, 0.0);
        EXPECT_DOUBLE_EQ(result->min, 100.0);
        EXPECT_DOUBLE_EQ(result->max, 100.0);
        EXPECT_EQ(result->voxelCount, 25);  // 5x5 rectangle
    }
}

TEST_F(RoiStatisticsTest, CalculatorEllipseRoi) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);

    AreaMeasurement roi;
    roi.id = 2;
    roi.type = RoiType::Ellipse;
    roi.centroid = {5.0, 5.0, 0.0};
    roi.semiAxisA = 2.0;  // Horizontal semi-axis
    roi.semiAxisB = 2.0;  // Vertical semi-axis

    auto result = calculator.calculate(roi, 2);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_EQ(result->roiId, 2);
        // Ellipse centered at (5,5) with radius 2 should cover mostly 100-value pixels
        EXPECT_GT(result->voxelCount, 0);
        EXPECT_GE(result->mean, 0.0);
        EXPECT_LE(result->mean, 100.0);
    }
}

TEST_F(RoiStatisticsTest, CalculatorSliceOutOfRange) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);

    AreaMeasurement roi;
    roi.type = RoiType::Rectangle;
    roi.points = {{3.0, 3.0, 0.0}, {7.0, 7.0, 0.0}};

    auto result = calculator.calculate(roi, 100);  // Out of range
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, StatisticsError::Code::InvalidRoi);
}

TEST_F(RoiStatisticsTest, CalculatorMultipleRois) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);

    std::vector<AreaMeasurement> rois;

    AreaMeasurement roi1;
    roi1.id = 1;
    roi1.type = RoiType::Rectangle;
    roi1.points = {{3.0, 3.0, 0.0}, {7.0, 7.0, 0.0}};
    rois.push_back(roi1);

    AreaMeasurement roi2;
    roi2.id = 2;
    roi2.type = RoiType::Rectangle;
    roi2.points = {{0.0, 0.0, 0.0}, {2.0, 2.0, 0.0}};
    rois.push_back(roi2);

    auto results = calculator.calculateMultiple(rois, 2);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(RoiStatisticsTest, CalculatorLabelMapStatistics) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);

    // Create a label map
    auto labelMap = LabelMapType::New();
    labelMap->SetRegions(testImage_->GetLargestPossibleRegion());
    labelMap->SetSpacing(testImage_->GetSpacing());
    labelMap->Allocate();
    labelMap->FillBuffer(0);

    // Set label 1 for center region
    for (int z = 0; z < 5; ++z) {
        for (int y = 3; y <= 7; ++y) {
            for (int x = 3; x <= 7; ++x) {
                LabelMapType::IndexType idx = {x, y, z};
                labelMap->SetPixel(idx, 1);
            }
        }
    }

    auto result = calculator.calculate(labelMap, 1);
    EXPECT_TRUE(result.has_value());

    if (result) {
        // Label 1 covers the region with value 100
        EXPECT_DOUBLE_EQ(result->mean, 100.0);
        EXPECT_DOUBLE_EQ(result->min, 100.0);
        EXPECT_DOUBLE_EQ(result->max, 100.0);
        EXPECT_GT(result->volumeMm3, 0.0);
    }
}

TEST_F(RoiStatisticsTest, CalculatorLabelNotFound) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);

    auto labelMap = LabelMapType::New();
    labelMap->SetRegions(testImage_->GetLargestPossibleRegion());
    labelMap->Allocate();
    labelMap->FillBuffer(0);

    auto result = calculator.calculate(labelMap, 99);  // Label doesn't exist
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, StatisticsError::Code::NoPixelsInRoi);
}

TEST_F(RoiStatisticsTest, ExportMultipleToCsv) {
    std::vector<RoiStatistics> stats;

    RoiStatistics s1;
    s1.roiId = 1;
    s1.roiLabel = "ROI1";
    s1.mean = 50.0;
    stats.push_back(s1);

    RoiStatistics s2;
    s2.roiId = 2;
    s2.roiLabel = "ROI2";
    s2.mean = 100.0;
    stats.push_back(s2);

    auto result = RoiStatisticsCalculator::exportMultipleToCsv(stats, testCsvPath_);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testCsvPath_));

    // Verify file has 3 lines (header + 2 data)
    std::ifstream file(testCsvPath_);
    int lineCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lineCount;
    }
    EXPECT_EQ(lineCount, 3);
}

TEST_F(RoiStatisticsTest, CompareStatistics) {
    RoiStatistics s1;
    s1.roiLabel = "ROI1";
    s1.mean = 50.0;
    s1.stdDev = 10.0;

    RoiStatistics s2;
    s2.roiLabel = "ROI2";
    s2.mean = 100.0;
    s2.stdDev = 20.0;

    std::string comparison = RoiStatisticsCalculator::compareStatistics(s1, s2);

    EXPECT_NE(comparison.find("ROI1"), std::string::npos);
    EXPECT_NE(comparison.find("ROI2"), std::string::npos);
    EXPECT_NE(comparison.find("Mean"), std::string::npos);
}

// =============================================================================
// Progress callback test
// =============================================================================

TEST_F(RoiStatisticsTest, ProgressCallback) {
    RoiStatisticsCalculator calculator;
    calculator.setImage(testImage_);

    int callCount = 0;
    double lastProgress = 0.0;

    calculator.setProgressCallback([&](double progress) {
        ++callCount;
        lastProgress = progress;
    });

    std::vector<AreaMeasurement> rois;
    for (int i = 0; i < 5; ++i) {
        AreaMeasurement roi;
        roi.id = i;
        roi.type = RoiType::Rectangle;
        roi.points = {{3.0, 3.0, 0.0}, {7.0, 7.0, 0.0}};
        rois.push_back(roi);
    }

    calculator.calculateMultiple(rois, 2);

    EXPECT_EQ(callCount, 5);
    EXPECT_DOUBLE_EQ(lastProgress, 1.0);
}

}  // namespace
}  // namespace dicom_viewer::services

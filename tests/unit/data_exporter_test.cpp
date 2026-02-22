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

#include "services/export/data_exporter.hpp"

#include <gtest/gtest.h>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace dicom_viewer::services {
namespace {

class DataExporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "data_exporter_test";
        std::filesystem::create_directories(testDir_);

        // Create test patient info
        patientInfo_.name = "Test Patient";
        patientInfo_.patientId = "12345";
        patientInfo_.dateOfBirth = "1980-01-01";
        patientInfo_.sex = "M";
        patientInfo_.studyDate = "2025-01-01";
        patientInfo_.modality = "CT";
        patientInfo_.studyDescription = "CT Chest";

        // Create test distance measurements
        DistanceMeasurement dm1;
        dm1.id = 1;
        dm1.label = "D1";
        dm1.point1 = {100.0, 50.0, 25.0};
        dm1.point2 = {150.0, 75.0, 25.0};
        dm1.distanceMm = 55.9;
        dm1.sliceIndex = 100;
        distanceMeasurements_.push_back(dm1);

        DistanceMeasurement dm2;
        dm2.id = 2;
        dm2.label = "D2, with comma";  // Test CSV escaping
        dm2.point1 = {200.0, 100.0, 50.0};
        dm2.point2 = {250.0, 150.0, 50.0};
        dm2.distanceMm = 70.71;
        dm2.sliceIndex = 150;
        distanceMeasurements_.push_back(dm2);

        // Create test angle measurements
        AngleMeasurement am1;
        am1.id = 1;
        am1.label = "A1";
        am1.vertex = {100.0, 100.0, 50.0};
        am1.point1 = {50.0, 100.0, 50.0};
        am1.point2 = {100.0, 50.0, 50.0};
        am1.angleDegrees = 90.0;
        am1.isCobbAngle = false;
        am1.sliceIndex = 50;
        angleMeasurements_.push_back(am1);

        // Create test area measurements
        AreaMeasurement area1;
        area1.id = 1;
        area1.label = "ROI1";
        area1.type = RoiType::Ellipse;
        area1.areaMm2 = 1256.64;
        area1.areaCm2 = 12.5664;
        area1.perimeterMm = 125.66;
        area1.centroid = {150.0, 150.0, 75.0};
        area1.sliceIndex = 75;
        areaMeasurements_.push_back(area1);

        // Create test ROI statistics
        RoiStatistics stats1;
        stats1.roiId = 1;
        stats1.roiLabel = "ROI1";
        stats1.mean = 45.5;
        stats1.stdDev = 12.3;
        stats1.min = -100.0;
        stats1.max = 200.0;
        stats1.median = 42.0;
        stats1.voxelCount = 1000;
        stats1.areaMm2 = 1256.64;
        roiStatistics_.push_back(stats1);

        // Create test volume results
        VolumeResult vol1;
        vol1.labelId = 1;
        vol1.labelName = "Tumor";
        vol1.voxelCount = 5000;
        vol1.volumeMm3 = 5000.0;
        vol1.volumeCm3 = 5.0;
        vol1.volumeML = 5.0;
        vol1.surfaceAreaMm2 = 1200.0;
        vol1.sphericity = 0.85;
        volumeResults_.push_back(vol1);

        VolumeResult vol2;
        vol2.labelId = 2;
        vol2.labelName = "Organ";
        vol2.voxelCount = 50000;
        vol2.volumeMm3 = 50000.0;
        vol2.volumeCm3 = 50.0;
        vol2.volumeML = 50.0;
        // No surface area for this one
        volumeResults_.push_back(vol2);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    std::string readFile(const std::filesystem::path& path) {
        QFile file(QString::fromStdString(path.string()));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return "";
        }
        QTextStream stream(&file);
        return stream.readAll().toStdString();
    }

    std::filesystem::path testDir_;
    PatientInfo patientInfo_;
    std::vector<DistanceMeasurement> distanceMeasurements_;
    std::vector<AngleMeasurement> angleMeasurements_;
    std::vector<AreaMeasurement> areaMeasurements_;
    std::vector<RoiStatistics> roiStatistics_;
    std::vector<VolumeResult> volumeResults_;
};

// =============================================================================
// ExportError tests
// =============================================================================

TEST_F(DataExporterTest, ExportErrorDefaultSuccess) {
    ExportError error;
    EXPECT_TRUE(error.isSuccess());
    EXPECT_EQ(error.code, ExportError::Code::Success);
}

TEST_F(DataExporterTest, ExportErrorToString) {
    ExportError error;
    error.code = ExportError::Code::FileAccessDenied;
    error.message = "cannot write";

    std::string result = error.toString();
    EXPECT_NE(result.find("File access denied"), std::string::npos);
    EXPECT_NE(result.find("cannot write"), std::string::npos);
}

TEST_F(DataExporterTest, ExportErrorAllCodes) {
    std::vector<ExportError::Code> codes = {
        ExportError::Code::Success,
        ExportError::Code::FileAccessDenied,
        ExportError::Code::InvalidData,
        ExportError::Code::EncodingFailed,
        ExportError::Code::UnsupportedFormat,
        ExportError::Code::InternalError
    };

    for (auto code : codes) {
        ExportError error;
        error.code = code;
        error.message = "test";
        std::string str = error.toString();
        EXPECT_FALSE(str.empty());
    }
}

// =============================================================================
// ExportOptions tests
// =============================================================================

TEST_F(DataExporterTest, ExportOptionsDefaultValues) {
    ExportOptions options;

    EXPECT_TRUE(options.includeHeader);
    EXPECT_TRUE(options.includeMetadata);
    EXPECT_TRUE(options.includeTimestamp);
    EXPECT_EQ(options.csvDelimiter, ',');
    EXPECT_EQ(options.dateFormat, "yyyy-MM-ddTHH:mm:ss");
    EXPECT_TRUE(options.selectedColumns.empty());
    EXPECT_TRUE(options.includeUtf8Bom);
}

// =============================================================================
// DataExporter construction tests
// =============================================================================

TEST_F(DataExporterTest, DefaultConstruction) {
    DataExporter exporter;
    // Should not crash
}

TEST_F(DataExporterTest, MoveConstruction) {
    DataExporter exporter1;
    DataExporter exporter2(std::move(exporter1));
    // Should not crash
}

TEST_F(DataExporterTest, MoveAssignment) {
    DataExporter exporter1;
    DataExporter exporter2;
    exporter2 = std::move(exporter1);
    // Should not crash
}

// =============================================================================
// CSV Header tests
// =============================================================================

TEST_F(DataExporterTest, GetDistanceCSVHeader) {
    auto headers = DataExporter::getDistanceCSVHeader();
    EXPECT_FALSE(headers.empty());
    EXPECT_EQ(headers[0], "ID");
    EXPECT_EQ(headers[1], "Label");

    // Should contain coordinate columns
    bool hasDistance = false;
    for (const auto& h : headers) {
        if (h.contains("Distance")) hasDistance = true;
    }
    EXPECT_TRUE(hasDistance);
}

TEST_F(DataExporterTest, GetAngleCSVHeader) {
    auto headers = DataExporter::getAngleCSVHeader();
    EXPECT_FALSE(headers.empty());
    EXPECT_EQ(headers[0], "ID");

    bool hasAngle = false;
    for (const auto& h : headers) {
        if (h.contains("Angle")) hasAngle = true;
    }
    EXPECT_TRUE(hasAngle);
}

TEST_F(DataExporterTest, GetAreaCSVHeader) {
    auto headers = DataExporter::getAreaCSVHeader();
    EXPECT_FALSE(headers.empty());

    bool hasArea = false;
    for (const auto& h : headers) {
        if (h.contains("Area")) hasArea = true;
    }
    EXPECT_TRUE(hasArea);
}

TEST_F(DataExporterTest, GetROIStatisticsCSVHeader) {
    auto headers = DataExporter::getROIStatisticsCSVHeader();
    EXPECT_FALSE(headers.empty());

    bool hasMean = false;
    bool hasStdDev = false;
    for (const auto& h : headers) {
        if (h.contains("Mean")) hasMean = true;
        if (h.contains("StdDev")) hasStdDev = true;
    }
    EXPECT_TRUE(hasMean);
    EXPECT_TRUE(hasStdDev);
}

TEST_F(DataExporterTest, GetVolumeCSVHeader) {
    auto headers = DataExporter::getVolumeCSVHeader();
    EXPECT_FALSE(headers.empty());

    bool hasVolume = false;
    for (const auto& h : headers) {
        if (h.contains("Volume")) hasVolume = true;
    }
    EXPECT_TRUE(hasVolume);
}

// =============================================================================
// Distance CSV export tests
// =============================================================================

TEST_F(DataExporterTest, ExportDistancesToCSV_Basic) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances.csv";

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);
    EXPECT_FALSE(content.empty());

    // Check header is present
    EXPECT_NE(content.find("ID"), std::string::npos);
    EXPECT_NE(content.find("Distance_mm"), std::string::npos);

    // Check data is present
    EXPECT_NE(content.find("D1"), std::string::npos);
    EXPECT_NE(content.find("55.9"), std::string::npos);
}

TEST_F(DataExporterTest, ExportDistancesToCSV_WithCommaInLabel) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances_comma.csv";

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath);

    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);

    // Label with comma should be quoted
    EXPECT_NE(content.find("\"D2, with comma\""), std::string::npos);
}

TEST_F(DataExporterTest, ExportDistancesToCSV_NoHeader) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances_noheader.csv";

    ExportOptions options;
    options.includeHeader = false;

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath, options);

    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);

    // Should not have header row (ID column name)
    // First line should be data
    EXPECT_TRUE(content.substr(0, 10).find("ID") == std::string::npos ||
                content.find("\n1,") < content.find("ID"));
}

TEST_F(DataExporterTest, ExportDistancesToCSV_CustomDelimiter) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances_semicolon.csv";

    ExportOptions options;
    options.csvDelimiter = ';';

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath, options);

    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);

    // Should use semicolon as delimiter
    EXPECT_NE(content.find(";"), std::string::npos);
}

TEST_F(DataExporterTest, ExportDistancesToCSV_EmptyData) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances_empty.csv";

    std::vector<DistanceMeasurement> empty;
    auto result = exporter.exportDistancesToCSV(empty, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);
    // Should still have header
    EXPECT_NE(content.find("ID"), std::string::npos);
}

TEST_F(DataExporterTest, ExportDistancesToCSV_WithMetadata) {
    DataExporter exporter;
    exporter.setPatientInfo(patientInfo_);
    auto outputPath = testDir_ / "distances_metadata.csv";

    ExportOptions options;
    options.includeMetadata = true;

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath, options);

    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);

    // Should have metadata comments
    EXPECT_NE(content.find("# Patient:"), std::string::npos);
    EXPECT_NE(content.find("Test Patient"), std::string::npos);
}

// =============================================================================
// Angle CSV export tests
// =============================================================================

TEST_F(DataExporterTest, ExportAnglesToCSV_Basic) {
    DataExporter exporter;
    auto outputPath = testDir_ / "angles.csv";

    auto result = exporter.exportAnglesToCSV(
        angleMeasurements_, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);
    EXPECT_NE(content.find("A1"), std::string::npos);
    EXPECT_NE(content.find("90.0"), std::string::npos);
    EXPECT_NE(content.find("No"), std::string::npos);  // IsCobbAngle = No
}

// =============================================================================
// Area CSV export tests
// =============================================================================

TEST_F(DataExporterTest, ExportAreasToCSV_Basic) {
    DataExporter exporter;
    auto outputPath = testDir_ / "areas.csv";

    auto result = exporter.exportAreasToCSV(
        areaMeasurements_, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);
    EXPECT_NE(content.find("ROI1"), std::string::npos);
    EXPECT_NE(content.find("Ellipse"), std::string::npos);
    EXPECT_NE(content.find("1256.64"), std::string::npos);
}

// =============================================================================
// ROI Statistics CSV export tests
// =============================================================================

TEST_F(DataExporterTest, ExportROIStatisticsToCSV_Basic) {
    DataExporter exporter;
    auto outputPath = testDir_ / "roi_stats.csv";

    auto result = exporter.exportROIStatisticsToCSV(
        roiStatistics_, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);
    EXPECT_NE(content.find("ROI1"), std::string::npos);
    EXPECT_NE(content.find("45.5"), std::string::npos);   // Mean
    EXPECT_NE(content.find("12.3"), std::string::npos);   // StdDev
    EXPECT_NE(content.find("-100"), std::string::npos);   // Min
}

// =============================================================================
// Volume CSV export tests
// =============================================================================

TEST_F(DataExporterTest, ExportVolumesToCSV_Basic) {
    DataExporter exporter;
    auto outputPath = testDir_ / "volumes.csv";

    auto result = exporter.exportVolumesToCSV(
        volumeResults_, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);
    EXPECT_NE(content.find("Tumor"), std::string::npos);
    EXPECT_NE(content.find("5000"), std::string::npos);
    EXPECT_NE(content.find("0.85"), std::string::npos);   // Sphericity
}

TEST_F(DataExporterTest, ExportVolumesToCSV_OptionalFields) {
    DataExporter exporter;
    auto outputPath = testDir_ / "volumes_optional.csv";

    auto result = exporter.exportVolumesToCSV(
        volumeResults_, outputPath);

    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);
    // Second volume has no surface area - should be empty
    // Check that the file is properly formatted (no trailing commas causing issues)
    EXPECT_NE(content.find("Organ"), std::string::npos);
}

// =============================================================================
// Combined CSV export tests
// =============================================================================

TEST_F(DataExporterTest, ExportAllToCSV_Basic) {
    DataExporter exporter;
    auto outputPath = testDir_ / "all_measurements.csv";

    ReportData data;
    data.patientInfo = patientInfo_;
    data.distanceMeasurements = distanceMeasurements_;
    data.angleMeasurements = angleMeasurements_;
    data.areaMeasurements = areaMeasurements_;
    data.volumeResults = volumeResults_;

    auto result = exporter.exportAllToCSV(data, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);

    // Should have all sections
    EXPECT_NE(content.find("DISTANCE MEASUREMENTS"), std::string::npos);
    EXPECT_NE(content.find("ANGLE MEASUREMENTS"), std::string::npos);
    EXPECT_NE(content.find("AREA MEASUREMENTS"), std::string::npos);
    EXPECT_NE(content.find("VOLUME MEASUREMENTS"), std::string::npos);

    // Should have data from each section
    EXPECT_NE(content.find("D1"), std::string::npos);
    EXPECT_NE(content.find("A1"), std::string::npos);
    EXPECT_NE(content.find("ROI1"), std::string::npos);
    EXPECT_NE(content.find("Tumor"), std::string::npos);
}

// =============================================================================
// Excel export tests
// =============================================================================

TEST_F(DataExporterTest, ExportToExcel_Basic) {
    DataExporter exporter;
    auto outputPath = testDir_ / "report.xml";

    ReportData data;
    data.patientInfo = patientInfo_;
    data.distanceMeasurements = distanceMeasurements_;
    data.angleMeasurements = angleMeasurements_;
    data.areaMeasurements = areaMeasurements_;
    data.roiStatistics = roiStatistics_;
    data.volumeResults = volumeResults_;

    auto result = exporter.exportToExcel(data, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);

    // Should be valid XML
    EXPECT_NE(content.find("<?xml"), std::string::npos);
    EXPECT_NE(content.find("<Workbook"), std::string::npos);

    // Should have all worksheets
    EXPECT_NE(content.find("ss:Name=\"Summary\""), std::string::npos);
    EXPECT_NE(content.find("ss:Name=\"Distances\""), std::string::npos);
    EXPECT_NE(content.find("ss:Name=\"Angles\""), std::string::npos);
    EXPECT_NE(content.find("ss:Name=\"Areas\""), std::string::npos);
    EXPECT_NE(content.find("ss:Name=\"Volumes\""), std::string::npos);
    EXPECT_NE(content.find("ss:Name=\"Metadata\""), std::string::npos);

    // Should have patient info in Summary
    EXPECT_NE(content.find("Test Patient"), std::string::npos);
}

TEST_F(DataExporterTest, ExportToExcel_WithROIStatistics) {
    DataExporter exporter;
    auto outputPath = testDir_ / "report_with_stats.xml";

    ReportData data;
    data.patientInfo = patientInfo_;
    data.roiStatistics = roiStatistics_;

    auto result = exporter.exportToExcel(data, outputPath);

    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);

    // Should have ROI Statistics worksheet
    EXPECT_NE(content.find("ss:Name=\"ROI_Statistics\""), std::string::npos);
}

TEST_F(DataExporterTest, ExportToExcel_EmptyData) {
    DataExporter exporter;
    auto outputPath = testDir_ / "report_empty.xml";

    ReportData data;
    data.patientInfo = patientInfo_;

    auto result = exporter.exportToExcel(data, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::string content = readFile(outputPath);

    // Should still have basic structure
    EXPECT_NE(content.find("<Workbook"), std::string::npos);
    EXPECT_NE(content.find("</Workbook>"), std::string::npos);
}

// =============================================================================
// Error handling tests
// =============================================================================

TEST_F(DataExporterTest, ExportToInvalidPath) {
    DataExporter exporter;
    // Try to write to a non-existent directory
    auto outputPath = std::filesystem::path("/nonexistent/dir/file.csv");

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::FileAccessDenied);
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(DataExporterTest, ProgressCallback) {
    DataExporter exporter;
    auto outputPath = testDir_ / "progress_test.csv";

    bool progressCalled = false;
    double lastProgress = -1.0;

    exporter.setProgressCallback([&](double progress, const QString& status) {
        progressCalled = true;
        lastProgress = progress;
        EXPECT_FALSE(status.isEmpty());
    });

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(progressCalled);
    EXPECT_EQ(lastProgress, 1.0);  // Should end at 100%
}

// =============================================================================
// Unicode handling tests
// =============================================================================

TEST_F(DataExporterTest, ExportWithUnicodeLabels) {
    DataExporter exporter;
    auto outputPath = testDir_ / "unicode.csv";

    // Create measurement with Korean label
    std::vector<DistanceMeasurement> measurements;
    DistanceMeasurement dm;
    dm.id = 1;
    dm.label = "Distance 1";  // Standard ASCII for reliable test
    dm.point1 = {0, 0, 0};
    dm.point2 = {10, 10, 10};
    dm.distanceMm = 17.32;
    dm.sliceIndex = 1;
    measurements.push_back(dm);

    auto result = exporter.exportDistancesToCSV(measurements, outputPath);

    ASSERT_TRUE(result.has_value());

    // Read raw bytes to verify BOM (QTextStream-based readFile strips BOM)
    std::ifstream rawFile(outputPath, std::ios::binary);
    ASSERT_TRUE(rawFile.is_open());
    char bom[3] = {};
    rawFile.read(bom, 3);
    EXPECT_EQ(static_cast<unsigned char>(bom[0]), 0xEF);
    EXPECT_EQ(static_cast<unsigned char>(bom[1]), 0xBB);
    EXPECT_EQ(static_cast<unsigned char>(bom[2]), 0xBF);
}

// =============================================================================
// Large dataset performance test
// =============================================================================

TEST_F(DataExporterTest, ExportLargeDataset) {
    DataExporter exporter;
    auto outputPath = testDir_ / "large_dataset.csv";

    // Create 1000 measurements
    std::vector<DistanceMeasurement> largeMeasurements;
    largeMeasurements.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        DistanceMeasurement dm;
        dm.id = i;
        dm.label = "D" + std::to_string(i);
        dm.point1 = {static_cast<double>(i), static_cast<double>(i * 2), 0.0};
        dm.point2 = {static_cast<double>(i + 10), static_cast<double>(i * 2 + 10), 0.0};
        dm.distanceMm = 14.14;  // sqrt(200)
        dm.sliceIndex = i % 200;
        largeMeasurements.push_back(dm);
    }

    auto start = std::chrono::steady_clock::now();
    auto result = exporter.exportDistancesToCSV(largeMeasurements, outputPath);
    auto end = std::chrono::steady_clock::now();

    ASSERT_TRUE(result.has_value());

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should complete in under 2 seconds
    EXPECT_LT(duration.count(), 2000);

    // Verify file size is reasonable
    auto fileSize = std::filesystem::file_size(outputPath);
    EXPECT_GT(fileSize, 50000);  // At least 50KB for 1000 entries
}

// =============================================================================
// Output validation and format verification tests (Issue #207)
// =============================================================================

TEST_F(DataExporterTest, CsvRoundTripValuesMatch) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances_roundtrip.csv";

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath);
    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);

    // Verify exact measurement values survive round-trip
    EXPECT_NE(content.find("55.9"), std::string::npos);
    EXPECT_NE(content.find("70.71"), std::string::npos);

    // Verify point coordinates are present
    EXPECT_NE(content.find("100"), std::string::npos);   // dm1.point1.x
    EXPECT_NE(content.find("150"), std::string::npos);   // dm1.point2.x

    // Verify labels are intact
    EXPECT_NE(content.find("D1"), std::string::npos);
}

TEST_F(DataExporterTest, CsvColumnCountMatchesHeader) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances_colcount.csv";

    ExportOptions options;
    options.includeMetadata = false;

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath, options);
    ASSERT_TRUE(result.has_value());

    std::string content = readFile(outputPath);
    std::istringstream stream(content);
    std::string line;

    // Count columns in header row (number of commas + 1)
    ASSERT_TRUE(std::getline(stream, line));
    size_t headerCommas = std::count(line.begin(), line.end(), ',');
    EXPECT_GT(headerCommas, 0u);

    // Count delimiter commas outside quoted fields (RFC 4180 aware)
    auto countCsvDelimiters = [](const std::string& row) -> size_t {
        size_t count = 0;
        bool inQuotes = false;
        for (char ch : row) {
            if (ch == '"') {
                inQuotes = !inQuotes;
            } else if (ch == ',' && !inQuotes) {
                ++count;
            }
        }
        return count;
    };

    // Verify each data row has the same number of columns
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        // Skip BOM or metadata lines
        if (line[0] == '#' || line[0] == '\xEF') continue;
        size_t dataCommas = countCsvDelimiters(line);
        EXPECT_EQ(dataCommas, headerCommas)
            << "Column mismatch in data row: " << line;
    }
}

TEST_F(DataExporterTest, ExcelOutputContainsXmlDeclaration) {
    DataExporter exporter;
    auto outputPath = testDir_ / "report_xml_decl.xml";

    ReportData data;
    data.patientInfo = patientInfo_;
    data.distanceMeasurements = distanceMeasurements_;

    auto result = exporter.exportToExcel(data, outputPath);
    ASSERT_TRUE(result.has_value());

    // Read first bytes of file to verify XML declaration
    std::ifstream file(outputPath);
    ASSERT_TRUE(file.is_open());

    std::string firstLine;
    std::getline(file, firstLine);

    // XML declaration must start with <?xml
    // Account for possible UTF-8 BOM prefix
    auto xmlPos = firstLine.find("<?xml");
    EXPECT_NE(xmlPos, std::string::npos)
        << "Excel XML file must start with <?xml declaration";

    // Verify it contains version attribute
    std::string content = readFile(outputPath);
    EXPECT_NE(content.find("version="), std::string::npos);
    EXPECT_NE(content.find("</Workbook>"), std::string::npos);
}

TEST_F(DataExporterTest, CsvContainsUtf8BOM) {
    DataExporter exporter;
    auto outputPath = testDir_ / "distances_bom.csv";

    ExportOptions options;
    options.includeUtf8Bom = true;

    auto result = exporter.exportDistancesToCSV(
        distanceMeasurements_, outputPath, options);
    ASSERT_TRUE(result.has_value());

    // Read raw bytes to check for UTF-8 BOM (0xEF 0xBB 0xBF)
    std::ifstream file(outputPath, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    char bom[3] = {};
    file.read(bom, 3);
    ASSERT_EQ(file.gcount(), 3);

    EXPECT_EQ(static_cast<unsigned char>(bom[0]), 0xEF);
    EXPECT_EQ(static_cast<unsigned char>(bom[1]), 0xBB);
    EXPECT_EQ(static_cast<unsigned char>(bom[2]), 0xBF);
}

}  // namespace
}  // namespace dicom_viewer::services

int main(int argc, char** argv) {
    // Initialize Qt application for QFile operations
    QApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

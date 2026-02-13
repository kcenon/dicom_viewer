#include "services/export/dicom_sr_writer.hpp"

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace dicom_viewer::services {
namespace {

class DicomSRWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        testDir_ = std::filesystem::temp_directory_path() / "dicom_sr_writer_test";
        std::filesystem::create_directories(testDir_);

        // Create sample content
        sampleContent_ = createSampleContent();
    }

    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(testDir_)) {
            std::filesystem::remove_all(testDir_);
        }
    }

    SRContent createSampleContent() {
        SRContent content;

        // Patient info
        content.patient.patientId = "TEST_PAT_001";
        content.patient.patientName = "Test^Patient";
        content.patient.patientBirthDate = "19800101";
        content.patient.patientSex = "M";

        // Study info
        content.study.studyInstanceUid = "1.2.840.113619.2.55.3.123456789.1";
        content.study.studyDate = "20260119";
        content.study.studyTime = "120000";
        content.study.studyDescription = "CT Chest";
        content.study.accessionNumber = "ACC123456";
        content.study.referringPhysicianName = "Dr^Smith";

        // Series info
        content.series.seriesInstanceUid = "1.2.840.113619.2.55.3.123456789.2";
        content.series.modality = "CT";
        content.series.seriesDescription = "Axial Images";

        // Operator info
        content.operatorName = "Test Operator";
        content.institutionName = "Test Hospital";
        content.performedDateTime = std::chrono::system_clock::now();

        return content;
    }

    void addSampleDistances(SRContent& content) {
        DistanceMeasurement dist1;
        dist1.id = 1;
        dist1.label = "Lesion Diameter";
        dist1.point1 = {100.0, 100.0, 50.0};
        dist1.point2 = {130.0, 100.0, 50.0};
        dist1.distanceMm = 30.0;
        dist1.visible = true;
        dist1.sliceIndex = 50;

        DistanceMeasurement dist2;
        dist2.id = 2;
        dist2.label = "Reference Distance";
        dist2.point1 = {200.0, 150.0, 75.0};
        dist2.point2 = {200.0, 200.0, 75.0};
        dist2.distanceMm = 50.0;
        dist2.visible = true;
        dist2.sliceIndex = 75;

        content.distances.push_back(dist1);
        content.distances.push_back(dist2);
    }

    void addSampleAngles(SRContent& content) {
        AngleMeasurement angle1;
        angle1.id = 1;
        angle1.label = "Vertebral Angle";
        angle1.vertex = {150.0, 200.0, 100.0};
        angle1.point1 = {100.0, 150.0, 100.0};
        angle1.point2 = {200.0, 150.0, 100.0};
        angle1.angleDegrees = 45.5;
        angle1.visible = true;
        angle1.sliceIndex = 100;

        content.angles.push_back(angle1);
    }

    void addSampleAreas(SRContent& content) {
        AreaMeasurement area1;
        area1.id = 1;
        area1.label = "Tumor Region";
        area1.type = RoiType::Ellipse;
        area1.points = {{100.0, 100.0, 50.0}, {120.0, 100.0, 50.0},
                        {120.0, 120.0, 50.0}, {100.0, 120.0, 50.0}};
        area1.areaMm2 = 400.0;
        area1.areaCm2 = 0.04;
        area1.perimeterMm = 80.0;
        area1.centroid = {110.0, 110.0, 50.0};
        area1.visible = true;
        area1.sliceIndex = 50;

        content.areas.push_back(area1);
    }

    void addSampleVolumes(SRContent& content) {
        VolumeResult vol1;
        vol1.labelId = 1;
        vol1.labelName = "Liver";
        vol1.voxelCount = 1500000;
        vol1.volumeMm3 = 1500000.0;  // 1500 cm3
        vol1.volumeCm3 = 1500.0;
        vol1.volumeML = 1500.0;
        vol1.surfaceAreaMm2 = 120000.0;

        VolumeResult vol2;
        vol2.labelId = 2;
        vol2.labelName = "Tumor";
        vol2.voxelCount = 50000;
        vol2.volumeMm3 = 50000.0;  // 50 cm3
        vol2.volumeCm3 = 50.0;
        vol2.volumeML = 50.0;
        vol2.surfaceAreaMm2 = 8500.0;

        content.volumes.push_back(vol1);
        content.volumes.push_back(vol2);
    }

    void addSampleROIStatistics(SRContent& content) {
        SRROIStatistics stats1;
        stats1.label = "Lesion ROI";
        stats1.mean = 45.2;
        stats1.stdDev = 12.5;
        stats1.min = -50.0;
        stats1.max = 120.0;
        stats1.areaMm2 = 250.0;

        content.roiStatistics.push_back(stats1);
    }

    std::filesystem::path testDir_;
    SRContent sampleContent_;
};

// =============================================================================
// Validation Tests
// =============================================================================

TEST_F(DicomSRWriterTest, ValidateEmptyContent) {
    DicomSRWriter writer;

    SRContent emptyContent;
    emptyContent.study.studyInstanceUid = "1.2.3.4.5";  // Required field

    auto result = writer.validate(emptyContent);

    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.errors.empty());
}

TEST_F(DicomSRWriterTest, ValidateMissingStudyUid) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    content.study.studyInstanceUid = "";
    addSampleDistances(content);

    auto result = writer.validate(content);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.hasErrors());
}

TEST_F(DicomSRWriterTest, ValidateValidContentWithDistances) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto result = writer.validate(content);

    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.hasErrors());
}

TEST_F(DicomSRWriterTest, ValidateNegativeDistance) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    DistanceMeasurement dist;
    dist.distanceMm = -10.0;
    dist.label = "Invalid Distance";
    content.distances.push_back(dist);

    auto result = writer.validate(content);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.hasErrors());
}

TEST_F(DicomSRWriterTest, ValidateMissingPatientInfo) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    content.patient.patientId = "";
    content.patient.patientName = "";
    addSampleDistances(content);

    auto result = writer.validate(content);

    // Should have warnings but still be valid
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.hasWarnings());
}

// =============================================================================
// SR Creation Tests
// =============================================================================

TEST_F(DicomSRWriterTest, CreateSRWithDistances) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto result = writer.createSR(content);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->sopInstanceUid.empty());
    EXPECT_FALSE(result->seriesInstanceUid.empty());
    EXPECT_EQ(result->measurementCount, 2);
}

TEST_F(DicomSRWriterTest, CreateSRWithAngles) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleAngles(content);

    auto result = writer.createSR(content);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->measurementCount, 1);
}

TEST_F(DicomSRWriterTest, CreateSRWithAreas) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleAreas(content);

    auto result = writer.createSR(content);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->measurementCount, 1);
}

TEST_F(DicomSRWriterTest, CreateSRWithVolumes) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleVolumes(content);

    auto result = writer.createSR(content);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->measurementCount, 2);
}

TEST_F(DicomSRWriterTest, CreateSRWithAllMeasurements) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);
    addSampleAngles(content);
    addSampleAreas(content);
    addSampleVolumes(content);
    addSampleROIStatistics(content);

    auto result = writer.createSR(content);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->measurementCount, 7);  // 2 + 1 + 1 + 2 + 1
}

TEST_F(DicomSRWriterTest, CreateSRGeneratesUniqueUids) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto result1 = writer.createSR(content);
    auto result2 = writer.createSR(content);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // UIDs should be different for each creation
    EXPECT_NE(result1->sopInstanceUid, result2->sopInstanceUid);
    EXPECT_NE(result1->seriesInstanceUid, result2->seriesInstanceUid);
}

// =============================================================================
// File Save Tests
// =============================================================================

TEST_F(DicomSRWriterTest, SaveToFileSuccess) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto outputPath = testDir_ / "test_sr.dcm";
    auto result = writer.saveToFile(content, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));
    EXPECT_GT(std::filesystem::file_size(outputPath), 0);
    EXPECT_TRUE(result->filePath.has_value());
    EXPECT_EQ(result->filePath.value(), outputPath);
}

TEST_F(DicomSRWriterTest, SaveToFileWithAllMeasurements) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);
    addSampleAngles(content);
    addSampleAreas(content);
    addSampleVolumes(content);
    addSampleROIStatistics(content);

    auto outputPath = testDir_ / "full_report.dcm";
    auto result = writer.saveToFile(content, outputPath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));
    EXPECT_EQ(result->measurementCount, 7);
}

TEST_F(DicomSRWriterTest, SaveToFileInvalidDirectory) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto outputPath = std::filesystem::path("/nonexistent/directory/test.dcm");
    auto result = writer.saveToFile(content, outputPath);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SRError::Code::FileAccessDenied);
}

TEST_F(DicomSRWriterTest, SaveToFileWithCustomOptions) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    SRWriterOptions options;
    options.seriesDescription = "Custom Measurement Report";
    options.seriesNumber = 100;
    options.manufacturer = "Test Manufacturer";

    auto outputPath = testDir_ / "custom_options.dcm";
    auto result = writer.saveToFile(content, outputPath, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

TEST_F(DicomSRWriterTest, SaveToFileWithoutSpatialCoordinates) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    SRWriterOptions options;
    options.includeSpatialCoordinates = false;

    auto outputPath = testDir_ / "no_coords.dcm";
    auto result = writer.saveToFile(content, outputPath, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outputPath));
}

// =============================================================================
// Progress Callback Tests
// =============================================================================

TEST_F(DicomSRWriterTest, ProgressCallbackCalled) {
    DicomSRWriter writer;

    std::vector<double> progressValues;
    writer.setProgressCallback([&progressValues](double progress, const QString&) {
        progressValues.push_back(progress);
    });

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto result = writer.createSR(content);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(progressValues.empty());

    // Progress should start at 0 and end at 1
    EXPECT_DOUBLE_EQ(progressValues.front(), 0.0);
    EXPECT_DOUBLE_EQ(progressValues.back(), 1.0);

    // Progress should be monotonically increasing
    for (size_t i = 1; i < progressValues.size(); ++i) {
        EXPECT_GE(progressValues[i], progressValues[i - 1]);
    }
}

// =============================================================================
// Utility Method Tests
// =============================================================================

TEST_F(DicomSRWriterTest, GenerateUidIsUnique) {
    std::string uid1 = DicomSRWriter::generateUid();
    std::string uid2 = DicomSRWriter::generateUid();

    EXPECT_FALSE(uid1.empty());
    EXPECT_FALSE(uid2.empty());
    EXPECT_NE(uid1, uid2);
}

TEST_F(DicomSRWriterTest, GenerateUidIsValidDicomUid) {
    std::string uid = DicomSRWriter::generateUid();

    // DICOM UID should only contain digits and dots
    for (char c : uid) {
        EXPECT_TRUE(std::isdigit(c) || c == '.');
    }

    // DICOM UID should not start or end with a dot
    EXPECT_NE(uid.front(), '.');
    EXPECT_NE(uid.back(), '.');

    // DICOM UID max length is 64 characters
    EXPECT_LE(uid.length(), 64);
}

TEST_F(DicomSRWriterTest, GetSupportedSopClasses) {
    auto sopClasses = DicomSRWriter::getSupportedSopClasses();

    EXPECT_FALSE(sopClasses.empty());
    EXPECT_GE(sopClasses.size(), 2);

    // Should include Comprehensive SR
    bool hasComprehensiveSR = false;
    for (const auto& sopClass : sopClasses) {
        if (sopClass == DicomSRWriter::COMPREHENSIVE_SR_SOP_CLASS) {
            hasComprehensiveSR = true;
            break;
        }
    }
    EXPECT_TRUE(hasComprehensiveSR);
}

TEST_F(DicomSRWriterTest, GetAnatomicRegionCodes) {
    auto codes = DicomSRWriter::getAnatomicRegionCodes();

    EXPECT_FALSE(codes.empty());

    // Check that all codes are valid
    for (const auto& code : codes) {
        EXPECT_TRUE(code.isValid());
        EXPECT_FALSE(code.value.empty());
        EXPECT_FALSE(code.scheme.empty());
        EXPECT_FALSE(code.meaning.empty());
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(DicomSRWriterTest, SRErrorToString) {
    SRError error;
    error.code = SRError::Code::InvalidData;
    error.message = "Test error message";

    std::string str = error.toString();

    EXPECT_FALSE(str.empty());
    EXPECT_TRUE(str.find("Invalid data") != std::string::npos);
    EXPECT_TRUE(str.find("Test error message") != std::string::npos);
}

TEST_F(DicomSRWriterTest, DicomCodeValidation) {
    DicomCode validCode{"122712", "DCM", "Length"};
    EXPECT_TRUE(validCode.isValid());

    DicomCode emptyValue{"", "DCM", "Length"};
    EXPECT_FALSE(emptyValue.isValid());

    DicomCode emptyScheme{"122712", "", "Length"};
    EXPECT_FALSE(emptyScheme.isValid());

    DicomCode emptyMeaning{"122712", "DCM", ""};
    EXPECT_FALSE(emptyMeaning.isValid());
}

// =============================================================================
// SRValidationResult Tests
// =============================================================================

TEST_F(DicomSRWriterTest, ValidationResultMethods) {
    SRValidationResult result;
    result.valid = true;

    EXPECT_FALSE(result.hasErrors());
    EXPECT_FALSE(result.hasWarnings());

    result.errors.push_back("Error 1");
    EXPECT_TRUE(result.hasErrors());

    result.warnings.push_back("Warning 1");
    EXPECT_TRUE(result.hasWarnings());
}

// =============================================================================
// Output validation and format verification tests (Issue #207)
// =============================================================================

TEST_F(DicomSRWriterTest, DicomFileHasValidPreamble) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto outputPath = testDir_ / "preamble_check.dcm";
    auto result = writer.saveToFile(content, outputPath);

    if (!result.has_value()) {
        GTEST_SKIP() << "SR file creation not available: "
                     << result.error().toString();
    }

    // DICOM file format: 128 bytes preamble + "DICM" magic at offset 128
    std::ifstream file(outputPath, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    // Skip 128-byte preamble
    file.seekg(128);
    char magic[4] = {};
    file.read(magic, 4);
    ASSERT_EQ(file.gcount(), 4);

    EXPECT_EQ(std::string(magic, 4), "DICM")
        << "DICOM file must have 'DICM' magic at offset 128";
}

TEST_F(DicomSRWriterTest, SRCreationResultMeasurementCountMatchesInput) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);   // +2
    addSampleAngles(content);      // +1
    addSampleAreas(content);       // +1
    addSampleVolumes(content);     // +2
    addSampleROIStatistics(content); // +1
    // Total: 7 measurements

    auto result = writer.createSR(content);

    if (!result.has_value()) {
        GTEST_SKIP() << "SR creation not available: "
                     << result.error().toString();
    }

    EXPECT_EQ(result->measurementCount, 7u)
        << "Measurement count should match: 2 distances + 1 angle + "
           "1 area + 2 volumes + 1 ROI stat = 7";
}

TEST_F(DicomSRWriterTest, SavedFileUidFieldsAreWellFormed) {
    DicomSRWriter writer;

    SRContent content = sampleContent_;
    addSampleDistances(content);

    auto outputPath = testDir_ / "uid_check.dcm";
    auto result = writer.saveToFile(content, outputPath);

    if (!result.has_value()) {
        GTEST_SKIP() << "SR file creation not available: "
                     << result.error().toString();
    }

    // Verify UIDs follow DICOM format: digits and dots, max 64 chars
    auto isValidDicomUid = [](const std::string& uid) {
        if (uid.empty() || uid.size() > 64) return false;
        for (char c : uid) {
            if (c != '.' && (c < '0' || c > '9')) return false;
        }
        // Must not start or end with dot
        if (uid.front() == '.' || uid.back() == '.') return false;
        return true;
    };

    EXPECT_TRUE(isValidDicomUid(result->sopInstanceUid))
        << "SOP Instance UID is not well-formed: " << result->sopInstanceUid;
    EXPECT_TRUE(isValidDicomUid(result->seriesInstanceUid))
        << "Series Instance UID is not well-formed: " << result->seriesInstanceUid;

    // UIDs should be unique
    EXPECT_NE(result->sopInstanceUid, result->seriesInstanceUid)
        << "SOP Instance UID and Series Instance UID must be different";
}

}  // namespace
}  // namespace dicom_viewer::services

// Main function for Qt-based tests
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

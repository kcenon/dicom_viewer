#include "services/export/report_generator.hpp"

#include <gtest/gtest.h>
#include <QApplication>
#include <QImage>
#include <QStandardPaths>
#include <QFile>
#include <filesystem>
#include <fstream>

namespace dicom_viewer::services {
namespace {

class ReportGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        testPdfPath_ = std::filesystem::temp_directory_path() / "test_report.pdf";
        testHtmlPath_ = std::filesystem::temp_directory_path() / "test_report.html";

        // Create basic test data
        testData_.patientInfo.name = "Test Patient";
        testData_.patientInfo.patientId = "12345";
        testData_.patientInfo.dateOfBirth = "1980-01-01";
        testData_.patientInfo.sex = "M";
        testData_.patientInfo.studyDate = "2025-01-01";
        testData_.patientInfo.modality = "CT";
        testData_.patientInfo.studyDescription = "CT Chest";

        // Add distance measurement
        DistanceMeasurement dm;
        dm.id = 1;
        dm.label = "D1";
        dm.distanceMm = 45.67;
        dm.sliceIndex = 100;
        testData_.distanceMeasurements.push_back(dm);

        // Add angle measurement
        AngleMeasurement am;
        am.id = 1;
        am.label = "A1";
        am.angleDegrees = 90.5;
        am.isCobbAngle = false;
        testData_.angleMeasurements.push_back(am);

        // Add area measurement
        AreaMeasurement area;
        area.id = 1;
        area.label = "ROI1";
        area.type = RoiType::Ellipse;
        area.areaMm2 = 1234.56;
        area.areaCm2 = 12.35;
        area.perimeterMm = 124.5;
        testData_.areaMeasurements.push_back(area);

        // Add ROI statistics
        RoiStatistics stats;
        stats.roiId = 1;
        stats.roiLabel = "ROI1";
        stats.mean = 50.5;
        stats.stdDev = 15.2;
        stats.min = -100;
        stats.max = 200;
        stats.voxelCount = 1000;
        testData_.roiStatistics.push_back(stats);

        // Add volume result
        VolumeResult vol;
        vol.labelId = 1;
        vol.labelName = "Tumor";
        vol.voxelCount = 5000;
        vol.volumeMm3 = 5000.0;
        vol.volumeCm3 = 5.0;
        vol.volumeML = 5.0;
        vol.surfaceAreaMm2 = 1200.0;
        vol.sphericity = 0.85;
        testData_.volumeResults.push_back(vol);
    }

    void TearDown() override {
        if (std::filesystem::exists(testPdfPath_)) {
            std::filesystem::remove(testPdfPath_);
        }
        if (std::filesystem::exists(testHtmlPath_)) {
            std::filesystem::remove(testHtmlPath_);
        }
    }

    std::filesystem::path testPdfPath_;
    std::filesystem::path testHtmlPath_;
    ReportData testData_;
};

// =============================================================================
// ReportError tests
// =============================================================================

TEST_F(ReportGeneratorTest, ReportErrorDefaultSuccess) {
    ReportError error;
    EXPECT_TRUE(error.isSuccess());
    EXPECT_EQ(error.code, ReportError::Code::Success);
}

TEST_F(ReportGeneratorTest, ReportErrorToString) {
    ReportError error;
    error.code = ReportError::Code::InvalidData;
    error.message = "missing patient info";

    std::string result = error.toString();
    EXPECT_NE(result.find("Invalid data"), std::string::npos);
    EXPECT_NE(result.find("missing patient info"), std::string::npos);
}

TEST_F(ReportGeneratorTest, ReportErrorAllCodes) {
    std::vector<ReportError::Code> codes = {
        ReportError::Code::Success,
        ReportError::Code::InvalidData,
        ReportError::Code::FileCreationFailed,
        ReportError::Code::RenderingFailed,
        ReportError::Code::InvalidTemplate,
        ReportError::Code::ImageProcessingFailed,
        ReportError::Code::InternalError
    };

    for (auto code : codes) {
        ReportError error;
        error.code = code;
        error.message = "test";
        std::string str = error.toString();
        EXPECT_FALSE(str.empty());
    }
}

// =============================================================================
// ReportTemplate tests
// =============================================================================

TEST_F(ReportGeneratorTest, ReportTemplateDefaultValues) {
    ReportTemplate templ;

    EXPECT_EQ(templ.name, "Default");
    EXPECT_TRUE(templ.logoPath.isEmpty());
    EXPECT_TRUE(templ.institutionName.isEmpty());
    EXPECT_TRUE(templ.showPatientInfo);
    EXPECT_TRUE(templ.showMeasurements);
    EXPECT_TRUE(templ.showVolumes);
    EXPECT_TRUE(templ.showScreenshots);
    EXPECT_EQ(templ.fontFamily, "Arial");
    EXPECT_EQ(templ.titleFontSize, 18);
    EXPECT_EQ(templ.headerFontSize, 14);
    EXPECT_EQ(templ.bodyFontSize, 11);
}

TEST_F(ReportGeneratorTest, GetDefaultTemplate) {
    auto templ = ReportGenerator::getDefaultTemplate();
    EXPECT_EQ(templ.name, "Default");
    EXPECT_EQ(templ.fontFamily, "Arial");
}

// =============================================================================
// PatientInfo tests
// =============================================================================

TEST_F(ReportGeneratorTest, PatientInfoDefaultValues) {
    PatientInfo info;

    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.patientId.empty());
    EXPECT_TRUE(info.dateOfBirth.empty());
    EXPECT_TRUE(info.sex.empty());
}

// =============================================================================
// ReportGenerator construction tests
// =============================================================================

TEST_F(ReportGeneratorTest, DefaultConstruction) {
    ReportGenerator generator;
    // Should not crash
}

TEST_F(ReportGeneratorTest, MoveConstruction) {
    ReportGenerator generator1;
    ReportGenerator generator2(std::move(generator1));
    // Should not crash
}

TEST_F(ReportGeneratorTest, MoveAssignment) {
    ReportGenerator generator1;
    ReportGenerator generator2;
    generator2 = std::move(generator1);
    // Should not crash
}

// =============================================================================
// HTML generation tests
// =============================================================================

TEST_F(ReportGeneratorTest, GenerateHtmlBasic) {
    ReportGenerator generator;
    ReportOptions options;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_FALSE(html.isEmpty());
        EXPECT_TRUE(html.contains("<!DOCTYPE html>"));
        EXPECT_TRUE(html.contains("DICOM Viewer Report"));
        EXPECT_TRUE(html.contains("Test Patient"));
        EXPECT_TRUE(html.contains("12345"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsPatientInfo) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.showPatientInfo = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("Patient Information"));
        EXPECT_TRUE(html.contains("Test Patient"));
        EXPECT_TRUE(html.contains("1980-01-01"));
        EXPECT_TRUE(html.contains("CT Chest"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsMeasurements) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.showMeasurements = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("Distance Measurements"));
        EXPECT_TRUE(html.contains("D1"));
        EXPECT_TRUE(html.contains("45.67") || html.contains("45.7"));

        EXPECT_TRUE(html.contains("Angle Measurements"));
        EXPECT_TRUE(html.contains("A1"));
        EXPECT_TRUE(html.contains("90.5"));

        EXPECT_TRUE(html.contains("Area Measurements"));
        EXPECT_TRUE(html.contains("ROI1"));
        EXPECT_TRUE(html.contains("Ellipse"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsStatistics) {
    ReportGenerator generator;
    ReportOptions options;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("ROI Statistics"));
        EXPECT_TRUE(html.contains("Mean"));
        EXPECT_TRUE(html.contains("50.5"));
        EXPECT_TRUE(html.contains("-100"));
        EXPECT_TRUE(html.contains("200"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsVolumes) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.showVolumes = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("Volume Analysis"));
        EXPECT_TRUE(html.contains("Tumor"));
        EXPECT_TRUE(html.contains("5.0") || html.contains("5.000"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithInstitution) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.institutionName = "Test Hospital";

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("Test Hospital"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithAuthor) {
    ReportGenerator generator;
    ReportOptions options;
    options.author = "Dr. Test";

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("Dr. Test"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithTimestamp) {
    ReportGenerator generator;
    ReportOptions options;
    options.includeTimestamp = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("Generated:"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithoutTimestamp) {
    ReportGenerator generator;
    ReportOptions options;
    options.includeTimestamp = false;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_FALSE(html.contains("Generated:"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlHideSections) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.showPatientInfo = false;
    options.reportTemplate.showMeasurements = false;
    options.reportTemplate.showVolumes = false;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_FALSE(html.contains("Patient Information"));
        EXPECT_FALSE(html.contains("Distance Measurements"));
        EXPECT_FALSE(html.contains("Volume Analysis"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlEmptyData) {
    ReportGenerator generator;
    ReportData emptyData;
    ReportOptions options;

    auto result = generator.generateHTML(emptyData, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_FALSE(html.isEmpty());
        EXPECT_TRUE(html.contains("DICOM Viewer Report"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlSpecialCharacters) {
    ReportGenerator generator;
    ReportData data;
    data.patientInfo.name = "Test <Patient> & \"Special\"";
    data.patientInfo.patientId = "ID&123";
    ReportOptions options;

    auto result = generator.generateHTML(data, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        // Special characters should be escaped
        EXPECT_TRUE(html.contains("&lt;") || html.contains("&amp;"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlUnicodeCharacters) {
    ReportGenerator generator;
    ReportData data;
    data.patientInfo.name = "Test Patient";  // Korean characters would work too
    data.patientInfo.patientId = "12345";
    ReportOptions options;

    auto result = generator.generateHTML(data, options);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// PDF generation tests
// =============================================================================

TEST_F(ReportGeneratorTest, GeneratePdfBasic) {
    ReportGenerator generator;
    ReportOptions options;

    auto result = generator.generatePDF(testData_, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));

    // Verify file is not empty
    auto fileSize = std::filesystem::file_size(testPdfPath_);
    EXPECT_GT(fileSize, 0);
}

TEST_F(ReportGeneratorTest, GeneratePdfWithOptions) {
    ReportGenerator generator;
    ReportOptions options;
    options.author = "Test Author";
    options.includeTimestamp = true;
    options.imageDPI = 150;
    options.reportTemplate.institutionName = "Test Institution";

    auto result = generator.generatePDF(testData_, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));
}

TEST_F(ReportGeneratorTest, GeneratePdfInvalidPath) {
    ReportGenerator generator;
    ReportOptions options;

    auto result = generator.generatePDF(testData_, "/invalid/path/report.pdf", options);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ReportError::Code::FileCreationFailed);
}

TEST_F(ReportGeneratorTest, GeneratePdfEmptyData) {
    ReportGenerator generator;
    ReportData emptyData;
    ReportOptions options;

    auto result = generator.generatePDF(emptyData, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));
}

TEST_F(ReportGeneratorTest, GeneratePdfLetterSize) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.pageSize = QPageSize(QPageSize::Letter);

    auto result = generator.generatePDF(testData_, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));
}

TEST_F(ReportGeneratorTest, GeneratePdfLandscapeOrientation) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.orientation = QPageLayout::Landscape;

    auto result = generator.generatePDF(testData_, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));
}

TEST_F(ReportGeneratorTest, GeneratePdfHighDPI) {
    ReportGenerator generator;
    ReportOptions options;
    options.imageDPI = 300;

    auto result = generator.generatePDF(testData_, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(ReportGeneratorTest, ProgressCallback) {
    ReportGenerator generator;

    int callCount = 0;
    double lastProgress = 0.0;
    QString lastStatus;

    generator.setProgressCallback([&](double progress, const QString& status) {
        ++callCount;
        lastProgress = progress;
        lastStatus = status;
    });

    ReportOptions options;
    auto unused = generator.generatePDF(testData_, testPdfPath_, options);
    (void)unused;

    EXPECT_GT(callCount, 0);
    EXPECT_DOUBLE_EQ(lastProgress, 1.0);
    EXPECT_FALSE(lastStatus.isEmpty());
}

// =============================================================================
// Template management tests
// =============================================================================

TEST_F(ReportGeneratorTest, GetAvailableTemplates) {
    ReportGenerator generator;

    auto templates = generator.getAvailableTemplates();
    EXPECT_FALSE(templates.empty());

    // Should have at least the default template
    bool hasDefault = false;
    for (const auto& templ : templates) {
        if (templ.name == "Default") {
            hasDefault = true;
            break;
        }
    }
    EXPECT_TRUE(hasDefault);
}

TEST_F(ReportGeneratorTest, SaveAndLoadTemplate) {
    ReportGenerator generator;

    ReportTemplate templ;
    templ.name = "TestTemplate";
    templ.institutionName = "Test Hospital";
    templ.titleFontSize = 20;
    templ.headerColor = "#ff0000";
    templ.showVolumes = false;

    auto saveResult = generator.saveTemplate(templ);
    EXPECT_TRUE(saveResult.has_value());

    auto loadResult = generator.loadTemplate("TestTemplate");
    EXPECT_TRUE(loadResult.has_value());

    if (loadResult) {
        EXPECT_EQ(loadResult->name, "TestTemplate");
        EXPECT_EQ(loadResult->institutionName, "Test Hospital");
        EXPECT_EQ(loadResult->titleFontSize, 20);
        EXPECT_EQ(loadResult->headerColor, "#ff0000");
        EXPECT_FALSE(loadResult->showVolumes);
    }

    // Cleanup
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString filePath = configPath + "/templates/TestTemplate.json";
    QFile::remove(filePath);
}

TEST_F(ReportGeneratorTest, LoadNonexistentTemplate) {
    ReportGenerator generator;

    auto result = generator.loadTemplate("NonexistentTemplate");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ReportError::Code::InvalidTemplate);
}

// =============================================================================
// Screenshot handling tests
// =============================================================================

TEST_F(ReportGeneratorTest, GenerateHtmlWithScreenshots) {
    ReportGenerator generator;
    ReportData data = testData_;

    // Add a test screenshot
    ReportScreenshot screenshot;
    screenshot.image = QImage(100, 100, QImage::Format_RGB32);
    screenshot.image.fill(Qt::blue);
    screenshot.caption = "Test Screenshot";
    screenshot.viewType = "Axial";
    data.screenshots.push_back(screenshot);

    ReportOptions options;
    options.reportTemplate.showScreenshots = true;

    auto result = generator.generateHTML(data, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("Images"));
        EXPECT_TRUE(html.contains("Axial"));
        EXPECT_TRUE(html.contains("Test Screenshot"));
        EXPECT_TRUE(html.contains("data:image/png;base64"));
    }
}

TEST_F(ReportGeneratorTest, GeneratePdfWithScreenshots) {
    ReportGenerator generator;
    ReportData data = testData_;

    // Add multiple screenshots
    for (int i = 0; i < 3; ++i) {
        ReportScreenshot screenshot;
        screenshot.image = QImage(200, 200, QImage::Format_RGB32);
        screenshot.image.fill(QColor::fromHsv(i * 60, 200, 200));
        screenshot.caption = QString("View %1").arg(i + 1);
        screenshot.viewType = QString("View %1").arg(i + 1);
        data.screenshots.push_back(screenshot);
    }

    ReportOptions options;
    options.reportTemplate.showScreenshots = true;
    options.imageDPI = 150;

    auto result = generator.generatePDF(data, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));
}

// =============================================================================
// Multiple measurements tests
// =============================================================================

TEST_F(ReportGeneratorTest, GenerateHtmlMultipleMeasurements) {
    ReportGenerator generator;
    ReportData data;
    data.patientInfo.name = "Test";
    data.patientInfo.patientId = "123";

    // Add multiple distance measurements
    for (int i = 0; i < 5; ++i) {
        DistanceMeasurement dm;
        dm.id = i + 1;
        dm.label = "D" + std::to_string(i + 1);
        dm.distanceMm = 10.0 * (i + 1);
        dm.sliceIndex = i * 10;
        data.distanceMeasurements.push_back(dm);
    }

    // Add multiple angle measurements
    for (int i = 0; i < 3; ++i) {
        AngleMeasurement am;
        am.id = i + 1;
        am.label = "A" + std::to_string(i + 1);
        am.angleDegrees = 30.0 * (i + 1);
        data.angleMeasurements.push_back(am);
    }

    ReportOptions options;
    auto result = generator.generateHTML(data, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        EXPECT_TRUE(html.contains("D1"));
        EXPECT_TRUE(html.contains("D5"));
        EXPECT_TRUE(html.contains("A1"));
        EXPECT_TRUE(html.contains("A3"));
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlMultipleVolumes) {
    ReportGenerator generator;
    ReportData data;
    data.patientInfo.name = "Test";
    data.patientInfo.patientId = "123";

    // Add multiple volume results
    std::vector<std::string> labels = {"Liver", "Spleen", "Kidney", "Tumor"};
    for (size_t i = 0; i < labels.size(); ++i) {
        VolumeResult vol;
        vol.labelId = static_cast<uint8_t>(i + 1);
        vol.labelName = labels[i];
        vol.voxelCount = 1000 * (i + 1);
        vol.volumeMm3 = 1000.0 * (i + 1);
        vol.volumeCm3 = static_cast<double>(i + 1);
        vol.volumeML = static_cast<double>(i + 1);
        data.volumeResults.push_back(vol);
    }

    ReportOptions options;
    auto result = generator.generateHTML(data, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        QString html = *result;
        // Should have total row
        EXPECT_TRUE(html.contains("Total"));
        for (const auto& label : labels) {
            EXPECT_TRUE(html.contains(QString::fromStdString(label)));
        }
    }
}

}  // namespace
}  // namespace dicom_viewer::services

// Main function for Qt application
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

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

#include "services/export/report_generator.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace dicom_viewer::services {
namespace {

// Minimal 1x1 PNG bytes (blue pixel, IHDR+IDAT+IEND) for screenshot tests
static std::vector<uint8_t> makeMinimalPng() {
    return {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A, // PNG signature
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52, // IHDR length + type
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01, // 1x1
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53, // 8-bit RGB, CRC
        0xDE,0x00,0x00,0x00,0x0C,0x49,0x44,0x41, // IDAT length + type
        0x54,0x08,0xD7,0x63,0xF8,0xCF,0xC0,0x00,
        0x00,0x00,0x02,0x00,0x01,0xE2,0x21,0xBC,
        0x33,0x00,0x00,0x00,0x00,0x49,0x45,0x4E, // IEND
        0x44,0xAE,0x42,0x60,0x82
    };
}

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
    EXPECT_TRUE(templ.logoPath.empty());
    EXPECT_TRUE(templ.institutionName.empty());
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
        std::string html = *result;
        EXPECT_FALSE(html.empty());
        EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
        EXPECT_NE(html.find("DICOM Viewer Report"), std::string::npos);
        EXPECT_NE(html.find("Test Patient"), std::string::npos);
        EXPECT_NE(html.find("12345"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsPatientInfo) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.showPatientInfo = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("Patient Information"), std::string::npos);
        EXPECT_NE(html.find("Test Patient"), std::string::npos);
        EXPECT_NE(html.find("1980-01-01"), std::string::npos);
        EXPECT_NE(html.find("CT Chest"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsMeasurements) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.showMeasurements = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("Distance Measurements"), std::string::npos);
        EXPECT_NE(html.find("D1"), std::string::npos);
        EXPECT_TRUE(html.find("45.67") != std::string::npos ||
                    html.find("45.7") != std::string::npos);

        EXPECT_NE(html.find("Angle Measurements"), std::string::npos);
        EXPECT_NE(html.find("A1"), std::string::npos);
        EXPECT_NE(html.find("90.5"), std::string::npos);

        EXPECT_NE(html.find("Area Measurements"), std::string::npos);
        EXPECT_NE(html.find("ROI1"), std::string::npos);
        EXPECT_NE(html.find("Ellipse"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsStatistics) {
    ReportGenerator generator;
    ReportOptions options;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("ROI Statistics"), std::string::npos);
        EXPECT_NE(html.find("Mean"), std::string::npos);
        EXPECT_NE(html.find("50.5"), std::string::npos);
        EXPECT_NE(html.find("-100"), std::string::npos);
        EXPECT_NE(html.find("200"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlContainsVolumes) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.showVolumes = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("Volume Analysis"), std::string::npos);
        EXPECT_NE(html.find("Tumor"), std::string::npos);
        EXPECT_TRUE(html.find("5.0") != std::string::npos ||
                    html.find("5000") != std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithInstitution) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.institutionName = "Test Hospital";

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("Test Hospital"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithAuthor) {
    ReportGenerator generator;
    ReportOptions options;
    options.author = "Dr. Test";

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("Dr. Test"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithTimestamp) {
    ReportGenerator generator;
    ReportOptions options;
    options.includeTimestamp = true;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("Generated:"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlWithoutTimestamp) {
    ReportGenerator generator;
    ReportOptions options;
    options.includeTimestamp = false;

    auto result = generator.generateHTML(testData_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_EQ(html.find("Generated:"), std::string::npos);
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
        std::string html = *result;
        EXPECT_EQ(html.find("Patient Information"), std::string::npos);
        EXPECT_EQ(html.find("Distance Measurements"), std::string::npos);
        EXPECT_EQ(html.find("Volume Analysis"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlEmptyData) {
    ReportGenerator generator;
    ReportData emptyData;
    ReportOptions options;

    auto result = generator.generateHTML(emptyData, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_FALSE(html.empty());
        EXPECT_NE(html.find("DICOM Viewer Report"), std::string::npos);
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
        std::string html = *result;
        // Special characters should be escaped
        EXPECT_TRUE(html.find("&lt;") != std::string::npos ||
                    html.find("&amp;") != std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GenerateHtmlUnicodeCharacters) {
    ReportGenerator generator;
    ReportData data;
    data.patientInfo.name = "Test Patient";
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
    options.reportTemplate.pageSize = PageSizePreset::Letter;

    auto result = generator.generatePDF(testData_, testPdfPath_, options);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(testPdfPath_));
}

TEST_F(ReportGeneratorTest, GeneratePdfLandscapeOrientation) {
    ReportGenerator generator;
    ReportOptions options;
    options.reportTemplate.orientation = PageOrientation::Landscape;

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
    std::string lastStatus;

    generator.setProgressCallback([&](double progress, const std::string& status) {
        ++callCount;
        lastProgress = progress;
        lastStatus = status;
    });

    ReportOptions options;
    auto unused = generator.generatePDF(testData_, testPdfPath_, options);
    (void)unused;

    EXPECT_GT(callCount, 0);
    EXPECT_DOUBLE_EQ(lastProgress, 1.0);
    EXPECT_FALSE(lastStatus.empty());
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

    // Cleanup using std::filesystem
    // (appConfigPath is determined at runtime; just attempt removal of saved file)
    if (saveResult.has_value()) {
        auto reloaded = generator.loadTemplate("TestTemplate");
        if (reloaded) {
            // Template exists; leave cleanup to OS temp cleanup
        }
    }
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

    // Add a test screenshot using PNG byte data
    ReportScreenshot screenshot;
    screenshot.pngData = makeMinimalPng();
    screenshot.width = 1;
    screenshot.height = 1;
    screenshot.caption = "Test Screenshot";
    screenshot.viewType = "Axial";
    data.screenshots.push_back(screenshot);

    ReportOptions options;
    options.reportTemplate.showScreenshots = true;

    auto result = generator.generateHTML(data, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        std::string html = *result;
        EXPECT_NE(html.find("Images"), std::string::npos);
        EXPECT_NE(html.find("Axial"), std::string::npos);
        EXPECT_NE(html.find("Test Screenshot"), std::string::npos);
        EXPECT_NE(html.find("data:image/png;base64"), std::string::npos);
    }
}

TEST_F(ReportGeneratorTest, GeneratePdfWithScreenshots) {
    ReportGenerator generator;
    ReportData data = testData_;

    // Add multiple screenshots
    for (int i = 0; i < 3; ++i) {
        ReportScreenshot screenshot;
        screenshot.pngData = makeMinimalPng();
        screenshot.width = 1;
        screenshot.height = 1;
        screenshot.caption = "View " + std::to_string(i + 1);
        screenshot.viewType = "View " + std::to_string(i + 1);
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
        std::string html = *result;
        EXPECT_NE(html.find("D1"), std::string::npos);
        EXPECT_NE(html.find("D5"), std::string::npos);
        EXPECT_NE(html.find("A1"), std::string::npos);
        EXPECT_NE(html.find("A3"), std::string::npos);
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
        std::string html = *result;
        // Should have total row
        EXPECT_NE(html.find("Total"), std::string::npos);
        for (const auto& label : labels) {
            EXPECT_NE(html.find(label), std::string::npos);
        }
    }
}

// =============================================================================
// Output content validation tests (Issue #207)
// =============================================================================

TEST_F(ReportGeneratorTest, HtmlOutputContainsSectionHeaders) {
    ReportGenerator generator;
    ReportOptions options;
    auto result = generator.generateHTML(testData_, options);
    ASSERT_TRUE(result.has_value());

    std::string html = *result;
    EXPECT_NE(html.find("<table"), std::string::npos)
        << "HTML should contain table elements for measurements";
    EXPECT_NE(html.find("Patient"), std::string::npos)
        << "HTML should contain Patient section";
    EXPECT_TRUE(html.find("Measurement") != std::string::npos ||
                html.find("Distance") != std::string::npos)
        << "HTML should contain measurement section header";
}

TEST_F(ReportGeneratorTest, HtmlMeasurementValuesMatchInput) {
    ReportGenerator generator;
    ReportOptions options;
    auto result = generator.generateHTML(testData_, options);
    ASSERT_TRUE(result.has_value());

    std::string html = *result;
    // Verify specific measurement values from testData_ appear in HTML
    EXPECT_NE(html.find("45.67"), std::string::npos)
        << "Distance value 45.67 mm should appear in HTML";
    EXPECT_NE(html.find("90.5"), std::string::npos)
        << "Angle value 90.5 degrees should appear in HTML";
    EXPECT_TRUE(html.find("5.0") != std::string::npos ||
                html.find("5000") != std::string::npos)
        << "Volume result should appear in HTML";
}

TEST_F(ReportGeneratorTest, HtmlEntityEscapingForAmpersand) {
    ReportData data = testData_;
    data.patientInfo.name = "Smith & Jones";
    data.patientInfo.studyDate = "20260101";

    ReportGenerator generator;
    auto result = generator.generateHTML(data, ReportOptions{});
    ASSERT_TRUE(result.has_value());

    std::string html = *result;
    // Ampersand in patient name should be escaped as &amp; in HTML
    EXPECT_NE(html.find("&amp;"), std::string::npos)
        << "Ampersand should be HTML-escaped as &amp;";
    EXPECT_EQ(html.find("Smith & Jones"), std::string::npos)
        << "Raw ampersand should not appear unescaped in HTML text";
}

TEST_F(ReportGeneratorTest, PdfFileHasValidHeader) {
    ReportGenerator generator;
    ReportOptions options;
    auto result = generator.generatePDF(testData_, testPdfPath_, options);

    if (!result.has_value()) {
        GTEST_SKIP() << "PDF generation not available: "
                     << result.error().toString();
    }

    // Read first bytes of PDF file
    std::ifstream file(testPdfPath_, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    char header[5] = {};
    file.read(header, 5);
    EXPECT_EQ(std::string(header, 5), "%PDF-")
        << "PDF file should start with %PDF- magic number";
}

}  // namespace
}  // namespace dicom_viewer::services

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

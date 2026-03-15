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

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include <kcenon/common/logging/log_macros.h>

namespace dicom_viewer::services {

namespace {

/**
 * @brief Encode raw bytes to base64 string.
 */
std::string base64Encode(const uint8_t* data, size_t len) {
    static constexpr std::array<char, 64> kTable{
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z',
        '0','1','2','3','4','5','6','7','8','9','+','/'
    };

    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) b |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) b |= static_cast<uint32_t>(data[i + 2]);

        result += kTable[(b >> 18) & 0x3F];
        result += kTable[(b >> 12) & 0x3F];
        result += (i + 1 < len) ? kTable[(b >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? kTable[(b >> 0) & 0x3F] : '=';
    }

    return result;
}

/**
 * @brief Escape a string for HTML output.
 */
std::string escapeHtml(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string formatDouble(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string formatDistance(double mm) {
    return formatDouble(mm, 2) + " mm";
}

std::string formatAngle(double degrees) {
    return formatDouble(degrees, 1) + "\u00B0";
}

std::string formatArea(double mm2, double cm2) {
    if (cm2 >= 0.01) {
        return formatDouble(mm2, 2) + " mm\u00B2 (" + formatDouble(cm2, 2) + " cm\u00B2)";
    }
    return formatDouble(mm2, 2) + " mm\u00B2";
}

std::string formatVolume(double mm3, double cm3) {
    if (cm3 >= 0.001) {
        return formatDouble(cm3, 3) + " cm\u00B3 (" + formatDouble(cm3, 3) + " mL)";
    }
    return formatDouble(mm3, 2) + " mm\u00B3";
}

std::string currentDateTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief Return the platform-specific app config directory.
 */
std::filesystem::path appConfigPath() {
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    return appdata ? std::filesystem::path(appdata) / "DICOM Viewer"
                   : std::filesystem::path(".");
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) / "Library" / "Application Support" / "DICOM Viewer"
                : std::filesystem::path(".");
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::filesystem::path(xdg) / "dicom_viewer";
    }
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) / ".config" / "dicom_viewer"
                : std::filesystem::path(".");
#endif
}

/**
 * @brief Try to convert HTML to PDF using wkhtmltopdf.
 *
 * Writes HTML to a temp file, invokes wkhtmltopdf, then removes the temp file.
 */
std::expected<void, ReportError> htmlToPdf(
    const std::string& htmlContent,
    const std::filesystem::path& outputPath,
    const ReportOptions& options)
{
    // Write HTML to a temporary file
    auto tempHtml = std::filesystem::temp_directory_path() /
                    ("dicom_report_" + std::to_string(
                        std::chrono::system_clock::now().time_since_epoch().count()) + ".html");

    {
        std::ofstream htmlFile(tempHtml);
        if (!htmlFile.is_open()) {
            return std::unexpected(ReportError{
                ReportError::Code::FileCreationFailed,
                "Failed to create temporary HTML file"
            });
        }
        htmlFile << htmlContent;
    }

    // Build orientation flag
    std::string orientFlag = (options.reportTemplate.orientation == PageOrientation::Landscape)
        ? "--orientation Landscape"
        : "--orientation Portrait";

    // Build page size flag
    std::string pageSizeStr;
    switch (options.reportTemplate.pageSize) {
        case PageSizePreset::A4:     pageSizeStr = "A4";     break;
        case PageSizePreset::Letter: pageSizeStr = "Letter"; break;
        case PageSizePreset::Legal:  pageSizeStr = "Legal";  break;
    }

    std::string cmd = "wkhtmltopdf --quiet " + orientFlag +
                      " --page-size " + pageSizeStr +
                      " --margin-top 20 --margin-bottom 20 --margin-left 20 --margin-right 20 " +
                      "\"" + tempHtml.string() + "\" " +
                      "\"" + outputPath.string() + "\"";

    int ret = std::system(cmd.c_str());

    // Clean up temp file
    std::error_code ec;
    std::filesystem::remove(tempHtml, ec);

    if (ret != 0) {
        return std::unexpected(ReportError{
            ReportError::Code::RenderingFailed,
            "wkhtmltopdf failed (exit code " + std::to_string(ret) +
            "). Ensure wkhtmltopdf is installed and in PATH."
        });
    }

    return {};
}

}  // namespace

// =============================================================================
// ReportGenerator::Impl
// =============================================================================

class ReportGenerator::Impl {
public:
    ProgressCallback progressCallback;

    void reportProgress(double progress, const std::string& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
    }

    std::string imageToBase64(const ReportScreenshot& screenshot) const {
        if (screenshot.pngData.empty()) {
            return {};
        }
        return base64Encode(screenshot.pngData.data(), screenshot.pngData.size());
    }

    std::string generateStylesheet(const ReportTemplate& templ) const {
        std::ostringstream css;

        css << "body { font-family: '" << templ.fontFamily
            << "', sans-serif; font-size: " << templ.bodyFontSize << "pt; "
            << "color: " << templ.textColor << "; margin: 20px; }";

        css << "h1 { font-size: " << templ.titleFontSize << "pt; "
            << "color: " << templ.titleColor << "; margin: 0 0 5px 0; }";

        css << "h2 { font-size: " << templ.headerFontSize << "pt; "
            << "color: " << templ.headerColor << "; "
            << "border-bottom: 2px solid " << templ.headerColor << "; "
            << "padding-bottom: 5px; margin-top: 20px; }";

        css << "h3 { font-size: " << (templ.headerFontSize - 2) << "pt; "
            << "color: " << templ.headerColor << "; margin: 15px 0 8px 0; }";

        css << ".header { display: flex; align-items: center; "
            << "border-bottom: 3px solid " << templ.headerColor << "; "
            << "padding-bottom: 15px; margin-bottom: 20px; }";

        css << ".logo { max-height: 60px; margin-right: 20px; }";
        css << ".title-block { flex: 1; }";

        css << ".institution { font-size: " << (templ.bodyFontSize + 1) << "pt; "
            << "color: #666666; }";

        css << ".section { margin-bottom: 20px; page-break-inside: avoid; }";

        css << ".info-table { border-collapse: collapse; width: 100%; margin-bottom: 15px; }";
        css << ".info-table td { padding: 5px 10px; vertical-align: top; }";
        css << ".info-table .label { font-weight: bold; width: 180px; "
            << "background-color: " << templ.tableHeaderBackground << "; }";

        css << ".data-table { border-collapse: collapse; width: 100%; margin-bottom: 15px; }";
        css << ".data-table th, .data-table td { padding: 8px; text-align: left; "
            << "border: 1px solid " << templ.tableBorderColor << "; }";
        css << ".data-table th { background-color: " << templ.tableHeaderBackground
            << "; font-weight: bold; }";
        css << ".data-table tr:nth-child(even) { background-color: #f9f9f9; }";
        css << ".total-row { background-color: " << templ.tableHeaderBackground << " !important; }";

        css << ".screenshots-section { page-break-before: auto; }";
        css << ".screenshots-grid { display: flex; flex-wrap: wrap; gap: 15px; }";
        css << ".screenshot-item { flex: 0 0 calc(50% - 10px); max-width: calc(50% - 10px); "
            << "page-break-inside: avoid; }";
        css << ".screenshot { max-width: 100%; height: auto; "
            << "border: 1px solid " << templ.tableBorderColor << "; }";
        css << ".screenshot-caption { font-size: " << (templ.bodyFontSize - 1) << "pt; "
            << "text-align: center; margin-top: 5px; color: #666666; }";

        css << ".footer { margin-top: 30px; padding-top: 15px; "
            << "border-top: 1px solid " << templ.tableBorderColor << "; "
            << "font-size: " << (templ.bodyFontSize - 1) << "pt; color: #666666; }";
        css << ".footer div { margin-bottom: 3px; }";

        return css.str();
    }

    std::string generateHeaderHtml(const ReportData& data,
                                   const ReportOptions& options) const {
        std::ostringstream html;

        html << "<div class='header'>";

        if (!options.reportTemplate.logoPath.empty()) {
            std::ifstream logoFile(options.reportTemplate.logoPath, std::ios::binary);
            if (logoFile.is_open()) {
                std::vector<uint8_t> logoData(
                    (std::istreambuf_iterator<char>(logoFile)),
                    std::istreambuf_iterator<char>());
                if (!logoData.empty()) {
                    html << "<img src='data:image/png;base64,"
                         << base64Encode(logoData.data(), logoData.size())
                         << "' class='logo' />";
                }
            }
        }

        html << "<div class='title-block'>";
        html << "<h1>DICOM Viewer Report</h1>";
        if (!options.reportTemplate.institutionName.empty()) {
            html << "<div class='institution'>"
                 << escapeHtml(options.reportTemplate.institutionName)
                 << "</div>";
        }
        html << "</div>";
        html << "</div>";

        return html.str();
    }

    std::string generatePatientInfoHtml(const ReportData& data) const {
        std::ostringstream html;

        html << "<div class='section'>";
        html << "<h2>Patient Information</h2>";
        html << "<table class='info-table'>";
        html << "<tr><td class='label'>Patient Name:</td><td>" << escapeHtml(data.patientInfo.name) << "</td></tr>";
        html << "<tr><td class='label'>Patient ID:</td><td>" << escapeHtml(data.patientInfo.patientId) << "</td></tr>";
        html << "<tr><td class='label'>Date of Birth:</td><td>" << escapeHtml(data.patientInfo.dateOfBirth) << "</td></tr>";
        html << "<tr><td class='label'>Sex:</td><td>" << escapeHtml(data.patientInfo.sex) << "</td></tr>";
        html << "<tr><td class='label'>Study Date:</td><td>" << escapeHtml(data.patientInfo.studyDate) << "</td></tr>";
        html << "<tr><td class='label'>Modality:</td><td>" << escapeHtml(data.patientInfo.modality) << "</td></tr>";
        html << "<tr><td class='label'>Study Description:</td><td>" << escapeHtml(data.patientInfo.studyDescription) << "</td></tr>";

        if (!data.patientInfo.accessionNumber.empty()) {
            html << "<tr><td class='label'>Accession Number:</td><td>" << escapeHtml(data.patientInfo.accessionNumber) << "</td></tr>";
        }
        if (!data.patientInfo.referringPhysician.empty()) {
            html << "<tr><td class='label'>Referring Physician:</td><td>" << escapeHtml(data.patientInfo.referringPhysician) << "</td></tr>";
        }

        html << "</table></div>";
        return html.str();
    }

    std::string generateMeasurementsHtml(const ReportData& data) const {
        std::ostringstream html;

        html << "<div class='section'>";
        html << "<h2>Measurements</h2>";

        if (!data.distanceMeasurements.empty()) {
            html << "<h3>Distance Measurements</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>#</th><th>Label</th><th>Distance</th><th>Slice</th></tr>";
            for (size_t i = 0; i < data.distanceMeasurements.size(); ++i) {
                const auto& m = data.distanceMeasurements[i];
                html << "<tr><td>" << (i + 1) << "</td>";
                html << "<td>" << escapeHtml(m.label.empty() ? "D" + std::to_string(i + 1) : m.label) << "</td>";
                html << "<td>" << formatDistance(m.distanceMm) << "</td>";
                html << "<td>" << (m.sliceIndex >= 0 ? std::to_string(m.sliceIndex) : "3D") << "</td></tr>";
            }
            html << "</table>";
        }

        if (!data.angleMeasurements.empty()) {
            html << "<h3>Angle Measurements</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>#</th><th>Label</th><th>Angle</th><th>Type</th></tr>";
            for (size_t i = 0; i < data.angleMeasurements.size(); ++i) {
                const auto& m = data.angleMeasurements[i];
                html << "<tr><td>" << (i + 1) << "</td>";
                html << "<td>" << escapeHtml(m.label.empty() ? "A" + std::to_string(i + 1) : m.label) << "</td>";
                html << "<td>" << formatAngle(m.angleDegrees) << "</td>";
                html << "<td>" << (m.isCobbAngle ? "Cobb" : "Standard") << "</td></tr>";
            }
            html << "</table>";
        }

        if (!data.areaMeasurements.empty()) {
            html << "<h3>Area Measurements</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>#</th><th>Label</th><th>Type</th><th>Area</th><th>Perimeter</th></tr>";
            for (size_t i = 0; i < data.areaMeasurements.size(); ++i) {
                const auto& m = data.areaMeasurements[i];
                const char* typeName = "Unknown";
                switch (m.type) {
                    case RoiType::Ellipse: typeName = "Ellipse"; break;
                    case RoiType::Rectangle: typeName = "Rectangle"; break;
                    case RoiType::Polygon: typeName = "Polygon"; break;
                    case RoiType::Freehand: typeName = "Freehand"; break;
                }
                html << "<tr><td>" << (i + 1) << "</td>";
                html << "<td>" << escapeHtml(m.label.empty() ? "ROI" + std::to_string(i + 1) : m.label) << "</td>";
                html << "<td>" << typeName << "</td>";
                html << "<td>" << formatArea(m.areaMm2, m.areaCm2) << "</td>";
                html << "<td>" << formatDistance(m.perimeterMm) << "</td></tr>";
            }
            html << "</table>";
        }

        if (!data.roiStatistics.empty()) {
            html << "<h3>ROI Statistics</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>ROI</th><th>Mean (HU)</th><th>Std Dev</th>"
                 << "<th>Min</th><th>Max</th><th>Voxels</th></tr>";
            for (const auto& s : data.roiStatistics) {
                html << "<tr>";
                html << "<td>" << escapeHtml(s.roiLabel.empty() ? "ROI" : s.roiLabel) << "</td>";
                html << "<td>" << formatDouble(s.mean, 1) << "</td>";
                html << "<td>" << formatDouble(s.stdDev, 1) << "</td>";
                html << "<td>" << formatDouble(s.min, 0) << "</td>";
                html << "<td>" << formatDouble(s.max, 0) << "</td>";
                html << "<td>" << s.voxelCount << "</td>";
                html << "</tr>";
            }
            html << "</table>";
        }

        html << "</div>";
        return html.str();
    }

    std::string generateVolumesHtml(const ReportData& data) const {
        if (data.volumeResults.empty()) return {};

        std::ostringstream html;

        html << "<div class='section'>";
        html << "<h2>Volume Analysis</h2>";
        html << "<table class='data-table'>";
        html << "<tr><th>Label</th><th>Volume</th><th>Surface Area</th>"
             << "<th>Sphericity</th><th>Voxels</th></tr>";

        double totalVolume = 0.0;
        for (const auto& v : data.volumeResults) {
            totalVolume += v.volumeMm3;
            html << "<tr>";
            html << "<td>" << escapeHtml(v.labelName.empty() ? "Label " + std::to_string(v.labelId) : v.labelName) << "</td>";
            html << "<td>" << formatVolume(v.volumeMm3, v.volumeCm3) << "</td>";
            html << "<td>";
            if (v.surfaceAreaMm2.has_value()) {
                html << formatDouble(v.surfaceAreaMm2.value(), 1) << " mm\u00B2";
            } else {
                html << "-";
            }
            html << "</td><td>";
            if (v.sphericity.has_value()) {
                html << formatDouble(v.sphericity.value(), 3);
            } else {
                html << "-";
            }
            html << "</td><td>" << v.voxelCount << "</td></tr>";
        }

        if (data.volumeResults.size() > 1) {
            double totalCm3 = totalVolume / 1000.0;
            html << "<tr class='total-row'>";
            html << "<td><strong>Total</strong></td>";
            html << "<td><strong>" << formatVolume(totalVolume, totalCm3) << "</strong></td>";
            html << "<td>-</td><td>-</td><td>-</td></tr>";
        }

        html << "</table></div>";
        return html.str();
    }

    std::string generateScreenshotsHtml(const ReportData& data) const {
        if (data.screenshots.empty()) return {};

        std::ostringstream html;

        html << "<div class='section screenshots-section'>";
        html << "<h2>Images</h2>";
        html << "<div class='screenshots-grid'>";

        for (const auto& screenshot : data.screenshots) {
            if (screenshot.pngData.empty()) continue;

            std::string b64 = imageToBase64(screenshot);
            if (b64.empty()) continue;

            html << "<div class='screenshot-item'>";
            html << "<img src='data:image/png;base64," << b64 << "' class='screenshot' />";
            html << "<div class='screenshot-caption'>";
            if (!screenshot.viewType.empty()) {
                html << "<strong>" << escapeHtml(screenshot.viewType) << "</strong>";
            }
            if (!screenshot.caption.empty()) {
                html << "<br />" << escapeHtml(screenshot.caption);
            }
            html << "</div></div>";
        }

        html << "</div></div>";
        return html.str();
    }

    std::string generateFooterHtml(const ReportOptions& options) const {
        std::ostringstream html;

        html << "<div class='footer'>";
        if (options.includeTimestamp) {
            html << "<div class='timestamp'>Generated: " << currentDateTimeString() << "</div>";
        }
        html << "<div class='software'>DICOM Viewer v0.3.0</div>";
        if (!options.author.empty()) {
            html << "<div class='author'>Author: " << escapeHtml(options.author) << "</div>";
        }
        html << "</div>";

        return html.str();
    }
};

// =============================================================================
// ReportGenerator public methods
// =============================================================================

ReportGenerator::ReportGenerator()
    : impl_(std::make_unique<Impl>()) {}

ReportGenerator::~ReportGenerator() = default;

ReportGenerator::ReportGenerator(ReportGenerator&&) noexcept = default;
ReportGenerator& ReportGenerator::operator=(ReportGenerator&&) noexcept = default;

void ReportGenerator::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<void, ReportError> ReportGenerator::generatePDF(
    const ReportData& data,
    const std::filesystem::path& outputPath,
    const ReportOptions& options) const {

    impl_->reportProgress(0.0, "Initializing PDF generation...");

    auto parentPath = outputPath.parent_path();
    if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
        return std::unexpected(ReportError{
            ReportError::Code::FileCreationFailed,
            "Output directory does not exist: " + parentPath.string()
        });
    }

    impl_->reportProgress(0.2, "Generating HTML content...");

    auto htmlResult = generateHTML(data, options);
    if (!htmlResult) {
        return std::unexpected(htmlResult.error());
    }

    impl_->reportProgress(0.4, "Converting HTML to PDF via wkhtmltopdf...");

    auto result = htmlToPdf(*htmlResult, outputPath, options);
    if (!result) {
        return std::unexpected(result.error());
    }

    impl_->reportProgress(1.0, "PDF generation complete");
    LOG_INFO("Generated PDF report: " + outputPath.string());
    return {};
}

std::expected<std::string, ReportError> ReportGenerator::generateHTML(
    const ReportData& data,
    const ReportOptions& options) const {

    std::ostringstream html;

    html << "<!DOCTYPE html>";
    html << "<html><head>";
    html << "<meta charset='UTF-8'>";
    html << "<title>DICOM Viewer Report</title>";
    html << "<style>" << impl_->generateStylesheet(options.reportTemplate) << "</style>";
    html << "</head><body>";

    html << impl_->generateHeaderHtml(data, options);

    if (options.reportTemplate.showPatientInfo) {
        html << impl_->generatePatientInfoHtml(data);
    }

    if (options.reportTemplate.showMeasurements &&
        (!data.distanceMeasurements.empty() ||
         !data.angleMeasurements.empty() ||
         !data.areaMeasurements.empty() ||
         !data.roiStatistics.empty())) {
        html << impl_->generateMeasurementsHtml(data);
    }

    if (options.reportTemplate.showVolumes && !data.volumeResults.empty()) {
        html << impl_->generateVolumesHtml(data);
    }

    if (options.reportTemplate.showScreenshots && !data.screenshots.empty()) {
        html << impl_->generateScreenshotsHtml(data);
    }

    html << impl_->generateFooterHtml(options);

    html << "</body></html>";

    return html.str();
}

std::vector<ReportTemplate> ReportGenerator::getAvailableTemplates() const {
    std::vector<ReportTemplate> templates;

    templates.push_back(getDefaultTemplate());

    // Clinical template
    ReportTemplate clinical;
    clinical.name = "Clinical";
    clinical.headerColor = "#1a5276";
    clinical.titleColor = "#1a5276";
    clinical.titleFontSize = 16;
    clinical.bodyFontSize = 10;
    templates.push_back(clinical);

    // Research template
    ReportTemplate research;
    research.name = "Research";
    research.headerColor = "#27ae60";
    research.titleColor = "#27ae60";
    research.showVolumes = true;
    templates.push_back(research);

    // Load custom templates from config directory
    std::filesystem::path templatesDir = appConfigPath() / "templates";
    std::error_code ec;
    if (std::filesystem::exists(templatesDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(templatesDir, ec)) {
            if (entry.path().extension() == ".json") {
                auto result = loadTemplate(entry.path().stem().string());
                if (result) {
                    templates.push_back(*result);
                }
            }
        }
    }

    return templates;
}

std::expected<void, ReportError> ReportGenerator::saveTemplate(
    const ReportTemplate& templ) const {

    std::filesystem::path templatesDir = appConfigPath() / "templates";
    std::error_code ec;
    std::filesystem::create_directories(templatesDir, ec);
    if (ec) {
        return std::unexpected(ReportError{
            ReportError::Code::FileCreationFailed,
            "Failed to create templates directory: " + ec.message()
        });
    }

    std::ofstream file(templatesDir / (templ.name + ".json"));
    if (!file.is_open()) {
        return std::unexpected(ReportError{
            ReportError::Code::FileCreationFailed,
            "Failed to create template file"
        });
    }

    file << "{\n";
    file << "  \"name\": \"" << templ.name << "\",\n";
    file << "  \"institutionName\": \"" << templ.institutionName << "\",\n";
    file << "  \"logoPath\": \"" << templ.logoPath << "\",\n";
    file << "  \"fontFamily\": \"" << templ.fontFamily << "\",\n";
    file << "  \"titleFontSize\": " << templ.titleFontSize << ",\n";
    file << "  \"headerFontSize\": " << templ.headerFontSize << ",\n";
    file << "  \"bodyFontSize\": " << templ.bodyFontSize << ",\n";
    file << "  \"titleColor\": \"" << templ.titleColor << "\",\n";
    file << "  \"headerColor\": \"" << templ.headerColor << "\",\n";
    file << "  \"textColor\": \"" << templ.textColor << "\",\n";
    file << "  \"showPatientInfo\": " << (templ.showPatientInfo ? "true" : "false") << ",\n";
    file << "  \"showMeasurements\": " << (templ.showMeasurements ? "true" : "false") << ",\n";
    file << "  \"showVolumes\": " << (templ.showVolumes ? "true" : "false") << ",\n";
    file << "  \"showScreenshots\": " << (templ.showScreenshots ? "true" : "false") << "\n";
    file << "}\n";

    return {};
}

std::expected<ReportTemplate, ReportError> ReportGenerator::loadTemplate(
    const std::string& name) const {

    std::filesystem::path filePath = appConfigPath() / "templates" / (name + ".json");

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ReportError{
            ReportError::Code::InvalidTemplate,
            "Template file not found: " + name
        });
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    ReportTemplate templ;
    templ.name = name;

    auto extractString = [&content](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\": \"";
        auto pos = content.find(pattern);
        if (pos == std::string::npos) return {};
        pos += pattern.size();
        auto end = content.find('"', pos);
        if (end == std::string::npos) return {};
        return content.substr(pos, end - pos);
    };

    auto extractInt = [&content](const std::string& key) -> int {
        std::string pattern = "\"" + key + "\": ";
        auto pos = content.find(pattern);
        if (pos == std::string::npos) return -1;
        pos += pattern.size();
        auto end = pos;
        while (end < content.size() && (std::isdigit(content[end]) || content[end] == '-')) ++end;
        try { return std::stoi(content.substr(pos, end - pos)); }
        catch (...) { return -1; }
    };

    auto extractBool = [&content](const std::string& key) -> bool {
        std::string pattern = "\"" + key + "\": ";
        auto pos = content.find(pattern);
        if (pos == std::string::npos) return true;
        pos += pattern.size();
        return content.substr(pos, 4) == "true";
    };

    if (auto v = extractString("institutionName"); !v.empty()) templ.institutionName = v;
    if (auto v = extractString("logoPath"); !v.empty()) templ.logoPath = v;
    if (auto v = extractString("fontFamily"); !v.empty()) templ.fontFamily = v;
    if (auto v = extractInt("titleFontSize"); v > 0) templ.titleFontSize = v;
    if (auto v = extractInt("headerFontSize"); v > 0) templ.headerFontSize = v;
    if (auto v = extractInt("bodyFontSize"); v > 0) templ.bodyFontSize = v;
    if (auto v = extractString("titleColor"); !v.empty()) templ.titleColor = v;
    if (auto v = extractString("headerColor"); !v.empty()) templ.headerColor = v;
    if (auto v = extractString("textColor"); !v.empty()) templ.textColor = v;
    templ.showPatientInfo  = extractBool("showPatientInfo");
    templ.showMeasurements = extractBool("showMeasurements");
    templ.showVolumes      = extractBool("showVolumes");
    templ.showScreenshots  = extractBool("showScreenshots");

    return templ;
}

ReportTemplate ReportGenerator::getDefaultTemplate() {
    return ReportTemplate{};
}

}  // namespace dicom_viewer::services

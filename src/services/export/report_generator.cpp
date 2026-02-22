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

#include <QBuffer>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QStandardPaths>
#include <QTextDocument>
#include <QVBoxLayout>

#include <cmath>
#include <fstream>
#include <sstream>

namespace dicom_viewer::services {

class ReportGenerator::Impl {
public:
    ProgressCallback progressCallback;

    void reportProgress(double progress, const QString& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
    }

    QString formatDistance(double mm) const {
        if (std::abs(mm) >= 10.0) {
            return QString::number(mm, 'f', 2) + " mm";
        }
        return QString::number(mm, 'f', 2) + " mm";
    }

    QString formatAngle(double degrees) const {
        return QString::number(degrees, 'f', 1) + QString::fromUtf8("\u00B0");
    }

    QString formatArea(double mm2, double cm2) const {
        if (cm2 >= 0.01) {
            return QString::number(mm2, 'f', 2) + QString::fromUtf8(" mm\u00B2 (") +
                   QString::number(cm2, 'f', 2) + QString::fromUtf8(" cm\u00B2)");
        }
        return QString::number(mm2, 'f', 2) + QString::fromUtf8(" mm\u00B2");
    }

    QString formatVolume(double mm3, double cm3) const {
        if (cm3 >= 0.001) {
            return QString::number(cm3, 'f', 3) + QString::fromUtf8(" cm\u00B3 (") +
                   QString::number(cm3, 'f', 3) + " mL)";
        }
        return QString::number(mm3, 'f', 2) + QString::fromUtf8(" mm\u00B3");
    }

    QString escapeHtml(const std::string& text) const {
        QString result = QString::fromStdString(text);
        result.replace("&", "&amp;");
        result.replace("<", "&lt;");
        result.replace(">", "&gt;");
        result.replace("\"", "&quot;");
        return result;
    }

    QString imageToBase64(const QImage& image, int dpi) const {
        QImage scaledImage = image;
        if (dpi != 72) {
            double scale = static_cast<double>(dpi) / 72.0;
            scaledImage = image.scaled(
                static_cast<int>(image.width() * scale),
                static_cast<int>(image.height() * scale),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
        }

        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        scaledImage.save(&buffer, "PNG");

        return QString::fromLatin1(byteArray.toBase64());
    }

    QString generateHeaderHtml(const ReportData& data,
                               const ReportOptions& options) const {
        std::ostringstream html;

        html << "<div class='header'>";

        // Logo and institution name
        if (!options.reportTemplate.logoPath.isEmpty()) {
            QImage logo(options.reportTemplate.logoPath);
            if (!logo.isNull()) {
                // Scale logo to reasonable size
                logo = logo.scaledToHeight(60, Qt::SmoothTransformation);
                html << "<img src='data:image/png;base64,"
                     << imageToBase64(logo, 72).toStdString()
                     << "' class='logo' />";
            }
        }

        html << "<div class='title-block'>";
        html << "<h1>DICOM Viewer Report</h1>";
        if (!options.reportTemplate.institutionName.isEmpty()) {
            html << "<div class='institution'>"
                 << escapeHtml(options.reportTemplate.institutionName.toStdString()).toStdString()
                 << "</div>";
        }
        html << "</div>";
        html << "</div>";

        return QString::fromStdString(html.str());
    }

    QString generatePatientInfoHtml(const ReportData& data) const {
        std::ostringstream html;

        html << "<div class='section'>";
        html << "<h2>Patient Information</h2>";
        html << "<table class='info-table'>";
        html << "<tr><td class='label'>Patient Name:</td><td>"
             << escapeHtml(data.patientInfo.name).toStdString() << "</td></tr>";
        html << "<tr><td class='label'>Patient ID:</td><td>"
             << escapeHtml(data.patientInfo.patientId).toStdString() << "</td></tr>";
        html << "<tr><td class='label'>Date of Birth:</td><td>"
             << escapeHtml(data.patientInfo.dateOfBirth).toStdString() << "</td></tr>";
        html << "<tr><td class='label'>Sex:</td><td>"
             << escapeHtml(data.patientInfo.sex).toStdString() << "</td></tr>";
        html << "<tr><td class='label'>Study Date:</td><td>"
             << escapeHtml(data.patientInfo.studyDate).toStdString() << "</td></tr>";
        html << "<tr><td class='label'>Modality:</td><td>"
             << escapeHtml(data.patientInfo.modality).toStdString() << "</td></tr>";
        html << "<tr><td class='label'>Study Description:</td><td>"
             << escapeHtml(data.patientInfo.studyDescription).toStdString() << "</td></tr>";

        if (!data.patientInfo.accessionNumber.empty()) {
            html << "<tr><td class='label'>Accession Number:</td><td>"
                 << escapeHtml(data.patientInfo.accessionNumber).toStdString() << "</td></tr>";
        }
        if (!data.patientInfo.referringPhysician.empty()) {
            html << "<tr><td class='label'>Referring Physician:</td><td>"
                 << escapeHtml(data.patientInfo.referringPhysician).toStdString() << "</td></tr>";
        }

        html << "</table>";
        html << "</div>";

        return QString::fromStdString(html.str());
    }

    QString generateMeasurementsHtml(const ReportData& data) const {
        std::ostringstream html;

        html << "<div class='section'>";
        html << "<h2>Measurements</h2>";

        // Distance measurements
        if (!data.distanceMeasurements.empty()) {
            html << "<h3>Distance Measurements</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>#</th><th>Label</th><th>Distance</th><th>Slice</th></tr>";

            for (size_t i = 0; i < data.distanceMeasurements.size(); ++i) {
                const auto& m = data.distanceMeasurements[i];
                html << "<tr>";
                html << "<td>" << (i + 1) << "</td>";
                html << "<td>" << escapeHtml(m.label.empty() ? "D" + std::to_string(i + 1) : m.label).toStdString() << "</td>";
                html << "<td>" << formatDistance(m.distanceMm).toStdString() << "</td>";
                html << "<td>" << (m.sliceIndex >= 0 ? std::to_string(m.sliceIndex) : "3D") << "</td>";
                html << "</tr>";
            }
            html << "</table>";
        }

        // Angle measurements
        if (!data.angleMeasurements.empty()) {
            html << "<h3>Angle Measurements</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>#</th><th>Label</th><th>Angle</th><th>Type</th></tr>";

            for (size_t i = 0; i < data.angleMeasurements.size(); ++i) {
                const auto& m = data.angleMeasurements[i];
                html << "<tr>";
                html << "<td>" << (i + 1) << "</td>";
                html << "<td>" << escapeHtml(m.label.empty() ? "A" + std::to_string(i + 1) : m.label).toStdString() << "</td>";
                html << "<td>" << formatAngle(m.angleDegrees).toStdString() << "</td>";
                html << "<td>" << (m.isCobbAngle ? "Cobb" : "Standard") << "</td>";
                html << "</tr>";
            }
            html << "</table>";
        }

        // Area measurements
        if (!data.areaMeasurements.empty()) {
            html << "<h3>Area Measurements</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>#</th><th>Label</th><th>Type</th><th>Area</th><th>Perimeter</th></tr>";

            for (size_t i = 0; i < data.areaMeasurements.size(); ++i) {
                const auto& m = data.areaMeasurements[i];
                html << "<tr>";
                html << "<td>" << (i + 1) << "</td>";
                html << "<td>" << escapeHtml(m.label.empty() ? "ROI" + std::to_string(i + 1) : m.label).toStdString() << "</td>";
                html << "<td>";
                switch (m.type) {
                    case RoiType::Ellipse: html << "Ellipse"; break;
                    case RoiType::Rectangle: html << "Rectangle"; break;
                    case RoiType::Polygon: html << "Polygon"; break;
                    case RoiType::Freehand: html << "Freehand"; break;
                }
                html << "</td>";
                html << "<td>" << formatArea(m.areaMm2, m.areaCm2).toStdString() << "</td>";
                html << "<td>" << formatDistance(m.perimeterMm).toStdString() << "</td>";
                html << "</tr>";
            }
            html << "</table>";
        }

        // ROI Statistics
        if (!data.roiStatistics.empty()) {
            html << "<h3>ROI Statistics</h3>";
            html << "<table class='data-table'>";
            html << "<tr><th>ROI</th><th>Mean (HU)</th><th>Std Dev</th>"
                 << "<th>Min</th><th>Max</th><th>Voxels</th></tr>";

            for (const auto& s : data.roiStatistics) {
                html << "<tr>";
                html << "<td>" << escapeHtml(s.roiLabel.empty() ? "ROI" : s.roiLabel).toStdString() << "</td>";
                html << "<td>" << QString::number(s.mean, 'f', 1).toStdString() << "</td>";
                html << "<td>" << QString::number(s.stdDev, 'f', 1).toStdString() << "</td>";
                html << "<td>" << QString::number(s.min, 'f', 0).toStdString() << "</td>";
                html << "<td>" << QString::number(s.max, 'f', 0).toStdString() << "</td>";
                html << "<td>" << s.voxelCount << "</td>";
                html << "</tr>";
            }
            html << "</table>";
        }

        html << "</div>";

        return QString::fromStdString(html.str());
    }

    QString generateVolumesHtml(const ReportData& data) const {
        if (data.volumeResults.empty()) {
            return QString();
        }

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
            html << "<td>" << escapeHtml(v.labelName.empty() ? "Label " + std::to_string(v.labelId) : v.labelName).toStdString() << "</td>";
            html << "<td>" << formatVolume(v.volumeMm3, v.volumeCm3).toStdString() << "</td>";
            html << "<td>";
            if (v.surfaceAreaMm2.has_value()) {
                html << QString::number(v.surfaceAreaMm2.value(), 'f', 1).toStdString()
                     << QString::fromUtf8(" mm\u00B2").toStdString();
            } else {
                html << "-";
            }
            html << "</td>";
            html << "<td>";
            if (v.sphericity.has_value()) {
                html << QString::number(v.sphericity.value(), 'f', 3).toStdString();
            } else {
                html << "-";
            }
            html << "</td>";
            html << "<td>" << v.voxelCount << "</td>";
            html << "</tr>";
        }

        // Total row
        if (data.volumeResults.size() > 1) {
            double totalCm3 = totalVolume / 1000.0;
            html << "<tr class='total-row'>";
            html << "<td><strong>Total</strong></td>";
            html << "<td><strong>" << formatVolume(totalVolume, totalCm3).toStdString() << "</strong></td>";
            html << "<td>-</td><td>-</td><td>-</td>";
            html << "</tr>";
        }

        html << "</table>";
        html << "</div>";

        return QString::fromStdString(html.str());
    }

    QString generateScreenshotsHtml(const ReportData& data, int dpi) const {
        if (data.screenshots.empty()) {
            return QString();
        }

        std::ostringstream html;

        html << "<div class='section screenshots-section'>";
        html << "<h2>Images</h2>";
        html << "<div class='screenshots-grid'>";

        for (const auto& screenshot : data.screenshots) {
            if (screenshot.image.isNull()) {
                continue;
            }

            html << "<div class='screenshot-item'>";
            html << "<img src='data:image/png;base64,"
                 << imageToBase64(screenshot.image, dpi).toStdString()
                 << "' class='screenshot' />";
            html << "<div class='screenshot-caption'>";
            if (!screenshot.viewType.isEmpty()) {
                html << "<strong>" << screenshot.viewType.toStdString() << "</strong>";
            }
            if (!screenshot.caption.isEmpty()) {
                html << "<br />" << escapeHtml(screenshot.caption.toStdString()).toStdString();
            }
            html << "</div>";
            html << "</div>";
        }

        html << "</div>";
        html << "</div>";

        return QString::fromStdString(html.str());
    }

    QString generateFooterHtml(const ReportOptions& options) const {
        std::ostringstream html;

        html << "<div class='footer'>";

        if (options.includeTimestamp) {
            html << "<div class='timestamp'>Generated: "
                 << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString()
                 << "</div>";
        }

        html << "<div class='software'>DICOM Viewer v0.3.0</div>";

        if (!options.author.empty()) {
            html << "<div class='author'>Author: "
                 << escapeHtml(options.author).toStdString()
                 << "</div>";
        }

        html << "</div>";

        return QString::fromStdString(html.str());
    }

    QString generateStylesheet(const ReportTemplate& templ) const {
        std::ostringstream css;

        css << "body { font-family: '" << templ.fontFamily.toStdString()
            << "', sans-serif; font-size: " << templ.bodyFontSize << "pt; "
            << "color: " << templ.textColor.toStdString() << "; margin: 20px; }";

        css << "h1 { font-size: " << templ.titleFontSize << "pt; "
            << "color: " << templ.titleColor.toStdString() << "; margin: 0 0 5px 0; }";

        css << "h2 { font-size: " << templ.headerFontSize << "pt; "
            << "color: " << templ.headerColor.toStdString() << "; "
            << "border-bottom: 2px solid " << templ.headerColor.toStdString() << "; "
            << "padding-bottom: 5px; margin-top: 20px; }";

        css << "h3 { font-size: " << (templ.headerFontSize - 2) << "pt; "
            << "color: " << templ.headerColor.toStdString() << "; margin: 15px 0 8px 0; }";

        css << ".header { display: flex; align-items: center; "
            << "border-bottom: 3px solid " << templ.headerColor.toStdString() << "; "
            << "padding-bottom: 15px; margin-bottom: 20px; }";

        css << ".logo { max-height: 60px; margin-right: 20px; }";

        css << ".title-block { flex: 1; }";

        css << ".institution { font-size: " << (templ.bodyFontSize + 1) << "pt; "
            << "color: #666666; }";

        css << ".section { margin-bottom: 20px; page-break-inside: avoid; }";

        css << ".info-table { border-collapse: collapse; width: 100%; margin-bottom: 15px; }";
        css << ".info-table td { padding: 5px 10px; vertical-align: top; }";
        css << ".info-table .label { font-weight: bold; width: 180px; "
            << "background-color: " << templ.tableHeaderBackground.toStdString() << "; }";

        css << ".data-table { border-collapse: collapse; width: 100%; margin-bottom: 15px; }";
        css << ".data-table th, .data-table td { padding: 8px; text-align: left; "
            << "border: 1px solid " << templ.tableBorderColor.toStdString() << "; }";
        css << ".data-table th { background-color: " << templ.tableHeaderBackground.toStdString()
            << "; font-weight: bold; }";
        css << ".data-table tr:nth-child(even) { background-color: #f9f9f9; }";
        css << ".total-row { background-color: " << templ.tableHeaderBackground.toStdString()
            << " !important; }";

        css << ".screenshots-section { page-break-before: auto; }";
        css << ".screenshots-grid { display: flex; flex-wrap: wrap; gap: 15px; }";
        css << ".screenshot-item { flex: 0 0 calc(50% - 10px); max-width: calc(50% - 10px); "
            << "page-break-inside: avoid; }";
        css << ".screenshot { max-width: 100%; height: auto; "
            << "border: 1px solid " << templ.tableBorderColor.toStdString() << "; }";
        css << ".screenshot-caption { font-size: " << (templ.bodyFontSize - 1) << "pt; "
            << "text-align: center; margin-top: 5px; color: #666666; }";

        css << ".footer { margin-top: 30px; padding-top: 15px; "
            << "border-top: 1px solid " << templ.tableBorderColor.toStdString() << "; "
            << "font-size: " << (templ.bodyFontSize - 1) << "pt; color: #666666; }";
        css << ".footer div { margin-bottom: 3px; }";

        return QString::fromStdString(css.str());
    }
};

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

    // Validate output path
    auto parentPath = outputPath.parent_path();
    if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
        return std::unexpected(ReportError{
            ReportError::Code::FileCreationFailed,
            "Output directory does not exist: " + parentPath.string()
        });
    }

    impl_->reportProgress(0.1, "Generating HTML content...");

    // Generate HTML
    auto htmlResult = generateHTML(data, options);
    if (!htmlResult) {
        return std::unexpected(htmlResult.error());
    }

    impl_->reportProgress(0.4, "Creating PDF document...");

    // Create PDF writer
    QPdfWriter pdfWriter(QString::fromStdString(outputPath.string()));

    // Configure page settings
    QPageLayout layout;
    layout.setPageSize(options.reportTemplate.pageSize);
    layout.setOrientation(options.reportTemplate.orientation);
    layout.setMargins(QMarginsF(20, 20, 20, 20));
    pdfWriter.setPageLayout(layout);

    pdfWriter.setResolution(options.imageDPI);

    impl_->reportProgress(0.5, "Rendering document...");

    // Use QTextDocument for HTML rendering
    QTextDocument document;
    document.setDefaultFont(QFont(options.reportTemplate.fontFamily,
                                  options.reportTemplate.bodyFontSize));
    document.setHtml(*htmlResult);

    // Set page size for proper text wrapping
    QPageSize pageSize = options.reportTemplate.pageSize;
    QSizeF pageSizeMm = pageSize.size(QPageSize::Millimeter);
    double pageWidthPx = (pageSizeMm.width() - 40) * options.imageDPI / 25.4;  // Subtract margins
    document.setPageSize(QSizeF(pageWidthPx, document.size().height()));

    impl_->reportProgress(0.7, "Writing PDF pages...");

    // Paint to PDF
    QPainter painter(&pdfWriter);
    if (!painter.isActive()) {
        return std::unexpected(ReportError{
            ReportError::Code::FileCreationFailed,
            "Failed to initialize PDF painter"
        });
    }

    document.drawContents(&painter);
    painter.end();

    impl_->reportProgress(1.0, "PDF generation complete");

    return {};
}

std::expected<QString, ReportError> ReportGenerator::generateHTML(
    const ReportData& data,
    const ReportOptions& options) const {

    std::ostringstream html;

    // HTML header
    html << "<!DOCTYPE html>";
    html << "<html>";
    html << "<head>";
    html << "<meta charset='UTF-8'>";
    html << "<title>DICOM Viewer Report</title>";
    html << "<style>" << impl_->generateStylesheet(options.reportTemplate).toStdString() << "</style>";
    html << "</head>";
    html << "<body>";

    // Report header
    html << impl_->generateHeaderHtml(data, options).toStdString();

    // Patient information
    if (options.reportTemplate.showPatientInfo) {
        html << impl_->generatePatientInfoHtml(data).toStdString();
    }

    // Measurements
    if (options.reportTemplate.showMeasurements &&
        (!data.distanceMeasurements.empty() ||
         !data.angleMeasurements.empty() ||
         !data.areaMeasurements.empty() ||
         !data.roiStatistics.empty())) {
        html << impl_->generateMeasurementsHtml(data).toStdString();
    }

    // Volume analysis
    if (options.reportTemplate.showVolumes && !data.volumeResults.empty()) {
        html << impl_->generateVolumesHtml(data).toStdString();
    }

    // Screenshots
    if (options.reportTemplate.showScreenshots && !data.screenshots.empty()) {
        html << impl_->generateScreenshotsHtml(data, options.imageDPI).toStdString();
    }

    // Footer
    html << impl_->generateFooterHtml(options).toStdString();

    html << "</body>";
    html << "</html>";

    return QString::fromStdString(html.str());
}

void ReportGenerator::showPreview(const ReportData& data, QWidget* parent,
                                  const ReportOptions& options) {
    auto htmlResult = generateHTML(data, options);
    if (!htmlResult) {
        return;
    }

    // Create preview dialog
    QDialog dialog(parent);
    dialog.setWindowTitle("Report Preview");
    dialog.resize(800, 600);

    auto layout = new QVBoxLayout(&dialog);

    // Use QTextDocument for rendering
    QTextDocument document;
    document.setDefaultFont(QFont(options.reportTemplate.fontFamily,
                                  options.reportTemplate.bodyFontSize));
    document.setHtml(*htmlResult);

    // Create print preview dialog
    QPrintPreviewDialog preview(parent);
    preview.setWindowTitle("Report Preview");

    QObject::connect(&preview, &QPrintPreviewDialog::paintRequested,
                     [&document](QPrinter* printer) {
                         document.print(printer);
                     });

    preview.exec();
}

std::vector<ReportTemplate> ReportGenerator::getAvailableTemplates() const {
    std::vector<ReportTemplate> templates;

    // Default template
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
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir templatesDir(configPath + "/templates");
    if (templatesDir.exists()) {
        QStringList filters;
        filters << "*.json";
        QFileInfoList files = templatesDir.entryInfoList(filters);
        for (const auto& fileInfo : files) {
            auto result = loadTemplate(fileInfo.baseName());
            if (result) {
                templates.push_back(*result);
            }
        }
    }

    return templates;
}

std::expected<void, ReportError> ReportGenerator::saveTemplate(
    const ReportTemplate& templ) const {

    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir templatesDir(configPath + "/templates");

    if (!templatesDir.exists()) {
        if (!QDir().mkpath(templatesDir.path())) {
            return std::unexpected(ReportError{
                ReportError::Code::FileCreationFailed,
                "Failed to create templates directory"
            });
        }
    }

    QString filePath = templatesDir.filePath(templ.name + ".json");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return std::unexpected(ReportError{
            ReportError::Code::FileCreationFailed,
            "Failed to create template file"
        });
    }

    // Write JSON manually for simplicity
    std::ostringstream json;
    json << "{\n";
    json << "  \"name\": \"" << templ.name.toStdString() << "\",\n";
    json << "  \"institutionName\": \"" << templ.institutionName.toStdString() << "\",\n";
    json << "  \"logoPath\": \"" << templ.logoPath.toStdString() << "\",\n";
    json << "  \"fontFamily\": \"" << templ.fontFamily.toStdString() << "\",\n";
    json << "  \"titleFontSize\": " << templ.titleFontSize << ",\n";
    json << "  \"headerFontSize\": " << templ.headerFontSize << ",\n";
    json << "  \"bodyFontSize\": " << templ.bodyFontSize << ",\n";
    json << "  \"titleColor\": \"" << templ.titleColor.toStdString() << "\",\n";
    json << "  \"headerColor\": \"" << templ.headerColor.toStdString() << "\",\n";
    json << "  \"textColor\": \"" << templ.textColor.toStdString() << "\",\n";
    json << "  \"showPatientInfo\": " << (templ.showPatientInfo ? "true" : "false") << ",\n";
    json << "  \"showMeasurements\": " << (templ.showMeasurements ? "true" : "false") << ",\n";
    json << "  \"showVolumes\": " << (templ.showVolumes ? "true" : "false") << ",\n";
    json << "  \"showScreenshots\": " << (templ.showScreenshots ? "true" : "false") << "\n";
    json << "}\n";

    file.write(QString::fromStdString(json.str()).toUtf8());
    file.close();

    return {};
}

std::expected<ReportTemplate, ReportError> ReportGenerator::loadTemplate(
    const QString& name) const {

    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString filePath = configPath + "/templates/" + name + ".json";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::unexpected(ReportError{
            ReportError::Code::InvalidTemplate,
            "Template file not found: " + name.toStdString()
        });
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Simple JSON parsing
    ReportTemplate templ;
    templ.name = name;

    // Extract values using simple string operations
    auto extractString = [&content](const QString& key) -> QString {
        QString pattern = "\"" + key + "\": \"";
        int start = content.indexOf(pattern);
        if (start < 0) return QString();
        start += pattern.length();
        int end = content.indexOf("\"", start);
        if (end < 0) return QString();
        return content.mid(start, end - start);
    };

    auto extractInt = [&content](const QString& key) -> int {
        QString pattern = "\"" + key + "\": ";
        int start = content.indexOf(pattern);
        if (start < 0) return -1;
        start += pattern.length();
        int end = start;
        while (end < content.length() && (content[end].isDigit() || content[end] == '-')) {
            ++end;
        }
        return content.mid(start, end - start).toInt();
    };

    auto extractBool = [&content](const QString& key) -> bool {
        QString pattern = "\"" + key + "\": ";
        int start = content.indexOf(pattern);
        if (start < 0) return true;
        start += pattern.length();
        return content.mid(start, 4) == "true";
    };

    templ.institutionName = extractString("institutionName");
    templ.logoPath = extractString("logoPath");
    templ.fontFamily = extractString("fontFamily");
    if (templ.fontFamily.isEmpty()) templ.fontFamily = "Arial";

    int titleSize = extractInt("titleFontSize");
    if (titleSize > 0) templ.titleFontSize = titleSize;

    int headerSize = extractInt("headerFontSize");
    if (headerSize > 0) templ.headerFontSize = headerSize;

    int bodySize = extractInt("bodyFontSize");
    if (bodySize > 0) templ.bodyFontSize = bodySize;

    QString titleColor = extractString("titleColor");
    if (!titleColor.isEmpty()) templ.titleColor = titleColor;

    QString headerColor = extractString("headerColor");
    if (!headerColor.isEmpty()) templ.headerColor = headerColor;

    QString textColor = extractString("textColor");
    if (!textColor.isEmpty()) templ.textColor = textColor;

    templ.showPatientInfo = extractBool("showPatientInfo");
    templ.showMeasurements = extractBool("showMeasurements");
    templ.showVolumes = extractBool("showVolumes");
    templ.showScreenshots = extractBool("showScreenshots");

    return templ;
}

ReportTemplate ReportGenerator::getDefaultTemplate() {
    return ReportTemplate{};
}

}  // namespace dicom_viewer::services

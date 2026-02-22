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

#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include <fstream>
#include <sstream>

namespace dicom_viewer::services {

namespace {

/**
 * @brief Escape a string for CSV output
 *
 * If the value contains delimiter, quotes, or newlines, wrap in quotes
 * and escape internal quotes by doubling them.
 */
QString escapeCSV(const QString& value, char delimiter) {
    bool needsQuotes = value.contains(delimiter) ||
                       value.contains('"') ||
                       value.contains('\n') ||
                       value.contains('\r');

    if (!needsQuotes) {
        return value;
    }

    QString escaped = value;
    escaped.replace("\"", "\"\"");
    return "\"" + escaped + "\"";
}

QString escapeCSV(const std::string& value, char delimiter) {
    return escapeCSV(QString::fromStdString(value), delimiter);
}

QString roiTypeToString(RoiType type) {
    switch (type) {
        case RoiType::Ellipse: return "Ellipse";
        case RoiType::Rectangle: return "Rectangle";
        case RoiType::Polygon: return "Polygon";
        case RoiType::Freehand: return "Freehand";
    }
    return "Unknown";
}

QString formatDouble(double value, int precision = 2) {
    return QString::number(value, 'f', precision);
}

QString formatPoint3D(const Point3D& point) {
    return QString("(%1,%2,%3)")
        .arg(formatDouble(point[0], 1))
        .arg(formatDouble(point[1], 1))
        .arg(formatDouble(point[2], 1));
}

}  // namespace

// =============================================================================
// DataExporter::Impl
// =============================================================================

class DataExporter::Impl {
public:
    ProgressCallback progressCallback;
    PatientInfo patientInfo;

    void reportProgress(double progress, const QString& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
    }

    std::expected<void, ExportError> writeCSVFile(
        const std::filesystem::path& outputPath,
        const std::vector<QString>& headers,
        const std::vector<std::vector<QString>>& rows,
        const ExportOptions& options) const {

        QFile file(QString::fromStdString(outputPath.string()));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return std::unexpected(ExportError{
                ExportError::Code::FileAccessDenied,
                "Cannot open file: " + outputPath.string()
            });
        }

        // Write UTF-8 BOM directly to file before QTextStream wraps it
        if (options.includeUtf8Bom) {
            file.write("\xEF\xBB\xBF", 3);
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);

        // Write metadata header as comments
        if (options.includeMetadata && !patientInfo.name.empty()) {
            stream << "# Patient: " << QString::fromStdString(patientInfo.name) << "\n";
            stream << "# Patient ID: " << QString::fromStdString(patientInfo.patientId) << "\n";
            stream << "# Study Date: " << QString::fromStdString(patientInfo.studyDate) << "\n";
            stream << "# Modality: " << QString::fromStdString(patientInfo.modality) << "\n";
            stream << "# Export Date: " << QDateTime::currentDateTime().toString(options.dateFormat) << "\n";
            stream << "#\n";
        }

        QString delim(options.csvDelimiter);

        // Write header
        if (options.includeHeader && !headers.empty()) {
            QStringList headerList;
            for (const auto& h : headers) {
                headerList << escapeCSV(h, options.csvDelimiter);
            }
            stream << headerList.join(delim) << "\n";
        }

        // Write data rows
        for (const auto& row : rows) {
            QStringList rowList;
            for (const auto& cell : row) {
                rowList << escapeCSV(cell, options.csvDelimiter);
            }
            stream << rowList.join(delim) << "\n";
        }

        file.close();
        return {};
    }

    std::expected<void, ExportError> writeExcelFile(
        const std::filesystem::path& outputPath,
        const ReportData& data,
        const ExportOptions& options) const {

        // Since we don't have QXlsx or libxlsxwriter as a dependency,
        // we'll create a simple XML-based Excel format (SpreadsheetML)
        // that can be opened by Excel

        QFile file(QString::fromStdString(outputPath.string()));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return std::unexpected(ExportError{
                ExportError::Code::FileAccessDenied,
                "Cannot open file: " + outputPath.string()
            });
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);

        // Write XML spreadsheet format
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        stream << "<?mso-application progid=\"Excel.Sheet\"?>\n";
        stream << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n";
        stream << "  xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n";

        // Summary sheet
        stream << "  <Worksheet ss:Name=\"Summary\">\n";
        stream << "    <Table>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Patient Name</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(patientInfo.name) << "</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Patient ID</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(patientInfo.patientId) << "</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Study Date</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(patientInfo.studyDate) << "</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Modality</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(patientInfo.modality) << "</Data></Cell></Row>\n";
        stream << "      <Row></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Total Measurements</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Distances</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"Number\">" << data.distanceMeasurements.size() << "</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Angles</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"Number\">" << data.angleMeasurements.size() << "</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Areas</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"Number\">" << data.areaMeasurements.size() << "</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Volumes</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"Number\">" << data.volumeResults.size() << "</Data></Cell></Row>\n";
        stream << "    </Table>\n";
        stream << "  </Worksheet>\n";

        // Distances sheet
        stream << "  <Worksheet ss:Name=\"Distances\">\n";
        stream << "    <Table>\n";
        auto distHeaders = DataExporter::getDistanceCSVHeader();
        stream << "      <Row>";
        for (const auto& h : distHeaders) {
            stream << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        stream << "</Row>\n";
        for (const auto& m : data.distanceMeasurements) {
            stream << "      <Row>";
            stream << "<Cell><Data ss:Type=\"Number\">" << m.id << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(m.label) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[0], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[1], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[2], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[0], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[1], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[2], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.distanceMm, 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << m.sliceIndex << "</Data></Cell>";
            stream << "</Row>\n";
        }
        stream << "    </Table>\n";
        stream << "  </Worksheet>\n";

        // Angles sheet
        stream << "  <Worksheet ss:Name=\"Angles\">\n";
        stream << "    <Table>\n";
        auto angleHeaders = DataExporter::getAngleCSVHeader();
        stream << "      <Row>";
        for (const auto& h : angleHeaders) {
            stream << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        stream << "</Row>\n";
        for (const auto& m : data.angleMeasurements) {
            stream << "      <Row>";
            stream << "<Cell><Data ss:Type=\"Number\">" << m.id << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(m.label) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.vertex[0], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.vertex[1], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.vertex[2], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[0], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[1], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[2], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[0], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[1], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[2], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.angleDegrees, 1) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"String\">" << (m.isCobbAngle ? "Yes" : "No") << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << m.sliceIndex << "</Data></Cell>";
            stream << "</Row>\n";
        }
        stream << "    </Table>\n";
        stream << "  </Worksheet>\n";

        // Areas sheet
        stream << "  <Worksheet ss:Name=\"Areas\">\n";
        stream << "    <Table>\n";
        auto areaHeaders = DataExporter::getAreaCSVHeader();
        stream << "      <Row>";
        for (const auto& h : areaHeaders) {
            stream << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        stream << "</Row>\n";
        for (const auto& m : data.areaMeasurements) {
            stream << "      <Row>";
            stream << "<Cell><Data ss:Type=\"Number\">" << m.id << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(m.label) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"String\">" << roiTypeToString(m.type) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.areaMm2, 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.areaCm2, 4) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.perimeterMm, 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.centroid[0], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.centroid[1], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.centroid[2], 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << m.sliceIndex << "</Data></Cell>";
            stream << "</Row>\n";
        }
        stream << "    </Table>\n";
        stream << "  </Worksheet>\n";

        // ROI Statistics sheet
        if (!data.roiStatistics.empty()) {
            stream << "  <Worksheet ss:Name=\"ROI_Statistics\">\n";
            stream << "    <Table>\n";
            auto statsHeaders = DataExporter::getROIStatisticsCSVHeader();
            stream << "      <Row>";
            for (const auto& h : statsHeaders) {
                stream << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
            }
            stream << "</Row>\n";
            for (const auto& s : data.roiStatistics) {
                stream << "      <Row>";
                stream << "<Cell><Data ss:Type=\"Number\">" << s.roiId << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(s.roiLabel) << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.mean, 2) << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.stdDev, 2) << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.min, 2) << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.max, 2) << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.median, 2) << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"Number\">" << s.voxelCount << "</Data></Cell>";
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.areaMm2, 2) << "</Data></Cell>";
                stream << "</Row>\n";
            }
            stream << "    </Table>\n";
            stream << "  </Worksheet>\n";
        }

        // Volumes sheet
        stream << "  <Worksheet ss:Name=\"Volumes\">\n";
        stream << "    <Table>\n";
        auto volHeaders = DataExporter::getVolumeCSVHeader();
        stream << "      <Row>";
        for (const auto& h : volHeaders) {
            stream << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        stream << "</Row>\n";
        for (const auto& v : data.volumeResults) {
            stream << "      <Row>";
            stream << "<Cell><Data ss:Type=\"Number\">" << static_cast<int>(v.labelId) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"String\">" << QString::fromStdString(v.labelName) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << v.voxelCount << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(v.volumeMm3, 2) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(v.volumeCm3, 4) << "</Data></Cell>";
            stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(v.volumeML, 4) << "</Data></Cell>";
            if (v.surfaceAreaMm2.has_value()) {
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(*v.surfaceAreaMm2, 2) << "</Data></Cell>";
            } else {
                stream << "<Cell><Data ss:Type=\"String\"></Data></Cell>";
            }
            if (v.sphericity.has_value()) {
                stream << "<Cell><Data ss:Type=\"Number\">" << formatDouble(*v.sphericity, 3) << "</Data></Cell>";
            } else {
                stream << "<Cell><Data ss:Type=\"String\"></Data></Cell>";
            }
            stream << "</Row>\n";
        }
        stream << "    </Table>\n";
        stream << "  </Worksheet>\n";

        // Metadata sheet
        stream << "  <Worksheet ss:Name=\"Metadata\">\n";
        stream << "    <Table>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Export Date</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"String\">" << QDateTime::currentDateTime().toString(options.dateFormat) << "</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Software</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"String\">DICOM Viewer v0.3.0</Data></Cell></Row>\n";
        stream << "      <Row><Cell><Data ss:Type=\"String\">Format</Data></Cell>";
        stream << "<Cell><Data ss:Type=\"String\">SpreadsheetML</Data></Cell></Row>\n";
        stream << "    </Table>\n";
        stream << "  </Worksheet>\n";

        stream << "</Workbook>\n";

        file.close();
        return {};
    }
};

// =============================================================================
// DataExporter public methods
// =============================================================================

DataExporter::DataExporter() : impl_(std::make_unique<Impl>()) {}

DataExporter::~DataExporter() = default;

DataExporter::DataExporter(DataExporter&&) noexcept = default;
DataExporter& DataExporter::operator=(DataExporter&&) noexcept = default;

void DataExporter::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

void DataExporter::setPatientInfo(const PatientInfo& info) {
    impl_->patientInfo = info;
}

// =============================================================================
// CSV Export Methods
// =============================================================================

std::expected<void, ExportError> DataExporter::exportDistancesToCSV(
    const std::vector<DistanceMeasurement>& measurements,
    const std::filesystem::path& outputPath,
    const ExportOptions& options) const {

    impl_->reportProgress(0.0, "Exporting distances to CSV...");

    std::vector<std::vector<QString>> rows;
    rows.reserve(measurements.size());

    for (const auto& m : measurements) {
        std::vector<QString> row;
        row.push_back(QString::number(m.id));
        row.push_back(QString::fromStdString(m.label));
        row.push_back(formatDouble(m.point1[0], 2));
        row.push_back(formatDouble(m.point1[1], 2));
        row.push_back(formatDouble(m.point1[2], 2));
        row.push_back(formatDouble(m.point2[0], 2));
        row.push_back(formatDouble(m.point2[1], 2));
        row.push_back(formatDouble(m.point2[2], 2));
        row.push_back(formatDouble(m.distanceMm, 2));
        row.push_back(QString::number(m.sliceIndex));
        rows.push_back(std::move(row));
    }

    auto result = impl_->writeCSVFile(outputPath, getDistanceCSVHeader(), rows, options);
    impl_->reportProgress(1.0, "Export complete");
    return result;
}

std::expected<void, ExportError> DataExporter::exportAnglesToCSV(
    const std::vector<AngleMeasurement>& measurements,
    const std::filesystem::path& outputPath,
    const ExportOptions& options) const {

    impl_->reportProgress(0.0, "Exporting angles to CSV...");

    std::vector<std::vector<QString>> rows;
    rows.reserve(measurements.size());

    for (const auto& m : measurements) {
        std::vector<QString> row;
        row.push_back(QString::number(m.id));
        row.push_back(QString::fromStdString(m.label));
        row.push_back(formatDouble(m.vertex[0], 2));
        row.push_back(formatDouble(m.vertex[1], 2));
        row.push_back(formatDouble(m.vertex[2], 2));
        row.push_back(formatDouble(m.point1[0], 2));
        row.push_back(formatDouble(m.point1[1], 2));
        row.push_back(formatDouble(m.point1[2], 2));
        row.push_back(formatDouble(m.point2[0], 2));
        row.push_back(formatDouble(m.point2[1], 2));
        row.push_back(formatDouble(m.point2[2], 2));
        row.push_back(formatDouble(m.angleDegrees, 1));
        row.push_back(m.isCobbAngle ? "Yes" : "No");
        row.push_back(QString::number(m.sliceIndex));
        rows.push_back(std::move(row));
    }

    auto result = impl_->writeCSVFile(outputPath, getAngleCSVHeader(), rows, options);
    impl_->reportProgress(1.0, "Export complete");
    return result;
}

std::expected<void, ExportError> DataExporter::exportAreasToCSV(
    const std::vector<AreaMeasurement>& measurements,
    const std::filesystem::path& outputPath,
    const ExportOptions& options) const {

    impl_->reportProgress(0.0, "Exporting areas to CSV...");

    std::vector<std::vector<QString>> rows;
    rows.reserve(measurements.size());

    for (const auto& m : measurements) {
        std::vector<QString> row;
        row.push_back(QString::number(m.id));
        row.push_back(QString::fromStdString(m.label));
        row.push_back(roiTypeToString(m.type));
        row.push_back(formatDouble(m.areaMm2, 2));
        row.push_back(formatDouble(m.areaCm2, 4));
        row.push_back(formatDouble(m.perimeterMm, 2));
        row.push_back(formatDouble(m.centroid[0], 2));
        row.push_back(formatDouble(m.centroid[1], 2));
        row.push_back(formatDouble(m.centroid[2], 2));
        row.push_back(QString::number(m.sliceIndex));
        rows.push_back(std::move(row));
    }

    auto result = impl_->writeCSVFile(outputPath, getAreaCSVHeader(), rows, options);
    impl_->reportProgress(1.0, "Export complete");
    return result;
}

std::expected<void, ExportError> DataExporter::exportROIStatisticsToCSV(
    const std::vector<RoiStatistics>& stats,
    const std::filesystem::path& outputPath,
    const ExportOptions& options) const {

    impl_->reportProgress(0.0, "Exporting ROI statistics to CSV...");

    std::vector<std::vector<QString>> rows;
    rows.reserve(stats.size());

    for (const auto& s : stats) {
        std::vector<QString> row;
        row.push_back(QString::number(s.roiId));
        row.push_back(QString::fromStdString(s.roiLabel));
        row.push_back(formatDouble(s.mean, 2));
        row.push_back(formatDouble(s.stdDev, 2));
        row.push_back(formatDouble(s.min, 2));
        row.push_back(formatDouble(s.max, 2));
        row.push_back(formatDouble(s.median, 2));
        row.push_back(QString::number(s.voxelCount));
        row.push_back(formatDouble(s.areaMm2, 2));
        rows.push_back(std::move(row));
    }

    auto result = impl_->writeCSVFile(outputPath, getROIStatisticsCSVHeader(), rows, options);
    impl_->reportProgress(1.0, "Export complete");
    return result;
}

std::expected<void, ExportError> DataExporter::exportVolumesToCSV(
    const std::vector<VolumeResult>& volumes,
    const std::filesystem::path& outputPath,
    const ExportOptions& options) const {

    impl_->reportProgress(0.0, "Exporting volumes to CSV...");

    std::vector<std::vector<QString>> rows;
    rows.reserve(volumes.size());

    for (const auto& v : volumes) {
        std::vector<QString> row;
        row.push_back(QString::number(v.labelId));
        row.push_back(QString::fromStdString(v.labelName));
        row.push_back(QString::number(v.voxelCount));
        row.push_back(formatDouble(v.volumeMm3, 2));
        row.push_back(formatDouble(v.volumeCm3, 4));
        row.push_back(formatDouble(v.volumeML, 4));
        row.push_back(v.surfaceAreaMm2.has_value() ? formatDouble(*v.surfaceAreaMm2, 2) : "");
        row.push_back(v.sphericity.has_value() ? formatDouble(*v.sphericity, 3) : "");
        rows.push_back(std::move(row));
    }

    auto result = impl_->writeCSVFile(outputPath, getVolumeCSVHeader(), rows, options);
    impl_->reportProgress(1.0, "Export complete");
    return result;
}

std::expected<void, ExportError> DataExporter::exportAllToCSV(
    const ReportData& data,
    const std::filesystem::path& outputPath,
    const ExportOptions& options) const {

    impl_->reportProgress(0.0, "Exporting all measurements to CSV...");

    QFile file(QString::fromStdString(outputPath.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Cannot open file: " + outputPath.string()
        });
    }

    // Write UTF-8 BOM directly to file before QTextStream wraps it
    if (options.includeUtf8Bom) {
        file.write("\xEF\xBB\xBF", 3);
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QString delim(options.csvDelimiter);

    // Metadata header
    if (options.includeMetadata) {
        stream << "# DICOM Viewer - Measurement Export\n";
        stream << "# Patient: " << QString::fromStdString(data.patientInfo.name) << "\n";
        stream << "# Patient ID: " << QString::fromStdString(data.patientInfo.patientId) << "\n";
        stream << "# Study Date: " << QString::fromStdString(data.patientInfo.studyDate) << "\n";
        stream << "# Export Date: " << QDateTime::currentDateTime().toString(options.dateFormat) << "\n";
        stream << "#\n";
    }

    // Distance measurements section
    if (!data.distanceMeasurements.empty()) {
        stream << "# DISTANCE MEASUREMENTS\n";
        if (options.includeHeader) {
            QStringList headers;
            for (const auto& h : getDistanceCSVHeader()) {
                headers << escapeCSV(h, options.csvDelimiter);
            }
            stream << headers.join(delim) << "\n";
        }
        for (const auto& m : data.distanceMeasurements) {
            QStringList row;
            row << QString::number(m.id);
            row << escapeCSV(QString::fromStdString(m.label), options.csvDelimiter);
            row << formatDouble(m.point1[0], 2);
            row << formatDouble(m.point1[1], 2);
            row << formatDouble(m.point1[2], 2);
            row << formatDouble(m.point2[0], 2);
            row << formatDouble(m.point2[1], 2);
            row << formatDouble(m.point2[2], 2);
            row << formatDouble(m.distanceMm, 2);
            row << QString::number(m.sliceIndex);
            stream << row.join(delim) << "\n";
        }
        stream << "\n";
    }

    impl_->reportProgress(0.25, "Exported distances...");

    // Angle measurements section
    if (!data.angleMeasurements.empty()) {
        stream << "# ANGLE MEASUREMENTS\n";
        if (options.includeHeader) {
            QStringList headers;
            for (const auto& h : getAngleCSVHeader()) {
                headers << escapeCSV(h, options.csvDelimiter);
            }
            stream << headers.join(delim) << "\n";
        }
        for (const auto& m : data.angleMeasurements) {
            QStringList row;
            row << QString::number(m.id);
            row << escapeCSV(QString::fromStdString(m.label), options.csvDelimiter);
            row << formatDouble(m.vertex[0], 2);
            row << formatDouble(m.vertex[1], 2);
            row << formatDouble(m.vertex[2], 2);
            row << formatDouble(m.point1[0], 2);
            row << formatDouble(m.point1[1], 2);
            row << formatDouble(m.point1[2], 2);
            row << formatDouble(m.point2[0], 2);
            row << formatDouble(m.point2[1], 2);
            row << formatDouble(m.point2[2], 2);
            row << formatDouble(m.angleDegrees, 1);
            row << (m.isCobbAngle ? "Yes" : "No");
            row << QString::number(m.sliceIndex);
            stream << row.join(delim) << "\n";
        }
        stream << "\n";
    }

    impl_->reportProgress(0.5, "Exported angles...");

    // Area measurements section
    if (!data.areaMeasurements.empty()) {
        stream << "# AREA MEASUREMENTS\n";
        if (options.includeHeader) {
            QStringList headers;
            for (const auto& h : getAreaCSVHeader()) {
                headers << escapeCSV(h, options.csvDelimiter);
            }
            stream << headers.join(delim) << "\n";
        }
        for (const auto& m : data.areaMeasurements) {
            QStringList row;
            row << QString::number(m.id);
            row << escapeCSV(QString::fromStdString(m.label), options.csvDelimiter);
            row << roiTypeToString(m.type);
            row << formatDouble(m.areaMm2, 2);
            row << formatDouble(m.areaCm2, 4);
            row << formatDouble(m.perimeterMm, 2);
            row << formatDouble(m.centroid[0], 2);
            row << formatDouble(m.centroid[1], 2);
            row << formatDouble(m.centroid[2], 2);
            row << QString::number(m.sliceIndex);
            stream << row.join(delim) << "\n";
        }
        stream << "\n";
    }

    impl_->reportProgress(0.75, "Exported areas...");

    // Volume results section
    if (!data.volumeResults.empty()) {
        stream << "# VOLUME MEASUREMENTS\n";
        if (options.includeHeader) {
            QStringList headers;
            for (const auto& h : getVolumeCSVHeader()) {
                headers << escapeCSV(h, options.csvDelimiter);
            }
            stream << headers.join(delim) << "\n";
        }
        for (const auto& v : data.volumeResults) {
            QStringList row;
            row << QString::number(v.labelId);
            row << escapeCSV(QString::fromStdString(v.labelName), options.csvDelimiter);
            row << QString::number(v.voxelCount);
            row << formatDouble(v.volumeMm3, 2);
            row << formatDouble(v.volumeCm3, 4);
            row << formatDouble(v.volumeML, 4);
            row << (v.surfaceAreaMm2.has_value() ? formatDouble(*v.surfaceAreaMm2, 2) : "");
            row << (v.sphericity.has_value() ? formatDouble(*v.sphericity, 3) : "");
            stream << row.join(delim) << "\n";
        }
    }

    file.close();
    impl_->reportProgress(1.0, "Export complete");
    return {};
}

// =============================================================================
// Excel Export Methods
// =============================================================================

std::expected<void, ExportError> DataExporter::exportToExcel(
    const ReportData& data,
    const std::filesystem::path& outputPath,
    const ExportOptions& options) const {

    impl_->reportProgress(0.0, "Exporting to Excel...");
    impl_->patientInfo = data.patientInfo;
    auto result = impl_->writeExcelFile(outputPath, data, options);
    impl_->reportProgress(1.0, "Export complete");
    return result;
}

// =============================================================================
// Utility Methods
// =============================================================================

std::vector<QString> DataExporter::getDistanceCSVHeader() {
    return {
        "ID", "Label",
        "Point1_X", "Point1_Y", "Point1_Z",
        "Point2_X", "Point2_Y", "Point2_Z",
        "Distance_mm", "SliceIndex"
    };
}

std::vector<QString> DataExporter::getAngleCSVHeader() {
    return {
        "ID", "Label",
        "Vertex_X", "Vertex_Y", "Vertex_Z",
        "Point1_X", "Point1_Y", "Point1_Z",
        "Point2_X", "Point2_Y", "Point2_Z",
        "Angle_degrees", "IsCobbAngle", "SliceIndex"
    };
}

std::vector<QString> DataExporter::getAreaCSVHeader() {
    return {
        "ID", "Label", "Type",
        "Area_mm2", "Area_cm2", "Perimeter_mm",
        "Centroid_X", "Centroid_Y", "Centroid_Z",
        "SliceIndex"
    };
}

std::vector<QString> DataExporter::getROIStatisticsCSVHeader() {
    return {
        "ROI_ID", "Label",
        "Mean_HU", "StdDev_HU", "Min_HU", "Max_HU", "Median_HU",
        "VoxelCount", "Area_mm2"
    };
}

std::vector<QString> DataExporter::getVolumeCSVHeader() {
    return {
        "LabelID", "LabelName",
        "VoxelCount", "Volume_mm3", "Volume_cm3", "Volume_mL",
        "SurfaceArea_mm2", "Sphericity"
    };
}

}  // namespace dicom_viewer::services

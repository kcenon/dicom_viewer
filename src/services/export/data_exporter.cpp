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

#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dicom_viewer::services {

namespace {

/**
 * @brief Escape a string for CSV output
 *
 * If the value contains delimiter, quotes, or newlines, wrap in quotes
 * and escape internal quotes by doubling them.
 */
std::string escapeCSV(const std::string& value, char delimiter) {
    bool needsQuotes = value.contains(static_cast<char>(delimiter)) ||
                       value.contains('"') ||
                       value.contains('\n') ||
                       value.contains('\r');

    if (!needsQuotes) {
        return value;
    }

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped += '"';
    for (char c : value) {
        if (c == '"') escaped += '"';
        escaped += c;
    }
    escaped += '"';
    return escaped;
}

std::string roiTypeToString(RoiType type) {
    switch (type) {
        case RoiType::Ellipse: return "Ellipse";
        case RoiType::Rectangle: return "Rectangle";
        case RoiType::Polygon: return "Polygon";
        case RoiType::Freehand: return "Freehand";
    }
    return "Unknown";
}

std::string formatDouble(double value, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string currentDateTimeString(const std::string& format) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, format.c_str());
    return oss.str();
}

}  // namespace

// =============================================================================
// DataExporter::Impl
// =============================================================================

class DataExporter::Impl {
public:
    ProgressCallback progressCallback;
    PatientInfo patientInfo;

    void reportProgress(double progress, const std::string& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
    }

    std::expected<void, ExportError> writeCSVFile(
        const std::filesystem::path& outputPath,
        const std::vector<std::string>& headers,
        const std::vector<std::vector<std::string>>& rows,
        const ExportOptions& options) const {

        std::ofstream file(outputPath, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(ExportError{
                ExportError::Code::FileAccessDenied,
                "Cannot open file: " + outputPath.string()
            });
        }

        if (options.includeUtf8Bom) {
            file.write("\xEF\xBB\xBF", 3);
        }

        const char delim = options.csvDelimiter;

        // Write metadata header as comments
        if (options.includeMetadata && !patientInfo.name.empty()) {
            file << "# Patient: " << patientInfo.name << "\n";
            file << "# Patient ID: " << patientInfo.patientId << "\n";
            file << "# Study Date: " << patientInfo.studyDate << "\n";
            file << "# Modality: " << patientInfo.modality << "\n";
            file << "# Export Date: " << currentDateTimeString(options.dateFormat) << "\n";
            file << "#\n";
        }

        // Write header
        if (options.includeHeader && !headers.empty()) {
            for (size_t i = 0; i < headers.size(); ++i) {
                if (i > 0) file << delim;
                file << escapeCSV(headers[i], delim);
            }
            file << "\n";
        }

        // Write data rows
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) file << delim;
                file << escapeCSV(row[i], delim);
            }
            file << "\n";
        }

        return {};
    }

    std::expected<void, ExportError> writeExcelFile(
        const std::filesystem::path& outputPath,
        const ReportData& data,
        const ExportOptions& options) const {

        std::ofstream file(outputPath, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(ExportError{
                ExportError::Code::FileAccessDenied,
                "Cannot open file: " + outputPath.string()
            });
        }

        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<?mso-application progid=\"Excel.Sheet\"?>\n";
        file << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n";
        file << "  xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n";

        // Summary sheet
        file << "  <Worksheet ss:Name=\"Summary\">\n";
        file << "    <Table>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Patient Name</Data></Cell>";
        file << "<Cell><Data ss:Type=\"String\">" << patientInfo.name << "</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Patient ID</Data></Cell>";
        file << "<Cell><Data ss:Type=\"String\">" << patientInfo.patientId << "</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Study Date</Data></Cell>";
        file << "<Cell><Data ss:Type=\"String\">" << patientInfo.studyDate << "</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Modality</Data></Cell>";
        file << "<Cell><Data ss:Type=\"String\">" << patientInfo.modality << "</Data></Cell></Row>\n";
        file << "      <Row></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Total Measurements</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Distances</Data></Cell>";
        file << "<Cell><Data ss:Type=\"Number\">" << data.distanceMeasurements.size() << "</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Angles</Data></Cell>";
        file << "<Cell><Data ss:Type=\"Number\">" << data.angleMeasurements.size() << "</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Areas</Data></Cell>";
        file << "<Cell><Data ss:Type=\"Number\">" << data.areaMeasurements.size() << "</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Volumes</Data></Cell>";
        file << "<Cell><Data ss:Type=\"Number\">" << data.volumeResults.size() << "</Data></Cell></Row>\n";
        file << "    </Table>\n";
        file << "  </Worksheet>\n";

        // Distances sheet
        file << "  <Worksheet ss:Name=\"Distances\">\n";
        file << "    <Table>\n";
        auto distHeaders = DataExporter::getDistanceCSVHeader();
        file << "      <Row>";
        for (const auto& h : distHeaders) {
            file << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        file << "</Row>\n";
        for (const auto& m : data.distanceMeasurements) {
            file << "      <Row>";
            file << "<Cell><Data ss:Type=\"Number\">" << m.id << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"String\">" << m.label << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[0], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[1], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[2], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[0], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[1], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[2], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.distanceMm, 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << m.sliceIndex << "</Data></Cell>";
            file << "</Row>\n";
        }
        file << "    </Table>\n";
        file << "  </Worksheet>\n";

        // Angles sheet
        file << "  <Worksheet ss:Name=\"Angles\">\n";
        file << "    <Table>\n";
        auto angleHeaders = DataExporter::getAngleCSVHeader();
        file << "      <Row>";
        for (const auto& h : angleHeaders) {
            file << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        file << "</Row>\n";
        for (const auto& m : data.angleMeasurements) {
            file << "      <Row>";
            file << "<Cell><Data ss:Type=\"Number\">" << m.id << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"String\">" << m.label << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.vertex[0], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.vertex[1], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.vertex[2], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[0], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[1], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point1[2], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[0], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[1], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.point2[2], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.angleDegrees, 1) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"String\">" << (m.isCobbAngle ? "Yes" : "No") << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << m.sliceIndex << "</Data></Cell>";
            file << "</Row>\n";
        }
        file << "    </Table>\n";
        file << "  </Worksheet>\n";

        // Areas sheet
        file << "  <Worksheet ss:Name=\"Areas\">\n";
        file << "    <Table>\n";
        auto areaHeaders = DataExporter::getAreaCSVHeader();
        file << "      <Row>";
        for (const auto& h : areaHeaders) {
            file << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        file << "</Row>\n";
        for (const auto& m : data.areaMeasurements) {
            file << "      <Row>";
            file << "<Cell><Data ss:Type=\"Number\">" << m.id << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"String\">" << m.label << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"String\">" << roiTypeToString(m.type) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.areaMm2, 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.areaCm2, 4) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.perimeterMm, 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.centroid[0], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.centroid[1], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(m.centroid[2], 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << m.sliceIndex << "</Data></Cell>";
            file << "</Row>\n";
        }
        file << "    </Table>\n";
        file << "  </Worksheet>\n";

        // ROI Statistics sheet
        if (!data.roiStatistics.empty()) {
            file << "  <Worksheet ss:Name=\"ROI_Statistics\">\n";
            file << "    <Table>\n";
            auto statsHeaders = DataExporter::getROIStatisticsCSVHeader();
            file << "      <Row>";
            for (const auto& h : statsHeaders) {
                file << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
            }
            file << "</Row>\n";
            for (const auto& s : data.roiStatistics) {
                file << "      <Row>";
                file << "<Cell><Data ss:Type=\"Number\">" << s.roiId << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"String\">" << s.roiLabel << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.mean, 2) << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.stdDev, 2) << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.min, 2) << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.max, 2) << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.median, 2) << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"Number\">" << s.voxelCount << "</Data></Cell>";
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(s.areaMm2, 2) << "</Data></Cell>";
                file << "</Row>\n";
            }
            file << "    </Table>\n";
            file << "  </Worksheet>\n";
        }

        // Volumes sheet
        file << "  <Worksheet ss:Name=\"Volumes\">\n";
        file << "    <Table>\n";
        auto volHeaders = DataExporter::getVolumeCSVHeader();
        file << "      <Row>";
        for (const auto& h : volHeaders) {
            file << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
        }
        file << "</Row>\n";
        for (const auto& v : data.volumeResults) {
            file << "      <Row>";
            file << "<Cell><Data ss:Type=\"Number\">" << static_cast<int>(v.labelId) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"String\">" << v.labelName << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << v.voxelCount << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(v.volumeMm3, 2) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(v.volumeCm3, 4) << "</Data></Cell>";
            file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(v.volumeML, 4) << "</Data></Cell>";
            if (v.surfaceAreaMm2.has_value()) {
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(*v.surfaceAreaMm2, 2) << "</Data></Cell>";
            } else {
                file << "<Cell><Data ss:Type=\"String\"></Data></Cell>";
            }
            if (v.sphericity.has_value()) {
                file << "<Cell><Data ss:Type=\"Number\">" << formatDouble(*v.sphericity, 3) << "</Data></Cell>";
            } else {
                file << "<Cell><Data ss:Type=\"String\"></Data></Cell>";
            }
            file << "</Row>\n";
        }
        file << "    </Table>\n";
        file << "  </Worksheet>\n";

        // Metadata sheet
        file << "  <Worksheet ss:Name=\"Metadata\">\n";
        file << "    <Table>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Export Date</Data></Cell>";
        file << "<Cell><Data ss:Type=\"String\">" << currentDateTimeString(options.dateFormat) << "</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Software</Data></Cell>";
        file << "<Cell><Data ss:Type=\"String\">DICOM Viewer v0.3.0</Data></Cell></Row>\n";
        file << "      <Row><Cell><Data ss:Type=\"String\">Format</Data></Cell>";
        file << "<Cell><Data ss:Type=\"String\">SpreadsheetML</Data></Cell></Row>\n";
        file << "    </Table>\n";
        file << "  </Worksheet>\n";

        file << "</Workbook>\n";

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

    std::vector<std::vector<std::string>> rows;
    rows.reserve(measurements.size());

    for (const auto& m : measurements) {
        std::vector<std::string> row;
        row.push_back(std::to_string(m.id));
        row.push_back(m.label);
        row.push_back(formatDouble(m.point1[0], 2));
        row.push_back(formatDouble(m.point1[1], 2));
        row.push_back(formatDouble(m.point1[2], 2));
        row.push_back(formatDouble(m.point2[0], 2));
        row.push_back(formatDouble(m.point2[1], 2));
        row.push_back(formatDouble(m.point2[2], 2));
        row.push_back(formatDouble(m.distanceMm, 2));
        row.push_back(std::to_string(m.sliceIndex));
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

    std::vector<std::vector<std::string>> rows;
    rows.reserve(measurements.size());

    for (const auto& m : measurements) {
        std::vector<std::string> row;
        row.push_back(std::to_string(m.id));
        row.push_back(m.label);
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
        row.push_back(std::to_string(m.sliceIndex));
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

    std::vector<std::vector<std::string>> rows;
    rows.reserve(measurements.size());

    for (const auto& m : measurements) {
        std::vector<std::string> row;
        row.push_back(std::to_string(m.id));
        row.push_back(m.label);
        row.push_back(roiTypeToString(m.type));
        row.push_back(formatDouble(m.areaMm2, 2));
        row.push_back(formatDouble(m.areaCm2, 4));
        row.push_back(formatDouble(m.perimeterMm, 2));
        row.push_back(formatDouble(m.centroid[0], 2));
        row.push_back(formatDouble(m.centroid[1], 2));
        row.push_back(formatDouble(m.centroid[2], 2));
        row.push_back(std::to_string(m.sliceIndex));
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

    std::vector<std::vector<std::string>> rows;
    rows.reserve(stats.size());

    for (const auto& s : stats) {
        std::vector<std::string> row;
        row.push_back(std::to_string(s.roiId));
        row.push_back(s.roiLabel);
        row.push_back(formatDouble(s.mean, 2));
        row.push_back(formatDouble(s.stdDev, 2));
        row.push_back(formatDouble(s.min, 2));
        row.push_back(formatDouble(s.max, 2));
        row.push_back(formatDouble(s.median, 2));
        row.push_back(std::to_string(s.voxelCount));
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

    std::vector<std::vector<std::string>> rows;
    rows.reserve(volumes.size());

    for (const auto& v : volumes) {
        std::vector<std::string> row;
        row.push_back(std::to_string(v.labelId));
        row.push_back(v.labelName);
        row.push_back(std::to_string(v.voxelCount));
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

    std::ofstream file(outputPath, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Cannot open file: " + outputPath.string()
        });
    }

    if (options.includeUtf8Bom) {
        file.write("\xEF\xBB\xBF", 3);
    }

    const char delim = options.csvDelimiter;

    // Metadata header
    if (options.includeMetadata) {
        file << "# DICOM Viewer - Measurement Export\n";
        file << "# Patient: " << data.patientInfo.name << "\n";
        file << "# Patient ID: " << data.patientInfo.patientId << "\n";
        file << "# Study Date: " << data.patientInfo.studyDate << "\n";
        file << "# Export Date: " << currentDateTimeString(options.dateFormat) << "\n";
        file << "#\n";
    }

    auto writeRow = [&](const std::vector<std::string>& cells) {
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i > 0) file << delim;
            file << escapeCSV(cells[i], delim);
        }
        file << "\n";
    };

    auto writeHeaders = [&](const std::vector<std::string>& headers) {
        if (options.includeHeader) {
            writeRow(headers);
        }
    };

    // Distance measurements section
    if (!data.distanceMeasurements.empty()) {
        file << "# DISTANCE MEASUREMENTS\n";
        writeHeaders(getDistanceCSVHeader());
        for (const auto& m : data.distanceMeasurements) {
            writeRow({
                std::to_string(m.id), m.label,
                formatDouble(m.point1[0], 2), formatDouble(m.point1[1], 2), formatDouble(m.point1[2], 2),
                formatDouble(m.point2[0], 2), formatDouble(m.point2[1], 2), formatDouble(m.point2[2], 2),
                formatDouble(m.distanceMm, 2), std::to_string(m.sliceIndex)
            });
        }
        file << "\n";
    }

    impl_->reportProgress(0.25, "Exported distances...");

    // Angle measurements section
    if (!data.angleMeasurements.empty()) {
        file << "# ANGLE MEASUREMENTS\n";
        writeHeaders(getAngleCSVHeader());
        for (const auto& m : data.angleMeasurements) {
            writeRow({
                std::to_string(m.id), m.label,
                formatDouble(m.vertex[0], 2), formatDouble(m.vertex[1], 2), formatDouble(m.vertex[2], 2),
                formatDouble(m.point1[0], 2), formatDouble(m.point1[1], 2), formatDouble(m.point1[2], 2),
                formatDouble(m.point2[0], 2), formatDouble(m.point2[1], 2), formatDouble(m.point2[2], 2),
                formatDouble(m.angleDegrees, 1), m.isCobbAngle ? "Yes" : "No",
                std::to_string(m.sliceIndex)
            });
        }
        file << "\n";
    }

    impl_->reportProgress(0.5, "Exported angles...");

    // Area measurements section
    if (!data.areaMeasurements.empty()) {
        file << "# AREA MEASUREMENTS\n";
        writeHeaders(getAreaCSVHeader());
        for (const auto& m : data.areaMeasurements) {
            writeRow({
                std::to_string(m.id), m.label, roiTypeToString(m.type),
                formatDouble(m.areaMm2, 2), formatDouble(m.areaCm2, 4), formatDouble(m.perimeterMm, 2),
                formatDouble(m.centroid[0], 2), formatDouble(m.centroid[1], 2), formatDouble(m.centroid[2], 2),
                std::to_string(m.sliceIndex)
            });
        }
        file << "\n";
    }

    impl_->reportProgress(0.75, "Exported areas...");

    // Volume results section
    if (!data.volumeResults.empty()) {
        file << "# VOLUME MEASUREMENTS\n";
        writeHeaders(getVolumeCSVHeader());
        for (const auto& v : data.volumeResults) {
            writeRow({
                std::to_string(v.labelId), v.labelName, std::to_string(v.voxelCount),
                formatDouble(v.volumeMm3, 2), formatDouble(v.volumeCm3, 4), formatDouble(v.volumeML, 4),
                v.surfaceAreaMm2.has_value() ? formatDouble(*v.surfaceAreaMm2, 2) : "",
                v.sphericity.has_value() ? formatDouble(*v.sphericity, 3) : ""
            });
        }
    }

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

std::vector<std::string> DataExporter::getDistanceCSVHeader() {
    return {
        "ID", "Label",
        "Point1_X", "Point1_Y", "Point1_Z",
        "Point2_X", "Point2_Y", "Point2_Z",
        "Distance_mm", "SliceIndex"
    };
}

std::vector<std::string> DataExporter::getAngleCSVHeader() {
    return {
        "ID", "Label",
        "Vertex_X", "Vertex_Y", "Vertex_Z",
        "Point1_X", "Point1_Y", "Point1_Z",
        "Point2_X", "Point2_Y", "Point2_Z",
        "Angle_degrees", "IsCobbAngle", "SliceIndex"
    };
}

std::vector<std::string> DataExporter::getAreaCSVHeader() {
    return {
        "ID", "Label", "Type",
        "Area_mm2", "Area_cm2", "Perimeter_mm",
        "Centroid_X", "Centroid_Y", "Centroid_Z",
        "SliceIndex"
    };
}

std::vector<std::string> DataExporter::getROIStatisticsCSVHeader() {
    return {
        "ROI_ID", "Label",
        "Mean_HU", "StdDev_HU", "Min_HU", "Max_HU", "Median_HU",
        "VoxelCount", "Area_mm2"
    };
}

std::vector<std::string> DataExporter::getVolumeCSVHeader() {
    return {
        "LabelID", "LabelName",
        "VoxelCount", "Volume_mm3", "Volume_cm3", "Volume_mL",
        "SurfaceArea_mm2", "Sphericity"
    };
}

}  // namespace dicom_viewer::services

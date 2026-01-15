#pragma once

#include "services/export/report_generator.hpp"
#include "services/measurement/measurement_types.hpp"
#include "services/measurement/roi_statistics.hpp"
#include "services/measurement/volume_calculator.hpp"

#include <QString>

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Error information for data export operations
 *
 * @trace SRS-FR-046
 */
struct ExportError {
    enum class Code {
        Success,
        FileAccessDenied,
        InvalidData,
        EncodingFailed,
        UnsupportedFormat,
        InternalError
    };

    Code code = Code::Success;
    std::string message;

    [[nodiscard]] bool isSuccess() const noexcept {
        return code == Code::Success;
    }

    [[nodiscard]] std::string toString() const {
        switch (code) {
            case Code::Success: return "Success";
            case Code::FileAccessDenied: return "File access denied: " + message;
            case Code::InvalidData: return "Invalid data: " + message;
            case Code::EncodingFailed: return "Encoding failed: " + message;
            case Code::UnsupportedFormat: return "Unsupported format: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Options for data export operations
 *
 * @trace SRS-FR-046
 */
struct ExportOptions {
    /// Include header row in CSV output
    bool includeHeader = true;

    /// Include patient/study metadata as comment header
    bool includeMetadata = true;

    /// Include timestamp column
    bool includeTimestamp = true;

    /// CSV delimiter character
    char csvDelimiter = ',';

    /// Date format string (Qt format)
    QString dateFormat = "yyyy-MM-ddTHH:mm:ss";

    /// Selected columns (empty = all columns)
    std::vector<QString> selectedColumns;

    /// Include UTF-8 BOM for Excel compatibility
    bool includeUtf8Bom = true;
};

/**
 * @brief Exporter for measurement data to CSV and Excel formats
 *
 * Exports measurement data, ROI statistics, and volume calculations
 * to CSV and Excel formats for external analysis.
 *
 * @example
 * @code
 * DataExporter exporter;
 *
 * ExportOptions options;
 * options.includeHeader = true;
 * options.csvDelimiter = ',';
 *
 * auto result = exporter.exportDistancesToCSV(
 *     measurements, "/path/to/distances.csv", options);
 * if (result) {
 *     // Success
 * }
 *
 * // Export all data to Excel
 * auto excelResult = exporter.exportToExcel(reportData, "/path/to/report.xlsx");
 * @endcode
 *
 * @trace SRS-FR-046
 */
class DataExporter {
public:
    using ProgressCallback = std::function<void(double progress, const QString& status)>;

    DataExporter();
    ~DataExporter();

    // Non-copyable, movable
    DataExporter(const DataExporter&) = delete;
    DataExporter& operator=(const DataExporter&) = delete;
    DataExporter(DataExporter&&) noexcept;
    DataExporter& operator=(DataExporter&&) noexcept;

    /**
     * @brief Set progress callback
     * @param callback Function called with progress (0.0-1.0) and status message
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set patient info for metadata header
     * @param info Patient information
     */
    void setPatientInfo(const PatientInfo& info);

    // =========================================================================
    // CSV Export Methods
    // =========================================================================

    /**
     * @brief Export distance measurements to CSV
     *
     * @param measurements Vector of distance measurements
     * @param outputPath Output file path
     * @param options Export options
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ExportError> exportDistancesToCSV(
        const std::vector<DistanceMeasurement>& measurements,
        const std::filesystem::path& outputPath,
        const ExportOptions& options = {}) const;

    /**
     * @brief Export angle measurements to CSV
     *
     * @param measurements Vector of angle measurements
     * @param outputPath Output file path
     * @param options Export options
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ExportError> exportAnglesToCSV(
        const std::vector<AngleMeasurement>& measurements,
        const std::filesystem::path& outputPath,
        const ExportOptions& options = {}) const;

    /**
     * @brief Export area measurements to CSV
     *
     * @param measurements Vector of area measurements
     * @param outputPath Output file path
     * @param options Export options
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ExportError> exportAreasToCSV(
        const std::vector<AreaMeasurement>& measurements,
        const std::filesystem::path& outputPath,
        const ExportOptions& options = {}) const;

    /**
     * @brief Export ROI statistics to CSV
     *
     * @param stats Vector of ROI statistics
     * @param outputPath Output file path
     * @param options Export options
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ExportError> exportROIStatisticsToCSV(
        const std::vector<RoiStatistics>& stats,
        const std::filesystem::path& outputPath,
        const ExportOptions& options = {}) const;

    /**
     * @brief Export volume results to CSV
     *
     * @param volumes Vector of volume results
     * @param outputPath Output file path
     * @param options Export options
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ExportError> exportVolumesToCSV(
        const std::vector<VolumeResult>& volumes,
        const std::filesystem::path& outputPath,
        const ExportOptions& options = {}) const;

    /**
     * @brief Export all measurements to a single CSV file
     *
     * @param data Report data containing all measurements
     * @param outputPath Output file path
     * @param options Export options
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ExportError> exportAllToCSV(
        const ReportData& data,
        const std::filesystem::path& outputPath,
        const ExportOptions& options = {}) const;

    // =========================================================================
    // Excel Export Methods
    // =========================================================================

    /**
     * @brief Export all data to Excel workbook
     *
     * Creates an Excel workbook with multiple sheets:
     * - Summary: Patient info and totals
     * - Distances: Distance measurements
     * - Angles: Angle measurements
     * - Areas: Area measurements with ROI statistics
     * - Volumes: Volume calculations
     * - Metadata: Export settings and software info
     *
     * @param data Report data containing all measurements
     * @param outputPath Output file path (.xlsx)
     * @param options Export options
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ExportError> exportToExcel(
        const ReportData& data,
        const std::filesystem::path& outputPath,
        const ExportOptions& options = {}) const;

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Get CSV header for distance measurements
     * @return Vector of column names
     */
    [[nodiscard]] static std::vector<QString> getDistanceCSVHeader();

    /**
     * @brief Get CSV header for angle measurements
     * @return Vector of column names
     */
    [[nodiscard]] static std::vector<QString> getAngleCSVHeader();

    /**
     * @brief Get CSV header for area measurements
     * @return Vector of column names
     */
    [[nodiscard]] static std::vector<QString> getAreaCSVHeader();

    /**
     * @brief Get CSV header for ROI statistics
     * @return Vector of column names
     */
    [[nodiscard]] static std::vector<QString> getROIStatisticsCSVHeader();

    /**
     * @brief Get CSV header for volume results
     * @return Vector of column names
     */
    [[nodiscard]] static std::vector<QString> getVolumeCSVHeader();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

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


/**
 * @file report_generator.hpp
 * @brief Clinical report PDF generation with measurement and image embedding
 * @details Generates multi-page PDF reports embedding measurement tables,
 *          ROI statistics, volume calculations, and captured images.
 *          Uses wkhtmltopdf for PDF rendering; no Qt dependency.
 *
 * ## Thread Safety
 * - PDF rendering is a blocking operation; use background threads for UI
 * - Image capture requires synchronized access to render windows
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include "services/measurement/measurement_types.hpp"
#include "services/measurement/roi_statistics.hpp"
#include "services/measurement/volume_calculator.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Error information for report generation operations
 *
 * @trace SRS-FR-045
 */
struct ReportError {
    enum class Code {
        Success,
        InvalidData,
        FileCreationFailed,
        RenderingFailed,
        InvalidTemplate,
        ImageProcessingFailed,
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
            case Code::InvalidData: return "Invalid data: " + message;
            case Code::FileCreationFailed: return "File creation failed: " + message;
            case Code::RenderingFailed: return "Rendering failed: " + message;
            case Code::InvalidTemplate: return "Invalid template: " + message;
            case Code::ImageProcessingFailed: return "Image processing failed: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Page orientation for report layout
 */
enum class PageOrientation {
    Portrait,
    Landscape
};

/**
 * @brief Standard page size presets
 */
enum class PageSizePreset {
    A4,
    Letter,
    Legal
};

/**
 * @brief Report template configuration
 *
 * Defines the appearance and content settings for generated reports.
 *
 * @trace SRS-FR-045
 */
struct ReportTemplate {
    std::string name = "Default";
    std::string logoPath;
    std::string institutionName;

    // Section visibility
    bool showPatientInfo = true;
    bool showMeasurements = true;
    bool showVolumes = true;
    bool showScreenshots = true;

    // Formatting
    std::string fontFamily = "Arial";
    int titleFontSize = 18;
    int headerFontSize = 14;
    int bodyFontSize = 11;
    PageSizePreset pageSize = PageSizePreset::A4;
    PageOrientation orientation = PageOrientation::Portrait;

    // Colors (RGB hex strings)
    std::string titleColor = "#333333";
    std::string headerColor = "#2a5db0";
    std::string textColor = "#333333";
    std::string tableHeaderBackground = "#e8e8e8";
    std::string tableBorderColor = "#cccccc";
};

/**
 * @brief Patient demographics for the report
 *
 * @trace SRS-FR-045
 */
struct PatientInfo {
    std::string name;
    std::string patientId;
    std::string dateOfBirth;
    std::string sex;
    std::string studyDate;
    std::string studyDescription;
    std::string modality;
    std::string accessionNumber;
    std::string referringPhysician;
};

/**
 * @brief Screenshot data for report embedding
 *
 * Stores PNG-encoded image bytes for embedding in HTML/PDF output.
 * Use width/height to preserve original dimensions for layout.
 *
 * @trace SRS-FR-045
 */
struct ReportScreenshot {
    std::vector<uint8_t> pngData;  ///< PNG-encoded image bytes
    int width = 0;                 ///< Image width in pixels
    int height = 0;                ///< Image height in pixels
    std::string caption;
    std::string viewType;  ///< "Axial", "Sagittal", "Coronal", "Volume", etc.
};

/**
 * @brief Complete data package for report generation
 *
 * @trace SRS-FR-045
 */
struct ReportData {
    PatientInfo patientInfo;

    // Measurements
    std::vector<DistanceMeasurement> distanceMeasurements;
    std::vector<AngleMeasurement> angleMeasurements;
    std::vector<AreaMeasurement> areaMeasurements;

    // ROI Statistics
    std::vector<RoiStatistics> roiStatistics;

    // Volume measurements
    std::vector<VolumeResult> volumeResults;

    // Screenshots
    std::vector<ReportScreenshot> screenshots;
};

/**
 * @brief Options for report generation
 *
 * @trace SRS-FR-045
 */
struct ReportOptions {
    ReportTemplate reportTemplate;
    bool includeTimestamp = true;
    std::string author;
    int imageDPI = 300;
};

/**
 * @brief Generator for PDF medical imaging reports
 *
 * Creates professional PDF reports containing patient information,
 * measurements, screenshots, and volume calculations following
 * medical imaging documentation standards.
 *
 * PDF output is produced via wkhtmltopdf (must be available in PATH).
 *
 * @example
 * @code
 * ReportGenerator generator;
 *
 * ReportData data;
 * data.patientInfo.name = "John Doe";
 * data.patientInfo.patientId = "12345";
 * data.distanceMeasurements = measurements;
 * data.volumeResults = volumes;
 *
 * ReportOptions options;
 * options.author = "Dr. Smith";
 * options.reportTemplate.institutionName = "City Hospital";
 *
 * auto result = generator.generatePDF(data, "/path/to/report.pdf", options);
 * if (result) {
 *     // Success
 * }
 * @endcode
 *
 * @trace SRS-FR-045
 */
class ReportGenerator {
public:
    using ProgressCallback = std::function<void(double progress, const std::string& status)>;

    ReportGenerator();
    ~ReportGenerator();

    // Non-copyable, movable
    ReportGenerator(const ReportGenerator&) = delete;
    ReportGenerator& operator=(const ReportGenerator&) = delete;
    ReportGenerator(ReportGenerator&&) noexcept;
    ReportGenerator& operator=(ReportGenerator&&) noexcept;

    /**
     * @brief Set progress callback
     * @param callback Function called with progress (0.0-1.0) and status message
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Generate PDF report
     *
     * Creates a PDF document with all report sections based on the provided
     * data and options. Internally generates HTML then converts via wkhtmltopdf.
     *
     * @param data Report data containing patient info, measurements, etc.
     * @param outputPath Output file path for the PDF
     * @param options Generation options including template settings
     * @return Success or error information
     */
    [[nodiscard]] std::expected<void, ReportError> generatePDF(
        const ReportData& data,
        const std::filesystem::path& outputPath,
        const ReportOptions& options = {}) const;

    /**
     * @brief Generate HTML report string
     *
     * Creates a standalone HTML document suitable for browser preview
     * or conversion to PDF via wkhtmltopdf.
     *
     * @param data Report data
     * @param options Generation options
     * @return HTML string or error
     */
    [[nodiscard]] std::expected<std::string, ReportError> generateHTML(
        const ReportData& data,
        const ReportOptions& options = {}) const;

    /**
     * @brief Get available report templates
     * @return Vector of templates
     */
    [[nodiscard]] std::vector<ReportTemplate> getAvailableTemplates() const;

    /**
     * @brief Save custom template
     * @param templ Template to save
     * @return Success or error
     */
    [[nodiscard]] std::expected<void, ReportError>
    saveTemplate(const ReportTemplate& templ) const;

    /**
     * @brief Load template by name
     * @param name Template name
     * @return Template or error
     */
    [[nodiscard]] std::expected<ReportTemplate, ReportError>
    loadTemplate(const std::string& name) const;

    /**
     * @brief Get default template
     * @return Default report template
     */
    [[nodiscard]] static ReportTemplate getDefaultTemplate();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

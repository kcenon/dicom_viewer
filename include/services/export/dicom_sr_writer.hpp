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

#pragma once

#include "services/dicom_echo_scu.hpp"
#include "services/measurement/measurement_types.hpp"
#include "services/measurement/volume_calculator.hpp"
#include "services/pacs_config.hpp"

#include <QString>

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Error information for DICOM SR operations
 *
 * @trace SRS-FR-047
 */
struct SRError {
    enum class Code {
        Success,
        InvalidData,
        EncodingFailed,
        FileAccessDenied,
        PacsConnectionFailed,
        PacsStoreFailed,
        ValidationFailed,
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
            case Code::EncodingFailed: return "Encoding failed: " + message;
            case Code::FileAccessDenied: return "File access denied: " + message;
            case Code::PacsConnectionFailed: return "PACS connection failed: " + message;
            case Code::PacsStoreFailed: return "PACS store failed: " + message;
            case Code::ValidationFailed: return "Validation failed: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief DICOM code triplet (value, scheme, meaning)
 */
struct DicomCode {
    std::string value;       ///< Code value (e.g., "122712")
    std::string scheme;      ///< Coding scheme designator (e.g., "DCM", "SCT", "UCUM")
    std::string meaning;     ///< Code meaning (e.g., "Length")

    [[nodiscard]] bool isValid() const noexcept {
        return !value.empty() && !scheme.empty() && !meaning.empty();
    }
};

/**
 * @brief Standard DICOM SR codes for measurement reports
 *
 * Provides commonly used codes from CID tables for TID 1500 reports.
 */
namespace SRCodes {
    // CID 7469 - Measurement Types
    inline const DicomCode Length{"122712", "DCM", "Length"};
    inline const DicomCode Area{"42798000", "SCT", "Area"};
    inline const DicomCode Volume{"118565006", "SCT", "Volume"};
    inline const DicomCode Angle{"1483009", "SCT", "Angle"};
    inline const DicomCode Mean{"373098007", "SCT", "Mean"};
    inline const DicomCode StandardDeviation{"386136009", "SCT", "Standard Deviation"};
    inline const DicomCode Minimum{"255605001", "SCT", "Minimum"};
    inline const DicomCode Maximum{"56851009", "SCT", "Maximum"};

    // CID 7470 - Measurement Units
    inline const DicomCode Millimeter{"mm", "UCUM", "mm"};
    inline const DicomCode SquareMillimeter{"mm2", "UCUM", "mm2"};
    inline const DicomCode CubicMillimeter{"mm3", "UCUM", "mm3"};
    inline const DicomCode CubicCentimeter{"cm3", "UCUM", "cm3"};
    inline const DicomCode Degree{"deg", "UCUM", "deg"};
    inline const DicomCode HounsfieldUnit{"[hnsf'U]", "UCUM", "Hounsfield unit"};

    // CID 6147 - Common Anatomic Regions (subset)
    inline const DicomCode Liver{"10200004", "SCT", "Liver"};
    inline const DicomCode Lung{"39607008", "SCT", "Lung structure"};
    inline const DicomCode Kidney{"64033007", "SCT", "Kidney structure"};
    inline const DicomCode Brain{"12738006", "SCT", "Brain structure"};
    inline const DicomCode Heart{"80891009", "SCT", "Heart structure"};
    inline const DicomCode Spine{"421060004", "SCT", "Spinal column"};
    inline const DicomCode Abdomen{"818983003", "SCT", "Abdomen"};
    inline const DicomCode Chest{"51185008", "SCT", "Thoracic structure"};
    inline const DicomCode Pelvis{"12921003", "SCT", "Pelvis"};

    // Document titles
    inline const DicomCode ImagingMeasurementReport{"126000", "DCM", "Imaging Measurement Report"};
}

/**
 * @brief Patient information for SR document
 */
struct SRPatientInfo {
    std::string patientId;
    std::string patientName;
    std::string patientBirthDate;  ///< Format: YYYYMMDD
    std::string patientSex;        ///< M, F, or O
};

/**
 * @brief Study information for SR document
 */
struct SRStudyInfo {
    std::string studyInstanceUid;
    std::string studyDate;         ///< Format: YYYYMMDD
    std::string studyTime;         ///< Format: HHMMSS
    std::string studyDescription;
    std::string accessionNumber;
    std::string referringPhysicianName;
};

/**
 * @brief Series information for SR document
 */
struct SRSeriesInfo {
    std::string seriesInstanceUid;
    std::string modality;          ///< Original modality (CT, MR, etc.)
    std::string seriesDescription;
};

/**
 * @brief Single measurement entry for SR content
 */
struct SRMeasurement {
    /// Measurement type (distance, area, volume, angle)
    enum class Type {
        Distance,
        Area,
        Volume,
        Angle,
        ROIStatistic
    };

    Type type = Type::Distance;

    /// Measurement value
    double value = 0.0;

    /// Unit code
    DicomCode unit;

    /// Measurement label/name
    std::string label;

    /// Spatial coordinates in world space (mm)
    std::vector<Point3D> coordinates;

    /// Optional anatomic region
    std::optional<DicomCode> findingSite;

    /// Tracking identifier for this measurement
    std::string trackingId;

    /// Optional comment
    std::string comment;

    /// Referenced SOP Instance UID (source image)
    std::string referencedSopInstanceUid;

    /// Referenced frame number (for multi-frame images)
    std::optional<int> referencedFrameNumber;
};

/**
 * @brief ROI statistics for inclusion in SR
 */
struct SRROIStatistics {
    std::string label;
    double mean = 0.0;
    double stdDev = 0.0;
    double min = 0.0;
    double max = 0.0;
    double areaMm2 = 0.0;
    std::optional<DicomCode> findingSite;
    std::string referencedSopInstanceUid;
};

/**
 * @brief Complete content for SR document
 */
struct SRContent {
    /// Patient information
    SRPatientInfo patient;

    /// Study information
    SRStudyInfo study;

    /// Series information
    SRSeriesInfo series;

    /// Distance measurements
    std::vector<DistanceMeasurement> distances;

    /// Angle measurements
    std::vector<AngleMeasurement> angles;

    /// Area measurements
    std::vector<AreaMeasurement> areas;

    /// Volume results
    std::vector<VolumeResult> volumes;

    /// ROI statistics
    std::vector<SRROIStatistics> roiStatistics;

    /// Referenced SOP Instance UIDs (source images)
    std::vector<std::string> referencedSopInstanceUids;

    /// Operator/author name
    std::string operatorName;

    /// Institution name
    std::string institutionName;

    /// Performed date/time
    std::chrono::system_clock::time_point performedDateTime;
};

/**
 * @brief Result of SR validation
 */
struct SRValidationResult {
    bool valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    [[nodiscard]] bool hasErrors() const noexcept {
        return !errors.empty();
    }

    [[nodiscard]] bool hasWarnings() const noexcept {
        return !warnings.empty();
    }
};

/**
 * @brief Result of successful SR creation
 */
struct SRCreationResult {
    /// Generated SOP Instance UID for the SR
    std::string sopInstanceUid;

    /// Generated Series Instance UID for the SR series
    std::string seriesInstanceUid;

    /// Path to saved file (if saved locally)
    std::optional<std::filesystem::path> filePath;

    /// Number of measurements included
    size_t measurementCount = 0;
};

/**
 * @brief Options for SR generation
 */
struct SRWriterOptions {
    /// Include patient information in SR
    bool includePatientInfo = true;

    /// Include study information in SR
    bool includeStudyInfo = true;

    /// Include spatial coordinates (SCOORD3D)
    bool includeSpatialCoordinates = true;

    /// Include ROI statistics if available
    bool includeROIStatistics = true;

    /// Series description for the SR series
    QString seriesDescription = "Measurement Report";

    /// Series number for the SR series
    int seriesNumber = 999;

    /// Instance number
    int instanceNumber = 1;

    /// Manufacturer name
    QString manufacturer = "DICOM Viewer";

    /// Software version
    QString softwareVersion = "0.3.0";
};

/**
 * @brief DICOM Structured Report (SR) Writer
 *
 * Generates DICOM Structured Reports containing measurement results
 * following the TID 1500 (Measurement Report) template. The generated
 * SR documents can be saved to file or stored directly to PACS.
 *
 * Supported content:
 * - Distance measurements with 3D coordinates
 * - Angle measurements
 * - Area measurements with ROI statistics
 * - Volume measurements
 *
 * @example
 * @code
 * DicomSRWriter writer;
 *
 * SRContent content;
 * content.patient = {...};
 * content.study = {...};
 * content.distances = distanceMeasurements;
 * content.volumes = volumeResults;
 *
 * auto result = writer.createSR(content);
 * if (result) {
 *     writer.saveToFile(*result, "/path/to/output.dcm");
 *
 *     // Or store to PACS
 *     PacsServerConfig pacsConfig{...};
 *     writer.storeToPacs(*result, pacsConfig);
 * }
 * @endcode
 *
 * @trace SRS-FR-047
 */
class DicomSRWriter {
public:
    /// Callback for progress updates
    using ProgressCallback = std::function<void(double progress, const QString& status)>;

    // Standard SOP Class UIDs
    static constexpr const char* COMPREHENSIVE_SR_SOP_CLASS = "1.2.840.10008.5.1.4.1.1.88.33";
    static constexpr const char* ENHANCED_SR_SOP_CLASS = "1.2.840.10008.5.1.4.1.1.88.22";

    DicomSRWriter();
    ~DicomSRWriter();

    // Non-copyable, movable
    DicomSRWriter(const DicomSRWriter&) = delete;
    DicomSRWriter& operator=(const DicomSRWriter&) = delete;
    DicomSRWriter(DicomSRWriter&&) noexcept;
    DicomSRWriter& operator=(DicomSRWriter&&) noexcept;

    /**
     * @brief Set progress callback
     * @param callback Function called with progress (0.0-1.0) and status message
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Create DICOM Structured Report from content
     *
     * Generates a DICOM SR document following TID 1500 template with
     * all provided measurements encoded as structured content.
     *
     * @param content SR content including measurements and metadata
     * @param options Generation options
     * @return Creation result on success, error on failure
     */
    [[nodiscard]] std::expected<SRCreationResult, SRError> createSR(
        const SRContent& content,
        const SRWriterOptions& options = {}) const;

    /**
     * @brief Save SR document to file
     *
     * @param content SR content to save
     * @param outputPath Output file path (.dcm)
     * @param options Generation options
     * @return Success or error
     */
    [[nodiscard]] std::expected<SRCreationResult, SRError> saveToFile(
        const SRContent& content,
        const std::filesystem::path& outputPath,
        const SRWriterOptions& options = {}) const;

    /**
     * @brief Store SR document to PACS via C-STORE
     *
     * Creates the SR document and sends it to the specified PACS server
     * using DICOM C-STORE operation.
     *
     * @param content SR content to store
     * @param pacsConfig PACS server configuration
     * @param options Generation options
     * @return Success or error
     */
    [[nodiscard]] std::expected<SRCreationResult, SRError> storeToPacs(
        const SRContent& content,
        const PacsServerConfig& pacsConfig,
        const SRWriterOptions& options = {}) const;

    /**
     * @brief Validate SR content before creation
     *
     * Checks if the provided content is valid for SR generation.
     *
     * @param content SR content to validate
     * @return Validation result with errors and warnings
     */
    [[nodiscard]] SRValidationResult validate(const SRContent& content) const;

    /**
     * @brief Generate a new unique DICOM UID
     * @return New UID string
     */
    [[nodiscard]] static std::string generateUid();

    /**
     * @brief Get supported SOP classes for SR
     * @return List of supported SOP Class UIDs
     */
    [[nodiscard]] static std::vector<std::string> getSupportedSopClasses();

    /**
     * @brief Get available anatomic region codes
     * @return List of common anatomic region codes
     */
    [[nodiscard]] static std::vector<DicomCode> getAnatomicRegionCodes();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

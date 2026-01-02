#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief Error information for volume calculation operations
 *
 * @trace SRS-FR-029
 */
struct VolumeError {
    enum class Code {
        Success,
        InvalidLabelMap,
        InvalidSpacing,
        LabelNotFound,
        MeshGenerationFailed,
        CalculationFailed,
        ExportFailed,
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
            case Code::InvalidLabelMap: return "Invalid label map: " + message;
            case Code::InvalidSpacing: return "Invalid spacing: " + message;
            case Code::LabelNotFound: return "Label not found: " + message;
            case Code::MeshGenerationFailed: return "Mesh generation failed: " + message;
            case Code::CalculationFailed: return "Calculation failed: " + message;
            case Code::ExportFailed: return "Export failed: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Result of volume calculation for a segmentation label
 *
 * Contains volume measurements in multiple units and optional surface area.
 *
 * @trace SRS-FR-029
 */
struct VolumeResult {
    /// Label ID (1-255)
    uint8_t labelId = 0;

    /// Label name for display
    std::string labelName;

    /// Number of voxels in the segmented region
    int64_t voxelCount = 0;

    /// Volume in cubic millimeters
    double volumeMm3 = 0.0;

    /// Volume in cubic centimeters (volumeMm3 / 1000)
    double volumeCm3 = 0.0;

    /// Volume in milliliters (equal to volumeCm3)
    double volumeML = 0.0;

    /// Surface area in square millimeters (optional, requires mesh generation)
    std::optional<double> surfaceAreaMm2;

    /// Sphericity: ratio of surface area of equivalent sphere to actual surface area
    /// (1.0 = perfect sphere, <1.0 = irregular shape)
    std::optional<double> sphericity;

    /// Bounding box dimensions [x, y, z] in mm
    std::array<double, 3> boundingBoxMm = {0.0, 0.0, 0.0};

    /**
     * @brief Convert result to formatted string
     * @return Formatted volume string
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Get header row for CSV export
     * @return Vector of column names
     */
    [[nodiscard]] static std::vector<std::string> getCsvHeader();

    /**
     * @brief Get data row for CSV export
     * @return Vector of values as strings
     */
    [[nodiscard]] std::vector<std::string> getCsvRow() const;
};

/**
 * @brief Volume tracking entry for serial studies
 *
 * Stores volume measurements over time for trend analysis.
 *
 * @trace SRS-FR-029
 */
struct VolumeTimePoint {
    /// Study date (YYYYMMDD format)
    std::string studyDate;

    /// Study description
    std::string studyDescription;

    /// Volume result at this time point
    VolumeResult volume;

    /// Change from previous measurement (if available)
    std::optional<double> changeFromPreviousMm3;

    /// Percentage change from previous measurement
    std::optional<double> changePercentage;
};

/**
 * @brief Comparison table for multiple label volumes
 *
 * @trace SRS-FR-029
 */
struct VolumeComparisonTable {
    /// All volume results
    std::vector<VolumeResult> results;

    /// Total volume of all labels combined
    double totalVolumeMm3 = 0.0;

    /// Percentage contribution of each label
    std::vector<double> percentages;

    /**
     * @brief Generate formatted comparison table string
     * @return Formatted table
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Export comparison table to CSV
     * @param filePath Output file path
     * @return Success or error message
     */
    [[nodiscard]] std::expected<void, std::string>
    exportToCsv(const std::filesystem::path& filePath) const;
};

/**
 * @brief Calculator for 3D volume measurements of segmented regions
 *
 * Provides accurate volume calculation with proper unit conversion,
 * optional surface area measurement using marching cubes mesh generation,
 * and comparison/tracking features for multiple labels and serial studies.
 *
 * @example
 * @code
 * VolumeCalculator calculator;
 *
 * // Calculate single label volume
 * auto result = calculator.calculate(labelMap, labelId, spacing);
 * if (result) {
 *     std::cout << "Volume: " << result->volumeCm3 << " cm^3" << std::endl;
 * }
 *
 * // Calculate all labels with surface area
 * auto allResults = calculator.calculateAll(labelMap, spacing, true);
 * for (const auto& res : allResults) {
 *     if (res) {
 *         std::cout << res->labelName << ": " << res->volumeML << " mL" << std::endl;
 *     }
 * }
 *
 * // Export to CSV
 * VolumeCalculator::exportToCsv(allResults, "/path/to/output.csv");
 * @endcode
 *
 * @trace SRS-FR-029
 */
class VolumeCalculator {
public:
    /// Label map type for segmentation
    using LabelMapType = itk::Image<uint8_t, 3>;

    /// Spacing type [x, y, z] in mm
    using SpacingType = std::array<double, 3>;

    /// Callback for progress updates
    using ProgressCallback = std::function<void(double progress)>;

    VolumeCalculator();
    ~VolumeCalculator();

    // Non-copyable, movable
    VolumeCalculator(const VolumeCalculator&) = delete;
    VolumeCalculator& operator=(const VolumeCalculator&) = delete;
    VolumeCalculator(VolumeCalculator&&) noexcept;
    VolumeCalculator& operator=(VolumeCalculator&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Calculate volume for a single segmentation label
     *
     * @param labelMap Label map containing segmentation
     * @param labelId Label ID to analyze (1-255)
     * @param spacing Voxel spacing [x, y, z] in mm
     * @param computeSurfaceArea If true, compute surface area using mesh (slower)
     * @return Volume result or error
     */
    [[nodiscard]] std::expected<VolumeResult, VolumeError>
    calculate(LabelMapType::Pointer labelMap,
              uint8_t labelId,
              const SpacingType& spacing,
              bool computeSurfaceArea = false);

    /**
     * @brief Calculate volumes for all labels in the label map
     *
     * @param labelMap Label map containing segmentation
     * @param spacing Voxel spacing [x, y, z] in mm
     * @param computeSurfaceArea If true, compute surface area for each label
     * @return Vector of volume results (one per label found)
     */
    [[nodiscard]] std::vector<std::expected<VolumeResult, VolumeError>>
    calculateAll(LabelMapType::Pointer labelMap,
                 const SpacingType& spacing,
                 bool computeSurfaceArea = false);

    /**
     * @brief Create comparison table for multiple labels
     *
     * @param results Vector of successful volume results
     * @return Comparison table with percentages
     */
    [[nodiscard]] static VolumeComparisonTable
    createComparisonTable(const std::vector<VolumeResult>& results);

    /**
     * @brief Calculate volume change between two time points
     *
     * @param current Current volume result
     * @param previous Previous volume result
     * @return Time point with change information
     */
    [[nodiscard]] static VolumeTimePoint
    calculateChange(const VolumeResult& current,
                    const VolumeResult& previous,
                    const std::string& studyDate,
                    const std::string& studyDescription = "");

    /**
     * @brief Export volume results to CSV file
     *
     * @param results Vector of volume results to export
     * @param filePath Output file path
     * @return Success or error
     */
    [[nodiscard]] static std::expected<void, VolumeError>
    exportToCsv(const std::vector<VolumeResult>& results,
                const std::filesystem::path& filePath);

    /**
     * @brief Export volume tracking data to CSV file
     *
     * @param timePoints Vector of volume time points
     * @param filePath Output file path
     * @return Success or error
     */
    [[nodiscard]] static std::expected<void, VolumeError>
    exportTrackingToCsv(const std::vector<VolumeTimePoint>& timePoints,
                        const std::filesystem::path& filePath);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

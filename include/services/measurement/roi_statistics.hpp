#pragma once

#include "measurement_types.hpp"

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
 * @brief Statistics calculated for a Region of Interest (ROI)
 *
 * Contains comprehensive statistical measures for pixel/voxel values
 * within a defined ROI or segmentation region.
 *
 * @trace SRS-FR-028
 */
struct RoiStatistics {
    /// Unique identifier linking to the source ROI
    int roiId = 0;

    /// ROI label/name for display
    std::string roiLabel;

    /// Mean (average) value in the ROI
    double mean = 0.0;

    /// Standard deviation of values
    double stdDev = 0.0;

    /// Minimum value in the ROI
    double min = 0.0;

    /// Maximum value in the ROI
    double max = 0.0;

    /// Median value (50th percentile)
    double median = 0.0;

    /// Number of pixels/voxels in the ROI
    int64_t voxelCount = 0;

    /// Volume in cubic millimeters (for 3D)
    double volumeMm3 = 0.0;

    /// Area in square millimeters (for 2D)
    double areaMm2 = 0.0;

    /// Histogram data (typically 256 bins for HU range)
    std::vector<int64_t> histogram;

    /// Histogram bin edges
    double histogramMin = -1024.0;
    double histogramMax = 3071.0;
    int histogramBins = 256;

    /// Percentile values (5th, 25th, 75th, 95th)
    double percentile5 = 0.0;
    double percentile25 = 0.0;
    double percentile75 = 0.0;
    double percentile95 = 0.0;

    /// Skewness (measure of asymmetry)
    double skewness = 0.0;

    /// Kurtosis (measure of "tailedness")
    double kurtosis = 0.0;

    /// Entropy (measure of randomness/uniformity)
    double entropy = 0.0;

    /**
     * @brief Convert statistics to formatted string
     * @return Formatted statistics string
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Export statistics to CSV format
     * @param filePath Output file path
     * @return Success or error message
     */
    [[nodiscard]] std::expected<void, std::string>
    exportToCsv(const std::filesystem::path& filePath) const;

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
 * @brief Error information for statistics operations
 */
struct StatisticsError {
    enum class Code {
        Success,
        InvalidRoi,
        InvalidImage,
        NoPixelsInRoi,
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
            case Code::InvalidRoi: return "Invalid ROI: " + message;
            case Code::InvalidImage: return "Invalid image: " + message;
            case Code::NoPixelsInRoi: return "No pixels in ROI: " + message;
            case Code::CalculationFailed: return "Calculation failed: " + message;
            case Code::ExportFailed: return "Export failed: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Calculator for ROI statistics
 *
 * Computes comprehensive statistics for regions of interest (ROIs) defined
 * by area measurements or segmentation labels. Supports both 2D and 3D analysis.
 *
 * @example
 * @code
 * RoiStatisticsCalculator calculator;
 * calculator.setImage(ctImage);
 * calculator.setPixelSpacing(0.5, 0.5, 1.0);
 *
 * // Calculate statistics for a 2D ROI
 * auto stats = calculator.calculate(areaMeasurement, sliceIndex);
 * if (stats) {
 *     std::cout << "Mean HU: " << stats->mean << std::endl;
 * }
 *
 * // Calculate statistics for segmentation label
 * auto labelStats = calculator.calculate(labelMap, labelId);
 * @endcode
 *
 * @trace SRS-FR-028
 */
class RoiStatisticsCalculator {
public:
    /// Image type (3D short for CT, typically)
    using ImageType = itk::Image<short, 3>;

    /// Label map type for segmentation
    using LabelMapType = itk::Image<uint8_t, 3>;

    /// Callback for progress updates
    using ProgressCallback = std::function<void(double progress)>;

    RoiStatisticsCalculator();
    ~RoiStatisticsCalculator();

    // Non-copyable, movable
    RoiStatisticsCalculator(const RoiStatisticsCalculator&) = delete;
    RoiStatisticsCalculator& operator=(const RoiStatisticsCalculator&) = delete;
    RoiStatisticsCalculator(RoiStatisticsCalculator&&) noexcept;
    RoiStatisticsCalculator& operator=(RoiStatisticsCalculator&&) noexcept;

    /**
     * @brief Set the source image for statistics calculation
     * @param image ITK image pointer
     */
    void setImage(ImageType::Pointer image);

    /**
     * @brief Set pixel spacing for accurate measurements
     * @param spacingX Pixel spacing in X direction (mm)
     * @param spacingY Pixel spacing in Y direction (mm)
     * @param spacingZ Pixel spacing in Z direction (mm)
     */
    void setPixelSpacing(double spacingX, double spacingY, double spacingZ);

    /**
     * @brief Set histogram parameters
     * @param minValue Minimum value for histogram range
     * @param maxValue Maximum value for histogram range
     * @param numBins Number of histogram bins
     */
    void setHistogramParameters(double minValue, double maxValue, int numBins);

    /**
     * @brief Set progress callback
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Calculate statistics for a 2D area measurement (ROI)
     *
     * Creates a binary mask from the ROI shape and calculates statistics
     * for all pixels within the mask on the specified slice.
     *
     * @param roi Area measurement defining the ROI
     * @param sliceIndex Slice index for 2D analysis
     * @return Statistics or error
     */
    [[nodiscard]] std::expected<RoiStatistics, StatisticsError>
    calculate(const AreaMeasurement& roi, int sliceIndex);

    /**
     * @brief Calculate statistics for a 3D segmentation label
     *
     * Uses ITK LabelStatisticsImageFilter to compute statistics
     * for all voxels with the specified label value.
     *
     * @param labelMap Label map containing segmentation
     * @param labelId Label ID to analyze
     * @return Statistics or error
     */
    [[nodiscard]] std::expected<RoiStatistics, StatisticsError>
    calculate(LabelMapType::Pointer labelMap, uint8_t labelId);

    /**
     * @brief Calculate statistics for multiple ROIs
     * @param rois Vector of area measurements
     * @param sliceIndex Slice index for 2D analysis
     * @return Vector of statistics for each ROI
     */
    [[nodiscard]] std::vector<std::expected<RoiStatistics, StatisticsError>>
    calculateMultiple(const std::vector<AreaMeasurement>& rois, int sliceIndex);

    /**
     * @brief Export multiple ROI statistics to CSV
     * @param statistics Vector of statistics to export
     * @param filePath Output file path
     * @return Success or error
     */
    [[nodiscard]] static std::expected<void, StatisticsError>
    exportMultipleToCsv(const std::vector<RoiStatistics>& statistics,
                        const std::filesystem::path& filePath);

    /**
     * @brief Compare statistics between two ROIs
     * @param stats1 First ROI statistics
     * @param stats2 Second ROI statistics
     * @return Comparison summary string
     */
    [[nodiscard]] static std::string compareStatistics(const RoiStatistics& stats1,
                                                       const RoiStatistics& stats2);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

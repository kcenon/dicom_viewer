#pragma once

#include "dicom_loader.hpp"

#include <array>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dicom_viewer::core {

/// Progress callback for series building operations
using ProgressCallback = std::function<void(size_t current, size_t total, const std::string& message)>;

/// Series information with metadata summary
struct SeriesInfo {
    std::string seriesInstanceUid;
    std::string seriesDescription;
    std::string modality;
    size_t sliceCount = 0;
    std::vector<SliceInfo> slices;

    // Spacing information
    double pixelSpacingX = 1.0;
    double pixelSpacingY = 1.0;
    double sliceSpacing = 1.0;

    // Volume dimensions
    std::array<size_t, 3> dimensions = {0, 0, 0};
};

/**
 * @brief High-level series builder for DICOM volume assembly
 *
 * Provides convenient interface for scanning directories, grouping series,
 * and building 3D volumes with progress reporting.
 *
 * @trace SRS-FR-002
 */
class SeriesBuilder {
public:
    SeriesBuilder();
    ~SeriesBuilder();

    // Non-copyable, movable
    SeriesBuilder(const SeriesBuilder&) = delete;
    SeriesBuilder& operator=(const SeriesBuilder&) = delete;
    SeriesBuilder(SeriesBuilder&&) noexcept;
    SeriesBuilder& operator=(SeriesBuilder&&) noexcept;

    /**
     * @brief Set progress callback for long-running operations
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Scan directory and return available series
     * @param directoryPath Path to directory containing DICOM files
     * @return Vector of series information on success
     */
    std::expected<std::vector<SeriesInfo>, DicomErrorInfo>
    scanForSeries(const std::filesystem::path& directoryPath);

    /**
     * @brief Build 3D volume from a specific series
     * @param series Series information with sorted slices
     * @return ITK 3D CT image on success
     */
    std::expected<CTImageType::Pointer, DicomErrorInfo>
    buildCTVolume(const SeriesInfo& series);

    /**
     * @brief Build 3D volume from a specific series
     * @param series Series information with sorted slices
     * @return ITK 3D MR image on success
     */
    std::expected<MRImageType::Pointer, DicomErrorInfo>
    buildMRVolume(const SeriesInfo& series);

    /**
     * @brief Build volume asynchronously
     * @param series Series information with sorted slices
     * @return Future containing the result
     */
    std::future<std::expected<CTImageType::Pointer, DicomErrorInfo>>
    buildCTVolumeAsync(const SeriesInfo& series);

    /**
     * @brief Get metadata for the last processed series
     */
    const DicomMetadata& getMetadata() const;

    /**
     * @brief Calculate slice spacing from series information
     * @param slices Sorted slice information
     * @return Calculated slice spacing in mm
     */
    static double calculateSliceSpacing(const std::vector<SliceInfo>& slices);

    /**
     * @brief Validate series consistency (uniform spacing, orientation)
     * @param slices Sorted slice information
     * @return true if series is consistent
     */
    static bool validateSeriesConsistency(const std::vector<SliceInfo>& slices);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::core

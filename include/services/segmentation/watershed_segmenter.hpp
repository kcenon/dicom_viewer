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
 * @file watershed_segmenter.hpp
 * @brief Watershed segmentation with region analysis and merging
 * @details Provides flood level and threshold control for output region count.
 *          Includes Gaussian preprocessing, gradient computation,
 *          marker-based option, and small region merging for
 *          oversegmentation reduction.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <array>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::services {

struct SegmentationError;

/**
 * @brief Information about a segmented region
 */
struct RegionInfo {
    /// Unique label identifier
    unsigned long label = 0;

    /// Number of voxels in the region
    size_t voxelCount = 0;

    /// Centroid coordinates (x, y, z)
    std::array<double, 3> centroid = {0.0, 0.0, 0.0};
};

/**
 * @brief Parameters for Watershed segmentation
 */
struct WatershedParameters {
    /// Flood level (0.0 - 1.0), controls number of output regions
    double level = 0.1;

    /// Minimum basin depth threshold (0.0 - 1.0)
    double threshold = 0.01;

    /// Gaussian smoothing sigma before gradient computation
    double gradientSigma = 1.0;

    /// Use marker-based watershed (requires external markers)
    bool useMarkers = false;

    /// Minimum region size in voxels (regions smaller are merged)
    int minimumRegionSize = 100;

    /// Merge small regions into neighbors
    bool mergeSmallRegions = true;

    /**
     * @brief Validate parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return level >= 0.0 && level <= 1.0 &&
               threshold >= 0.0 && threshold <= 1.0 &&
               gradientSigma > 0.0 &&
               minimumRegionSize >= 0;
    }
};

/**
 * @brief Result of Watershed segmentation
 */
struct WatershedResult {
    /// Label map with unique IDs per region
    using LabelMapType = itk::Image<unsigned long, 3>;
    LabelMapType::Pointer labelMap;

    /// Number of distinct regions
    size_t regionCount = 0;

    /// Information about each region
    std::vector<RegionInfo> regions;
};

/**
 * @brief Watershed segmentation for image partitioning
 *
 * Implements Watershed segmentation algorithm for partitioning images into
 * distinct regions based on topographical interpretation of gradient magnitude.
 * Useful for cell/tissue separation and organ boundary detection.
 *
 * Supported modes:
 * - Automatic Watershed: Uses gradient magnitude and flooding level
 * - Marker-based Watershed: Uses user-provided markers for controlled segmentation
 *
 * @example
 * @code
 * WatershedSegmenter segmenter;
 *
 * // Automatic watershed
 * WatershedParameters params;
 * params.level = 0.1;
 * params.threshold = 0.01;
 *
 * auto result = segmenter.segment(ctImage, params);
 * if (result) {
 *     auto labelMap = result->labelMap;
 *     size_t regions = result->regionCount;
 * }
 *
 * // Marker-based watershed
 * auto markers = createMarkerImage();  // User-defined markers
 * auto markerResult = segmenter.segmentWithMarkers(ctImage, markers, params);
 * @endcode
 *
 * @trace SRS-FR-027
 */
class WatershedSegmenter {
public:
    /// Input image type (typically CT or MRI)
    using ImageType = itk::Image<short, 3>;

    /// Float image type for intermediate processing
    using FloatImageType = itk::Image<float, 3>;

    /// Label map type with unique region IDs
    using LabelMapType = itk::Image<unsigned long, 3>;

    /// Binary mask type for single region extraction
    using BinaryMaskType = itk::Image<unsigned char, 3>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    WatershedSegmenter() = default;
    ~WatershedSegmenter() = default;

    // Copyable and movable
    WatershedSegmenter(const WatershedSegmenter&) = default;
    WatershedSegmenter& operator=(const WatershedSegmenter&) = default;
    WatershedSegmenter(WatershedSegmenter&&) noexcept = default;
    WatershedSegmenter& operator=(WatershedSegmenter&&) noexcept = default;

    /**
     * @brief Apply automatic Watershed segmentation
     *
     * Computes gradient magnitude and applies watershed transform to partition
     * the image into distinct regions. The level parameter controls the number
     * of output regions - higher values produce fewer regions.
     *
     * @param input Input 3D image
     * @param params Watershed parameters including level and threshold
     * @return Watershed result with label map and region info on success
     */
    [[nodiscard]] std::expected<WatershedResult, SegmentationError>
    segment(
        ImageType::Pointer input,
        const WatershedParameters& params
    ) const;

    /**
     * @brief Apply marker-based Watershed segmentation
     *
     * Uses user-provided marker image to guide the segmentation. Each unique
     * marker value defines a separate catchment basin. This provides more
     * control over the segmentation result.
     *
     * @param input Input 3D image
     * @param markers Marker image with unique labels for each seed region
     * @param params Watershed parameters
     * @return Watershed result with label map and region info on success
     */
    [[nodiscard]] std::expected<WatershedResult, SegmentationError>
    segmentWithMarkers(
        ImageType::Pointer input,
        LabelMapType::Pointer markers,
        const WatershedParameters& params
    ) const;

    /**
     * @brief Extract a single region as binary mask
     *
     * @param labelMap Label map from watershed segmentation
     * @param regionLabel Label value of the region to extract
     * @return Binary mask for the specified region
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    extractRegion(
        LabelMapType::Pointer labelMap,
        unsigned long regionLabel
    ) const;

    /**
     * @brief Set progress callback for long operations
     * @param callback Progress callback function
     */
    void setProgressCallback(ProgressCallback callback);

private:
    /**
     * @brief Compute gradient magnitude image
     *
     * Applies Gaussian smoothing followed by gradient magnitude filter.
     *
     * @param input Input image
     * @param sigma Gaussian smoothing sigma
     * @return Gradient magnitude image
     */
    [[nodiscard]] FloatImageType::Pointer computeGradientMagnitude(
        ImageType::Pointer input,
        double sigma
    ) const;

    /**
     * @brief Apply watershed transform to gradient image
     *
     * @param gradient Gradient magnitude image
     * @param level Flood level (0.0 - 1.0)
     * @param threshold Minimum basin depth
     * @return Label map from watershed
     */
    [[nodiscard]] LabelMapType::Pointer applyWatershed(
        FloatImageType::Pointer gradient,
        double level,
        double threshold
    ) const;

    /**
     * @brief Apply morphological watershed with markers
     *
     * @param gradient Gradient magnitude image
     * @param markers Marker image
     * @return Label map from watershed
     */
    [[nodiscard]] LabelMapType::Pointer applyMarkerWatershed(
        FloatImageType::Pointer gradient,
        LabelMapType::Pointer markers
    ) const;

    /**
     * @brief Remove small regions and relabel
     *
     * @param labelMap Input label map
     * @param minimumSize Minimum region size in voxels
     * @return Cleaned and relabeled map
     */
    [[nodiscard]] LabelMapType::Pointer removeSmallRegions(
        LabelMapType::Pointer labelMap,
        int minimumSize
    ) const;

    /**
     * @brief Compute region statistics
     *
     * @param labelMap Label map
     * @return Vector of RegionInfo for each region
     */
    [[nodiscard]] std::vector<RegionInfo> computeRegionStatistics(
        LabelMapType::Pointer labelMap
    ) const;

    ProgressCallback progressCallback_;
};

}  // namespace dicom_viewer::services

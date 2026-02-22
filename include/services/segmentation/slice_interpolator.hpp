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
 * @file slice_interpolator.hpp
 * @brief Interpolation method selection and parameters for slice interpolation
 * @details Supports morphological (recommended), shape-based, and linear
 *          interpolation methods with heuristic contour alignment and
 *          multiple pass options. Includes label filtering and slice bounds.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

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
 * @brief Interpolation method selection
 */
enum class InterpolationMethod {
    Morphological,  ///< ITK MorphologicalContourInterpolator (recommended)
    ShapeBased,     ///< Signed distance field interpolation
    Linear          ///< Simple linear blend
};

/**
 * @brief Parameters for slice interpolation
 */
struct InterpolationParameters {
    /// Interpolation method to use
    InterpolationMethod method = InterpolationMethod::Morphological;

    /// Which labels to interpolate (empty = all labels)
    std::vector<uint8_t> labelIds;

    /// Optional start slice bound
    std::optional<int> startSlice;

    /// Optional end slice bound
    std::optional<int> endSlice;

    /// Auto-align contours between slices (morphological only)
    bool useHeuristicAlignment = true;

    /// Multiple passes for complex shapes
    int interpolationPasses = 1;

    /**
     * @brief Validate parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return interpolationPasses > 0;
    }
};

/**
 * @brief Result of slice interpolation
 */
struct InterpolationResult {
    /// Interpolated label map
    using LabelMapType = itk::Image<unsigned char, 3>;
    LabelMapType::Pointer interpolatedMask;

    /// Indices of slices that were interpolated
    std::vector<int> interpolatedSlices;

    /// Indices of original annotated slices
    std::vector<int> sourceSlices;
};

/**
 * @brief Slice interpolation for segmentation masks
 *
 * Implements automatic interpolation of segmentation masks between manually
 * segmented slices. This dramatically reduces manual annotation effort by
 * allowing users to segment every Nth slice and interpolating the gaps.
 *
 * Supported algorithms:
 * - Morphological Contour Interpolation: ITK's gold standard for medical imaging
 * - Shape-Based Interpolation: Using signed distance maps
 * - Linear Interpolation: Simple blend for basic cases
 *
 * @example
 * @code
 * SliceInterpolator interpolator;
 *
 * // Detect annotated slices
 * auto slices = interpolator.detectAnnotatedSlices(labelMap, 1);
 * // Returns e.g., {10, 20, 30} for slices with label 1
 *
 * // Interpolate all gaps
 * InterpolationParameters params;
 * params.labelIds = {1};  // Interpolate only label 1
 *
 * auto result = interpolator.interpolate(labelMap, params);
 * if (result) {
 *     // Slices 11-19, 21-29 are now filled
 *     auto interpolatedMask = result->interpolatedMask;
 * }
 * @endcode
 *
 * @trace SRS-FR-029
 */
class SliceInterpolator {
public:
    /// Label map type (3D volume with label IDs)
    using LabelMapType = itk::Image<unsigned char, 3>;

    /// Float image type for intermediate processing
    using FloatImageType = itk::Image<float, 3>;

    /// 2D slice type for preview
    using SliceType = itk::Image<unsigned char, 2>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    SliceInterpolator() = default;
    ~SliceInterpolator() = default;

    // Copyable and movable
    SliceInterpolator(const SliceInterpolator&) = default;
    SliceInterpolator& operator=(const SliceInterpolator&) = default;
    SliceInterpolator(SliceInterpolator&&) noexcept = default;
    SliceInterpolator& operator=(SliceInterpolator&&) noexcept = default;

    /**
     * @brief Detect which slices have annotations for a specific label
     *
     * Scans through the volume and identifies slices containing the specified
     * label ID.
     *
     * @param labelMap Input label map
     * @param labelId Label ID to search for
     * @return Vector of slice indices containing the label (sorted ascending)
     */
    [[nodiscard]] std::vector<int> detectAnnotatedSlices(
        LabelMapType::Pointer labelMap,
        uint8_t labelId
    ) const;

    /**
     * @brief Detect all unique labels in the label map
     *
     * @param labelMap Input label map
     * @return Vector of unique label IDs (excluding background 0)
     */
    [[nodiscard]] std::vector<uint8_t> detectLabels(
        LabelMapType::Pointer labelMap
    ) const;

    /**
     * @brief Interpolate all gaps for specified labels
     *
     * Automatically detects annotated slices and fills gaps between them.
     *
     * @param labelMap Input label map with sparse annotations
     * @param params Interpolation parameters
     * @return Interpolation result with filled mask, or error
     */
    [[nodiscard]] std::expected<InterpolationResult, SegmentationError>
    interpolate(
        LabelMapType::Pointer labelMap,
        const InterpolationParameters& params
    ) const;

    /**
     * @brief Interpolate specific slice range for a single label
     *
     * @param labelMap Input label map
     * @param labelId Label ID to interpolate
     * @param startSlice Start slice index
     * @param endSlice End slice index
     * @return Interpolation result, or error
     */
    [[nodiscard]] std::expected<InterpolationResult, SegmentationError>
    interpolateRange(
        LabelMapType::Pointer labelMap,
        uint8_t labelId,
        int startSlice,
        int endSlice
    ) const;

    /**
     * @brief Preview interpolation for a single target slice
     *
     * Useful for showing preview before committing interpolation.
     *
     * @param labelMap Input label map
     * @param labelId Label ID to interpolate
     * @param targetSlice Target slice index to preview
     * @return 2D slice with interpolated content, or error
     */
    [[nodiscard]] std::expected<SliceType::Pointer, SegmentationError>
    previewSlice(
        LabelMapType::Pointer labelMap,
        uint8_t labelId,
        int targetSlice
    ) const;

    /**
     * @brief Set progress callback for long operations
     * @param callback Progress callback function
     */
    void setProgressCallback(ProgressCallback callback);

private:
    /**
     * @brief Apply morphological contour interpolation (ITK)
     *
     * @param input Input label map
     * @param labelId Label ID to process
     * @return Interpolated label map
     */
    [[nodiscard]] LabelMapType::Pointer morphologicalInterpolation(
        LabelMapType::Pointer input,
        uint8_t labelId
    ) const;

    /**
     * @brief Apply shape-based interpolation using signed distance maps
     *
     * @param input Input label map
     * @param labelId Label ID to process
     * @return Interpolated label map
     */
    [[nodiscard]] LabelMapType::Pointer shapeBasedInterpolation(
        LabelMapType::Pointer input,
        uint8_t labelId
    ) const;

    /**
     * @brief Apply simple linear interpolation
     *
     * @param input Input label map
     * @param labelId Label ID to process
     * @return Interpolated label map
     */
    [[nodiscard]] LabelMapType::Pointer linearInterpolation(
        LabelMapType::Pointer input,
        uint8_t labelId
    ) const;

    /**
     * @brief Extract single label as binary mask
     *
     * @param labelMap Input label map
     * @param labelId Label ID to extract
     * @return Binary mask for the label
     */
    [[nodiscard]] LabelMapType::Pointer extractLabel(
        LabelMapType::Pointer labelMap,
        uint8_t labelId
    ) const;

    /**
     * @brief Merge interpolated label back into label map
     *
     * @param labelMap Original label map
     * @param interpolated Interpolated binary mask
     * @param labelId Label ID to merge
     * @return Combined label map
     */
    [[nodiscard]] LabelMapType::Pointer mergeLabel(
        LabelMapType::Pointer labelMap,
        LabelMapType::Pointer interpolated,
        uint8_t labelId
    ) const;

    /**
     * @brief Extract 2D slice from 3D volume
     *
     * @param volume Input 3D volume
     * @param sliceIndex Slice index to extract
     * @return 2D slice
     */
    [[nodiscard]] SliceType::Pointer extractSlice(
        LabelMapType::Pointer volume,
        int sliceIndex
    ) const;

    ProgressCallback progressCallback_;
};

}  // namespace dicom_viewer::services

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
 * @file threshold_segmenter.hpp
 * @brief Error types and result structures for segmentation operations
 * @details Defines error codes (Success, InvalidInput, InvalidParameters,
 *          ProcessingFailed, InternalError) with human-readable messages.
 *          Includes OtsuThresholdResult for automatic threshold calculation.
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

/**
 * @brief Error information for segmentation operations
 */
struct SegmentationError {
    enum class Code {
        Success,
        InvalidInput,
        InvalidParameters,
        ProcessingFailed,
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
            case Code::InvalidInput: return "Invalid input: " + message;
            case Code::InvalidParameters: return "Invalid parameters: " + message;
            case Code::ProcessingFailed: return "Processing failed: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Result of Otsu threshold calculation
 */
struct OtsuThresholdResult {
    /// Calculated threshold value
    double threshold;

    /// Binary mask from thresholding
    using BinaryMaskType = itk::Image<unsigned char, 3>;
    BinaryMaskType::Pointer mask;
};

/**
 * @brief Result of multi-threshold Otsu calculation
 */
struct OtsuMultiThresholdResult {
    /// Calculated threshold values (sorted ascending)
    std::vector<double> thresholds;

    /// Label map with regions (0 = below first threshold, 1..N = between thresholds)
    using LabelMapType = itk::Image<unsigned char, 3>;
    LabelMapType::Pointer labelMap;
};

/**
 * @brief Threshold-based segmentation using ITK filters
 *
 * Provides manual and automatic threshold segmentation capabilities
 * for CT/MRI medical images based on HU or signal intensity values.
 *
 * Supported algorithms:
 * - Manual binary threshold (user-defined lower/upper bounds)
 * - Otsu automatic threshold
 * - Multi-class Otsu threshold
 *
 * @example
 * @code
 * ThresholdSegmenter segmenter;
 *
 * // Manual thresholding for bone segmentation
 * auto result = segmenter.manualThreshold(ctImage, 200.0, 3000.0);
 * if (result) {
 *     auto boneMask = result.value();
 * }
 *
 * // Automatic Otsu thresholding
 * auto otsuResult = segmenter.otsuThreshold(mrImage);
 * if (otsuResult) {
 *     double threshold = otsuResult->threshold;
 *     auto mask = otsuResult->mask;
 * }
 * @endcode
 *
 * @trace SRS-FR-020
 */
class ThresholdSegmenter {
public:
    /// Input image type (typically CT or MRI)
    using ImageType = itk::Image<short, 3>;

    /// Binary mask output type
    using BinaryMaskType = itk::Image<unsigned char, 3>;

    /// Label map type for multi-threshold
    using LabelMapType = itk::Image<unsigned char, 3>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Parameters for manual threshold segmentation
     */
    struct ThresholdParameters {
        /// Lower threshold value (inclusive)
        double lowerThreshold = 0.0;

        /// Upper threshold value (inclusive)
        double upperThreshold = 3000.0;

        /// Value for pixels inside the threshold range
        unsigned char insideValue = 1;

        /// Value for pixels outside the threshold range
        unsigned char outsideValue = 0;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            return lowerThreshold <= upperThreshold;
        }
    };

    /**
     * @brief Parameters for Otsu threshold
     */
    struct OtsuParameters {
        /// Number of histogram bins (default 256)
        unsigned int numberOfHistogramBins = 256;

        /// For multi-threshold: number of thresholds (2-255)
        unsigned int numberOfThresholds = 1;

        /// Value spread across thresholds (for multi-threshold)
        bool valleyEmphasis = false;
    };

    ThresholdSegmenter() = default;
    ~ThresholdSegmenter() = default;

    // Copyable and movable
    ThresholdSegmenter(const ThresholdSegmenter&) = default;
    ThresholdSegmenter& operator=(const ThresholdSegmenter&) = default;
    ThresholdSegmenter(ThresholdSegmenter&&) noexcept = default;
    ThresholdSegmenter& operator=(ThresholdSegmenter&&) noexcept = default;

    /**
     * @brief Apply manual binary threshold segmentation
     *
     * Segments the image using user-defined lower and upper threshold values.
     * Pixels within [lower, upper] are set to insideValue, others to outsideValue.
     *
     * @param input Input 3D image
     * @param lowerThreshold Lower threshold (HU value for CT)
     * @param upperThreshold Upper threshold (HU value for CT)
     * @return Binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    manualThreshold(
        ImageType::Pointer input,
        double lowerThreshold,
        double upperThreshold
    ) const;

    /**
     * @brief Apply manual binary threshold with detailed parameters
     *
     * @param input Input 3D image
     * @param params Threshold parameters
     * @return Binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    manualThreshold(
        ImageType::Pointer input,
        const ThresholdParameters& params
    ) const;

    /**
     * @brief Apply Otsu automatic threshold segmentation with default parameters
     *
     * Automatically calculates optimal threshold using Otsu's method
     * to maximize between-class variance.
     *
     * @param input Input 3D image
     * @return Threshold result with calculated value and mask
     */
    [[nodiscard]] std::expected<OtsuThresholdResult, SegmentationError>
    otsuThreshold(ImageType::Pointer input) const;

    /**
     * @brief Apply Otsu automatic threshold segmentation
     *
     * Automatically calculates optimal threshold using Otsu's method
     * to maximize between-class variance.
     *
     * @param input Input 3D image
     * @param params Otsu parameters
     * @return Threshold result with calculated value and mask
     */
    [[nodiscard]] std::expected<OtsuThresholdResult, SegmentationError>
    otsuThreshold(
        ImageType::Pointer input,
        const OtsuParameters& params
    ) const;

    /**
     * @brief Apply multi-class Otsu threshold segmentation with default parameters
     *
     * Segments image into multiple classes using multiple thresholds.
     * Results in N+1 regions for N thresholds.
     *
     * @param input Input 3D image
     * @param numThresholds Number of thresholds (2-255, creates numThresholds+1 regions)
     * @return Multi-threshold result with thresholds and label map
     */
    [[nodiscard]] std::expected<OtsuMultiThresholdResult, SegmentationError>
    otsuMultiThreshold(
        ImageType::Pointer input,
        unsigned int numThresholds
    ) const;

    /**
     * @brief Apply multi-class Otsu threshold segmentation
     *
     * Segments image into multiple classes using multiple thresholds.
     * Results in N+1 regions for N thresholds.
     *
     * @param input Input 3D image
     * @param numThresholds Number of thresholds (2-255, creates numThresholds+1 regions)
     * @param params Otsu parameters
     * @return Multi-threshold result with thresholds and label map
     */
    [[nodiscard]] std::expected<OtsuMultiThresholdResult, SegmentationError>
    otsuMultiThreshold(
        ImageType::Pointer input,
        unsigned int numThresholds,
        const OtsuParameters& params
    ) const;

    /**
     * @brief Apply threshold to a single 2D slice (for preview)
     *
     * @param input Input 3D image
     * @param sliceIndex Slice index (along Z axis)
     * @param lowerThreshold Lower threshold
     * @param upperThreshold Upper threshold
     * @return 2D binary mask for the specified slice
     */
    [[nodiscard]] std::expected<itk::Image<unsigned char, 2>::Pointer, SegmentationError>
    thresholdSlice(
        ImageType::Pointer input,
        unsigned int sliceIndex,
        double lowerThreshold,
        double upperThreshold
    ) const;

    /**
     * @brief Set progress callback for long operations
     * @param callback Progress callback function
     */
    void setProgressCallback(ProgressCallback callback);

private:
    ProgressCallback progressCallback_;
};

} // namespace dicom_viewer::services

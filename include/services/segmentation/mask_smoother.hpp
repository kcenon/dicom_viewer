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
 * @file mask_smoother.hpp
 * @brief Volume-preserving Gaussian boundary smoother for binary masks
 * @details Smooths mask boundaries using Gaussian blur followed by adaptive
 *          re-thresholding via binary search to preserve original volume
 *          within tolerance (default 1%).
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include "threshold_segmenter.hpp"

#include <expected>

#include <itkImage.h>

namespace dicom_viewer::services {

/**
 * @brief Volume-preserving Gaussian boundary smoother for binary masks
 *
 * Smooths the boundary of a binary segmentation mask using Gaussian
 * blurring followed by adaptive re-thresholding. The key improvement
 * over naive Gaussian blur is binary search for the threshold that
 * preserves the original mask volume within a configurable tolerance.
 *
 * Algorithm:
 * 1. Convert binary mask to float [0, 1]
 * 2. Apply Gaussian smoothing with configurable sigma
 * 3. Binary search for threshold that preserves volume (±tolerance)
 * 4. Threshold smoothed image at optimal level
 *
 * @trace SRS-FR-025
 */
class MaskSmoother {
public:
    using BinaryMaskType = itk::Image<uint8_t, 3>;
    using FloatImageType = itk::Image<float, 3>;

    /**
     * @brief Configuration for mask smoothing
     */
    struct Config {
        double sigmaMm = 1.0;           ///< Gaussian sigma in millimeters
        double volumeTolerance = 0.01;  ///< Acceptable volume deviation (1%)
        int maxBinarySearchIter = 50;   ///< Max iterations for threshold search
        uint8_t foregroundValue = 1;
    };

    /**
     * @brief Smooth mask boundaries while preserving volume
     *
     * @param input Binary mask to smooth
     * @param config Smoothing configuration
     * @return Smoothed mask or error
     */
    [[nodiscard]] static std::expected<BinaryMaskType::Pointer, SegmentationError>
    smooth(BinaryMaskType::Pointer input, const Config& config);

    /**
     * @brief Smooth mask with default configuration (1mm sigma)
     *
     * @param input Binary mask to smooth
     * @param sigmaMm Gaussian sigma in millimeters
     * @return Smoothed mask or error
     */
    [[nodiscard]] static std::expected<BinaryMaskType::Pointer, SegmentationError>
    smooth(BinaryMaskType::Pointer input, double sigmaMm = 1.0);

    /**
     * @brief Count foreground voxels in a binary mask
     *
     * @param mask Binary mask
     * @param foregroundValue Value to count
     * @return Number of foreground voxels
     */
    [[nodiscard]] static size_t countForeground(
        const BinaryMaskType* mask, uint8_t foregroundValue = 1);

    /**
     * @brief Count voxels above threshold in a float image
     *
     * @param image Float image (values in [0, 1])
     * @param threshold Threshold value
     * @return Number of voxels above threshold
     */
    [[nodiscard]] static size_t countAboveThreshold(
        const FloatImageType* image, float threshold);
};

}  // namespace dicom_viewer::services

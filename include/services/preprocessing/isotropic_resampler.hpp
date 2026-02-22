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

#include <array>
#include <expected>
#include <functional>
#include <memory>
#include <string>

#include <itkImage.h>
#include <itkSmartPointer.h>

#include "services/preprocessing/gaussian_smoother.hpp"

namespace dicom_viewer::services {

/**
 * @brief Isotropic resampling for anisotropic voxel normalization
 *
 * Resamples anisotropic voxels (e.g., 0.5x0.5x2.5 mm) to isotropic voxels
 * (e.g., 1.0x1.0x1.0 mm). This is essential for algorithms that assume
 * isotropic voxel spacing, such as 3D segmentation and surface rendering.
 *
 * The filter uses ITK's ResampleImageFilter with configurable interpolation
 * methods for different use cases:
 * - Nearest Neighbor: Label maps, binary masks (preserves discrete values)
 * - Linear: General purpose, good balance of quality and speed
 * - B-Spline: High quality visualization with smooth results
 * - Windowed Sinc: Best quality but slowest
 *
 * @example
 * @code
 * IsotropicResampler resampler;
 *
 * // Apply with default parameters (1.0mm isotropic, linear interpolation)
 * auto result = resampler.resample(anisotropicImage);
 * if (result) {
 *     auto isotropicImage = result.value();
 * }
 *
 * // Apply with custom parameters
 * IsotropicResampler::Parameters params;
 * params.targetSpacing = 0.5;  // 0.5mm isotropic
 * params.interpolation = IsotropicResampler::Interpolation::BSpline;
 * auto customResult = resampler.resample(anisotropicImage, params);
 *
 * // Resample label map (automatically uses nearest neighbor)
 * auto labelResult = resampler.resampleLabels(labelMap, 1.0);
 * @endcode
 *
 * @trace SRS-FR-019
 */
class IsotropicResampler {
public:
    /// Input/Output image type (short for DICOM compatibility)
    using ImageType = itk::Image<short, 3>;

    /// Label map type for segmentation masks
    using LabelMapType = itk::Image<unsigned char, 3>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Interpolation method for resampling
     */
    enum class Interpolation {
        NearestNeighbor,  ///< For label maps and binary masks
        Linear,           ///< General purpose (default)
        BSpline,          ///< High quality visualization
        WindowedSinc      ///< Best quality, slowest
    };

    /**
     * @brief Parameters for isotropic resampling
     */
    struct Parameters {
        /// Target spacing in mm (same for all dimensions)
        /// Range: 0.1 to 10.0 mm
        double targetSpacing = 1.0;

        /// Interpolation method
        Interpolation interpolation = Interpolation::Linear;

        /// Default pixel value for out-of-bounds regions
        double defaultValue = 0.0;

        /// B-spline order (only used when interpolation is BSpline)
        /// Range: 2 to 5
        unsigned int splineOrder = 3;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            if (targetSpacing < 0.1 || targetSpacing > 10.0) {
                return false;
            }
            if (splineOrder < 2 || splineOrder > 5) {
                return false;
            }
            return true;
        }
    };

    /**
     * @brief Information about resampled volume dimensions
     */
    struct ResampledInfo {
        /// Original image size
        std::array<unsigned int, 3> originalSize;

        /// Original spacing in mm
        std::array<double, 3> originalSpacing;

        /// Resampled image size
        std::array<unsigned int, 3> resampledSize;

        /// Resampled spacing in mm (isotropic)
        double resampledSpacing;

        /// Memory size estimate in bytes
        size_t estimatedMemoryBytes;
    };

    IsotropicResampler();
    ~IsotropicResampler();

    // Non-copyable, movable
    IsotropicResampler(const IsotropicResampler&) = delete;
    IsotropicResampler& operator=(const IsotropicResampler&) = delete;
    IsotropicResampler(IsotropicResampler&&) noexcept;
    IsotropicResampler& operator=(IsotropicResampler&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Resample image to isotropic voxels with default parameters
     *
     * Uses 1.0mm target spacing with linear interpolation.
     *
     * @param input Input 3D image with anisotropic voxels
     * @return Resampled image on success, error on failure
     */
    [[nodiscard]] std::expected<ImageType::Pointer, PreprocessingError>
    resample(ImageType::Pointer input) const;

    /**
     * @brief Resample image to isotropic voxels with custom parameters
     *
     * @param input Input 3D image with anisotropic voxels
     * @param params Resampling parameters
     * @return Resampled image on success, error on failure
     */
    [[nodiscard]] std::expected<ImageType::Pointer, PreprocessingError>
    resample(ImageType::Pointer input, const Parameters& params) const;

    /**
     * @brief Resample label map to isotropic voxels
     *
     * Automatically uses nearest neighbor interpolation to preserve
     * discrete label values. This is essential for segmentation masks
     * and binary masks.
     *
     * @param input Input label map with anisotropic voxels
     * @param targetSpacing Target isotropic spacing in mm
     * @return Resampled label map on success, error on failure
     */
    [[nodiscard]] std::expected<LabelMapType::Pointer, PreprocessingError>
    resampleLabels(LabelMapType::Pointer input, double targetSpacing) const;

    /**
     * @brief Preview resampled volume dimensions without actual resampling
     *
     * Useful for UI display to show the user what the output dimensions
     * will be before committing to the potentially expensive operation.
     *
     * @param input Input image to analyze
     * @param params Resampling parameters
     * @return Information about the resampled volume
     */
    [[nodiscard]] std::expected<ResampledInfo, PreprocessingError>
    previewDimensions(ImageType::Pointer input, const Parameters& params) const;

    /**
     * @brief Check if an image needs isotropic resampling
     *
     * Returns true if the image spacing is significantly anisotropic
     * (difference > 1% between any dimensions).
     *
     * @param input Input image to check
     * @return true if resampling is recommended
     */
    [[nodiscard]] static bool needsResampling(ImageType::Pointer input);

    /**
     * @brief Get string representation of interpolation method
     * @param interp Interpolation method
     * @return Human-readable string
     */
    [[nodiscard]] static std::string interpolationToString(Interpolation interp);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

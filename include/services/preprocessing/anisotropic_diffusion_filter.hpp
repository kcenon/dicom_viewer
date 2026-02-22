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

#include <expected>
#include <functional>
#include <memory>
#include <string>

#include <itkImage.h>
#include <itkSmartPointer.h>

#include "services/preprocessing/gaussian_smoother.hpp"

namespace dicom_viewer::services {

/**
 * @brief Anisotropic diffusion filter for edge-preserving noise reduction
 *
 * Applies curvature-driven anisotropic diffusion filter to remove noise
 * from CT/MRI images while preserving edges. Unlike Gaussian smoothing,
 * this filter reduces noise in homogeneous regions while maintaining
 * sharp boundaries between different tissue types.
 *
 * The filter uses ITK's CurvatureAnisotropicDiffusionImageFilter which
 * implements the Perona-Malik anisotropic diffusion equation with
 * curvature-based conductance.
 *
 * @example
 * @code
 * AnisotropicDiffusionFilter filter;
 *
 * // Apply with default parameters
 * auto result = filter.apply(mriImage);
 * if (result) {
 *     auto filteredImage = result.value();
 * }
 *
 * // Apply with custom parameters
 * AnisotropicDiffusionFilter::Parameters params;
 * params.numberOfIterations = 15;
 * params.conductance = 3.0;
 * auto customResult = filter.apply(mriImage, params);
 *
 * // Preview on single slice (faster)
 * auto sliceResult = filter.applyToSlice(mriImage, 50, params);
 * @endcode
 *
 * @trace SRS-FR-017
 */
class AnisotropicDiffusionFilter {
public:
    /// Input/Output image type (typically CT or MRI)
    using ImageType = itk::Image<float, 3>;

    /// Internal computation type (requires float for diffusion)
    using InternalImageType = itk::Image<float, 3>;

    /// Input image type (short for DICOM compatibility)
    using InputImageType = itk::Image<short, 3>;

    /// 2D slice image type for preview
    using Image2DType = itk::Image<float, 2>;

    /// 2D input image type for preview
    using Input2DImageType = itk::Image<short, 2>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Parameters for anisotropic diffusion filtering
     */
    struct Parameters {
        /// Number of iterations for the diffusion process
        /// Range: 1 to 50
        /// More iterations produce stronger smoothing
        int numberOfIterations = 10;

        /// Conductance parameter controlling edge sensitivity
        /// Range: 0.5 to 10.0
        /// Lower values preserve more edges
        double conductance = 3.0;

        /// Time step for numerical stability
        /// Range: 0.0 to 0.125 (3D stability limit)
        /// 0.0 = automatic calculation
        double timeStep = 0.0;

        /// Whether to use image spacing in diffusion computation
        /// true = conductance is in physical units (mm)
        /// false = conductance is in voxel units
        bool useImageSpacing = true;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            if (numberOfIterations < 1 || numberOfIterations > 50) {
                return false;
            }
            if (conductance < 0.5 || conductance > 10.0) {
                return false;
            }
            if (timeStep < 0.0 || timeStep > 0.125) {
                return false;
            }
            return true;
        }

        /**
         * @brief Get automatic time step for 3D stability
         * @return Maximum stable time step for 3D diffusion
         */
        [[nodiscard]] static constexpr double getDefaultTimeStep() noexcept {
            // For 3D: timeStep <= 1 / (2^N) where N = dimension
            // 3D: 1/8 = 0.125, but use 0.0625 for better stability
            return 0.0625;
        }
    };

    AnisotropicDiffusionFilter();
    ~AnisotropicDiffusionFilter();

    // Non-copyable, movable
    AnisotropicDiffusionFilter(const AnisotropicDiffusionFilter&) = delete;
    AnisotropicDiffusionFilter& operator=(const AnisotropicDiffusionFilter&) = delete;
    AnisotropicDiffusionFilter(AnisotropicDiffusionFilter&&) noexcept;
    AnisotropicDiffusionFilter& operator=(AnisotropicDiffusionFilter&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Apply anisotropic diffusion with default parameters
     *
     * Uses default iteration count of 10 and conductance of 3.0.
     *
     * @param input Input 3D image (short type)
     * @return Filtered image on success, error on failure
     */
    [[nodiscard]] std::expected<InputImageType::Pointer, PreprocessingError>
    apply(InputImageType::Pointer input) const;

    /**
     * @brief Apply anisotropic diffusion with custom parameters
     *
     * @param input Input 3D image (short type)
     * @param params Filtering parameters
     * @return Filtered image on success, error on failure
     */
    [[nodiscard]] std::expected<InputImageType::Pointer, PreprocessingError>
    apply(InputImageType::Pointer input, const Parameters& params) const;

    /**
     * @brief Apply anisotropic diffusion to a single 2D slice (for preview)
     *
     * Extracts a slice from the 3D volume, applies filtering, and returns
     * the 2D result. Useful for previewing filter effects before applying
     * to the full volume.
     *
     * @param input Input 3D image
     * @param sliceIndex Slice index along Z axis
     * @return Filtered 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Input2DImageType::Pointer, PreprocessingError>
    applyToSlice(InputImageType::Pointer input, unsigned int sliceIndex) const;

    /**
     * @brief Apply anisotropic diffusion to a single 2D slice with custom parameters
     *
     * @param input Input 3D image
     * @param sliceIndex Slice index along Z axis
     * @param params Filtering parameters
     * @return Filtered 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Input2DImageType::Pointer, PreprocessingError>
    applyToSlice(
        InputImageType::Pointer input,
        unsigned int sliceIndex,
        const Parameters& params
    ) const;

    /**
     * @brief Estimate processing time based on image size and parameters
     *
     * Useful for UI display and progress estimation.
     *
     * @param imageSize Image dimensions (x, y, z)
     * @param params Filtering parameters
     * @return Estimated processing time in seconds
     */
    [[nodiscard]] static double estimateProcessingTime(
        const std::array<unsigned int, 3>& imageSize,
        const Parameters& params
    );

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

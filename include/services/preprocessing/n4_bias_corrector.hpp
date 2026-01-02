#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

#include "services/preprocessing/gaussian_smoother.hpp"

namespace dicom_viewer::services {

/**
 * @brief N4 bias field correction for MRI intensity inhomogeneity
 *
 * Corrects MRI intensity inhomogeneity (bias field artifact) using the N4ITK
 * algorithm. The bias field is a smooth, low-frequency artifact caused by
 * magnetic field inhomogeneity in MRI scanners, which appears as slow
 * intensity variation across the image.
 *
 * The filter uses ITK's N4BiasFieldCorrectionImageFilter which implements
 * an improved version of the N3 (nonparametric nonuniform intensity
 * normalization) algorithm.
 *
 * @example
 * @code
 * N4BiasCorrector corrector;
 *
 * // Apply with default parameters
 * auto result = corrector.apply(mriImage);
 * if (result) {
 *     auto correctedImage = result->correctedImage;
 *     auto biasField = result->biasField;
 * }
 *
 * // Apply with custom parameters
 * N4BiasCorrector::Parameters params;
 * params.shrinkFactor = 2;
 * params.numberOfFittingLevels = 4;
 * auto customResult = corrector.apply(mriImage, params);
 *
 * // Apply with mask for ROI-based correction
 * auto maskedResult = corrector.apply(mriImage, params, brainMask);
 * @endcode
 *
 * @trace SRS-FR-018
 */
class N4BiasCorrector {
public:
    /// Input image type (short for DICOM compatibility)
    using InputImageType = itk::Image<short, 3>;

    /// Internal float image type for processing
    using FloatImageType = itk::Image<float, 3>;

    /// Binary mask type for ROI-based correction
    using MaskImageType = itk::Image<unsigned char, 3>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Parameters for N4 bias field correction
     */
    struct Parameters {
        /// Shrink factor for speed optimization
        /// Range: 1 to 8
        /// Higher values = faster but less accurate
        int shrinkFactor = 4;

        /// Number of fitting levels in the B-spline hierarchy
        /// Range: 1 to 8
        int numberOfFittingLevels = 4;

        /// Maximum number of iterations at each fitting level
        /// Must have numberOfFittingLevels elements
        std::vector<int> maxIterationsPerLevel = {50, 50, 50, 50};

        /// Convergence threshold for the optimization
        /// Range: 1e-7 to 1e-1
        double convergenceThreshold = 0.001;

        /// Number of control points in the B-spline grid
        /// Initial number; doubles at each fitting level
        int numberOfControlPoints = 4;

        /// Spline order for B-spline fitting
        /// Range: 2 to 4
        int splineOrder = 3;

        /// Wiener filter noise variance (0 = automatic estimation)
        double wienerFilterNoise = 0.0;

        /// Bias field full width at half maximum (in mm)
        double biasFieldFullWidthAtHalfMaximum = 0.15;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            if (shrinkFactor < 1 || shrinkFactor > 8) {
                return false;
            }
            if (numberOfFittingLevels < 1 || numberOfFittingLevels > 8) {
                return false;
            }
            if (maxIterationsPerLevel.size() !=
                static_cast<size_t>(numberOfFittingLevels)) {
                return false;
            }
            for (int iter : maxIterationsPerLevel) {
                if (iter < 1 || iter > 500) {
                    return false;
                }
            }
            if (convergenceThreshold < 1e-7 || convergenceThreshold > 1e-1) {
                return false;
            }
            if (numberOfControlPoints < 2 || numberOfControlPoints > 32) {
                return false;
            }
            if (splineOrder < 2 || splineOrder > 4) {
                return false;
            }
            if (wienerFilterNoise < 0.0) {
                return false;
            }
            if (biasFieldFullWidthAtHalfMaximum <= 0.0) {
                return false;
            }
            return true;
        }
    };

    /**
     * @brief Result of N4 bias field correction
     */
    struct Result {
        /// Bias-corrected image
        InputImageType::Pointer correctedImage;

        /// Estimated bias field (logarithmic scale)
        FloatImageType::Pointer biasField;
    };

    N4BiasCorrector();
    ~N4BiasCorrector();

    // Non-copyable, movable
    N4BiasCorrector(const N4BiasCorrector&) = delete;
    N4BiasCorrector& operator=(const N4BiasCorrector&) = delete;
    N4BiasCorrector(N4BiasCorrector&&) noexcept;
    N4BiasCorrector& operator=(N4BiasCorrector&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Apply N4 bias correction with default parameters
     *
     * Uses default shrink factor of 4 with 4 fitting levels.
     *
     * @param input Input 3D MRI image (short type)
     * @return Result containing corrected image and bias field, or error
     */
    [[nodiscard]] std::expected<Result, PreprocessingError>
    apply(InputImageType::Pointer input) const;

    /**
     * @brief Apply N4 bias correction with custom parameters
     *
     * @param input Input 3D MRI image (short type)
     * @param params Correction parameters
     * @return Result containing corrected image and bias field, or error
     */
    [[nodiscard]] std::expected<Result, PreprocessingError>
    apply(InputImageType::Pointer input, const Parameters& params) const;

    /**
     * @brief Apply N4 bias correction with mask for ROI-based processing
     *
     * The mask specifies which voxels to use for bias field estimation.
     * Typically a brain mask for brain MRI.
     *
     * @param input Input 3D MRI image (short type)
     * @param params Correction parameters
     * @param mask Binary mask (non-zero = include in estimation)
     * @return Result containing corrected image and bias field, or error
     */
    [[nodiscard]] std::expected<Result, PreprocessingError>
    apply(
        InputImageType::Pointer input,
        const Parameters& params,
        MaskImageType::Pointer mask
    ) const;

    /**
     * @brief Estimate processing time based on image size and parameters
     *
     * @param imageSize Image dimensions (x, y, z)
     * @param params Correction parameters
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

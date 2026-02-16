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

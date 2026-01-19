#pragma once

#include <array>
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
 * @brief Histogram equalization methods
 *
 * @trace SRS-FR-019
 */
enum class EqualizationMethod {
    Standard,  ///< Global histogram equalization
    Adaptive,  ///< Local (tile-based) equalization
    CLAHE      ///< Contrast Limited Adaptive Histogram Equalization (recommended)
};

/**
 * @brief Histogram data for visualization and analysis
 */
struct HistogramData {
    std::vector<double> bins;      ///< Bin center values
    std::vector<size_t> counts;    ///< Count per bin
    double minValue;               ///< Minimum pixel value
    double maxValue;               ///< Maximum pixel value
};

/**
 * @brief Histogram equalization filter for contrast enhancement in medical images
 *
 * Applies histogram equalization to enhance image contrast, particularly useful for:
 * - Low-contrast soft tissue visualization
 * - Underexposed or overexposed images
 * - Preparing images for segmentation
 * - Enhancing subtle density differences
 *
 * The filter supports three methods:
 * - **Standard**: Global histogram equalization
 * - **Adaptive**: Local tile-based equalization (AHE)
 * - **CLAHE**: Contrast Limited Adaptive Histogram Equalization (recommended)
 *
 * CLAHE is generally preferred as it prevents over-amplification of noise
 * in homogeneous regions while still enhancing local contrast.
 *
 * @example
 * @code
 * HistogramEqualizer equalizer;
 *
 * // Apply CLAHE with default parameters
 * auto result = equalizer.applyCLAHE(ctImage);
 * if (result) {
 *     auto enhancedImage = result.value();
 * }
 *
 * // Apply with custom parameters
 * HistogramEqualizer::Parameters params;
 * params.method = EqualizationMethod::CLAHE;
 * params.clipLimit = 2.0;
 * params.tileSize = {16, 16, 16};
 * auto customResult = equalizer.equalize(ctImage, params);
 *
 * // Preview on single slice (faster)
 * auto sliceResult = equalizer.equalizeSlice(ctImage, 50, params);
 * @endcode
 *
 * @trace SRS-FR-019
 */
class HistogramEqualizer {
public:
    /// Input/Output image type (typically CT or MRI)
    using ImageType = itk::Image<short, 3>;

    /// 2D slice image type for preview
    using Image2DType = itk::Image<short, 2>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Parameters for histogram equalization
     */
    struct Parameters {
        /// Equalization method to use
        EqualizationMethod method = EqualizationMethod::CLAHE;

        /// Contrast limiting factor for CLAHE
        /// Range: 0.1 to 10.0
        /// Lower values produce less contrast enhancement
        /// Higher values may amplify noise
        double clipLimit = 3.0;

        /// Tile size for adaptive methods (x, y, z)
        /// Smaller tiles provide more local adaptation
        /// Range: 1 to 64 for each dimension
        std::array<unsigned int, 3> tileSize = {8, 8, 8};

        /// Number of histogram bins for standard equalization
        /// Range: 16 to 4096
        int numberOfBins = 256;

        /// Output minimum value (for standard equalization)
        double outputMinimum = 0.0;

        /// Output maximum value (for standard equalization)
        double outputMaximum = 255.0;

        /// Whether to preserve the original intensity range
        /// true = output uses original min/max range
        /// false = output uses outputMinimum/outputMaximum
        bool preserveRange = true;

        /// Whether to use ROI-based processing
        bool useROI = false;

        /// ROI bounds [xmin, xmax, ymin, ymax, zmin, zmax]
        /// Only used when useROI is true
        std::array<int, 6> roiBounds = {0, 0, 0, 0, 0, 0};

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            if (clipLimit < 0.1 || clipLimit > 10.0) {
                return false;
            }
            for (unsigned int size : tileSize) {
                if (size < 1 || size > 64) {
                    return false;
                }
            }
            if (numberOfBins < 16 || numberOfBins > 4096) {
                return false;
            }
            return true;
        }
    };

    HistogramEqualizer();
    ~HistogramEqualizer();

    // Non-copyable, movable
    HistogramEqualizer(const HistogramEqualizer&) = delete;
    HistogramEqualizer& operator=(const HistogramEqualizer&) = delete;
    HistogramEqualizer(HistogramEqualizer&&) noexcept;
    HistogramEqualizer& operator=(HistogramEqualizer&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Apply histogram equalization with default parameters
     *
     * @param input Input 3D image
     * @return Enhanced image on success, error on failure
     */
    [[nodiscard]] std::expected<ImageType::Pointer, PreprocessingError>
    equalize(ImageType::Pointer input) const;

    /**
     * @brief Apply histogram equalization with custom parameters
     *
     * @param input Input 3D image
     * @param params Equalization parameters
     * @return Enhanced image on success, error on failure
     */
    [[nodiscard]] std::expected<ImageType::Pointer, PreprocessingError>
    equalize(ImageType::Pointer input, const Parameters& params) const;

    /**
     * @brief Apply CLAHE with specified clip limit and tile size
     *
     * Convenience method for applying CLAHE directly.
     *
     * @param input Input 3D image
     * @param clipLimit Contrast limiting factor (default: 3.0)
     * @param tileSize Tile size for local processing (default: 8x8x8)
     * @return Enhanced image on success, error on failure
     */
    [[nodiscard]] std::expected<ImageType::Pointer, PreprocessingError>
    applyCLAHE(
        ImageType::Pointer input,
        double clipLimit = 3.0,
        const std::array<unsigned int, 3>& tileSize = {8, 8, 8}
    ) const;

    /**
     * @brief Apply histogram equalization to a single 2D slice (for preview)
     *
     * Extracts a slice from the 3D volume, applies equalization, and returns
     * the 2D result. Useful for previewing filter effects before applying
     * to the full volume.
     *
     * @param input Input 3D image
     * @param sliceIndex Slice index along Z axis
     * @return Enhanced 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Image2DType::Pointer, PreprocessingError>
    equalizeSlice(
        ImageType::Pointer input,
        unsigned int sliceIndex
    ) const;

    /**
     * @brief Apply histogram equalization to a single 2D slice with custom parameters
     *
     * @param input Input 3D image
     * @param sliceIndex Slice index along Z axis
     * @param params Equalization parameters
     * @return Enhanced 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Image2DType::Pointer, PreprocessingError>
    equalizeSlice(
        ImageType::Pointer input,
        unsigned int sliceIndex,
        const Parameters& params
    ) const;

    /**
     * @brief Preview equalization (lightweight computation for parameter tuning)
     *
     * Applies equalization to a single slice for quick preview.
     *
     * @param input Input 3D image
     * @param previewSlice Slice index for preview
     * @return Enhanced 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Image2DType::Pointer, PreprocessingError>
    preview(
        ImageType::Pointer input,
        unsigned int previewSlice
    ) const;

    /**
     * @brief Preview equalization with custom parameters
     *
     * @param input Input 3D image
     * @param previewSlice Slice index for preview
     * @param params Equalization parameters
     * @return Enhanced 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Image2DType::Pointer, PreprocessingError>
    preview(
        ImageType::Pointer input,
        unsigned int previewSlice,
        const Parameters& params
    ) const;

    /**
     * @brief Compute histogram of the input image
     *
     * @param input Input 3D image
     * @param numBins Number of histogram bins (default: 256)
     * @return Histogram data
     */
    [[nodiscard]] HistogramData computeHistogram(
        ImageType::Pointer input,
        int numBins = 256
    ) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

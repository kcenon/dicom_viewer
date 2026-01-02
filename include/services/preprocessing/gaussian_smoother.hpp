#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief Error information for preprocessing operations
 *
 * @trace SRS-FR-016
 */
struct PreprocessingError {
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
 * @brief Gaussian smoothing filter for noise reduction in medical images
 *
 * Applies discrete Gaussian filter to remove noise from CT/MRI images
 * while preserving overall image structure. This is a fundamental
 * preprocessing step before segmentation or analysis.
 *
 * The filter uses ITK's DiscreteGaussianImageFilter which provides
 * a high-quality approximation of continuous Gaussian filtering.
 *
 * @example
 * @code
 * GaussianSmoother smoother;
 *
 * // Apply with default parameters (variance = 1.0)
 * auto result = smoother.apply(ctImage);
 * if (result) {
 *     auto smoothedImage = result.value();
 * }
 *
 * // Apply with custom parameters
 * GaussianSmoother::Parameters params;
 * params.variance = 2.5;
 * params.maxKernelWidth = 16;
 * auto customResult = smoother.apply(ctImage, params);
 *
 * // Preview on single slice (faster)
 * auto sliceResult = smoother.applyToSlice(ctImage, 50, params);
 * @endcode
 *
 * @trace SRS-FR-016
 */
class GaussianSmoother {
public:
    /// Input/Output image type (typically CT or MRI)
    using ImageType = itk::Image<short, 3>;

    /// 2D slice image type for preview
    using Image2DType = itk::Image<short, 2>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Parameters for Gaussian smoothing
     */
    struct Parameters {
        /// Variance (sigma squared) of the Gaussian kernel
        /// Range: 0.1 to 10.0
        /// Larger values produce stronger smoothing
        double variance = 1.0;

        /// Maximum kernel width in pixels
        /// 0 = automatic (default), otherwise limits kernel size
        /// Range: 0 or 3-32
        unsigned int maxKernelWidth = 0;

        /// Whether to use image spacing for kernel computation
        /// true = kernel is defined in physical units (mm)
        /// false = kernel is defined in voxel units
        bool useImageSpacing = true;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            if (variance < 0.1 || variance > 10.0) {
                return false;
            }
            if (maxKernelWidth != 0 && (maxKernelWidth < 3 || maxKernelWidth > 32)) {
                return false;
            }
            return true;
        }
    };

    GaussianSmoother();
    ~GaussianSmoother();

    // Non-copyable, movable
    GaussianSmoother(const GaussianSmoother&) = delete;
    GaussianSmoother& operator=(const GaussianSmoother&) = delete;
    GaussianSmoother(GaussianSmoother&&) noexcept;
    GaussianSmoother& operator=(GaussianSmoother&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Apply Gaussian smoothing with default parameters
     *
     * Uses default variance of 1.0 with automatic kernel width.
     *
     * @param input Input 3D image
     * @return Smoothed image on success, error on failure
     */
    [[nodiscard]] std::expected<ImageType::Pointer, PreprocessingError>
    apply(ImageType::Pointer input) const;

    /**
     * @brief Apply Gaussian smoothing with custom parameters
     *
     * @param input Input 3D image
     * @param params Smoothing parameters
     * @return Smoothed image on success, error on failure
     */
    [[nodiscard]] std::expected<ImageType::Pointer, PreprocessingError>
    apply(ImageType::Pointer input, const Parameters& params) const;

    /**
     * @brief Apply Gaussian smoothing to a single 2D slice (for preview)
     *
     * Extracts a slice from the 3D volume, applies smoothing, and returns
     * the 2D result. Useful for previewing filter effects before applying
     * to the full volume.
     *
     * @param input Input 3D image
     * @param sliceIndex Slice index along Z axis
     * @return Smoothed 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Image2DType::Pointer, PreprocessingError>
    applyToSlice(ImageType::Pointer input, unsigned int sliceIndex) const;

    /**
     * @brief Apply Gaussian smoothing to a single 2D slice with custom parameters
     *
     * @param input Input 3D image
     * @param sliceIndex Slice index along Z axis
     * @param params Smoothing parameters
     * @return Smoothed 2D slice on success, error on failure
     */
    [[nodiscard]] std::expected<Image2DType::Pointer, PreprocessingError>
    applyToSlice(
        ImageType::Pointer input,
        unsigned int sliceIndex,
        const Parameters& params
    ) const;

    /**
     * @brief Get the effective kernel radius for given parameters
     *
     * Useful for UI display and understanding filter extent.
     *
     * @param params Smoothing parameters
     * @param spacing Image spacing in mm (x, y, z)
     * @return Effective kernel radius in voxels for each dimension
     */
    [[nodiscard]] static std::array<unsigned int, 3> getKernelRadius(
        const Parameters& params,
        const std::array<double, 3>& spacing
    );

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

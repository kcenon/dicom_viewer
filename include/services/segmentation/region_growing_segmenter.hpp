#pragma once

#include <expected>
#include <functional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief Error information for segmentation operations
 */
struct SegmentationError;

/**
 * @brief 3D seed point for region growing algorithms
 */
struct SeedPoint {
    int x = 0;
    int y = 0;
    int z = 0;

    SeedPoint() = default;
    SeedPoint(int px, int py, int pz) : x(px), y(py), z(pz) {}

    [[nodiscard]] bool operator==(const SeedPoint& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @brief Seed point-based region growing segmentation using ITK filters
 *
 * Provides connected threshold and confidence connected region growing
 * algorithms for semi-automatic medical image segmentation.
 *
 * Supported algorithms:
 * - Connected Threshold: User-defined intensity range with seed points
 * - Confidence Connected: Automatic intensity range based on seed statistics
 *
 * @example
 * @code
 * RegionGrowingSegmenter segmenter;
 *
 * // Connected threshold with user-defined range
 * std::vector<SeedPoint> seeds = {{100, 100, 50}};
 * auto result = segmenter.connectedThreshold(image, seeds, -100.0, 200.0);
 * if (result) {
 *     auto organMask = result.value();
 * }
 *
 * // Confidence connected with automatic range
 * auto confResult = segmenter.confidenceConnected(image, seeds, 2.5, 5);
 * if (confResult) {
 *     auto autoMask = confResult.value();
 * }
 * @endcode
 *
 * @trace SRS-FR-021
 */
class RegionGrowingSegmenter {
public:
    /// Input image type (typically CT or MRI)
    using ImageType = itk::Image<short, 3>;

    /// Binary mask output type
    using BinaryMaskType = itk::Image<unsigned char, 3>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    /**
     * @brief Parameters for connected threshold segmentation
     */
    struct ConnectedThresholdParameters {
        /// Seed points for region growing
        std::vector<SeedPoint> seeds;

        /// Lower threshold value (inclusive)
        double lowerThreshold = 0.0;

        /// Upper threshold value (inclusive)
        double upperThreshold = 3000.0;

        /// Value for pixels inside the region
        unsigned char replaceValue = 1;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            return !seeds.empty() && lowerThreshold <= upperThreshold;
        }
    };

    /**
     * @brief Parameters for confidence connected segmentation
     */
    struct ConfidenceConnectedParameters {
        /// Seed points for region growing
        std::vector<SeedPoint> seeds;

        /// Multiplier for standard deviation to define intensity range
        double multiplier = 2.5;

        /// Number of iterations for refining the intensity statistics
        unsigned int numberOfIterations = 5;

        /// Radius for initial neighborhood statistics (in voxels)
        unsigned int initialNeighborhoodRadius = 2;

        /// Value for pixels inside the region
        unsigned char replaceValue = 1;

        /**
         * @brief Validate parameters
         * @return true if parameters are valid
         */
        [[nodiscard]] bool isValid() const noexcept {
            return !seeds.empty() && multiplier > 0.0 && numberOfIterations > 0;
        }
    };

    RegionGrowingSegmenter() = default;
    ~RegionGrowingSegmenter() = default;

    // Copyable and movable
    RegionGrowingSegmenter(const RegionGrowingSegmenter&) = default;
    RegionGrowingSegmenter& operator=(const RegionGrowingSegmenter&) = default;
    RegionGrowingSegmenter(RegionGrowingSegmenter&&) noexcept = default;
    RegionGrowingSegmenter& operator=(RegionGrowingSegmenter&&) noexcept = default;

    /**
     * @brief Apply connected threshold region growing segmentation
     *
     * Grows regions from seed points where connected pixels fall within
     * the specified intensity range [lower, upper].
     *
     * @param input Input 3D image
     * @param seeds Seed points to start region growing
     * @param lowerThreshold Lower intensity threshold (HU for CT)
     * @param upperThreshold Upper intensity threshold (HU for CT)
     * @return Binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    connectedThreshold(
        ImageType::Pointer input,
        const std::vector<SeedPoint>& seeds,
        double lowerThreshold,
        double upperThreshold
    ) const;

    /**
     * @brief Apply connected threshold with detailed parameters
     *
     * @param input Input 3D image
     * @param params Connected threshold parameters
     * @return Binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    connectedThreshold(
        ImageType::Pointer input,
        const ConnectedThresholdParameters& params
    ) const;

    /**
     * @brief Apply confidence connected region growing segmentation
     *
     * Automatically determines intensity range based on seed point statistics.
     * The range is: [mean - multiplier * stdDev, mean + multiplier * stdDev]
     *
     * @param input Input 3D image
     * @param seeds Seed points for statistics calculation
     * @param multiplier Standard deviation multiplier (default 2.5)
     * @param iterations Number of refinement iterations (default 5)
     * @return Binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    confidenceConnected(
        ImageType::Pointer input,
        const std::vector<SeedPoint>& seeds,
        double multiplier = 2.5,
        unsigned int iterations = 5
    ) const;

    /**
     * @brief Apply confidence connected with detailed parameters
     *
     * @param input Input 3D image
     * @param params Confidence connected parameters
     * @return Binary mask on success, error on failure
     */
    [[nodiscard]] std::expected<BinaryMaskType::Pointer, SegmentationError>
    confidenceConnected(
        ImageType::Pointer input,
        const ConfidenceConnectedParameters& params
    ) const;

    /**
     * @brief Validate seed point against image bounds
     *
     * @param input Input image
     * @param seed Seed point to validate
     * @return true if seed is within image bounds
     */
    [[nodiscard]] static bool isValidSeedPoint(
        ImageType::Pointer input,
        const SeedPoint& seed
    );

    /**
     * @brief Set progress callback for long operations
     * @param callback Progress callback function
     */
    void setProgressCallback(ProgressCallback callback);

private:
    /**
     * @brief Validate all seed points against image bounds
     *
     * @param input Input image
     * @param seeds Seed points to validate
     * @return Error if any seed is invalid, nullopt if all valid
     */
    [[nodiscard]] std::optional<SegmentationError> validateSeeds(
        ImageType::Pointer input,
        const std::vector<SeedPoint>& seeds
    ) const;

    ProgressCallback progressCallback_;
};

} // namespace dicom_viewer::services

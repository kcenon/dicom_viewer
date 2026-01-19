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
struct SeedPoint;

/**
 * @brief 3D seed point with floating-point coordinates for Level Set algorithms
 */
struct LevelSetSeedPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    LevelSetSeedPoint() = default;
    LevelSetSeedPoint(double px, double py, double pz) : x(px), y(py), z(pz) {}

    [[nodiscard]] bool operator==(const LevelSetSeedPoint& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @brief Parameters for Level Set segmentation algorithms
 */
struct LevelSetParameters {
    /// Initial seed sphere radius in mm
    double seedRadius = 5.0;

    /// Seed point locations
    std::vector<LevelSetSeedPoint> seedPoints;

    /// Speed of front propagation (positive = expansion, negative = contraction)
    double propagationScaling = 1.0;

    /// Smoothness constraint (higher = smoother boundaries)
    double curvatureScaling = 0.5;

    /// Edge attraction strength
    double advectionScaling = 1.0;

    /// Maximum number of iterations
    int maxIterations = 500;

    /// RMS change threshold for convergence
    double rmsThreshold = 0.02;

    /// Feature image scaling factor
    double featureScaling = 1.0;

    /// Gaussian smoothing sigma for preprocessing
    double sigma = 1.0;

    /**
     * @brief Validate parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return !seedPoints.empty() &&
               seedRadius > 0.0 &&
               maxIterations > 0 &&
               rmsThreshold > 0.0 &&
               sigma > 0.0;
    }
};

/**
 * @brief Parameters for Threshold Level Set segmentation
 */
struct ThresholdLevelSetParameters {
    /// Lower intensity threshold
    double lowerThreshold = -1000.0;

    /// Upper intensity threshold
    double upperThreshold = 1000.0;

    /// Initial seed sphere radius in mm
    double seedRadius = 5.0;

    /// Seed point locations
    std::vector<LevelSetSeedPoint> seedPoints;

    /// Smoothness constraint
    double curvatureScaling = 1.0;

    /// Speed of front propagation
    double propagationScaling = 1.0;

    /// Maximum number of iterations
    int maxIterations = 500;

    /// RMS change threshold for convergence
    double rmsThreshold = 0.02;

    /**
     * @brief Validate parameters
     * @return true if parameters are valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return !seedPoints.empty() &&
               seedRadius > 0.0 &&
               lowerThreshold <= upperThreshold &&
               maxIterations > 0 &&
               rmsThreshold > 0.0;
    }
};

/**
 * @brief Result of Level Set segmentation
 */
struct LevelSetResult {
    /// Binary mask from segmentation
    using MaskType = itk::Image<unsigned char, 3>;
    MaskType::Pointer mask;

    /// Number of iterations performed
    int iterations = 0;

    /// Final RMS change value
    double finalRMS = 0.0;
};

/**
 * @brief Level Set segmentation for accurate boundary detection
 *
 * Provides Geodesic Active Contour and Threshold Level Set methods
 * for semi-automatic medical image segmentation with sub-pixel accuracy.
 *
 * Supported algorithms:
 * - Geodesic Active Contour: Edge-based segmentation with smoothness constraints
 * - Threshold Level Set: Intensity-based region growing with smooth boundaries
 *
 * @example
 * @code
 * LevelSetSegmenter segmenter;
 *
 * // Geodesic Active Contour
 * LevelSetParameters params;
 * params.seedPoints = {{100.0, 100.0, 50.0}};
 * params.propagationScaling = 1.0;
 * params.curvatureScaling = 0.5;
 *
 * auto result = segmenter.geodesicActiveContour(ctImage, params);
 * if (result) {
 *     auto tumorMask = result->mask;
 *     int iterations = result->iterations;
 * }
 *
 * // Threshold Level Set
 * ThresholdLevelSetParameters threshParams;
 * threshParams.seedPoints = {{100.0, 100.0, 50.0}};
 * threshParams.lowerThreshold = -100.0;
 * threshParams.upperThreshold = 200.0;
 *
 * auto threshResult = segmenter.thresholdLevelSet(ctImage, threshParams);
 * @endcode
 *
 * @trace SRS-FR-026
 */
class LevelSetSegmenter {
public:
    /// Input image type (typically CT or MRI)
    using ImageType = itk::Image<short, 3>;

    /// Float image type for intermediate processing
    using FloatImageType = itk::Image<float, 3>;

    /// Binary mask output type
    using MaskType = itk::Image<unsigned char, 3>;

    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    LevelSetSegmenter() = default;
    ~LevelSetSegmenter() = default;

    // Copyable and movable
    LevelSetSegmenter(const LevelSetSegmenter&) = default;
    LevelSetSegmenter& operator=(const LevelSetSegmenter&) = default;
    LevelSetSegmenter(LevelSetSegmenter&&) noexcept = default;
    LevelSetSegmenter& operator=(LevelSetSegmenter&&) noexcept = default;

    /**
     * @brief Apply Geodesic Active Contour Level Set segmentation
     *
     * Uses edge information to evolve the level set surface towards object
     * boundaries. Best for objects with well-defined edges.
     *
     * @param input Input 3D image
     * @param params Level Set parameters including seed points
     * @return Level Set result with mask and iteration info on success
     */
    [[nodiscard]] std::expected<LevelSetResult, SegmentationError>
    geodesicActiveContour(
        ImageType::Pointer input,
        const LevelSetParameters& params
    ) const;

    /**
     * @brief Apply Threshold Level Set segmentation
     *
     * Uses intensity thresholds to guide the level set evolution.
     * Good for homogeneous regions with known intensity ranges.
     *
     * @param input Input 3D image
     * @param params Threshold Level Set parameters
     * @return Level Set result with mask and iteration info on success
     */
    [[nodiscard]] std::expected<LevelSetResult, SegmentationError>
    thresholdLevelSet(
        ImageType::Pointer input,
        const ThresholdLevelSetParameters& params
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
        const LevelSetSeedPoint& seed
    );

    /**
     * @brief Set progress callback for long operations
     * @param callback Progress callback function
     */
    void setProgressCallback(ProgressCallback callback);

private:
    /**
     * @brief Create feature image (speed/edge potential) from input
     *
     * Applies gradient magnitude filter and sigmoid mapping
     *
     * @param input Input image
     * @param sigma Gaussian smoothing sigma
     * @return Feature image for level set evolution
     */
    [[nodiscard]] FloatImageType::Pointer createFeatureImage(
        ImageType::Pointer input,
        double sigma
    ) const;

    /**
     * @brief Create initial level set from seed points
     *
     * Creates signed distance function with negative values inside seed regions
     *
     * @param input Input image (for size/spacing information)
     * @param seedPoints Seed point locations
     * @param radius Seed sphere radius
     * @return Initial level set image
     */
    [[nodiscard]] FloatImageType::Pointer createInitialLevelSet(
        ImageType::Pointer input,
        const std::vector<LevelSetSeedPoint>& seedPoints,
        double radius
    ) const;

    /**
     * @brief Validate all seed points against image bounds
     *
     * @param input Input image
     * @param seedPoints Seed points to validate
     * @return Error if any seed is invalid, nullopt if all valid
     */
    [[nodiscard]] std::optional<SegmentationError> validateSeeds(
        ImageType::Pointer input,
        const std::vector<LevelSetSeedPoint>& seedPoints
    ) const;

    /**
     * @brief Convert float level set to binary mask
     *
     * @param levelSet Level set image (negative = inside)
     * @return Binary mask
     */
    [[nodiscard]] MaskType::Pointer levelSetToMask(
        FloatImageType::Pointer levelSet
    ) const;

    ProgressCallback progressCallback_;
};

} // namespace dicom_viewer::services

#pragma once

#include "threshold_segmenter.hpp"

#include <array>
#include <expected>
#include <vector>

#include <itkImage.h>

namespace dicom_viewer::services {

/**
 * @brief Edge-following contour tool using flood-fill and Moore boundary tracing
 *
 * Automatically traces intensity boundaries on 2D slices. Given a seed point,
 * the algorithm:
 * 1. Samples intensity at the seed and defines a tolerance band
 * 2. Flood-fills connected pixels within the band from the seed
 * 3. Extracts the ordered boundary contour using Moore neighborhood tracing
 * 4. Optionally fills the enclosed region to produce a binary mask
 *
 * @trace SRS-FR-025
 */
class LevelTracingTool {
public:
    using FloatSlice2D = itk::Image<float, 2>;
    using BinarySlice2D = itk::Image<uint8_t, 2>;
    using Point2D = std::array<double, 2>;
    using IndexPoint = std::array<int, 2>;

    /**
     * @brief Configuration for level tracing
     */
    struct Config {
        double tolerancePct = 5.0;      ///< Intensity tolerance as % of image range
        uint8_t foregroundValue = 1;
    };

    /**
     * @brief Trace the boundary contour at the intensity level of the seed point
     *
     * Performs flood-fill within the tolerance band, then extracts the
     * ordered boundary using Moore neighborhood contour tracing.
     *
     * @param slice Input 2D float slice
     * @param seedPoint Seed pixel index [x, y]
     * @param config Tracing configuration
     * @return Ordered contour points (pixel coordinates) or error
     */
    [[nodiscard]] static std::expected<std::vector<IndexPoint>, SegmentationError>
    traceContour(const FloatSlice2D* slice,
                 const IndexPoint& seedPoint,
                 const Config& config);

    /// @overload Uses default Config
    [[nodiscard]] static std::expected<std::vector<IndexPoint>, SegmentationError>
    traceContour(const FloatSlice2D* slice,
                 const IndexPoint& seedPoint);

    /**
     * @brief Convert a closed contour to a filled binary mask
     *
     * Uses scanline ray-casting to fill the interior of the contour.
     *
     * @param contour Ordered boundary points
     * @param referenceSlice Image defining output geometry
     * @return Filled binary mask or error
     */
    [[nodiscard]] static std::expected<BinarySlice2D::Pointer, SegmentationError>
    contourToMask(const std::vector<IndexPoint>& contour,
                  const FloatSlice2D* referenceSlice);

    /**
     * @brief Trace contour and fill in one step
     *
     * Combines traceContour and contourToMask. More efficient than
     * calling them separately because the flood-fill result is reused
     * directly as the mask.
     *
     * @param slice Input 2D float slice
     * @param seedPoint Seed pixel index [x, y]
     * @param config Tracing configuration
     * @return Filled binary mask or error
     */
    [[nodiscard]] static std::expected<BinarySlice2D::Pointer, SegmentationError>
    traceAndFill(const FloatSlice2D* slice,
                 const IndexPoint& seedPoint,
                 const Config& config);

    /// @overload Uses default Config
    [[nodiscard]] static std::expected<BinarySlice2D::Pointer, SegmentationError>
    traceAndFill(const FloatSlice2D* slice,
                 const IndexPoint& seedPoint);
};

}  // namespace dicom_viewer::services

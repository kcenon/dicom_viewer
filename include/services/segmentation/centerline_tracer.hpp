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

#include "threshold_segmenter.hpp"

#include <array>
#include <expected>
#include <vector>

#include <itkImage.h>

namespace dicom_viewer::services {

/**
 * @brief 3D point in physical (world) coordinates
 */
using Point3D = std::array<double, 3>;

/**
 * @brief Vessel centerline tracing using Dijkstra path finding
 *
 * Computes the optimal path between two user-specified points through
 * a 3D image volume, following high-intensity (bright-blood) or
 * low-intensity (dark-blood) vessel structures.
 *
 * Algorithm:
 * 1. Convert intensity image to cost map
 * 2. Dijkstra shortest path on 3D voxel grid (26-connectivity)
 * 3. Catmull-Rom spline smoothing to remove staircase artifacts
 * 4. Local radius estimation via radial gradient sampling
 * 5. Tubular mask generation along the smoothed centerline
 *
 * @trace SRS-FR-025
 */
class CenterlineTracer {
public:
    using FloatImage3D = itk::Image<float, 3>;
    using BinaryMaskType = itk::Image<uint8_t, 3>;

    /**
     * @brief Configuration for centerline tracing
     */
    struct TraceConfig {
        double initialRadiusMm = 5.0;  ///< Initial radius estimate for radius search
        bool brightVessels = true;      ///< True for bright-blood MRA, false for dark-blood
        double costExponent = 1.0;      ///< Higher = stronger preference for vessel interior
    };

    /**
     * @brief Result of centerline tracing
     */
    struct CenterlineResult {
        std::vector<Point3D> points;    ///< Smoothed centerline points (physical coords)
        std::vector<double> radii;      ///< Estimated vessel radius at each point (mm)
        double totalLengthMm = 0.0;     ///< Total centerline length in mm
    };

    /**
     * @brief Trace centerline between two physical points
     *
     * Uses Dijkstra shortest path on an intensity-derived cost map,
     * followed by spline smoothing and radius estimation.
     *
     * @param image Input magnitude/MRA image
     * @param startPoint Start point in physical coordinates
     * @param endPoint End point in physical coordinates
     * @param config Tracing configuration
     * @return Centerline result or error
     */
    [[nodiscard]] static std::expected<CenterlineResult, SegmentationError>
    traceCenterline(const FloatImage3D* image,
                    const Point3D& startPoint,
                    const Point3D& endPoint,
                    const TraceConfig& config);

    /// @overload Uses default TraceConfig
    [[nodiscard]] static std::expected<CenterlineResult, SegmentationError>
    traceCenterline(const FloatImage3D* image,
                    const Point3D& startPoint,
                    const Point3D& endPoint);

    /**
     * @brief Generate a tubular binary mask along a centerline
     *
     * For each voxel in the reference image, computes the minimum
     * distance to the centerline. Voxels within the radius are marked
     * as foreground.
     *
     * @param centerline Centerline with radius information
     * @param radiusOverrideMm Override radius (< 0 for per-point auto radius)
     * @param referenceImage Image defining output geometry (spacing, size, origin)
     * @return Binary mask or error
     */
    [[nodiscard]] static std::expected<BinaryMaskType::Pointer, SegmentationError>
    generateMask(const CenterlineResult& centerline,
                 double radiusOverrideMm,
                 const FloatImage3D* referenceImage);

    /**
     * @brief Estimate local vessel radius at a point
     *
     * Samples radially in multiple directions perpendicular to the
     * local tangent, detecting where intensity drops below a threshold
     * (vessel boundary).
     *
     * @param image Input intensity image
     * @param center Point on centerline (physical coords)
     * @param tangent Local tangent direction (unit vector)
     * @param maxRadiusMm Maximum search radius in mm
     * @return Estimated radius in mm
     */
    [[nodiscard]] static double estimateLocalRadius(
        const FloatImage3D* image,
        const Point3D& center,
        const Point3D& tangent,
        double maxRadiusMm = 20.0);

    /**
     * @brief Smooth a voxel-grid path using Catmull-Rom splines
     *
     * @param rawPoints Path points from Dijkstra (physical coords)
     * @param subdivisions Number of interpolated points between each pair
     * @return Smoothed path
     */
    [[nodiscard]] static std::vector<Point3D>
    smoothPath(const std::vector<Point3D>& rawPoints, int subdivisions = 3);

    /**
     * @brief Convert physical point to nearest voxel index
     *
     * @param image Reference image for coordinate transform
     * @param point Physical point
     * @param[out] index Output voxel index
     * @return True if the point is inside the image bounds
     */
    [[nodiscard]] static bool physicalToIndex(
        const FloatImage3D* image,
        const Point3D& point,
        FloatImage3D::IndexType& index);
};

}  // namespace dicom_viewer::services

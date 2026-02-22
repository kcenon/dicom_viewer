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


/**
 * @file level_tracing_tool.hpp
 * @brief Edge-following contour tool using flood-fill and Moore boundary tracing
 * @details Automatically traces intensity boundaries on 2D slices using
 *          flood-fill within tolerance band and Moore neighborhood contour
 *          extraction. Optionally fills enclosed regions to produce binary masks.
 *
 * @author kcenon
 * @since 1.0.0
 */
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

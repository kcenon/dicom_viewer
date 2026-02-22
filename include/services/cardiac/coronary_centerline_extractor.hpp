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
#include <memory>
#include <vector>

#include <itkImage.h>

#include "services/cardiac/cardiac_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Coronary artery centerline extraction using Frangi vesselness and minimal path
 *
 * Implements a complete pipeline for coronary CTA analysis:
 * 1. Multi-scale Frangi vesselness filter for tubular structure enhancement
 * 2. Minimal path centerline extraction using FastMarching + gradient descent
 * 3. B-spline centerline smoothing
 * 4. Vessel radius estimation and stenosis measurement
 *
 * @example
 * @code
 * CoronaryCenterlineExtractor extractor;
 * auto vesselness = extractor.computeVesselness(cardiacVolume);
 * if (vesselness) {
 *     auto centerline = extractor.extractCenterline(
 *         seedPoint, vesselness.value(), cardiacVolume);
 *     if (centerline) {
 *         auto& result = centerline.value();
 *         std::cout << "Length: " << result.totalLength << " mm" << std::endl;
 *     }
 * }
 * @endcode
 *
 * @trace SRS-FR-051, SDS-MOD-009
 */
class CoronaryCenterlineExtractor {
public:
    CoronaryCenterlineExtractor();
    ~CoronaryCenterlineExtractor();

    CoronaryCenterlineExtractor(const CoronaryCenterlineExtractor&) = delete;
    CoronaryCenterlineExtractor& operator=(const CoronaryCenterlineExtractor&) = delete;
    CoronaryCenterlineExtractor(CoronaryCenterlineExtractor&&) noexcept;
    CoronaryCenterlineExtractor& operator=(CoronaryCenterlineExtractor&&) noexcept;

    /**
     * @brief Compute multi-scale Frangi vesselness response
     *
     * For each scale sigma in [sigmaMin, sigmaMax]:
     *   1. Apply Hessian at scale sigma
     *   2. Compute eigenvalues and vesselness response
     * Maximum response across all scales is returned.
     *
     * @param image Input CT volume (short pixel type)
     * @param params Vesselness filter parameters
     * @return Float vesselness image (0.0 = non-vessel, 1.0 = strong vessel), or error
     * @trace SRS-FR-051
     */
    [[nodiscard]] std::expected<itk::Image<float, 3>::Pointer, CardiacError>
    computeVesselness(itk::Image<short, 3>::Pointer image,
                      const VesselnessParams& params = VesselnessParams{}) const;

    /**
     * @brief Extract centerline from seed point using minimal path
     *
     * Uses vesselness image as speed function for FastMarching,
     * then backtracks from endpoint to seed via gradient descent
     * on the arrival time field.
     *
     * @param seedPoint Start point (physical coordinates in mm)
     * @param endPoint End point (physical coordinates in mm)
     * @param vesselness Vesselness response image from computeVesselness()
     * @param originalImage Original CT volume for radius estimation
     * @return CenterlineResult with ordered path points, or error
     * @trace SRS-FR-051
     */
    [[nodiscard]] std::expected<CenterlineResult, CardiacError>
    extractCenterline(const std::array<double, 3>& seedPoint,
                      const std::array<double, 3>& endPoint,
                      itk::Image<float, 3>::Pointer vesselness,
                      itk::Image<short, 3>::Pointer originalImage) const;

    /**
     * @brief Smooth centerline with B-spline fitting
     *
     * Fits a cubic B-spline to the raw centerline points and
     * resamples at uniform arc-length intervals.
     *
     * @param rawPath Raw centerline points from extractCenterline()
     * @param controlPointCount Number of B-spline control points
     * @return Smoothed centerline points with recomputed tangent/normal
     */
    [[nodiscard]] std::vector<CenterlinePoint>
    smoothCenterline(const std::vector<CenterlinePoint>& rawPath,
                     int controlPointCount = 50) const;

    /**
     * @brief Estimate vessel radius at each centerline point
     *
     * Casts rays perpendicular to the tangent direction and measures
     * the vessel boundary using the half-maximum intensity criterion.
     *
     * @param points Centerline points (modified in-place with radius)
     * @param image Original CT volume
     */
    void estimateRadii(std::vector<CenterlinePoint>& points,
                       itk::Image<short, 3>::Pointer image) const;

    /**
     * @brief Measure stenosis along the centerline
     *
     * Computes minimum lumen diameter, proximal reference diameter,
     * and stenosis percentage: (1 - Dmin/Dref) * 100.
     *
     * @param result CenterlineResult to update (modified in-place)
     * @param image Original CT volume
     */
    void measureStenosis(CenterlineResult& result,
                         itk::Image<short, 3>::Pointer image) const;

    /**
     * @brief Compute total arc length of a centerline
     *
     * @param points Ordered centerline points
     * @return Total length in mm
     */
    [[nodiscard]] static double computeLength(
        const std::vector<CenterlinePoint>& points);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

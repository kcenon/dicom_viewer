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
 * @file curved_planar_reformatter.hpp
 * @brief Curved Planar Reformation (CPR) view generator from vessel centerlines
 * @details Generates straightened CPR, cross-sectional CPR, and stretched CPR
 *          views from vessel centerline and CT volume. Essential for
 *          coronary CTA visualization allowing complete vessel inspection
 *          in single 2D image despite tortuous 3D path.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <expected>
#include <memory>
#include <vector>

#include <itkImage.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include "services/cardiac/cardiac_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Curved Planar Reformation (CPR) view generator
 *
 * Generates three types of CPR views from a vessel centerline and CT volume:
 * - Straightened CPR: unfolds the vessel onto a flat 2D plane
 * - Cross-sectional CPR: perpendicular slices at regular intervals
 * - Stretched CPR: preserves proportional arc-length distances
 *
 * CPR views are essential for coronary CTA analysis, allowing the entire
 * vessel to be visualized in a single 2D image despite its tortuous 3D path.
 *
 * @example
 * @code
 * CurvedPlanarReformatter cpr;
 * auto straightened = cpr.generateStraightenedCPR(centerline, volume);
 * if (straightened) {
 *     auto imageData = straightened.value();
 *     // Display in VTK viewer...
 * }
 * @endcode
 *
 * @trace SRS-FR-051, SDS-MOD-009
 */
class CurvedPlanarReformatter {
public:
    CurvedPlanarReformatter();
    ~CurvedPlanarReformatter();

    CurvedPlanarReformatter(const CurvedPlanarReformatter&) = delete;
    CurvedPlanarReformatter& operator=(const CurvedPlanarReformatter&) = delete;
    CurvedPlanarReformatter(CurvedPlanarReformatter&&) noexcept;
    CurvedPlanarReformatter& operator=(CurvedPlanarReformatter&&) noexcept;

    /**
     * @brief Generate straightened CPR view
     *
     * Samples the volume along the centerline, creating a 2D image
     * where the x-axis is perpendicular width and y-axis is arc length.
     *
     * @param centerline Centerline with Frenet frame
     * @param volume CT volume to sample from
     * @param samplingWidth Half-width of sampling plane in mm
     * @param samplingResolution Pixel size in mm
     * @return 2D vtkImageData (straightened CPR), or error
     * @trace SRS-FR-051
     */
    [[nodiscard]] std::expected<vtkSmartPointer<vtkImageData>, CardiacError>
    generateStraightenedCPR(const CenterlineResult& centerline,
                            itk::Image<short, 3>::Pointer volume,
                            double samplingWidth = 20.0,
                            double samplingResolution = 0.5) const;

    /**
     * @brief Generate cross-sectional CPR views
     *
     * Produces a set of 2D images representing perpendicular cross-sections
     * of the vessel at regular intervals along the centerline.
     *
     * @param centerline Centerline with Frenet frame
     * @param volume CT volume to sample from
     * @param interval Distance between cross-sections in mm
     * @param crossSectionSize Half-width of cross-section in mm
     * @param samplingResolution Pixel size in mm
     * @return Vector of 2D vtkImageData cross-sections, or error
     * @trace SRS-FR-051
     */
    [[nodiscard]] std::expected<std::vector<vtkSmartPointer<vtkImageData>>, CardiacError>
    generateCrossSectionalCPR(const CenterlineResult& centerline,
                              itk::Image<short, 3>::Pointer volume,
                              double interval = 1.0,
                              double crossSectionSize = 10.0,
                              double samplingResolution = 0.5) const;

    /**
     * @brief Generate stretched CPR view
     *
     * Similar to straightened CPR but preserves proportional distances
     * along the vessel length. The output is a 2D image where the y-axis
     * represents proportional arc length.
     *
     * @param centerline Centerline with Frenet frame
     * @param volume CT volume to sample from
     * @param samplingWidth Half-width of sampling plane in mm
     * @param samplingResolution Pixel size in mm
     * @return 2D vtkImageData (stretched CPR), or error
     * @trace SRS-FR-051
     */
    [[nodiscard]] std::expected<vtkSmartPointer<vtkImageData>, CardiacError>
    generateStretchedCPR(const CenterlineResult& centerline,
                         itk::Image<short, 3>::Pointer volume,
                         double samplingWidth = 20.0,
                         double samplingResolution = 0.5) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

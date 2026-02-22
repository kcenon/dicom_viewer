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

#include <array>
#include <expected>
#include <memory>
#include <vector>

#include <vtkSmartPointer.h>

#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/velocity_field_assembler.hpp"

class vtkPolyData;
class vtkImageData;
class vtkLookupTable;

namespace dicom_viewer::services {

/**
 * @brief Visualization type for velocity field rendering
 *
 * @trace SRS-FR-046
 */
enum class VisualizationType {
    Streamlines,   ///< Instantaneous flow trajectories (tangent to velocity)
    Pathlines,     ///< Time-resolved particle paths across phases
    VectorGlyphs   ///< Arrow markers at discrete sample points
};

/**
 * @brief Color mapping mode for velocity visualization
 *
 * @trace SRS-FR-046
 */
enum class ColorMode {
    VelocityMagnitude,  ///< |V| mapped to rainbow colormap [0, VENC]
    VelocityComponent,  ///< Single component with diverging colormap [-VENC, VENC]
    FlowDirection,      ///< RGB-encoded direction
    TriggerTime         ///< Time from R-wave with sequential colormap
};

/**
 * @brief Parameters for streamline generation
 */
struct StreamlineParams {
    int maxSeedPoints = 5000;
    double stepLength = 0.5;         ///< Integration step in mm
    int maxSteps = 2000;
    double terminalSpeed = 0.1;      ///< Stop threshold in cm/s
    double tubeRadius = 0.5;         ///< Tube filter radius in mm
    int tubeSides = 8;
};

/**
 * @brief Parameters for vector glyph rendering
 */
struct GlyphParams {
    double scaleFactor = 1.0;
    int skipFactor = 4;              ///< Sample every Nth voxel
    double minMagnitude = 1.0;       ///< Minimum velocity threshold in cm/s
};

/**
 * @brief Parameters for pathline generation
 */
struct PathlineParams {
    int maxSeedPoints = 1000;
    int maxSteps = 2000;
    double terminalSpeed = 0.1;      ///< cm/s
    double tubeRadius = 0.5;         ///< mm
    int tubeSides = 8;
};

/**
 * @brief Seed region for streamline and pathline origins
 */
struct SeedRegion {
    enum class Type { Plane, Volume, Points };
    Type type = Type::Volume;
    std::array<double, 6> bounds = {0, 0, 0, 0, 0, 0};       ///< xmin,xmax,ymin,ymax,zmin,zmax
    std::array<double, 3> planeOrigin = {0, 0, 0};
    std::array<double, 3> planeNormal = {0, 0, 1};
    double planeRadius = 50.0;
    int numSeedPoints = 5000;
};

/**
 * @brief Flow visualization pipeline for 4D Flow MRI velocity data
 *
 * Renders velocity vector fields as streamlines, pathlines, and vector
 * glyphs using VTK visualization pipeline. Supports 4 color mapping modes
 * for encoding velocity magnitude, component, direction, or trigger time.
 *
 * This is a service-layer class without Qt dependency. The VTK pipeline
 * produces vtkPolyData output that can be attached to any VTK renderer.
 *
 * Pipeline Architecture:
 * @code
 * ITK VectorImage → vtkImageData (via velocityFieldToVTK)
 *                        ↓
 *         ┌──────────────┼──────────────┐
 *     Streamlines    Pathlines     VectorGlyphs
 *   (vtkStreamTracer) (RK4 manual)  (vtkGlyph3D)
 *         ↓              ↓              ↓
 *    vtkTubeFilter    vtkPolyLine   vtkArrowSource
 *         ↓              ↓              ↓
 *      vtkPolyData    vtkPolyData   vtkPolyData
 * @endcode
 *
 * @trace SRS-FR-046
 */
class FlowVisualizer {
public:
    FlowVisualizer();
    ~FlowVisualizer();

    // Non-copyable, movable
    FlowVisualizer(const FlowVisualizer&) = delete;
    FlowVisualizer& operator=(const FlowVisualizer&) = delete;
    FlowVisualizer(FlowVisualizer&&) noexcept;
    FlowVisualizer& operator=(FlowVisualizer&&) noexcept;

    /**
     * @brief Set velocity field for visualization
     * @param phase Assembled velocity phase with 3-component vector field
     * @return void on success, FlowError if velocity field is null or invalid
     */
    std::expected<void, FlowError> setVelocityField(const VelocityPhase& phase);

    /**
     * @brief Set seed region for streamline/pathline origins
     * @param region Seed region configuration
     */
    void setSeedRegion(const SeedRegion& region);

    /**
     * @brief Generate streamlines from current velocity field
     *
     * Uses vtkStreamTracer with RK45 integrator and vtkTubeFilter
     * for 3D tube rendering of flow trajectories.
     *
     * @param params Streamline generation parameters
     * @return PolyData with streamline geometry, or FlowError
     */
    [[nodiscard]] std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
    generateStreamlines(const StreamlineParams& params = {}) const;

    /**
     * @brief Generate pathlines across multiple cardiac phases
     *
     * Traces particle motion through temporal velocity fields using
     * Euler integration across phases. Each seed point produces one
     * polyline connecting positions across time.
     *
     * @param allPhases Vector of all cardiac phases
     * @param params Pathline generation parameters
     * @return PolyData with pathline geometry, or FlowError
     */
    [[nodiscard]] std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
    generatePathlines(const std::vector<VelocityPhase>& allPhases,
                      const PathlineParams& params = {}) const;

    /**
     * @brief Generate vector glyphs from current velocity field
     *
     * Subsamples the velocity field and places oriented arrow glyphs
     * at each sample point, scaled by velocity magnitude.
     *
     * @param params Glyph generation parameters
     * @return PolyData with glyph geometry, or FlowError
     */
    [[nodiscard]] std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
    generateGlyphs(const GlyphParams& params = {}) const;

    /**
     * @brief Set color mapping mode
     */
    void setColorMode(ColorMode mode);

    /**
     * @brief Set velocity range for color mapping
     * @param minVel Minimum velocity (cm/s)
     * @param maxVel Maximum velocity (cm/s)
     */
    void setVelocityRange(double minVel, double maxVel);

    /**
     * @brief Create VTK lookup table for current color mode
     * @return Configured lookup table
     */
    [[nodiscard]] vtkSmartPointer<vtkLookupTable> createLookupTable() const;

    // --- State queries ---

    [[nodiscard]] bool hasVelocityField() const;
    [[nodiscard]] ColorMode colorMode() const;
    [[nodiscard]] SeedRegion seedRegion() const;

    // --- Utility ---

    /**
     * @brief Convert ITK VectorImage to VTK ImageData
     *
     * Copies the 3-component velocity data from ITK VectorImage into
     * a vtkImageData with vectors set as active point vectors and
     * magnitude as active scalars.
     *
     * @param phase Velocity phase with ITK vector image
     * @return vtkImageData with velocity vectors, or FlowError
     */
    [[nodiscard]] static std::expected<vtkSmartPointer<vtkImageData>, FlowError>
    velocityFieldToVTK(const VelocityPhase& phase);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

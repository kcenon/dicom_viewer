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
 * @file streamline_overlay_renderer.hpp
 * @brief 2D streamline overlay rendering for flow visualization on MPR views
 * @details Generates and renders 2D streamlines from 3D velocity field data
 *          projected onto MPR slice planes. Supports configurable
 *          streamline density, length, and color mapping for
 *          flow visualization.
 *
 * ## Thread Safety
 * - All rendering operations must be called from the main (UI) thread
 * - Streamline computation may be offloaded to background threads
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <array>
#include <expected>
#include <memory>

#include <vtkSmartPointer.h>

class vtkImageData;
class vtkRenderer;
class vtkPolyData;

namespace dicom_viewer::services {

enum class MPRPlane;
enum class OverlayError;

/**
 * @brief Parameters for 2D streamline generation on a slice plane
 */
struct Streamline2DParams {
    int numSeedPoints = 200;          ///< Number of seed points on the slice
    double stepLength = 0.5;          ///< Integration step in mm
    int maxSteps = 500;               ///< Maximum integration steps per streamline
    double terminalSpeed = 0.01;      ///< Stop when velocity drops below this (cm/s)
    double lineWidth = 1.5;           ///< Rendered line width in pixels
};

/**
 * @brief Parameters for LIC (Line Integral Convolution) texture generation
 */
struct LICParams {
    int kernelLength = 20;            ///< Number of steps forward+backward for convolution
    double stepSize = 0.5;            ///< Euler integration step in pixels
    unsigned int noiseSeed = 42;      ///< Random seed for reproducible noise texture
};

/**
 * @brief 2D streamline and LIC overlay renderer for MPR views
 *
 * Renders velocity vector fields as 2D streamlines or Line Integral
 * Convolution (LIC) textures on MPR slice planes. Unlike the scalar
 * HemodynamicOverlayRenderer, this processes vector data to produce
 * flow visualization overlays.
 *
 * Pipeline (Streamlines):
 * @code
 *   3D velocity field (vtkImageData, 3-component)
 *     → extract 2D in-plane velocity at slice position
 *     → vtkStreamTracer (2D integration)
 *     → vtkPolyDataMapper (velocity-coded colors)
 *     → vtkActor (line overlay)
 * @endcode
 *
 * Pipeline (LIC):
 * @code
 *   3D velocity field (vtkImageData, 3-component)
 *     → extract 2D in-plane velocity at slice position
 *     → Line Integral Convolution (noise texture + streamline averaging)
 *     → vtkImageActor (grayscale overlay)
 * @endcode
 *
 * @trace SRS-FR-046
 */
class StreamlineOverlayRenderer {
public:
    /**
     * @brief Rendering mode for the overlay
     */
    enum class Mode {
        Streamline,      ///< 2D streamlines colored by velocity
        LIC              ///< Line Integral Convolution texture
    };

    StreamlineOverlayRenderer();
    ~StreamlineOverlayRenderer();

    // Non-copyable, movable
    StreamlineOverlayRenderer(const StreamlineOverlayRenderer&) = delete;
    StreamlineOverlayRenderer& operator=(const StreamlineOverlayRenderer&) = delete;
    StreamlineOverlayRenderer(StreamlineOverlayRenderer&&) noexcept;
    StreamlineOverlayRenderer& operator=(StreamlineOverlayRenderer&&) noexcept;

    // ==================== Input Data ====================

    /**
     * @brief Set the 3D velocity field for streamline/LIC rendering
     * @param velocityField 3D vtkImageData with 3-component vectors
     */
    void setVelocityField(vtkSmartPointer<vtkImageData> velocityField);

    /**
     * @brief Check if a velocity field has been set
     */
    [[nodiscard]] bool hasVelocityField() const noexcept;

    // ==================== Settings ====================

    /**
     * @brief Set rendering mode (Streamline or LIC)
     */
    void setMode(Mode mode);

    /**
     * @brief Get current mode
     */
    [[nodiscard]] Mode mode() const noexcept;

    /**
     * @brief Set overlay visibility
     */
    void setVisible(bool visible);

    /**
     * @brief Get visibility state
     */
    [[nodiscard]] bool isVisible() const noexcept;

    /**
     * @brief Set overlay opacity (0.0 = transparent, 1.0 = opaque)
     */
    void setOpacity(double opacity);

    /**
     * @brief Get opacity
     */
    [[nodiscard]] double opacity() const noexcept;

    /**
     * @brief Set streamline generation parameters
     */
    void setStreamlineParams(const Streamline2DParams& params);

    /**
     * @brief Set LIC parameters
     */
    void setLICParams(const LICParams& params);

    // ==================== Rendering ====================

    /**
     * @brief Set VTK renderers for the three MPR planes
     */
    void setRenderers(vtkSmartPointer<vtkRenderer> axial,
                      vtkSmartPointer<vtkRenderer> coronal,
                      vtkSmartPointer<vtkRenderer> sagittal);

    /**
     * @brief Set slice position for a specific plane
     * @return Success or OverlayError
     */
    [[nodiscard]] std::expected<void, OverlayError>
    setSlicePosition(MPRPlane plane, double worldPosition);

    /**
     * @brief Regenerate and update all planes
     */
    void update();

    /**
     * @brief Regenerate and update a specific plane
     */
    void updatePlane(MPRPlane plane);

    // ==================== Static Utilities ====================

    /**
     * @brief Extract 2D in-plane velocity from a 3D velocity field at a slice
     *
     * For Axial: extracts (Vx, Vy) at given Z position
     * For Coronal: extracts (Vx, Vz) at given Y position
     * For Sagittal: extracts (Vy, Vz) at given X position
     *
     * @param velocityField 3D velocity field (3-component)
     * @param plane MPR plane
     * @param worldPosition Slice position in world coordinates
     * @return 2D vtkImageData with 3-component vectors (in-plane + zero)
     */
    [[nodiscard]] static std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
    extractSliceVelocity(vtkSmartPointer<vtkImageData> velocityField,
                         MPRPlane plane, double worldPosition);

    /**
     * @brief Generate 2D streamlines from a 2D velocity field
     *
     * Uses uniform grid seeding and vtkStreamTracer for integration.
     *
     * @param velocitySlice 2D velocity field from extractSliceVelocity()
     * @param params Streamline parameters
     * @return PolyData with streamline geometry, or error
     */
    [[nodiscard]] static std::expected<vtkSmartPointer<vtkPolyData>, OverlayError>
    generateStreamlines2D(vtkSmartPointer<vtkImageData> velocitySlice,
                          const Streamline2DParams& params = {});

    /**
     * @brief Compute Line Integral Convolution texture from a 2D velocity field
     *
     * Creates a grayscale texture showing flow direction patterns by
     * convolving a white noise image along streamlines.
     *
     * @param velocitySlice 2D velocity field from extractSliceVelocity()
     * @param params LIC parameters
     * @return Grayscale vtkImageData texture, or error
     */
    [[nodiscard]] static std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
    computeLIC(vtkSmartPointer<vtkImageData> velocitySlice,
               const LICParams& params = {});

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

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
 * @file volume_renderer.hpp
 * @brief GPU-accelerated volume rendering with transfer function support
 * @details Provides the VolumeRenderer class for ray-casting volume visualization
 *          using VTK. Supports GPU rendering with CPU fallback, multiple
 *          blend modes (composite, MIP, MinIP, average), interactive LOD,
 *          and clipping planes. Includes built-in CT/MRI presets.
 *
 * ## Thread Safety
 * - All rendering operations must be called from the main (UI) thread
 * - Transfer function and window/level updates are not thread-safe
 * - Input data (vtkImageData) should not be modified during rendering
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <tuple>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkVolume.h>

class vtkColorTransferFunction;
class vtkPiecewiseFunction;

namespace dicom_viewer::services {

/**
 * @brief Transfer function preset for volume rendering
 */
struct TransferFunctionPreset {
    std::string name;
    double windowWidth;
    double windowCenter;
    // Color points: (scalar value, R, G, B)
    std::vector<std::tuple<double, double, double, double>> colorPoints;
    // Opacity points: (scalar value, opacity)
    std::vector<std::pair<double, double>> opacityPoints;
    // Gradient opacity points for edge enhancement
    std::vector<std::pair<double, double>> gradientOpacityPoints;
};

/**
 * @brief Rendering mode for volume visualization
 */
enum class BlendMode {
    Composite,      // Default compositing
    MaximumIntensity,  // MIP
    MinimumIntensity,  // MinIP
    Average         // Average intensity
};

/**
 * @brief GPU-accelerated volume renderer using VTK
 *
 * Implements ray casting volume rendering with GPU acceleration.
 * Falls back to CPU rendering when GPU is not available.
 *
 * @trace SRS-FR-005, SRS-FR-006, SRS-FR-007
 */
class VolumeRenderer {
public:
    VolumeRenderer();
    ~VolumeRenderer();

    // Non-copyable, movable
    VolumeRenderer(const VolumeRenderer&) = delete;
    VolumeRenderer& operator=(const VolumeRenderer&) = delete;
    VolumeRenderer(VolumeRenderer&&) noexcept;
    VolumeRenderer& operator=(VolumeRenderer&&) noexcept;

    /**
     * @brief Set the input volume data
     * @param imageData VTK image data (3D volume)
     */
    void setInputData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Get the VTK volume actor
     */
    vtkSmartPointer<vtkVolume> getVolume() const;

    /**
     * @brief Apply a preset transfer function
     * @param preset Transfer function preset
     */
    void applyPreset(const TransferFunctionPreset& preset);

    /**
     * @brief Set window/level (for convenience)
     * @param width Window width
     * @param center Window center
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Set blend mode
     * @param mode Rendering blend mode
     */
    void setBlendMode(BlendMode mode);

    /**
     * @brief Enable/disable GPU rendering
     * @param enable True to enable GPU rendering
     * @return True if GPU rendering is available and enabled
     */
    bool setGPURenderingEnabled(bool enable);

    /**
     * @brief Check if GPU rendering is being used
     */
    bool isGPURenderingEnabled() const;

    /**
     * @brief Validate GPU support with render window
     * @param renderWindow VTK render window for GPU capability check
     * @return True if GPU rendering is supported
     *
     * This method checks if the current GPU supports volume ray casting.
     * If GPU is not supported, automatically falls back to CPU rendering.
     */
    bool validateGPUSupport(vtkSmartPointer<vtkRenderWindow> renderWindow);

    /**
     * @brief Enable LOD (Level of Detail) during interaction
     * @param enable True to enable LOD
     */
    void setInteractiveLODEnabled(bool enable);

    /**
     * @brief Set clipping planes for volume cropping
     * @param planes Array of 6 plane values [xmin, xmax, ymin, ymax, zmin, zmax]
     */
    void setClippingPlanes(const std::array<double, 6>& planes);

    /**
     * @brief Clear clipping planes
     */
    void clearClippingPlanes();

    /**
     * @brief Update rendering (call after changes)
     */
    void update();

    // Built-in presets
    static TransferFunctionPreset getPresetCTBone();
    static TransferFunctionPreset getPresetCTSoftTissue();
    static TransferFunctionPreset getPresetCTLung();
    static TransferFunctionPreset getPresetCTAngio();
    static TransferFunctionPreset getPresetCTAbdomen();
    static TransferFunctionPreset getPresetMRIDefault();

    // ==================== Scalar Overlay Volumes ====================

    /**
     * @brief Add a scalar overlay volume with custom transfer functions
     *
     * Each overlay is rendered as an independent vtkVolume with its own
     * color and opacity transfer functions. Multiple overlays can coexist.
     *
     * @param name Unique identifier for this overlay
     * @param scalarField 3D scalar data (e.g., velocity magnitude)
     * @param colorTF Color transfer function mapping scalars to RGB
     * @param opacityTF Opacity transfer function mapping scalars to alpha
     */
    void addScalarOverlay(const std::string& name,
                          vtkSmartPointer<vtkImageData> scalarField,
                          vtkSmartPointer<vtkColorTransferFunction> colorTF,
                          vtkSmartPointer<vtkPiecewiseFunction> opacityTF);

    /**
     * @brief Remove a scalar overlay by name
     * @param name Overlay identifier
     * @return True if overlay was found and removed
     */
    bool removeScalarOverlay(const std::string& name);

    /**
     * @brief Remove all scalar overlays
     */
    void removeAllScalarOverlays();

    /**
     * @brief Check if an overlay exists
     * @param name Overlay identifier
     */
    [[nodiscard]] bool hasOverlay(const std::string& name) const;

    /**
     * @brief Get all overlay names
     * @return Vector of overlay identifiers
     */
    [[nodiscard]] std::vector<std::string> overlayNames() const;

    /**
     * @brief Set overlay visibility
     * @param name Overlay identifier
     * @param visible True to show overlay
     */
    void setOverlayVisible(const std::string& name, bool visible);

    /**
     * @brief Set overlay opacity scaling factor
     * @param name Overlay identifier
     * @param opacity Global opacity multiplier (0.0-1.0)
     */
    void setOverlayOpacity(const std::string& name, double opacity);

    /**
     * @brief Get the VTK volume actor for an overlay
     * @param name Overlay identifier
     * @return Volume actor, or nullptr if not found
     */
    [[nodiscard]] vtkSmartPointer<vtkVolume> getOverlayVolume(const std::string& name) const;

    /**
     * @brief Update transfer functions for an existing overlay
     * @param name Overlay identifier
     * @param colorTF New color transfer function
     * @param opacityTF New opacity transfer function
     * @return True if overlay was found and updated
     */
    bool updateOverlayTransferFunctions(
        const std::string& name,
        vtkSmartPointer<vtkColorTransferFunction> colorTF,
        vtkSmartPointer<vtkPiecewiseFunction> opacityTF);

    // ==================== Convenience: Velocity Overlay ====================

    /**
     * @brief Create a jet colormap color transfer function for velocity
     * @param maxVelocity Maximum velocity in cm/s (maps to red)
     * @return Configured color transfer function
     */
    [[nodiscard]] static vtkSmartPointer<vtkColorTransferFunction>
    createVelocityColorFunction(double maxVelocity);

    /**
     * @brief Create an opacity transfer function for velocity overlay
     * @param maxVelocity Maximum velocity in cm/s
     * @param baseOpacity Base opacity for visible regions (0.0-1.0)
     * @return Configured opacity transfer function
     */
    [[nodiscard]] static vtkSmartPointer<vtkPiecewiseFunction>
    createVelocityOpacityFunction(double maxVelocity, double baseOpacity = 0.3);

    // ==================== Convenience: Vorticity Overlay ====================

    /**
     * @brief Create a blue-white-red colormap for vorticity magnitude
     * @param maxVorticity Maximum vorticity in 1/s
     * @return Configured color transfer function
     */
    [[nodiscard]] static vtkSmartPointer<vtkColorTransferFunction>
    createVorticityColorFunction(double maxVorticity);

    /**
     * @brief Create an opacity transfer function for vorticity overlay
     * @param maxVorticity Maximum vorticity in 1/s
     * @param baseOpacity Base opacity for visible regions (0.0-1.0)
     * @return Configured opacity transfer function
     */
    [[nodiscard]] static vtkSmartPointer<vtkPiecewiseFunction>
    createVorticityOpacityFunction(double maxVorticity, double baseOpacity = 0.3);

    // ==================== Convenience: Energy Loss Overlay ====================

    /**
     * @brief Create a hot metal colormap for energy loss (viscous dissipation)
     * @param maxEnergyLoss Maximum energy loss in W/m^3
     * @return Configured color transfer function
     */
    [[nodiscard]] static vtkSmartPointer<vtkColorTransferFunction>
    createEnergyLossColorFunction(double maxEnergyLoss);

    /**
     * @brief Create an opacity transfer function for energy loss overlay
     * @param maxEnergyLoss Maximum energy loss in W/m^3
     * @param baseOpacity Base opacity for visible regions (0.0-1.0)
     * @return Configured opacity transfer function
     */
    [[nodiscard]] static vtkSmartPointer<vtkPiecewiseFunction>
    createEnergyLossOpacityFunction(double maxEnergyLoss, double baseOpacity = 0.3);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

#pragma once

#include <array>
#include <expected>
#include <functional>
#include <memory>
#include <string>

#include <vtkSmartPointer.h>

class vtkImageData;
class vtkRenderer;
class vtkLookupTable;

namespace dicom_viewer::services {

enum class MPRPlane;

/**
 * @brief Error codes for hemodynamic overlay operations
 */
enum class OverlayError {
    NoScalarField,
    InvalidSliceIndex,
    InvalidPlane,
    ResliceFailed,
    InternalError
};

/**
 * @brief Type of hemodynamic overlay to render
 */
enum class OverlayType {
    VelocityMagnitude,  ///< Speed = sqrt(Vx^2 + Vy^2 + Vz^2) in cm/s
    VelocityX,          ///< X component of velocity
    VelocityY,          ///< Y component of velocity
    VelocityZ,          ///< Z component of velocity
    Vorticity,          ///< |curl(V)| vorticity magnitude in 1/s
    EnergyLoss          ///< Viscous dissipation rate in W/m^3
};

/**
 * @brief Built-in colormap presets for hemodynamic overlays
 */
enum class ColormapPreset {
    Jet,           ///< Blue-cyan-green-yellow-red (default for magnitude)
    HotMetal,      ///< Black-red-yellow-white
    CoolWarm,      ///< Blue-white-red (diverging, for signed data)
    Viridis        ///< Perceptually uniform sequential
};

/**
 * @brief 2D hemodynamic overlay renderer for MPR views
 *
 * Extracts scalar fields (velocity magnitude, components) from 3D
 * vtkImageData volumes and renders them as color-mapped, alpha-blended
 * overlays on top of anatomical MPR slices.
 *
 * Pipeline:
 * @code
 *   3D vtkImageData (scalar field)
 *     → vtkImageReslice (extract 2D slice matching MPR plane/position)
 *     → vtkLookupTable (colormap: scalar → RGBA)
 *     → vtkImageMapToColors
 *     → vtkImageActor (alpha-blended overlay)
 * @endcode
 *
 * @trace SRS-FR-046
 */
class HemodynamicOverlayRenderer {
public:
    HemodynamicOverlayRenderer();
    ~HemodynamicOverlayRenderer();

    // Non-copyable, movable
    HemodynamicOverlayRenderer(const HemodynamicOverlayRenderer&) = delete;
    HemodynamicOverlayRenderer& operator=(const HemodynamicOverlayRenderer&) = delete;
    HemodynamicOverlayRenderer(HemodynamicOverlayRenderer&&) noexcept;
    HemodynamicOverlayRenderer& operator=(HemodynamicOverlayRenderer&&) noexcept;

    // ==================== Input Data ====================

    /**
     * @brief Set the 3D scalar field for overlay rendering
     *
     * The scalar field should contain the hemodynamic parameter to visualize
     * (e.g., velocity magnitude computed from VectorImage3D).
     *
     * @param scalarField 3D vtkImageData with scalar values
     */
    void setScalarField(vtkSmartPointer<vtkImageData> scalarField);

    /**
     * @brief Check if a scalar field has been set
     */
    [[nodiscard]] bool hasScalarField() const noexcept;

    // ==================== Overlay Settings ====================

    /**
     * @brief Set overlay type
     * @param type Overlay type (determines how the field is interpreted)
     */
    void setOverlayType(OverlayType type);

    /**
     * @brief Get current overlay type
     */
    [[nodiscard]] OverlayType overlayType() const noexcept;

    /**
     * @brief Set overlay visibility
     * @param visible True to show overlay
     */
    void setVisible(bool visible);

    /**
     * @brief Check if overlay is visible
     */
    [[nodiscard]] bool isVisible() const noexcept;

    /**
     * @brief Set overlay opacity for alpha blending
     * @param opacity Opacity value (0.0 = transparent, 1.0 = opaque)
     */
    void setOpacity(double opacity);

    /**
     * @brief Get overlay opacity
     */
    [[nodiscard]] double opacity() const noexcept;

    // ==================== Colormap ====================

    /**
     * @brief Apply a colormap preset
     * @param preset Colormap preset to use
     */
    void setColormapPreset(ColormapPreset preset);

    /**
     * @brief Get current colormap preset
     */
    [[nodiscard]] ColormapPreset colormapPreset() const noexcept;

    /**
     * @brief Set scalar range for colormap mapping
     * @param minVal Minimum scalar value (mapped to colormap start)
     * @param maxVal Maximum scalar value (mapped to colormap end)
     */
    void setScalarRange(double minVal, double maxVal);

    /**
     * @brief Get current scalar range
     * @return {min, max}
     */
    [[nodiscard]] std::pair<double, double> scalarRange() const noexcept;

    /**
     * @brief Get the VTK lookup table used for colormapping
     * @return Configured lookup table
     */
    [[nodiscard]] vtkSmartPointer<vtkLookupTable> getLookupTable() const;

    // ==================== Rendering ====================

    /**
     * @brief Set VTK renderers for the three MPR planes
     * @param axial Renderer for axial plane
     * @param coronal Renderer for coronal plane
     * @param sagittal Renderer for sagittal plane
     */
    void setRenderers(vtkSmartPointer<vtkRenderer> axial,
                      vtkSmartPointer<vtkRenderer> coronal,
                      vtkSmartPointer<vtkRenderer> sagittal);

    /**
     * @brief Set slice position for a specific plane
     * @param plane MPR plane
     * @param worldPosition Slice position in world coordinates (mm)
     * @return Success or OverlayError
     */
    [[nodiscard]] std::expected<void, OverlayError>
    setSlicePosition(MPRPlane plane, double worldPosition);

    /**
     * @brief Update rendering for all planes
     */
    void update();

    /**
     * @brief Update rendering for a specific plane
     * @param plane MPR plane to update
     */
    void updatePlane(MPRPlane plane);

    // ==================== Utility ====================

    /**
     * @brief Compute velocity magnitude from a 3-component velocity field
     *
     * Creates a scalar vtkImageData where each voxel = sqrt(Vx^2 + Vy^2 + Vz^2).
     *
     * @param velocityField 3D vtkImageData with 3-component vectors
     * @return Scalar magnitude image, or OverlayError
     */
    [[nodiscard]] static std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
    computeVelocityMagnitude(vtkSmartPointer<vtkImageData> velocityField);

    /**
     * @brief Extract a single component from a multi-component field
     *
     * @param velocityField 3D vtkImageData with 3-component vectors
     * @param component Component index (0=X, 1=Y, 2=Z)
     * @return Scalar component image, or OverlayError
     */
    [[nodiscard]] static std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
    extractComponent(vtkSmartPointer<vtkImageData> velocityField, int component);

    /**
     * @brief Get the default colormap preset for an overlay type
     *
     * Default mappings:
     * - VelocityMagnitude → Jet
     * - VelocityX/Y/Z → CoolWarm (diverging, signed data)
     * - Vorticity → CoolWarm
     * - EnergyLoss → HotMetal
     *
     * @param type Overlay type
     * @return Recommended colormap preset
     */
    [[nodiscard]] static ColormapPreset defaultColormapForType(OverlayType type) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

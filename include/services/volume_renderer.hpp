#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkVolume.h>

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

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkActor.h>
#include <vtkRenderer.h>

namespace dicom_viewer::services {

/**
 * @brief Preset tissue type for surface rendering
 */
enum class TissueType {
    Bone,       // HU 200-400
    SoftTissue, // HU 40-80
    Skin,       // HU -100 to 0
    Custom      // User-defined
};

/**
 * @brief Surface quality settings
 */
enum class SurfaceQuality {
    Low,        // Fast, fewer triangles
    Medium,     // Balanced
    High        // Best quality, more triangles
};

/**
 * @brief Configuration for a single surface
 */
struct SurfaceConfig {
    std::string name;
    double isovalue;                        // Threshold value (HU for CT)
    std::array<double, 3> color{1.0, 1.0, 1.0};  // RGB [0-1]
    double opacity = 1.0;                   // [0-1]
    bool smoothingEnabled = true;
    int smoothingIterations = 20;
    double smoothingPassBand = 0.01;
    bool decimationEnabled = true;
    double decimationReduction = 0.5;       // Target reduction [0-1]
    bool visible = true;
};

/**
 * @brief Surface data result from extraction
 */
struct SurfaceData {
    std::string name;
    vtkSmartPointer<vtkActor> actor;
    size_t triangleCount;
    double surfaceArea;
    double volume;
};

/**
 * @brief Marching Cubes based surface renderer
 *
 * Implements isosurface extraction using Marching Cubes algorithm
 * with optional smoothing and decimation for mesh optimization.
 *
 * @trace SRS-FR-012
 */
class SurfaceRenderer {
public:
    SurfaceRenderer();
    ~SurfaceRenderer();

    // Non-copyable, movable
    SurfaceRenderer(const SurfaceRenderer&) = delete;
    SurfaceRenderer& operator=(const SurfaceRenderer&) = delete;
    SurfaceRenderer(SurfaceRenderer&&) noexcept;
    SurfaceRenderer& operator=(SurfaceRenderer&&) noexcept;

    /**
     * @brief Set the input volume data
     * @param imageData VTK image data (3D volume)
     */
    void setInputData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Add a surface with specified configuration
     * @param config Surface configuration
     * @return Index of the added surface
     */
    size_t addSurface(const SurfaceConfig& config);

    /**
     * @brief Add a preset tissue surface
     * @param tissue Tissue type preset
     * @return Index of the added surface
     */
    size_t addPresetSurface(TissueType tissue);

    /**
     * @brief Remove a surface by index
     * @param index Surface index
     */
    void removeSurface(size_t index);

    /**
     * @brief Clear all surfaces
     */
    void clearSurfaces();

    /**
     * @brief Get the number of surfaces
     */
    size_t getSurfaceCount() const;

    /**
     * @brief Get surface configuration
     * @param index Surface index
     * @return Surface configuration
     */
    SurfaceConfig getSurfaceConfig(size_t index) const;

    /**
     * @brief Update surface configuration
     * @param index Surface index
     * @param config New configuration
     */
    void updateSurface(size_t index, const SurfaceConfig& config);

    /**
     * @brief Set surface visibility
     * @param index Surface index
     * @param visible Visibility flag
     */
    void setSurfaceVisibility(size_t index, bool visible);

    /**
     * @brief Set surface color
     * @param index Surface index
     * @param r Red component [0-1]
     * @param g Green component [0-1]
     * @param b Blue component [0-1]
     */
    void setSurfaceColor(size_t index, double r, double g, double b);

    /**
     * @brief Set surface opacity
     * @param index Surface index
     * @param opacity Opacity value [0-1]
     */
    void setSurfaceOpacity(size_t index, double opacity);

    /**
     * @brief Set global surface quality
     * @param quality Surface quality preset
     */
    void setSurfaceQuality(SurfaceQuality quality);

    /**
     * @brief Get the VTK actor for a surface
     * @param index Surface index
     * @return VTK actor
     */
    vtkSmartPointer<vtkActor> getActor(size_t index) const;

    /**
     * @brief Get all surface actors
     * @return Vector of all actors
     */
    std::vector<vtkSmartPointer<vtkActor>> getAllActors() const;

    /**
     * @brief Add all surfaces to a renderer
     * @param renderer VTK renderer
     */
    void addToRenderer(vtkSmartPointer<vtkRenderer> renderer);

    /**
     * @brief Remove all surfaces from a renderer
     * @param renderer VTK renderer
     */
    void removeFromRenderer(vtkSmartPointer<vtkRenderer> renderer);

    /**
     * @brief Get surface data (statistics)
     * @param index Surface index
     * @return Surface data with statistics
     */
    SurfaceData getSurfaceData(size_t index) const;

    /**
     * @brief Extract surfaces (process pipeline)
     *
     * This triggers the Marching Cubes extraction for all configured surfaces.
     * Call this after adding/updating surfaces.
     */
    void extractSurfaces();

    /**
     * @brief Update rendering
     */
    void update();

    // Preset surface configurations
    static SurfaceConfig getPresetBone();
    static SurfaceConfig getPresetBoneHighDensity();
    static SurfaceConfig getPresetSoftTissue();
    static SurfaceConfig getPresetSkin();
    static SurfaceConfig getPresetLung();
    static SurfaceConfig getPresetBloodVessels();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

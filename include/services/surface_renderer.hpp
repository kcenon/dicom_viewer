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
#include <memory>
#include <string>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkActor.h>
#include <vtkRenderer.h>

class vtkLookupTable;
class vtkPolyData;

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

    // ==================== Per-Vertex Scalar Coloring ====================

    /**
     * @brief Add a pre-built surface with per-vertex scalar coloring
     *
     * Accepts a vtkPolyData that already contains point data arrays
     * (e.g., from VesselAnalyzer::computeWSS) and renders it with
     * color mapping based on the specified scalar array.
     *
     * Unlike addSurface(), this bypasses the Marching Cubes pipeline
     * and uses the provided mesh directly.
     *
     * @param name Display name for the surface
     * @param surface vtkPolyData with point data arrays
     * @param activeArrayName Name of the scalar array to use for coloring
     * @return Index of the added surface
     */
    size_t addScalarSurface(const std::string& name,
                            vtkSmartPointer<vtkPolyData> surface,
                            const std::string& activeArrayName);

    /**
     * @brief Set scalar range for color mapping on a surface
     *
     * Controls the min/max values mapped to the colormap endpoints.
     * Only effective on surfaces added via addScalarSurface().
     *
     * @param index Surface index
     * @param minVal Minimum scalar value (mapped to colormap start)
     * @param maxVal Maximum scalar value (mapped to colormap end)
     */
    void setSurfaceScalarRange(size_t index, double minVal, double maxVal);

    /**
     * @brief Get the scalar range for a surface
     * @param index Surface index
     * @return {min, max} pair, or {0,0} if index is invalid
     */
    [[nodiscard]] std::pair<double, double> surfaceScalarRange(size_t index) const;

    /**
     * @brief Set custom lookup table for scalar-to-color mapping
     * @param index Surface index
     * @param lut VTK lookup table
     */
    void setSurfaceLookupTable(size_t index, vtkSmartPointer<vtkLookupTable> lut);

    // ==================== Hemodynamic Colormap Factories ====================

    /**
     * @brief Create WSS lookup table (blue-green-yellow-red sequential)
     * @param maxWSS Maximum WSS value in Pa
     * @return Configured lookup table for [0, maxWSS]
     */
    [[nodiscard]] static vtkSmartPointer<vtkLookupTable>
    createWSSLookupTable(double maxWSS);

    /**
     * @brief Create OSI lookup table (blue-white-red diverging)
     * @return Configured lookup table for [0, 0.5]
     */
    [[nodiscard]] static vtkSmartPointer<vtkLookupTable>
    createOSILookupTable();

    /**
     * @brief Create RRT lookup table (yellow-orange-red sequential)
     * @param maxRRT Maximum RRT value
     * @return Configured lookup table for [0, maxRRT]
     */
    [[nodiscard]] static vtkSmartPointer<vtkLookupTable>
    createRRTLookupTable(double maxRRT);

    /**
     * @brief Create AFI lookup table (green-yellow-red sequential)
     *
     * AFI (Aneurysm Formation Indicator) = TAWSS / mean_TAWSS.
     * Green (AFI < 1, below average) → Yellow (AFI ≈ 1) → Red (AFI > 1, above average).
     *
     * @param maxAFI Maximum AFI value for range (default: 2.0)
     * @return Configured lookup table for [0, maxAFI]
     */
    [[nodiscard]] static vtkSmartPointer<vtkLookupTable>
    createAFILookupTable(double maxAFI = 2.0);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

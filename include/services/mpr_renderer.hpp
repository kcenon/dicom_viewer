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
 * @file mpr_renderer.hpp
 * @brief Multi-Planar Reconstruction rendering with crosshair synchronization
 * @details Manages simultaneous Axial, Coronal, and Sagittal slice rendering
 *          with synchronized crosshair navigation. Integrates with
 *          MPRSegmentationRenderer for label overlay and LabelManager
 *          for segmentation visualization on MPR views.
 *
 * ## Thread Safety
 * - All rendering and slice navigation must occur on the main (UI) thread
 * - Crosshair position updates trigger synchronized view refreshes
 * - Window/level adjustments affect all three planes simultaneously
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <array>
#include <functional>
#include <memory>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>

#include <itkImage.h>

namespace dicom_viewer::services {

// Forward declarations
class MPRSegmentationRenderer;
class LabelManager;

namespace coordinate {
class MPRCoordinateTransformer;
}  // namespace coordinate

/**
 * @brief MPR view plane orientation
 */
enum class MPRPlane {
    Axial,      // XY plane (top-down)
    Coronal,    // XZ plane (front view)
    Sagittal    // YZ plane (side view)
};

/**
 * @brief Thick slab rendering mode
 */
enum class SlabMode {
    None,       // Single slice
    MIP,        // Maximum Intensity Projection
    MinIP,      // Minimum Intensity Projection
    Average     // Average Intensity Projection
};

/**
 * @brief Callback for slice position changes
 */
using SlicePositionCallback = std::function<void(MPRPlane plane, double position)>;

/**
 * @brief Callback for crosshair position changes
 */
using CrosshairCallback = std::function<void(double x, double y, double z)>;

/**
 * @brief Multi-Planar Reconstruction (MPR) renderer
 *
 * Provides synchronized orthogonal views (Axial, Coronal, Sagittal)
 * with crosshair navigation and window/level adjustment.
 *
 * @trace SRS-FR-008, SRS-FR-009, SRS-FR-010, SRS-FR-011
 */
class MPRRenderer {
public:
    MPRRenderer();
    ~MPRRenderer();

    // Non-copyable, movable
    MPRRenderer(const MPRRenderer&) = delete;
    MPRRenderer& operator=(const MPRRenderer&) = delete;
    MPRRenderer(MPRRenderer&&) noexcept;
    MPRRenderer& operator=(MPRRenderer&&) noexcept;

    /**
     * @brief Set the input volume data
     * @param imageData VTK image data (3D volume)
     */
    void setInputData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Get renderer for a specific plane
     * @param plane MPR plane type
     * @return VTK renderer for the plane
     */
    vtkSmartPointer<vtkRenderer> getRenderer(MPRPlane plane) const;

    /**
     * @brief Set slice position for a plane
     * @param plane MPR plane type
     * @param position Position in world coordinates
     */
    void setSlicePosition(MPRPlane plane, double position);

    /**
     * @brief Get current slice position
     * @param plane MPR plane type
     * @return Position in world coordinates
     */
    double getSlicePosition(MPRPlane plane) const;

    /**
     * @brief Get slice range for a plane
     * @param plane MPR plane type
     * @return [min, max] position range
     */
    std::pair<double, double> getSliceRange(MPRPlane plane) const;

    /**
     * @brief Scroll slice by delta
     * @param plane MPR plane type
     * @param delta Number of slices to scroll (positive = forward)
     */
    void scrollSlice(MPRPlane plane, int delta);

    /**
     * @brief Set window/level for display
     * @param width Window width
     * @param center Window center (level)
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Get current window/level
     * @return {width, center}
     */
    std::pair<double, double> getWindowLevel() const;

    /**
     * @brief Set crosshair position (in world coordinates)
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     */
    void setCrosshairPosition(double x, double y, double z);

    /**
     * @brief Get crosshair position
     * @return [x, y, z] world coordinates
     */
    std::array<double, 3> getCrosshairPosition() const;

    /**
     * @brief Enable/disable crosshair display
     * @param visible True to show crosshairs
     */
    void setCrosshairVisible(bool visible);

    /**
     * @brief Set thick slab mode
     * @param mode Slab rendering mode
     * @param thickness Slab thickness in mm (1-100mm range)
     */
    void setSlabMode(SlabMode mode, double thickness = 1.0);

    /**
     * @brief Get current slab mode
     * @return Current slab mode
     */
    [[nodiscard]] SlabMode getSlabMode() const;

    /**
     * @brief Get current slab thickness
     * @return Slab thickness in mm
     */
    [[nodiscard]] double getSlabThickness() const;

    /**
     * @brief Set slab mode for a specific plane
     * @param plane Target plane
     * @param mode Slab mode
     * @param thickness Slab thickness in mm
     */
    void setPlaneSlabMode(MPRPlane plane, SlabMode mode, double thickness = 1.0);

    /**
     * @brief Get slab mode for a specific plane
     * @param plane Target plane
     * @return Slab mode for the plane
     */
    [[nodiscard]] SlabMode getPlaneSlabMode(MPRPlane plane) const;

    /**
     * @brief Get slab thickness for a specific plane
     * @param plane Target plane
     * @return Slab thickness in mm
     */
    [[nodiscard]] double getPlaneSlabThickness(MPRPlane plane) const;

    /**
     * @brief Get effective slice count for current slab settings
     * @param plane Target plane
     * @return Number of slices used in slab
     */
    [[nodiscard]] int getEffectiveSliceCount(MPRPlane plane) const;

    /**
     * @brief Register callback for slice position changes
     */
    void setSlicePositionCallback(SlicePositionCallback callback);

    /**
     * @brief Register callback for crosshair changes
     */
    void setCrosshairCallback(CrosshairCallback callback);

    /**
     * @brief Update all views
     */
    void update();

    /**
     * @brief Reset views to default positions (center of volume)
     */
    void resetViews();

    // ==================== Segmentation Support ====================

    /// Label map type for segmentation
    using LabelMapType = itk::Image<uint8_t, 3>;

    /**
     * @brief Set the label map for segmentation overlay
     * @param labelMap 3D label map
     */
    void setLabelMap(LabelMapType::Pointer labelMap);

    /**
     * @brief Get the current label map
     * @return Label map pointer or nullptr
     */
    [[nodiscard]] LabelMapType::Pointer getLabelMap() const;

    /**
     * @brief Set the label manager for color/visibility
     * @param labelManager Label manager instance
     */
    void setLabelManager(LabelManager* labelManager);

    /**
     * @brief Set segmentation overlay visibility
     * @param visible True to show overlay
     */
    void setSegmentationVisible(bool visible);

    /**
     * @brief Check if segmentation overlay is visible
     * @return True if visible
     */
    [[nodiscard]] bool isSegmentationVisible() const;

    /**
     * @brief Set segmentation overlay opacity
     * @param opacity Opacity value (0.0-1.0)
     */
    void setSegmentationOpacity(double opacity);

    /**
     * @brief Get segmentation overlay opacity
     * @return Opacity value
     */
    [[nodiscard]] double getSegmentationOpacity() const;

    /**
     * @brief Update segmentation overlay after label map modification
     */
    void updateSegmentationOverlay();

    /**
     * @brief Update segmentation overlay for a specific plane
     * @param plane MPR plane to update
     */
    void updateSegmentationOverlay(MPRPlane plane);

    /**
     * @brief Get the coordinate transformer
     * @return Pointer to coordinate transformer
     */
    [[nodiscard]] coordinate::MPRCoordinateTransformer* getCoordinateTransformer() const;

    /**
     * @brief Get the segmentation renderer
     * @return Pointer to segmentation renderer
     */
    [[nodiscard]] MPRSegmentationRenderer* getSegmentationRenderer() const;

    /**
     * @brief Get slice index from world position for a plane
     * @param plane MPR plane
     * @param worldPosition World position in mm
     * @return Slice index
     */
    [[nodiscard]] int worldPositionToSliceIndex(MPRPlane plane, double worldPosition) const;

    /**
     * @brief Get world position from slice index for a plane
     * @param plane MPR plane
     * @param sliceIndex Slice index
     * @return World position in mm
     */
    [[nodiscard]] double sliceIndexToWorldPosition(MPRPlane plane, int sliceIndex) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

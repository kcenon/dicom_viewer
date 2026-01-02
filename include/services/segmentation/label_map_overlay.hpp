#pragma once

#include "services/mpr_renderer.hpp"
#include "services/segmentation/segmentation_label.hpp"

#include <memory>
#include <unordered_map>

#include <itkImage.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkImageData.h>

namespace dicom_viewer::services {

// LabelColor is defined in segmentation_label.hpp

/**
 * @brief Renders segmentation label map as overlay on MPR views
 *
 * Provides visualization of ITK label maps on VTK renderers,
 * with support for multiple labels and customizable colors.
 *
 * @trace SRS-FR-023
 */
class LabelMapOverlay {
public:
    using LabelMapType = itk::Image<uint8_t, 3>;

    LabelMapOverlay();
    ~LabelMapOverlay();

    // Non-copyable, movable
    LabelMapOverlay(const LabelMapOverlay&) = delete;
    LabelMapOverlay& operator=(const LabelMapOverlay&) = delete;
    LabelMapOverlay(LabelMapOverlay&&) noexcept;
    LabelMapOverlay& operator=(LabelMapOverlay&&) noexcept;

    /**
     * @brief Set the source label map
     * @param labelMap ITK label map image
     */
    void setLabelMap(LabelMapType::Pointer labelMap);

    /**
     * @brief Get the current label map
     * @return Label map pointer
     */
    [[nodiscard]] LabelMapType::Pointer getLabelMap() const;

    /**
     * @brief Set color for a specific label
     * @param labelId Label ID (1-255)
     * @param color RGBA color
     */
    void setLabelColor(uint8_t labelId, const LabelColor& color);

    /**
     * @brief Get color for a label
     * @param labelId Label ID
     * @return Label color (default if not set)
     */
    [[nodiscard]] LabelColor getLabelColor(uint8_t labelId) const;

    /**
     * @brief Set overlay opacity (global)
     * @param opacity Opacity value (0.0-1.0)
     */
    void setOpacity(double opacity);

    /**
     * @brief Get current overlay opacity
     * @return Opacity value
     */
    [[nodiscard]] double getOpacity() const;

    /**
     * @brief Set visibility of the overlay
     * @param visible True to show overlay
     */
    void setVisible(bool visible);

    /**
     * @brief Check if overlay is visible
     * @return Visibility state
     */
    [[nodiscard]] bool isVisible() const;

    /**
     * @brief Attach overlay to an MPR renderer
     *
     * @param renderer VTK renderer to attach to
     * @param plane MPR plane for this renderer
     */
    void attachToRenderer(vtkSmartPointer<vtkRenderer> renderer, MPRPlane plane);

    /**
     * @brief Detach overlay from a renderer
     *
     * @param plane MPR plane to detach from
     */
    void detachFromRenderer(MPRPlane plane);

    /**
     * @brief Update overlay for current slice position
     *
     * Call this when the slice position changes to update
     * the displayed overlay.
     *
     * @param plane MPR plane to update
     * @param slicePosition Current slice position in world coordinates
     */
    void updateSlice(MPRPlane plane, double slicePosition);

    /**
     * @brief Update all attached overlays
     *
     * Call this after label map modifications to refresh display.
     */
    void updateAll();

    /**
     * @brief Notify that a specific slice was modified
     *
     * Triggers re-rendering of the affected plane(s).
     *
     * @param sliceIndex Modified slice index
     */
    void notifySliceModified(int sliceIndex);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

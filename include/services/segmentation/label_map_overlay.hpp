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
 * @file label_map_overlay.hpp
 * @brief Renders segmentation label map as overlay on MPR views
 * @details Visualizes ITK label maps on VTK renderers with support for
 *          multiple labels and customizable colors. Non-copyable and movable
 *          for proper resource management.
 *
 * @author kcenon
 * @since 1.0.0
 */
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

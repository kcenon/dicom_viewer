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
 * @file mpr_segmentation_renderer.hpp
 * @brief Renders segmentation overlays on MPR views
 * @details Creates and manages VTK actors for displaying segmentation labels
 *          as semi-transparent colored overlays on MPR planes. Extracts 2D
 *          slices from 3D label map for each plane with customizable colors
 *          and opacity.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkImageData.h>
#include <itkImage.h>

#include "services/mpr_renderer.hpp"
#include "services/segmentation/label_manager.hpp"

namespace dicom_viewer::services {

/**
 * @brief Renders segmentation overlays on MPR views
 *
 * Creates and manages VTK actors for displaying segmentation labels
 * as semi-transparent colored overlays on each MPR view.
 *
 * The renderer extracts 2D slices from the 3D label map for each
 * MPR plane and displays them with appropriate colors and opacity.
 *
 * @trace SRS-FR-023
 */
class MPRSegmentationRenderer {
public:
    /// Label map type (3D volume)
    using LabelMapType = itk::Image<uint8_t, 3>;

    /// Callback when rendering needs update
    using UpdateCallback = std::function<void()>;

    MPRSegmentationRenderer();
    ~MPRSegmentationRenderer();

    // Non-copyable but movable
    MPRSegmentationRenderer(const MPRSegmentationRenderer&) = delete;
    MPRSegmentationRenderer& operator=(const MPRSegmentationRenderer&) = delete;
    MPRSegmentationRenderer(MPRSegmentationRenderer&&) noexcept;
    MPRSegmentationRenderer& operator=(MPRSegmentationRenderer&&) noexcept;

    /**
     * @brief Set the label map to render
     * @param labelMap 3D label map from segmentation
     */
    void setLabelMap(LabelMapType::Pointer labelMap);

    /**
     * @brief Get the current label map
     * @return Label map pointer or nullptr
     */
    [[nodiscard]] LabelMapType::Pointer getLabelMap() const;

    /**
     * @brief Set the renderers for each MPR plane
     *
     * The overlay actors will be added to these renderers.
     *
     * @param axialRenderer Renderer for axial view
     * @param coronalRenderer Renderer for coronal view
     * @param sagittalRenderer Renderer for sagittal view
     */
    void setRenderers(
        vtkSmartPointer<vtkRenderer> axialRenderer,
        vtkSmartPointer<vtkRenderer> coronalRenderer,
        vtkSmartPointer<vtkRenderer> sagittalRenderer);

    /**
     * @brief Set renderer for a specific plane
     * @param plane MPR plane
     * @param renderer VTK renderer
     */
    void setRenderer(MPRPlane plane, vtkSmartPointer<vtkRenderer> renderer);

    /**
     * @brief Set the label manager for color/visibility information
     * @param labelManager Label manager instance
     */
    void setLabelManager(LabelManager* labelManager);

    /**
     * @brief Update the slice position for a plane
     *
     * Extracts the appropriate 2D slice from the label map
     * and updates the overlay actor.
     *
     * @param plane MPR plane
     * @param sliceIndex Slice index (not world position)
     */
    void setSliceIndex(MPRPlane plane, int sliceIndex);

    /**
     * @brief Get current slice index for a plane
     * @param plane MPR plane
     * @return Current slice index
     */
    [[nodiscard]] int getSliceIndex(MPRPlane plane) const;

    /**
     * @brief Set overall visibility of segmentation overlay
     * @param visible True to show overlay
     */
    void setVisible(bool visible);

    /**
     * @brief Check if overlay is visible
     * @return True if visible
     */
    [[nodiscard]] bool isVisible() const;

    /**
     * @brief Set visibility for a specific label
     * @param labelId Label ID
     * @param visible True to show label
     */
    void setLabelVisible(uint8_t labelId, bool visible);

    /**
     * @brief Set color for a label
     * @param labelId Label ID
     * @param color Label color (RGBA, 0-1 range)
     */
    void setLabelColor(uint8_t labelId, const LabelColor& color);

    /**
     * @brief Set overall opacity for segmentation overlay
     * @param opacity Opacity value (0.0-1.0)
     */
    void setOpacity(double opacity);

    /**
     * @brief Get current opacity
     * @return Opacity value
     */
    [[nodiscard]] double getOpacity() const;

    /**
     * @brief Force update of all overlays
     *
     * Call this after modifying the label map.
     */
    void update();

    /**
     * @brief Update overlay for a specific plane
     * @param plane MPR plane to update
     */
    void updatePlane(MPRPlane plane);

    /**
     * @brief Set callback for render updates
     * @param callback Callback function
     */
    void setUpdateCallback(UpdateCallback callback);

    /**
     * @brief Remove all overlay actors from renderers
     */
    void removeFromRenderers();

    /**
     * @brief Clear the label map and overlays
     */
    void clear();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

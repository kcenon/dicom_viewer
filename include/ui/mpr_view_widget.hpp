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

#include "services/mpr_renderer.hpp"
#include "services/coordinate/mpr_coordinate_transformer.hpp"
#include "services/segmentation/manual_segmentation_controller.hpp"
#include "services/segmentation/label_map_overlay.hpp"

#include <memory>

#include <QWidget>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

namespace dicom_viewer::ui {

enum class ScrollMode;

/**
 * @brief Composite widget displaying synchronized MPR views with segmentation support
 *
 * Provides a 2x2 layout of MPR views (Axial, Coronal, Sagittal, and optionally 3D)
 * with integrated segmentation tools that work across all views.
 *
 * Features:
 * - Synchronized crosshair navigation between views
 * - Segmentation tools working on all MPR planes
 * - Label map overlay visualization
 * - Coordinate transformation between views
 *
 * @trace SRS-FR-008, SRS-FR-023
 */
class MPRViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit MPRViewWidget(QWidget* parent = nullptr);
    ~MPRViewWidget() override;

    // Non-copyable
    MPRViewWidget(const MPRViewWidget&) = delete;
    MPRViewWidget& operator=(const MPRViewWidget&) = delete;

    /**
     * @brief Set the input image data
     * @param imageData VTK image data (3D volume)
     */
    void setImageData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Get the current image data
     * @return VTK image data pointer
     */
    [[nodiscard]] vtkSmartPointer<vtkImageData> getImageData() const;

    /**
     * @brief Set window/level for all views
     * @param width Window width
     * @param center Window center
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Get current window/level
     * @return {width, center}
     */
    [[nodiscard]] std::pair<double, double> getWindowLevel() const;

    /**
     * @brief Reset all views to default positions
     */
    void resetViews();

    // Segmentation interface

    /**
     * @brief Set the active segmentation tool
     * @param tool Segmentation tool to activate
     */
    void setSegmentationTool(services::SegmentationTool tool);

    /**
     * @brief Get current segmentation tool
     * @return Current tool
     */
    [[nodiscard]] services::SegmentationTool getSegmentationTool() const;

    /**
     * @brief Set brush size for segmentation
     * @param size Brush size in pixels (1-50)
     */
    void setSegmentationBrushSize(int size);

    /**
     * @brief Get current brush size
     * @return Brush size in pixels
     */
    [[nodiscard]] int getSegmentationBrushSize() const;

    /**
     * @brief Set brush shape for segmentation
     * @param shape Brush shape
     */
    void setSegmentationBrushShape(services::BrushShape shape);

    /**
     * @brief Get current brush shape
     * @return Brush shape
     */
    [[nodiscard]] services::BrushShape getSegmentationBrushShape() const;

    /**
     * @brief Set active label for segmentation
     * @param labelId Label ID (1-255)
     */
    void setSegmentationActiveLabel(uint8_t labelId);

    /**
     * @brief Get current active label
     * @return Label ID
     */
    [[nodiscard]] uint8_t getSegmentationActiveLabel() const;

    /**
     * @brief Set color for a label
     * @param labelId Label ID
     * @param color Label color
     */
    void setLabelColor(uint8_t labelId, const services::LabelColor& color);

    /**
     * @brief Undo last segmentation operation
     */
    void undoSegmentationOperation();

    /**
     * @brief Complete current segmentation operation
     */
    void completeSegmentationOperation();

    /**
     * @brief Clear all segmentation data
     */
    void clearAllSegmentation();

    /**
     * @brief Check if segmentation mode is active
     * @return true if a segmentation tool is selected
     */
    [[nodiscard]] bool isSegmentationModeActive() const;

    /**
     * @brief Set overlay visibility
     * @param visible True to show overlay
     */
    void setOverlayVisible(bool visible);

    /**
     * @brief Set overlay opacity
     * @param opacity Opacity value (0.0-1.0)
     */
    void setOverlayOpacity(double opacity);

    /**
     * @brief Get current slice index for a plane
     * @param plane MPR plane
     * @return Slice index
     */
    [[nodiscard]] int getSliceIndex(services::MPRPlane plane) const;

    /**
     * @brief Get the active plane (last interacted)
     * @return Active MPR plane
     */
    [[nodiscard]] services::MPRPlane getActivePlane() const;

    // ==================== Thick Slab Rendering Interface ====================

    /**
     * @brief Set thick slab mode for all planes
     * @param mode Slab rendering mode (None, MIP, MinIP, Average)
     * @param thickness Slab thickness in mm (1-100mm)
     */
    void setSlabMode(services::SlabMode mode, double thickness = 1.0);

    /**
     * @brief Get current slab mode
     * @return Current slab mode
     */
    [[nodiscard]] services::SlabMode getSlabMode() const;

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
    void setPlaneSlabMode(services::MPRPlane plane, services::SlabMode mode, double thickness = 1.0);

    /**
     * @brief Get slab mode for a specific plane
     * @param plane Target plane
     * @return Slab mode
     */
    [[nodiscard]] services::SlabMode getPlaneSlabMode(services::MPRPlane plane) const;

    /**
     * @brief Get slab thickness for a specific plane
     * @param plane Target plane
     * @return Slab thickness in mm
     */
    [[nodiscard]] double getPlaneSlabThickness(services::MPRPlane plane) const;

    /**
     * @brief Get effective slice count for a plane
     * @param plane Target plane
     * @return Number of slices in current slab
     */
    [[nodiscard]] int getEffectiveSliceCount(services::MPRPlane plane) const;

signals:
    /// Emitted when crosshair position changes (world coordinates)
    void crosshairPositionChanged(double x, double y, double z);

    /// Emitted when window/level changes
    void windowLevelChanged(double width, double center);

    /// Emitted when segmentation tool changes
    void segmentationToolChanged(services::SegmentationTool tool);

    /// Emitted when segmentation is modified
    void segmentationModified(int sliceIndex);

    /// Emitted when slice position changes
    void slicePositionChanged(services::MPRPlane plane, double position);

    /// Emitted when slab mode changes
    void slabModeChanged(services::SlabMode mode, double thickness);

    /// Emitted when scroll wheel is used in Phase mode
    void phaseScrollRequested(int delta);

public slots:
    /// Set the scroll mode (Slice or Phase)
    void setScrollMode(ScrollMode mode);
    /// Set crosshair position from external source
    void setCrosshairPosition(double x, double y, double z);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Allow MPRInteractorCallback to access Impl
    friend class MPRInteractorCallback;
};

} // namespace dicom_viewer::ui

#pragma once

#include "services/mpr_renderer.hpp"
#include "services/mpr_coordinate_transformer.hpp"
#include "services/segmentation/manual_segmentation_controller.hpp"
#include "services/segmentation/label_map_overlay.hpp"

#include <memory>

#include <QWidget>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

namespace dicom_viewer::ui {

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

public slots:
    /// Set crosshair position from external source
    void setCrosshairPosition(double x, double y, double z);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

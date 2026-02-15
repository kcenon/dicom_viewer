#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QWidget>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

#include "services/measurement/measurement_types.hpp"
#include "services/segmentation/manual_segmentation_controller.hpp"

class QVTKOpenGLNativeWidget;

namespace dicom_viewer::ui {

/**
 * @brief Rendering mode for viewport
 */
enum class ViewportMode {
    VolumeRendering,    // 3D volume rendering
    SurfaceRendering,   // Surface (isosurface) rendering
    MPR,               // Multi-planar reconstruction (2x2 layout)
    SingleSlice        // Single 2D slice view
};

/**
 * @brief VTK viewport widget for medical image visualization
 *
 * Wraps QVTKOpenGLNativeWidget and provides high-level interface
 * for volume rendering, MPR views, and surface rendering.
 *
 * @trace SRS-FR-005, SRS-FR-008, SRS-FR-012
 */
class ViewportWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override;

    // Non-copyable
    ViewportWidget(const ViewportWidget&) = delete;
    ViewportWidget& operator=(const ViewportWidget&) = delete;

    /**
     * @brief Set the input image data
     * @param imageData VTK image data
     */
    void setImageData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Set the viewport rendering mode
     * @param mode Viewport mode
     */
    void setMode(ViewportMode mode);

    /**
     * @brief Get current viewport mode
     */
    ViewportMode getMode() const;

    /**
     * @brief Set window/level for 2D views
     * @param width Window width
     * @param center Window center
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Apply a volume rendering preset
     * @param presetName Name of the preset
     */
    void applyPreset(const QString& presetName);

    /**
     * @brief Reset camera to fit the data
     */
    void resetCamera();

    /**
     * @brief Capture screenshot
     * @param filePath Output file path
     * @return True on success
     */
    bool captureScreenshot(const QString& filePath);

    /**
     * @brief Start distance measurement
     */
    void startDistanceMeasurement();

    /**
     * @brief Start angle measurement
     */
    void startAngleMeasurement();

    /**
     * @brief Start area measurement with specified ROI type
     * @param type Type of ROI to draw
     */
    void startAreaMeasurement(services::RoiType type);

    /**
     * @brief Cancel any active measurement
     */
    void cancelMeasurement();

    /**
     * @brief Delete all measurements
     */
    void deleteAllMeasurements();

    /**
     * @brief Delete all area measurements
     */
    void deleteAllAreaMeasurements();

    /**
     * @brief Get current measurement mode
     */
    services::MeasurementMode getMeasurementMode() const;

    /**
     * @brief Get all area measurements
     * @return Vector of area measurements
     */
    std::vector<services::AreaMeasurement> getAreaMeasurements() const;

    /**
     * @brief Get a specific area measurement by ID
     * @param id Measurement ID
     * @return Area measurement if found
     */
    std::optional<services::AreaMeasurement> getAreaMeasurement(int id) const;

    /**
     * @brief Get current slice index
     * @return Current slice index
     */
    int getCurrentSlice() const;

    /**
     * @brief Get current image data
     * @return VTK image data pointer
     */
    vtkSmartPointer<vtkImageData> getImageData() const;

    // Segmentation methods

    /**
     * @brief Set the active segmentation tool
     * @param tool Segmentation tool to activate
     */
    void setSegmentationTool(services::SegmentationTool tool);

    /**
     * @brief Get current segmentation tool
     * @return Current tool
     */
    services::SegmentationTool getSegmentationTool() const;

    /**
     * @brief Set brush size for segmentation
     * @param size Brush size in pixels (1-50)
     */
    void setSegmentationBrushSize(int size);

    /**
     * @brief Set brush shape for segmentation
     * @param shape Brush shape
     */
    void setSegmentationBrushShape(services::BrushShape shape);

    /**
     * @brief Set active label for segmentation
     * @param labelId Label ID (1-255)
     */
    void setSegmentationActiveLabel(uint8_t labelId);

    /**
     * @brief Undo last segmentation operation (polygon vertex/smart scissors anchor)
     */
    void undoSegmentationOperation();

    /**
     * @brief Complete current segmentation operation (polygon/smart scissors)
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
    bool isSegmentationModeActive() const;

signals:
    /// Emitted when crosshair position changes (world coordinates)
    void crosshairPositionChanged(double x, double y, double z);

    /// Emitted when window/level changes
    void windowLevelChanged(double width, double center);

    /// Emitted when mouse is over a voxel
    void voxelValueChanged(double value, double x, double y, double z);

    /// Emitted when a distance measurement is completed
    void distanceMeasurementCompleted(double distanceMm, int measurementId);

    /// Emitted when an angle measurement is completed
    void angleMeasurementCompleted(double angleDegrees, int measurementId);

    /// Emitted when an area measurement is completed
    void areaMeasurementCompleted(double areaMm2, double areaCm2, int measurementId);

    /// Emitted when measurement mode changes
    void measurementModeChanged(services::MeasurementMode mode);

    /// Emitted when segmentation tool changes
    void segmentationToolChanged(services::SegmentationTool tool);

    /// Emitted when segmentation is modified
    void segmentationModified(int sliceIndex);

    /// Emitted when phase index changes
    void phaseIndexChanged(int phaseIndex);

public slots:
    /// Set crosshair position from external source
    void setCrosshairPosition(double x, double y, double z);

    /// Set the cardiac phase index for 4D display
    void setPhaseIndex(int phaseIndex);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

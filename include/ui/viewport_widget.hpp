#pragma once

#include <memory>
#include <QWidget>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

#include "services/measurement/measurement_types.hpp"

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
     * @brief Cancel any active measurement
     */
    void cancelMeasurement();

    /**
     * @brief Delete all measurements
     */
    void deleteAllMeasurements();

    /**
     * @brief Get current measurement mode
     */
    services::MeasurementMode getMeasurementMode() const;

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

    /// Emitted when measurement mode changes
    void measurementModeChanged(services::MeasurementMode mode);

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

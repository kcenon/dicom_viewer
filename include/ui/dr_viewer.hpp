#pragma once

#include <memory>
#include <vector>
#include <optional>

#include <QWidget>
#include <QString>
#include <QPointF>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

namespace dicom_viewer::core {
struct DicomMetadata;
}

namespace dicom_viewer::ui {

/**
 * @brief Comparison layout mode for prior studies
 */
enum class ComparisonLayout {
    SideBySide,
    TopBottom,
    Overlay
};

/**
 * @brief DR/CR window preset configuration
 */
struct DRPreset {
    QString name;
    double windowWidth;
    double windowCenter;
    QString description;
};

/**
 * @brief Annotation type for DR viewer
 */
enum class DRAnnotationType {
    Text,
    Arrow,
    Marker
};

/**
 * @brief Annotation data for DR viewer
 */
struct DRAnnotation {
    int id;
    DRAnnotationType type;
    QPointF position;
    QPointF endPosition;  // For arrows
    QString text;
    int markerNumber;     // For numbered markers
    bool visible = true;
};

/**
 * @brief DR viewer options configuration
 */
struct DRViewerOptions {
    bool showOrientationMarkers = true;
    bool showPatientInfo = true;
    bool showStudyInfo = true;
    bool showScaleBar = true;

    bool autoDetectMagnification = true;
    double manualPixelSpacing = -1.0;

    QString defaultPreset = "Chest";

    bool enableComparison = true;
    ComparisonLayout comparisonLayout = ComparisonLayout::SideBySide;

    bool persistAnnotations = true;
};

/**
 * @brief Dedicated 2D viewer widget for DR (Digital Radiography) and CR (Computed Radiography) images
 *
 * Provides optimized viewing for single-frame radiographic images with features including:
 * - Proper orientation markers (L/R, Sup/Inf) based on DICOM tags
 * - Calibration for accurate measurements using pixel spacing
 * - Standard radiography window presets (Chest, Bone, Soft Tissue, etc.)
 * - Annotation tools (text, arrows, numbered markers)
 * - Side-by-side prior study comparison
 * - True 1:1 pixel display mode
 *
 * @trace SRS-FR-012
 */
class DRViewer : public QWidget {
    Q_OBJECT

public:
    explicit DRViewer(QWidget* parent = nullptr);
    ~DRViewer() override;

    // Non-copyable
    DRViewer(const DRViewer&) = delete;
    DRViewer& operator=(const DRViewer&) = delete;

    /**
     * @brief Load DR/CR image from VTK image data
     * @param image VTK image data (2D or 3D with single slice)
     */
    void setImage(vtkSmartPointer<vtkImageData> image);

    /**
     * @brief Load DICOM metadata for orientation and calibration
     * @param metadata DICOM metadata structure
     */
    void setDicomMetadata(const core::DicomMetadata& metadata);

    /**
     * @brief Get current image data
     * @return VTK image data pointer
     */
    [[nodiscard]] vtkSmartPointer<vtkImageData> getImage() const;

    // ==================== Display Settings ====================

    /**
     * @brief Set visibility of orientation markers
     * @param show True to show markers
     */
    void setShowOrientationMarkers(bool show);

    /**
     * @brief Set visibility of patient information overlay
     * @param show True to show patient info
     */
    void setShowPatientInfo(bool show);

    /**
     * @brief Set visibility of study information overlay
     * @param show True to show study info
     */
    void setShowStudyInfo(bool show);

    /**
     * @brief Set visibility of scale bar
     * @param show True to show scale bar
     */
    void setShowScaleBar(bool show);

    // ==================== Window/Level ====================

    /**
     * @brief Set window width and level
     * @param window Window width
     * @param level Window level (center)
     */
    void setWindowLevel(double window, double level);

    /**
     * @brief Get current window/level
     * @return {window, level}
     */
    [[nodiscard]] std::pair<double, double> getWindowLevel() const;

    /**
     * @brief Apply a named preset
     * @param presetName Name of the preset
     */
    void applyPreset(const QString& presetName);

    /**
     * @brief Get list of available presets
     * @return Vector of preset names
     */
    [[nodiscard]] std::vector<QString> getAvailablePresets() const;

    /**
     * @brief Get preset by name
     * @param name Preset name
     * @return Preset if found
     */
    [[nodiscard]] std::optional<DRPreset> getPreset(const QString& name) const;

    // ==================== Zoom ====================

    /**
     * @brief Set zoom level
     * @param zoom Zoom factor (1.0 = 100%)
     */
    void setZoomLevel(double zoom);

    /**
     * @brief Get current zoom level
     * @return Zoom factor
     */
    [[nodiscard]] double getZoomLevel() const;

    /**
     * @brief Fit image to window
     */
    void fitToWindow();

    /**
     * @brief Display at actual pixel size (1:1)
     */
    void actualSize();

    /**
     * @brief Reset view to default
     */
    void resetView();

    // ==================== Calibration ====================

    /**
     * @brief Set pixel spacing manually
     * @param spacingMm Pixel spacing in mm
     */
    void setPixelSpacing(double spacingMm);

    /**
     * @brief Get current pixel spacing
     * @return Pixel spacing in mm
     */
    [[nodiscard]] double getPixelSpacing() const;

    /**
     * @brief Check if calibration is available
     * @return True if calibrated
     */
    [[nodiscard]] bool isCalibrated() const;

    // ==================== Annotations ====================

    /**
     * @brief Add text annotation
     * @param position Position in image coordinates
     * @param text Annotation text
     * @return Annotation ID
     */
    int addTextAnnotation(const QPointF& position, const QString& text);

    /**
     * @brief Add arrow annotation
     * @param start Arrow start position
     * @param end Arrow end position
     * @return Annotation ID
     */
    int addArrowAnnotation(const QPointF& start, const QPointF& end);

    /**
     * @brief Add numbered marker
     * @param position Marker position
     * @param number Marker number (1-99)
     * @return Annotation ID
     */
    int addMarker(const QPointF& position, int number);

    /**
     * @brief Get all annotations
     * @return Vector of annotations
     */
    [[nodiscard]] std::vector<DRAnnotation> getAnnotations() const;

    /**
     * @brief Remove annotation by ID
     * @param id Annotation ID
     */
    void removeAnnotation(int id);

    /**
     * @brief Clear all annotations
     */
    void clearAnnotations();

    /**
     * @brief Save annotations to JSON file
     * @param filePath Output file path
     * @return True on success
     */
    bool saveAnnotations(const QString& filePath) const;

    /**
     * @brief Load annotations from JSON file
     * @param filePath Input file path
     * @return True on success
     */
    bool loadAnnotations(const QString& filePath);

    // ==================== Comparison ====================

    /**
     * @brief Set comparison image (prior study)
     * @param priorImage Prior study image
     */
    void setComparisonImage(vtkSmartPointer<vtkImageData> priorImage);

    /**
     * @brief Set comparison layout mode
     * @param layout Layout mode
     */
    void setComparisonLayout(ComparisonLayout layout);

    /**
     * @brief Enable/disable linked zoom and pan
     * @param enable True to link
     */
    void enableLinkZoomPan(bool enable);

    /**
     * @brief Check if comparison mode is active
     * @return True if comparison image is set
     */
    [[nodiscard]] bool isComparisonActive() const;

    /**
     * @brief Clear comparison image
     */
    void clearComparison();

    // ==================== Screenshot ====================

    /**
     * @brief Capture screenshot
     * @param filePath Output file path
     * @return True on success
     */
    bool captureScreenshot(const QString& filePath);

signals:
    /// Emitted when window/level changes
    void windowLevelChanged(double window, double level);

    /// Emitted when zoom level changes
    void zoomLevelChanged(double zoom);

    /// Emitted when a measurement is made
    void measurementMade(double lengthMm);

    /// Emitted when an annotation is added
    void annotationAdded(int id);

    /// Emitted when an annotation is removed
    void annotationRemoved(int id);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Get standard DR/CR presets
 * @return Vector of standard presets
 */
[[nodiscard]] std::vector<DRPreset> getStandardDRPresets();

/**
 * @brief Check if modality is DR or CR
 * @param modality DICOM modality string
 * @return True if DR or CR
 */
[[nodiscard]] bool isDRorCRModality(const QString& modality);

} // namespace dicom_viewer::ui

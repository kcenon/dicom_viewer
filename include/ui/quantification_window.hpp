#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QColor>
#include <QMainWindow>
#include <QRectF>

#include <vtkSmartPointer.h>

class QPainter;
class vtkImageData;

namespace dicom_viewer::ui {

class FlowGraphWidget;

/**
 * @brief Measurement parameter identifiers for quantification
 */
enum class MeasurementParameter {
    FlowRate,       ///< Volume flow rate (mL/s)
    PeakVelocity,   ///< Peak velocity (cm/s)
    MeanVelocity,   ///< Mean velocity (cm/s)
    KineticEnergy,  ///< Kinetic energy (mJ)
    RegurgitantFraction, ///< Regurgitant fraction (%)
    StrokeVolume    ///< Stroke volume (mL)
};

/**
 * @brief Row data for the statistics table
 */
struct QuantificationRow {
    MeasurementParameter parameter;
    double mean = 0.0;
    double stdDev = 0.0;
    double max = 0.0;
    double min = 0.0;
};

/**
 * @brief Volume-level measurement parameter identifiers
 */
enum class VolumeParameter {
    TotalKE,       ///< Total kinetic energy (mJ)
    VortexVolume,  ///< Vortex volume (mL)
    EnergyLoss,    ///< Energy loss (mW)
    MeanWSS,       ///< Mean wall shear stress (Pa)
    PeakWSS        ///< Peak wall shear stress (Pa)
};

/**
 * @brief Row data for the 3D volume statistics table
 */
struct VolumeStatRow {
    VolumeParameter parameter;
    double value = 0.0;
    QString unit;
};

/**
 * @brief Spatial position of a measurement plane in 3D space
 */
struct PlanePosition {
    double normalX = 0.0;  ///< Normal vector X component
    double normalY = 0.0;  ///< Normal vector Y component
    double normalZ = 1.0;  ///< Normal vector Z component (default: axial)
    double centerX = 0.0;  ///< Center point X in mm
    double centerY = 0.0;  ///< Center point Y in mm
    double centerZ = 0.0;  ///< Center point Z in mm
    double extent = 50.0;  ///< Measurement region extent in mm
};

/**
 * @brief Independent window for quantitative flow analysis
 *
 * Displays measurement statistics in a table (Mean/Std/Max/Min) and
 * provides checkboxes for selecting which parameters to display.
 * The right area is reserved for future graph and 2D/3D views.
 *
 * @trace SRS-FR-045
 */
class QuantificationWindow : public QMainWindow {
    Q_OBJECT

public:
    /// Maximum number of simultaneous measurement planes
    static constexpr int kMaxPlanes = 5;

    explicit QuantificationWindow(QWidget* parent = nullptr);
    ~QuantificationWindow() override;

    // Non-copyable
    QuantificationWindow(const QuantificationWindow&) = delete;
    QuantificationWindow& operator=(const QuantificationWindow&) = delete;

    /**
     * @brief Set statistics data for the table
     * @param rows Vector of quantification rows
     */
    void setStatistics(const std::vector<QuantificationRow>& rows);

    /**
     * @brief Get current statistics data
     * @return Vector of quantification rows
     */
    [[nodiscard]] std::vector<QuantificationRow> getStatistics() const;

    /**
     * @brief Clear all statistics data
     */
    void clearStatistics();

    /**
     * @brief Get the number of rows in the statistics table
     */
    [[nodiscard]] int rowCount() const;

    /**
     * @brief Check if a measurement parameter is enabled
     * @param param Parameter to check
     * @return True if enabled
     */
    [[nodiscard]] bool isParameterEnabled(MeasurementParameter param) const;

    /**
     * @brief Enable or disable a measurement parameter checkbox
     * @param param Parameter to set
     * @param enabled Whether to enable
     */
    void setParameterEnabled(MeasurementParameter param, bool enabled);

    /**
     * @brief Get formatted summary text for clipboard
     * @return Tab-separated summary string
     */
    [[nodiscard]] QString summaryText() const;

    /**
     * @brief Get the embedded flow graph widget
     * @return Pointer to the FlowGraphWidget
     */
    [[nodiscard]] FlowGraphWidget* graphWidget() const;

    /**
     * @brief Export statistics and time-series data to CSV file
     *
     * Opens a file dialog and writes comma-separated data.
     */
    void exportCsv();

    /**
     * @brief Export summary report to PDF file
     *
     * Generates a PDF containing the statistics table and flow graph image.
     * Opens a file dialog for path selection.
     */
    void exportPdf();

    /**
     * @brief Render PDF report content to a QPainter
     *
     * Draws the statistics table and graph onto the given painter.
     * Used by exportPdf() and useful for testing.
     *
     * @param painter Configured QPainter (from QPrinter or QPixmap)
     * @param pageRect Bounding rectangle for content layout
     */
    void renderReport(QPainter& painter, const QRectF& pageRect) const;

    /**
     * @brief Set flow direction flip state
     * @param flipped True to negate all flow rate values
     */
    void setFlowDirectionFlipped(bool flipped);

    /**
     * @brief Check if flow direction is flipped
     */
    [[nodiscard]] bool isFlowDirectionFlipped() const;

    // -- Plane management API --

    /**
     * @brief Add a measurement plane with name and color
     * @param name Display name (e.g., "Plane 1")
     * @param color Color for graph line and selector indicator
     */
    void addPlane(const QString& name, const QColor& color);

    /**
     * @brief Remove a measurement plane by index
     * @param index 0-based plane index
     */
    void removePlane(int index);

    /**
     * @brief Get the number of registered planes
     */
    [[nodiscard]] int planeCount() const;

    /**
     * @brief Get the currently active plane index
     * @return Active plane index, or -1 if no planes
     */
    [[nodiscard]] int activePlaneIndex() const;

    /**
     * @brief Programmatically select an active plane
     * @param index 0-based plane index
     */
    void setActivePlane(int index);

    /**
     * @brief Get a plane's display name
     * @param index 0-based plane index
     * @return Plane name, or empty string if out of range
     */
    [[nodiscard]] QString planeName(int index) const;

    /**
     * @brief Get a plane's color
     * @param index 0-based plane index
     * @return Plane color, or invalid QColor if out of range
     */
    [[nodiscard]] QColor planeColor(int index) const;

    /**
     * @brief Add a measurement plane with name, color, and position
     * @param name Display name
     * @param color Color for graph line and selector indicator
     * @param position Spatial position in 3D space
     */
    void addPlane(const QString& name, const QColor& color,
                  const PlanePosition& position);

    /**
     * @brief Get a plane's spatial position
     * @param index 0-based plane index
     * @return Plane position, or default PlanePosition if out of range
     */
    [[nodiscard]] PlanePosition planePosition(int index) const;

    /**
     * @brief Set a plane's spatial position
     * @param index 0-based plane index
     * @param position New spatial position
     */
    void setPlanePosition(int index, const PlanePosition& position);

    // -- Volume measurement API --

    /**
     * @brief Set volume statistics data for the 3D volume tab
     * @param rows Vector of volume stat rows
     */
    void setVolumeStatistics(const std::vector<VolumeStatRow>& rows);

    /**
     * @brief Get the number of rows in the volume statistics table
     */
    [[nodiscard]] int volumeRowCount() const;

    /**
     * @brief Clear all volume statistics data
     */
    void clearVolumeStatistics();

    // -- Inline contour editing API --

    /**
     * @brief Enable or disable inline contour editing tools
     * @param enabled True to enable brush/eraser toolbar
     */
    void setEditingEnabled(bool enabled);

    /**
     * @brief Check if contour editing is enabled
     */
    [[nodiscard]] bool isEditingEnabled() const;

    /**
     * @brief Set undo/redo button enabled states
     * @param canUndo True to enable undo button
     * @param canRedo True to enable redo button
     */
    void setUndoRedoEnabled(bool canUndo, bool canRedo);

    /**
     * @brief Get current brush size
     * @return Brush radius in pixels (1-20)
     */
    [[nodiscard]] int brushSize() const;

    /**
     * @brief Check if brush tool is active (vs eraser)
     * @return True for brush, false for eraser
     */
    [[nodiscard]] bool isBrushActive() const;

    // -- 3D Volume visualization API --

    /**
     * @brief Set velocity field for 3D streamline rendering in Volume tab
     * @param velocityField 3D vtkImageData with 3-component velocity vectors
     */
    void setVolumeVelocityField(vtkSmartPointer<vtkImageData> velocityField);

    /**
     * @brief Reset the 3D volume camera to fit all visible actors
     */
    void resetVolumeCamera();

    // -- Tab API --

    /**
     * @brief Get the active tab index (0=2D Plane, 1=3D Volume)
     */
    [[nodiscard]] int activeTab() const;

    /**
     * @brief Set the active tab by index
     * @param index Tab index (0=2D Plane, 1=3D Volume)
     */
    void setActiveTab(int index);

signals:
    /**
     * @brief Emitted when parameter checkbox state changes
     * @param param The parameter that changed
     * @param enabled New state
     */
    void parameterToggled(MeasurementParameter param, bool enabled);

    /**
     * @brief Emitted when Copy Summary is clicked
     * @param text The summary text that was copied
     */
    void summaryCopied(const QString& text);

    /**
     * @brief Emitted when user clicks a phase on the flow graph
     * @param phaseIndex Clicked phase index
     */
    void phaseChangeRequested(int phaseIndex);

    /**
     * @brief Emitted when flow direction flip state changes
     * @param flipped New flip state
     */
    void flowDirectionFlipped(bool flipped);

    /**
     * @brief Emitted when the active measurement plane changes
     * @param index New active plane index
     */
    void activePlaneChanged(int index);

    /**
     * @brief Emitted when the active tab changes (2D Plane / 3D Volume)
     * @param index New tab index
     */
    void activeTabChanged(int index);

    /**
     * @brief Emitted when a plane's spatial position changes
     * @param index Plane index whose position changed
     */
    void planePositionChanged(int index);

    /**
     * @brief Emitted when the active editing tool changes
     * @param isBrush True for brush, false for eraser
     */
    void editToolChanged(bool isBrush);

    /**
     * @brief Emitted when the brush size changes
     * @param size New brush radius in pixels
     */
    void editBrushSizeChanged(int size);

    /**
     * @brief Emitted when user requests undo of a contour edit
     */
    void contourUndoRequested();

    /**
     * @brief Emitted when user requests redo of a contour edit
     */
    void contourRedoRequested();

private:
    void setupUI();
    void setupConnections();
    void updateTable();
    void applyFlowDirectionToGraph();
    void updatePlaneButtons();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

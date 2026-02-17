#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QMainWindow>

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

private:
    void setupUI();
    void setupConnections();
    void updateTable();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

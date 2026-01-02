#pragma once

#include "services/measurement/roi_statistics.hpp"

#include <memory>
#include <vector>

#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Panel for displaying ROI statistics
 *
 * Displays comprehensive statistics for selected ROIs including:
 * - Basic statistics (mean, std dev, min, max, median)
 * - Histogram visualization
 * - Multi-ROI comparison
 * - CSV export functionality
 *
 * @trace SRS-FR-028
 */
class StatisticsPanel : public QWidget {
    Q_OBJECT

public:
    explicit StatisticsPanel(QWidget* parent = nullptr);
    ~StatisticsPanel() override;

    // Non-copyable
    StatisticsPanel(const StatisticsPanel&) = delete;
    StatisticsPanel& operator=(const StatisticsPanel&) = delete;

    /**
     * @brief Set statistics for a single ROI
     * @param stats ROI statistics to display
     */
    void setStatistics(const services::RoiStatistics& stats);

    /**
     * @brief Set statistics for multiple ROIs
     * @param stats Vector of ROI statistics
     */
    void setMultipleStatistics(const std::vector<services::RoiStatistics>& stats);

    /**
     * @brief Clear all statistics display
     */
    void clearStatistics();

    /**
     * @brief Get currently displayed statistics
     * @return Vector of current statistics
     */
    [[nodiscard]] std::vector<services::RoiStatistics> getStatistics() const;

    /**
     * @brief Set histogram display range
     * @param minValue Minimum value
     * @param maxValue Maximum value
     */
    void setHistogramRange(double minValue, double maxValue);

    /**
     * @brief Set number of histogram bins
     * @param bins Number of bins
     */
    void setHistogramBins(int bins);

signals:
    /**
     * @brief Emitted when export to CSV is requested
     */
    void exportRequested(const QString& filePath);

    /**
     * @brief Emitted when an ROI is selected for detailed view
     */
    void roiSelected(int roiId);

    /**
     * @brief Emitted when comparison mode is toggled
     */
    void comparisonModeChanged(bool enabled);

private slots:
    void onExportClicked();
    void onRoiSelectionChanged(int index);
    void onCompareButtonClicked();
    void onHistogramBinsChanged(int value);

private:
    void setupUI();
    void setupConnections();
    void createStatisticsTable();
    void createHistogramWidget();
    void createComparisonSection();
    void createExportSection();
    void updateStatisticsTable();
    void updateHistogram();
    void updateComparison();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::ui

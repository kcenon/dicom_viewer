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

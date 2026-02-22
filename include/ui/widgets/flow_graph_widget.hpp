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
 * @file flow_graph_widget.hpp
 * @brief Custom QPainter-based time-series graph widget
 * @details Renders flow rate curves over cardiac phases with multi-plane
 *          display, auto/manual Y-axis scaling, and phase marker.
 *          Uses QPainter directly to avoid Qt Charts dependency.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QWidget-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QColor>
#include <QPixmap>
#include <QString>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Time-series data for a single measurement plane
 */
struct FlowTimeSeries {
    QString planeName;              ///< Display name (e.g., "Plane 1")
    QColor color = Qt::blue;        ///< Line color
    std::vector<double> values;     ///< One value per cardiac phase
};

/**
 * @brief Custom QPainter-based time-series graph widget
 *
 * Renders flow rate curves over cardiac phases with multi-plane
 * display, auto/manual Y-axis scaling, and phase marker.
 * Uses QPainter directly to avoid Qt Charts dependency.
 *
 * @trace SRS-FR-046
 */
class FlowGraphWidget : public QWidget {
    Q_OBJECT

public:
    explicit FlowGraphWidget(QWidget* parent = nullptr);
    ~FlowGraphWidget() override;

    // Non-copyable
    FlowGraphWidget(const FlowGraphWidget&) = delete;
    FlowGraphWidget& operator=(const FlowGraphWidget&) = delete;

    /**
     * @brief Add a time-series data set
     * @param series Series data with name, color, and values
     */
    void addSeries(const FlowTimeSeries& series);

    /**
     * @brief Remove all series
     */
    void clearSeries();

    /**
     * @brief Get number of currently loaded series
     */
    [[nodiscard]] int seriesCount() const;

    /**
     * @brief Get a series by index
     * @param index 0-based series index
     * @return Series data, or empty series if out of range
     */
    [[nodiscard]] FlowTimeSeries series(int index) const;

    /**
     * @brief Set the current phase marker position
     * @param phaseIndex 0-based phase index
     */
    void setPhaseMarker(int phaseIndex);

    /**
     * @brief Get current phase marker position
     */
    [[nodiscard]] int phaseMarker() const;

    /**
     * @brief Enable or disable automatic Y-axis scaling
     * @param enabled True for auto-scale (default)
     */
    void setAutoScale(bool enabled);

    /**
     * @brief Check if auto-scale is enabled
     */
    [[nodiscard]] bool isAutoScale() const;

    /**
     * @brief Set manual Y-axis range (disables auto-scale)
     * @param min Minimum Y value
     * @param max Maximum Y value
     */
    void setYRange(double min, double max);

    /**
     * @brief Get current Y-axis minimum
     */
    [[nodiscard]] double yMin() const;

    /**
     * @brief Get current Y-axis maximum
     */
    [[nodiscard]] double yMax() const;

    /**
     * @brief Set X-axis label text
     */
    void setXAxisLabel(const QString& label);

    /**
     * @brief Set Y-axis label text
     */
    void setYAxisLabel(const QString& label);

    /**
     * @brief Get tab-separated chart data as text
     * @return Formatted data suitable for spreadsheet paste
     */
    [[nodiscard]] QString chartDataText() const;

    /**
     * @brief Render chart to a pixmap image
     * @return Chart image at current widget size
     */
    [[nodiscard]] QPixmap chartImage() const;

signals:
    /**
     * @brief Emitted when user clicks on a phase in the chart
     * @param phaseIndex Clicked phase index
     */
    void phaseClicked(int phaseIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void recalculateYRange();
    QRectF plotArea() const;
    double mapXToPixel(int phaseIndex) const;
    double mapYToPixel(double value) const;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

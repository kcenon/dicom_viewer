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

#include "ui/widgets/flow_graph_widget.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace dicom_viewer::ui {

class FlowGraphWidget::Impl {
public:
    std::vector<FlowTimeSeries> seriesList;
    int phaseMarkerIndex = -1;
    bool autoScale = true;
    double yMinVal = 0.0;
    double yMaxVal = 1.0;
    QString xLabel = "Phase";
    QString yLabel = "Value";

    // Layout constants
    static constexpr int marginLeft = 60;
    static constexpr int marginRight = 20;
    static constexpr int marginTop = 30;
    static constexpr int marginBottom = 40;
    static constexpr int legendLineLength = 20;
    static constexpr int legendSpacing = 15;
};

FlowGraphWidget::FlowGraphWidget(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

FlowGraphWidget::~FlowGraphWidget() = default;

void FlowGraphWidget::addSeries(const FlowTimeSeries& series)
{
    impl_->seriesList.push_back(series);
    if (impl_->autoScale) {
        recalculateYRange();
    }
    update();
}

void FlowGraphWidget::clearSeries()
{
    impl_->seriesList.clear();
    impl_->phaseMarkerIndex = -1;
    if (impl_->autoScale) {
        impl_->yMinVal = 0.0;
        impl_->yMaxVal = 1.0;
    }
    update();
}

int FlowGraphWidget::seriesCount() const
{
    return static_cast<int>(impl_->seriesList.size());
}

FlowTimeSeries FlowGraphWidget::series(int index) const
{
    if (index < 0 || index >= seriesCount()) return {};
    return impl_->seriesList[index];
}

void FlowGraphWidget::setPhaseMarker(int phaseIndex)
{
    if (impl_->phaseMarkerIndex == phaseIndex) return;
    impl_->phaseMarkerIndex = phaseIndex;
    update();
}

int FlowGraphWidget::phaseMarker() const
{
    return impl_->phaseMarkerIndex;
}

void FlowGraphWidget::setAutoScale(bool enabled)
{
    impl_->autoScale = enabled;
    if (enabled) {
        recalculateYRange();
        update();
    }
}

bool FlowGraphWidget::isAutoScale() const
{
    return impl_->autoScale;
}

void FlowGraphWidget::setYRange(double min, double max)
{
    impl_->autoScale = false;
    impl_->yMinVal = min;
    impl_->yMaxVal = max;
    update();
}

double FlowGraphWidget::yMin() const
{
    return impl_->yMinVal;
}

double FlowGraphWidget::yMax() const
{
    return impl_->yMaxVal;
}

void FlowGraphWidget::setXAxisLabel(const QString& label)
{
    impl_->xLabel = label;
    update();
}

void FlowGraphWidget::setYAxisLabel(const QString& label)
{
    impl_->yLabel = label;
    update();
}

QString FlowGraphWidget::chartDataText() const
{
    if (impl_->seriesList.empty()) return {};

    // Find max phase count
    int maxPhases = 0;
    for (const auto& s : impl_->seriesList) {
        maxPhases = std::max(maxPhases, static_cast<int>(s.values.size()));
    }

    // Header: Phase\tPlane1\tPlane2\t...
    QString text = "Phase";
    for (const auto& s : impl_->seriesList) {
        text += "\t" + s.planeName;
    }
    text += "\n";

    // Data rows
    for (int p = 0; p < maxPhases; ++p) {
        text += QString::number(p + 1);
        for (const auto& s : impl_->seriesList) {
            text += "\t";
            if (p < static_cast<int>(s.values.size())) {
                text += QString::number(s.values[p], 'f', 3);
            }
        }
        text += "\n";
    }

    return text;
}

QPixmap FlowGraphWidget::chartImage() const
{
    QPixmap offscreen(size());
    offscreen.fill(palette().color(QPalette::Window));
    const_cast<FlowGraphWidget*>(this)->render(&offscreen);
    return offscreen;
}

void FlowGraphWidget::recalculateYRange()
{
    if (impl_->seriesList.empty()) {
        impl_->yMinVal = 0.0;
        impl_->yMaxVal = 1.0;
        return;
    }

    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();

    for (const auto& s : impl_->seriesList) {
        for (double v : s.values) {
            minVal = std::min(minVal, v);
            maxVal = std::max(maxVal, v);
        }
    }

    // Add 10% padding
    double range = maxVal - minVal;
    if (range < 1e-9) range = 1.0;
    impl_->yMinVal = minVal - range * 0.1;
    impl_->yMaxVal = maxVal + range * 0.1;
}

QRectF FlowGraphWidget::plotArea() const
{
    return QRectF(
        Impl::marginLeft,
        Impl::marginTop,
        width() - Impl::marginLeft - Impl::marginRight,
        height() - Impl::marginTop - Impl::marginBottom);
}

double FlowGraphWidget::mapXToPixel(int phaseIndex) const
{
    auto area = plotArea();
    int maxPhases = 0;
    for (const auto& s : impl_->seriesList) {
        maxPhases = std::max(maxPhases, static_cast<int>(s.values.size()));
    }
    if (maxPhases <= 1) return area.left() + area.width() / 2.0;
    return area.left() + (static_cast<double>(phaseIndex) / (maxPhases - 1)) * area.width();
}

double FlowGraphWidget::mapYToPixel(double value) const
{
    auto area = plotArea();
    double range = impl_->yMaxVal - impl_->yMinVal;
    if (range < 1e-9) range = 1.0;
    double normalized = (value - impl_->yMinVal) / range;
    return area.bottom() - normalized * area.height();
}

void FlowGraphWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    auto area = plotArea();
    QColor bgColor = palette().color(QPalette::Window);
    QColor textColor = palette().color(QPalette::WindowText);
    QColor gridColor = textColor;
    gridColor.setAlpha(40);

    // Background
    painter.fillRect(rect(), bgColor);

    // Plot area background (slightly different)
    QColor plotBg = bgColor.lighter(105);
    painter.fillRect(area.toRect(), plotBg);

    // Grid lines and Y-axis labels
    painter.setPen(QPen(gridColor, 1, Qt::DashLine));
    int yTicks = 5;
    for (int i = 0; i <= yTicks; ++i) {
        double val = impl_->yMinVal + (impl_->yMaxVal - impl_->yMinVal) * i / yTicks;
        double y = mapYToPixel(val);
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));

        painter.setPen(textColor);
        QString label = QString::number(val, 'f', 1);
        QRectF labelRect(0, y - 10, Impl::marginLeft - 5, 20);
        painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, label);
        painter.setPen(QPen(gridColor, 1, Qt::DashLine));
    }

    // X-axis tick labels
    int maxPhases = 0;
    for (const auto& s : impl_->seriesList) {
        maxPhases = std::max(maxPhases, static_cast<int>(s.values.size()));
    }

    if (maxPhases > 0) {
        painter.setPen(textColor);
        int step = std::max(1, maxPhases / 10);
        for (int p = 0; p < maxPhases; p += step) {
            double x = mapXToPixel(p);
            QRectF labelRect(x - 15, area.bottom() + 2, 30, 18);
            painter.drawText(labelRect, Qt::AlignCenter, QString::number(p + 1));
        }
    }

    // Axis labels
    painter.setPen(textColor);
    QRectF xLabelRect(area.left(), height() - 18, area.width(), 18);
    painter.drawText(xLabelRect, Qt::AlignCenter, impl_->xLabel);

    painter.save();
    painter.translate(12, area.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-area.height() / 2, 0, area.height(), 16),
                     Qt::AlignCenter, impl_->yLabel);
    painter.restore();

    // Axes
    painter.setPen(QPen(textColor, 1));
    painter.drawLine(QPointF(area.left(), area.top()),
                     QPointF(area.left(), area.bottom()));
    painter.drawLine(QPointF(area.left(), area.bottom()),
                     QPointF(area.right(), area.bottom()));

    // Phase marker
    if (impl_->phaseMarkerIndex >= 0 && maxPhases > 0) {
        double mx = mapXToPixel(impl_->phaseMarkerIndex);
        QColor markerColor(255, 165, 0, 120);  // Orange semi-transparent
        painter.setPen(QPen(markerColor, 2, Qt::DashDotLine));
        painter.drawLine(QPointF(mx, area.top()), QPointF(mx, area.bottom()));
    }

    // Series lines
    for (const auto& s : impl_->seriesList) {
        if (s.values.size() < 2) continue;

        QPainterPath path;
        path.moveTo(mapXToPixel(0), mapYToPixel(s.values[0]));

        for (int i = 1; i < static_cast<int>(s.values.size()); ++i) {
            path.lineTo(mapXToPixel(i), mapYToPixel(s.values[i]));
        }

        painter.setPen(QPen(s.color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        // Data point markers
        painter.setPen(QPen(s.color, 1));
        painter.setBrush(s.color);
        for (int i = 0; i < static_cast<int>(s.values.size()); ++i) {
            double px = mapXToPixel(i);
            double py = mapYToPixel(s.values[i]);
            painter.drawEllipse(QPointF(px, py), 3, 3);
        }
    }

    // Legend (top-right)
    if (!impl_->seriesList.empty()) {
        int legendX = static_cast<int>(area.right()) - 150;
        int legendY = static_cast<int>(area.top()) + 5;

        for (int i = 0; i < static_cast<int>(impl_->seriesList.size()); ++i) {
            const auto& s = impl_->seriesList[i];
            int ly = legendY + i * Impl::legendSpacing;

            painter.setPen(QPen(s.color, 2));
            painter.drawLine(legendX, ly + 6, legendX + Impl::legendLineLength, ly + 6);

            painter.setPen(textColor);
            painter.drawText(legendX + Impl::legendLineLength + 5, ly + 10, s.planeName);
        }
    }
}

void FlowGraphWidget::mousePressEvent(QMouseEvent* event)
{
    auto area = plotArea();
    if (!area.contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    int maxPhases = 0;
    for (const auto& s : impl_->seriesList) {
        maxPhases = std::max(maxPhases, static_cast<int>(s.values.size()));
    }
    if (maxPhases == 0) return;

    // Find nearest phase to click X
    double clickX = event->pos().x();
    int bestPhase = 0;
    double bestDist = std::abs(clickX - mapXToPixel(0));

    for (int p = 1; p < maxPhases; ++p) {
        double dist = std::abs(clickX - mapXToPixel(p));
        if (dist < bestDist) {
            bestDist = dist;
            bestPhase = p;
        }
    }

    setPhaseMarker(bestPhase);
    emit phaseClicked(bestPhase);
}

} // namespace dicom_viewer::ui

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

#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "ui/widgets/flow_graph_widget.hpp"

using namespace dicom_viewer::ui;

namespace {

// QApplication must exist for QWidget instantiation
int argc = 0;
char* argv[] = {nullptr};
QApplication app(argc, argv);

FlowTimeSeries makeSeries(const QString& name, QColor color,
                          std::vector<double> values)
{
    FlowTimeSeries s;
    s.planeName = name;
    s.color = color;
    s.values = std::move(values);
    return s;
}

}  // anonymous namespace

// =============================================================================
// Construction and defaults
// =============================================================================

TEST(FlowGraphWidgetTest, DefaultConstruction) {
    FlowGraphWidget widget;
    EXPECT_EQ(widget.seriesCount(), 0);
    EXPECT_EQ(widget.phaseMarker(), -1);
    EXPECT_TRUE(widget.isAutoScale());
}

TEST(FlowGraphWidgetTest, MinimumSize) {
    FlowGraphWidget widget;
    EXPECT_GE(widget.minimumWidth(), 200);
    EXPECT_GE(widget.minimumHeight(), 150);
}

// =============================================================================
// Series management
// =============================================================================

TEST(FlowGraphWidgetTest, AddSeries) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("Plane 1", Qt::blue, {1.0, 2.0, 3.0}));
    EXPECT_EQ(widget.seriesCount(), 1);

    widget.addSeries(makeSeries("Plane 2", Qt::red, {4.0, 5.0, 6.0}));
    EXPECT_EQ(widget.seriesCount(), 2);
}

TEST(FlowGraphWidgetTest, GetSeries) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("Plane 1", Qt::blue, {10.0, 20.0}));

    FlowTimeSeries s = widget.series(0);
    EXPECT_EQ(s.planeName, "Plane 1");
    EXPECT_EQ(s.values.size(), 2u);
    EXPECT_DOUBLE_EQ(s.values[0], 10.0);
    EXPECT_DOUBLE_EQ(s.values[1], 20.0);
}

TEST(FlowGraphWidgetTest, GetSeries_OutOfRange) {
    FlowGraphWidget widget;
    FlowTimeSeries s = widget.series(0);
    EXPECT_TRUE(s.planeName.isEmpty());
    EXPECT_TRUE(s.values.empty());

    s = widget.series(-1);
    EXPECT_TRUE(s.values.empty());
}

TEST(FlowGraphWidgetTest, ClearSeries) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("A", Qt::blue, {1.0, 2.0}));
    widget.addSeries(makeSeries("B", Qt::red, {3.0, 4.0}));
    EXPECT_EQ(widget.seriesCount(), 2);

    widget.clearSeries();
    EXPECT_EQ(widget.seriesCount(), 0);
    EXPECT_EQ(widget.phaseMarker(), -1);
}

// =============================================================================
// Phase marker
// =============================================================================

TEST(FlowGraphWidgetTest, SetPhaseMarker) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("P", Qt::blue, {1.0, 2.0, 3.0, 4.0}));

    widget.setPhaseMarker(2);
    EXPECT_EQ(widget.phaseMarker(), 2);

    widget.setPhaseMarker(0);
    EXPECT_EQ(widget.phaseMarker(), 0);
}

TEST(FlowGraphWidgetTest, PhaseClicked_Signal) {
    FlowGraphWidget widget;
    widget.resize(400, 300);

    widget.addSeries(makeSeries("P", Qt::blue, {1.0, 2.0, 3.0}));

    QSignalSpy spy(&widget, &FlowGraphWidget::phaseClicked);
    ASSERT_TRUE(spy.isValid());

    // Simulate click in the plot area center
    QPoint center(widget.width() / 2, widget.height() / 2);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, center);

    EXPECT_GE(spy.count(), 1);
    int clickedPhase = spy.at(0).at(0).toInt();
    EXPECT_GE(clickedPhase, 0);
    EXPECT_LT(clickedPhase, 3);
}

// =============================================================================
// Y-axis scaling
// =============================================================================

TEST(FlowGraphWidgetTest, AutoScale_Default) {
    FlowGraphWidget widget;
    EXPECT_TRUE(widget.isAutoScale());
}

TEST(FlowGraphWidgetTest, AutoScale_RecalculatesOnAddSeries) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("P", Qt::blue, {10.0, 20.0, 30.0}));

    // Auto-scale should set range based on data + padding
    EXPECT_LT(widget.yMin(), 10.0);
    EXPECT_GT(widget.yMax(), 30.0);
}

TEST(FlowGraphWidgetTest, ManualYRange) {
    FlowGraphWidget widget;

    widget.setYRange(-5.0, 50.0);
    EXPECT_FALSE(widget.isAutoScale());
    EXPECT_DOUBLE_EQ(widget.yMin(), -5.0);
    EXPECT_DOUBLE_EQ(widget.yMax(), 50.0);
}

TEST(FlowGraphWidgetTest, SetAutoScale_ReenablesAutoScale) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("P", Qt::blue, {10.0, 20.0}));
    widget.setYRange(0.0, 100.0);
    EXPECT_FALSE(widget.isAutoScale());

    widget.setAutoScale(true);
    EXPECT_TRUE(widget.isAutoScale());
    // Should recalculate based on data
    EXPECT_LT(widget.yMin(), 10.0);
    EXPECT_GT(widget.yMax(), 20.0);
}

// =============================================================================
// Axis labels
// =============================================================================

TEST(FlowGraphWidgetTest, AxisLabels) {
    FlowGraphWidget widget;
    // Just verify no crash — labels are painted, not queryable
    widget.setXAxisLabel("Cardiac Phase");
    widget.setYAxisLabel("Flow Rate (mL/s)");
}

// =============================================================================
// Chart data export
// =============================================================================

TEST(FlowGraphWidgetTest, ChartDataText_Empty) {
    FlowGraphWidget widget;
    EXPECT_TRUE(widget.chartDataText().isEmpty());
}

TEST(FlowGraphWidgetTest, ChartDataText_SingleSeries) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("Plane 1", Qt::blue, {1.5, 2.5, 3.5}));

    QString text = widget.chartDataText();
    EXPECT_TRUE(text.contains("Phase"));
    EXPECT_TRUE(text.contains("Plane 1"));
    EXPECT_TRUE(text.contains("1.500"));
    EXPECT_TRUE(text.contains("2.500"));
    EXPECT_TRUE(text.contains("3.500"));
}

TEST(FlowGraphWidgetTest, ChartDataText_MultipleSeries) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("A", Qt::blue, {1.0, 2.0}));
    widget.addSeries(makeSeries("B", Qt::red, {3.0, 4.0}));

    QString text = widget.chartDataText();

    // Header should contain both plane names
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    ASSERT_GE(lines.size(), 3);  // header + 2 data rows

    EXPECT_TRUE(lines[0].contains("A"));
    EXPECT_TRUE(lines[0].contains("B"));
}

TEST(FlowGraphWidgetTest, ChartDataText_TabSeparated) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("P", Qt::blue, {1.0}));

    QString text = widget.chartDataText();
    EXPECT_TRUE(text.contains('\t'));
}

// =============================================================================
// Chart image
// =============================================================================

TEST(FlowGraphWidgetTest, ChartImage_NotEmpty) {
    FlowGraphWidget widget;
    widget.resize(400, 300);

    widget.addSeries(makeSeries("P", Qt::blue, {1.0, 2.0, 3.0}));

    QPixmap image = widget.chartImage();
    EXPECT_FALSE(image.isNull());
    EXPECT_EQ(image.width(), 400);
    EXPECT_EQ(image.height(), 300);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(FlowGraphWidgetTest, SingleValueSeries) {
    FlowGraphWidget widget;
    widget.addSeries(makeSeries("P", Qt::blue, {42.0}));
    EXPECT_EQ(widget.seriesCount(), 1);
    // Should not crash on paint
    widget.resize(200, 150);
    widget.repaint();
}

TEST(FlowGraphWidgetTest, ClearSeries_ResetsYRange) {
    FlowGraphWidget widget;

    widget.addSeries(makeSeries("P", Qt::blue, {100.0, 200.0}));
    EXPECT_GT(widget.yMax(), 100.0);

    widget.clearSeries();
    EXPECT_DOUBLE_EQ(widget.yMin(), 0.0);
    EXPECT_DOUBLE_EQ(widget.yMax(), 1.0);
}

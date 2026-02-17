#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>

#include "ui/quantification_window.hpp"
#include "ui/widgets/flow_graph_widget.hpp"

using namespace dicom_viewer::ui;

namespace {

// QApplication must exist for QWidget instantiation
int argc = 0;
char* argv[] = {nullptr};
QApplication app(argc, argv);

}  // anonymous namespace

// =============================================================================
// Construction and defaults
// =============================================================================

TEST(QuantificationWindowTest, DefaultConstruction) {
    QuantificationWindow window;
    EXPECT_EQ(window.windowTitle(), "Quantification");
    EXPECT_EQ(window.rowCount(), 0);
    EXPECT_TRUE(window.getStatistics().empty());
}

TEST(QuantificationWindowTest, AllParametersEnabledByDefault) {
    QuantificationWindow window;
    EXPECT_TRUE(window.isParameterEnabled(MeasurementParameter::FlowRate));
    EXPECT_TRUE(window.isParameterEnabled(MeasurementParameter::PeakVelocity));
    EXPECT_TRUE(window.isParameterEnabled(MeasurementParameter::MeanVelocity));
    EXPECT_TRUE(window.isParameterEnabled(MeasurementParameter::KineticEnergy));
    EXPECT_TRUE(window.isParameterEnabled(MeasurementParameter::RegurgitantFraction));
    EXPECT_TRUE(window.isParameterEnabled(MeasurementParameter::StrokeVolume));
}

// =============================================================================
// Statistics table
// =============================================================================

TEST(QuantificationWindowTest, SetStatistics_PopulatesTable) {
    QuantificationWindow window;

    std::vector<QuantificationRow> rows = {
        {MeasurementParameter::FlowRate, 10.5, 2.3, 15.0, 6.0},
        {MeasurementParameter::PeakVelocity, 120.0, 15.0, 150.0, 90.0},
    };

    window.setStatistics(rows);

    EXPECT_EQ(window.rowCount(), 2);
    EXPECT_EQ(window.getStatistics().size(), 2u);
}

TEST(QuantificationWindowTest, ClearStatistics) {
    QuantificationWindow window;

    std::vector<QuantificationRow> rows = {
        {MeasurementParameter::FlowRate, 10.5, 2.3, 15.0, 6.0},
    };

    window.setStatistics(rows);
    EXPECT_EQ(window.rowCount(), 1);

    window.clearStatistics();
    EXPECT_EQ(window.rowCount(), 0);
    EXPECT_TRUE(window.getStatistics().empty());
}

TEST(QuantificationWindowTest, SetStatistics_ReplacesExisting) {
    QuantificationWindow window;

    window.setStatistics({{MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0}});
    EXPECT_EQ(window.rowCount(), 1);

    window.setStatistics({
        {MeasurementParameter::PeakVelocity, 100.0, 10.0, 120.0, 80.0},
        {MeasurementParameter::MeanVelocity, 50.0, 5.0, 60.0, 40.0},
        {MeasurementParameter::KineticEnergy, 3.0, 0.5, 4.0, 2.0},
    });
    EXPECT_EQ(window.rowCount(), 3);
}

// =============================================================================
// Parameter checkboxes
// =============================================================================

TEST(QuantificationWindowTest, DisableParameter_HidesRow) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0},
        {MeasurementParameter::PeakVelocity, 100.0, 10.0, 120.0, 80.0},
    });
    EXPECT_EQ(window.rowCount(), 2);

    window.setParameterEnabled(MeasurementParameter::FlowRate, false);
    EXPECT_EQ(window.rowCount(), 1);
}

TEST(QuantificationWindowTest, ReEnableParameter_ShowsRow) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0},
        {MeasurementParameter::PeakVelocity, 100.0, 10.0, 120.0, 80.0},
    });

    window.setParameterEnabled(MeasurementParameter::FlowRate, false);
    EXPECT_EQ(window.rowCount(), 1);

    window.setParameterEnabled(MeasurementParameter::FlowRate, true);
    EXPECT_EQ(window.rowCount(), 2);
}

TEST(QuantificationWindowTest, ParameterToggled_Signal) {
    QuantificationWindow window;

    bool signalReceived = false;
    MeasurementParameter receivedParam = MeasurementParameter::FlowRate;
    bool receivedEnabled = true;

    QObject::connect(&window, &QuantificationWindow::parameterToggled,
                     [&](MeasurementParameter p, bool e) {
        signalReceived = true;
        receivedParam = p;
        receivedEnabled = e;
    });

    window.setParameterEnabled(MeasurementParameter::PeakVelocity, false);

    EXPECT_TRUE(signalReceived);
    EXPECT_EQ(receivedParam, MeasurementParameter::PeakVelocity);
    EXPECT_FALSE(receivedEnabled);
}

// =============================================================================
// Copy Summary
// =============================================================================

TEST(QuantificationWindowTest, SummaryText_ContainsHeader) {
    QuantificationWindow window;
    QString text = window.summaryText();
    EXPECT_TRUE(text.contains("Parameter"));
    EXPECT_TRUE(text.contains("Mean"));
    EXPECT_TRUE(text.contains("Std Dev"));
    EXPECT_TRUE(text.contains("Max"));
    EXPECT_TRUE(text.contains("Min"));
}

TEST(QuantificationWindowTest, SummaryText_ContainsData) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.50, 2.30, 15.00, 6.00},
    });

    QString text = window.summaryText();
    EXPECT_TRUE(text.contains("Flow Rate"));
    EXPECT_TRUE(text.contains("10.50"));
    EXPECT_TRUE(text.contains("mL/s"));
}

TEST(QuantificationWindowTest, SummaryText_ExcludesDisabledParams) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0},
        {MeasurementParameter::PeakVelocity, 100.0, 10.0, 120.0, 80.0},
    });

    window.setParameterEnabled(MeasurementParameter::FlowRate, false);

    QString text = window.summaryText();
    EXPECT_FALSE(text.contains("Flow Rate"));
    EXPECT_TRUE(text.contains("Peak Velocity"));
}

TEST(QuantificationWindowTest, SummaryCopied_Signal) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0},
    });

    QSignalSpy spy(&window, &QuantificationWindow::summaryCopied);
    ASSERT_TRUE(spy.isValid());

    // Find "Copy Summary" button specifically (multiple QPushButtons exist)
    QPushButton* btn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Copy Summary") {
            btn = b;
            break;
        }
    }
    ASSERT_NE(btn, nullptr);
    btn->click();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.at(0).at(0).toString().isEmpty());
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(QuantificationWindowTest, EmptyStatistics_ZeroRows) {
    QuantificationWindow window;
    window.setStatistics({});
    EXPECT_EQ(window.rowCount(), 0);
}

TEST(QuantificationWindowTest, AllParametersDisabled_ZeroRows) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0},
    });

    window.setParameterEnabled(MeasurementParameter::FlowRate, false);
    EXPECT_EQ(window.rowCount(), 0);
}

// =============================================================================
// Phase sync
// =============================================================================

TEST(QuantificationWindowTest, PhaseChangeRequested_Signal) {
    QuantificationWindow window;
    window.resize(1000, 600);

    auto* graph = window.graphWidget();
    ASSERT_NE(graph, nullptr);

    FlowTimeSeries s;
    s.planeName = "Test";
    s.color = Qt::blue;
    s.values = {1.0, 2.0, 3.0};
    graph->addSeries(s);

    QSignalSpy spy(&window, &QuantificationWindow::phaseChangeRequested);
    ASSERT_TRUE(spy.isValid());

    // Simulate clicking a phase on the graph
    emit graph->phaseClicked(1);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 1);
}

// =============================================================================
// Flow direction flip
// =============================================================================

TEST(QuantificationWindowTest, FlowDirectionFlip_Default) {
    QuantificationWindow window;
    EXPECT_FALSE(window.isFlowDirectionFlipped());
}

TEST(QuantificationWindowTest, FlowDirectionFlip_NegatesValues) {
    QuantificationWindow window;

    auto* graph = window.graphWidget();
    FlowTimeSeries s;
    s.planeName = "Test";
    s.color = Qt::blue;
    s.values = {10.0, -5.0, 20.0};
    graph->addSeries(s);

    window.setFlowDirectionFlipped(true);
    EXPECT_TRUE(window.isFlowDirectionFlipped());

    // Values should be negated
    auto flipped = graph->series(0);
    EXPECT_DOUBLE_EQ(flipped.values[0], -10.0);
    EXPECT_DOUBLE_EQ(flipped.values[1], 5.0);
    EXPECT_DOUBLE_EQ(flipped.values[2], -20.0);
}

TEST(QuantificationWindowTest, FlowDirectionFlip_Signal) {
    QuantificationWindow window;

    QSignalSpy spy(&window, &QuantificationWindow::flowDirectionFlipped);
    ASSERT_TRUE(spy.isValid());

    window.setFlowDirectionFlipped(true);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.at(0).at(0).toBool());
}

TEST(QuantificationWindowTest, FlowDirectionFlip_DoubleFlipRestores) {
    QuantificationWindow window;

    auto* graph = window.graphWidget();
    FlowTimeSeries s;
    s.planeName = "Test";
    s.color = Qt::blue;
    s.values = {10.0, 20.0, 30.0};
    graph->addSeries(s);

    window.setFlowDirectionFlipped(true);
    window.setFlowDirectionFlipped(false);
    EXPECT_FALSE(window.isFlowDirectionFlipped());

    // Values should be restored
    auto restored = graph->series(0);
    EXPECT_DOUBLE_EQ(restored.values[0], 10.0);
    EXPECT_DOUBLE_EQ(restored.values[1], 20.0);
    EXPECT_DOUBLE_EQ(restored.values[2], 30.0);
}

// =============================================================================
// Export CSV button presence
// =============================================================================

TEST(QuantificationWindowTest, ExportCsvButton_Exists) {
    QuantificationWindow window;

    QPushButton* btn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Export CSV...") {
            btn = b;
            break;
        }
    }
    EXPECT_NE(btn, nullptr);
}

TEST(QuantificationWindowTest, FlipFlowButton_Exists) {
    QuantificationWindow window;

    QPushButton* btn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Flip Flow Direction") {
            btn = b;
            break;
        }
    }
    ASSERT_NE(btn, nullptr);
    EXPECT_TRUE(btn->isCheckable());
}

// =============================================================================
// Export PDF button and renderReport
// =============================================================================

TEST(QuantificationWindowTest, ExportPdfButton_Exists) {
    QuantificationWindow window;

    QPushButton* btn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Export PDF...") {
            btn = b;
            break;
        }
    }
    EXPECT_NE(btn, nullptr);
}

TEST(QuantificationWindowTest, RenderReport_Empty_NoCrash) {
    QuantificationWindow window;

    QImage image(800, 600, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should not crash with empty data
    window.renderReport(painter, QRectF(0, 0, 800, 600));
    painter.end();

    EXPECT_FALSE(image.isNull());
}

TEST(QuantificationWindowTest, RenderReport_WithData_DrawsContent) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.50, 2.30, 15.00, 6.00},
        {MeasurementParameter::PeakVelocity, 120.0, 15.0, 150.0, 90.0},
    });

    QImage image(800, 600, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    window.renderReport(painter, QRectF(0, 0, 800, 600));
    painter.end();

    // Verify that something was drawn (not all white)
    bool hasNonWhitePixel = false;
    for (int y = 0; y < image.height() && !hasNonWhitePixel; ++y) {
        for (int x = 0; x < image.width() && !hasNonWhitePixel; ++x) {
            if (image.pixelColor(x, y) != QColor(Qt::white)) {
                hasNonWhitePixel = true;
            }
        }
    }
    EXPECT_TRUE(hasNonWhitePixel);
}

TEST(QuantificationWindowTest, RenderReport_WithGraph_DrawsChart) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0},
    });

    auto* graph = window.graphWidget();
    FlowTimeSeries s;
    s.planeName = "Plane 1";
    s.color = Qt::blue;
    s.values = {5.0, 10.0, 15.0, 12.0, 8.0};
    graph->addSeries(s);

    QImage image(800, 600, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    window.renderReport(painter, QRectF(0, 0, 800, 600));
    painter.end();

    EXPECT_FALSE(image.isNull());
}

TEST(QuantificationWindowTest, RenderReport_DisabledParams_Excluded) {
    QuantificationWindow window;

    window.setStatistics({
        {MeasurementParameter::FlowRate, 10.0, 2.0, 15.0, 5.0},
        {MeasurementParameter::PeakVelocity, 100.0, 10.0, 120.0, 80.0},
    });
    window.setParameterEnabled(MeasurementParameter::FlowRate, false);

    QImage image(800, 600, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render without the disabled parameter's row
    window.renderReport(painter, QRectF(0, 0, 800, 600));
    painter.end();

    EXPECT_FALSE(image.isNull());
}

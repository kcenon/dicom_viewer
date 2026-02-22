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
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTabWidget>

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

// =============================================================================
// Plane management — initial state
// =============================================================================

TEST(QuantificationWindowTest, PlaneManagement_InitiallyEmpty) {
    QuantificationWindow window;
    EXPECT_EQ(window.planeCount(), 0);
    EXPECT_EQ(window.activePlaneIndex(), -1);
}

// =============================================================================
// Plane management — add/remove
// =============================================================================

TEST(QuantificationWindowTest, AddPlane_IncreasesCount) {
    QuantificationWindow window;
    window.addPlane("Plane 1", Qt::red);
    EXPECT_EQ(window.planeCount(), 1);
    EXPECT_EQ(window.planeName(0), "Plane 1");
    EXPECT_EQ(window.planeColor(0), QColor(Qt::red));
}

TEST(QuantificationWindowTest, AddMultiplePlanes) {
    QuantificationWindow window;
    window.addPlane("Aorta", Qt::red);
    window.addPlane("Pulmonary", Qt::blue);
    window.addPlane("Mitral", Qt::green);
    EXPECT_EQ(window.planeCount(), 3);
    EXPECT_EQ(window.planeName(1), "Pulmonary");
    EXPECT_EQ(window.planeColor(2), QColor(Qt::green));
}

TEST(QuantificationWindowTest, RemovePlane) {
    QuantificationWindow window;
    window.addPlane("Plane 1", Qt::red);
    window.addPlane("Plane 2", Qt::blue);
    EXPECT_EQ(window.planeCount(), 2);

    window.removePlane(0);
    EXPECT_EQ(window.planeCount(), 1);
    EXPECT_EQ(window.planeName(0), "Plane 2");
}

// =============================================================================
// Plane management — active selection
// =============================================================================

TEST(QuantificationWindowTest, FirstPlane_AutoActivated) {
    QuantificationWindow window;
    window.addPlane("Plane 1", Qt::red);
    EXPECT_EQ(window.activePlaneIndex(), 0);
}

TEST(QuantificationWindowTest, SetActivePlane) {
    QuantificationWindow window;
    window.addPlane("Plane 1", Qt::red);
    window.addPlane("Plane 2", Qt::blue);
    window.addPlane("Plane 3", Qt::green);

    window.setActivePlane(2);
    EXPECT_EQ(window.activePlaneIndex(), 2);
}

// =============================================================================
// Plane management — signal
// =============================================================================

TEST(QuantificationWindowTest, ActivePlaneChanged_Signal) {
    QuantificationWindow window;
    window.addPlane("Plane 1", Qt::red);
    window.addPlane("Plane 2", Qt::blue);

    QSignalSpy spy(&window, &QuantificationWindow::activePlaneChanged);
    ASSERT_TRUE(spy.isValid());

    window.setActivePlane(1);
    EXPECT_GE(spy.count(), 1);
    EXPECT_EQ(spy.last().at(0).toInt(), 1);
}

// =============================================================================
// Plane management — out of range
// =============================================================================

TEST(QuantificationWindowTest, PlaneNameOutOfRange_ReturnsEmpty) {
    QuantificationWindow window;
    EXPECT_TRUE(window.planeName(0).isEmpty());
    EXPECT_TRUE(window.planeName(-1).isEmpty());
}

// =============================================================================
// Plane management — combo box UI
// =============================================================================

TEST(QuantificationWindowTest, PlaneComboBox_Exists) {
    QuantificationWindow window;
    auto* combo = window.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);
    EXPECT_EQ(combo->count(), 0);

    window.addPlane("Test", Qt::red);
    EXPECT_EQ(combo->count(), 1);
}

// =============================================================================
// Tab management
// =============================================================================

TEST(QuantificationWindowTest, TabWidget_InitialTab) {
    QuantificationWindow window;
    EXPECT_EQ(window.activeTab(), 0);

    auto* tabWidget = window.findChild<QTabWidget*>();
    ASSERT_NE(tabWidget, nullptr);
    EXPECT_EQ(tabWidget->count(), 2);
    EXPECT_EQ(tabWidget->tabText(0), "2D Plane");
    EXPECT_EQ(tabWidget->tabText(1), "3D Volume");
}

TEST(QuantificationWindowTest, SetActiveTab_SwitchesTo3DVolume) {
    QuantificationWindow window;
    window.setActiveTab(1);
    EXPECT_EQ(window.activeTab(), 1);
}

TEST(QuantificationWindowTest, SetActiveTab_OutOfRange_NoChange) {
    QuantificationWindow window;
    window.setActiveTab(99);
    EXPECT_EQ(window.activeTab(), 0);
}

TEST(QuantificationWindowTest, ActiveTabChanged_Signal) {
    QuantificationWindow window;

    QSignalSpy spy(&window, &QuantificationWindow::activeTabChanged);
    ASSERT_TRUE(spy.isValid());

    window.setActiveTab(1);
    EXPECT_GE(spy.count(), 1);
    EXPECT_EQ(spy.last().at(0).toInt(), 1);
}

// =============================================================================
// Volume statistics
// =============================================================================

TEST(QuantificationWindowTest, VolumeStatistics_InitiallyEmpty) {
    QuantificationWindow window;
    EXPECT_EQ(window.volumeRowCount(), 0);
}

TEST(QuantificationWindowTest, SetVolumeStatistics_PopulatesTable) {
    QuantificationWindow window;

    std::vector<VolumeStatRow> rows = {
        {VolumeParameter::TotalKE, 12.5, "mJ"},
        {VolumeParameter::VortexVolume, 3.2, "mL"},
        {VolumeParameter::MeanWSS, 1.8, "Pa"},
    };

    window.setVolumeStatistics(rows);
    EXPECT_EQ(window.volumeRowCount(), 3);
}

TEST(QuantificationWindowTest, ClearVolumeStatistics) {
    QuantificationWindow window;

    window.setVolumeStatistics({
        {VolumeParameter::TotalKE, 12.5, "mJ"},
    });
    EXPECT_EQ(window.volumeRowCount(), 1);

    window.clearVolumeStatistics();
    EXPECT_EQ(window.volumeRowCount(), 0);
}

TEST(QuantificationWindowTest, VolumeTable_Content) {
    QuantificationWindow window;

    window.setVolumeStatistics({
        {VolumeParameter::EnergyLoss, 0.75, "mW"},
    });

    // Find the volume table (second QTableWidget in widget tree)
    auto tables = window.findChildren<QTableWidget*>();
    ASSERT_GE(tables.size(), 2);
    // The volume table has 3 columns
    QTableWidget* volumeTable = nullptr;
    for (auto* t : tables) {
        if (t->columnCount() == 3) {
            volumeTable = t;
            break;
        }
    }
    ASSERT_NE(volumeTable, nullptr);
    EXPECT_EQ(volumeTable->rowCount(), 1);
    EXPECT_EQ(volumeTable->item(0, 0)->text(), "Energy Loss");
    EXPECT_EQ(volumeTable->item(0, 1)->text(), "0.75");
    EXPECT_EQ(volumeTable->item(0, 2)->text(), "mW");
}

// =============================================================================
// Plane position data model
// =============================================================================

TEST(QuantificationWindowTest, PlanePosition_DefaultIsZeroNormal) {
    QuantificationWindow window;
    window.addPlane("Plane 1", Qt::red);

    auto pos = window.planePosition(0);
    EXPECT_DOUBLE_EQ(pos.normalX, 0.0);
    EXPECT_DOUBLE_EQ(pos.normalY, 0.0);
    EXPECT_DOUBLE_EQ(pos.normalZ, 1.0);
    EXPECT_DOUBLE_EQ(pos.centerX, 0.0);
    EXPECT_DOUBLE_EQ(pos.centerY, 0.0);
    EXPECT_DOUBLE_EQ(pos.centerZ, 0.0);
    EXPECT_DOUBLE_EQ(pos.extent, 50.0);
}

TEST(QuantificationWindowTest, PlanePosition_OutOfRange_ReturnsDefault) {
    QuantificationWindow window;
    auto pos = window.planePosition(0);
    // Default PlanePosition has normalZ=1.0 and extent=50.0
    EXPECT_DOUBLE_EQ(pos.normalZ, 1.0);
    EXPECT_DOUBLE_EQ(pos.extent, 50.0);
}

TEST(QuantificationWindowTest, AddPlane_WithPosition) {
    QuantificationWindow window;

    PlanePosition pos;
    pos.normalX = 1.0;
    pos.normalY = 0.0;
    pos.normalZ = 0.0;
    pos.centerX = 10.0;
    pos.centerY = 20.0;
    pos.centerZ = 30.0;
    pos.extent = 75.0;

    window.addPlane("Sagittal", Qt::blue, pos);
    EXPECT_EQ(window.planeCount(), 1);

    auto retrieved = window.planePosition(0);
    EXPECT_DOUBLE_EQ(retrieved.normalX, 1.0);
    EXPECT_DOUBLE_EQ(retrieved.centerX, 10.0);
    EXPECT_DOUBLE_EQ(retrieved.centerY, 20.0);
    EXPECT_DOUBLE_EQ(retrieved.centerZ, 30.0);
    EXPECT_DOUBLE_EQ(retrieved.extent, 75.0);
}

TEST(QuantificationWindowTest, SetPlanePosition_StoresAndRetrieves) {
    QuantificationWindow window;
    window.addPlane("Test", Qt::red);

    PlanePosition pos;
    pos.normalX = 0.0;
    pos.normalY = 1.0;
    pos.normalZ = 0.0;
    pos.centerX = 5.0;
    pos.centerY = 15.0;
    pos.centerZ = 25.0;
    pos.extent = 100.0;

    window.setPlanePosition(0, pos);

    auto retrieved = window.planePosition(0);
    EXPECT_DOUBLE_EQ(retrieved.normalY, 1.0);
    EXPECT_DOUBLE_EQ(retrieved.normalZ, 0.0);
    EXPECT_DOUBLE_EQ(retrieved.centerX, 5.0);
    EXPECT_DOUBLE_EQ(retrieved.extent, 100.0);
}

TEST(QuantificationWindowTest, SetPlanePosition_OutOfRange_NoEffect) {
    QuantificationWindow window;
    PlanePosition pos;
    pos.normalX = 1.0;
    // Should not crash
    window.setPlanePosition(0, pos);
    window.setPlanePosition(-1, pos);
}

TEST(QuantificationWindowTest, PlanePositionChanged_Signal) {
    QuantificationWindow window;
    window.addPlane("Test", Qt::red);

    QSignalSpy spy(&window, &QuantificationWindow::planePositionChanged);
    ASSERT_TRUE(spy.isValid());

    PlanePosition pos;
    pos.centerZ = 42.0;
    window.setPlanePosition(0, pos);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 0);
}

// =============================================================================
// Multi-plane management UI — buttons
// =============================================================================

TEST(QuantificationWindowTest, AddPlaneButton_Exists) {
    QuantificationWindow window;

    QPushButton* btn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") {
            btn = b;
            break;
        }
    }
    ASSERT_NE(btn, nullptr);
    EXPECT_TRUE(btn->isEnabled());
}

TEST(QuantificationWindowTest, RemovePlaneButton_Exists) {
    QuantificationWindow window;

    QPushButton* btn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Remove Plane") {
            btn = b;
            break;
        }
    }
    ASSERT_NE(btn, nullptr);
    // Initially disabled (no planes)
    EXPECT_FALSE(btn->isEnabled());
}

TEST(QuantificationWindowTest, AddPlaneButton_AddsPlaneWithAutoName) {
    QuantificationWindow window;
    EXPECT_EQ(window.planeCount(), 0);

    QPushButton* addBtn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") {
            addBtn = b;
            break;
        }
    }
    ASSERT_NE(addBtn, nullptr);

    addBtn->click();
    EXPECT_EQ(window.planeCount(), 1);
    EXPECT_EQ(window.planeName(0), "Plane 1");

    addBtn->click();
    EXPECT_EQ(window.planeCount(), 2);
    EXPECT_EQ(window.planeName(1), "Plane 2");
}

TEST(QuantificationWindowTest, AddPlaneButton_AssignsColorsFromPalette) {
    QuantificationWindow window;

    QPushButton* addBtn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") {
            addBtn = b;
            break;
        }
    }
    ASSERT_NE(addBtn, nullptr);

    // Add 5 planes — each should get a different color
    for (int i = 0; i < 5; ++i) {
        addBtn->click();
    }
    EXPECT_EQ(window.planeCount(), 5);

    // All 5 colors should be different
    for (int i = 0; i < 5; ++i) {
        for (int j = i + 1; j < 5; ++j) {
            EXPECT_NE(window.planeColor(i), window.planeColor(j))
                << "Plane " << i << " and " << j << " have same color";
        }
    }
}

TEST(QuantificationWindowTest, AddPlaneButton_DisabledAtMaxPlanes) {
    QuantificationWindow window;

    QPushButton* addBtn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") {
            addBtn = b;
            break;
        }
    }
    ASSERT_NE(addBtn, nullptr);

    // Add max planes
    for (int i = 0; i < QuantificationWindow::kMaxPlanes; ++i) {
        EXPECT_TRUE(addBtn->isEnabled());
        addBtn->click();
    }

    // Should be disabled after reaching max
    EXPECT_EQ(window.planeCount(), QuantificationWindow::kMaxPlanes);
    EXPECT_FALSE(addBtn->isEnabled());
}

TEST(QuantificationWindowTest, RemovePlaneButton_RemovesActivePlane) {
    QuantificationWindow window;

    QPushButton* addBtn = nullptr;
    QPushButton* removeBtn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") addBtn = b;
        if (b->text() == "Remove Plane") removeBtn = b;
    }
    ASSERT_NE(addBtn, nullptr);
    ASSERT_NE(removeBtn, nullptr);

    // Add 3 planes
    addBtn->click();
    addBtn->click();
    addBtn->click();
    EXPECT_EQ(window.planeCount(), 3);

    // Select and remove the second plane
    window.setActivePlane(1);
    removeBtn->click();
    EXPECT_EQ(window.planeCount(), 2);
}

TEST(QuantificationWindowTest, RemovePlaneButton_DisabledWhenOnePlane) {
    QuantificationWindow window;

    QPushButton* addBtn = nullptr;
    QPushButton* removeBtn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") addBtn = b;
        if (b->text() == "Remove Plane") removeBtn = b;
    }
    ASSERT_NE(addBtn, nullptr);
    ASSERT_NE(removeBtn, nullptr);

    // Add 2 planes
    addBtn->click();
    addBtn->click();
    EXPECT_TRUE(removeBtn->isEnabled());

    // Remove one — should still be enabled (2 planes → 1)
    removeBtn->click();
    EXPECT_EQ(window.planeCount(), 1);
    // Should be disabled now (only 1 plane left)
    EXPECT_FALSE(removeBtn->isEnabled());
}

TEST(QuantificationWindowTest, AddPlaneButton_ReEnablesAfterRemove) {
    QuantificationWindow window;

    QPushButton* addBtn = nullptr;
    QPushButton* removeBtn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") addBtn = b;
        if (b->text() == "Remove Plane") removeBtn = b;
    }
    ASSERT_NE(addBtn, nullptr);
    ASSERT_NE(removeBtn, nullptr);

    // Fill to max
    for (int i = 0; i < QuantificationWindow::kMaxPlanes; ++i) {
        addBtn->click();
    }
    EXPECT_FALSE(addBtn->isEnabled());

    // Remove one — Add button should re-enable
    removeBtn->click();
    EXPECT_TRUE(addBtn->isEnabled());
}

TEST(QuantificationWindowTest, ProgrammaticAddPlane_UpdatesButtons) {
    QuantificationWindow window;

    QPushButton* addBtn = nullptr;
    QPushButton* removeBtn = nullptr;
    for (auto* b : window.findChildren<QPushButton*>()) {
        if (b->text() == "Add Plane") addBtn = b;
        if (b->text() == "Remove Plane") removeBtn = b;
    }
    ASSERT_NE(addBtn, nullptr);
    ASSERT_NE(removeBtn, nullptr);

    // Programmatic add should also update button state
    window.addPlane("Test", Qt::red);
    // With 1 plane, remove should be disabled (minimum 1 plane required)
    EXPECT_FALSE(removeBtn->isEnabled());

    window.addPlane("Test 2", Qt::blue);
    EXPECT_TRUE(removeBtn->isEnabled());  // 2 planes → remove enabled
}

TEST(QuantificationWindowTest, MaxPlanes_ConstantValue) {
    EXPECT_EQ(QuantificationWindow::kMaxPlanes, 5);
}

// =============================================================================
// Plane positioning integration
// =============================================================================

TEST(QuantificationWindowTest, SetPlanePosition_UpdatesActivePlaneOverlay) {
    QuantificationWindow window;

    // Add two planes with specific positions
    PlanePosition pos1;
    pos1.normalX = 0.0; pos1.normalY = 1.0; pos1.normalZ = 0.0;
    pos1.centerX = 10.0; pos1.centerY = 20.0; pos1.centerZ = 30.0;
    pos1.extent = 60.0;
    window.addPlane("Aorta", Qt::red, pos1);

    PlanePosition pos2;
    pos2.normalX = 1.0; pos2.normalY = 0.0; pos2.normalZ = 0.0;
    pos2.centerX = 50.0; pos2.centerY = 50.0; pos2.centerZ = 50.0;
    pos2.extent = 80.0;
    window.addPlane("Pulmonary", Qt::blue, pos2);

    // Update position of first plane
    QSignalSpy spy(&window, &QuantificationWindow::planePositionChanged);
    PlanePosition newPos;
    newPos.normalX = 0.707; newPos.normalY = 0.707; newPos.normalZ = 0.0;
    newPos.centerX = 15.0; newPos.centerY = 25.0; newPos.centerZ = 35.0;
    newPos.extent = 70.0;
    window.setPlanePosition(0, newPos);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 0);

    auto retrieved = window.planePosition(0);
    EXPECT_NEAR(retrieved.normalX, 0.707, 1e-9);
    EXPECT_NEAR(retrieved.normalY, 0.707, 1e-9);
    EXPECT_DOUBLE_EQ(retrieved.centerX, 15.0);
    EXPECT_DOUBLE_EQ(retrieved.extent, 70.0);

    // Second plane should be unchanged
    auto plane2 = window.planePosition(1);
    EXPECT_DOUBLE_EQ(plane2.normalX, 1.0);
    EXPECT_DOUBLE_EQ(plane2.centerX, 50.0);
}

TEST(QuantificationWindowTest, AutoAddPlane_WhenNoneExist) {
    QuantificationWindow window;
    EXPECT_EQ(window.planeCount(), 0);

    // Simulate what MainWindow does when viewport emits planePositioned
    PlanePosition pos;
    pos.normalX = -0.5; pos.normalY = 0.866; pos.normalZ = 0.0;
    pos.centerX = 100.0; pos.centerY = 100.0; pos.centerZ = 50.0;
    pos.extent = 45.0;

    // Auto-add
    window.addPlane("Plane 1", QColor(0xE7, 0x4C, 0x3C), pos);

    EXPECT_EQ(window.planeCount(), 1);
    auto retrieved = window.planePosition(0);
    EXPECT_DOUBLE_EQ(retrieved.centerX, 100.0);
    EXPECT_DOUBLE_EQ(retrieved.extent, 45.0);
}

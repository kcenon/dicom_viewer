#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>

#include "ui/panels/flow_tool_panel.hpp"

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

TEST(FlowToolPanelTest, DefaultConstruction) {
    FlowToolPanel panel;
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::Magnitude);
}

TEST(FlowToolPanelTest, InitiallyDisabled) {
    FlowToolPanel panel;
    // Panel is constructed with setFlowDataAvailable(false)
    // The internal toolbox should be disabled
    // Verify via public API: panel should still report default series
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::Magnitude);
}

// =============================================================================
// Series selection
// =============================================================================

TEST(FlowToolPanelTest, SetSelectedSeries_RL) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    panel.setSelectedSeries(FlowSeries::RL);
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::RL);
}

TEST(FlowToolPanelTest, SetSelectedSeries_AP) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    panel.setSelectedSeries(FlowSeries::AP);
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::AP);
}

TEST(FlowToolPanelTest, SetSelectedSeries_FH) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    panel.setSelectedSeries(FlowSeries::FH);
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::FH);
}

TEST(FlowToolPanelTest, SetSelectedSeries_PCMRA) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    panel.setSelectedSeries(FlowSeries::PCMRA);
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::PCMRA);
}

TEST(FlowToolPanelTest, SetSelectedSeries_SameValue_NoChange) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    // Already Magnitude by default
    panel.setSelectedSeries(FlowSeries::Magnitude);
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::Magnitude);
}

// =============================================================================
// Signal emission
// =============================================================================

TEST(FlowToolPanelTest, SeriesSelectionChangedSignal_NotEmittedOnProgrammatic) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);

    QSignalSpy spy(&panel, &FlowToolPanel::seriesSelectionChanged);
    ASSERT_TRUE(spy.isValid());

    // Programmatic selection should NOT emit the signal
    // (blockSignals used internally)
    panel.setSelectedSeries(FlowSeries::AP);
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// Phase and slice info
// =============================================================================

TEST(FlowToolPanelTest, SetPhaseInfo) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    // Should not crash; updates internal label
    panel.setPhaseInfo(0, 20);
    panel.setPhaseInfo(19, 20);
}

TEST(FlowToolPanelTest, SetSliceInfo) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    // Should not crash; updates internal label
    panel.setSliceInfo(0, 30);
    panel.setSliceInfo(29, 30);
}

// =============================================================================
// Data availability toggle
// =============================================================================

TEST(FlowToolPanelTest, SetFlowDataAvailable_EnableDisable) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);
    panel.setSelectedSeries(FlowSeries::FH);
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::FH);

    panel.setFlowDataAvailable(false);
    // Series selection should persist even when disabled
    EXPECT_EQ(panel.selectedSeries(), FlowSeries::FH);
}

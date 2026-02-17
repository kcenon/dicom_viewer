#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>

#include "services/segmentation/label_manager.hpp"
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

// =============================================================================
// Display 2D checkboxes
// =============================================================================

TEST(FlowToolPanelTest, Display2D_AllDisabledByDefault) {
    FlowToolPanel panel;
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::Mask));
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::Velocity));
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::Streamline));
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::EnergyLoss));
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::Vorticity));
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::VelocityTexture));
}

TEST(FlowToolPanelTest, Display2D_SetEnabled) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);

    panel.setDisplay2DEnabled(Display2DItem::Velocity, true);
    EXPECT_TRUE(panel.isDisplay2DEnabled(Display2DItem::Velocity));
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::Mask));

    panel.setDisplay2DEnabled(Display2DItem::Velocity, false);
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::Velocity));
}

TEST(FlowToolPanelTest, Display2D_MultipleCheckboxes) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);

    panel.setDisplay2DEnabled(Display2DItem::Vorticity, true);
    panel.setDisplay2DEnabled(Display2DItem::EnergyLoss, true);

    EXPECT_TRUE(panel.isDisplay2DEnabled(Display2DItem::Vorticity));
    EXPECT_TRUE(panel.isDisplay2DEnabled(Display2DItem::EnergyLoss));
    EXPECT_FALSE(panel.isDisplay2DEnabled(Display2DItem::Streamline));
}

TEST(FlowToolPanelTest, Display2D_SignalNotEmittedOnProgrammatic) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);

    QSignalSpy spy(&panel, &FlowToolPanel::display2DToggled);
    ASSERT_TRUE(spy.isValid());

    panel.setDisplay2DEnabled(Display2DItem::Velocity, true);
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// Display 3D checkboxes
// =============================================================================

TEST(FlowToolPanelTest, Display3D_AllDisabledByDefault) {
    FlowToolPanel panel;
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::MaskVolume));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::Surface));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::Cine));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::Magnitude));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::Velocity));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::ASC));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::Streamline));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::EnergyLoss));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::WSS));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::OSI));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::AFI));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::RRT));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::Vorticity));
}

TEST(FlowToolPanelTest, Display3D_SetEnabled) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);

    panel.setDisplay3DEnabled(Display3DItem::WSS, true);
    EXPECT_TRUE(panel.isDisplay3DEnabled(Display3DItem::WSS));

    panel.setDisplay3DEnabled(Display3DItem::WSS, false);
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::WSS));
}

TEST(FlowToolPanelTest, Display3D_MultipleSurfaceParams) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);

    panel.setDisplay3DEnabled(Display3DItem::WSS, true);
    panel.setDisplay3DEnabled(Display3DItem::OSI, true);
    panel.setDisplay3DEnabled(Display3DItem::RRT, true);

    EXPECT_TRUE(panel.isDisplay3DEnabled(Display3DItem::WSS));
    EXPECT_TRUE(panel.isDisplay3DEnabled(Display3DItem::OSI));
    EXPECT_TRUE(panel.isDisplay3DEnabled(Display3DItem::RRT));
    EXPECT_FALSE(panel.isDisplay3DEnabled(Display3DItem::AFI));
}

TEST(FlowToolPanelTest, Display3D_SignalNotEmittedOnProgrammatic) {
    FlowToolPanel panel;
    panel.setFlowDataAvailable(true);

    QSignalSpy spy(&panel, &FlowToolPanel::display3DToggled);
    ASSERT_TRUE(spy.isValid());

    panel.setDisplay3DEnabled(Display3DItem::Vorticity, true);
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// Mask section
// =============================================================================

TEST(FlowToolPanelTest, MaskCount_InitiallyZero) {
    FlowToolPanel panel;
    EXPECT_EQ(panel.maskCount(), 0);
}

TEST(FlowToolPanelTest, MaskSection_NoLabelManager) {
    FlowToolPanel panel;
    // Should not crash when no LabelManager is set
    panel.refreshMaskList();
    EXPECT_EQ(panel.maskCount(), 0);
}

TEST(FlowToolPanelTest, MaskSection_SetNullLabelManager) {
    FlowToolPanel panel;
    panel.setLabelManager(nullptr);
    panel.refreshMaskList();
    EXPECT_EQ(panel.maskCount(), 0);
}

TEST(FlowToolPanelTest, MaskSection_WithLabelManager) {
    FlowToolPanel panel;
    dicom_viewer::services::LabelManager manager;
    auto result = manager.initializeLabelMap(16, 16, 16);
    ASSERT_TRUE(result.has_value());

    auto label1 = manager.addLabel("Aorta");
    ASSERT_TRUE(label1.has_value());
    auto label2 = manager.addLabel("Ventricle");
    ASSERT_TRUE(label2.has_value());

    panel.setLabelManager(&manager);
    EXPECT_EQ(panel.maskCount(), 2);
}

TEST(FlowToolPanelTest, MaskSection_RefreshUpdatesCount) {
    FlowToolPanel panel;
    dicom_viewer::services::LabelManager manager;
    auto result = manager.initializeLabelMap(16, 16, 16);
    ASSERT_TRUE(result.has_value());

    panel.setLabelManager(&manager);
    EXPECT_EQ(panel.maskCount(), 0);

    auto label1 = manager.addLabel("Aorta");
    ASSERT_TRUE(label1.has_value());

    // Must manually refresh to sync
    panel.refreshMaskList();
    EXPECT_EQ(panel.maskCount(), 1);
}

TEST(FlowToolPanelTest, MaskSection_LoadSignal) {
    FlowToolPanel panel;
    QSignalSpy spy(&panel, &FlowToolPanel::maskLoadRequested);
    ASSERT_TRUE(spy.isValid());
    // Signal exists and spy is valid (button click requires user interaction)
    EXPECT_EQ(spy.count(), 0);
}

TEST(FlowToolPanelTest, MaskSection_RemoveSignal) {
    FlowToolPanel panel;
    QSignalSpy spy(&panel, &FlowToolPanel::maskRemoveRequested);
    ASSERT_TRUE(spy.isValid());
    EXPECT_EQ(spy.count(), 0);
}

TEST(FlowToolPanelTest, MaskSection_VisibilitySignal) {
    FlowToolPanel panel;
    QSignalSpy spy(&panel, &FlowToolPanel::maskVisibilityToggled);
    ASSERT_TRUE(spy.isValid());
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// 3D Object list section
// =============================================================================

TEST(FlowToolPanelTest, ObjectCount_InitiallyZero) {
    FlowToolPanel panel;
    EXPECT_EQ(panel.objectCount(), 0);
}

TEST(FlowToolPanelTest, AddObject_IncreasesCount) {
    FlowToolPanel panel;
    panel.addObject("Volume");
    EXPECT_EQ(panel.objectCount(), 1);
    panel.addObject("Surface");
    EXPECT_EQ(panel.objectCount(), 2);
}

TEST(FlowToolPanelTest, AddObject_NoDuplicates) {
    FlowToolPanel panel;
    panel.addObject("Volume");
    panel.addObject("Volume");
    EXPECT_EQ(panel.objectCount(), 1);
}

TEST(FlowToolPanelTest, RemoveObject) {
    FlowToolPanel panel;
    panel.addObject("Volume");
    panel.addObject("Surface");
    EXPECT_EQ(panel.objectCount(), 2);

    panel.removeObject("Volume");
    EXPECT_EQ(panel.objectCount(), 1);
}

TEST(FlowToolPanelTest, RemoveObject_NonExistent) {
    FlowToolPanel panel;
    panel.addObject("Volume");
    panel.removeObject("NonExistent");
    EXPECT_EQ(panel.objectCount(), 1);
}

TEST(FlowToolPanelTest, ObjectVisibility_DefaultTrue) {
    FlowToolPanel panel;
    panel.addObject("Volume");
    EXPECT_TRUE(panel.isObjectVisible("Volume"));
}

TEST(FlowToolPanelTest, ObjectVisibility_InitialFalse) {
    FlowToolPanel panel;
    panel.addObject("Volume", false);
    EXPECT_FALSE(panel.isObjectVisible("Volume"));
}

TEST(FlowToolPanelTest, ObjectVisibility_SetProgrammatic) {
    FlowToolPanel panel;
    panel.addObject("Volume");
    EXPECT_TRUE(panel.isObjectVisible("Volume"));

    panel.setObjectVisible("Volume", false);
    EXPECT_FALSE(panel.isObjectVisible("Volume"));

    panel.setObjectVisible("Volume", true);
    EXPECT_TRUE(panel.isObjectVisible("Volume"));
}

TEST(FlowToolPanelTest, ObjectVisibility_NonExistent) {
    FlowToolPanel panel;
    EXPECT_FALSE(panel.isObjectVisible("NonExistent"));
}

TEST(FlowToolPanelTest, ObjectVisibility_SetProgrammatic_NoSignal) {
    FlowToolPanel panel;
    panel.addObject("Volume");

    QSignalSpy spy(&panel, &FlowToolPanel::objectVisibilityToggled);
    ASSERT_TRUE(spy.isValid());

    // Programmatic change should not emit signal (blockSignals)
    panel.setObjectVisible("Volume", false);
    EXPECT_EQ(spy.count(), 0);
}

TEST(FlowToolPanelTest, ObjectVisibilitySignal_Exists) {
    FlowToolPanel panel;
    QSignalSpy spy(&panel, &FlowToolPanel::objectVisibilityToggled);
    ASSERT_TRUE(spy.isValid());
    EXPECT_EQ(spy.count(), 0);
}

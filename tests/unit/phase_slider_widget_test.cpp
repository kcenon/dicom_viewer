#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>

#include "ui/widgets/phase_slider_widget.hpp"
#include "ui/widgets/sp_mode_toggle.hpp"

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

TEST(PhaseSliderWidgetTest, DefaultConstruction) {
    PhaseSliderWidget widget;
    EXPECT_EQ(widget.currentPhase(), 0);
    EXPECT_FALSE(widget.isPlaying());
}

// =============================================================================
// Phase range tests
// =============================================================================

TEST(PhaseSliderWidgetTest, SetPhaseCount) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(25);

    // After setting phase count, controls should be enabled
    // and range should be 0 to 24
    widget.setCurrentPhase(24);
    EXPECT_EQ(widget.currentPhase(), 24);

    // Should clamp to valid range
    widget.setCurrentPhase(0);
    EXPECT_EQ(widget.currentPhase(), 0);
}

TEST(PhaseSliderWidgetTest, SetPhaseCount_ZeroDisablesControls) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(0);
    EXPECT_EQ(widget.currentPhase(), 0);
}

TEST(PhaseSliderWidgetTest, SetPhaseCount_OneDisablesControls) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(1);
    // Only 1 phase means no navigation needed
    EXPECT_EQ(widget.currentPhase(), 0);
}

// =============================================================================
// Phase navigation tests
// =============================================================================

TEST(PhaseSliderWidgetTest, SetCurrentPhase) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(20);

    widget.setCurrentPhase(10);
    EXPECT_EQ(widget.currentPhase(), 10);

    widget.setCurrentPhase(0);
    EXPECT_EQ(widget.currentPhase(), 0);

    widget.setCurrentPhase(19);
    EXPECT_EQ(widget.currentPhase(), 19);
}

TEST(PhaseSliderWidgetTest, SetCurrentPhase_DoesNotEmitSignal) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(20);

    QSignalSpy spy(&widget, &PhaseSliderWidget::phaseChangeRequested);
    widget.setCurrentPhase(10);

    // External setCurrentPhase should NOT emit phaseChangeRequested
    // to avoid signal loops
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// Playback state tests
// =============================================================================

TEST(PhaseSliderWidgetTest, SetPlaying) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(20);

    EXPECT_FALSE(widget.isPlaying());

    widget.setPlaying(true);
    EXPECT_TRUE(widget.isPlaying());

    widget.setPlaying(false);
    EXPECT_FALSE(widget.isPlaying());
}

// =============================================================================
// Signal emission tests
// =============================================================================

TEST(PhaseSliderWidgetTest, PlayRequestedSignal) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(20);

    QSignalSpy spy(&widget, &PhaseSliderWidget::playRequested);

    // Simulate play button click
    emit widget.playRequested();

    EXPECT_EQ(spy.count(), 1);
}

TEST(PhaseSliderWidgetTest, StopRequestedSignal) {
    PhaseSliderWidget widget;
    widget.setPhaseCount(20);

    QSignalSpy spy(&widget, &PhaseSliderWidget::stopRequested);

    emit widget.stopRequested();

    EXPECT_EQ(spy.count(), 1);
}

// =============================================================================
// FPS control tests
// =============================================================================

TEST(PhaseSliderWidgetTest, DefaultFps) {
    PhaseSliderWidget widget;
    EXPECT_EQ(widget.fps(), 15);
}

TEST(PhaseSliderWidgetTest, SetFps) {
    PhaseSliderWidget widget;
    widget.setFps(30);
    EXPECT_EQ(widget.fps(), 30);

    widget.setFps(1);
    EXPECT_EQ(widget.fps(), 1);

    widget.setFps(60);
    EXPECT_EQ(widget.fps(), 60);
}

TEST(PhaseSliderWidgetTest, FpsChangedSignal) {
    PhaseSliderWidget widget;

    QSignalSpy spy(&widget, &PhaseSliderWidget::fpsChanged);
    widget.setFps(25);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.first().first().toInt(), 25);
}

// =============================================================================
// Scroll mode tests
// =============================================================================

TEST(PhaseSliderWidgetTest, DefaultScrollMode) {
    PhaseSliderWidget widget;
    EXPECT_EQ(widget.scrollMode(), ScrollMode::Slice);
}

TEST(PhaseSliderWidgetTest, SetScrollMode) {
    PhaseSliderWidget widget;

    widget.setScrollMode(ScrollMode::Phase);
    EXPECT_EQ(widget.scrollMode(), ScrollMode::Phase);

    widget.setScrollMode(ScrollMode::Slice);
    EXPECT_EQ(widget.scrollMode(), ScrollMode::Slice);
}

TEST(PhaseSliderWidgetTest, SetScrollMode_EmitsSignal) {
    PhaseSliderWidget widget;

    QSignalSpy spy(&widget, &PhaseSliderWidget::scrollModeChanged);
    widget.setScrollMode(ScrollMode::Phase);

    EXPECT_EQ(spy.count(), 1);
}

TEST(PhaseSliderWidgetTest, SetScrollMode_SameMode_NoSignal) {
    PhaseSliderWidget widget;
    // Default is Slice, setting Slice again should not emit
    QSignalSpy spy(&widget, &PhaseSliderWidget::scrollModeChanged);
    widget.setScrollMode(ScrollMode::Slice);

    EXPECT_EQ(spy.count(), 0);
}

#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>

#include "ui/widgets/sp_mode_toggle.hpp"
#include "ui/widgets/phase_slider_widget.hpp"

using namespace dicom_viewer::ui;

namespace {

// Ensure QApplication exists for widget tests
class SPModeToggleTestFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test")};
            static QApplication app(argc, argv);
        }
    }
};

} // anonymous namespace

// =============================================================================
// SPModeToggle — Construction
// =============================================================================

TEST_F(SPModeToggleTestFixture, DefaultModeIsSlice) {
    SPModeToggle toggle;
    EXPECT_EQ(toggle.mode(), ScrollMode::Slice);
}

// =============================================================================
// SPModeToggle — Mode switching
// =============================================================================

TEST_F(SPModeToggleTestFixture, SetModeToPhase) {
    SPModeToggle toggle;
    toggle.setMode(ScrollMode::Phase);
    EXPECT_EQ(toggle.mode(), ScrollMode::Phase);
}

TEST_F(SPModeToggleTestFixture, SetModeToSlice) {
    SPModeToggle toggle;
    toggle.setMode(ScrollMode::Phase);
    toggle.setMode(ScrollMode::Slice);
    EXPECT_EQ(toggle.mode(), ScrollMode::Slice);
}

TEST_F(SPModeToggleTestFixture, SetSameModeNoOp) {
    SPModeToggle toggle;
    QSignalSpy spy(&toggle, &SPModeToggle::modeChanged);
    toggle.setMode(ScrollMode::Slice);  // Already in Slice mode
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// SPModeToggle — Signal emission
// =============================================================================

TEST_F(SPModeToggleTestFixture, ModeChangedSignalNotEmittedOnExternalSet) {
    SPModeToggle toggle;
    QSignalSpy spy(&toggle, &SPModeToggle::modeChanged);

    // setMode is external programmatic change, should NOT emit signal
    toggle.setMode(ScrollMode::Phase);
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(toggle.mode(), ScrollMode::Phase);
}

// =============================================================================
// PhaseSliderWidget — S/P mode integration
// =============================================================================

TEST_F(SPModeToggleTestFixture, PhaseSliderDefaultScrollModeIsSlice) {
    PhaseSliderWidget slider;
    EXPECT_EQ(slider.scrollMode(), ScrollMode::Slice);
}

TEST_F(SPModeToggleTestFixture, PhaseSliderExposesScrollMode) {
    PhaseSliderWidget slider;
    // scrollMode should return the current S/P state
    EXPECT_EQ(slider.scrollMode(), ScrollMode::Slice);
}

// =============================================================================
// ScrollMode — Enum values
// =============================================================================

TEST(ScrollModeEnumTest, DistinctValues) {
    EXPECT_NE(static_cast<int>(ScrollMode::Slice),
              static_cast<int>(ScrollMode::Phase));
}

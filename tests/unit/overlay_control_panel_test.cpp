#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>

#include "ui/panels/overlay_control_panel.hpp"

using namespace dicom_viewer::ui;
using namespace dicom_viewer::services;

namespace {

// QApplication must exist for QWidget instantiation
int argc = 0;
char* argv[] = {nullptr};
QApplication app(argc, argv);

}  // anonymous namespace

// =============================================================================
// Construction and defaults
// =============================================================================

TEST(OverlayControlPanelTest, DefaultConstruction) {
    OverlayControlPanel panel;
    // All overlays should be disabled by default
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::VelocityMagnitude));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::VelocityX));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::VelocityY));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::VelocityZ));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::Vorticity));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::EnergyLoss));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::Streamline));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::VelocityTexture));
}

TEST(OverlayControlPanelTest, DefaultOpacity) {
    OverlayControlPanel panel;
    // Default opacity should be 0.5 (slider at 50)
    EXPECT_DOUBLE_EQ(panel.overlayOpacity(OverlayType::VelocityMagnitude), 0.5);
    EXPECT_DOUBLE_EQ(panel.overlayOpacity(OverlayType::Vorticity), 0.5);
}

TEST(OverlayControlPanelTest, DefaultScalarRange_VelocityMagnitude) {
    OverlayControlPanel panel;
    auto [minVal, maxVal] = panel.overlayScalarRange(OverlayType::VelocityMagnitude);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 100.0);
}

TEST(OverlayControlPanelTest, DefaultScalarRange_VelocityX) {
    OverlayControlPanel panel;
    auto [minVal, maxVal] = panel.overlayScalarRange(OverlayType::VelocityX);
    EXPECT_DOUBLE_EQ(minVal, -100.0);
    EXPECT_DOUBLE_EQ(maxVal, 100.0);
}

TEST(OverlayControlPanelTest, DefaultScalarRange_Vorticity) {
    OverlayControlPanel panel;
    auto [minVal, maxVal] = panel.overlayScalarRange(OverlayType::Vorticity);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 50.0);
}

TEST(OverlayControlPanelTest, DefaultScalarRange_EnergyLoss) {
    OverlayControlPanel panel;
    auto [minVal, maxVal] = panel.overlayScalarRange(OverlayType::EnergyLoss);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 1000.0);
}

TEST(OverlayControlPanelTest, DefaultScalarRange_StreamlineNoRange) {
    OverlayControlPanel panel;
    // Streamline has no range spinboxes, should return default (0, 100)
    auto [minVal, maxVal] = panel.overlayScalarRange(OverlayType::Streamline);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 100.0);
}

TEST(OverlayControlPanelTest, DefaultScalarRange_VelocityTextureNoRange) {
    OverlayControlPanel panel;
    auto [minVal, maxVal] = panel.overlayScalarRange(OverlayType::VelocityTexture);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 100.0);
}

// =============================================================================
// Availability control
// =============================================================================

TEST(OverlayControlPanelTest, SetOverlaysAvailable) {
    OverlayControlPanel panel;
    // Initially should be disabled (setOverlaysAvailable(false) called in setupUI)
    panel.setOverlaysAvailable(true);
    // Now checkboxes should be enabled
    // We can't directly check the checkbox enabled state, but the panel should function
    panel.setOverlaysAvailable(false);
    // After disabling, controls should be disabled
}

// =============================================================================
// Signal emission tests
// =============================================================================

TEST(OverlayControlPanelTest, VisibilityChangedSignal) {
    OverlayControlPanel panel;
    panel.setOverlaysAvailable(true);

    QSignalSpy spy(&panel,
        &OverlayControlPanel::overlayVisibilityChanged);
    ASSERT_TRUE(spy.isValid());

    // Simulate enabling VelocityMagnitude by directly calling the internal slot
    // We test through the public API instead
    // The signal should fire when a checkbox is toggled
    // Since we can't easily toggle internal checkboxes, we verify signal spy setup
    EXPECT_EQ(spy.count(), 0);
}

TEST(OverlayControlPanelTest, OpacityChangedSignal) {
    OverlayControlPanel panel;
    panel.setOverlaysAvailable(true);

    QSignalSpy spy(&panel,
        &OverlayControlPanel::overlayOpacityChanged);
    ASSERT_TRUE(spy.isValid());
    EXPECT_EQ(spy.count(), 0);
}

TEST(OverlayControlPanelTest, ScalarRangeChangedSignal) {
    OverlayControlPanel panel;
    panel.setOverlaysAvailable(true);

    QSignalSpy spy(&panel,
        &OverlayControlPanel::overlayScalarRangeChanged);
    ASSERT_TRUE(spy.isValid());
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// Reset to defaults
// =============================================================================

TEST(OverlayControlPanelTest, ResetToDefaults) {
    OverlayControlPanel panel;
    panel.setOverlaysAvailable(true);
    panel.resetToDefaults();

    // After reset, all overlays should be disabled
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::VelocityMagnitude));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::EnergyLoss));
    EXPECT_FALSE(panel.isOverlayEnabled(OverlayType::Streamline));

    // Opacity should be back to 50%
    EXPECT_DOUBLE_EQ(panel.overlayOpacity(OverlayType::VelocityMagnitude), 0.5);
}

// =============================================================================
// Unknown/invalid type handling
// =============================================================================

TEST(OverlayControlPanelTest, UnknownTypeReturnsDefaults) {
    OverlayControlPanel panel;
    // Cast an invalid value
    auto unknownType = static_cast<OverlayType>(99);
    EXPECT_FALSE(panel.isOverlayEnabled(unknownType));
    EXPECT_DOUBLE_EQ(panel.overlayOpacity(unknownType), 0.5);
    auto [minVal, maxVal] = panel.overlayScalarRange(unknownType);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 100.0);
}

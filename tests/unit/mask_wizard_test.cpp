#include <gtest/gtest.h>

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QWizardPage>

#include "ui/dialogs/mask_wizard.hpp"

using namespace dicom_viewer::ui;

namespace {

// QApplication must exist for QWidget instantiation
int argc = 0;
char* argv[] = {nullptr};
QApplication app(argc, argv);

}  // anonymous namespace

// =============================================================================
// Wizard page structure
// =============================================================================

TEST(MaskWizardTest, HasFourPages) {
    MaskWizard wizard;
    EXPECT_EQ(wizard.pageIds().size(), 4);
}

TEST(MaskWizardTest, InitialStepIsCrop) {
    MaskWizard wizard;
    wizard.restart();  // QWizard requires restart() to initialize currentId()
    EXPECT_EQ(wizard.currentStep(), MaskWizardStep::Crop);
}

TEST(MaskWizardTest, PageTitlesAreSet) {
    MaskWizard wizard;
    auto ids = wizard.pageIds();
    ASSERT_EQ(ids.size(), 4);

    EXPECT_FALSE(wizard.page(ids[0])->title().isEmpty());
    EXPECT_FALSE(wizard.page(ids[1])->title().isEmpty());
    EXPECT_FALSE(wizard.page(ids[2])->title().isEmpty());
    EXPECT_FALSE(wizard.page(ids[3])->title().isEmpty());
}

// =============================================================================
// Navigation
// =============================================================================

TEST(MaskWizardTest, NextAdvancesToThreshold) {
    MaskWizard wizard;
    wizard.restart();
    wizard.next();
    EXPECT_EQ(wizard.currentStep(), MaskWizardStep::Threshold);
}

TEST(MaskWizardTest, FullForwardNavigation) {
    MaskWizard wizard;
    wizard.restart();

    wizard.next();
    EXPECT_EQ(wizard.currentStep(), MaskWizardStep::Threshold);

    wizard.next();
    EXPECT_EQ(wizard.currentStep(), MaskWizardStep::Separate);

    wizard.next();
    EXPECT_EQ(wizard.currentStep(), MaskWizardStep::Track);
}

TEST(MaskWizardTest, BackReturnsToSeparate) {
    MaskWizard wizard;
    wizard.restart();
    wizard.next();  // Threshold
    wizard.next();  // Separate
    wizard.next();  // Track
    wizard.back();
    EXPECT_EQ(wizard.currentStep(), MaskWizardStep::Separate);
}

// =============================================================================
// Appearance
// =============================================================================

TEST(MaskWizardTest, WindowTitleIsSet) {
    MaskWizard wizard;
    EXPECT_FALSE(wizard.windowTitle().isEmpty());
}

TEST(MaskWizardTest, MinimumSizeIsReasonable) {
    MaskWizard wizard;
    EXPECT_GE(wizard.minimumWidth(), 500);
    EXPECT_GE(wizard.minimumHeight(), 400);
}

// =============================================================================
// Signal
// =============================================================================

TEST(MaskWizardTest, WizardCompletedSignalExists) {
    MaskWizard wizard;
    QSignalSpy spy(&wizard, &MaskWizard::wizardCompleted);
    EXPECT_TRUE(spy.isValid());
}

// =============================================================================
// Threshold page — default values
// =============================================================================

TEST(MaskWizardTest, ThresholdDefaultRange) {
    MaskWizard wizard;
    // Default CT HU range: -1024 to 3071
    EXPECT_EQ(wizard.thresholdMin(), -1024);
    EXPECT_EQ(wizard.thresholdMax(), 3071);
}

// =============================================================================
// Threshold page — API
// =============================================================================

TEST(MaskWizardTest, SetThresholdRange) {
    MaskWizard wizard;
    wizard.setThresholdRange(0, 1000);
    // After range change, values should be clamped
    EXPECT_GE(wizard.thresholdMin(), 0);
    EXPECT_LE(wizard.thresholdMax(), 1000);
}

TEST(MaskWizardTest, SetOtsuThreshold) {
    MaskWizard wizard;
    wizard.setOtsuThreshold(245.7);
    // Otsu sets min to rounded value
    EXPECT_EQ(wizard.thresholdMin(), 246);
    // Max remains at range max
    EXPECT_EQ(wizard.thresholdMax(), 3071);
}

// =============================================================================
// Threshold page — signals
// =============================================================================

TEST(MaskWizardTest, ThresholdChangedSignal) {
    MaskWizard wizard;
    QSignalSpy spy(&wizard, &MaskWizard::thresholdChanged);
    EXPECT_TRUE(spy.isValid());

    // Navigate to threshold page and change slider
    wizard.restart();
    wizard.next();  // Now on Threshold page

    // Find the min spinbox on the threshold page and change it
    auto* thresholdPage = wizard.page(1);
    auto* minSpin = thresholdPage->findChild<QSpinBox*>();
    ASSERT_NE(minSpin, nullptr);
    minSpin->setValue(100);

    EXPECT_GE(spy.count(), 1);
    auto lastArgs = spy.last();
    EXPECT_EQ(lastArgs.at(0).toInt(), 100);
}

TEST(MaskWizardTest, OtsuRequestedSignal) {
    MaskWizard wizard;
    QSignalSpy spy(&wizard, &MaskWizard::otsuRequested);
    EXPECT_TRUE(spy.isValid());

    // Find the Otsu button and click it
    auto* thresholdPage = wizard.page(1);
    auto* otsuButton = thresholdPage->findChild<QPushButton*>();
    ASSERT_NE(otsuButton, nullptr);
    otsuButton->click();

    EXPECT_EQ(spy.count(), 1);
}

// =============================================================================
// Threshold page — constraint enforcement
// =============================================================================

TEST(MaskWizardTest, MinCannotExceedMax) {
    MaskWizard wizard;
    wizard.restart();
    wizard.next();  // Threshold page

    auto* thresholdPage = wizard.page(1);
    auto spinBoxes = thresholdPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 2);

    // First spinbox is min, second is max
    auto* minSpin = spinBoxes[0];
    auto* maxSpin = spinBoxes[1];

    // Set max to 500 first, then try to set min to 600
    maxSpin->setValue(500);
    minSpin->setValue(600);

    // min <= max must hold
    EXPECT_LE(wizard.thresholdMin(), wizard.thresholdMax());
}

TEST(MaskWizardTest, MaxCannotGoBelowMin) {
    MaskWizard wizard;
    wizard.restart();
    wizard.next();  // Threshold page

    auto* thresholdPage = wizard.page(1);
    auto spinBoxes = thresholdPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 2);

    auto* minSpin = spinBoxes[0];
    auto* maxSpin = spinBoxes[1];

    // Set min to 800 first, then try to set max to 200
    minSpin->setValue(800);
    maxSpin->setValue(200);

    // min <= max must hold
    EXPECT_LE(wizard.thresholdMin(), wizard.thresholdMax());
}

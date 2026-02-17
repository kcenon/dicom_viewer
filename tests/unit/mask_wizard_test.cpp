#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
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

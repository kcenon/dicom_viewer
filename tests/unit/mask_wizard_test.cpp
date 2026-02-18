#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
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

// =============================================================================
// Separate page — helpers
// =============================================================================

namespace {

std::vector<ComponentInfo> makeSampleComponents() {
    return {
        {1, 12345, QColor(Qt::red), true},
        {2, 8901, QColor(Qt::green), true},
        {3, 234, QColor(Qt::blue), false},
    };
}

}  // anonymous namespace

// =============================================================================
// Separate page — initial state
// =============================================================================

TEST(MaskWizardTest, SeparateInitiallyEmpty) {
    MaskWizard wizard;
    EXPECT_EQ(wizard.componentCount(), 0);
    EXPECT_TRUE(wizard.selectedComponentIndices().empty());
}

// =============================================================================
// Separate page — setComponents
// =============================================================================

TEST(MaskWizardTest, SetComponentsPopulatesTable) {
    MaskWizard wizard;
    wizard.setComponents(makeSampleComponents());
    EXPECT_EQ(wizard.componentCount(), 3);
}

TEST(MaskWizardTest, SelectedIndicesReflectsInput) {
    MaskWizard wizard;
    wizard.setComponents(makeSampleComponents());

    // Components 0 and 1 are selected, 2 is not
    auto selected = wizard.selectedComponentIndices();
    EXPECT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected[0], 0);
    EXPECT_EQ(selected[1], 1);
}

// =============================================================================
// Separate page — bulk selection buttons
// =============================================================================

TEST(MaskWizardTest, SelectAllButton) {
    MaskWizard wizard;
    wizard.setComponents(makeSampleComponents());

    auto* separatePage = wizard.page(2);
    auto buttons = separatePage->findChildren<QPushButton*>();
    // Find "Select All" button
    QPushButton* selectAllBtn = nullptr;
    for (auto* btn : buttons) {
        if (btn->text().contains("Select All")) {
            selectAllBtn = btn;
            break;
        }
    }
    ASSERT_NE(selectAllBtn, nullptr);
    selectAllBtn->click();

    EXPECT_EQ(wizard.selectedComponentIndices().size(), 3u);
}

TEST(MaskWizardTest, DeselectAllButton) {
    MaskWizard wizard;
    wizard.setComponents(makeSampleComponents());

    auto* separatePage = wizard.page(2);
    auto buttons = separatePage->findChildren<QPushButton*>();
    QPushButton* deselectAllBtn = nullptr;
    for (auto* btn : buttons) {
        if (btn->text().contains("Deselect All")) {
            deselectAllBtn = btn;
            break;
        }
    }
    ASSERT_NE(deselectAllBtn, nullptr);
    deselectAllBtn->click();

    EXPECT_TRUE(wizard.selectedComponentIndices().empty());
}

TEST(MaskWizardTest, InvertSelectionButton) {
    MaskWizard wizard;
    wizard.setComponents(makeSampleComponents());

    // Initially: 0=selected, 1=selected, 2=deselected
    auto* separatePage = wizard.page(2);
    auto buttons = separatePage->findChildren<QPushButton*>();
    QPushButton* invertBtn = nullptr;
    for (auto* btn : buttons) {
        if (btn->text().contains("Invert")) {
            invertBtn = btn;
            break;
        }
    }
    ASSERT_NE(invertBtn, nullptr);
    invertBtn->click();

    // After invert: 0=deselected, 1=deselected, 2=selected
    auto selected = wizard.selectedComponentIndices();
    EXPECT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], 2);
}

// =============================================================================
// Separate page — signal
// =============================================================================

TEST(MaskWizardTest, ComponentSelectionChangedSignal) {
    MaskWizard wizard;
    wizard.setComponents(makeSampleComponents());
    QSignalSpy spy(&wizard, &MaskWizard::componentSelectionChanged);
    EXPECT_TRUE(spy.isValid());

    // Click Select All to trigger signal
    auto* separatePage = wizard.page(2);
    auto buttons = separatePage->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Select All")) {
            btn->click();
            break;
        }
    }

    EXPECT_GE(spy.count(), 1);
}

// =============================================================================
// Separate page — table checkbox toggle
// =============================================================================

TEST(MaskWizardTest, CheckboxToggleUpdatesSelection) {
    MaskWizard wizard;
    wizard.setComponents(makeSampleComponents());

    // Find the table
    auto* separatePage = wizard.page(2);
    auto* table = separatePage->findChild<QTableWidget*>();
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->rowCount(), 3);

    // Uncheck row 0 (was selected)
    auto* item = table->item(0, 0);
    ASSERT_NE(item, nullptr);
    item->setCheckState(Qt::Unchecked);

    // Now only index 1 should be selected
    auto selected = wizard.selectedComponentIndices();
    EXPECT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], 1);
}

// =============================================================================
// Crop page — default values
// =============================================================================

TEST(MaskWizardTest, CropDefaultRegion) {
    MaskWizard wizard;
    // Default dimensions: 256x256x128
    auto r = wizard.cropRegion();
    EXPECT_EQ(r.xMin, 0);
    EXPECT_EQ(r.xMax, 255);
    EXPECT_EQ(r.yMin, 0);
    EXPECT_EQ(r.yMax, 255);
    EXPECT_EQ(r.zMin, 0);
    EXPECT_EQ(r.zMax, 127);
}

// =============================================================================
// Crop page — setVolumeDimensions
// =============================================================================

TEST(MaskWizardTest, SetVolumeDimensions) {
    MaskWizard wizard;
    wizard.setVolumeDimensions(512, 512, 64);
    auto r = wizard.cropRegion();
    EXPECT_EQ(r.xMin, 0);
    EXPECT_EQ(r.xMax, 511);
    EXPECT_EQ(r.yMin, 0);
    EXPECT_EQ(r.yMax, 511);
    EXPECT_EQ(r.zMin, 0);
    EXPECT_EQ(r.zMax, 63);
}

// =============================================================================
// Crop page — spinbox interaction
// =============================================================================

TEST(MaskWizardTest, CropSpinboxModifiesRegion) {
    MaskWizard wizard;
    wizard.restart();  // Initialize to crop page

    auto* cropPage = wizard.page(0);
    auto spinBoxes = cropPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 6);

    // Modify X min spinbox
    spinBoxes[0]->setValue(10);
    auto r = wizard.cropRegion();
    EXPECT_EQ(r.xMin, 10);
}

// =============================================================================
// Crop page — constraint enforcement
// =============================================================================

TEST(MaskWizardTest, CropMinCannotExceedMax) {
    MaskWizard wizard;
    wizard.restart();

    auto* cropPage = wizard.page(0);
    auto spinBoxes = cropPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 6);

    // spinBoxes[0] = X min, spinBoxes[1] = X max
    spinBoxes[1]->setValue(50);   // Set X max to 50
    spinBoxes[0]->setValue(100);  // Try to set X min to 100

    auto r = wizard.cropRegion();
    EXPECT_LE(r.xMin, r.xMax);
}

// =============================================================================
// Crop page — reset button
// =============================================================================

TEST(MaskWizardTest, ResetToFullVolume) {
    MaskWizard wizard;
    wizard.setVolumeDimensions(100, 200, 50);

    // Modify region via spinbox
    auto* cropPage = wizard.page(0);
    auto spinBoxes = cropPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 6);
    spinBoxes[0]->setValue(10);  // Change X min

    // Find and click reset button
    auto buttons = cropPage->findChildren<QPushButton*>();
    QPushButton* resetBtn = nullptr;
    for (auto* btn : buttons) {
        if (btn->text().contains("Reset")) {
            resetBtn = btn;
            break;
        }
    }
    ASSERT_NE(resetBtn, nullptr);
    resetBtn->click();

    // Region should be reset to full volume
    auto r = wizard.cropRegion();
    EXPECT_EQ(r.xMin, 0);
    EXPECT_EQ(r.xMax, 99);
    EXPECT_EQ(r.yMin, 0);
    EXPECT_EQ(r.yMax, 199);
    EXPECT_EQ(r.zMin, 0);
    EXPECT_EQ(r.zMax, 49);
}

// =============================================================================
// Crop page — signal
// =============================================================================

TEST(MaskWizardTest, CropRegionChangedSignal) {
    MaskWizard wizard;
    QSignalSpy spy(&wizard, &MaskWizard::cropRegionChanged);
    EXPECT_TRUE(spy.isValid());

    wizard.restart();
    auto* cropPage = wizard.page(0);
    auto spinBoxes = cropPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 6);

    spinBoxes[0]->setValue(5);
    EXPECT_GE(spy.count(), 1);
}

// =============================================================================
// Track page — default values
// =============================================================================

TEST(MaskWizardTest, TrackDefaultPhaseCount) {
    MaskWizard wizard;
    EXPECT_EQ(wizard.phaseCount(), 1);
}

// =============================================================================
// Track page — API
// =============================================================================

TEST(MaskWizardTest, SetPhaseCount) {
    MaskWizard wizard;
    wizard.setPhaseCount(25);
    EXPECT_EQ(wizard.phaseCount(), 25);
}

TEST(MaskWizardTest, SetTrackProgress) {
    MaskWizard wizard;
    wizard.restart();
    wizard.next();  // Threshold
    wizard.next();  // Separate
    wizard.next();  // Track

    auto* trackPage = wizard.page(3);
    auto* progressBar = trackPage->findChild<QProgressBar*>();
    ASSERT_NE(progressBar, nullptr);

    wizard.setTrackProgress(50);
    EXPECT_EQ(progressBar->value(), 50);
}

TEST(MaskWizardTest, SetTrackStatus) {
    MaskWizard wizard;

    auto* trackPage = wizard.page(3);
    auto labels = trackPage->findChildren<QLabel*>();

    wizard.setTrackStatus("Processing phase 5/25");

    // Verify at least one label contains the status text
    bool found = false;
    for (auto* label : labels) {
        if (label->text().contains("Processing phase 5/25")) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(MaskWizardTest, PhaseCountLabel) {
    MaskWizard wizard;
    wizard.setPhaseCount(30);

    auto* trackPage = wizard.page(3);
    auto labels = trackPage->findChildren<QLabel*>();

    bool found = false;
    for (auto* label : labels) {
        if (label->text().contains("30")) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// =============================================================================
// Track page — signal
// =============================================================================

TEST(MaskWizardTest, PropagationRequestedSignal) {
    MaskWizard wizard;
    QSignalSpy spy(&wizard, &MaskWizard::propagationRequested);
    EXPECT_TRUE(spy.isValid());

    // Find the Run Propagation button on track page
    auto* trackPage = wizard.page(3);
    auto buttons = trackPage->findChildren<QPushButton*>();
    QPushButton* runBtn = nullptr;
    for (auto* btn : buttons) {
        if (btn->text().contains("Run Propagation")) {
            runBtn = btn;
            break;
        }
    }
    ASSERT_NE(runBtn, nullptr);
    runBtn->click();

    EXPECT_EQ(spy.count(), 1);
}

// =============================================================================
// Track page — progress bar range
// =============================================================================

TEST(MaskWizardTest, ProgressBarFullRange) {
    MaskWizard wizard;

    wizard.setTrackProgress(0);
    auto* trackPage = wizard.page(3);
    auto* progressBar = trackPage->findChild<QProgressBar*>();
    ASSERT_NE(progressBar, nullptr);
    EXPECT_EQ(progressBar->value(), 0);

    wizard.setTrackProgress(100);
    EXPECT_EQ(progressBar->value(), 100);
}

// =============================================================================
// Crop page — confirmation dialog logic
// =============================================================================

TEST(MaskWizardTest, CropFullVolume_DefaultIsTrue) {
    MaskWizard wizard;
    // Default crop region equals full volume
    EXPECT_TRUE(wizard.isCropFullVolume());
}

TEST(MaskWizardTest, CropFullVolume_AfterModification) {
    MaskWizard wizard;
    wizard.restart();

    auto* cropPage = wizard.page(0);
    auto spinBoxes = cropPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 6);

    // Modify X min → no longer full volume
    spinBoxes[0]->setValue(10);
    EXPECT_FALSE(wizard.isCropFullVolume());
}

TEST(MaskWizardTest, CropFullVolume_AfterReset) {
    MaskWizard wizard;
    wizard.restart();

    auto* cropPage = wizard.page(0);
    auto spinBoxes = cropPage->findChildren<QSpinBox*>();
    ASSERT_GE(spinBoxes.size(), 6);

    // Modify then reset
    spinBoxes[0]->setValue(10);
    EXPECT_FALSE(wizard.isCropFullVolume());

    // Click Reset button
    auto buttons = cropPage->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Reset")) {
            btn->click();
            break;
        }
    }
    EXPECT_TRUE(wizard.isCropFullVolume());
}

TEST(MaskWizardTest, CropFullVolume_AfterDimensionChange) {
    MaskWizard wizard;
    wizard.setVolumeDimensions(100, 200, 50);
    // After dimension change, crop resets to full new volume
    EXPECT_TRUE(wizard.isCropFullVolume());
}

TEST(MaskWizardTest, CropNextSucceeds_WhenFullVolume) {
    MaskWizard wizard;
    wizard.restart();
    // Full volume → validatePage skips dialog → next succeeds
    wizard.next();
    EXPECT_EQ(wizard.currentStep(), MaskWizardStep::Threshold);
}

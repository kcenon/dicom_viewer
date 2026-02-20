#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

#include "ui/dialogs/settings_dialog.hpp"
#include "core/app_log_level.hpp"

using namespace dicom_viewer;
using namespace dicom_viewer::ui;

namespace {

// QApplication must exist for QWidget instantiation
int argc = 0;
char* argv[] = {nullptr};
QApplication app(argc, argv);

} // anonymous namespace

// =============================================================================
// Construction and defaults
// =============================================================================

TEST(SettingsDialogTest, DefaultConstruction) {
    SettingsDialog dialog;
    EXPECT_NE(dialog.windowTitle(), QString());
}

TEST(SettingsDialogTest, ComboBoxHasFourLevels) {
    SettingsDialog dialog;
    auto* combo = dialog.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);
    EXPECT_EQ(combo->count(), 4);
    EXPECT_EQ(combo->itemText(0), "Exception");
    EXPECT_EQ(combo->itemText(1), "Error");
    EXPECT_EQ(combo->itemText(2), "Information");
    EXPECT_EQ(combo->itemText(3), "Debug");
}

TEST(SettingsDialogTest, HasDialogButtons) {
    SettingsDialog dialog;
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    ASSERT_NE(buttons, nullptr);
    EXPECT_NE(buttons->button(QDialogButtonBox::Ok), nullptr);
    EXPECT_NE(buttons->button(QDialogButtonBox::Cancel), nullptr);
}

// =============================================================================
// QSettings persistence
// =============================================================================

TEST(SettingsDialogTest, LoadsFromQSettings) {
    // Set a known value
    QSettings settings;
    settings.setValue("logging/level", to_settings_value(AppLogLevel::Debug));
    settings.sync();

    SettingsDialog dialog;
    auto* combo = dialog.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);
    EXPECT_EQ(combo->currentIndex(), static_cast<int>(AppLogLevel::Debug));

    // Clean up
    settings.remove("logging/level");
}

TEST(SettingsDialogTest, DefaultLevelIsInformation) {
    QSettings settings;
    settings.remove("logging/level");
    settings.sync();

    SettingsDialog dialog;
    auto* combo = dialog.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);
    EXPECT_EQ(combo->currentIndex(), static_cast<int>(AppLogLevel::Information));
}

TEST(SettingsDialogTest, AcceptSavesToQSettings) {
    QSettings settings;
    settings.remove("logging/level");
    settings.sync();

    SettingsDialog dialog;
    auto* combo = dialog.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);

    combo->setCurrentIndex(static_cast<int>(AppLogLevel::Error));
    dialog.accept();

    settings.sync();
    int saved = settings.value("logging/level", -1).toInt();
    EXPECT_EQ(saved, to_settings_value(AppLogLevel::Error));

    // Clean up
    settings.remove("logging/level");
}

// =============================================================================
// Description label updates
// =============================================================================

TEST(SettingsDialogTest, DescriptionUpdatesOnSelection) {
    SettingsDialog dialog;
    auto* combo = dialog.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);

    // Find the description label (the italic gray one)
    auto labels = dialog.findChildren<QLabel*>();
    QLabel* descLabel = nullptr;
    for (auto* label : labels) {
        if (label->styleSheet().contains("italic")) {
            descLabel = label;
            break;
        }
    }
    ASSERT_NE(descLabel, nullptr);

    combo->setCurrentIndex(0); // Exception
    EXPECT_TRUE(descLabel->text().contains("Unintended"));

    combo->setCurrentIndex(3); // Debug
    EXPECT_TRUE(descLabel->text().contains("detailed traces"));
}

// =============================================================================
// Cancel discards changes
// =============================================================================

TEST(SettingsDialogTest, CancelDoesNotSave) {
    QSettings settings;
    settings.setValue("logging/level", to_settings_value(AppLogLevel::Information));
    settings.sync();

    SettingsDialog dialog;
    auto* combo = dialog.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);

    combo->setCurrentIndex(static_cast<int>(AppLogLevel::Debug));
    dialog.reject();

    settings.sync();
    int saved = settings.value("logging/level", -1).toInt();
    EXPECT_EQ(saved, to_settings_value(AppLogLevel::Information));

    // Clean up
    settings.remove("logging/level");
}

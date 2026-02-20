#pragma once

#include <memory>
#include <QDialog>

namespace dicom_viewer::ui {

/**
 * @brief Application settings dialog
 *
 * Provides UI for configuring application settings, starting with
 * log level configuration. Users can select from four log levels
 * with descriptions, and changes are persisted via QSettings.
 *
 * @trace SRS-FR-041
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

    // Non-copyable
    SettingsDialog(const SettingsDialog&) = delete;
    SettingsDialog& operator=(const SettingsDialog&) = delete;

public slots:
    void accept() override;

private slots:
    void onLogLevelChanged(int index);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void applyLogLevel();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

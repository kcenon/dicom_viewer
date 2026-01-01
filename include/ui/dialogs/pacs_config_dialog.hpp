#pragma once

#include <memory>
#include <QDialog>
#include <QUuid>

namespace dicom_viewer::services {
class PacsConfigManager;
}

namespace dicom_viewer::ui {

/**
 * @brief Dialog for managing PACS server configurations
 *
 * Provides a UI for adding, editing, and removing PACS server
 * configurations. Includes a connection test button using C-ECHO.
 *
 * @trace SRS-FR-038
 */
class PacsConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit PacsConfigDialog(services::PacsConfigManager* manager,
                              QWidget* parent = nullptr);
    ~PacsConfigDialog() override;

    // Non-copyable
    PacsConfigDialog(const PacsConfigDialog&) = delete;
    PacsConfigDialog& operator=(const PacsConfigDialog&) = delete;

    /**
     * @brief Get the currently selected server ID
     */
    [[nodiscard]] QUuid selectedServerId() const;

private slots:
    void onAddServer();
    void onEditServer();
    void onRemoveServer();
    void onTestConnection();
    void onSetDefault();
    void onServerSelectionChanged();
    void refreshServerList();

private:
    void setupUI();
    void setupConnections();
    void updateButtonStates();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

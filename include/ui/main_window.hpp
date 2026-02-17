#pragma once

#include <memory>
#include <string>
#include <QMainWindow>

namespace dicom_viewer::core {
class ProjectManager;
}

namespace dicom_viewer::ui {

/**
 * @brief Main application window
 *
 * Qt6-based main window with dockable panels, toolbar,
 * and VTK viewport integration.
 *
 * @trace SRS-FR-039, SRS-FR-040
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Non-copyable
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

public slots:
    /// Open DICOM directory
    void onOpenDirectory();

    /// Open single DICOM file
    void onOpenFile();

    /// Connect to PACS server
    void onConnectPACS();

    /// Toggle Storage SCP server
    void onToggleStorageSCP();

    /// Show settings dialog
    void onShowSettings();

    /// Show about dialog
    void onShowAbout();

    /// Reset window layout to default
    void onResetLayout();

    /// Toggle full screen mode
    void onToggleFullScreen();

    /// Show ROI statistics for current measurements
    void onShowRoiStatistics();

    /// Create a new project
    void onNewProject();

    /// Save the current project
    void onSaveProject();

    /// Save the current project to a new path
    void onSaveProjectAs();

    /// Open a project file
    void onOpenProject();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void setupPhaseControl();
    void applyDarkTheme();
    void saveLayout();
    void restoreLayout();
    void registerShortcuts();
    void uncheckAllMeasurementActions();
    void updateWindowTitle();
    void updateRecentProjectsMenu();
    bool promptSaveIfModified();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


/**
 * @file main_window.hpp
 * @brief Main application window with dockable panels and VTK viewports
 * @details Qt6-based main window coordinating DICOM loading, PACS access,
 *          settings management, and viewport layout. Integrates
 *          dockable tool panels, toolbar, and status bar with VTK
 *          rendering widgets.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QMainWindow-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
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
    void updateIntroPageRecentProjects();
    bool promptSaveIfModified();
    void importDicomDirectory(const QString& dir);
    void importProjectFile(const QString& path);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

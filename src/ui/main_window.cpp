#include "ui/main_window.hpp"
#include "ui/viewport_widget.hpp"
#include "ui/panels/patient_browser.hpp"
#include "ui/panels/tools_panel.hpp"
#include "ui/dialogs/pacs_config_dialog.hpp"
#include "services/pacs_config_manager.hpp"
#include "services/dicom_store_scp.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QShortcut>
#include <QActionGroup>
#include <QLabel>

namespace dicom_viewer::ui {

class MainWindow::Impl {
public:
    ViewportWidget* viewport = nullptr;
    PatientBrowser* patientBrowser = nullptr;
    ToolsPanel* toolsPanel = nullptr;

    QDockWidget* patientBrowserDock = nullptr;
    QDockWidget* toolsPanelDock = nullptr;

    QToolBar* mainToolBar = nullptr;
    QActionGroup* toolActionGroup = nullptr;

    // Status bar labels
    QLabel* statusLabel = nullptr;
    QLabel* positionLabel = nullptr;
    QLabel* valueLabel = nullptr;

    // View menu actions for dock toggle
    QAction* togglePatientBrowserAction = nullptr;
    QAction* toggleToolsPanelAction = nullptr;

    // PACS configuration manager
    services::PacsConfigManager* pacsConfigManager = nullptr;

    // Storage SCP
    std::unique_ptr<services::DicomStoreSCP> storageScp;
    QAction* toggleStorageScpAction = nullptr;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , impl_(std::make_unique<Impl>())
{
    setWindowTitle("DICOM Viewer");
    setMinimumSize(1280, 720);

    // Initialize PACS config manager
    impl_->pacsConfigManager = new services::PacsConfigManager(this);

    // Initialize Storage SCP
    impl_->storageScp = std::make_unique<services::DicomStoreSCP>();

    applyDarkTheme();
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupDockWidgets();
    setupStatusBar();
    setupConnections();
    registerShortcuts();
    restoreLayout();
}

MainWindow::~MainWindow()
{
    saveLayout();
}

void MainWindow::setupUI()
{
    impl_->viewport = new ViewportWidget(this);
    setCentralWidget(impl_->viewport);
}

void MainWindow::setupMenuBar()
{
    // File menu
    auto fileMenu = menuBar()->addMenu(tr("&File"));

    auto openDirAction = fileMenu->addAction(tr("Open &Directory..."));
    openDirAction->setShortcut(QKeySequence::Open);
    openDirAction->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(openDirAction, &QAction::triggered, this, &MainWindow::onOpenDirectory);

    auto openFileAction = fileMenu->addAction(tr("Open &File..."));
    openFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(openFileAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    fileMenu->addSeparator();

    auto pacsAction = fileMenu->addAction(tr("Connect to &PACS..."));
    pacsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(pacsAction, &QAction::triggered, this, &MainWindow::onConnectPACS);

    impl_->toggleStorageScpAction = fileMenu->addAction(tr("Start &Storage SCP"));
    impl_->toggleStorageScpAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    impl_->toggleStorageScpAction->setCheckable(true);
    connect(impl_->toggleStorageScpAction, &QAction::triggered,
            this, &MainWindow::onToggleStorageSCP);

    fileMenu->addSeparator();

    auto saveScreenshotAction = fileMenu->addAction(tr("&Save Screenshot..."));
    saveScreenshotAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(saveScreenshotAction, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(
            this, tr("Save Screenshot"), QString(),
            tr("PNG Images (*.png);;JPEG Images (*.jpg)"));
        if (!filePath.isEmpty()) {
            impl_->viewport->captureScreenshot(filePath);
            statusBar()->showMessage(tr("Screenshot saved: %1").arg(filePath), 3000);
        }
    });

    fileMenu->addSeparator();

    auto exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    // Edit menu
    auto editMenu = menuBar()->addMenu(tr("&Edit"));

    auto undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(false);

    auto redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(false);

    editMenu->addSeparator();

    auto settingsAction = editMenu->addAction(tr("&Settings..."));
    settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onShowSettings);

    // View menu
    auto viewMenu = menuBar()->addMenu(tr("&View"));

    impl_->togglePatientBrowserAction = viewMenu->addAction(tr("&Patient Browser"));
    impl_->togglePatientBrowserAction->setCheckable(true);
    impl_->togglePatientBrowserAction->setChecked(true);
    impl_->togglePatientBrowserAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));

    impl_->toggleToolsPanelAction = viewMenu->addAction(tr("&Tools Panel"));
    impl_->toggleToolsPanelAction->setCheckable(true);
    impl_->toggleToolsPanelAction->setChecked(true);
    impl_->toggleToolsPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));

    viewMenu->addSeparator();

    auto fullScreenAction = viewMenu->addAction(tr("&Full Screen"));
    fullScreenAction->setShortcut(Qt::Key_F11);
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::triggered, this, &MainWindow::onToggleFullScreen);

    auto resetLayoutAction = viewMenu->addAction(tr("&Reset Layout"));
    resetLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetLayout);

    // Image menu
    auto imageMenu = menuBar()->addMenu(tr("&Image"));

    auto zoomInAction = imageMenu->addAction(tr("Zoom &In"));
    zoomInAction->setShortcut(QKeySequence::ZoomIn);

    auto zoomOutAction = imageMenu->addAction(tr("Zoom &Out"));
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);

    auto fitToWindowAction = imageMenu->addAction(tr("&Fit to Window"));
    fitToWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(fitToWindowAction, &QAction::triggered, this, [this]() {
        impl_->viewport->resetCamera();
    });

    imageMenu->addSeparator();

    auto resetWLAction = imageMenu->addAction(tr("Reset &Window/Level"));
    resetWLAction->setShortcut(QKeySequence(Qt::Key_Escape));

    // Tools menu
    auto toolsMenu = menuBar()->addMenu(tr("&Tools"));

    auto distanceAction = toolsMenu->addAction(tr("&Distance"));
    distanceAction->setShortcut(QKeySequence(Qt::Key_D));

    auto angleAction = toolsMenu->addAction(tr("&Angle"));
    angleAction->setShortcut(QKeySequence(Qt::Key_A));

    auto roiAction = toolsMenu->addAction(tr("&ROI"));
    roiAction->setShortcut(QKeySequence(Qt::Key_R));

    // Window menu
    auto windowMenu = menuBar()->addMenu(tr("&Window"));

    auto cascadeAction = windowMenu->addAction(tr("&Cascade"));
    auto tileAction = windowMenu->addAction(tr("&Tile"));

    // Help menu
    auto helpMenu = menuBar()->addMenu(tr("&Help"));

    auto docsAction = helpMenu->addAction(tr("&Documentation"));
    docsAction->setShortcut(QKeySequence::HelpContents);

    helpMenu->addSeparator();

    auto aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onShowAbout);
}

void MainWindow::setupToolBar()
{
    impl_->mainToolBar = addToolBar(tr("Main"));
    impl_->mainToolBar->setMovable(false);
    impl_->mainToolBar->setIconSize(QSize(24, 24));

    auto openAction = impl_->mainToolBar->addAction(
        style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Open"));
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenDirectory);

    impl_->mainToolBar->addSeparator();

    impl_->toolActionGroup = new QActionGroup(this);
    impl_->toolActionGroup->setExclusive(true);

    auto scrollAction = impl_->mainToolBar->addAction(tr("Scroll"));
    scrollAction->setCheckable(true);
    scrollAction->setChecked(true);
    impl_->toolActionGroup->addAction(scrollAction);

    auto zoomAction = impl_->mainToolBar->addAction(tr("Zoom"));
    zoomAction->setCheckable(true);
    impl_->toolActionGroup->addAction(zoomAction);

    auto panAction = impl_->mainToolBar->addAction(tr("Pan"));
    panAction->setCheckable(true);
    impl_->toolActionGroup->addAction(panAction);

    auto wlAction = impl_->mainToolBar->addAction(tr("W/L"));
    wlAction->setCheckable(true);
    impl_->toolActionGroup->addAction(wlAction);

    impl_->mainToolBar->addSeparator();

    auto measureAction = impl_->mainToolBar->addAction(tr("Measure"));
    measureAction->setCheckable(true);
    impl_->toolActionGroup->addAction(measureAction);

    impl_->mainToolBar->addSeparator();

    auto resetAction = impl_->mainToolBar->addAction(tr("Reset"));
    connect(resetAction, &QAction::triggered, this, [this]() {
        impl_->viewport->resetCamera();
    });
}

void MainWindow::setupDockWidgets()
{
    // Patient Browser (left)
    impl_->patientBrowserDock = new QDockWidget(tr("Patient Browser"), this);
    impl_->patientBrowserDock->setObjectName("PatientBrowserDock");
    impl_->patientBrowser = new PatientBrowser();
    impl_->patientBrowserDock->setWidget(impl_->patientBrowser);
    impl_->patientBrowserDock->setMinimumWidth(250);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->patientBrowserDock);

    // Tools Panel (right)
    impl_->toolsPanelDock = new QDockWidget(tr("Tools"), this);
    impl_->toolsPanelDock->setObjectName("ToolsPanelDock");
    impl_->toolsPanel = new ToolsPanel();
    impl_->toolsPanelDock->setWidget(impl_->toolsPanel);
    impl_->toolsPanelDock->setMinimumWidth(200);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolsPanelDock);
}

void MainWindow::setupStatusBar()
{
    impl_->statusLabel = new QLabel(tr("Ready"));
    impl_->positionLabel = new QLabel();
    impl_->valueLabel = new QLabel();

    statusBar()->addWidget(impl_->statusLabel, 1);
    statusBar()->addPermanentWidget(impl_->positionLabel);
    statusBar()->addPermanentWidget(impl_->valueLabel);
}

void MainWindow::setupConnections()
{
    // Dock visibility toggles
    connect(impl_->togglePatientBrowserAction, &QAction::toggled,
            impl_->patientBrowserDock, &QDockWidget::setVisible);
    connect(impl_->patientBrowserDock, &QDockWidget::visibilityChanged,
            impl_->togglePatientBrowserAction, &QAction::setChecked);

    connect(impl_->toggleToolsPanelAction, &QAction::toggled,
            impl_->toolsPanelDock, &QDockWidget::setVisible);
    connect(impl_->toolsPanelDock, &QDockWidget::visibilityChanged,
            impl_->toggleToolsPanelAction, &QAction::setChecked);

    // Patient browser -> Load series
    connect(impl_->patientBrowser, &PatientBrowser::seriesLoadRequested,
            this, [this](const QString& /*seriesUid*/, const QString& /*path*/) {
                impl_->statusLabel->setText(tr("Loading series..."));
                // TODO: Load series through controller
            });

    // Tools panel -> Viewport
    connect(impl_->toolsPanel, &ToolsPanel::windowLevelChanged,
            impl_->viewport, &ViewportWidget::setWindowLevel);

    connect(impl_->toolsPanel, &ToolsPanel::presetSelected,
            impl_->viewport, &ViewportWidget::applyPreset);

    connect(impl_->toolsPanel, &ToolsPanel::visualizationModeChanged,
            this, [this](int mode) {
                impl_->viewport->setMode(static_cast<ViewportMode>(mode));
            });

    // Viewport -> Tools panel (bidirectional sync)
    connect(impl_->viewport, &ViewportWidget::windowLevelChanged,
            impl_->toolsPanel, &ToolsPanel::setWindowLevel);

    // Viewport -> Status bar
    connect(impl_->viewport, &ViewportWidget::voxelValueChanged,
            this, [this](double value, double x, double y, double z) {
                impl_->positionLabel->setText(
                    QString("Position: (%1, %2, %3)")
                        .arg(x, 0, 'f', 1).arg(y, 0, 'f', 1).arg(z, 0, 'f', 1));
                impl_->valueLabel->setText(
                    QString("Value: %1 HU").arg(value, 0, 'f', 0));
            });
}

void MainWindow::applyDarkTheme()
{
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));

    qApp->setStyle("Fusion");
    qApp->setPalette(darkPalette);

    // Custom stylesheet for additional styling
    qApp->setStyleSheet(R"(
        QToolTip {
            color: #ffffff;
            background-color: #353535;
            border: 1px solid #767676;
            padding: 4px;
        }
        QDockWidget::title {
            background-color: #404040;
            padding: 4px;
        }
        QGroupBox {
            border: 1px solid #505050;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
        }
    )");
}

void MainWindow::saveLayout()
{
    QSettings settings("DicomViewer", "DicomViewer");
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/state", saveState());
}

void MainWindow::restoreLayout()
{
    QSettings settings("DicomViewer", "DicomViewer");
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/state").toByteArray());
}

void MainWindow::registerShortcuts()
{
    // Navigation shortcuts
    auto scrollUpShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    connect(scrollUpShortcut, &QShortcut::activated, this, [](){});

    auto scrollDownShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    connect(scrollDownShortcut, &QShortcut::activated, this, [](){});

    // Page up/down for faster scrolling
    auto pageUpShortcut = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
    connect(pageUpShortcut, &QShortcut::activated, this, [](){});

    auto pageDownShortcut = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
    connect(pageDownShortcut, &QShortcut::activated, this, [](){});

    // Home/End for first/last slice
    auto homeShortcut = new QShortcut(QKeySequence(Qt::Key_Home), this);
    connect(homeShortcut, &QShortcut::activated, this, [](){});

    auto endShortcut = new QShortcut(QKeySequence(Qt::Key_End), this);
    connect(endShortcut, &QShortcut::activated, this, [](){});

    // Quick window presets
    auto bonePreset = new QShortcut(QKeySequence(Qt::Key_1), this);
    connect(bonePreset, &QShortcut::activated, this, [this](){
        impl_->viewport->applyPreset("CT Bone");
    });

    auto lungPreset = new QShortcut(QKeySequence(Qt::Key_2), this);
    connect(lungPreset, &QShortcut::activated, this, [this](){
        impl_->viewport->applyPreset("CT Lung");
    });

    auto abdomenPreset = new QShortcut(QKeySequence(Qt::Key_3), this);
    connect(abdomenPreset, &QShortcut::activated, this, [this](){
        impl_->viewport->applyPreset("CT Abdomen");
    });

    auto brainPreset = new QShortcut(QKeySequence(Qt::Key_4), this);
    connect(brainPreset, &QShortcut::activated, this, [this](){
        impl_->viewport->applyPreset("CT Brain");
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveLayout();
    event->accept();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
}

void MainWindow::onOpenDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Open DICOM Directory"), QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        impl_->statusLabel->setText(tr("Loading %1...").arg(dir));
        // TODO: Load DICOM series through controller
    }
}

void MainWindow::onOpenFile()
{
    QString file = QFileDialog::getOpenFileName(
        this, tr("Open DICOM File"), QString(),
        tr("DICOM Files (*.dcm);;All Files (*)"));

    if (!file.isEmpty()) {
        impl_->statusLabel->setText(tr("Loading %1...").arg(file));
        // TODO: Load DICOM file through controller
    }
}

void MainWindow::onConnectPACS()
{
    PacsConfigDialog dialog(impl_->pacsConfigManager, this);
    dialog.exec();
}

void MainWindow::onToggleStorageSCP()
{
    if (impl_->storageScp->isRunning()) {
        impl_->storageScp->stop();
        impl_->toggleStorageScpAction->setText(tr("Start &Storage SCP"));
        impl_->toggleStorageScpAction->setChecked(false);
        statusBar()->showMessage(tr("Storage SCP stopped"), 3000);
    } else {
        services::StorageScpConfig config;
        config.port = 11112;
        config.aeTitle = "DICOM_VIEWER_SCP";
        config.storageDirectory = QDir::homePath().toStdString() + "/DICOM_Incoming";

        // Set up image received callback
        impl_->storageScp->setImageReceivedCallback(
            [this](const services::ReceivedImageInfo& info) {
                // Queue UI update to main thread
                QMetaObject::invokeMethod(this, [this, info]() {
                    impl_->statusLabel->setText(
                        tr("Received: %1").arg(
                            QString::fromStdString(info.filePath.filename().string())));
                    // TODO: Optionally auto-load received images
                }, Qt::QueuedConnection);
            });

        auto result = impl_->storageScp->start(config);
        if (result) {
            impl_->toggleStorageScpAction->setText(tr("Stop &Storage SCP"));
            impl_->toggleStorageScpAction->setChecked(true);
            statusBar()->showMessage(
                tr("Storage SCP started on port %1").arg(config.port), 3000);
        } else {
            impl_->toggleStorageScpAction->setChecked(false);
            QMessageBox::warning(this, tr("Storage SCP Error"),
                tr("Failed to start Storage SCP:\n%1")
                    .arg(QString::fromStdString(result.error().toString())));
        }
    }
}

void MainWindow::onShowSettings()
{
    QMessageBox::information(this, tr("Settings"),
        tr("Settings dialog will be implemented in a future version."));
}

void MainWindow::onShowAbout()
{
    QMessageBox::about(this, tr("About DICOM Viewer"),
        tr("<h3>DICOM Viewer v0.3.0</h3>"
           "<p>High-performance medical image viewer<br>"
           "with 3D Volume Rendering and MPR views.</p>"
           "<p>Features:</p>"
           "<ul>"
           "<li>2D slice viewing with window/level control</li>"
           "<li>Multi-planar reconstruction (MPR)</li>"
           "<li>GPU-accelerated volume rendering</li>"
           "<li>Surface rendering with Marching Cubes</li>"
           "</ul>"
           "<p>&copy; 2025 kcenon</p>"));
}

void MainWindow::onResetLayout()
{
    impl_->patientBrowserDock->setFloating(false);
    impl_->toolsPanelDock->setFloating(false);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->patientBrowserDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolsPanelDock);
    impl_->patientBrowserDock->show();
    impl_->toolsPanelDock->show();
}

void MainWindow::onToggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

} // namespace dicom_viewer::ui

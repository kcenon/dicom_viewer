#include "ui/main_window.hpp"
#include "ui/viewport_widget.hpp"
#include "ui/widgets/phase_slider_widget.hpp"
#include "ui/widgets/sp_mode_toggle.hpp"
#include "ui/panels/patient_browser.hpp"
#include "ui/panels/tools_panel.hpp"
#include "ui/panels/statistics_panel.hpp"
#include "ui/panels/segmentation_panel.hpp"
#include "ui/dialogs/pacs_config_dialog.hpp"
#include "services/pacs_config_manager.hpp"
#include "services/dicom_store_scp.hpp"
#include "services/measurement/roi_statistics.hpp"
#include "services/flow/temporal_navigator.hpp"

#include <itkImage.h>
#include <itkVTKImageToImageFilter.h>

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
#include <QTimer>

namespace dicom_viewer::ui {

class MainWindow::Impl {
public:
    ViewportWidget* viewport = nullptr;
    PatientBrowser* patientBrowser = nullptr;
    ToolsPanel* toolsPanel = nullptr;

    QDockWidget* patientBrowserDock = nullptr;
    QDockWidget* toolsPanelDock = nullptr;
    QDockWidget* statisticsPanelDock = nullptr;
    QDockWidget* segmentationPanelDock = nullptr;
    StatisticsPanel* statisticsPanel = nullptr;
    SegmentationPanel* segmentationPanel = nullptr;

    QToolBar* mainToolBar = nullptr;
    QActionGroup* toolActionGroup = nullptr;

    // Status bar labels
    QLabel* statusLabel = nullptr;
    QLabel* positionLabel = nullptr;
    QLabel* valueLabel = nullptr;

    // View menu actions for dock toggle
    QAction* togglePatientBrowserAction = nullptr;
    QAction* toggleToolsPanelAction = nullptr;
    QAction* toggleStatisticsPanelAction = nullptr;
    QAction* toggleSegmentationPanelAction = nullptr;

    // Statistics action
    QAction* showStatisticsAction = nullptr;

    // PACS configuration manager
    services::PacsConfigManager* pacsConfigManager = nullptr;

    // Storage SCP
    std::unique_ptr<services::DicomStoreSCP> storageScp;
    QAction* toggleStorageScpAction = nullptr;

    // Phase control
    PhaseSliderWidget* phaseSlider = nullptr;
    QDockWidget* phaseControlDock = nullptr;
    QAction* togglePhaseControlAction = nullptr;
    services::TemporalNavigator temporalNavigator;
    QTimer* cineTimer = nullptr;

    // Measurement actions
    QAction* distanceAction = nullptr;
    QAction* angleAction = nullptr;
    QAction* rectangleRoiAction = nullptr;
    QAction* ellipseRoiAction = nullptr;
    QAction* polygonRoiAction = nullptr;
    QAction* freehandRoiAction = nullptr;
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
    setupPhaseControl();
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

    impl_->toggleStatisticsPanelAction = viewMenu->addAction(tr("&Statistics Panel"));
    impl_->toggleStatisticsPanelAction->setCheckable(true);
    impl_->toggleStatisticsPanelAction->setChecked(false);
    impl_->toggleStatisticsPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));

    impl_->toggleSegmentationPanelAction = viewMenu->addAction(tr("Se&gmentation Panel"));
    impl_->toggleSegmentationPanelAction->setCheckable(true);
    impl_->toggleSegmentationPanelAction->setChecked(false);
    impl_->toggleSegmentationPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));

    impl_->togglePhaseControlAction = viewMenu->addAction(tr("&Phase Control"));
    impl_->togglePhaseControlAction->setCheckable(true);
    impl_->togglePhaseControlAction->setChecked(false);
    impl_->togglePhaseControlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));

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

    impl_->distanceAction = toolsMenu->addAction(tr("&Distance"));
    impl_->distanceAction->setShortcut(QKeySequence(Qt::Key_D));
    impl_->distanceAction->setCheckable(true);
    connect(impl_->distanceAction, &QAction::triggered, this, [this]() {
        if (impl_->distanceAction->isChecked()) {
            uncheckAllMeasurementActions();
            impl_->distanceAction->setChecked(true);
            impl_->viewport->startDistanceMeasurement();
        } else {
            impl_->viewport->cancelMeasurement();
        }
    });

    impl_->angleAction = toolsMenu->addAction(tr("&Angle"));
    impl_->angleAction->setShortcut(QKeySequence(Qt::Key_A));
    impl_->angleAction->setCheckable(true);
    connect(impl_->angleAction, &QAction::triggered, this, [this]() {
        if (impl_->angleAction->isChecked()) {
            uncheckAllMeasurementActions();
            impl_->angleAction->setChecked(true);
            impl_->viewport->startAngleMeasurement();
        } else {
            impl_->viewport->cancelMeasurement();
        }
    });

    toolsMenu->addSeparator();

    auto clearMeasurementsAction = toolsMenu->addAction(tr("&Clear All Measurements"));
    clearMeasurementsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(clearMeasurementsAction, &QAction::triggered, this, [this]() {
        impl_->viewport->deleteAllMeasurements();
        impl_->statisticsPanel->clearStatistics();
        statusBar()->showMessage(tr("All measurements cleared"), 3000);
    });

    impl_->showStatisticsAction = toolsMenu->addAction(tr("&Show ROI Statistics"));
    connect(impl_->showStatisticsAction, &QAction::triggered,
            this, &MainWindow::onShowRoiStatistics);

    toolsMenu->addSeparator();

    // ROI submenu for area measurements
    auto roiMenu = toolsMenu->addMenu(tr("&ROI"));

    impl_->rectangleRoiAction = roiMenu->addAction(tr("&Rectangle"));
    impl_->rectangleRoiAction->setShortcut(QKeySequence(Qt::Key_R));
    impl_->rectangleRoiAction->setCheckable(true);
    connect(impl_->rectangleRoiAction, &QAction::triggered, this, [this]() {
        if (impl_->rectangleRoiAction->isChecked()) {
            uncheckAllMeasurementActions();
            impl_->rectangleRoiAction->setChecked(true);
            impl_->viewport->startAreaMeasurement(services::RoiType::Rectangle);
        } else {
            impl_->viewport->cancelMeasurement();
        }
    });

    impl_->ellipseRoiAction = roiMenu->addAction(tr("&Ellipse"));
    impl_->ellipseRoiAction->setShortcut(QKeySequence(Qt::Key_E));
    impl_->ellipseRoiAction->setCheckable(true);
    connect(impl_->ellipseRoiAction, &QAction::triggered, this, [this]() {
        if (impl_->ellipseRoiAction->isChecked()) {
            uncheckAllMeasurementActions();
            impl_->ellipseRoiAction->setChecked(true);
            impl_->viewport->startAreaMeasurement(services::RoiType::Ellipse);
        } else {
            impl_->viewport->cancelMeasurement();
        }
    });

    impl_->polygonRoiAction = roiMenu->addAction(tr("&Polygon"));
    impl_->polygonRoiAction->setCheckable(true);
    connect(impl_->polygonRoiAction, &QAction::triggered, this, [this]() {
        if (impl_->polygonRoiAction->isChecked()) {
            uncheckAllMeasurementActions();
            impl_->polygonRoiAction->setChecked(true);
            impl_->viewport->startAreaMeasurement(services::RoiType::Polygon);
        } else {
            impl_->viewport->cancelMeasurement();
        }
    });

    impl_->freehandRoiAction = roiMenu->addAction(tr("&Freehand"));
    impl_->freehandRoiAction->setShortcut(QKeySequence(Qt::Key_F));
    impl_->freehandRoiAction->setCheckable(true);
    connect(impl_->freehandRoiAction, &QAction::triggered, this, [this]() {
        if (impl_->freehandRoiAction->isChecked()) {
            uncheckAllMeasurementActions();
            impl_->freehandRoiAction->setChecked(true);
            impl_->viewport->startAreaMeasurement(services::RoiType::Freehand);
        } else {
            impl_->viewport->cancelMeasurement();
        }
    });

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

    // Statistics Panel (right, tabbed with Tools)
    impl_->statisticsPanelDock = new QDockWidget(tr("Statistics"), this);
    impl_->statisticsPanelDock->setObjectName("StatisticsPanelDock");
    impl_->statisticsPanel = new StatisticsPanel();
    impl_->statisticsPanelDock->setWidget(impl_->statisticsPanel);
    impl_->statisticsPanelDock->setMinimumWidth(250);
    addDockWidget(Qt::RightDockWidgetArea, impl_->statisticsPanelDock);
    tabifyDockWidget(impl_->toolsPanelDock, impl_->statisticsPanelDock);
    impl_->statisticsPanelDock->hide();  // Initially hidden

    // Segmentation Panel (right, tabbed with Tools and Statistics)
    impl_->segmentationPanelDock = new QDockWidget(tr("Segmentation"), this);
    impl_->segmentationPanelDock->setObjectName("SegmentationPanelDock");
    impl_->segmentationPanel = new SegmentationPanel();
    impl_->segmentationPanelDock->setWidget(impl_->segmentationPanel);
    impl_->segmentationPanelDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, impl_->segmentationPanelDock);
    tabifyDockWidget(impl_->statisticsPanelDock, impl_->segmentationPanelDock);
    impl_->segmentationPanelDock->hide();  // Initially hidden
}

void MainWindow::setupPhaseControl()
{
    // Phase control dock (left area, below Patient Browser)
    impl_->phaseControlDock = new QDockWidget(tr("Phase Control"), this);
    impl_->phaseControlDock->setObjectName("PhaseControlDock");
    impl_->phaseSlider = new PhaseSliderWidget();
    impl_->phaseControlDock->setWidget(impl_->phaseSlider);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->phaseControlDock);
    impl_->phaseControlDock->hide();  // Hidden until 4D data is loaded

    // Cine playback timer
    impl_->cineTimer = new QTimer(this);

    // Phase slider → TemporalNavigator
    connect(impl_->phaseSlider, &PhaseSliderWidget::phaseChangeRequested,
            this, [this](int phaseIndex) {
        (void)impl_->temporalNavigator.goToPhase(phaseIndex);
        impl_->viewport->setPhaseIndex(phaseIndex);
    });

    connect(impl_->phaseSlider, &PhaseSliderWidget::playRequested,
            this, [this]() {
        impl_->temporalNavigator.play();
        impl_->phaseSlider->setPlaying(true);
        int fps = impl_->phaseSlider->fps();
        auto state = impl_->temporalNavigator.playbackState();
        int intervalMs = static_cast<int>(1000.0 / (fps * state.speedMultiplier));
        impl_->cineTimer->start(intervalMs);
    });

    connect(impl_->phaseSlider, &PhaseSliderWidget::stopRequested,
            this, [this]() {
        impl_->temporalNavigator.pause();
        impl_->phaseSlider->setPlaying(false);
        impl_->cineTimer->stop();
    });

    // Cine timer tick → advance phase
    connect(impl_->cineTimer, &QTimer::timeout,
            this, [this]() {
        auto result = impl_->temporalNavigator.tick();
        if (result) {
            int phase = impl_->temporalNavigator.currentPhase();
            impl_->phaseSlider->setCurrentPhase(phase);
            impl_->viewport->setPhaseIndex(phase);
        } else {
            // Reached end without looping
            impl_->cineTimer->stop();
            impl_->phaseSlider->setPlaying(false);
            impl_->temporalNavigator.pause();
        }
    });

    // S/P mode toggle → propagate to status bar and viewport
    connect(impl_->phaseSlider, &PhaseSliderWidget::scrollModeChanged,
            this, [this](ScrollMode mode) {
        impl_->viewport->setScrollMode(mode);
        if (mode == ScrollMode::Phase) {
            impl_->statusLabel->setText(tr("Phase scroll mode"));
        } else {
            impl_->statusLabel->setText(tr("Slice scroll mode"));
        }
    });

    // Viewport phase scroll (wheel in Phase mode) → navigate phases
    connect(impl_->viewport, &ViewportWidget::phaseScrollRequested,
            this, [this](int delta) {
        if (!impl_->temporalNavigator.isInitialized()) return;
        if (delta > 0) {
            (void)impl_->temporalNavigator.nextPhase();
        } else {
            (void)impl_->temporalNavigator.previousPhase();
        }
        int phase = impl_->temporalNavigator.currentPhase();
        impl_->phaseSlider->setCurrentPhase(phase);
        impl_->viewport->setPhaseIndex(phase);
    });

    // FPS change → update cine timer interval
    connect(impl_->phaseSlider, &PhaseSliderWidget::fpsChanged,
            this, [this](int fps) {
        if (impl_->cineTimer->isActive()) {
            auto state = impl_->temporalNavigator.playbackState();
            int intervalMs = static_cast<int>(1000.0 / (fps * state.speedMultiplier));
            impl_->cineTimer->setInterval(intervalMs);
        }
    });
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

    connect(impl_->toggleStatisticsPanelAction, &QAction::toggled,
            impl_->statisticsPanelDock, &QDockWidget::setVisible);
    connect(impl_->statisticsPanelDock, &QDockWidget::visibilityChanged,
            impl_->toggleStatisticsPanelAction, &QAction::setChecked);

    connect(impl_->toggleSegmentationPanelAction, &QAction::toggled,
            impl_->segmentationPanelDock, &QDockWidget::setVisible);
    connect(impl_->segmentationPanelDock, &QDockWidget::visibilityChanged,
            impl_->toggleSegmentationPanelAction, &QAction::setChecked);

    connect(impl_->togglePhaseControlAction, &QAction::toggled,
            impl_->phaseControlDock, &QDockWidget::setVisible);
    connect(impl_->phaseControlDock, &QDockWidget::visibilityChanged,
            impl_->togglePhaseControlAction, &QAction::setChecked);

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

    // Segmentation panel -> Viewport
    connect(impl_->segmentationPanel, &SegmentationPanel::toolChanged,
            impl_->viewport, &ViewportWidget::setSegmentationTool);
    connect(impl_->segmentationPanel, &SegmentationPanel::brushSizeChanged,
            impl_->viewport, &ViewportWidget::setSegmentationBrushSize);
    connect(impl_->segmentationPanel, &SegmentationPanel::brushShapeChanged,
            impl_->viewport, &ViewportWidget::setSegmentationBrushShape);
    connect(impl_->segmentationPanel, &SegmentationPanel::activeLabelChanged,
            impl_->viewport, &ViewportWidget::setSegmentationActiveLabel);
    connect(impl_->segmentationPanel, &SegmentationPanel::undoRequested,
            impl_->viewport, &ViewportWidget::undoSegmentationOperation);
    connect(impl_->segmentationPanel, &SegmentationPanel::completeRequested,
            impl_->viewport, &ViewportWidget::completeSegmentationOperation);
    connect(impl_->segmentationPanel, &SegmentationPanel::clearAllRequested,
            impl_->viewport, &ViewportWidget::clearAllSegmentation);

    // Segmentation status updates
    connect(impl_->viewport, &ViewportWidget::segmentationModified,
            this, [this](int /*sliceIndex*/) {
                impl_->statusLabel->setText(tr("Segmentation modified"));
            });

    // Measurement completed -> Status bar
    connect(impl_->viewport, &ViewportWidget::distanceMeasurementCompleted,
            this, [this](double distanceMm, int /*measurementId*/) {
                impl_->statusLabel->setText(
                    tr("Distance: %1 mm").arg(distanceMm, 0, 'f', 2));
                impl_->distanceAction->setChecked(false);
            });

    connect(impl_->viewport, &ViewportWidget::angleMeasurementCompleted,
            this, [this](double angleDegrees, int /*measurementId*/) {
                impl_->statusLabel->setText(
                    tr("Angle: %1°").arg(angleDegrees, 0, 'f', 1));
                impl_->angleAction->setChecked(false);
            });

    connect(impl_->viewport, &ViewportWidget::areaMeasurementCompleted,
            this, [this](double areaMm2, double areaCm2, int /*measurementId*/) {
                if (areaCm2 >= 0.01) {
                    impl_->statusLabel->setText(
                        tr("Area: %1 mm² (%2 cm²)").arg(areaMm2, 0, 'f', 2).arg(areaCm2, 0, 'f', 2));
                } else {
                    impl_->statusLabel->setText(
                        tr("Area: %1 mm²").arg(areaMm2, 0, 'f', 2));
                }
                uncheckAllMeasurementActions();
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

    // Phase navigation: Left/Right for single phase step
    auto phaseLeftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(phaseLeftShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->temporalNavigator.isInitialized()) return;
        (void)impl_->temporalNavigator.previousPhase();
        int phase = impl_->temporalNavigator.currentPhase();
        impl_->phaseSlider->setCurrentPhase(phase);
        impl_->viewport->setPhaseIndex(phase);
    });

    auto phaseRightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(phaseRightShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->temporalNavigator.isInitialized()) return;
        (void)impl_->temporalNavigator.nextPhase();
        int phase = impl_->temporalNavigator.currentPhase();
        impl_->phaseSlider->setCurrentPhase(phase);
        impl_->viewport->setPhaseIndex(phase);
    });

    // Phase page step: < / > for 5-phase jump
    auto phasePageBackShortcut = new QShortcut(
        QKeySequence(Qt::SHIFT | Qt::Key_Comma), this);
    connect(phasePageBackShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->temporalNavigator.isInitialized()) return;
        int target = std::max(0, impl_->temporalNavigator.currentPhase() - 5);
        (void)impl_->temporalNavigator.goToPhase(target);
        impl_->phaseSlider->setCurrentPhase(target);
        impl_->viewport->setPhaseIndex(target);
    });

    auto phasePageForwardShortcut = new QShortcut(
        QKeySequence(Qt::SHIFT | Qt::Key_Period), this);
    connect(phasePageForwardShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->temporalNavigator.isInitialized()) return;
        int maxPhase = impl_->temporalNavigator.phaseCount() - 1;
        int target = std::min(maxPhase, impl_->temporalNavigator.currentPhase() + 5);
        (void)impl_->temporalNavigator.goToPhase(target);
        impl_->phaseSlider->setCurrentPhase(target);
        impl_->viewport->setPhaseIndex(target);
    });

    // S/P mode toggle shortcuts
    auto sliceModeShortcut = new QShortcut(QKeySequence(Qt::Key_S), this);
    connect(sliceModeShortcut, &QShortcut::activated, this, [this]() {
        impl_->phaseSlider->setScrollMode(ScrollMode::Slice);
    });

    auto phaseModeShortcut = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(phaseModeShortcut, &QShortcut::activated, this, [this]() {
        impl_->phaseSlider->setScrollMode(ScrollMode::Phase);
    });

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
    impl_->statisticsPanelDock->setFloating(false);
    impl_->segmentationPanelDock->setFloating(false);
    impl_->phaseControlDock->setFloating(false);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->patientBrowserDock);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->phaseControlDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolsPanelDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->statisticsPanelDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->segmentationPanelDock);
    tabifyDockWidget(impl_->toolsPanelDock, impl_->statisticsPanelDock);
    tabifyDockWidget(impl_->statisticsPanelDock, impl_->segmentationPanelDock);
    impl_->patientBrowserDock->show();
    impl_->toolsPanelDock->show();
    impl_->toolsPanelDock->raise();  // Show Tools tab
}

void MainWindow::onToggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::uncheckAllMeasurementActions()
{
    impl_->distanceAction->setChecked(false);
    impl_->angleAction->setChecked(false);
    impl_->rectangleRoiAction->setChecked(false);
    impl_->ellipseRoiAction->setChecked(false);
    impl_->polygonRoiAction->setChecked(false);
    impl_->freehandRoiAction->setChecked(false);
}

void MainWindow::onShowRoiStatistics()
{
    auto measurements = impl_->viewport->getAreaMeasurements();
    if (measurements.empty()) {
        statusBar()->showMessage(tr("No ROI measurements to analyze"), 3000);
        return;
    }

    auto vtkImage = impl_->viewport->getImageData();
    if (!vtkImage) {
        statusBar()->showMessage(tr("No image data available"), 3000);
        return;
    }

    // Convert VTK image to ITK image
    using ImageType = itk::Image<short, 3>;
    using FilterType = itk::VTKImageToImageFilter<ImageType>;
    auto filter = FilterType::New();
    filter->SetInput(vtkImage);

    try {
        filter->Update();
    } catch (const itk::ExceptionObject& e) {
        statusBar()->showMessage(tr("Failed to convert image: %1").arg(e.what()), 3000);
        return;
    }

    // Calculate statistics
    services::RoiStatisticsCalculator calculator;
    calculator.setImage(filter->GetOutput());

    int sliceIndex = impl_->viewport->getCurrentSlice();
    std::vector<services::RoiStatistics> stats;

    for (const auto& roi : measurements) {
        auto result = calculator.calculate(roi, sliceIndex);
        if (result) {
            stats.push_back(*result);
        }
    }

    if (stats.empty()) {
        statusBar()->showMessage(tr("Failed to calculate statistics"), 3000);
        return;
    }

    // Update statistics panel
    impl_->statisticsPanel->setMultipleStatistics(stats);

    // Show statistics panel
    impl_->statisticsPanelDock->show();
    impl_->statisticsPanelDock->raise();
    impl_->toggleStatisticsPanelAction->setChecked(true);

    statusBar()->showMessage(tr("Calculated statistics for %1 ROI(s)").arg(stats.size()), 3000);
}

} // namespace dicom_viewer::ui

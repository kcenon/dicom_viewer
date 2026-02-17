#include "ui/main_window.hpp"
#include "ui/viewport_widget.hpp"
#include "ui/viewport_layout_manager.hpp"
#include "ui/widgets/phase_slider_widget.hpp"
#include "ui/widgets/sp_mode_toggle.hpp"
#include "ui/panels/patient_browser.hpp"
#include "ui/panels/tools_panel.hpp"
#include "ui/panels/statistics_panel.hpp"
#include "ui/panels/segmentation_panel.hpp"
#include "ui/panels/overlay_control_panel.hpp"
#include "ui/panels/flow_tool_panel.hpp"
#include "ui/display_3d_controller.hpp"
#include "ui/dialogs/pacs_config_dialog.hpp"
#include "ui/dialogs/mask_wizard.hpp"
#include "ui/quantification_window.hpp"
#include "core/project_manager.hpp"
#include "core/series_builder.hpp"
#include "core/dicom_loader.hpp"
#include "services/enhanced_dicom/series_classifier.hpp"
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
    ViewportLayoutManager* layoutManager = nullptr;
    ViewportWidget* viewport = nullptr;
    PatientBrowser* patientBrowser = nullptr;
    ToolsPanel* toolsPanel = nullptr;

    QDockWidget* patientBrowserDock = nullptr;
    QDockWidget* toolsPanelDock = nullptr;
    QDockWidget* statisticsPanelDock = nullptr;
    QDockWidget* segmentationPanelDock = nullptr;
    QDockWidget* overlayControlDock = nullptr;
    QDockWidget* flowToolDock = nullptr;
    StatisticsPanel* statisticsPanel = nullptr;
    SegmentationPanel* segmentationPanel = nullptr;
    OverlayControlPanel* overlayControlPanel = nullptr;
    FlowToolPanel* flowToolPanel = nullptr;

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
    QAction* toggleOverlayControlAction = nullptr;
    QAction* toggleFlowToolAction = nullptr;

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

    // Edit menu actions
    QAction* undoAction = nullptr;
    QAction* redoAction = nullptr;

    // Measurement actions
    QAction* distanceAction = nullptr;
    QAction* angleAction = nullptr;
    QAction* rectangleRoiAction = nullptr;
    QAction* ellipseRoiAction = nullptr;
    QAction* polygonRoiAction = nullptr;
    QAction* freehandRoiAction = nullptr;

    // Layout actions
    QAction* singleLayoutAction = nullptr;
    QAction* dualLayoutAction = nullptr;
    QAction* quadLayoutAction = nullptr;

    // Active viewport W/L connections
    QMetaObject::Connection activeWlToToolsConn;
    QMetaObject::Connection toolsToActiveWlConn;

    // Quantification window
    QuantificationWindow* quantificationWindow = nullptr;

    // Display 3D controller
    std::unique_ptr<Display3DController> display3DController;

    // Project management
    std::unique_ptr<core::ProjectManager> projectManager;
    QMenu* recentProjectsMenu = nullptr;
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

    // Initialize Display 3D controller
    impl_->display3DController = std::make_unique<Display3DController>();

    // Initialize project manager
    impl_->projectManager = std::make_unique<core::ProjectManager>();
    auto recentPath = QDir(QSettings().fileName()).absolutePath();
    recentPath = QDir::homePath() + "/.dicom_viewer_recent.json";
    impl_->projectManager->setRecentProjectsPath(recentPath.toStdString());

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
    impl_->layoutManager = new ViewportLayoutManager(this);
    impl_->viewport = impl_->layoutManager->primaryViewport();
    setCentralWidget(impl_->layoutManager);
}

void MainWindow::setupMenuBar()
{
    // =========================================================================
    // File menu
    // =========================================================================
    auto fileMenu = menuBar()->addMenu(tr("&File"));

    // Project operations
    auto newProjectAction = fileMenu->addAction(tr("&New Project"));
    newProjectAction->setShortcut(QKeySequence::New);
    connect(newProjectAction, &QAction::triggered, this, &MainWindow::onNewProject);

    auto openProjectAction = fileMenu->addAction(tr("Open &Project..."));
    openProjectAction->setShortcut(QKeySequence::Open);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::onOpenProject);

    impl_->recentProjectsMenu = fileMenu->addMenu(tr("Open &Recent"));
    updateRecentProjectsMenu();

    fileMenu->addSeparator();

    auto saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveProject);

    auto saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveProjectAs);

    fileMenu->addSeparator();

    // DICOM import operations
    auto openDirAction = fileMenu->addAction(tr("Open &Directory..."));
    openDirAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    openDirAction->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(openDirAction, &QAction::triggered, this, &MainWindow::onOpenDirectory);

    auto openFileAction = fileMenu->addAction(tr("Open &File..."));
    connect(openFileAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    fileMenu->addSeparator();

    auto pacsAction = fileMenu->addAction(tr("Connect to &PACS..."));
    pacsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(pacsAction, &QAction::triggered, this, &MainWindow::onConnectPACS);

    impl_->toggleStorageScpAction = fileMenu->addAction(tr("Start S&torage SCP"));
    impl_->toggleStorageScpAction->setCheckable(true);
    connect(impl_->toggleStorageScpAction, &QAction::triggered,
            this, &MainWindow::onToggleStorageSCP);

    fileMenu->addSeparator();

    auto exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);

    // =========================================================================
    // Seg(3D) menu — 3D segmentation operations
    // =========================================================================
    auto seg3dMenu = menuBar()->addMenu(tr("Seg(&3D)"));

    impl_->undoAction = seg3dMenu->addAction(tr("&Undo"));
    impl_->undoAction->setShortcut(QKeySequence::Undo);
    impl_->undoAction->setEnabled(false);
    connect(impl_->undoAction, &QAction::triggered,
            this, [this]() { impl_->viewport->undoSegmentationCommand(); });

    impl_->redoAction = seg3dMenu->addAction(tr("&Redo"));
    impl_->redoAction->setShortcut(QKeySequence::Redo);
    impl_->redoAction->setEnabled(false);
    connect(impl_->redoAction, &QAction::triggered,
            this, [this]() { impl_->viewport->redoSegmentationCommand(); });

    seg3dMenu->addSeparator();

    auto saveSegAction = seg3dMenu->addAction(tr("&Save Segmentation"));
    saveSegAction->setEnabled(false);
    saveSegAction->setToolTip(tr("Save current segmentation (not yet implemented)"));

    seg3dMenu->addSeparator();

    auto exportMaskAction = seg3dMenu->addAction(tr("&Export Mask..."));
    exportMaskAction->setEnabled(false);
    exportMaskAction->setToolTip(tr("Export segmentation mask (not yet implemented)"));

    auto importMaskAction = seg3dMenu->addAction(tr("&Import Mask..."));
    importMaskAction->setEnabled(false);
    importMaskAction->setToolTip(tr("Import segmentation mask (not yet implemented)"));

    seg3dMenu->addSeparator();

    auto exportStlAction = seg3dMenu->addAction(tr("Export S&TL..."));
    exportStlAction->setEnabled(false);
    exportStlAction->setToolTip(tr("Export mesh as STL (not yet implemented)"));

    auto importStlAction = seg3dMenu->addAction(tr("Import ST&L..."));
    importStlAction->setEnabled(false);
    importStlAction->setToolTip(tr("Import mesh from STL (not yet implemented)"));

    // =========================================================================
    // Seg(2D) menu — 2D segmentation tools
    // =========================================================================
    auto seg2dMenu = menuBar()->addMenu(tr("Seg(&2D)"));

    auto brushToolAction = seg2dMenu->addAction(tr("&Brush Tool"));
    brushToolAction->setEnabled(false);
    brushToolAction->setToolTip(tr("2D brush segmentation tool (not yet implemented)"));

    auto eraserToolAction = seg2dMenu->addAction(tr("&Eraser Tool"));
    eraserToolAction->setEnabled(false);
    eraserToolAction->setToolTip(tr("2D eraser tool (not yet implemented)"));

    auto smartBrushAction = seg2dMenu->addAction(tr("&Smart Brush"));
    smartBrushAction->setEnabled(false);
    smartBrushAction->setToolTip(tr("AI-assisted brush (not yet implemented)"));

    seg2dMenu->addSeparator();

    auto growShrinkAction = seg2dMenu->addAction(tr("&Grow/Shrink Selection"));
    growShrinkAction->setEnabled(false);
    growShrinkAction->setToolTip(tr("Morphological grow/shrink (not yet implemented)"));

    auto thresholdSegAction = seg2dMenu->addAction(tr("&Threshold Segmentation"));
    thresholdSegAction->setEnabled(false);
    thresholdSegAction->setToolTip(tr("Threshold-based segmentation (not yet implemented)"));

    seg2dMenu->addSeparator();

    auto maskWizardAction = seg2dMenu->addAction(tr("&Mask Wizard..."));
    maskWizardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(maskWizardAction, &QAction::triggered, this, [this]() {
        auto* wizard = new MaskWizard(this);
        wizard->setAttribute(Qt::WA_DeleteOnClose);
        connect(wizard, &MaskWizard::wizardCompleted, this, [this]() {
            statusBar()->showMessage(tr("Mask Wizard completed"), 3000);
        });
        wizard->show();
    });

    // =========================================================================
    // Data Correction menu
    // =========================================================================
    auto dataCorrMenu = menuBar()->addMenu(tr("Data &Correction"));

    auto phaseUnwrapAction = dataCorrMenu->addAction(tr("&Phase Unwrapping"));
    phaseUnwrapAction->setEnabled(false);
    phaseUnwrapAction->setToolTip(tr("Phase unwrapping correction (not yet implemented)"));

    auto eddyCurrentAction = dataCorrMenu->addAction(tr("&Eddy Current Correction"));
    eddyCurrentAction->setEnabled(false);
    eddyCurrentAction->setToolTip(tr("Eddy current correction (not yet implemented)"));

    auto bgPhaseAction = dataCorrMenu->addAction(tr("&Background Phase Correction"));
    bgPhaseAction->setEnabled(false);
    bgPhaseAction->setToolTip(tr("Background phase correction (not yet implemented)"));

    dataCorrMenu->addSeparator();

    auto noiseReductionAction = dataCorrMenu->addAction(tr("&Noise Reduction"));
    noiseReductionAction->setEnabled(false);
    noiseReductionAction->setToolTip(tr("Noise reduction (not yet implemented)"));

    auto antiAliasingAction = dataCorrMenu->addAction(tr("&Anti-aliasing"));
    antiAliasingAction->setEnabled(false);
    antiAliasingAction->setToolTip(tr("Anti-aliasing filter (not yet implemented)"));

    // =========================================================================
    // Display menu — panel toggles, zoom, W/L, layout
    // =========================================================================
    auto displayMenu = menuBar()->addMenu(tr("&Display"));

    // Panel visibility submenu
    auto panelsMenu = displayMenu->addMenu(tr("&Panels"));

    impl_->togglePatientBrowserAction = panelsMenu->addAction(tr("&Patient Browser"));
    impl_->togglePatientBrowserAction->setCheckable(true);
    impl_->togglePatientBrowserAction->setChecked(true);
    impl_->togglePatientBrowserAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));

    impl_->toggleToolsPanelAction = panelsMenu->addAction(tr("&Tools Panel"));
    impl_->toggleToolsPanelAction->setCheckable(true);
    impl_->toggleToolsPanelAction->setChecked(true);
    impl_->toggleToolsPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));

    impl_->toggleStatisticsPanelAction = panelsMenu->addAction(tr("&Statistics Panel"));
    impl_->toggleStatisticsPanelAction->setCheckable(true);
    impl_->toggleStatisticsPanelAction->setChecked(false);
    impl_->toggleStatisticsPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));

    impl_->toggleSegmentationPanelAction = panelsMenu->addAction(tr("Se&gmentation Panel"));
    impl_->toggleSegmentationPanelAction->setCheckable(true);
    impl_->toggleSegmentationPanelAction->setChecked(false);
    impl_->toggleSegmentationPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));

    impl_->togglePhaseControlAction = panelsMenu->addAction(tr("&Phase Control"));
    impl_->togglePhaseControlAction->setCheckable(true);
    impl_->togglePhaseControlAction->setChecked(false);
    impl_->togglePhaseControlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));

    impl_->toggleOverlayControlAction = panelsMenu->addAction(tr("&Overlay Controls"));
    impl_->toggleOverlayControlAction->setCheckable(true);
    impl_->toggleOverlayControlAction->setChecked(false);
    impl_->toggleOverlayControlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_6));

    impl_->toggleFlowToolAction = panelsMenu->addAction(tr("&Flow Tools"));
    impl_->toggleFlowToolAction->setCheckable(true);
    impl_->toggleFlowToolAction->setChecked(false);
    impl_->toggleFlowToolAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_7));

    displayMenu->addSeparator();

    // Zoom and camera controls
    auto zoomInAction = displayMenu->addAction(tr("Zoom &In"));
    zoomInAction->setShortcut(QKeySequence::ZoomIn);

    auto zoomOutAction = displayMenu->addAction(tr("Zoom &Out"));
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);

    auto fitToWindowAction = displayMenu->addAction(tr("&Fit to Window"));
    fitToWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(fitToWindowAction, &QAction::triggered, this, [this]() {
        impl_->viewport->resetCamera();
    });

    auto resetWLAction = displayMenu->addAction(tr("Reset &Window/Level"));
    resetWLAction->setShortcut(QKeySequence(Qt::Key_Escape));

    displayMenu->addSeparator();

    // Window layout
    auto fullScreenAction = displayMenu->addAction(tr("F&ull Screen"));
    fullScreenAction->setShortcut(Qt::Key_F11);
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::triggered, this, &MainWindow::onToggleFullScreen);

    auto resetLayoutAction = displayMenu->addAction(tr("&Reset Layout"));
    resetLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetLayout);

    displayMenu->addSeparator();

    auto cascadeAction = displayMenu->addAction(tr("&Cascade Windows"));
    auto tileAction = displayMenu->addAction(tr("&Tile Windows"));

    // =========================================================================
    // Measure menu — distance, angle, ROI, quantification
    // =========================================================================
    auto measureMenu = menuBar()->addMenu(tr("&Measure"));

    impl_->distanceAction = measureMenu->addAction(tr("&Distance"));
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

    impl_->angleAction = measureMenu->addAction(tr("&Angle"));
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

    measureMenu->addSeparator();

    // ROI submenu for area measurements
    auto roiMenu = measureMenu->addMenu(tr("&ROI"));

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

    measureMenu->addSeparator();

    auto clearMeasurementsAction = measureMenu->addAction(tr("&Clear All Measurements"));
    clearMeasurementsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(clearMeasurementsAction, &QAction::triggered, this, [this]() {
        impl_->viewport->deleteAllMeasurements();
        impl_->statisticsPanel->clearStatistics();
        statusBar()->showMessage(tr("All measurements cleared"), 3000);
    });

    impl_->showStatisticsAction = measureMenu->addAction(tr("&Show ROI Statistics"));
    connect(impl_->showStatisticsAction, &QAction::triggered,
            this, &MainWindow::onShowRoiStatistics);

    measureMenu->addSeparator();

    auto quantificationAction = measureMenu->addAction(tr("&Quantification..."));
    quantificationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(quantificationAction, &QAction::triggered, this, [this]() {
        if (!impl_->quantificationWindow) {
            impl_->quantificationWindow = new QuantificationWindow(this);
            impl_->quantificationWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(impl_->quantificationWindow, &QObject::destroyed, this, [this]() {
                impl_->quantificationWindow = nullptr;
            });
            // Phase sync: graph click → viewport phase navigation
            connect(impl_->quantificationWindow, &QuantificationWindow::phaseChangeRequested,
                    this, [this](int phaseIndex) {
                if (!impl_->temporalNavigator.isInitialized()) return;
                (void)impl_->temporalNavigator.goToPhase(phaseIndex);
                impl_->phaseSlider->setCurrentPhase(phaseIndex);
                impl_->viewport->setPhaseIndex(phaseIndex);
            });
        }
        impl_->quantificationWindow->show();
        impl_->quantificationWindow->raise();
        impl_->quantificationWindow->activateWindow();
    });

    // =========================================================================
    // Export menu
    // =========================================================================
    auto exportMenu = menuBar()->addMenu(tr("E&xport"));

    auto saveScreenshotAction = exportMenu->addAction(tr("Save Sc&reenshot..."));
    saveScreenshotAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(saveScreenshotAction, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(
            this, tr("Save Screenshot"), QString(),
            tr("PNG Images (*.png);;JPEG Images (*.jpg)"));
        if (!filePath.isEmpty()) {
            impl_->viewport->captureScreenshot(filePath);
            statusBar()->showMessage(tr("Screenshot saved: %1").arg(filePath), 3000);
        }
    });

    exportMenu->addSeparator();

    auto exportEnsightAction = exportMenu->addAction(tr("Export &Ensight..."));
    exportEnsightAction->setEnabled(false);
    exportEnsightAction->setToolTip(tr("Export as Ensight format (not yet implemented)"));

    auto exportMatlabAction = exportMenu->addAction(tr("Export &MATLAB..."));
    exportMatlabAction->setEnabled(false);
    exportMatlabAction->setToolTip(tr("Export as MATLAB format (not yet implemented)"));

    auto exportDicomAction = exportMenu->addAction(tr("Export &DICOM..."));
    exportDicomAction->setEnabled(false);
    exportDicomAction->setToolTip(tr("Export as DICOM (not yet implemented)"));

    exportMenu->addSeparator();

    auto generateReportAction = exportMenu->addAction(tr("Generate &Report..."));
    generateReportAction->setEnabled(false);
    generateReportAction->setToolTip(tr("Generate analysis report (not yet implemented)"));

    // =========================================================================
    // Settings menu
    // =========================================================================
    auto settingsMenu = menuBar()->addMenu(tr("&Settings"));

    auto settingsAction = settingsMenu->addAction(tr("&Preferences..."));
    settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onShowSettings);

    // =========================================================================
    // Filter menu
    // =========================================================================
    auto filterMenu = menuBar()->addMenu(tr("F&ilter"));

    auto medianFilterAction = filterMenu->addAction(tr("&Median Filter"));
    medianFilterAction->setEnabled(false);
    medianFilterAction->setToolTip(tr("Apply median filter (not yet implemented)"));

    auto gaussianAction = filterMenu->addAction(tr("&Gaussian Smoothing"));
    gaussianAction->setEnabled(false);
    gaussianAction->setToolTip(tr("Apply Gaussian smoothing (not yet implemented)"));

    auto edgeEnhanceAction = filterMenu->addAction(tr("&Edge Enhancement"));
    edgeEnhanceAction->setEnabled(false);
    edgeEnhanceAction->setToolTip(tr("Apply edge enhancement (not yet implemented)"));

    filterMenu->addSeparator();

    auto velocityAliasingAction = filterMenu->addAction(tr("&Velocity Aliasing Correction"));
    velocityAliasingAction->setEnabled(false);
    velocityAliasingAction->setToolTip(tr("Correct velocity aliasing (not yet implemented)"));

    // =========================================================================
    // Misc menu — about, documentation
    // =========================================================================
    auto miscMenu = menuBar()->addMenu(tr("Mi&sc"));

    auto docsAction = miscMenu->addAction(tr("&Documentation"));
    docsAction->setShortcut(QKeySequence::HelpContents);

    miscMenu->addSeparator();

    auto aboutAction = miscMenu->addAction(tr("&About"));
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

    // Layout toggle buttons
    impl_->mainToolBar->addSeparator();

    auto* layoutGroup = new QActionGroup(this);
    layoutGroup->setExclusive(true);

    impl_->singleLayoutAction = impl_->mainToolBar->addAction(tr("1x1"));
    impl_->singleLayoutAction->setCheckable(true);
    impl_->singleLayoutAction->setChecked(true);
    impl_->singleLayoutAction->setToolTip(tr("Single viewport"));
    layoutGroup->addAction(impl_->singleLayoutAction);

    impl_->dualLayoutAction = impl_->mainToolBar->addAction(tr("1x2"));
    impl_->dualLayoutAction->setCheckable(true);
    impl_->dualLayoutAction->setToolTip(tr("Dual split: 2D | 3D"));
    layoutGroup->addAction(impl_->dualLayoutAction);

    impl_->quadLayoutAction = impl_->mainToolBar->addAction(tr("2x2"));
    impl_->quadLayoutAction->setCheckable(true);
    impl_->quadLayoutAction->setToolTip(tr("Quad split: Axial | Sagittal | Coronal | 3D"));
    layoutGroup->addAction(impl_->quadLayoutAction);

    connect(impl_->singleLayoutAction, &QAction::triggered, this, [this]() {
        impl_->layoutManager->setLayoutMode(LayoutMode::Single);
    });
    connect(impl_->dualLayoutAction, &QAction::triggered, this, [this]() {
        impl_->layoutManager->setLayoutMode(LayoutMode::DualSplit);
    });
    connect(impl_->quadLayoutAction, &QAction::triggered, this, [this]() {
        impl_->layoutManager->setLayoutMode(LayoutMode::QuadSplit);
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

    // Overlay Control Panel (right, tabbed with other panels)
    impl_->overlayControlDock = new QDockWidget(tr("Overlay Controls"), this);
    impl_->overlayControlDock->setObjectName("OverlayControlDock");
    impl_->overlayControlPanel = new OverlayControlPanel();
    impl_->overlayControlDock->setWidget(impl_->overlayControlPanel);
    impl_->overlayControlDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, impl_->overlayControlDock);
    tabifyDockWidget(impl_->segmentationPanelDock, impl_->overlayControlDock);
    impl_->overlayControlDock->hide();  // Initially hidden

    // Flow Tool Panel (left, below Patient Browser)
    impl_->flowToolDock = new QDockWidget(tr("Flow Tools"), this);
    impl_->flowToolDock->setObjectName("FlowToolDock");
    impl_->flowToolPanel = new FlowToolPanel();
    impl_->flowToolDock->setWidget(impl_->flowToolPanel);
    impl_->flowToolDock->setMinimumWidth(200);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->flowToolDock);
    impl_->flowToolDock->hide();  // Initially hidden until 4D data loaded
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

    connect(impl_->toggleOverlayControlAction, &QAction::toggled,
            impl_->overlayControlDock, &QDockWidget::setVisible);
    connect(impl_->overlayControlDock, &QDockWidget::visibilityChanged,
            impl_->toggleOverlayControlAction, &QAction::setChecked);

    connect(impl_->toggleFlowToolAction, &QAction::toggled,
            impl_->flowToolDock, &QDockWidget::setVisible);
    connect(impl_->flowToolDock, &QDockWidget::visibilityChanged,
            impl_->toggleFlowToolAction, &QAction::setChecked);

    // Flow tool panel series selection
    connect(impl_->flowToolPanel, &FlowToolPanel::seriesSelectionChanged,
            this, [this](FlowSeries series) {
        static const char* names[] = {"Magnitude", "RL", "AP", "FH", "PC-MRA"};
        impl_->statusLabel->setText(
            tr("Series: %1").arg(names[static_cast<int>(series)]));
    });

    // Flow tool panel Display 3D toggles → Display3DController
    connect(impl_->flowToolPanel, &FlowToolPanel::display3DToggled,
            this, [this](Display3DItem item, bool enabled) {
        impl_->display3DController->handleToggle(item, enabled);
    });

    // Flow tool panel Display 3D range changes → Display3DController
    connect(impl_->flowToolPanel, &FlowToolPanel::display3DRangeChanged,
            this, [this](Display3DItem item, double minVal, double maxVal) {
        impl_->display3DController->setScalarRange(item, minVal, maxVal);
    });

    // Patient browser -> Load series
    connect(impl_->patientBrowser, &PatientBrowser::seriesLoadRequested,
            this, [this](const QString& /*seriesUid*/, const QString& /*path*/) {
                impl_->statusLabel->setText(tr("Loading series..."));
                // TODO: Load series through controller
            });

    // Tools panel -> Active viewport (bidirectional W/L sync)
    impl_->toolsToActiveWlConn = connect(
        impl_->toolsPanel, &ToolsPanel::windowLevelChanged,
        impl_->viewport, &ViewportWidget::setWindowLevel);
    impl_->activeWlToToolsConn = connect(
        impl_->viewport, &ViewportWidget::windowLevelChanged,
        impl_->toolsPanel, &ToolsPanel::setWindowLevel);

    connect(impl_->toolsPanel, &ToolsPanel::presetSelected,
            impl_->viewport, &ViewportWidget::applyPreset);

    connect(impl_->toolsPanel, &ToolsPanel::visualizationModeChanged,
            this, [this](int mode) {
                impl_->viewport->setMode(static_cast<ViewportMode>(mode));
            });

    // Active viewport switching: reconnect ToolsPanel W/L to active viewport
    connect(impl_->layoutManager, &ViewportLayoutManager::activeViewportChanged,
            this, [this](ViewportWidget* vp, int /*index*/) {
        // Disconnect old connections
        QObject::disconnect(impl_->toolsToActiveWlConn);
        QObject::disconnect(impl_->activeWlToToolsConn);
        // Reconnect to the new active viewport
        impl_->toolsToActiveWlConn = connect(
            impl_->toolsPanel, &ToolsPanel::windowLevelChanged,
            vp, &ViewportWidget::setWindowLevel);
        impl_->activeWlToToolsConn = connect(
            vp, &ViewportWidget::windowLevelChanged,
            impl_->toolsPanel, &ToolsPanel::setWindowLevel);
    });

    // Install click-to-activate on all viewports via layout mode change
    connect(impl_->layoutManager, &ViewportLayoutManager::layoutModeChanged,
            this, [this](LayoutMode /*mode*/) {
        int count = impl_->layoutManager->viewportCount();
        for (int i = 0; i < count; ++i) {
            auto* vp = impl_->layoutManager->viewport(i);
            if (vp) {
                vp->installEventFilter(this);
            }
        }
    });

    // Install event filter on initial primary viewport
    impl_->viewport->installEventFilter(this);

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
    connect(impl_->segmentationPanel, &SegmentationPanel::undoCommandRequested,
            impl_->viewport, &ViewportWidget::undoSegmentationCommand);
    connect(impl_->segmentationPanel, &SegmentationPanel::redoCommandRequested,
            impl_->viewport, &ViewportWidget::redoSegmentationCommand);

    // Segmentation status updates
    connect(impl_->viewport, &ViewportWidget::segmentationModified,
            this, [this](int /*sliceIndex*/) {
                impl_->statusLabel->setText(tr("Segmentation modified"));
            });

    // Segmentation undo/redo availability → Edit menu + panel buttons
    connect(impl_->viewport, &ViewportWidget::segmentationUndoRedoChanged,
            this, [this](bool canUndo, bool canRedo) {
                impl_->undoAction->setEnabled(canUndo);
                impl_->redoAction->setEnabled(canRedo);
                impl_->segmentationPanel->setUndoRedoEnabled(canUndo, canRedo);
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
    settings.setValue("mainWindow/layoutMode",
                      static_cast<int>(impl_->layoutManager->layoutMode()));
}

void MainWindow::restoreLayout()
{
    QSettings settings("DicomViewer", "DicomViewer");
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/state").toByteArray());

    // Restore viewport layout mode
    auto mode = static_cast<LayoutMode>(
        settings.value("mainWindow/layoutMode", 0).toInt());
    impl_->layoutManager->setLayoutMode(mode);

    // Update toolbar button state
    switch (mode) {
        case LayoutMode::Single:
            impl_->singleLayoutAction->setChecked(true);
            break;
        case LayoutMode::DualSplit:
            impl_->dualLayoutAction->setChecked(true);
            break;
        case LayoutMode::QuadSplit:
            impl_->quadLayoutAction->setChecked(true);
            break;
    }
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
    if (!promptSaveIfModified()) {
        event->ignore();
        return;
    }
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

    if (dir.isEmpty()) return;

    impl_->statusLabel->setText(tr("Scanning %1...").arg(dir));
    QApplication::processEvents();

    // 1. Scan directory for DICOM series
    core::SeriesBuilder builder;
    auto scanResult = builder.scanForSeries(
        std::filesystem::path(dir.toStdString()));
    if (!scanResult) {
        QMessageBox::warning(this, tr("DICOM Import Error"),
            tr("Failed to scan directory:\n%1")
                .arg(QString::fromStdString(scanResult.error().message)));
        impl_->statusLabel->setText(tr("Ready"));
        return;
    }

    auto& scannedSeries = *scanResult;
    if (scannedSeries.empty()) {
        impl_->statusLabel->setText(tr("No DICOM series found in %1").arg(dir));
        return;
    }

    // 2. Classify all series
    auto classifications =
        services::SeriesClassifier::classifyScannedSeries(scannedSeries);

    // 3. Read metadata and populate PatientBrowser
    impl_->patientBrowser->clear();
    core::DicomLoader loader;

    std::map<std::string, bool> addedPatients;
    std::map<std::string, bool> addedStudies;

    for (size_t i = 0; i < scannedSeries.size(); ++i) {
        const auto& series = scannedSeries[i];
        if (series.slices.empty()) continue;

        auto metaResult = loader.loadFile(series.slices[0].filePath);
        if (!metaResult) continue;

        const auto& meta = *metaResult;

        // Add patient (once per unique patient ID)
        if (!addedPatients.contains(meta.patientId)) {
            PatientInfo pi;
            pi.patientId = QString::fromStdString(meta.patientId);
            pi.patientName = QString::fromStdString(meta.patientName);
            pi.birthDate = QString::fromStdString(meta.patientBirthDate);
            pi.sex = QString::fromStdString(meta.patientSex);
            impl_->patientBrowser->addPatient(pi);
            addedPatients[meta.patientId] = true;
        }

        // Add study (once per unique study UID)
        if (!addedStudies.contains(meta.studyInstanceUid)) {
            StudyInfo si;
            si.studyInstanceUid =
                QString::fromStdString(meta.studyInstanceUid);
            si.studyDate = QString::fromStdString(meta.studyDate);
            si.studyDescription =
                QString::fromStdString(meta.studyDescription);
            si.accessionNumber =
                QString::fromStdString(meta.accessionNumber);
            si.modality = QString::fromStdString(meta.modality);
            impl_->patientBrowser->addStudy(
                QString::fromStdString(meta.patientId), si);
            addedStudies[meta.studyInstanceUid] = true;
        }

        // Add series with classification
        SeriesInfo uiSeries;
        uiSeries.seriesInstanceUid =
            QString::fromStdString(series.seriesInstanceUid);
        uiSeries.seriesNumber =
            QString::fromStdString(meta.seriesNumber);
        uiSeries.seriesDescription =
            QString::fromStdString(series.seriesDescription);
        uiSeries.modality = QString::fromStdString(series.modality);
        uiSeries.numberOfImages = static_cast<int>(series.sliceCount);

        if (i < classifications.size()) {
            uiSeries.seriesType = QString::fromStdString(
                services::seriesToString(classifications[i].type));
            uiSeries.is4DFlow = classifications[i].is4DFlow;
        }

        impl_->patientBrowser->addSeries(
            QString::fromStdString(meta.studyInstanceUid), uiSeries);
    }

    impl_->patientBrowser->expandAll();
    impl_->statusLabel->setText(
        tr("Loaded %1 series from %2")
            .arg(scannedSeries.size())
            .arg(dir));
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
    impl_->overlayControlDock->setFloating(false);
    impl_->flowToolDock->setFloating(false);
    impl_->phaseControlDock->setFloating(false);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->patientBrowserDock);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->flowToolDock);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->phaseControlDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolsPanelDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->statisticsPanelDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->segmentationPanelDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->overlayControlDock);
    tabifyDockWidget(impl_->toolsPanelDock, impl_->statisticsPanelDock);
    tabifyDockWidget(impl_->statisticsPanelDock, impl_->segmentationPanelDock);
    tabifyDockWidget(impl_->segmentationPanelDock, impl_->overlayControlDock);
    impl_->patientBrowserDock->show();
    impl_->toolsPanelDock->show();
    impl_->toolsPanelDock->raise();  // Show Tools tab
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        // Check if clicked widget is a viewport or child of one
        auto* widget = qobject_cast<QWidget*>(watched);
        int count = impl_->layoutManager->viewportCount();
        for (int i = 0; i < count; ++i) {
            auto* vp = impl_->layoutManager->viewport(i);
            if (vp && (widget == vp || vp->isAncestorOf(widget))) {
                impl_->layoutManager->setActiveViewport(i);
                break;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
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

// =============================================================================
// Project management
// =============================================================================

void MainWindow::onNewProject()
{
    if (!promptSaveIfModified()) return;

    impl_->projectManager->newProject();
    updateWindowTitle();
    statusBar()->showMessage(tr("New project created"), 3000);
}

void MainWindow::onSaveProject()
{
    auto path = impl_->projectManager->currentPath();
    if (path.empty()) {
        onSaveProjectAs();
        return;
    }

    auto result = impl_->projectManager->saveProject(path);
    if (result) {
        impl_->projectManager->addToRecent(path);
        updateRecentProjectsMenu();
        updateWindowTitle();
        statusBar()->showMessage(tr("Project saved: %1").arg(QString::fromStdString(path.string())), 3000);
    } else {
        QMessageBox::warning(this, tr("Save Failed"),
            tr("Could not save project to:\n%1").arg(QString::fromStdString(path.string())));
    }
}

void MainWindow::onSaveProjectAs()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Project As"),
        QString::fromStdString(impl_->projectManager->currentPath().string()),
        tr("Flow Project (*.flo)"));

    if (filePath.isEmpty()) return;

    auto path = std::filesystem::path(filePath.toStdString());
    auto result = impl_->projectManager->saveProject(path);
    if (result) {
        impl_->projectManager->addToRecent(path);
        updateRecentProjectsMenu();
        updateWindowTitle();
        statusBar()->showMessage(tr("Project saved: %1").arg(filePath), 3000);
    } else {
        QMessageBox::warning(this, tr("Save Failed"),
            tr("Could not save project to:\n%1").arg(filePath));
    }
}

void MainWindow::onOpenProject()
{
    if (!promptSaveIfModified()) return;

    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Project"), QString(),
        tr("Flow Project (*.flo);;All Files (*)"));

    if (filePath.isEmpty()) return;

    auto path = std::filesystem::path(filePath.toStdString());
    auto result = impl_->projectManager->loadProject(path);
    if (result) {
        impl_->projectManager->addToRecent(path);
        updateRecentProjectsMenu();
        updateWindowTitle();
        statusBar()->showMessage(tr("Project loaded: %1").arg(filePath), 3000);
    } else {
        QMessageBox::warning(this, tr("Open Failed"),
            tr("Could not open project:\n%1").arg(filePath));
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = "DICOM Viewer";
    auto name = impl_->projectManager->projectName();
    if (!name.empty()) {
        title = QString::fromStdString(name) + " - " + title;
    }
    if (impl_->projectManager->isModified()) {
        title = "* " + title;
    }
    setWindowTitle(title);
}

void MainWindow::updateRecentProjectsMenu()
{
    impl_->recentProjectsMenu->clear();

    auto recents = impl_->projectManager->recentProjects();
    if (recents.empty()) {
        auto* emptyAction = impl_->recentProjectsMenu->addAction(tr("(No recent projects)"));
        emptyAction->setEnabled(false);
        return;
    }

    for (const auto& recent : recents) {
        auto displayName = recent.name.empty()
            ? QString::fromStdString(recent.path.filename().string())
            : QString::fromStdString(recent.name);
        auto* action = impl_->recentProjectsMenu->addAction(displayName);
        auto recentPath = recent.path;
        connect(action, &QAction::triggered, this, [this, recentPath]() {
            if (!promptSaveIfModified()) return;

            auto result = impl_->projectManager->loadProject(recentPath);
            if (result) {
                impl_->projectManager->addToRecent(recentPath);
                updateRecentProjectsMenu();
                updateWindowTitle();
                statusBar()->showMessage(
                    tr("Project loaded: %1").arg(QString::fromStdString(recentPath.string())), 3000);
            } else {
                QMessageBox::warning(this, tr("Open Failed"),
                    tr("Could not open project:\n%1").arg(QString::fromStdString(recentPath.string())));
            }
        });
    }

    impl_->recentProjectsMenu->addSeparator();
    auto* clearAction = impl_->recentProjectsMenu->addAction(tr("Clear Recent"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        impl_->projectManager->clearRecentProjects();
        updateRecentProjectsMenu();
    });
}

bool MainWindow::promptSaveIfModified()
{
    if (!impl_->projectManager->isModified()) return true;

    auto result = QMessageBox::question(this, tr("Unsaved Changes"),
        tr("The current project has unsaved changes.\nDo you want to save before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    switch (result) {
    case QMessageBox::Save:
        onSaveProject();
        return !impl_->projectManager->isModified();
    case QMessageBox::Discard:
        return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

} // namespace dicom_viewer::ui

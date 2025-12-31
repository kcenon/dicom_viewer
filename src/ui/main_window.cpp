#include "ui/main_window.hpp"
#include "ui/viewport_widget.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>

namespace dicom_viewer::ui {

class MainWindow::Impl {
public:
    ViewportWidget* viewport = nullptr;
    QDockWidget* patientBrowserDock = nullptr;
    QDockWidget* toolsPanelDock = nullptr;
    QTreeWidget* patientBrowser = nullptr;
    QToolBar* mainToolBar = nullptr;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , impl_(std::make_unique<Impl>())
{
    setWindowTitle("DICOM Viewer");
    setMinimumSize(1280, 720);

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
    fileMenu->addAction(tr("Open &Directory..."), this, &MainWindow::onOpenDirectory,
                        QKeySequence::Open);
    fileMenu->addAction(tr("Open &File..."), this, &MainWindow::onOpenFile);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Connect to &PACS..."), this, &MainWindow::onConnectPACS);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), qApp, &QApplication::quit, QKeySequence::Quit);

    // Edit menu
    auto editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Undo"), this, [](){}, QKeySequence::Undo);
    editMenu->addAction(tr("&Redo"), this, [](){}, QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction(tr("&Settings..."), this, &MainWindow::onShowSettings);

    // View menu
    auto viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("&Full Screen"), this, &MainWindow::onToggleFullScreen, Qt::Key_F11);
    viewMenu->addAction(tr("&Reset Layout"), this, &MainWindow::onResetLayout);

    // Tools menu
    auto toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("&Distance"));
    toolsMenu->addAction(tr("&Angle"));
    toolsMenu->addAction(tr("&ROI"));

    // Help menu
    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, &MainWindow::onShowAbout);
}

void MainWindow::setupToolBar()
{
    impl_->mainToolBar = addToolBar(tr("Main"));
    impl_->mainToolBar->setMovable(false);

    impl_->mainToolBar->addAction(tr("Open"));
    impl_->mainToolBar->addSeparator();
    impl_->mainToolBar->addAction(tr("Scroll"));
    impl_->mainToolBar->addAction(tr("Zoom"));
    impl_->mainToolBar->addAction(tr("Pan"));
    impl_->mainToolBar->addAction(tr("W/L"));
    impl_->mainToolBar->addSeparator();
    impl_->mainToolBar->addAction(tr("Measure"));
}

void MainWindow::setupDockWidgets()
{
    // Patient Browser (left)
    impl_->patientBrowserDock = new QDockWidget(tr("Patient Browser"), this);
    impl_->patientBrowser = new QTreeWidget();
    impl_->patientBrowser->setHeaderLabels({tr("Name"), tr("ID"), tr("Date")});
    impl_->patientBrowserDock->setWidget(impl_->patientBrowser);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->patientBrowserDock);

    // Tools Panel (right)
    impl_->toolsPanelDock = new QDockWidget(tr("Tools"), this);
    auto toolsWidget = new QWidget();
    impl_->toolsPanelDock->setWidget(toolsWidget);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolsPanelDock);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupConnections()
{
    // Connect signals and slots
}

void MainWindow::applyDarkTheme()
{
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    qApp->setPalette(darkPalette);
}

void MainWindow::saveLayout()
{
    QSettings settings;
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/state", saveState());
}

void MainWindow::restoreLayout()
{
    QSettings settings;
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/state").toByteArray());
}

void MainWindow::registerShortcuts()
{
    // Additional shortcuts registered here
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
        statusBar()->showMessage(tr("Loading %1...").arg(dir));
        // TODO: Load DICOM series
    }
}

void MainWindow::onOpenFile()
{
    QString file = QFileDialog::getOpenFileName(
        this, tr("Open DICOM File"), QString(),
        tr("DICOM Files (*.dcm);;All Files (*)"));

    if (!file.isEmpty()) {
        statusBar()->showMessage(tr("Loading %1...").arg(file));
        // TODO: Load DICOM file
    }
}

void MainWindow::onConnectPACS()
{
    QMessageBox::information(this, tr("PACS"), tr("PACS connection not yet implemented."));
}

void MainWindow::onShowSettings()
{
    QMessageBox::information(this, tr("Settings"), tr("Settings dialog not yet implemented."));
}

void MainWindow::onShowAbout()
{
    QMessageBox::about(this, tr("About DICOM Viewer"),
        tr("DICOM Viewer v0.3.0\n\n"
           "High-performance medical image viewer\n"
           "with 3D Volume Rendering and MPR views.\n\n"
           "© 2025 kcenon"));
}

void MainWindow::onResetLayout()
{
    // Reset to default layout
    impl_->patientBrowserDock->setFloating(false);
    impl_->toolsPanelDock->setFloating(false);
    addDockWidget(Qt::LeftDockWidgetArea, impl_->patientBrowserDock);
    addDockWidget(Qt::RightDockWidgetArea, impl_->toolsPanelDock);
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

#include "ui/main_window.hpp"

#include <QApplication>
#include <QSurfaceFormat>
#include <QStyleFactory>

#include <vtkOpenGLRenderWindow.h>

/**
 * @brief Application entry point
 *
 * Initializes Qt, VTK, and launches the main window.
 */
int main(int argc, char* argv[])
{
    // VTK OpenGL settings (must be before QApplication)
    vtkOpenGLRenderWindow::SetGlobalMaximumNumberOfMultiSamples(0);

    // Qt OpenGL settings
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    // Create application
    QApplication app(argc, argv);
    app.setApplicationName("DICOM Viewer");
    app.setApplicationVersion("0.3.0");
    app.setOrganizationName("kcenon");
    app.setOrganizationDomain("github.com/kcenon");

    // Apply Fusion style (works well with dark theme)
    app.setStyle(QStyleFactory::create("Fusion"));

    // Create and show main window
    dicom_viewer::ui::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

// Ecosystem logger headers must precede Qt to avoid emit() macro conflict
#include <kcenon/common/interfaces/global_logger_registry.h>
#include <kcenon/logger/core/logger_builder.h>
#include <kcenon/logger/writers/console_writer.h>

#include "ui/main_window.hpp"
#include "core/app_log_level.hpp"

#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QStyleFactory>

#include <vtkOpenGLRenderWindow.h>

namespace {

/**
 * @brief Initialize ecosystem logger with persisted settings.
 *
 * Creates a logger via logger_builder, registers it in GlobalLoggerRegistry,
 * and sets the log level from QSettings.
 */
void initializeLogging()
{
    QSettings settings;
    const int levelValue = settings.value("logging/level", 2).toInt();
    const auto appLevel = dicom_viewer::from_settings_value(levelValue);
    const auto ecoLevel = dicom_viewer::to_ecosystem_level(appLevel);

    const QString logDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/logs";

    auto result = kcenon::logger::logger_builder()
        .with_min_level(static_cast<kcenon::logger::log_level>(ecoLevel))
        .add_writer("console", std::make_unique<kcenon::logger::console_writer>())
        .with_file_output(logDir.toStdString(), "dicom_viewer")
        .build();

    if (result) {
        auto logger = std::shared_ptr<kcenon::logger::logger>(result.value().release());
        auto& registry = kcenon::common::interfaces::GlobalLoggerRegistry::instance();
        registry.set_default_logger(logger);
    }
}

} // anonymous namespace

/**
 * @brief Application entry point
 *
 * Initializes Qt, VTK, logging, and launches the main window.
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

    // Initialize ecosystem logger with persisted settings
    initializeLogging();

    // Apply Fusion style (works well with dark theme)
    app.setStyle(QStyleFactory::create("Fusion"));

    // Create and show main window
    dicom_viewer::ui::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

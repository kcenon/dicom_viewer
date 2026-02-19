#pragma once

#include "services/export/video_exporter.hpp"

#include <memory>
#include <QDialog>

namespace dicom_viewer::ui {

/**
 * @brief Configuration dialog for video export
 *
 * Allows the user to select export mode (2D Cine, 3D Rotation, Combined),
 * set resolution, FPS, and mode-specific parameters. Generates the
 * appropriate VideoExporter config struct for the selected mode.
 *
 * @trace SRS-FR-046
 */
class VideoExportDialog : public QDialog {
    Q_OBJECT

public:
    /// Export mode selection
    enum class ExportMode { Cine2D, Rotation3D, Combined };

    /**
     * @brief Construct the dialog
     * @param totalPhases Total cardiac phases available (0 = no temporal data)
     * @param parent Parent widget
     */
    explicit VideoExportDialog(int totalPhases, QWidget* parent = nullptr);
    ~VideoExportDialog() override;

    // Non-copyable
    VideoExportDialog(const VideoExportDialog&) = delete;
    VideoExportDialog& operator=(const VideoExportDialog&) = delete;

    /// Get the selected export mode
    [[nodiscard]] ExportMode exportMode() const;

    /// Get the configured output file path
    [[nodiscard]] std::filesystem::path outputPath() const;

    /// Build CineConfig from dialog settings (valid when mode == Cine2D)
    [[nodiscard]] services::VideoExporter::CineConfig buildCineConfig() const;

    /// Build RotationConfig from dialog settings (valid when mode == Rotation3D)
    [[nodiscard]] services::VideoExporter::RotationConfig buildRotationConfig() const;

    /// Build CombinedConfig from dialog settings (valid when mode == Combined)
    [[nodiscard]] services::VideoExporter::CombinedConfig buildCombinedConfig() const;

private slots:
    void onModeChanged(int index);
    void onBrowseOutput();

private:
    void setupUI();
    void setupConnections();
    void updateModeOptions();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

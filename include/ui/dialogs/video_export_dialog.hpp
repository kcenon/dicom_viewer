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

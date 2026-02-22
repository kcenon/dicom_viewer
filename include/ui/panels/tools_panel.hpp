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
 * @file tools_panel.hpp
 * @brief Tools panel providing context-sensitive tool options
 * @details Displays tool settings based on selected tool category (Navigation,
 *          Measurement, Annotation, Visualization). Provides quick
 *          access to window/level presets and visualization modes.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QWidget-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Tool categories for the tools panel
 */
enum class ToolCategory {
    Navigation,     // Scroll, Zoom, Pan, W/L
    Measurement,    // Distance, Angle, ROI
    Annotation,     // Text, Arrow, Freehand
    Visualization   // Presets, 3D modes
};

/**
 * @brief Tools panel providing context-sensitive tool options
 *
 * Displays tool settings and options based on the currently selected tool.
 * Provides quick access to window/level presets and visualization modes.
 *
 * @trace SRS-FR-039, PRD FR-011.4
 */
class ToolsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ToolsPanel(QWidget* parent = nullptr);
    ~ToolsPanel() override;

    // Non-copyable
    ToolsPanel(const ToolsPanel&) = delete;
    ToolsPanel& operator=(const ToolsPanel&) = delete;

    /**
     * @brief Set the current tool category to display options for
     * @param category Tool category
     */
    void setToolCategory(ToolCategory category);

    /**
     * @brief Set window/level values (updates sliders)
     * @param width Window width
     * @param center Window center
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Get current window width
     */
    double windowWidth() const;

    /**
     * @brief Get current window center
     */
    double windowCenter() const;

signals:
    /**
     * @brief Emitted when window/level changes
     */
    void windowLevelChanged(double width, double center);

    /**
     * @brief Emitted when a preset is selected
     */
    void presetSelected(const QString& presetName);

    /**
     * @brief Emitted when visualization mode changes
     */
    void visualizationModeChanged(int mode);

    /**
     * @brief Emitted when slice changes
     */
    void sliceChanged(int slice);

private slots:
    void onWindowSliderChanged(int value);
    void onLevelSliderChanged(int value);
    void onPresetButtonClicked();

private:
    void setupUI();
    void setupConnections();
    void createNavigationSection();
    void createWindowLevelSection();
    void createPresetSection();
    void createVisualizationSection();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

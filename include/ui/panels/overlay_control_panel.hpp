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
 * @file overlay_control_panel.hpp
 * @brief UI panel for controlling 2D hemodynamic overlay display
 * @details Provides checkboxes for each overlay type with per-overlay
 *          colormap range controls and opacity sliders. Emits
 *          signals on setting changes.
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

#include "services/render/hemodynamic_overlay_renderer.hpp"

namespace dicom_viewer::ui {

/**
 * @brief UI panel for controlling 2D hemodynamic overlay display
 *
 * Provides Display 2D checkboxes for each overlay type, per-overlay
 * colormap range controls, and opacity sliders. Emits signals when
 * overlay settings change.
 *
 * @trace SRS-FR-046, PRD FR-015
 */
class OverlayControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit OverlayControlPanel(QWidget* parent = nullptr);
    ~OverlayControlPanel() override;

    // Non-copyable
    OverlayControlPanel(const OverlayControlPanel&) = delete;
    OverlayControlPanel& operator=(const OverlayControlPanel&) = delete;

    /**
     * @brief Set panel enabled state based on data availability
     */
    void setOverlaysAvailable(bool available);

    /**
     * @brief Check if a specific overlay type is enabled (checked)
     */
    [[nodiscard]] bool isOverlayEnabled(services::OverlayType type) const;

    /**
     * @brief Get opacity for a specific overlay type (0.0 - 1.0)
     */
    [[nodiscard]] double overlayOpacity(services::OverlayType type) const;

    /**
     * @brief Get scalar range for a specific overlay type
     */
    [[nodiscard]] std::pair<double, double>
    overlayScalarRange(services::OverlayType type) const;

    /**
     * @brief Reset all controls to default state
     */
    void resetToDefaults();

signals:
    /**
     * @brief Emitted when an overlay visibility checkbox changes
     * @param type Overlay type
     * @param visible New visibility state
     */
    void overlayVisibilityChanged(services::OverlayType type, bool visible);

    /**
     * @brief Emitted when overlay opacity changes
     * @param type Overlay type
     * @param opacity New opacity (0.0 - 1.0)
     */
    void overlayOpacityChanged(services::OverlayType type, double opacity);

    /**
     * @brief Emitted when overlay scalar range changes
     * @param type Overlay type
     * @param minVal Minimum scalar value
     * @param maxVal Maximum scalar value
     */
    void overlayScalarRangeChanged(services::OverlayType type,
                                   double minVal, double maxVal);

private slots:
    void onCheckboxToggled(int typeIndex, bool checked);
    void onOpacitySliderChanged(int typeIndex, int value);
    void onRangeMinChanged(int typeIndex, double value);
    void onRangeMaxChanged(int typeIndex, double value);

private:
    void setupUI();
    void setupConnections();
    void createOverlayGroup(services::OverlayType type,
                            const QString& label,
                            double defaultMin,
                            double defaultMax);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

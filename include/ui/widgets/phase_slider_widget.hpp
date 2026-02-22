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

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

enum class ScrollMode;

/**
 * @brief Widget for cardiac phase navigation with cine playback
 *
 * Provides a slider, spinbox, and play/stop controls for navigating
 * through cardiac phases in 4D Flow MRI data. Designed to connect
 * to TemporalNavigator via MainWindow signals/slots wiring.
 *
 * Layout:
 *   Phase: [Play/Stop] ═══════○═══ [15/25]
 *
 * @trace SRS-FR-048
 */
class PhaseSliderWidget : public QWidget {
    Q_OBJECT

public:
    explicit PhaseSliderWidget(QWidget* parent = nullptr);
    ~PhaseSliderWidget() override;

    // Non-copyable
    PhaseSliderWidget(const PhaseSliderWidget&) = delete;
    PhaseSliderWidget& operator=(const PhaseSliderWidget&) = delete;

    /**
     * @brief Get current phase index
     */
    [[nodiscard]] int currentPhase() const;

    /**
     * @brief Check if cine playback is active
     */
    [[nodiscard]] bool isPlaying() const;

    /**
     * @brief Get current scroll mode (Slice or Phase)
     */
    [[nodiscard]] ScrollMode scrollMode() const;

    /**
     * @brief Get current FPS setting
     */
    [[nodiscard]] int fps() const;

public slots:
    /**
     * @brief Set the phase range (0 to max)
     * @param phaseCount Total number of cardiac phases
     */
    void setPhaseCount(int phaseCount);

    /**
     * @brief Update the displayed phase index
     *
     * Called externally (e.g., from TemporalNavigator callback)
     * to synchronize the slider position without re-emitting
     * phaseChangeRequested.
     *
     * @param phase 0-based phase index
     */
    void setCurrentPhase(int phase);

    /**
     * @brief Update playback state indicator
     * @param playing True if cine is playing
     */
    void setPlaying(bool playing);

    /**
     * @brief Enable or disable all controls
     * @param enabled True to enable
     */
    void setControlsEnabled(bool enabled);

    /**
     * @brief Set the scroll mode programmatically
     * @param mode Slice or Phase mode
     */
    void setScrollMode(ScrollMode mode);

    /**
     * @brief Set FPS for cine playback
     * @param fps Frames per second (1-60)
     */
    void setFps(int fps);

signals:
    /**
     * @brief User requested phase change via slider or spinbox
     * @param phaseIndex Requested phase index
     */
    void phaseChangeRequested(int phaseIndex);

    /**
     * @brief User clicked Play button
     */
    void playRequested();

    /**
     * @brief User clicked Stop button
     */
    void stopRequested();

    /**
     * @brief S/P mode changed by user
     * @param mode New scroll mode
     */
    void scrollModeChanged(ScrollMode mode);

    /**
     * @brief FPS changed by user
     * @param fps New frames per second value
     */
    void fpsChanged(int fps);

private:
    void setupUI();
    void setupConnections();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

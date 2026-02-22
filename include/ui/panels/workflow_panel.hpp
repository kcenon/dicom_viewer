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
 * @file workflow_panel.hpp
 * @brief Container panel that organizes tools by workflow stage
 * @details Combines WorkflowTabBar with QStackedWidget to present
 *          stage-appropriate tools: Preprocessing, Segmentation,
 *          Analysis, and Report.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QWidget-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include "ui/widgets/workflow_tab_bar.hpp"

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

class ToolsPanel;

/**
 * @brief Container panel that organizes tools by workflow stage
 *
 * Combines a WorkflowTabBar with a QStackedWidget to present
 * stage-appropriate tools. The Analysis page wraps the existing
 * ToolsPanel; other pages are initially placeholders.
 *
 * @trace SRS-FR-039
 */
class WorkflowPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a WorkflowPanel
     * @param analysisPanel Existing ToolsPanel to embed as the Analysis page
     *                      (ownership transferred to this widget)
     * @param parent Parent widget
     */
    explicit WorkflowPanel(ToolsPanel* analysisPanel,
                           QWidget* parent = nullptr);
    ~WorkflowPanel() override;

    // Non-copyable
    WorkflowPanel(const WorkflowPanel&) = delete;
    WorkflowPanel& operator=(const WorkflowPanel&) = delete;

    /// Get the tab bar for external shortcut wiring
    [[nodiscard]] WorkflowTabBar* tabBar() const;

    /// Get the current workflow stage
    [[nodiscard]] WorkflowStage currentStage() const;

    /// Set the active workflow stage
    void setCurrentStage(WorkflowStage stage);

    /**
     * @brief Set the widget for a specific workflow stage
     * @param stage Target stage
     * @param widget Widget to display (ownership transferred)
     *
     * Replaces the placeholder for the given stage with a real panel.
     */
    void setStageWidget(WorkflowStage stage, QWidget* widget);

signals:
    /// Forwarded from WorkflowTabBar
    void stageChanged(WorkflowStage stage);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

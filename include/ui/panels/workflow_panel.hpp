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

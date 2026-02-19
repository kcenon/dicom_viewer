#pragma once

#include <QTabBar>

namespace dicom_viewer::ui {

/**
 * @brief Workflow stage for the analysis pipeline
 */
enum class WorkflowStage {
    Preprocessing,  ///< Data correction (registration, phase correction)
    Segmentation,   ///< Mask creation (brush, wizard, boolean ops)
    Analysis,       ///< Measurement and visualization controls
    Report          ///< Export and report generation
};

/**
 * @brief Tab bar for switching between workflow stages
 *
 * Provides 4 tabs representing the sequential analysis workflow:
 * Preprocessing -> Segmentation -> Analysis -> Report.
 * Each tab reconfigures the tool panel to show stage-relevant tools.
 *
 * @trace SRS-FR-039
 */
class WorkflowTabBar : public QTabBar {
    Q_OBJECT

public:
    explicit WorkflowTabBar(QWidget* parent = nullptr);

    /// Get the currently selected workflow stage
    [[nodiscard]] WorkflowStage currentStage() const;

    /// Set the active workflow stage
    void setCurrentStage(WorkflowStage stage);

signals:
    /// Emitted when the user switches workflow stages
    void stageChanged(WorkflowStage stage);

private:
    void setupTabs();
};

} // namespace dicom_viewer::ui

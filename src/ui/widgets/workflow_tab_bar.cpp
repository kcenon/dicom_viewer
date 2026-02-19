#include "ui/widgets/workflow_tab_bar.hpp"

namespace dicom_viewer::ui {

WorkflowTabBar::WorkflowTabBar(QWidget* parent)
    : QTabBar(parent)
{
    setupTabs();

    connect(this, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0 && index <= 3) {
            emit stageChanged(static_cast<WorkflowStage>(index));
        }
    });
}

void WorkflowTabBar::setupTabs()
{
    addTab(tr("Preprocessing"));
    addTab(tr("Segmentation"));
    addTab(tr("Analysis"));
    addTab(tr("Report"));

    // Default to Analysis tab (matches current tool panel behavior)
    setCurrentIndex(static_cast<int>(WorkflowStage::Analysis));
}

WorkflowStage WorkflowTabBar::currentStage() const
{
    return static_cast<WorkflowStage>(currentIndex());
}

void WorkflowTabBar::setCurrentStage(WorkflowStage stage)
{
    setCurrentIndex(static_cast<int>(stage));
}

} // namespace dicom_viewer::ui

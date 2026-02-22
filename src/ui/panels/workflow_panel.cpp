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

#include "ui/panels/workflow_panel.hpp"
#include "ui/panels/tools_panel.hpp"

#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

// =========================================================================
// Pimpl
// =========================================================================

class WorkflowPanel::Impl {
public:
    WorkflowTabBar* tabBar = nullptr;
    QStackedWidget* stack = nullptr;

    // Indexed by WorkflowStage ordinal
    QWidget* pages[4] = {};
};

// =========================================================================
// Construction
// =========================================================================

WorkflowPanel::WorkflowPanel(ToolsPanel* analysisPanel, QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Tab bar
    impl_->tabBar = new WorkflowTabBar(this);
    layout->addWidget(impl_->tabBar);

    // Stacked widget for page content
    impl_->stack = new QStackedWidget(this);
    layout->addWidget(impl_->stack, 1);

    // Preprocessing placeholder
    auto* preprocessingPage = new QLabel(tr("Preprocessing tools\n\n"
        "Registration, Phase correction,\n"
        "Anti-aliasing, Offset correction\n\n"
        "(Coming in a future update)"));
    preprocessingPage->setAlignment(Qt::AlignCenter);
    preprocessingPage->setWordWrap(true);
    impl_->stack->addWidget(preprocessingPage);
    impl_->pages[0] = preprocessingPage;

    // Segmentation placeholder
    auto* segmentationPage = new QLabel(tr("Segmentation tools\n\n"
        "Mask Wizard, Brush tools,\n"
        "Boolean operations, Import/Export\n\n"
        "(Coming in a future update)"));
    segmentationPage->setAlignment(Qt::AlignCenter);
    segmentationPage->setWordWrap(true);
    impl_->stack->addWidget(segmentationPage);
    impl_->pages[1] = segmentationPage;

    // Analysis page = existing ToolsPanel
    impl_->stack->addWidget(analysisPanel);
    impl_->pages[2] = analysisPanel;

    // Report placeholder
    auto* reportPage = new QLabel(tr("Report tools\n\n"
        "Export formats, Screenshot capture,\n"
        "Video export, Report templates\n\n"
        "(Coming in a future update)"));
    reportPage->setAlignment(Qt::AlignCenter);
    reportPage->setWordWrap(true);
    impl_->stack->addWidget(reportPage);
    impl_->pages[3] = reportPage;

    // Default to Analysis tab
    impl_->stack->setCurrentIndex(
        static_cast<int>(WorkflowStage::Analysis));

    // Wire tab changes to stack
    connect(impl_->tabBar, &WorkflowTabBar::stageChanged,
            this, [this](WorkflowStage stage) {
                impl_->stack->setCurrentIndex(static_cast<int>(stage));
                emit stageChanged(stage);
            });
}

WorkflowPanel::~WorkflowPanel() = default;

// =========================================================================
// Accessors
// =========================================================================

WorkflowTabBar* WorkflowPanel::tabBar() const
{
    return impl_->tabBar;
}

WorkflowStage WorkflowPanel::currentStage() const
{
    return impl_->tabBar->currentStage();
}

void WorkflowPanel::setCurrentStage(WorkflowStage stage)
{
    impl_->tabBar->setCurrentStage(stage);
}

void WorkflowPanel::setStageWidget(WorkflowStage stage, QWidget* widget)
{
    int index = static_cast<int>(stage);
    if (index < 0 || index > 3 || !widget) return;

    // Remove old widget from stack
    QWidget* old = impl_->pages[index];
    if (old) {
        impl_->stack->removeWidget(old);
        old->deleteLater();
    }

    // Insert new widget at the correct position
    impl_->stack->insertWidget(index, widget);
    impl_->pages[index] = widget;

    // If this stage is currently selected, show the new widget
    if (impl_->tabBar->currentStage() == stage) {
        impl_->stack->setCurrentIndex(index);
    }
}

} // namespace dicom_viewer::ui

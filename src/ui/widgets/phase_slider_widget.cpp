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

#include "ui/widgets/phase_slider_widget.hpp"
#include "ui/widgets/sp_mode_toggle.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

class PhaseSliderWidget::Impl {
public:
    QSlider* slider = nullptr;
    QSpinBox* spinBox = nullptr;
    QSpinBox* fpsSpinBox = nullptr;
    QPushButton* playStopButton = nullptr;
    QLabel* titleLabel = nullptr;
    SPModeToggle* spToggle = nullptr;

    int phaseCount = 0;
    bool playing = false;
    bool updatingFromExternal = false;
};

PhaseSliderWidget::PhaseSliderWidget(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    setupConnections();
    setControlsEnabled(false);
}

PhaseSliderWidget::~PhaseSliderWidget() = default;

void PhaseSliderWidget::setupUI()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    impl_->titleLabel = new QLabel(tr("Phase:"), this);
    layout->addWidget(impl_->titleLabel);

    // S/P mode toggle
    impl_->spToggle = new SPModeToggle(this);
    impl_->spToggle->setToolTip(tr("S = Slice scroll, P = Phase scroll"));
    layout->addWidget(impl_->spToggle);

    impl_->playStopButton = new QPushButton(tr("Play"), this);
    impl_->playStopButton->setFixedWidth(50);
    impl_->playStopButton->setToolTip(tr("Start/stop cine playback"));
    layout->addWidget(impl_->playStopButton);

    impl_->slider = new QSlider(Qt::Horizontal, this);
    impl_->slider->setMinimum(0);
    impl_->slider->setMaximum(0);
    impl_->slider->setSingleStep(1);
    impl_->slider->setPageStep(5);
    impl_->slider->setToolTip(tr("Navigate cardiac phases"));
    layout->addWidget(impl_->slider, 1);

    impl_->spinBox = new QSpinBox(this);
    impl_->spinBox->setMinimum(0);
    impl_->spinBox->setMaximum(0);
    impl_->spinBox->setFixedWidth(60);
    impl_->spinBox->setToolTip(tr("Current phase index"));
    layout->addWidget(impl_->spinBox);

    // FPS control for cine playback speed
    impl_->fpsSpinBox = new QSpinBox(this);
    impl_->fpsSpinBox->setMinimum(1);
    impl_->fpsSpinBox->setMaximum(60);
    impl_->fpsSpinBox->setValue(15);
    impl_->fpsSpinBox->setSuffix(tr(" fps"));
    impl_->fpsSpinBox->setFixedWidth(72);
    impl_->fpsSpinBox->setToolTip(tr("Cine playback speed (frames per second)"));
    layout->addWidget(impl_->fpsSpinBox);
}

void PhaseSliderWidget::setupConnections()
{
    connect(impl_->slider, &QSlider::valueChanged,
            this, [this](int value) {
        if (!impl_->updatingFromExternal) {
            impl_->spinBox->blockSignals(true);
            impl_->spinBox->setValue(value);
            impl_->spinBox->blockSignals(false);
            emit phaseChangeRequested(value);
        }
    });

    connect(impl_->spinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int value) {
        if (!impl_->updatingFromExternal) {
            impl_->slider->blockSignals(true);
            impl_->slider->setValue(value);
            impl_->slider->blockSignals(false);
            emit phaseChangeRequested(value);
        }
    });

    connect(impl_->playStopButton, &QPushButton::clicked,
            this, [this]() {
        if (impl_->playing) {
            emit stopRequested();
        } else {
            emit playRequested();
        }
    });

    // S/P toggle → forward as scrollModeChanged signal
    connect(impl_->spToggle, &SPModeToggle::modeChanged,
            this, &PhaseSliderWidget::scrollModeChanged);

    // FPS spinbox → forward as fpsChanged signal
    connect(impl_->fpsSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, &PhaseSliderWidget::fpsChanged);
}

int PhaseSliderWidget::currentPhase() const
{
    return impl_->slider->value();
}

bool PhaseSliderWidget::isPlaying() const
{
    return impl_->playing;
}

ScrollMode PhaseSliderWidget::scrollMode() const
{
    return impl_->spToggle->mode();
}

void PhaseSliderWidget::setPhaseCount(int phaseCount)
{
    impl_->phaseCount = phaseCount;
    int maxVal = (phaseCount > 0) ? phaseCount - 1 : 0;

    impl_->slider->setMaximum(maxVal);
    impl_->spinBox->setMaximum(maxVal);
    impl_->spinBox->setSuffix(QString("/%1").arg(phaseCount > 0 ? phaseCount : 0));

    setControlsEnabled(phaseCount > 1);
}

void PhaseSliderWidget::setCurrentPhase(int phase)
{
    impl_->updatingFromExternal = true;

    impl_->slider->setValue(phase);
    impl_->spinBox->setValue(phase);

    impl_->updatingFromExternal = false;
}

void PhaseSliderWidget::setPlaying(bool playing)
{
    impl_->playing = playing;
    impl_->playStopButton->setText(playing ? tr("Stop") : tr("Play"));
    impl_->slider->setEnabled(!playing);
    impl_->spinBox->setEnabled(!playing);
}

void PhaseSliderWidget::setControlsEnabled(bool enabled)
{
    impl_->slider->setEnabled(enabled);
    impl_->spinBox->setEnabled(enabled);
    impl_->playStopButton->setEnabled(enabled);
}

int PhaseSliderWidget::fps() const
{
    return impl_->fpsSpinBox->value();
}

void PhaseSliderWidget::setFps(int fps)
{
    impl_->fpsSpinBox->setValue(fps);
}

void PhaseSliderWidget::setScrollMode(ScrollMode mode)
{
    if (impl_->spToggle->mode() == mode) return;
    impl_->spToggle->setMode(mode);
    emit scrollModeChanged(mode);
}

} // namespace dicom_viewer::ui

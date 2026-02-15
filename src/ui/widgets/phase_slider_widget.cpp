#include "ui/widgets/phase_slider_widget.hpp"

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
    QPushButton* playStopButton = nullptr;
    QLabel* titleLabel = nullptr;

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
}

int PhaseSliderWidget::currentPhase() const
{
    return impl_->slider->value();
}

bool PhaseSliderWidget::isPlaying() const
{
    return impl_->playing;
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

} // namespace dicom_viewer::ui

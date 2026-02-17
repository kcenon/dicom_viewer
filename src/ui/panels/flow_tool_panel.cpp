#include "ui/panels/flow_tool_panel.hpp"

#include <QButtonGroup>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolBox>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

class FlowToolPanel::Impl {
public:
    QToolBox* toolBox = nullptr;

    // Settings section
    QLabel* phaseLabel = nullptr;
    QLabel* sliceLabel = nullptr;

    // Series section
    QButtonGroup* seriesGroup = nullptr;
    QPushButton* magButton = nullptr;
    QPushButton* rlButton = nullptr;
    QPushButton* apButton = nullptr;
    QPushButton* fhButton = nullptr;
    QPushButton* pcmraButton = nullptr;

    FlowSeries currentSeries = FlowSeries::Magnitude;
    bool flowDataAvailable = false;
};

FlowToolPanel::FlowToolPanel(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    setupConnections();
    setFlowDataAvailable(false);
}

FlowToolPanel::~FlowToolPanel() = default;

FlowSeries FlowToolPanel::selectedSeries() const
{
    return impl_->currentSeries;
}

void FlowToolPanel::setFlowDataAvailable(bool available)
{
    impl_->flowDataAvailable = available;
    impl_->toolBox->setEnabled(available);

    if (!available) {
        impl_->phaseLabel->setText(tr("Phase: --/--"));
        impl_->sliceLabel->setText(tr("Slice: --/--"));
    }
}

void FlowToolPanel::setPhaseInfo(int current, int total)
{
    impl_->phaseLabel->setText(
        tr("Phase: %1/%2").arg(current + 1).arg(total));
}

void FlowToolPanel::setSliceInfo(int current, int total)
{
    impl_->sliceLabel->setText(
        tr("Slice: %1/%2").arg(current + 1).arg(total));
}

void FlowToolPanel::setSelectedSeries(FlowSeries series)
{
    if (impl_->currentSeries == series) return;
    impl_->currentSeries = series;

    // Update button state without triggering signal
    QAbstractButton* target = nullptr;
    switch (series) {
        case FlowSeries::Magnitude: target = impl_->magButton; break;
        case FlowSeries::RL:        target = impl_->rlButton; break;
        case FlowSeries::AP:        target = impl_->apButton; break;
        case FlowSeries::FH:        target = impl_->fhButton; break;
        case FlowSeries::PCMRA:     target = impl_->pcmraButton; break;
    }
    if (target) {
        target->blockSignals(true);
        target->setChecked(true);
        target->blockSignals(false);
    }
}

void FlowToolPanel::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    impl_->toolBox = new QToolBox(this);
    mainLayout->addWidget(impl_->toolBox);

    createSettingsSection();
    createSeriesSection();
    createPlaceholderSections();

    mainLayout->addStretch(1);
}

void FlowToolPanel::createSettingsSection()
{
    auto* settingsWidget = new QWidget();
    auto* layout = new QVBoxLayout(settingsWidget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    impl_->phaseLabel = new QLabel(tr("Phase: --/--"));
    impl_->sliceLabel = new QLabel(tr("Slice: --/--"));

    layout->addWidget(impl_->phaseLabel);
    layout->addWidget(impl_->sliceLabel);
    layout->addStretch(1);

    impl_->toolBox->addItem(settingsWidget, tr("Settings"));
}

void FlowToolPanel::createSeriesSection()
{
    auto* seriesWidget = new QWidget();
    auto* layout = new QVBoxLayout(seriesWidget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    impl_->seriesGroup = new QButtonGroup(this);
    impl_->seriesGroup->setExclusive(true);

    auto createButton = [this, layout](const QString& text, int id) {
        auto* btn = new QPushButton(text);
        btn->setCheckable(true);
        btn->setMinimumHeight(28);
        impl_->seriesGroup->addButton(btn, id);
        layout->addWidget(btn);
        return btn;
    };

    impl_->magButton   = createButton(tr("Mag"),     static_cast<int>(FlowSeries::Magnitude));
    impl_->rlButton    = createButton(tr("RL"),      static_cast<int>(FlowSeries::RL));
    impl_->apButton    = createButton(tr("AP"),      static_cast<int>(FlowSeries::AP));
    impl_->fhButton    = createButton(tr("FH"),      static_cast<int>(FlowSeries::FH));
    impl_->pcmraButton = createButton(tr("PC-MRA"),  static_cast<int>(FlowSeries::PCMRA));

    impl_->magButton->setChecked(true);

    // Horizontal row layout for compact display
    auto* rowWidget = new QWidget();
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(2);

    // Re-parent buttons into horizontal layout
    for (auto* btn : impl_->seriesGroup->buttons()) {
        layout->removeWidget(btn);
        rowLayout->addWidget(btn);
    }

    layout->addWidget(rowWidget);
    layout->addStretch(1);

    impl_->toolBox->addItem(seriesWidget, tr("Series"));
}

void FlowToolPanel::createPlaceholderSections()
{
    // Display 2D placeholder (to be populated by #331)
    auto* display2dWidget = new QWidget();
    auto* layout2d = new QVBoxLayout(display2dWidget);
    layout2d->setContentsMargins(8, 4, 8, 4);
    auto* placeholder2d = new QLabel(tr("No overlay controls available"));
    placeholder2d->setStyleSheet("color: #888;");
    layout2d->addWidget(placeholder2d);
    layout2d->addStretch(1);
    impl_->toolBox->addItem(display2dWidget, tr("Display 2D"));

    // Display 3D placeholder (to be populated by #331)
    auto* display3dWidget = new QWidget();
    auto* layout3d = new QVBoxLayout(display3dWidget);
    layout3d->setContentsMargins(8, 4, 8, 4);
    auto* placeholder3d = new QLabel(tr("No 3D controls available"));
    placeholder3d->setStyleSheet("color: #888;");
    layout3d->addWidget(placeholder3d);
    layout3d->addStretch(1);
    impl_->toolBox->addItem(display3dWidget, tr("Display 3D"));
}

void FlowToolPanel::setupConnections()
{
    connect(impl_->seriesGroup, &QButtonGroup::idClicked,
            this, [this](int id) {
        auto series = static_cast<FlowSeries>(id);
        if (impl_->currentSeries != series) {
            impl_->currentSeries = series;
            emit seriesSelectionChanged(series);
        }
    });
}

} // namespace dicom_viewer::ui

#include "ui/panels/flow_tool_panel.hpp"

#include "services/segmentation/label_manager.hpp"
#include "ui/display_3d_controller.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QToolBox>
#include <QVBoxLayout>

#include <map>

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

    // Display 2D checkboxes
    std::map<Display2DItem, QCheckBox*> display2DChecks;

    // Display 3D checkboxes
    std::map<Display3DItem, QCheckBox*> display3DChecks;

    // Display 3D range controls (min/max spinboxes per colormap item)
    struct RangeControl {
        QWidget* container = nullptr;
        QDoubleSpinBox* minSpin = nullptr;
        QDoubleSpinBox* maxSpin = nullptr;
    };
    std::map<Display3DItem, RangeControl> display3DRanges;

    // Mask section
    QListWidget* maskList = nullptr;
    QPushButton* maskLoadButton = nullptr;
    QPushButton* maskRemoveButton = nullptr;
    services::LabelManager* labelManager = nullptr;

    // 3D Object list section
    QListWidget* objectList = nullptr;

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

bool FlowToolPanel::isDisplay2DEnabled(Display2DItem item) const
{
    auto it = impl_->display2DChecks.find(item);
    if (it == impl_->display2DChecks.end()) return false;
    return it->second->isChecked();
}

bool FlowToolPanel::isDisplay3DEnabled(Display3DItem item) const
{
    auto it = impl_->display3DChecks.find(item);
    if (it == impl_->display3DChecks.end()) return false;
    return it->second->isChecked();
}

int FlowToolPanel::maskCount() const
{
    return impl_->maskList ? impl_->maskList->count() : 0;
}

int FlowToolPanel::objectCount() const
{
    return impl_->objectList ? impl_->objectList->count() : 0;
}

bool FlowToolPanel::isObjectVisible(const QString& name) const
{
    if (!impl_->objectList) return false;
    for (int i = 0; i < impl_->objectList->count(); ++i) {
        auto* item = impl_->objectList->item(i);
        if (item->text() == name) {
            return item->checkState() == Qt::Checked;
        }
    }
    return false;
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

void FlowToolPanel::setDisplay2DEnabled(Display2DItem item, bool enabled)
{
    auto it = impl_->display2DChecks.find(item);
    if (it == impl_->display2DChecks.end()) return;
    it->second->blockSignals(true);
    it->second->setChecked(enabled);
    it->second->blockSignals(false);
}

void FlowToolPanel::setDisplay3DEnabled(Display3DItem item, bool enabled)
{
    auto it = impl_->display3DChecks.find(item);
    if (it == impl_->display3DChecks.end()) return;
    it->second->blockSignals(true);
    it->second->setChecked(enabled);
    it->second->blockSignals(false);
}

void FlowToolPanel::setDisplay3DRange(Display3DItem item, double minVal, double maxVal)
{
    auto it = impl_->display3DRanges.find(item);
    if (it == impl_->display3DRanges.end()) return;
    auto& rc = it->second;
    rc.minSpin->blockSignals(true);
    rc.maxSpin->blockSignals(true);
    rc.minSpin->setValue(minVal);
    rc.maxSpin->setValue(maxVal);
    rc.minSpin->blockSignals(false);
    rc.maxSpin->blockSignals(false);
}

void FlowToolPanel::setLabelManager(services::LabelManager* manager)
{
    impl_->labelManager = manager;
    refreshMaskList();
}

void FlowToolPanel::refreshMaskList()
{
    if (!impl_->maskList) return;

    impl_->maskList->blockSignals(true);
    impl_->maskList->clear();

    if (impl_->labelManager) {
        auto labels = impl_->labelManager->getAllLabels();
        for (const auto& label : labels) {
            auto* item = new QListWidgetItem();
            item->setText(QString::fromStdString(label.name));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(label.visible ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, static_cast<int>(label.id));

            // Color swatch as decoration icon
            QPixmap swatch(12, 12);
            auto rgba = label.color.toRGBA8();
            swatch.fill(QColor(rgba[0], rgba[1], rgba[2], rgba[3]));
            item->setIcon(QIcon(swatch));

            impl_->maskList->addItem(item);
        }
    }

    impl_->maskList->blockSignals(false);
}

void FlowToolPanel::addObject(const QString& name, bool visible)
{
    if (!impl_->objectList) return;

    // Avoid duplicates
    for (int i = 0; i < impl_->objectList->count(); ++i) {
        if (impl_->objectList->item(i)->text() == name) return;
    }

    auto* item = new QListWidgetItem(name);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
    impl_->objectList->addItem(item);
}

void FlowToolPanel::removeObject(const QString& name)
{
    if (!impl_->objectList) return;
    for (int i = 0; i < impl_->objectList->count(); ++i) {
        if (impl_->objectList->item(i)->text() == name) {
            delete impl_->objectList->takeItem(i);
            return;
        }
    }
}

void FlowToolPanel::setObjectVisible(const QString& name, bool visible)
{
    if (!impl_->objectList) return;
    for (int i = 0; i < impl_->objectList->count(); ++i) {
        auto* item = impl_->objectList->item(i);
        if (item->text() == name) {
            impl_->objectList->blockSignals(true);
            item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
            impl_->objectList->blockSignals(false);
            return;
        }
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
    createMaskSection();
    createDisplay2DSection();
    createDisplay3DSection();
    create3DObjectSection();

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

void FlowToolPanel::createMaskSection()
{
    auto* maskWidget = new QWidget();
    auto* layout = new QVBoxLayout(maskWidget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    impl_->maskList = new QListWidget();
    impl_->maskList->setSelectionMode(QAbstractItemView::SingleSelection);
    impl_->maskList->setMaximumHeight(120);
    layout->addWidget(impl_->maskList);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(4);
    impl_->maskLoadButton = new QPushButton(tr("Load"));
    impl_->maskRemoveButton = new QPushButton(tr("Remove"));
    impl_->maskRemoveButton->setEnabled(false);
    buttonLayout->addWidget(impl_->maskLoadButton);
    buttonLayout->addWidget(impl_->maskRemoveButton);
    layout->addLayout(buttonLayout);

    layout->addStretch(1);
    impl_->toolBox->addItem(maskWidget, tr("Mask"));
}

void FlowToolPanel::createDisplay2DSection()
{
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);

    struct Item {
        Display2DItem id;
        const char* label;
    };

    const Item items[] = {
        { Display2DItem::Mask,            "Mask" },
        { Display2DItem::Velocity,        "Velocity" },
        { Display2DItem::Streamline,      "Streamline" },
        { Display2DItem::EnergyLoss,      "Energy Loss" },
        { Display2DItem::Vorticity,       "Vorticity" },
        { Display2DItem::VelocityTexture, "Vel Texture" },
    };

    for (const auto& item : items) {
        auto* cb = new QCheckBox(tr(item.label));
        layout->addWidget(cb);
        impl_->display2DChecks[item.id] = cb;
    }

    layout->addStretch(1);
    impl_->toolBox->addItem(widget, tr("Display 2D"));
}

void FlowToolPanel::createDisplay3DSection()
{
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);

    struct Item {
        Display3DItem id;
        const char* label;
        double defaultMin;
        double defaultMax;
    };

    const Item items[] = {
        { Display3DItem::MaskVolume,  "Mask Vol",     0, 0 },
        { Display3DItem::Surface,     "Surface",      0, 0 },
        { Display3DItem::Cine,        "Cine",         0, 0 },
        { Display3DItem::Magnitude,   "Magnitude",    0, 100.0 },
        { Display3DItem::Velocity,    "Velocity",     0, 100.0 },
        { Display3DItem::ASC,         "ASC",          0, 0 },
        { Display3DItem::Streamline,  "Streamline",   0, 0 },
        { Display3DItem::EnergyLoss,  "Energy Loss",  0, 100.0 },
        { Display3DItem::WSS,         "WSS",          0, 5.0 },
        { Display3DItem::OSI,         "OSI",          0, 0.5 },
        { Display3DItem::AFI,         "AFI",          0, 2.0 },
        { Display3DItem::RRT,         "RRT",          0, 100.0 },
        { Display3DItem::Vorticity,   "Vorticity",    0, 100.0 },
    };

    for (const auto& item : items) {
        auto* cb = new QCheckBox(tr(item.label));
        layout->addWidget(cb);
        impl_->display3DChecks[item.id] = cb;

        // Add min/max range controls for colormap items
        if (Display3DController::hasColormapRange(item.id)) {
            auto* rangeContainer = new QWidget();
            auto* rangeLayout = new QHBoxLayout(rangeContainer);
            rangeLayout->setContentsMargins(20, 0, 4, 0);
            rangeLayout->setSpacing(4);

            auto* minSpin = new QDoubleSpinBox();
            minSpin->setPrefix(tr("Min: "));
            minSpin->setRange(0.0, 9999.0);
            minSpin->setDecimals(2);
            minSpin->setValue(item.defaultMin);
            minSpin->setSingleStep(0.1);
            minSpin->setMaximumHeight(22);

            auto* maxSpin = new QDoubleSpinBox();
            maxSpin->setPrefix(tr("Max: "));
            maxSpin->setRange(0.01, 9999.0);
            maxSpin->setDecimals(2);
            maxSpin->setValue(item.defaultMax);
            maxSpin->setSingleStep(0.1);
            maxSpin->setMaximumHeight(22);

            rangeLayout->addWidget(minSpin);
            rangeLayout->addWidget(maxSpin);
            layout->addWidget(rangeContainer);

            impl_->display3DRanges[item.id] = {rangeContainer, minSpin, maxSpin};
        }
    }

    layout->addStretch(1);
    impl_->toolBox->addItem(widget, tr("Display 3D"));
}

void FlowToolPanel::create3DObjectSection()
{
    auto* objectWidget = new QWidget();
    auto* layout = new QVBoxLayout(objectWidget);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    impl_->objectList = new QListWidget();
    impl_->objectList->setSelectionMode(QAbstractItemView::SingleSelection);
    impl_->objectList->setMaximumHeight(120);
    layout->addWidget(impl_->objectList);

    layout->addStretch(1);
    impl_->toolBox->addItem(objectWidget, tr("3D Object"));
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

    // Display 2D checkbox connections
    for (auto& [item, cb] : impl_->display2DChecks) {
        connect(cb, &QCheckBox::toggled,
                this, [this, item](bool checked) {
            emit display2DToggled(item, checked);
        });
    }

    // Display 3D checkbox connections
    for (auto& [item, cb] : impl_->display3DChecks) {
        connect(cb, &QCheckBox::toggled,
                this, [this, item](bool checked) {
            emit display3DToggled(item, checked);
        });
    }

    // Display 3D range spinbox connections
    for (auto& [item, rc] : impl_->display3DRanges) {
        connect(rc.minSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, item](double /*val*/) {
            auto& r = impl_->display3DRanges[item];
            emit display3DRangeChanged(item, r.minSpin->value(), r.maxSpin->value());
        });
        connect(rc.maxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, item](double /*val*/) {
            auto& r = impl_->display3DRanges[item];
            emit display3DRangeChanged(item, r.minSpin->value(), r.maxSpin->value());
        });
    }

    // Mask list item checkbox changes
    connect(impl_->maskList, &QListWidget::itemChanged,
            this, [this](QListWidgetItem* item) {
        auto labelId = static_cast<uint8_t>(item->data(Qt::UserRole).toInt());
        bool visible = item->checkState() == Qt::Checked;
        emit maskVisibilityToggled(labelId, visible);
    });

    // Mask list selection → enable/disable Remove button
    connect(impl_->maskList, &QListWidget::currentRowChanged,
            this, [this](int row) {
        impl_->maskRemoveButton->setEnabled(row >= 0);
    });

    // Mask Load button
    connect(impl_->maskLoadButton, &QPushButton::clicked,
            this, [this]() { emit maskLoadRequested(); });

    // Mask Remove button
    connect(impl_->maskRemoveButton, &QPushButton::clicked,
            this, [this]() {
        auto* current = impl_->maskList->currentItem();
        if (current) {
            auto labelId = static_cast<uint8_t>(current->data(Qt::UserRole).toInt());
            emit maskRemoveRequested(labelId);
        }
    });

    // 3D Object list item checkbox changes
    connect(impl_->objectList, &QListWidget::itemChanged,
            this, [this](QListWidgetItem* item) {
        bool visible = item->checkState() == Qt::Checked;
        emit objectVisibilityToggled(item->text(), visible);
    });
}

} // namespace dicom_viewer::ui

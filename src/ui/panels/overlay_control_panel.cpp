#include "ui/panels/overlay_control_panel.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <unordered_map>

namespace dicom_viewer::ui {

// =============================================================================
// Per-overlay UI control group
// =============================================================================

struct OverlayControlGroup {
    services::OverlayType type;
    QCheckBox* checkbox = nullptr;
    QSlider* opacitySlider = nullptr;
    QLabel* opacityLabel = nullptr;
    QDoubleSpinBox* rangeMin = nullptr;
    QDoubleSpinBox* rangeMax = nullptr;
};

// =============================================================================
// Implementation
// =============================================================================

class OverlayControlPanel::Impl {
public:
    QVBoxLayout* mainLayout = nullptr;
    std::vector<OverlayControlGroup> groups;
    bool overlaysAvailable = false;

    OverlayControlGroup* findGroup(services::OverlayType type) {
        for (auto& g : groups) {
            if (g.type == type) return &g;
        }
        return nullptr;
    }

    const OverlayControlGroup* findGroup(services::OverlayType type) const {
        for (const auto& g : groups) {
            if (g.type == type) return &g;
        }
        return nullptr;
    }
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

OverlayControlPanel::OverlayControlPanel(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    setupConnections();
}

OverlayControlPanel::~OverlayControlPanel() = default;

// =============================================================================
// UI Setup
// =============================================================================

void OverlayControlPanel::setupUI() {
    impl_->mainLayout = new QVBoxLayout(this);
    impl_->mainLayout->setContentsMargins(4, 4, 4, 4);
    impl_->mainLayout->setSpacing(4);

    auto* titleLabel = new QLabel(tr("Display 2D Overlays"));
    titleLabel->setStyleSheet("font-weight: bold;");
    impl_->mainLayout->addWidget(titleLabel);

    // Create control groups for each overlay type
    createOverlayGroup(services::OverlayType::VelocityMagnitude,
                       tr("Velocity Magnitude"), 0.0, 100.0);
    createOverlayGroup(services::OverlayType::VelocityX,
                       tr("Velocity X"), -100.0, 100.0);
    createOverlayGroup(services::OverlayType::VelocityY,
                       tr("Velocity Y"), -100.0, 100.0);
    createOverlayGroup(services::OverlayType::VelocityZ,
                       tr("Velocity Z"), -100.0, 100.0);
    createOverlayGroup(services::OverlayType::Vorticity,
                       tr("Vorticity"), 0.0, 50.0);
    createOverlayGroup(services::OverlayType::EnergyLoss,
                       tr("Energy Loss"), 0.0, 1000.0);
    createOverlayGroup(services::OverlayType::Streamline,
                       tr("Streamlines"), 0.0, 100.0);
    createOverlayGroup(services::OverlayType::VelocityTexture,
                       tr("Velocity Texture (LIC)"), 0.0, 255.0);

    impl_->mainLayout->addStretch();

    // Start disabled until data is available
    setOverlaysAvailable(false);
}

void OverlayControlPanel::createOverlayGroup(
    services::OverlayType type,
    const QString& label,
    double defaultMin,
    double defaultMax) {

    OverlayControlGroup group;
    group.type = type;

    auto* groupBox = new QGroupBox();
    groupBox->setFlat(true);
    auto* layout = new QVBoxLayout(groupBox);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // Checkbox for visibility
    group.checkbox = new QCheckBox(label);
    group.checkbox->setChecked(false);
    layout->addWidget(group.checkbox);

    // Opacity row
    auto* opacityRow = new QHBoxLayout();
    opacityRow->addWidget(new QLabel(tr("Opacity:")));
    group.opacitySlider = new QSlider(Qt::Horizontal);
    group.opacitySlider->setRange(0, 100);
    group.opacitySlider->setValue(50);
    group.opacitySlider->setFixedWidth(80);
    opacityRow->addWidget(group.opacitySlider);
    group.opacityLabel = new QLabel("50%");
    group.opacityLabel->setFixedWidth(30);
    opacityRow->addWidget(group.opacityLabel);
    layout->addLayout(opacityRow);

    // Scalar range row (skip for Streamline/VelocityTexture - not colormap-based)
    bool hasRange = (type != services::OverlayType::Streamline &&
                     type != services::OverlayType::VelocityTexture);
    if (hasRange) {
        auto* rangeRow = new QHBoxLayout();
        rangeRow->addWidget(new QLabel(tr("Range:")));

        group.rangeMin = new QDoubleSpinBox();
        group.rangeMin->setRange(-10000.0, 10000.0);
        group.rangeMin->setValue(defaultMin);
        group.rangeMin->setDecimals(1);
        group.rangeMin->setFixedWidth(70);
        rangeRow->addWidget(group.rangeMin);

        rangeRow->addWidget(new QLabel("-"));

        group.rangeMax = new QDoubleSpinBox();
        group.rangeMax->setRange(-10000.0, 10000.0);
        group.rangeMax->setValue(defaultMax);
        group.rangeMax->setDecimals(1);
        group.rangeMax->setFixedWidth(70);
        rangeRow->addWidget(group.rangeMax);

        layout->addLayout(rangeRow);
    }

    impl_->mainLayout->addWidget(groupBox);

    int typeIndex = static_cast<int>(type);

    // Connect checkbox
    connect(group.checkbox, &QCheckBox::toggled,
            this, [this, typeIndex](bool checked) {
                onCheckboxToggled(typeIndex, checked);
            });

    // Connect opacity slider
    connect(group.opacitySlider, &QSlider::valueChanged,
            this, [this, typeIndex](int value) {
                onOpacitySliderChanged(typeIndex, value);
            });

    // Connect range spinboxes
    if (group.rangeMin) {
        connect(group.rangeMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, typeIndex](double value) {
                    onRangeMinChanged(typeIndex, value);
                });
    }
    if (group.rangeMax) {
        connect(group.rangeMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, typeIndex](double value) {
                    onRangeMaxChanged(typeIndex, value);
                });
    }

    impl_->groups.push_back(std::move(group));
}

void OverlayControlPanel::setupConnections() {
    // Connections are set up in createOverlayGroup
}

// =============================================================================
// Public API
// =============================================================================

void OverlayControlPanel::setOverlaysAvailable(bool available) {
    impl_->overlaysAvailable = available;
    for (auto& group : impl_->groups) {
        group.checkbox->setEnabled(available);
        group.opacitySlider->setEnabled(available && group.checkbox->isChecked());
        if (group.rangeMin) {
            group.rangeMin->setEnabled(available && group.checkbox->isChecked());
        }
        if (group.rangeMax) {
            group.rangeMax->setEnabled(available && group.checkbox->isChecked());
        }
    }
}

bool OverlayControlPanel::isOverlayEnabled(services::OverlayType type) const {
    auto* group = impl_->findGroup(type);
    return group ? group->checkbox->isChecked() : false;
}

double OverlayControlPanel::overlayOpacity(services::OverlayType type) const {
    auto* group = impl_->findGroup(type);
    return group ? group->opacitySlider->value() / 100.0 : 0.5;
}

std::pair<double, double>
OverlayControlPanel::overlayScalarRange(services::OverlayType type) const {
    auto* group = impl_->findGroup(type);
    if (group && group->rangeMin && group->rangeMax) {
        return {group->rangeMin->value(), group->rangeMax->value()};
    }
    return {0.0, 100.0};
}

void OverlayControlPanel::resetToDefaults() {
    for (auto& group : impl_->groups) {
        group.checkbox->setChecked(false);
        group.opacitySlider->setValue(50);
    }
}

// =============================================================================
// Slots
// =============================================================================

void OverlayControlPanel::onCheckboxToggled(int typeIndex, bool checked) {
    auto type = static_cast<services::OverlayType>(typeIndex);
    auto* group = impl_->findGroup(type);
    if (group) {
        bool available = impl_->overlaysAvailable;
        group->opacitySlider->setEnabled(available && checked);
        if (group->rangeMin) group->rangeMin->setEnabled(available && checked);
        if (group->rangeMax) group->rangeMax->setEnabled(available && checked);
    }
    emit overlayVisibilityChanged(type, checked);
}

void OverlayControlPanel::onOpacitySliderChanged(int typeIndex, int value) {
    auto type = static_cast<services::OverlayType>(typeIndex);
    auto* group = impl_->findGroup(type);
    if (group) {
        group->opacityLabel->setText(QString("%1%").arg(value));
    }
    emit overlayOpacityChanged(type, value / 100.0);
}

void OverlayControlPanel::onRangeMinChanged(int typeIndex, double value) {
    auto type = static_cast<services::OverlayType>(typeIndex);
    auto* group = impl_->findGroup(type);
    if (group && group->rangeMax) {
        emit overlayScalarRangeChanged(type, value, group->rangeMax->value());
    }
}

void OverlayControlPanel::onRangeMaxChanged(int typeIndex, double value) {
    auto type = static_cast<services::OverlayType>(typeIndex);
    auto* group = impl_->findGroup(type);
    if (group && group->rangeMin) {
        emit overlayScalarRangeChanged(type, group->rangeMin->value(), value);
    }
}

} // namespace dicom_viewer::ui

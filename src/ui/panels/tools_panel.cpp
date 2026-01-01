#include "ui/panels/tools_panel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QScrollArea>

namespace dicom_viewer::ui {

class ToolsPanel::Impl {
public:
    // Window/Level controls
    QSlider* windowSlider = nullptr;
    QSlider* levelSlider = nullptr;
    QSpinBox* windowSpinBox = nullptr;
    QSpinBox* levelSpinBox = nullptr;

    // Preset buttons
    QButtonGroup* presetGroup = nullptr;
    QPushButton* presetBone = nullptr;
    QPushButton* presetLung = nullptr;
    QPushButton* presetAbdomen = nullptr;
    QPushButton* presetBrain = nullptr;

    // Visualization mode
    QComboBox* modeCombo = nullptr;

    // Slice control
    QSlider* sliceSlider = nullptr;
    QSpinBox* sliceSpinBox = nullptr;

    // Current values
    double currentWindowWidth = 400.0;
    double currentWindowCenter = 40.0;
    ToolCategory currentCategory = ToolCategory::Navigation;

    QWidget* createSliderWithSpinBox(const QString& label, QSlider*& slider,
                                      QSpinBox*& spinBox, int min, int max,
                                      int value, QWidget* parent) {
        auto container = new QWidget(parent);
        auto layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);

        auto labelWidget = new QLabel(label, container);
        layout->addWidget(labelWidget);

        auto sliderLayout = new QHBoxLayout();
        sliderLayout->setSpacing(4);

        slider = new QSlider(Qt::Horizontal, container);
        slider->setRange(min, max);
        slider->setValue(value);

        spinBox = new QSpinBox(container);
        spinBox->setRange(min, max);
        spinBox->setValue(value);
        spinBox->setFixedWidth(70);

        sliderLayout->addWidget(slider);
        sliderLayout->addWidget(spinBox);
        layout->addLayout(sliderLayout);

        // Sync slider and spinbox
        QObject::connect(slider, &QSlider::valueChanged,
                         spinBox, &QSpinBox::setValue);
        QObject::connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                         slider, &QSlider::setValue);

        return container;
    }
};

ToolsPanel::ToolsPanel(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    setupConnections();
}

ToolsPanel::~ToolsPanel() = default;

void ToolsPanel::setupUI()
{
    auto scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto content = new QWidget();
    auto mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);

    createWindowLevelSection();
    createPresetSection();
    createVisualizationSection();
    createNavigationSection();

    // Add sections to main layout
    auto wlGroup = new QGroupBox(tr("Window/Level"));
    auto wlLayout = new QVBoxLayout(wlGroup);
    wlLayout->addWidget(impl_->createSliderWithSpinBox(
        tr("Window Width"), impl_->windowSlider, impl_->windowSpinBox,
        1, 4000, static_cast<int>(impl_->currentWindowWidth), wlGroup));
    wlLayout->addWidget(impl_->createSliderWithSpinBox(
        tr("Window Center"), impl_->levelSlider, impl_->levelSpinBox,
        -1000, 3000, static_cast<int>(impl_->currentWindowCenter), wlGroup));
    mainLayout->addWidget(wlGroup);

    // Presets group
    auto presetGroup = new QGroupBox(tr("CT Presets"));
    auto presetLayout = new QGridLayout(presetGroup);
    presetLayout->setSpacing(4);

    impl_->presetBone = new QPushButton(tr("Bone"), presetGroup);
    impl_->presetLung = new QPushButton(tr("Lung"), presetGroup);
    impl_->presetAbdomen = new QPushButton(tr("Abdomen"), presetGroup);
    impl_->presetBrain = new QPushButton(tr("Brain"), presetGroup);

    presetLayout->addWidget(impl_->presetBone, 0, 0);
    presetLayout->addWidget(impl_->presetLung, 0, 1);
    presetLayout->addWidget(impl_->presetAbdomen, 1, 0);
    presetLayout->addWidget(impl_->presetBrain, 1, 1);
    mainLayout->addWidget(presetGroup);

    // Visualization group
    auto vizGroup = new QGroupBox(tr("Visualization"));
    auto vizLayout = new QVBoxLayout(vizGroup);

    auto modeLabel = new QLabel(tr("Mode:"), vizGroup);
    impl_->modeCombo = new QComboBox(vizGroup);
    impl_->modeCombo->addItem(tr("2D Slice View"), 0);
    impl_->modeCombo->addItem(tr("MPR (3-plane)"), 1);
    impl_->modeCombo->addItem(tr("Volume Rendering"), 2);
    impl_->modeCombo->addItem(tr("Surface Rendering"), 3);

    vizLayout->addWidget(modeLabel);
    vizLayout->addWidget(impl_->modeCombo);
    mainLayout->addWidget(vizGroup);

    // Slice navigation group
    auto navGroup = new QGroupBox(tr("Navigation"));
    auto navLayout = new QVBoxLayout(navGroup);
    navLayout->addWidget(impl_->createSliderWithSpinBox(
        tr("Slice"), impl_->sliceSlider, impl_->sliceSpinBox,
        0, 100, 50, navGroup));
    mainLayout->addWidget(navGroup);

    mainLayout->addStretch();

    scrollArea->setWidget(content);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scrollArea);
    setLayout(layout);
}

void ToolsPanel::setupConnections()
{
    // Window/Level
    connect(impl_->windowSlider, &QSlider::valueChanged,
            this, &ToolsPanel::onWindowSliderChanged);
    connect(impl_->levelSlider, &QSlider::valueChanged,
            this, &ToolsPanel::onLevelSliderChanged);

    // Presets
    connect(impl_->presetBone, &QPushButton::clicked, this, [this]() {
        emit presetSelected("CT Bone");
    });
    connect(impl_->presetLung, &QPushButton::clicked, this, [this]() {
        emit presetSelected("CT Lung");
    });
    connect(impl_->presetAbdomen, &QPushButton::clicked, this, [this]() {
        emit presetSelected("CT Abdomen");
    });
    connect(impl_->presetBrain, &QPushButton::clicked, this, [this]() {
        emit presetSelected("CT Brain");
    });

    // Visualization mode
    connect(impl_->modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolsPanel::visualizationModeChanged);

    // Slice
    connect(impl_->sliceSlider, &QSlider::valueChanged,
            this, &ToolsPanel::sliceChanged);
}

void ToolsPanel::createNavigationSection() {}
void ToolsPanel::createWindowLevelSection() {}
void ToolsPanel::createPresetSection() {}
void ToolsPanel::createVisualizationSection() {}

void ToolsPanel::setToolCategory(ToolCategory category)
{
    impl_->currentCategory = category;
}

void ToolsPanel::setWindowLevel(double width, double center)
{
    impl_->currentWindowWidth = width;
    impl_->currentWindowCenter = center;

    impl_->windowSlider->blockSignals(true);
    impl_->levelSlider->blockSignals(true);

    impl_->windowSlider->setValue(static_cast<int>(width));
    impl_->levelSlider->setValue(static_cast<int>(center));

    impl_->windowSlider->blockSignals(false);
    impl_->levelSlider->blockSignals(false);
}

double ToolsPanel::windowWidth() const
{
    return impl_->currentWindowWidth;
}

double ToolsPanel::windowCenter() const
{
    return impl_->currentWindowCenter;
}

void ToolsPanel::onWindowSliderChanged(int value)
{
    impl_->currentWindowWidth = static_cast<double>(value);
    emit windowLevelChanged(impl_->currentWindowWidth, impl_->currentWindowCenter);
}

void ToolsPanel::onLevelSliderChanged(int value)
{
    impl_->currentWindowCenter = static_cast<double>(value);
    emit windowLevelChanged(impl_->currentWindowWidth, impl_->currentWindowCenter);
}

void ToolsPanel::onPresetButtonClicked()
{
    // Handled by individual button connections
}

} // namespace dicom_viewer::ui

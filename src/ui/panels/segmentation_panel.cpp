#include "ui/panels/segmentation_panel.hpp"
#include "services/segmentation/segmentation_label.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QButtonGroup>
#include <QToolButton>
#include <QColorDialog>
#include <QScrollArea>
#include <QFrame>

namespace dicom_viewer::ui {

class SegmentationPanel::Impl {
public:
    // Tool buttons
    QButtonGroup* toolGroup = nullptr;
    QToolButton* brushButton = nullptr;
    QToolButton* eraserButton = nullptr;
    QToolButton* fillButton = nullptr;
    QToolButton* freehandButton = nullptr;
    QToolButton* polygonButton = nullptr;
    QToolButton* smartScissorsButton = nullptr;

    // Brush settings
    QSlider* brushSizeSlider = nullptr;
    QSpinBox* brushSizeSpinBox = nullptr;
    QComboBox* brushShapeCombo = nullptr;
    QWidget* brushOptionsWidget = nullptr;

    // Label settings
    QComboBox* labelCombo = nullptr;
    QPushButton* colorButton = nullptr;
    services::LabelColor currentColor{0.9f, 0.3f, 0.3f, 1.0f};

    // Action buttons
    QPushButton* undoButton = nullptr;
    QPushButton* completeButton = nullptr;
    QPushButton* clearAllButton = nullptr;
    QWidget* polygonActionsWidget = nullptr;

    // State
    services::SegmentationTool currentTool = services::SegmentationTool::None;
    int currentBrushSize = 5;
    services::BrushShape currentBrushShape = services::BrushShape::Circle;
    uint8_t currentLabelId = 1;

    void updateColorButtonStyle(QPushButton* button, const services::LabelColor& color) {
        auto rgba = color.toRGBA8();
        QString style = QString(
            "QPushButton { "
            "background-color: rgb(%1, %2, %3); "
            "border: 2px solid #555; "
            "border-radius: 4px; "
            "min-width: 40px; "
            "min-height: 24px; "
            "}"
            "QPushButton:hover { border-color: #888; }"
        ).arg(rgba[0]).arg(rgba[1]).arg(rgba[2]);
        button->setStyleSheet(style);
    }
};

SegmentationPanel::SegmentationPanel(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    setupConnections();
    updateToolOptions();
}

SegmentationPanel::~SegmentationPanel() = default;

void SegmentationPanel::setupUI()
{
    auto scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto content = new QWidget();
    auto mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);

    createToolSection();
    createBrushSection();
    createLabelSection();
    createActionSection();

    // Tools group
    auto toolsGroup = new QGroupBox(tr("Segmentation Tools"));
    auto toolsLayout = new QGridLayout(toolsGroup);
    toolsLayout->setSpacing(4);

    impl_->toolGroup = new QButtonGroup(this);
    impl_->toolGroup->setExclusive(true);

    impl_->brushButton = new QToolButton();
    impl_->brushButton->setText(tr("Brush"));
    impl_->brushButton->setCheckable(true);
    impl_->brushButton->setMinimumSize(60, 40);
    impl_->toolGroup->addButton(impl_->brushButton, static_cast<int>(services::SegmentationTool::Brush));

    impl_->eraserButton = new QToolButton();
    impl_->eraserButton->setText(tr("Eraser"));
    impl_->eraserButton->setCheckable(true);
    impl_->eraserButton->setMinimumSize(60, 40);
    impl_->toolGroup->addButton(impl_->eraserButton, static_cast<int>(services::SegmentationTool::Eraser));

    impl_->fillButton = new QToolButton();
    impl_->fillButton->setText(tr("Fill"));
    impl_->fillButton->setCheckable(true);
    impl_->fillButton->setMinimumSize(60, 40);
    impl_->toolGroup->addButton(impl_->fillButton, static_cast<int>(services::SegmentationTool::Fill));

    impl_->freehandButton = new QToolButton();
    impl_->freehandButton->setText(tr("Freehand"));
    impl_->freehandButton->setCheckable(true);
    impl_->freehandButton->setMinimumSize(60, 40);
    impl_->toolGroup->addButton(impl_->freehandButton, static_cast<int>(services::SegmentationTool::Freehand));

    impl_->polygonButton = new QToolButton();
    impl_->polygonButton->setText(tr("Polygon"));
    impl_->polygonButton->setCheckable(true);
    impl_->polygonButton->setMinimumSize(60, 40);
    impl_->toolGroup->addButton(impl_->polygonButton, static_cast<int>(services::SegmentationTool::Polygon));

    impl_->smartScissorsButton = new QToolButton();
    impl_->smartScissorsButton->setText(tr("Smart\nScissors"));
    impl_->smartScissorsButton->setCheckable(true);
    impl_->smartScissorsButton->setMinimumSize(60, 40);
    impl_->toolGroup->addButton(impl_->smartScissorsButton, static_cast<int>(services::SegmentationTool::SmartScissors));

    toolsLayout->addWidget(impl_->brushButton, 0, 0);
    toolsLayout->addWidget(impl_->eraserButton, 0, 1);
    toolsLayout->addWidget(impl_->fillButton, 0, 2);
    toolsLayout->addWidget(impl_->freehandButton, 1, 0);
    toolsLayout->addWidget(impl_->polygonButton, 1, 1);
    toolsLayout->addWidget(impl_->smartScissorsButton, 1, 2);
    mainLayout->addWidget(toolsGroup);

    // Brush options group
    impl_->brushOptionsWidget = new QGroupBox(tr("Brush Options"));
    auto brushLayout = new QVBoxLayout(static_cast<QGroupBox*>(impl_->brushOptionsWidget));

    // Brush size
    auto sizeLayout = new QVBoxLayout();
    sizeLayout->setSpacing(2);
    sizeLayout->addWidget(new QLabel(tr("Size (1-50 px):")));

    auto sizeSliderLayout = new QHBoxLayout();
    impl_->brushSizeSlider = new QSlider(Qt::Horizontal);
    impl_->brushSizeSlider->setRange(1, 50);
    impl_->brushSizeSlider->setValue(impl_->currentBrushSize);

    impl_->brushSizeSpinBox = new QSpinBox();
    impl_->brushSizeSpinBox->setRange(1, 50);
    impl_->brushSizeSpinBox->setValue(impl_->currentBrushSize);
    impl_->brushSizeSpinBox->setFixedWidth(60);

    sizeSliderLayout->addWidget(impl_->brushSizeSlider);
    sizeSliderLayout->addWidget(impl_->brushSizeSpinBox);
    sizeLayout->addLayout(sizeSliderLayout);
    brushLayout->addLayout(sizeLayout);

    // Brush shape
    auto shapeLayout = new QHBoxLayout();
    shapeLayout->addWidget(new QLabel(tr("Shape:")));
    impl_->brushShapeCombo = new QComboBox();
    impl_->brushShapeCombo->addItem(tr("Circle"), static_cast<int>(services::BrushShape::Circle));
    impl_->brushShapeCombo->addItem(tr("Square"), static_cast<int>(services::BrushShape::Square));
    shapeLayout->addWidget(impl_->brushShapeCombo);
    brushLayout->addLayout(shapeLayout);

    mainLayout->addWidget(impl_->brushOptionsWidget);

    // Label group
    auto labelGroup = new QGroupBox(tr("Label"));
    auto labelLayout = new QVBoxLayout(labelGroup);

    auto labelSelectLayout = new QHBoxLayout();
    labelSelectLayout->addWidget(new QLabel(tr("Active:")));
    impl_->labelCombo = new QComboBox();
    for (int i = 1; i <= 10; ++i) {
        impl_->labelCombo->addItem(QString("Label %1").arg(i), i);
    }
    labelSelectLayout->addWidget(impl_->labelCombo, 1);
    labelLayout->addLayout(labelSelectLayout);

    auto colorLayout = new QHBoxLayout();
    colorLayout->addWidget(new QLabel(tr("Color:")));
    impl_->colorButton = new QPushButton();
    impl_->updateColorButtonStyle(impl_->colorButton, impl_->currentColor);
    colorLayout->addWidget(impl_->colorButton);
    colorLayout->addStretch();
    labelLayout->addLayout(colorLayout);

    mainLayout->addWidget(labelGroup);

    // Polygon/Smart Scissors actions
    impl_->polygonActionsWidget = new QGroupBox(tr("Drawing Actions"));
    auto actionsLayout = new QHBoxLayout(static_cast<QGroupBox*>(impl_->polygonActionsWidget));

    impl_->undoButton = new QPushButton(tr("Undo"));
    impl_->undoButton->setToolTip(tr("Remove last point"));
    impl_->completeButton = new QPushButton(tr("Complete"));
    impl_->completeButton->setToolTip(tr("Complete and apply drawing"));

    actionsLayout->addWidget(impl_->undoButton);
    actionsLayout->addWidget(impl_->completeButton);
    mainLayout->addWidget(impl_->polygonActionsWidget);

    // Clear all
    auto clearGroup = new QGroupBox(tr("Actions"));
    auto clearLayout = new QVBoxLayout(clearGroup);
    impl_->clearAllButton = new QPushButton(tr("Clear All Segmentation"));
    impl_->clearAllButton->setToolTip(tr("Remove all segmentation data"));
    clearLayout->addWidget(impl_->clearAllButton);
    mainLayout->addWidget(clearGroup);

    mainLayout->addStretch();

    scrollArea->setWidget(content);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scrollArea);
    setLayout(layout);
}

void SegmentationPanel::setupConnections()
{
    // Tool selection
    connect(impl_->toolGroup, &QButtonGroup::idClicked,
            this, &SegmentationPanel::onToolButtonClicked);

    // Brush size sync
    connect(impl_->brushSizeSlider, &QSlider::valueChanged,
            impl_->brushSizeSpinBox, &QSpinBox::setValue);
    connect(impl_->brushSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            impl_->brushSizeSlider, &QSlider::setValue);
    connect(impl_->brushSizeSlider, &QSlider::valueChanged,
            this, &SegmentationPanel::onBrushSizeChanged);

    // Brush shape
    connect(impl_->brushShapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SegmentationPanel::onBrushShapeChanged);

    // Label selection
    connect(impl_->labelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SegmentationPanel::onLabelChanged);

    // Color picker
    connect(impl_->colorButton, &QPushButton::clicked,
            this, &SegmentationPanel::onColorButtonClicked);

    // Actions
    connect(impl_->undoButton, &QPushButton::clicked,
            this, &SegmentationPanel::onUndoClicked);
    connect(impl_->completeButton, &QPushButton::clicked,
            this, &SegmentationPanel::onCompleteClicked);
    connect(impl_->clearAllButton, &QPushButton::clicked,
            this, &SegmentationPanel::onClearAllClicked);
}

void SegmentationPanel::createToolSection() {}
void SegmentationPanel::createBrushSection() {}
void SegmentationPanel::createLabelSection() {}
void SegmentationPanel::createActionSection() {}

void SegmentationPanel::updateToolOptions()
{
    bool isBrushTool = (impl_->currentTool == services::SegmentationTool::Brush ||
                        impl_->currentTool == services::SegmentationTool::Eraser);
    bool isPolygonTool = (impl_->currentTool == services::SegmentationTool::Polygon ||
                          impl_->currentTool == services::SegmentationTool::SmartScissors);

    impl_->brushOptionsWidget->setVisible(isBrushTool);
    impl_->polygonActionsWidget->setVisible(isPolygonTool);
}

services::SegmentationTool SegmentationPanel::currentTool() const
{
    return impl_->currentTool;
}

int SegmentationPanel::brushSize() const
{
    return impl_->currentBrushSize;
}

services::BrushShape SegmentationPanel::brushShape() const
{
    return impl_->currentBrushShape;
}

uint8_t SegmentationPanel::activeLabel() const
{
    return impl_->currentLabelId;
}

services::LabelColor SegmentationPanel::labelColor() const
{
    return impl_->currentColor;
}

void SegmentationPanel::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
}

void SegmentationPanel::resetToDefaults()
{
    impl_->toolGroup->setExclusive(false);
    for (auto* button : impl_->toolGroup->buttons()) {
        button->setChecked(false);
    }
    impl_->toolGroup->setExclusive(true);

    impl_->currentTool = services::SegmentationTool::None;
    impl_->currentBrushSize = 5;
    impl_->currentBrushShape = services::BrushShape::Circle;
    impl_->currentLabelId = 1;
    impl_->currentColor = services::LabelColorPalette::getColor(1);

    impl_->brushSizeSlider->setValue(impl_->currentBrushSize);
    impl_->brushShapeCombo->setCurrentIndex(0);
    impl_->labelCombo->setCurrentIndex(0);
    impl_->updateColorButtonStyle(impl_->colorButton, impl_->currentColor);

    updateToolOptions();
}

void SegmentationPanel::onToolButtonClicked(int toolId)
{
    auto tool = static_cast<services::SegmentationTool>(toolId);

    // Toggle off if same tool clicked
    if (impl_->currentTool == tool) {
        impl_->toolGroup->setExclusive(false);
        impl_->toolGroup->checkedButton()->setChecked(false);
        impl_->toolGroup->setExclusive(true);
        impl_->currentTool = services::SegmentationTool::None;
    } else {
        impl_->currentTool = tool;
    }

    updateToolOptions();
    emit toolChanged(impl_->currentTool);
}

void SegmentationPanel::onBrushSizeChanged(int size)
{
    impl_->currentBrushSize = size;
    emit brushSizeChanged(size);
}

void SegmentationPanel::onBrushShapeChanged(int index)
{
    impl_->currentBrushShape = static_cast<services::BrushShape>(
        impl_->brushShapeCombo->itemData(index).toInt());
    emit brushShapeChanged(impl_->currentBrushShape);
}

void SegmentationPanel::onLabelChanged(int index)
{
    impl_->currentLabelId = static_cast<uint8_t>(
        impl_->labelCombo->itemData(index).toInt());
    impl_->currentColor = services::LabelColorPalette::getColor(impl_->currentLabelId);
    impl_->updateColorButtonStyle(impl_->colorButton, impl_->currentColor);
    emit activeLabelChanged(impl_->currentLabelId);
    emit labelColorChanged(impl_->currentColor);
}

void SegmentationPanel::onColorButtonClicked()
{
    auto rgba = impl_->currentColor.toRGBA8();
    QColor initial(rgba[0], rgba[1], rgba[2], rgba[3]);

    QColor color = QColorDialog::getColor(initial, this, tr("Select Label Color"),
                                          QColorDialog::ShowAlphaChannel);
    if (color.isValid()) {
        impl_->currentColor = services::LabelColor::fromRGBA8(
            static_cast<uint8_t>(color.red()),
            static_cast<uint8_t>(color.green()),
            static_cast<uint8_t>(color.blue()),
            static_cast<uint8_t>(color.alpha()));
        impl_->updateColorButtonStyle(impl_->colorButton, impl_->currentColor);
        emit labelColorChanged(impl_->currentColor);
    }
}

void SegmentationPanel::onClearAllClicked()
{
    emit clearAllRequested();
}

void SegmentationPanel::onUndoClicked()
{
    emit undoRequested();
}

void SegmentationPanel::onCompleteClicked()
{
    emit completeRequested();
}

} // namespace dicom_viewer::ui

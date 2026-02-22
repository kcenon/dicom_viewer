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
#include <QDoubleSpinBox>
#include <QCheckBox>

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
    QToolButton* levelTracingButton = nullptr;

    // Centerline tracing tool
    QToolButton* centerlineButton = nullptr;

    // Centerline options
    QWidget* centerlineOptionsWidget = nullptr;
    QDoubleSpinBox* centerlineRadiusSpinBox = nullptr;
    QCheckBox* centerlineAutoRadiusCheck = nullptr;
    QPushButton* centerlineConfirmButton = nullptr;
    QPushButton* centerlineCancelButton = nullptr;

    // Post-processing buttons
    QPushButton* hollowButton = nullptr;
    QPushButton* smoothButton = nullptr;

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

    // Command stack undo/redo buttons
    QPushButton* undoCommandButton = nullptr;
    QPushButton* redoCommandButton = nullptr;

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

    impl_->levelTracingButton = new QToolButton();
    impl_->levelTracingButton->setText(tr("Level\nTracing"));
    impl_->levelTracingButton->setCheckable(true);
    impl_->levelTracingButton->setMinimumSize(60, 40);
    impl_->levelTracingButton->setToolTip(tr("Trace contour at intensity boundary"));
    impl_->toolGroup->addButton(impl_->levelTracingButton, static_cast<int>(services::SegmentationTool::LevelTracing));

    impl_->centerlineButton = new QToolButton();
    impl_->centerlineButton->setText(tr("Centerline"));
    impl_->centerlineButton->setCheckable(true);
    impl_->centerlineButton->setMinimumSize(60, 40);
    impl_->centerlineButton->setToolTip(tr("Trace vessel centerline between two points"));
    impl_->toolGroup->addButton(impl_->centerlineButton, static_cast<int>(services::SegmentationTool::CenterlineTracing));

    toolsLayout->addWidget(impl_->brushButton, 0, 0);
    toolsLayout->addWidget(impl_->eraserButton, 0, 1);
    toolsLayout->addWidget(impl_->fillButton, 0, 2);
    toolsLayout->addWidget(impl_->freehandButton, 1, 0);
    toolsLayout->addWidget(impl_->polygonButton, 1, 1);
    toolsLayout->addWidget(impl_->smartScissorsButton, 1, 2);
    toolsLayout->addWidget(impl_->levelTracingButton, 2, 0);
    toolsLayout->addWidget(impl_->centerlineButton, 2, 1);
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

    // Centerline options
    impl_->centerlineOptionsWidget = new QGroupBox(tr("Centerline Options"));
    auto centerlineLayout = new QVBoxLayout(static_cast<QGroupBox*>(impl_->centerlineOptionsWidget));

    auto radiusLayout = new QHBoxLayout();
    radiusLayout->addWidget(new QLabel(tr("Radius (mm):")));
    impl_->centerlineRadiusSpinBox = new QDoubleSpinBox();
    impl_->centerlineRadiusSpinBox->setRange(0.5, 30.0);
    impl_->centerlineRadiusSpinBox->setValue(5.0);
    impl_->centerlineRadiusSpinBox->setSingleStep(0.5);
    impl_->centerlineRadiusSpinBox->setDecimals(1);
    radiusLayout->addWidget(impl_->centerlineRadiusSpinBox);
    centerlineLayout->addLayout(radiusLayout);

    impl_->centerlineAutoRadiusCheck = new QCheckBox(tr("Auto radius from image"));
    impl_->centerlineAutoRadiusCheck->setChecked(true);
    impl_->centerlineAutoRadiusCheck->setToolTip(tr("Estimate vessel radius from image gradients"));
    centerlineLayout->addWidget(impl_->centerlineAutoRadiusCheck);

    auto confirmLayout = new QHBoxLayout();
    impl_->centerlineConfirmButton = new QPushButton(tr("Confirm"));
    impl_->centerlineConfirmButton->setToolTip(tr("Apply centerline mask to segmentation"));
    impl_->centerlineCancelButton = new QPushButton(tr("Cancel"));
    impl_->centerlineCancelButton->setToolTip(tr("Discard current centerline"));
    confirmLayout->addWidget(impl_->centerlineConfirmButton);
    confirmLayout->addWidget(impl_->centerlineCancelButton);
    centerlineLayout->addLayout(confirmLayout);

    mainLayout->addWidget(impl_->centerlineOptionsWidget);

    // Post-processing actions
    auto postProcGroup = new QGroupBox(tr("Post-Processing"));
    auto postProcLayout = new QHBoxLayout(postProcGroup);
    impl_->hollowButton = new QPushButton(tr("Hollow"));
    impl_->hollowButton->setToolTip(tr("Create hollow shell from solid mask"));
    impl_->smoothButton = new QPushButton(tr("Smooth"));
    impl_->smoothButton->setToolTip(tr("Smooth mask boundary (volume-preserving)"));
    postProcLayout->addWidget(impl_->hollowButton);
    postProcLayout->addWidget(impl_->smoothButton);
    mainLayout->addWidget(postProcGroup);

    // History actions (Undo/Redo for command stack)
    auto historyGroup = new QGroupBox(tr("History"));
    auto historyLayout = new QHBoxLayout(historyGroup);
    impl_->undoCommandButton = new QPushButton(tr("Undo"));
    impl_->undoCommandButton->setToolTip(tr("Undo last segmentation operation (Ctrl+Z)"));
    impl_->undoCommandButton->setEnabled(false);
    impl_->redoCommandButton = new QPushButton(tr("Redo"));
    impl_->redoCommandButton->setToolTip(tr("Redo last undone operation (Ctrl+Y)"));
    impl_->redoCommandButton->setEnabled(false);
    historyLayout->addWidget(impl_->undoCommandButton);
    historyLayout->addWidget(impl_->redoCommandButton);
    mainLayout->addWidget(historyGroup);

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

    // Command stack undo/redo
    connect(impl_->undoCommandButton, &QPushButton::clicked,
            this, &SegmentationPanel::undoCommandRequested);
    connect(impl_->redoCommandButton, &QPushButton::clicked,
            this, &SegmentationPanel::redoCommandRequested);

    // Centerline options
    connect(impl_->centerlineRadiusSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (!impl_->centerlineAutoRadiusCheck->isChecked()) {
                    emit centerlineRadiusChanged(value);
                }
            });
    connect(impl_->centerlineAutoRadiusCheck, &QCheckBox::toggled,
            this, [this](bool autoRadius) {
                impl_->centerlineRadiusSpinBox->setEnabled(!autoRadius);
                emit centerlineRadiusChanged(autoRadius ? -1.0 : impl_->centerlineRadiusSpinBox->value());
            });
    connect(impl_->centerlineConfirmButton, &QPushButton::clicked,
            this, &SegmentationPanel::centerlineConfirmRequested);
    connect(impl_->centerlineCancelButton, &QPushButton::clicked,
            this, &SegmentationPanel::centerlineCancelRequested);

    // Post-processing
    connect(impl_->hollowButton, &QPushButton::clicked,
            this, &SegmentationPanel::hollowRequested);
    connect(impl_->smoothButton, &QPushButton::clicked,
            this, &SegmentationPanel::smoothRequested);
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
    bool isCenterlineTool = (impl_->currentTool == services::SegmentationTool::CenterlineTracing);

    impl_->brushOptionsWidget->setVisible(isBrushTool);
    impl_->polygonActionsWidget->setVisible(isPolygonTool);
    impl_->centerlineOptionsWidget->setVisible(isCenterlineTool);
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

void SegmentationPanel::setUndoRedoEnabled(bool canUndo, bool canRedo)
{
    impl_->undoCommandButton->setEnabled(canUndo);
    impl_->redoCommandButton->setEnabled(canRedo);
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

#include "ui/quantification_window.hpp"
#include "ui/widgets/flow_graph_widget.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPrinter>
#include <QButtonGroup>
#include <QFrame>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QToolButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

namespace {

QString parameterName(MeasurementParameter param)
{
    switch (param) {
        case MeasurementParameter::FlowRate:           return "Flow Rate";
        case MeasurementParameter::PeakVelocity:       return "Peak Velocity";
        case MeasurementParameter::MeanVelocity:       return "Mean Velocity";
        case MeasurementParameter::KineticEnergy:      return "Kinetic Energy";
        case MeasurementParameter::RegurgitantFraction: return "Regurgitant Fraction";
        case MeasurementParameter::StrokeVolume:       return "Stroke Volume";
    }
    return {};
}

QString parameterUnit(MeasurementParameter param)
{
    switch (param) {
        case MeasurementParameter::FlowRate:           return "mL/s";
        case MeasurementParameter::PeakVelocity:       return "cm/s";
        case MeasurementParameter::MeanVelocity:       return "cm/s";
        case MeasurementParameter::KineticEnergy:      return "mJ";
        case MeasurementParameter::RegurgitantFraction: return "%";
        case MeasurementParameter::StrokeVolume:       return "mL";
    }
    return {};
}

QString volumeParameterName(VolumeParameter param)
{
    switch (param) {
        case VolumeParameter::TotalKE:      return "Total KE";
        case VolumeParameter::VortexVolume: return "Vortex Volume";
        case VolumeParameter::EnergyLoss:   return "Energy Loss";
        case VolumeParameter::MeanWSS:      return "Mean WSS";
        case VolumeParameter::PeakWSS:      return "Peak WSS";
    }
    return {};
}

QIcon colorSwatchIcon(const QColor& color)
{
    QPixmap pix(12, 12);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 10, 10);
    p.end();
    return QIcon(pix);
}

/// Predefined color palette for measurement planes (max 5)
const QColor kPlaneColors[] = {
    QColor(0xE7, 0x4C, 0x3C),  // Red
    QColor(0x34, 0x98, 0xDB),  // Blue
    QColor(0x27, 0xAE, 0x60),  // Green
    QColor(0xE6, 0x7E, 0x22),  // Orange
    QColor(0x9B, 0x59, 0xB6),  // Purple
};
constexpr int kPlaneColorCount = 5;

} // anonymous namespace

struct PlaneInfo {
    QString name;
    QColor color;
    PlanePosition position;
};

class QuantificationWindow::Impl {
public:
    QTabWidget* tabWidget = nullptr;
    QSplitter* mainSplitter = nullptr;

    // Left panel
    QWidget* leftPanel = nullptr;
    QTableWidget* statsTable = nullptr;
    QPushButton* copySummaryBtn = nullptr;

    // Parameter checkboxes
    QCheckBox* flowRateCheck = nullptr;
    QCheckBox* peakVelocityCheck = nullptr;
    QCheckBox* meanVelocityCheck = nullptr;
    QCheckBox* kineticEnergyCheck = nullptr;
    QCheckBox* regurgitantFractionCheck = nullptr;
    QCheckBox* strokeVolumeCheck = nullptr;

    // Right panel
    QWidget* rightPanel = nullptr;
    FlowGraphWidget* graphWidget = nullptr;
    QPushButton* copyChartDataBtn = nullptr;
    QPushButton* copyChartImageBtn = nullptr;
    QPushButton* exportCsvBtn = nullptr;
    QPushButton* exportPdfBtn = nullptr;
    QPushButton* flipFlowBtn = nullptr;

    // Plane selector
    QComboBox* planeCombo = nullptr;
    QPushButton* addPlaneBtn = nullptr;
    QPushButton* removePlaneBtn = nullptr;
    int nextPlaneNumber = 1;

    // 3D Volume tab
    QWidget* volumePanel = nullptr;
    QTableWidget* volumeTable = nullptr;

    // Contour editing toolbar
    QWidget* editToolbar = nullptr;
    QToolButton* brushBtn = nullptr;
    QToolButton* eraserBtn = nullptr;
    QSpinBox* brushSizeSpinBox = nullptr;
    QPushButton* editUndoBtn = nullptr;
    QPushButton* editRedoBtn = nullptr;
    QFrame* contourViewArea = nullptr;

    // Data
    std::vector<QuantificationRow> rows;
    std::vector<VolumeStatRow> volumeRows;
    std::vector<PlaneInfo> planes;
    bool flowFlipped = false;

    QCheckBox* checkBoxFor(MeasurementParameter param)
    {
        switch (param) {
            case MeasurementParameter::FlowRate:           return flowRateCheck;
            case MeasurementParameter::PeakVelocity:       return peakVelocityCheck;
            case MeasurementParameter::MeanVelocity:       return meanVelocityCheck;
            case MeasurementParameter::KineticEnergy:      return kineticEnergyCheck;
            case MeasurementParameter::RegurgitantFraction: return regurgitantFractionCheck;
            case MeasurementParameter::StrokeVolume:       return strokeVolumeCheck;
        }
        return nullptr;
    }
};

QuantificationWindow::QuantificationWindow(QWidget* parent)
    : QMainWindow(parent)
    , impl_(std::make_unique<Impl>())
{
    setWindowTitle(tr("Quantification"));
    setMinimumSize(800, 500);
    resize(1000, 600);

    setupUI();
    setupConnections();
}

QuantificationWindow::~QuantificationWindow() = default;

void QuantificationWindow::setupUI()
{
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    impl_->tabWidget = new QTabWidget(centralWidget);
    mainLayout->addWidget(impl_->tabWidget);

    // === 2D Plane tab ===
    auto* planeTab = new QWidget();
    auto* planeLayout = new QHBoxLayout(planeTab);
    planeLayout->setContentsMargins(0, 0, 0, 0);

    impl_->mainSplitter = new QSplitter(Qt::Horizontal, planeTab);
    planeLayout->addWidget(impl_->mainSplitter);

    // --- Left panel ---
    impl_->leftPanel = new QWidget(impl_->mainSplitter);
    auto* leftLayout = new QVBoxLayout(impl_->leftPanel);

    // Parameter checkboxes group
    auto* paramGroup = new QGroupBox(tr("Parameters"), impl_->leftPanel);
    auto* paramLayout = new QVBoxLayout(paramGroup);

    impl_->flowRateCheck = new QCheckBox(tr("Flow Rate"), paramGroup);
    impl_->peakVelocityCheck = new QCheckBox(tr("Peak Velocity"), paramGroup);
    impl_->meanVelocityCheck = new QCheckBox(tr("Mean Velocity"), paramGroup);
    impl_->kineticEnergyCheck = new QCheckBox(tr("Kinetic Energy"), paramGroup);
    impl_->regurgitantFractionCheck = new QCheckBox(tr("Regurgitant Fraction"), paramGroup);
    impl_->strokeVolumeCheck = new QCheckBox(tr("Stroke Volume"), paramGroup);

    // All checked by default
    impl_->flowRateCheck->setChecked(true);
    impl_->peakVelocityCheck->setChecked(true);
    impl_->meanVelocityCheck->setChecked(true);
    impl_->kineticEnergyCheck->setChecked(true);
    impl_->regurgitantFractionCheck->setChecked(true);
    impl_->strokeVolumeCheck->setChecked(true);

    paramLayout->addWidget(impl_->flowRateCheck);
    paramLayout->addWidget(impl_->peakVelocityCheck);
    paramLayout->addWidget(impl_->meanVelocityCheck);
    paramLayout->addWidget(impl_->kineticEnergyCheck);
    paramLayout->addWidget(impl_->regurgitantFractionCheck);
    paramLayout->addWidget(impl_->strokeVolumeCheck);

    leftLayout->addWidget(paramGroup);

    // Plane selector
    auto* planeGroup = new QGroupBox(tr("Measurement Plane"), impl_->leftPanel);
    auto* planeGroupLayout = new QVBoxLayout(planeGroup);
    impl_->planeCombo = new QComboBox(planeGroup);
    impl_->planeCombo->setPlaceholderText(tr("No planes"));
    planeGroupLayout->addWidget(impl_->planeCombo);

    auto* planeBtnLayout = new QHBoxLayout();
    impl_->addPlaneBtn = new QPushButton(tr("Add Plane"), planeGroup);
    impl_->removePlaneBtn = new QPushButton(tr("Remove Plane"), planeGroup);
    impl_->removePlaneBtn->setEnabled(false);
    planeBtnLayout->addWidget(impl_->addPlaneBtn);
    planeBtnLayout->addWidget(impl_->removePlaneBtn);
    planeGroupLayout->addLayout(planeBtnLayout);

    leftLayout->addWidget(planeGroup);

    // Statistics table
    impl_->statsTable = new QTableWidget(0, 5, impl_->leftPanel);
    impl_->statsTable->setHorizontalHeaderLabels(
        {tr("Parameter"), tr("Mean"), tr("Std Dev"), tr("Max"), tr("Min")});
    impl_->statsTable->horizontalHeader()->setStretchLastSection(true);
    impl_->statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    impl_->statsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->statsTable->verticalHeader()->setVisible(false);

    leftLayout->addWidget(impl_->statsTable, 1);

    // Copy Summary button
    impl_->copySummaryBtn = new QPushButton(tr("Copy Summary"), impl_->leftPanel);
    leftLayout->addWidget(impl_->copySummaryBtn);

    // Export buttons row
    auto* exportBtnLayout = new QHBoxLayout();
    impl_->exportCsvBtn = new QPushButton(tr("Export CSV..."), impl_->leftPanel);
    impl_->exportPdfBtn = new QPushButton(tr("Export PDF..."), impl_->leftPanel);
    exportBtnLayout->addWidget(impl_->exportCsvBtn);
    exportBtnLayout->addWidget(impl_->exportPdfBtn);
    leftLayout->addLayout(exportBtnLayout);

    impl_->mainSplitter->addWidget(impl_->leftPanel);

    // --- Right panel (flow graph + copy buttons) ---
    impl_->rightPanel = new QWidget(impl_->mainSplitter);
    auto* rightLayout = new QVBoxLayout(impl_->rightPanel);

    impl_->graphWidget = new FlowGraphWidget(impl_->rightPanel);
    impl_->graphWidget->setXAxisLabel(tr("Cardiac Phase"));
    impl_->graphWidget->setYAxisLabel(tr("Flow Rate (mL/s)"));
    rightLayout->addWidget(impl_->graphWidget, 1);

    // Contour editing toolbar
    impl_->editToolbar = new QWidget(impl_->rightPanel);
    auto* editLayout = new QHBoxLayout(impl_->editToolbar);
    editLayout->setContentsMargins(0, 4, 0, 4);

    impl_->brushBtn = new QToolButton(impl_->editToolbar);
    impl_->brushBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    impl_->brushBtn->setText(tr("Brush"));
    impl_->brushBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    impl_->brushBtn->setToolTip(tr("Brush - add to contour"));
    impl_->brushBtn->setCheckable(true);
    impl_->brushBtn->setChecked(true);

    impl_->eraserBtn = new QToolButton(impl_->editToolbar);
    impl_->eraserBtn->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
    impl_->eraserBtn->setText(tr("Eraser"));
    impl_->eraserBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    impl_->eraserBtn->setToolTip(tr("Eraser - remove from contour"));
    impl_->eraserBtn->setCheckable(true);

    auto* toolGroup = new QButtonGroup(impl_->editToolbar);
    toolGroup->addButton(impl_->brushBtn);
    toolGroup->addButton(impl_->eraserBtn);
    toolGroup->setExclusive(true);

    impl_->brushSizeSpinBox = new QSpinBox(impl_->editToolbar);
    impl_->brushSizeSpinBox->setRange(1, 20);
    impl_->brushSizeSpinBox->setValue(5);
    impl_->brushSizeSpinBox->setPrefix(tr("Size: "));
    impl_->brushSizeSpinBox->setToolTip(tr("Brush radius in pixels"));

    impl_->editUndoBtn = new QPushButton(tr("Undo"), impl_->editToolbar);
    impl_->editUndoBtn->setEnabled(false);
    impl_->editRedoBtn = new QPushButton(tr("Redo"), impl_->editToolbar);
    impl_->editRedoBtn->setEnabled(false);

    editLayout->addWidget(impl_->brushBtn);
    editLayout->addWidget(impl_->eraserBtn);
    editLayout->addWidget(impl_->brushSizeSpinBox);
    editLayout->addStretch();
    editLayout->addWidget(impl_->editUndoBtn);
    editLayout->addWidget(impl_->editRedoBtn);

    impl_->editToolbar->setEnabled(false);
    rightLayout->addWidget(impl_->editToolbar);

    // Contour view area (placeholder for future VTK widget)
    impl_->contourViewArea = new QFrame(impl_->rightPanel);
    impl_->contourViewArea->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    impl_->contourViewArea->setMinimumHeight(80);
    auto* contourPlaceholder = new QLabel(
        tr("Contour editing area - connect a measurement plane to enable"),
        impl_->contourViewArea);
    contourPlaceholder->setAlignment(Qt::AlignCenter);
    contourPlaceholder->setStyleSheet("color: gray;");
    auto* contourLayout = new QVBoxLayout(impl_->contourViewArea);
    contourLayout->addWidget(contourPlaceholder);
    rightLayout->addWidget(impl_->contourViewArea, 1);

    // Flow direction flip button
    impl_->flipFlowBtn = new QPushButton(tr("Flip Flow Direction"), impl_->rightPanel);
    impl_->flipFlowBtn->setCheckable(true);
    impl_->flipFlowBtn->setToolTip(tr("Negate flow rate values for reversed vessel orientation"));

    auto* chartBtnLayout = new QHBoxLayout();
    chartBtnLayout->addWidget(impl_->flipFlowBtn);
    chartBtnLayout->addStretch();
    impl_->copyChartDataBtn = new QPushButton(tr("Copy Chart Data"), impl_->rightPanel);
    impl_->copyChartImageBtn = new QPushButton(tr("Copy Chart Image"), impl_->rightPanel);
    chartBtnLayout->addWidget(impl_->copyChartDataBtn);
    chartBtnLayout->addWidget(impl_->copyChartImageBtn);
    rightLayout->addLayout(chartBtnLayout);

    impl_->mainSplitter->addWidget(impl_->rightPanel);

    // 40% left, 60% right
    impl_->mainSplitter->setSizes({400, 600});

    impl_->tabWidget->addTab(planeTab, tr("2D Plane"));

    // === 3D Volume tab ===
    impl_->volumePanel = new QWidget();
    auto* volumeLayout = new QVBoxLayout(impl_->volumePanel);

    auto* volumeLabel = new QLabel(tr("Volume Measurements"), impl_->volumePanel);
    QFont labelFont = volumeLabel->font();
    labelFont.setBold(true);
    volumeLabel->setFont(labelFont);
    volumeLayout->addWidget(volumeLabel);

    impl_->volumeTable = new QTableWidget(0, 3, impl_->volumePanel);
    impl_->volumeTable->setHorizontalHeaderLabels(
        {tr("Parameter"), tr("Value"), tr("Unit")});
    impl_->volumeTable->horizontalHeader()->setStretchLastSection(true);
    impl_->volumeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    impl_->volumeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->volumeTable->verticalHeader()->setVisible(false);
    volumeLayout->addWidget(impl_->volumeTable, 1);

    auto* volumePlaceholder = new QLabel(tr("3D visualization area"), impl_->volumePanel);
    volumePlaceholder->setAlignment(Qt::AlignCenter);
    volumePlaceholder->setStyleSheet(
        "border: 1px dashed gray; color: gray; min-height: 200px;");
    volumeLayout->addWidget(volumePlaceholder, 1);

    impl_->tabWidget->addTab(impl_->volumePanel, tr("3D Volume"));
}

void QuantificationWindow::setupConnections()
{
    // Copy Summary button
    connect(impl_->copySummaryBtn, &QPushButton::clicked, this, [this]() {
        QString text = summaryText();
        QApplication::clipboard()->setText(text);
        emit summaryCopied(text);
    });

    // Copy Chart Data button
    connect(impl_->copyChartDataBtn, &QPushButton::clicked, this, [this]() {
        QString text = impl_->graphWidget->chartDataText();
        QApplication::clipboard()->setText(text);
    });

    // Copy Chart Image button
    connect(impl_->copyChartImageBtn, &QPushButton::clicked, this, [this]() {
        QPixmap image = impl_->graphWidget->chartImage();
        QApplication::clipboard()->setPixmap(image);
    });

    // Export buttons
    connect(impl_->exportCsvBtn, &QPushButton::clicked, this, &QuantificationWindow::exportCsv);
    connect(impl_->exportPdfBtn, &QPushButton::clicked, this, &QuantificationWindow::exportPdf);

    // Flow direction flip button
    connect(impl_->flipFlowBtn, &QPushButton::toggled, this, [this](bool checked) {
        setFlowDirectionFlipped(checked);
    });

    // Graph phase click → propagate as phase change request
    connect(impl_->graphWidget, &FlowGraphWidget::phaseClicked, this,
            &QuantificationWindow::phaseChangeRequested);

    // Tab widget
    connect(impl_->tabWidget, &QTabWidget::currentChanged,
            this, &QuantificationWindow::activeTabChanged);

    // Plane selector
    connect(impl_->planeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QuantificationWindow::activePlaneChanged);

    // Add Plane button
    connect(impl_->addPlaneBtn, &QPushButton::clicked, this, [this]() {
        int colorIdx = static_cast<int>(impl_->planes.size()) % kPlaneColorCount;
        QString name = QString("Plane %1").arg(impl_->nextPlaneNumber++);
        addPlane(name, kPlaneColors[colorIdx]);
        updatePlaneButtons();
    });

    // Remove Plane button
    connect(impl_->removePlaneBtn, &QPushButton::clicked, this, [this]() {
        int idx = impl_->planeCombo->currentIndex();
        if (idx >= 0) {
            removePlane(idx);
            updatePlaneButtons();
        }
    });

    // Parameter checkboxes → signal + table update
    auto connectCheck = [this](QCheckBox* box, MeasurementParameter param) {
        connect(box, &QCheckBox::toggled, this, [this, param](bool checked) {
            emit parameterToggled(param, checked);
            updateTable();
        });
    };

    connectCheck(impl_->flowRateCheck, MeasurementParameter::FlowRate);
    connectCheck(impl_->peakVelocityCheck, MeasurementParameter::PeakVelocity);
    connectCheck(impl_->meanVelocityCheck, MeasurementParameter::MeanVelocity);
    connectCheck(impl_->kineticEnergyCheck, MeasurementParameter::KineticEnergy);
    connectCheck(impl_->regurgitantFractionCheck, MeasurementParameter::RegurgitantFraction);
    connectCheck(impl_->strokeVolumeCheck, MeasurementParameter::StrokeVolume);

    // Contour editing tool toggle
    connect(impl_->brushBtn, &QToolButton::clicked, this, [this]() {
        emit editToolChanged(true);
    });
    connect(impl_->eraserBtn, &QToolButton::clicked, this, [this]() {
        emit editToolChanged(false);
    });

    // Brush size
    connect(impl_->brushSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &QuantificationWindow::editBrushSizeChanged);

    // Undo/Redo
    connect(impl_->editUndoBtn, &QPushButton::clicked,
            this, &QuantificationWindow::contourUndoRequested);
    connect(impl_->editRedoBtn, &QPushButton::clicked,
            this, &QuantificationWindow::contourRedoRequested);
}

void QuantificationWindow::setStatistics(const std::vector<QuantificationRow>& rows)
{
    impl_->rows = rows;
    updateTable();
}

std::vector<QuantificationRow> QuantificationWindow::getStatistics() const
{
    return impl_->rows;
}

void QuantificationWindow::clearStatistics()
{
    impl_->rows.clear();
    updateTable();
}

int QuantificationWindow::rowCount() const
{
    return impl_->statsTable->rowCount();
}

bool QuantificationWindow::isParameterEnabled(MeasurementParameter param) const
{
    auto* box = const_cast<Impl*>(impl_.get())->checkBoxFor(param);
    return box ? box->isChecked() : false;
}

void QuantificationWindow::setParameterEnabled(MeasurementParameter param, bool enabled)
{
    auto* box = impl_->checkBoxFor(param);
    if (box) {
        box->setChecked(enabled);
    }
}

QString QuantificationWindow::summaryText() const
{
    QString text;
    text += "Parameter\tMean\tStd Dev\tMax\tMin\n";

    for (const auto& row : impl_->rows) {
        if (!const_cast<Impl*>(impl_.get())->checkBoxFor(row.parameter)->isChecked()) {
            continue;
        }
        QString unit = parameterUnit(row.parameter);
        text += QString("%1\t%2 %3\t%4 %5\t%6 %7\t%8 %9\n")
            .arg(parameterName(row.parameter))
            .arg(row.mean, 0, 'f', 2).arg(unit)
            .arg(row.stdDev, 0, 'f', 2).arg(unit)
            .arg(row.max, 0, 'f', 2).arg(unit)
            .arg(row.min, 0, 'f', 2).arg(unit);
    }

    return text;
}

FlowGraphWidget* QuantificationWindow::graphWidget() const
{
    return impl_->graphWidget;
}

void QuantificationWindow::exportCsv()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export CSV"), QString(),
        tr("CSV Files (*.csv);;All Files (*)"));
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);

    // Statistics section
    out << "Parameter,Mean,Std Dev,Max,Min,Unit\n";
    for (const auto& row : impl_->rows) {
        if (!impl_->checkBoxFor(row.parameter)->isChecked()) continue;
        out << parameterName(row.parameter) << ","
            << QString::number(row.mean, 'f', 3) << ","
            << QString::number(row.stdDev, 'f', 3) << ","
            << QString::number(row.max, 'f', 3) << ","
            << QString::number(row.min, 'f', 3) << ","
            << parameterUnit(row.parameter) << "\n";
    }

    // Time-series section (if graph has data)
    if (impl_->graphWidget->seriesCount() > 0) {
        out << "\n";
        // Header
        out << "Phase";
        for (int i = 0; i < impl_->graphWidget->seriesCount(); ++i) {
            out << "," << impl_->graphWidget->series(i).planeName;
        }
        out << "\n";

        // Find max phases
        int maxPhases = 0;
        for (int i = 0; i < impl_->graphWidget->seriesCount(); ++i) {
            int sz = static_cast<int>(impl_->graphWidget->series(i).values.size());
            maxPhases = std::max(maxPhases, sz);
        }

        // Data rows
        for (int p = 0; p < maxPhases; ++p) {
            out << (p + 1);
            for (int i = 0; i < impl_->graphWidget->seriesCount(); ++i) {
                out << ",";
                auto s = impl_->graphWidget->series(i);
                if (p < static_cast<int>(s.values.size())) {
                    out << QString::number(s.values[p], 'f', 3);
                }
            }
            out << "\n";
        }
    }
}

void QuantificationWindow::setFlowDirectionFlipped(bool flipped)
{
    if (impl_->flowFlipped == flipped) return;
    impl_->flowFlipped = flipped;
    impl_->flipFlowBtn->setChecked(flipped);
    applyFlowDirectionToGraph();
    emit flowDirectionFlipped(flipped);
}

bool QuantificationWindow::isFlowDirectionFlipped() const
{
    return impl_->flowFlipped;
}

void QuantificationWindow::applyFlowDirectionToGraph()
{
    // Re-add all series with negated values if flipped
    int count = impl_->graphWidget->seriesCount();
    if (count == 0) return;

    std::vector<FlowTimeSeries> series;
    for (int i = 0; i < count; ++i) {
        series.push_back(impl_->graphWidget->series(i));
    }

    impl_->graphWidget->clearSeries();

    for (auto& s : series) {
        for (auto& v : s.values) {
            v = -v;
        }
        impl_->graphWidget->addSeries(s);
    }
}

void QuantificationWindow::updateTable()
{
    impl_->statsTable->setRowCount(0);

    for (const auto& row : impl_->rows) {
        auto* box = impl_->checkBoxFor(row.parameter);
        if (box && !box->isChecked()) {
            continue;
        }

        int r = impl_->statsTable->rowCount();
        impl_->statsTable->insertRow(r);

        QString unit = parameterUnit(row.parameter);
        impl_->statsTable->setItem(r, 0,
            new QTableWidgetItem(parameterName(row.parameter)));
        impl_->statsTable->setItem(r, 1,
            new QTableWidgetItem(QString("%1 %2").arg(row.mean, 0, 'f', 2).arg(unit)));
        impl_->statsTable->setItem(r, 2,
            new QTableWidgetItem(QString("%1 %2").arg(row.stdDev, 0, 'f', 2).arg(unit)));
        impl_->statsTable->setItem(r, 3,
            new QTableWidgetItem(QString("%1 %2").arg(row.max, 0, 'f', 2).arg(unit)));
        impl_->statsTable->setItem(r, 4,
            new QTableWidgetItem(QString("%1 %2").arg(row.min, 0, 'f', 2).arg(unit)));
    }
}

void QuantificationWindow::exportPdf()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export PDF"), QString(),
        tr("PDF Files (*.pdf);;All Files (*)"));
    if (filePath.isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));

    QPainter painter;
    if (!painter.begin(&printer)) return;

    renderReport(painter, printer.pageLayout().paintRectPixels(printer.resolution()));
    painter.end();
}

void QuantificationWindow::renderReport(QPainter& painter, const QRectF& pageRect) const
{
    const double w = pageRect.width();
    double y = pageRect.top();

    // Title
    QFont titleFont("Helvetica", 16, QFont::Bold);
    painter.setFont(titleFont);
    painter.drawText(QRectF(pageRect.left(), y, w, 40), Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Quantification Report"));
    y += 50;

    // Separator line
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawLine(QPointF(pageRect.left(), y), QPointF(pageRect.right(), y));
    y += 15;

    // Statistics table
    QFont headerFont("Helvetica", 10, QFont::Bold);
    QFont bodyFont("Helvetica", 10);

    const int colCount = 5;
    const double colWidth = w / colCount;
    const double rowHeight = 22;

    // Table header
    painter.setFont(headerFont);
    const QString headers[] = {
        tr("Parameter"), tr("Mean"), tr("Std Dev"), tr("Max"), tr("Min")};
    for (int c = 0; c < colCount; ++c) {
        painter.drawText(
            QRectF(pageRect.left() + c * colWidth, y, colWidth, rowHeight),
            Qt::AlignLeft | Qt::AlignVCenter, headers[c]);
    }
    y += rowHeight;

    // Header underline
    painter.drawLine(QPointF(pageRect.left(), y), QPointF(pageRect.right(), y));
    y += 4;

    // Data rows
    painter.setFont(bodyFont);
    for (const auto& row : impl_->rows) {
        auto* box = const_cast<Impl*>(impl_.get())->checkBoxFor(row.parameter);
        if (box && !box->isChecked()) continue;

        QString unit = parameterUnit(row.parameter);
        QString vals[] = {
            parameterName(row.parameter),
            QString("%1 %2").arg(row.mean, 0, 'f', 2).arg(unit),
            QString("%1 %2").arg(row.stdDev, 0, 'f', 2).arg(unit),
            QString("%1 %2").arg(row.max, 0, 'f', 2).arg(unit),
            QString("%1 %2").arg(row.min, 0, 'f', 2).arg(unit),
        };

        for (int c = 0; c < colCount; ++c) {
            painter.drawText(
                QRectF(pageRect.left() + c * colWidth, y, colWidth, rowHeight),
                Qt::AlignLeft | Qt::AlignVCenter, vals[c]);
        }
        y += rowHeight;
    }

    y += 20;

    // Graph image (fill remaining space, max 40% of page height)
    if (impl_->graphWidget && impl_->graphWidget->seriesCount() > 0) {
        double maxGraphHeight = pageRect.height() * 0.4;
        double availableHeight = pageRect.bottom() - y - 10;
        double graphHeight = std::min(maxGraphHeight, availableHeight);
        if (graphHeight > 50) {
            QPixmap chartPix = impl_->graphWidget->chartImage();
            if (!chartPix.isNull()) {
                double aspectRatio = static_cast<double>(chartPix.width()) / chartPix.height();
                double graphWidth = std::min(w, graphHeight * aspectRatio);
                graphHeight = graphWidth / aspectRatio;

                painter.drawPixmap(
                    QRectF(pageRect.left(), y, graphWidth, graphHeight).toRect(),
                    chartPix);
            }
        }
    }
}

void QuantificationWindow::addPlane(const QString& name, const QColor& color)
{
    impl_->planes.push_back({name, color, {}});
    impl_->planeCombo->addItem(colorSwatchIcon(color), name);
    if (impl_->planes.size() == 1) {
        impl_->planeCombo->setCurrentIndex(0);
    }
    updatePlaneButtons();
}

void QuantificationWindow::addPlane(const QString& name, const QColor& color,
                                     const PlanePosition& position)
{
    impl_->planes.push_back({name, color, position});
    impl_->planeCombo->addItem(colorSwatchIcon(color), name);
    if (impl_->planes.size() == 1) {
        impl_->planeCombo->setCurrentIndex(0);
    }
    updatePlaneButtons();
}

void QuantificationWindow::removePlane(int index)
{
    if (index < 0 || index >= static_cast<int>(impl_->planes.size())) return;
    impl_->planes.erase(impl_->planes.begin() + index);
    impl_->planeCombo->removeItem(index);
    updatePlaneButtons();
}

int QuantificationWindow::planeCount() const
{
    return static_cast<int>(impl_->planes.size());
}

int QuantificationWindow::activePlaneIndex() const
{
    return impl_->planeCombo->currentIndex();
}

void QuantificationWindow::setActivePlane(int index)
{
    if (index >= 0 && index < static_cast<int>(impl_->planes.size())) {
        impl_->planeCombo->setCurrentIndex(index);
    }
}

QString QuantificationWindow::planeName(int index) const
{
    if (index < 0 || index >= static_cast<int>(impl_->planes.size())) return {};
    return impl_->planes[index].name;
}

QColor QuantificationWindow::planeColor(int index) const
{
    if (index < 0 || index >= static_cast<int>(impl_->planes.size())) return {};
    return impl_->planes[index].color;
}

PlanePosition QuantificationWindow::planePosition(int index) const
{
    if (index < 0 || index >= static_cast<int>(impl_->planes.size())) return {};
    return impl_->planes[index].position;
}

void QuantificationWindow::setPlanePosition(int index, const PlanePosition& position)
{
    if (index < 0 || index >= static_cast<int>(impl_->planes.size())) return;
    impl_->planes[index].position = position;
    emit planePositionChanged(index);
}

void QuantificationWindow::updatePlaneButtons()
{
    int count = static_cast<int>(impl_->planes.size());
    impl_->addPlaneBtn->setEnabled(count < kMaxPlanes);
    impl_->removePlaneBtn->setEnabled(count > 1);
}

void QuantificationWindow::setVolumeStatistics(const std::vector<VolumeStatRow>& rows)
{
    impl_->volumeRows = rows;
    impl_->volumeTable->setRowCount(0);

    for (const auto& row : rows) {
        int r = impl_->volumeTable->rowCount();
        impl_->volumeTable->insertRow(r);
        impl_->volumeTable->setItem(r, 0,
            new QTableWidgetItem(volumeParameterName(row.parameter)));
        impl_->volumeTable->setItem(r, 1,
            new QTableWidgetItem(QString::number(row.value, 'f', 2)));
        impl_->volumeTable->setItem(r, 2,
            new QTableWidgetItem(row.unit));
    }
}

int QuantificationWindow::volumeRowCount() const
{
    return impl_->volumeTable->rowCount();
}

void QuantificationWindow::clearVolumeStatistics()
{
    impl_->volumeRows.clear();
    impl_->volumeTable->setRowCount(0);
}

int QuantificationWindow::activeTab() const
{
    return impl_->tabWidget->currentIndex();
}

void QuantificationWindow::setActiveTab(int index)
{
    if (index >= 0 && index < impl_->tabWidget->count()) {
        impl_->tabWidget->setCurrentIndex(index);
    }
}

void QuantificationWindow::setEditingEnabled(bool enabled)
{
    impl_->editToolbar->setEnabled(enabled);
}

bool QuantificationWindow::isEditingEnabled() const
{
    return impl_->editToolbar->isEnabled();
}

void QuantificationWindow::setUndoRedoEnabled(bool canUndo, bool canRedo)
{
    impl_->editUndoBtn->setEnabled(canUndo);
    impl_->editRedoBtn->setEnabled(canRedo);
}

int QuantificationWindow::brushSize() const
{
    return impl_->brushSizeSpinBox->value();
}

bool QuantificationWindow::isBrushActive() const
{
    return impl_->brushBtn->isChecked();
}

} // namespace dicom_viewer::ui

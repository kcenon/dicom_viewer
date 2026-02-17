#include "ui/quantification_window.hpp"
#include "ui/widgets/flow_graph_widget.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
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

} // anonymous namespace

class QuantificationWindow::Impl {
public:
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

    // Data
    std::vector<QuantificationRow> rows;

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

    impl_->mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);
    mainLayout->addWidget(impl_->mainSplitter);

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

    impl_->mainSplitter->addWidget(impl_->leftPanel);

    // --- Right panel (flow graph + copy buttons) ---
    impl_->rightPanel = new QWidget(impl_->mainSplitter);
    auto* rightLayout = new QVBoxLayout(impl_->rightPanel);

    impl_->graphWidget = new FlowGraphWidget(impl_->rightPanel);
    impl_->graphWidget->setXAxisLabel(tr("Cardiac Phase"));
    impl_->graphWidget->setYAxisLabel(tr("Flow Rate (mL/s)"));
    rightLayout->addWidget(impl_->graphWidget, 1);

    auto* chartBtnLayout = new QHBoxLayout();
    impl_->copyChartDataBtn = new QPushButton(tr("Copy Chart Data"), impl_->rightPanel);
    impl_->copyChartImageBtn = new QPushButton(tr("Copy Chart Image"), impl_->rightPanel);
    chartBtnLayout->addStretch();
    chartBtnLayout->addWidget(impl_->copyChartDataBtn);
    chartBtnLayout->addWidget(impl_->copyChartImageBtn);
    rightLayout->addLayout(chartBtnLayout);

    impl_->mainSplitter->addWidget(impl_->rightPanel);

    // 40% left, 60% right
    impl_->mainSplitter->setSizes({400, 600});
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

} // namespace dicom_viewer::ui

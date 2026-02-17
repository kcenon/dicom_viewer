#include "ui/quantification_window.hpp"
#include "ui/widgets/flow_graph_widget.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
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
    QPushButton* exportCsvBtn = nullptr;
    QPushButton* flipFlowBtn = nullptr;

    // Data
    std::vector<QuantificationRow> rows;
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

    // Export CSV button
    impl_->exportCsvBtn = new QPushButton(tr("Export CSV..."), impl_->leftPanel);
    leftLayout->addWidget(impl_->exportCsvBtn);

    impl_->mainSplitter->addWidget(impl_->leftPanel);

    // --- Right panel (flow graph + copy buttons) ---
    impl_->rightPanel = new QWidget(impl_->mainSplitter);
    auto* rightLayout = new QVBoxLayout(impl_->rightPanel);

    impl_->graphWidget = new FlowGraphWidget(impl_->rightPanel);
    impl_->graphWidget->setXAxisLabel(tr("Cardiac Phase"));
    impl_->graphWidget->setYAxisLabel(tr("Flow Rate (mL/s)"));
    rightLayout->addWidget(impl_->graphWidget, 1);

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

    // Export CSV button
    connect(impl_->exportCsvBtn, &QPushButton::clicked, this, &QuantificationWindow::exportCsv);

    // Flow direction flip button
    connect(impl_->flipFlowBtn, &QPushButton::toggled, this, [this](bool checked) {
        setFlowDirectionFlipped(checked);
    });

    // Graph phase click → propagate as phase change request
    connect(impl_->graphWidget, &FlowGraphWidget::phaseClicked, this,
            &QuantificationWindow::phaseChangeRequested);

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

} // namespace dicom_viewer::ui

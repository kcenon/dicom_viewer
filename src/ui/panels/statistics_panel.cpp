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

#include "ui/panels/statistics_panel.hpp"

#include <algorithm>

#include <QBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>

namespace dicom_viewer::ui {

// =============================================================================
// HistogramWidget - Custom widget for histogram visualization
// =============================================================================

class HistogramWidget : public QWidget {
public:
    explicit HistogramWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(200, 120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void setHistogram(const std::vector<int64_t>& histogram,
                      double minVal, double maxVal) {
        histogram_ = histogram;
        minValue_ = minVal;
        maxValue_ = maxVal;
        update();
    }

    void clear() {
        histogram_.clear();
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Background
        painter.fillRect(rect(), QColor(30, 30, 30));

        if (histogram_.empty()) {
            painter.setPen(QColor(100, 100, 100));
            painter.drawText(rect(), Qt::AlignCenter, tr("No data"));
            return;
        }

        // Find max value for scaling
        int64_t maxCount = *std::max_element(histogram_.begin(), histogram_.end());
        if (maxCount == 0) {
            maxCount = 1;
        }

        // Drawing area with margin
        constexpr int margin = 25;
        QRect plotArea(margin, 10, width() - margin * 2, height() - margin - 10);

        // Draw axes
        painter.setPen(QColor(100, 100, 100));
        painter.drawLine(plotArea.bottomLeft(), plotArea.bottomRight());
        painter.drawLine(plotArea.bottomLeft(), plotArea.topLeft());

        // Draw axis labels
        painter.setFont(QFont("Arial", 8));
        painter.drawText(QPoint(margin, height() - 5),
                         QString::number(static_cast<int>(minValue_)));
        painter.drawText(QPoint(width() - margin - 30, height() - 5),
                         QString::number(static_cast<int>(maxValue_)));

        // Draw bars
        double barWidth = static_cast<double>(plotArea.width()) / static_cast<double>(histogram_.size());

        painter.setPen(Qt::NoPen);
        QLinearGradient gradient(0, plotArea.top(), 0, plotArea.bottom());
        gradient.setColorAt(0, QColor(100, 180, 255));
        gradient.setColorAt(1, QColor(50, 100, 180));
        painter.setBrush(gradient);

        for (size_t i = 0; i < histogram_.size(); ++i) {
            double x = plotArea.left() + i * barWidth;
            double height = static_cast<double>(histogram_[i]) / static_cast<double>(maxCount)
                          * plotArea.height();

            QRectF bar(x, plotArea.bottom() - height, barWidth - 1, height);
            painter.drawRect(bar);
        }
    }

private:
    std::vector<int64_t> histogram_;
    double minValue_ = -1024.0;
    double maxValue_ = 3071.0;
};

// =============================================================================
// StatisticsPanel::Impl
// =============================================================================

class StatisticsPanel::Impl {
public:
    std::vector<services::RoiStatistics> statistics;

    QTabWidget* tabWidget = nullptr;
    QTableWidget* statsTable = nullptr;
    HistogramWidget* histogramWidget = nullptr;
    QTableWidget* comparisonTable = nullptr;

    QComboBox* roiSelector = nullptr;
    QSpinBox* histogramBinsSpin = nullptr;
    QPushButton* exportButton = nullptr;
    QPushButton* compareButton = nullptr;

    QLabel* meanLabel = nullptr;
    QLabel* stdDevLabel = nullptr;
    QLabel* minLabel = nullptr;
    QLabel* maxLabel = nullptr;
    QLabel* medianLabel = nullptr;
    QLabel* voxelCountLabel = nullptr;
    QLabel* areaLabel = nullptr;
    QLabel* volumeLabel = nullptr;

    int currentRoiIndex = 0;
    bool comparisonMode = false;
    int histogramBins = 256;
    double histogramMin = -1024.0;
    double histogramMax = 3071.0;
};

// =============================================================================
// StatisticsPanel implementation
// =============================================================================

StatisticsPanel::StatisticsPanel(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    setupConnections();
}

StatisticsPanel::~StatisticsPanel() = default;

void StatisticsPanel::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // ROI Selector
    auto selectorLayout = new QHBoxLayout();
    selectorLayout->addWidget(new QLabel(tr("ROI:")));
    impl_->roiSelector = new QComboBox();
    impl_->roiSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selectorLayout->addWidget(impl_->roiSelector);
    mainLayout->addLayout(selectorLayout);

    // Tab widget for different views
    impl_->tabWidget = new QTabWidget();

    // Statistics tab
    auto statsWidget = new QWidget();
    createStatisticsTable();
    auto statsLayout = new QVBoxLayout(statsWidget);
    statsLayout->setContentsMargins(4, 4, 4, 4);

    // Basic stats grid
    auto basicGroup = new QGroupBox(tr("Basic Statistics"));
    auto basicLayout = new QGridLayout(basicGroup);
    basicLayout->setSpacing(4);

    impl_->meanLabel = new QLabel("-");
    impl_->stdDevLabel = new QLabel("-");
    impl_->minLabel = new QLabel("-");
    impl_->maxLabel = new QLabel("-");
    impl_->medianLabel = new QLabel("-");
    impl_->voxelCountLabel = new QLabel("-");
    impl_->areaLabel = new QLabel("-");
    impl_->volumeLabel = new QLabel("-");

    basicLayout->addWidget(new QLabel(tr("Mean:")), 0, 0);
    basicLayout->addWidget(impl_->meanLabel, 0, 1);
    basicLayout->addWidget(new QLabel(tr("Std Dev:")), 0, 2);
    basicLayout->addWidget(impl_->stdDevLabel, 0, 3);

    basicLayout->addWidget(new QLabel(tr("Min:")), 1, 0);
    basicLayout->addWidget(impl_->minLabel, 1, 1);
    basicLayout->addWidget(new QLabel(tr("Max:")), 1, 2);
    basicLayout->addWidget(impl_->maxLabel, 1, 3);

    basicLayout->addWidget(new QLabel(tr("Median:")), 2, 0);
    basicLayout->addWidget(impl_->medianLabel, 2, 1);
    basicLayout->addWidget(new QLabel(tr("Pixels:")), 2, 2);
    basicLayout->addWidget(impl_->voxelCountLabel, 2, 3);

    basicLayout->addWidget(new QLabel(tr("Area:")), 3, 0);
    basicLayout->addWidget(impl_->areaLabel, 3, 1);
    basicLayout->addWidget(new QLabel(tr("Volume:")), 3, 2);
    basicLayout->addWidget(impl_->volumeLabel, 3, 3);

    statsLayout->addWidget(basicGroup);
    statsLayout->addStretch();

    impl_->tabWidget->addTab(statsWidget, tr("Statistics"));

    // Histogram tab
    auto histWidget = new QWidget();
    createHistogramWidget();
    auto histLayout = new QVBoxLayout(histWidget);
    histLayout->setContentsMargins(4, 4, 4, 4);
    histLayout->addWidget(impl_->histogramWidget);

    auto histControls = new QHBoxLayout();
    histControls->addWidget(new QLabel(tr("Bins:")));
    impl_->histogramBinsSpin = new QSpinBox();
    impl_->histogramBinsSpin->setRange(16, 512);
    impl_->histogramBinsSpin->setValue(256);
    impl_->histogramBinsSpin->setSingleStep(16);
    histControls->addWidget(impl_->histogramBinsSpin);
    histControls->addStretch();
    histLayout->addLayout(histControls);

    impl_->tabWidget->addTab(histWidget, tr("Histogram"));

    // Comparison tab
    auto compareWidget = new QWidget();
    createComparisonSection();
    auto compareLayout = new QVBoxLayout(compareWidget);
    compareLayout->setContentsMargins(4, 4, 4, 4);
    compareLayout->addWidget(impl_->comparisonTable);
    impl_->compareButton = new QPushButton(tr("Compare Selected ROIs"));
    compareLayout->addWidget(impl_->compareButton);

    impl_->tabWidget->addTab(compareWidget, tr("Compare"));

    mainLayout->addWidget(impl_->tabWidget);

    // Export section
    createExportSection();
    auto exportLayout = new QHBoxLayout();
    impl_->exportButton = new QPushButton(tr("Export to CSV..."));
    exportLayout->addStretch();
    exportLayout->addWidget(impl_->exportButton);
    mainLayout->addLayout(exportLayout);
}

void StatisticsPanel::setupConnections() {
    connect(impl_->roiSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StatisticsPanel::onRoiSelectionChanged);

    connect(impl_->exportButton, &QPushButton::clicked,
            this, &StatisticsPanel::onExportClicked);

    connect(impl_->compareButton, &QPushButton::clicked,
            this, &StatisticsPanel::onCompareButtonClicked);

    connect(impl_->histogramBinsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StatisticsPanel::onHistogramBinsChanged);
}

void StatisticsPanel::createStatisticsTable() {
    impl_->statsTable = new QTableWidget();
    impl_->statsTable->setColumnCount(2);
    impl_->statsTable->setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    impl_->statsTable->horizontalHeader()->setStretchLastSection(true);
    impl_->statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    impl_->statsTable->setAlternatingRowColors(true);
}

void StatisticsPanel::createHistogramWidget() {
    impl_->histogramWidget = new HistogramWidget();
}

void StatisticsPanel::createComparisonSection() {
    impl_->comparisonTable = new QTableWidget();
    impl_->comparisonTable->setColumnCount(4);
    impl_->comparisonTable->setHorizontalHeaderLabels({
        tr("Property"), tr("ROI 1"), tr("ROI 2"), tr("Difference")
    });
    impl_->comparisonTable->horizontalHeader()->setStretchLastSection(true);
    impl_->comparisonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    impl_->comparisonTable->setAlternatingRowColors(true);
}

void StatisticsPanel::createExportSection() {
    // Export section is created in setupUI
}

void StatisticsPanel::setStatistics(const services::RoiStatistics& stats) {
    impl_->statistics.clear();
    impl_->statistics.push_back(stats);

    impl_->roiSelector->clear();
    QString label = QString::fromStdString(stats.roiLabel);
    if (label.isEmpty()) {
        label = tr("ROI %1").arg(stats.roiId);
    }
    impl_->roiSelector->addItem(label, stats.roiId);

    impl_->currentRoiIndex = 0;
    updateStatisticsTable();
    updateHistogram();
}

void StatisticsPanel::setMultipleStatistics(const std::vector<services::RoiStatistics>& stats) {
    impl_->statistics = stats;

    impl_->roiSelector->clear();
    for (const auto& s : stats) {
        QString label = QString::fromStdString(s.roiLabel);
        if (label.isEmpty()) {
            label = tr("ROI %1").arg(s.roiId);
        }
        impl_->roiSelector->addItem(label, s.roiId);
    }

    if (!stats.empty()) {
        impl_->currentRoiIndex = 0;
        updateStatisticsTable();
        updateHistogram();
    }
}

void StatisticsPanel::clearStatistics() {
    impl_->statistics.clear();
    impl_->roiSelector->clear();

    impl_->meanLabel->setText("-");
    impl_->stdDevLabel->setText("-");
    impl_->minLabel->setText("-");
    impl_->maxLabel->setText("-");
    impl_->medianLabel->setText("-");
    impl_->voxelCountLabel->setText("-");
    impl_->areaLabel->setText("-");
    impl_->volumeLabel->setText("-");

    impl_->histogramWidget->clear();
    impl_->comparisonTable->setRowCount(0);
}

std::vector<services::RoiStatistics> StatisticsPanel::getStatistics() const {
    return impl_->statistics;
}

void StatisticsPanel::setHistogramRange(double minValue, double maxValue) {
    impl_->histogramMin = minValue;
    impl_->histogramMax = maxValue;
    updateHistogram();
}

void StatisticsPanel::setHistogramBins(int bins) {
    impl_->histogramBins = bins;
    impl_->histogramBinsSpin->setValue(bins);
}

void StatisticsPanel::updateStatisticsTable() {
    if (impl_->currentRoiIndex < 0 ||
        static_cast<size_t>(impl_->currentRoiIndex) >= impl_->statistics.size()) {
        return;
    }

    const auto& stats = impl_->statistics[impl_->currentRoiIndex];

    auto format = [](double val, int decimals = 2) {
        return QString::number(val, 'f', decimals);
    };

    impl_->meanLabel->setText(format(stats.mean) + " HU");
    impl_->stdDevLabel->setText(format(stats.stdDev) + " HU");
    impl_->minLabel->setText(format(stats.min, 0) + " HU");
    impl_->maxLabel->setText(format(stats.max, 0) + " HU");
    impl_->medianLabel->setText(format(stats.median) + " HU");
    impl_->voxelCountLabel->setText(QString::number(stats.voxelCount));

    if (stats.areaMm2 > 0) {
        impl_->areaLabel->setText(format(stats.areaMm2) + " mm²");
    } else {
        impl_->areaLabel->setText("-");
    }

    if (stats.volumeMm3 > 0) {
        impl_->volumeLabel->setText(format(stats.volumeMm3) + " mm³");
    } else {
        impl_->volumeLabel->setText("-");
    }
}

void StatisticsPanel::updateHistogram() {
    if (impl_->currentRoiIndex < 0 ||
        static_cast<size_t>(impl_->currentRoiIndex) >= impl_->statistics.size()) {
        impl_->histogramWidget->clear();
        return;
    }

    const auto& stats = impl_->statistics[impl_->currentRoiIndex];
    impl_->histogramWidget->setHistogram(stats.histogram,
                                          stats.histogramMin,
                                          stats.histogramMax);
}

void StatisticsPanel::updateComparison() {
    if (impl_->statistics.size() < 2) {
        impl_->comparisonTable->setRowCount(0);
        return;
    }

    const auto& s1 = impl_->statistics[0];
    const auto& s2 = impl_->statistics[1];

    struct Row {
        QString property;
        double val1;
        double val2;
    };

    std::vector<Row> rows = {
        {tr("Mean (HU)"), s1.mean, s2.mean},
        {tr("Std Dev (HU)"), s1.stdDev, s2.stdDev},
        {tr("Min (HU)"), s1.min, s2.min},
        {tr("Max (HU)"), s1.max, s2.max},
        {tr("Median (HU)"), s1.median, s2.median},
        {tr("Voxel Count"), static_cast<double>(s1.voxelCount), static_cast<double>(s2.voxelCount)},
        {tr("Area (mm²)"), s1.areaMm2, s2.areaMm2},
    };

    impl_->comparisonTable->setRowCount(static_cast<int>(rows.size()));

    for (size_t i = 0; i < rows.size(); ++i) {
        int row = static_cast<int>(i);
        const auto& r = rows[i];

        impl_->comparisonTable->setItem(row, 0, new QTableWidgetItem(r.property));
        impl_->comparisonTable->setItem(row, 1, new QTableWidgetItem(
            QString::number(r.val1, 'f', 2)));
        impl_->comparisonTable->setItem(row, 2, new QTableWidgetItem(
            QString::number(r.val2, 'f', 2)));
        impl_->comparisonTable->setItem(row, 3, new QTableWidgetItem(
            QString::number(r.val2 - r.val1, 'f', 2)));
    }
}

void StatisticsPanel::onExportClicked() {
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export Statistics"),
        QString(), tr("CSV Files (*.csv)"));

    if (!filePath.isEmpty()) {
        emit exportRequested(filePath);

        // Actually perform the export
        auto result = services::RoiStatisticsCalculator::exportMultipleToCsv(
            impl_->statistics,
            filePath.toStdString());

        if (!result) {
            // Error handling could be added here
        }
    }
}

void StatisticsPanel::onRoiSelectionChanged(int index) {
    impl_->currentRoiIndex = index;
    updateStatisticsTable();
    updateHistogram();

    if (index >= 0 && static_cast<size_t>(index) < impl_->statistics.size()) {
        emit roiSelected(impl_->statistics[index].roiId);
    }
}

void StatisticsPanel::onCompareButtonClicked() {
    impl_->comparisonMode = !impl_->comparisonMode;
    emit comparisonModeChanged(impl_->comparisonMode);

    if (impl_->comparisonMode) {
        updateComparison();
        impl_->tabWidget->setCurrentIndex(2); // Switch to comparison tab
    }
}

void StatisticsPanel::onHistogramBinsChanged(int value) {
    impl_->histogramBins = value;
    // Note: Would need to recalculate histogram with new bin count
    // For now, just update with existing data
    updateHistogram();
}

}  // namespace dicom_viewer::ui

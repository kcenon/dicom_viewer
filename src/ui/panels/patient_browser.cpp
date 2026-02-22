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

#include "ui/panels/patient_browser.hpp"

#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QBrush>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QMap>
#include <QStyle>

namespace dicom_viewer::ui {

namespace {
    enum ItemType {
        PatientItem = QTreeWidgetItem::UserType + 1,
        StudyItem,
        SeriesItem
    };

    enum DataRole {
        UidRole = Qt::UserRole + 1,
        PathRole
    };
}

class PatientBrowser::Impl {
public:
    QTreeWidget* treeWidget = nullptr;
    QLineEdit* searchEdit = nullptr;

    QMap<QString, QTreeWidgetItem*> patientItems;
    QMap<QString, QTreeWidgetItem*> studyItems;
    QMap<QString, QTreeWidgetItem*> seriesItems;

    void setupTreeWidget() {
        treeWidget->setHeaderLabels({
            QObject::tr("Patient/Study/Series"),
            QObject::tr("ID/Date"),
            QObject::tr("Description")
        });

        treeWidget->setAlternatingRowColors(true);
        treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        treeWidget->setRootIsDecorated(true);
        treeWidget->setAnimated(true);

        // Column sizing
        treeWidget->header()->setStretchLastSection(true);
        treeWidget->header()->setSectionResizeMode(0, QHeaderView::Interactive);
        treeWidget->setColumnWidth(0, 200);
        treeWidget->setColumnWidth(1, 120);
    }

    QTreeWidgetItem* findPatientItem(const QString& patientId) {
        return patientItems.value(patientId, nullptr);
    }

    QTreeWidgetItem* findStudyItem(const QString& studyUid) {
        return studyItems.value(studyUid, nullptr);
    }
};

PatientBrowser::PatientBrowser(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    setupConnections();
}

PatientBrowser::~PatientBrowser() = default;

void PatientBrowser::setupUI()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Search bar
    impl_->searchEdit = new QLineEdit(this);
    impl_->searchEdit->setPlaceholderText(tr("Search patients..."));
    impl_->searchEdit->setClearButtonEnabled(true);
    layout->addWidget(impl_->searchEdit);

    // Tree widget
    impl_->treeWidget = new QTreeWidget(this);
    impl_->setupTreeWidget();
    layout->addWidget(impl_->treeWidget);

    setLayout(layout);
}

void PatientBrowser::setupConnections()
{
    connect(impl_->treeWidget, &QTreeWidget::itemClicked,
            this, &PatientBrowser::onItemClicked);

    connect(impl_->treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &PatientBrowser::onItemDoubleClicked);

    connect(impl_->treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &PatientBrowser::selectionChanged);

    // Search filter
    connect(impl_->searchEdit, &QLineEdit::textChanged,
            this, [this](const QString& text) {
                QTreeWidgetItemIterator it(impl_->treeWidget);
                while (*it) {
                    bool match = text.isEmpty() ||
                                 (*it)->text(0).contains(text, Qt::CaseInsensitive) ||
                                 (*it)->text(1).contains(text, Qt::CaseInsensitive);
                    (*it)->setHidden(!match && (*it)->parent() != nullptr);
                    ++it;
                }
            });
}

void PatientBrowser::clear()
{
    impl_->treeWidget->clear();
    impl_->patientItems.clear();
    impl_->studyItems.clear();
    impl_->seriesItems.clear();
}

void PatientBrowser::addPatient(const PatientInfo& patient)
{
    auto item = new QTreeWidgetItem(impl_->treeWidget, PatientItem);
    item->setText(0, patient.patientName);
    item->setText(1, patient.patientId);
    item->setText(2, QString("%1, %2").arg(patient.sex, patient.birthDate));
    item->setData(0, UidRole, patient.patientId);
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));

    impl_->patientItems[patient.patientId] = item;
}

void PatientBrowser::addStudy(const QString& patientId, const StudyInfo& study)
{
    auto parentItem = impl_->findPatientItem(patientId);
    if (!parentItem) return;

    auto item = new QTreeWidgetItem(parentItem, StudyItem);
    item->setText(0, study.studyDescription.isEmpty() ?
                     tr("Study") : study.studyDescription);
    item->setText(1, study.studyDate);
    item->setText(2, study.modality);
    item->setData(0, UidRole, study.studyInstanceUid);
    item->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogContentsView));

    impl_->studyItems[study.studyInstanceUid] = item;
}

void PatientBrowser::addSeries(const QString& studyUid, const SeriesInfo& series)
{
    auto parentItem = impl_->findStudyItem(studyUid);
    if (!parentItem) return;

    auto item = new QTreeWidgetItem(parentItem, SeriesItem);

    QString displayName = series.seriesDescription.isEmpty()
        ? QString("Series %1").arg(series.seriesNumber)
        : series.seriesDescription;
    if (!series.seriesType.isEmpty() && series.seriesType != "Unknown") {
        displayName += QString(" [%1]").arg(series.seriesType);
    }

    item->setText(0, displayName);
    item->setText(1, QString::number(series.numberOfImages));
    item->setText(2, series.modality);
    item->setData(0, UidRole, series.seriesInstanceUid);
    item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));

    // Mark non-4D Flow classified series with red text
    if (!series.is4DFlow &&
        !series.seriesType.isEmpty() &&
        series.seriesType != "Unknown") {
        QBrush redBrush(Qt::red);
        for (int col = 0; col < 3; ++col) {
            item->setForeground(col, redBrush);
        }
    }

    impl_->seriesItems[series.seriesInstanceUid] = item;
}

QString PatientBrowser::selectedSeriesUid() const
{
    auto items = impl_->treeWidget->selectedItems();
    if (items.isEmpty()) return QString();

    auto item = items.first();
    if (item->type() == SeriesItem) {
        return item->data(0, UidRole).toString();
    }
    return QString();
}

void PatientBrowser::expandAll()
{
    impl_->treeWidget->expandAll();
}

void PatientBrowser::collapseAll()
{
    impl_->treeWidget->collapseAll();
}

void PatientBrowser::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (item->type() == SeriesItem) {
        QString uid = item->data(0, UidRole).toString();
        QString path = item->data(0, PathRole).toString();
        emit seriesSelected(uid, path);
    }
}

void PatientBrowser::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (item->type() == SeriesItem) {
        QString uid = item->data(0, UidRole).toString();
        QString path = item->data(0, PathRole).toString();
        emit seriesLoadRequested(uid, path);
    } else {
        // Toggle expansion for non-series items
        item->setExpanded(!item->isExpanded());
    }
}

} // namespace dicom_viewer::ui

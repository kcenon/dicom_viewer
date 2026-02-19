#include "ui/intro_page.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

// =========================================================================
// Pimpl
// =========================================================================

class IntroPage::Impl {
public:
    QLabel* logoLabel = nullptr;
    QLabel* versionLabel = nullptr;

    QPushButton* importFolderBtn = nullptr;
    QPushButton* importFileBtn = nullptr;
    QPushButton* importPacsBtn = nullptr;
    QPushButton* openProjectBtn = nullptr;
};

// =========================================================================
// Construction
// =========================================================================

IntroPage::IntroPage(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(60, 40, 60, 40);

    // Top spacer
    root->addStretch(2);

    // Logo / title area
    impl_->logoLabel = new QLabel(tr("DICOM Viewer"));
    QFont titleFont = impl_->logoLabel->font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    impl_->logoLabel->setFont(titleFont);
    impl_->logoLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(impl_->logoLabel);

    impl_->versionLabel = new QLabel(tr("Medical Imaging Workstation"));
    impl_->versionLabel->setAlignment(Qt::AlignCenter);
    QFont subFont = impl_->versionLabel->font();
    subFont.setPointSize(12);
    impl_->versionLabel->setFont(subFont);
    impl_->versionLabel->setStyleSheet("color: #888;");
    root->addWidget(impl_->versionLabel);

    root->addSpacing(40);

    // Button area — two columns
    auto* buttonArea = new QHBoxLayout();
    buttonArea->setSpacing(40);

    // Left column: DICOM import
    auto* importCol = new QVBoxLayout();
    auto* importHeader = new QLabel(tr("Import DICOM"));
    QFont headerFont = importHeader->font();
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    importHeader->setFont(headerFont);
    importCol->addWidget(importHeader);

    impl_->importFolderBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_DirOpenIcon),
        tr("  Import Folder..."));
    impl_->importFolderBtn->setMinimumHeight(36);
    importCol->addWidget(impl_->importFolderBtn);

    impl_->importFileBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_FileIcon),
        tr("  Import File..."));
    impl_->importFileBtn->setMinimumHeight(36);
    importCol->addWidget(impl_->importFileBtn);

    impl_->importPacsBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_DriveNetIcon),
        tr("  Connect to PACS..."));
    impl_->importPacsBtn->setMinimumHeight(36);
    importCol->addWidget(impl_->importPacsBtn);

    importCol->addStretch(1);
    buttonArea->addLayout(importCol);

    // Right column: Project
    auto* projectCol = new QVBoxLayout();
    auto* projectHeader = new QLabel(tr("Project"));
    projectHeader->setFont(headerFont);
    projectCol->addWidget(projectHeader);

    impl_->openProjectBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        tr("  Open Project..."));
    impl_->openProjectBtn->setMinimumHeight(36);
    projectCol->addWidget(impl_->openProjectBtn);

    projectCol->addStretch(1);
    buttonArea->addLayout(projectCol);

    root->addLayout(buttonArea);

    // Bottom spacer
    root->addStretch(3);

    // Wire buttons to signals
    connect(impl_->importFolderBtn, &QPushButton::clicked,
            this, &IntroPage::importFolderRequested);
    connect(impl_->importFileBtn, &QPushButton::clicked,
            this, &IntroPage::importFileRequested);
    connect(impl_->importPacsBtn, &QPushButton::clicked,
            this, &IntroPage::importPacsRequested);
    connect(impl_->openProjectBtn, &QPushButton::clicked,
            this, &IntroPage::openProjectRequested);
}

IntroPage::~IntroPage() = default;

} // namespace dicom_viewer::ui

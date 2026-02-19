#include "ui/panels/report_panel.hpp"

#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

// =========================================================================
// Pimpl
// =========================================================================

class ReportPanel::Impl {
public:
    QPushButton* screenshotBtn = nullptr;
    QPushButton* movieBtn = nullptr;
    QPushButton* matlabBtn = nullptr;
    QPushButton* ensightBtn = nullptr;
    QPushButton* dicomBtn = nullptr;
    QPushButton* reportBtn = nullptr;
};

// =========================================================================
// Construction
// =========================================================================

ReportPanel::ReportPanel(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Section header: Image Capture
    auto* captureHeader = new QLabel(tr("Image Capture"));
    QFont headerFont = captureHeader->font();
    headerFont.setBold(true);
    captureHeader->setFont(headerFont);
    layout->addWidget(captureHeader);

    impl_->screenshotBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_DesktopIcon),
        tr("  Save Screenshot..."));
    impl_->screenshotBtn->setMinimumHeight(32);
    impl_->screenshotBtn->setToolTip(tr("Capture viewport as image (Ctrl+Alt+S)"));
    layout->addWidget(impl_->screenshotBtn);

    impl_->movieBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_MediaPlay),
        tr("  Save Movie..."));
    impl_->movieBtn->setMinimumHeight(32);
    impl_->movieBtn->setToolTip(tr("Export cine or 3D rotation as video"));
    layout->addWidget(impl_->movieBtn);

    layout->addSpacing(8);

    // Section header: Data Export
    auto* exportHeader = new QLabel(tr("Data Export"));
    exportHeader->setFont(headerFont);
    layout->addWidget(exportHeader);

    impl_->matlabBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_FileIcon),
        tr("  Export MATLAB..."));
    impl_->matlabBtn->setMinimumHeight(32);
    impl_->matlabBtn->setToolTip(tr("Export velocity fields as MATLAB .mat files"));
    layout->addWidget(impl_->matlabBtn);

    impl_->ensightBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_FileIcon),
        tr("  Export Ensight..."));
    impl_->ensightBtn->setMinimumHeight(32);
    impl_->ensightBtn->setToolTip(tr("Export as Ensight format (not yet implemented)"));
    impl_->ensightBtn->setEnabled(false);
    layout->addWidget(impl_->ensightBtn);

    impl_->dicomBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_FileIcon),
        tr("  Export DICOM..."));
    impl_->dicomBtn->setMinimumHeight(32);
    impl_->dicomBtn->setToolTip(tr("Export as DICOM (not yet implemented)"));
    impl_->dicomBtn->setEnabled(false);
    layout->addWidget(impl_->dicomBtn);

    layout->addSpacing(8);

    // Section header: Report
    auto* reportHeader = new QLabel(tr("Report"));
    reportHeader->setFont(headerFont);
    layout->addWidget(reportHeader);

    impl_->reportBtn = new QPushButton(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        tr("  Generate Report..."));
    impl_->reportBtn->setMinimumHeight(32);
    impl_->reportBtn->setToolTip(tr("Generate analysis report (not yet implemented)"));
    impl_->reportBtn->setEnabled(false);
    layout->addWidget(impl_->reportBtn);

    // Push everything to the top
    layout->addStretch(1);

    // Wire buttons to signals
    connect(impl_->screenshotBtn, &QPushButton::clicked,
            this, &ReportPanel::screenshotRequested);
    connect(impl_->movieBtn, &QPushButton::clicked,
            this, &ReportPanel::movieRequested);
    connect(impl_->matlabBtn, &QPushButton::clicked,
            this, &ReportPanel::matlabExportRequested);
    connect(impl_->ensightBtn, &QPushButton::clicked,
            this, &ReportPanel::ensightExportRequested);
    connect(impl_->dicomBtn, &QPushButton::clicked,
            this, &ReportPanel::dicomExportRequested);
    connect(impl_->reportBtn, &QPushButton::clicked,
            this, &ReportPanel::reportGenerationRequested);
}

ReportPanel::~ReportPanel() = default;

} // namespace dicom_viewer::ui

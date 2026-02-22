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

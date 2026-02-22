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

#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Report and export panel for the workflow Report stage
 *
 * Provides quick-access buttons for all export operations:
 * Screenshot, Movie, MATLAB, Ensight, DICOM, and Report generation.
 * Designed to be embedded in WorkflowPanel as the Report tab content.
 *
 * @trace SRS-FR-039
 */
class ReportPanel : public QWidget {
    Q_OBJECT

public:
    explicit ReportPanel(QWidget* parent = nullptr);
    ~ReportPanel() override;

    // Non-copyable
    ReportPanel(const ReportPanel&) = delete;
    ReportPanel& operator=(const ReportPanel&) = delete;

signals:
    /// User clicked "Save Screenshot"
    void screenshotRequested();

    /// User clicked "Save Movie"
    void movieRequested();

    /// User clicked "Export MATLAB"
    void matlabExportRequested();

    /// User clicked "Export Ensight"
    void ensightExportRequested();

    /// User clicked "Export DICOM"
    void dicomExportRequested();

    /// User clicked "Generate Report"
    void reportGenerationRequested();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

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


/**
 * @file intro_page.hpp
 * @brief Landing page shown on application startup
 * @details Provides quick access to DICOM import, project opening, recent
 *          project list, and PACS connection before any data is loaded.
 *          Emits signals for user actions.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QWidget-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <memory>
#include <QStringList>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Landing page shown on application startup
 *
 * Provides quick access to DICOM import, project opening,
 * recent project list, and PACS connection before any data is loaded.
 *
 * @trace SRS-FR-039
 */
class IntroPage : public QWidget {
    Q_OBJECT

public:
    explicit IntroPage(QWidget* parent = nullptr);
    ~IntroPage() override;

    // Non-copyable
    IntroPage(const IntroPage&) = delete;
    IntroPage& operator=(const IntroPage&) = delete;

    /// Update the recent projects list displayed in the right column
    void setRecentProjects(const QStringList& paths);

signals:
    /// User clicked "Import DICOM Folder"
    void importFolderRequested();

    /// User clicked "Import DICOM File"
    void importFileRequested();

    /// User clicked "Connect to PACS"
    void importPacsRequested();

    /// User clicked "Open Project"
    void openProjectRequested();

    /// User clicked on a recent project entry
    void openRecentRequested(const QString& path);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

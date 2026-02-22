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

#include "ui/drop_handler.hpp"

#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QWidget>

namespace dicom_viewer::ui {

namespace {

constexpr int kDicmPreambleOffset = 128;
constexpr int kDicmMagicSize = 4;
constexpr int kMaxFilesToCheck = 5;

} // anonymous namespace

DropHandler::DropHandler(QWidget* target, QObject* parent)
    : QObject(parent)
    , target_(target)
{
    if (target_) {
        target_->setAcceptDrops(true);
        target_->installEventFilter(this);
    }
}

DropType DropHandler::classifyDrop(const QMimeData* mimeData)
{
    if (!mimeData || !mimeData->hasUrls()) {
        return DropType::Unknown;
    }

    const auto urls = mimeData->urls();
    if (urls.isEmpty()) {
        return DropType::Unknown;
    }

    // Classify based on the first URL
    return classifyPath(urls.first().toLocalFile());
}

bool DropHandler::isDicomFolder(const QString& folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        return false;
    }

    // Check .dcm extension first (fast path)
    auto dcmFiles = dir.entryInfoList({"*.dcm", "*.DCM"}, QDir::Files);
    if (!dcmFiles.isEmpty()) {
        return true;
    }

    // Check for DICM magic bytes in first N files
    auto allFiles = dir.entryInfoList(QDir::Files);
    int checked = 0;
    for (const auto& entry : allFiles) {
        if (checked >= kMaxFilesToCheck) {
            break;
        }

        QFile file(entry.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        if (file.size() < kDicmPreambleOffset + kDicmMagicSize) {
            ++checked;
            continue;
        }

        file.seek(kDicmPreambleOffset);
        auto magic = file.read(kDicmMagicSize);
        if (magic == "DICM") {
            return true;
        }

        ++checked;
    }

    return false;
}

DropType DropHandler::classifyPath(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return DropType::Unknown;
    }

    QFileInfo info(filePath);

    // Directory → check for DICOM
    if (info.isDir()) {
        if (isDicomFolder(filePath)) {
            return DropType::DicomFolder;
        }
        return DropType::Unknown;
    }

    // File → classify by extension
    auto suffix = info.suffix().toLower();

    if (suffix == "flo") {
        return DropType::ProjectFile;
    }
    if (suffix == "stl") {
        return DropType::StlFile;
    }
    if (suffix == "nii" || suffix == "nrrd") {
        return DropType::MaskFile;
    }
    if (suffix == "gz" && info.completeSuffix().toLower().endsWith("nii.gz")) {
        return DropType::MaskFile;
    }
    if (suffix == "dcm") {
        // Single .dcm file — treat parent as DICOM folder
        return DropType::DicomFolder;
    }

    return DropType::Unknown;
}

bool DropHandler::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != target_) {
        return QObject::eventFilter(obj, event);
    }

    switch (event->type()) {
        case QEvent::DragEnter: {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);
            auto type = classifyDrop(dragEvent->mimeData());
            if (type != DropType::Unknown) {
                dragEvent->acceptProposedAction();
                return true;
            }
            break;
        }
        case QEvent::Drop: {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            auto* mimeData = dropEvent->mimeData();
            if (!mimeData || !mimeData->hasUrls()) {
                break;
            }

            auto path = mimeData->urls().first().toLocalFile();
            auto type = classifyPath(path);

            switch (type) {
                case DropType::DicomFolder:
                    emit dicomFolderDropped(path);
                    break;
                case DropType::ProjectFile:
                    emit projectFileDropped(path);
                    break;
                case DropType::MaskFile:
                    emit maskFileDropped(path);
                    break;
                case DropType::StlFile:
                    emit stlFileDropped(path);
                    break;
                case DropType::Unknown:
                    break;
            }

            dropEvent->acceptProposedAction();
            return true;
        }
        default:
            break;
    }

    return QObject::eventFilter(obj, event);
}

} // namespace dicom_viewer::ui

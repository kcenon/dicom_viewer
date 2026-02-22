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
 * @file drop_handler.hpp
 * @brief Drag-and-drop handler for DICOM viewers
 * @details Classifies dropped content (DicomFolder, ProjectFile, MaskFile,
 *          StlFile). Detects DICOM folders by checking DICM magic
 *          bytes. Installed as event filter on QWidget targets.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QObject-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <QObject>
#include <QString>

class QMimeData;
class QWidget;

namespace dicom_viewer::ui {

/**
 * @brief Classification of dropped content types
 */
enum class DropType {
    DicomFolder,   ///< Folder containing DICOM files
    ProjectFile,   ///< .flo project file
    MaskFile,      ///< .nii/.nii.gz/.nrrd mask file
    StlFile,       ///< .stl 3D mesh file
    Unknown        ///< Unrecognized file type
};

/**
 * @brief Drag-and-drop handler for DICOM viewers
 *
 * Provides drop type classification and DICOM folder detection.
 * Install as an event filter on any QWidget to handle drag/drop events.
 *
 * @trace SRS-FR-051
 */
class DropHandler : public QObject {
    Q_OBJECT

public:
    explicit DropHandler(QWidget* target, QObject* parent = nullptr);

    /**
     * @brief Classify a drop based on MIME data
     * @param mimeData Drop data from drag event
     * @return Detected drop type
     */
    [[nodiscard]] static DropType classifyDrop(const QMimeData* mimeData);

    /**
     * @brief Check if a folder contains DICOM files
     *
     * Reads up to 5 files and checks for DICM magic bytes at offset 128.
     *
     * @param folderPath Path to the folder
     * @return True if DICOM files detected
     */
    [[nodiscard]] static bool isDicomFolder(const QString& folderPath);

    /**
     * @brief Classify a single file path by extension
     * @param filePath File or folder path
     * @return Detected type
     */
    [[nodiscard]] static DropType classifyPath(const QString& filePath);

signals:
    /**
     * @brief Emitted when a DICOM folder is dropped
     * @param path Folder path
     */
    void dicomFolderDropped(const QString& path);

    /**
     * @brief Emitted when a .flo project file is dropped
     * @param path File path
     */
    void projectFileDropped(const QString& path);

    /**
     * @brief Emitted when a mask file is dropped
     * @param path File path
     */
    void maskFileDropped(const QString& path);

    /**
     * @brief Emitted when an STL file is dropped
     * @param path File path
     */
    void stlFileDropped(const QString& path);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QWidget* target_ = nullptr;
};

} // namespace dicom_viewer::ui

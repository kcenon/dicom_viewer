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

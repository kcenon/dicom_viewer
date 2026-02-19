#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Landing page shown on application startup
 *
 * Provides quick access to DICOM import, project opening,
 * and PACS connection before any data is loaded.
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

signals:
    /// User clicked "Import DICOM Folder"
    void importFolderRequested();

    /// User clicked "Import DICOM File"
    void importFileRequested();

    /// User clicked "Connect to PACS"
    void importPacsRequested();

    /// User clicked "Open Project"
    void openProjectRequested();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

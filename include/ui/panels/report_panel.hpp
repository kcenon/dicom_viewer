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

#pragma once

#include <memory>
#include <QWidget>
#include <QString>
#include <QStringList>

class QTreeWidgetItem;

namespace dicom_viewer::ui {

/**
 * @brief Patient data structure for tree view
 */
struct PatientInfo {
    QString patientId;
    QString patientName;
    QString birthDate;
    QString sex;
};

/**
 * @brief Study data structure for tree view
 */
struct StudyInfo {
    QString studyInstanceUid;
    QString studyDate;
    QString studyDescription;
    QString accessionNumber;
    QString modality;
};

/**
 * @brief Series data structure for tree view
 */
struct SeriesInfo {
    QString seriesInstanceUid;
    QString seriesNumber;
    QString seriesDescription;
    QString modality;
    int numberOfImages = 0;
    QString seriesType;    ///< Classification result (e.g., "4D Flow Magnitude", "CT")
    bool is4DFlow = false; ///< true for 4D Flow magnitude or phase series
};

/**
 * @brief Patient browser panel for navigating DICOM studies
 *
 * Displays a hierarchical tree view of Patients > Studies > Series.
 * Supports loading from directory, PACS query results, or manual entry.
 *
 * @trace SRS-FR-039, PRD FR-011.3
 */
class PatientBrowser : public QWidget {
    Q_OBJECT

public:
    explicit PatientBrowser(QWidget* parent = nullptr);
    ~PatientBrowser() override;

    // Non-copyable
    PatientBrowser(const PatientBrowser&) = delete;
    PatientBrowser& operator=(const PatientBrowser&) = delete;

    /**
     * @brief Clear all items from the browser
     */
    void clear();

    /**
     * @brief Add a patient to the tree
     * @param patient Patient information
     */
    void addPatient(const PatientInfo& patient);

    /**
     * @brief Add a study under a patient
     * @param patientId Parent patient ID
     * @param study Study information
     */
    void addStudy(const QString& patientId, const StudyInfo& study);

    /**
     * @brief Add a series under a study
     * @param studyUid Parent study UID
     * @param series Series information
     */
    void addSeries(const QString& studyUid, const SeriesInfo& series);

    /**
     * @brief Get the currently selected series UID
     * @return Series UID or empty string
     */
    QString selectedSeriesUid() const;

    /**
     * @brief Expand all items in the tree
     */
    void expandAll();

    /**
     * @brief Collapse all items in the tree
     */
    void collapseAll();

signals:
    /**
     * @brief Emitted when a series is selected
     * @param seriesUid Series instance UID
     * @param seriesPath Path to series directory (if applicable)
     */
    void seriesSelected(const QString& seriesUid, const QString& seriesPath);

    /**
     * @brief Emitted when a series is double-clicked (load request)
     * @param seriesUid Series instance UID
     * @param seriesPath Path to series directory (if applicable)
     */
    void seriesLoadRequested(const QString& seriesUid, const QString& seriesPath);

    /**
     * @brief Emitted when selection changes
     */
    void selectionChanged();

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void setupUI();
    void setupConnections();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

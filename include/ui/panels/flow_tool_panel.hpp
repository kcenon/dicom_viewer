#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Available velocity series components for 4D Flow MRI
 */
enum class FlowSeries {
    Magnitude,  ///< Magnitude image
    RL,         ///< Right-Left velocity component
    AP,         ///< Anterior-Posterior velocity component
    FH,         ///< Foot-Head velocity component
    PCMRA       ///< Phase-Contrast MR Angiography
};

/**
 * @brief Left tool panel for 4D Flow analysis workflow
 *
 * Provides collapsible sections for Settings, Series selection,
 * and placeholders for Display 2D, Display 3D, Mask, and 3D Objects.
 * Uses QToolBox for collapsible section management.
 *
 * Layout:
 * @code
 * Flow Tool Panel
 * +-- Settings (Phase/Slice info)
 * +-- Series (Mag/RL/AP/FH/PC-MRA toggle buttons)
 * +-- Display 2D (placeholder)
 * +-- Display 3D (placeholder)
 * @endcode
 *
 * @trace SRS-FR-046, PRD FR-015
 */
class FlowToolPanel : public QWidget {
    Q_OBJECT

public:
    explicit FlowToolPanel(QWidget* parent = nullptr);
    ~FlowToolPanel() override;

    // Non-copyable
    FlowToolPanel(const FlowToolPanel&) = delete;
    FlowToolPanel& operator=(const FlowToolPanel&) = delete;

    /**
     * @brief Get the currently selected series
     */
    [[nodiscard]] FlowSeries selectedSeries() const;

    /**
     * @brief Enable or disable the panel based on data availability
     */
    void setFlowDataAvailable(bool available);

public slots:
    /**
     * @brief Update phase display info
     * @param current Current phase index (0-based)
     * @param total Total phase count
     */
    void setPhaseInfo(int current, int total);

    /**
     * @brief Update slice display info
     * @param current Current slice index (0-based)
     * @param total Total slice count
     */
    void setSliceInfo(int current, int total);

    /**
     * @brief Set the selected series programmatically
     */
    void setSelectedSeries(FlowSeries series);

signals:
    /**
     * @brief Emitted when the user selects a different velocity series
     * @param series Selected series component
     */
    void seriesSelectionChanged(FlowSeries series);

private:
    void setupUI();
    void setupConnections();
    void createSettingsSection();
    void createSeriesSection();
    void createPlaceholderSections();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

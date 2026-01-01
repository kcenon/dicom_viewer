#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Tool categories for the tools panel
 */
enum class ToolCategory {
    Navigation,     // Scroll, Zoom, Pan, W/L
    Measurement,    // Distance, Angle, ROI
    Annotation,     // Text, Arrow, Freehand
    Visualization   // Presets, 3D modes
};

/**
 * @brief Tools panel providing context-sensitive tool options
 *
 * Displays tool settings and options based on the currently selected tool.
 * Provides quick access to window/level presets and visualization modes.
 *
 * @trace SRS-FR-039, PRD FR-011.4
 */
class ToolsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ToolsPanel(QWidget* parent = nullptr);
    ~ToolsPanel() override;

    // Non-copyable
    ToolsPanel(const ToolsPanel&) = delete;
    ToolsPanel& operator=(const ToolsPanel&) = delete;

    /**
     * @brief Set the current tool category to display options for
     * @param category Tool category
     */
    void setToolCategory(ToolCategory category);

    /**
     * @brief Set window/level values (updates sliders)
     * @param width Window width
     * @param center Window center
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Get current window width
     */
    double windowWidth() const;

    /**
     * @brief Get current window center
     */
    double windowCenter() const;

signals:
    /**
     * @brief Emitted when window/level changes
     */
    void windowLevelChanged(double width, double center);

    /**
     * @brief Emitted when a preset is selected
     */
    void presetSelected(const QString& presetName);

    /**
     * @brief Emitted when visualization mode changes
     */
    void visualizationModeChanged(int mode);

    /**
     * @brief Emitted when slice changes
     */
    void sliceChanged(int slice);

private slots:
    void onWindowSliderChanged(int value);
    void onLevelSliderChanged(int value);
    void onPresetButtonClicked();

private:
    void setupUI();
    void setupConnections();
    void createNavigationSection();
    void createWindowLevelSection();
    void createPresetSection();
    void createVisualizationSection();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

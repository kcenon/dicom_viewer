#pragma once

#include <memory>
#include <QWidget>

#include "services/render/hemodynamic_overlay_renderer.hpp"

namespace dicom_viewer::ui {

/**
 * @brief UI panel for controlling 2D hemodynamic overlay display
 *
 * Provides Display 2D checkboxes for each overlay type, per-overlay
 * colormap range controls, and opacity sliders. Emits signals when
 * overlay settings change.
 *
 * @trace SRS-FR-046, PRD FR-015
 */
class OverlayControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit OverlayControlPanel(QWidget* parent = nullptr);
    ~OverlayControlPanel() override;

    // Non-copyable
    OverlayControlPanel(const OverlayControlPanel&) = delete;
    OverlayControlPanel& operator=(const OverlayControlPanel&) = delete;

    /**
     * @brief Set panel enabled state based on data availability
     */
    void setOverlaysAvailable(bool available);

    /**
     * @brief Check if a specific overlay type is enabled (checked)
     */
    [[nodiscard]] bool isOverlayEnabled(services::OverlayType type) const;

    /**
     * @brief Get opacity for a specific overlay type (0.0 - 1.0)
     */
    [[nodiscard]] double overlayOpacity(services::OverlayType type) const;

    /**
     * @brief Get scalar range for a specific overlay type
     */
    [[nodiscard]] std::pair<double, double>
    overlayScalarRange(services::OverlayType type) const;

    /**
     * @brief Reset all controls to default state
     */
    void resetToDefaults();

signals:
    /**
     * @brief Emitted when an overlay visibility checkbox changes
     * @param type Overlay type
     * @param visible New visibility state
     */
    void overlayVisibilityChanged(services::OverlayType type, bool visible);

    /**
     * @brief Emitted when overlay opacity changes
     * @param type Overlay type
     * @param opacity New opacity (0.0 - 1.0)
     */
    void overlayOpacityChanged(services::OverlayType type, double opacity);

    /**
     * @brief Emitted when overlay scalar range changes
     * @param type Overlay type
     * @param minVal Minimum scalar value
     * @param maxVal Maximum scalar value
     */
    void overlayScalarRangeChanged(services::OverlayType type,
                                   double minVal, double maxVal);

private slots:
    void onCheckboxToggled(int typeIndex, bool checked);
    void onOpacitySliderChanged(int typeIndex, int value);
    void onRangeMinChanged(int typeIndex, double value);
    void onRangeMaxChanged(int typeIndex, double value);

private:
    void setupUI();
    void setupConnections();
    void createOverlayGroup(services::OverlayType type,
                            const QString& label,
                            double defaultMin,
                            double defaultMax);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

class ViewportWidget;

/**
 * @brief Layout mode for viewport arrangement
 */
enum class LayoutMode {
    Single,     ///< Single viewport (default)
    DualSplit,  ///< Side-by-side: 2D slice | 3D rendering
    QuadSplit   ///< 2x2 grid: Axial | Sagittal | Coronal | 3D
};

/**
 * @brief Manages viewport layout with 1/2/4-split modes
 *
 * Uses QStackedWidget to switch between layout containers.
 * Each layout mode creates the required ViewportWidgets and
 * arranges them with splitters or grid layouts.
 *
 * @trace SRS-FR-005
 */
class ViewportLayoutManager : public QWidget {
    Q_OBJECT

public:
    explicit ViewportLayoutManager(QWidget* parent = nullptr);
    ~ViewportLayoutManager() override;

    // Non-copyable
    ViewportLayoutManager(const ViewportLayoutManager&) = delete;
    ViewportLayoutManager& operator=(const ViewportLayoutManager&) = delete;

    /**
     * @brief Get current layout mode
     */
    [[nodiscard]] LayoutMode layoutMode() const;

    /**
     * @brief Get the primary (always-available) viewport
     *
     * In Single mode this is the only viewport.
     * In DualSplit this is the left (2D) viewport.
     * In QuadSplit this is the top-left (Axial) viewport.
     */
    [[nodiscard]] ViewportWidget* primaryViewport() const;

    /**
     * @brief Get a viewport by index
     * @param index 0-based viewport index
     * @return ViewportWidget pointer, or nullptr if index is out of range
     *
     * Index mapping per mode:
     *   Single:    0 = primary
     *   DualSplit: 0 = 2D, 1 = 3D
     *   QuadSplit: 0 = Axial, 1 = Sagittal, 2 = Coronal, 3 = 3D
     */
    [[nodiscard]] ViewportWidget* viewport(int index) const;

    /**
     * @brief Get the number of active viewports in current mode
     */
    [[nodiscard]] int viewportCount() const;

    /**
     * @brief Get the active (focused) viewport index
     * @return 0-based index of the last-interacted viewport
     */
    [[nodiscard]] int activeViewportIndex() const;

    /**
     * @brief Get the active viewport widget
     * @return Pointer to the active viewport
     */
    [[nodiscard]] ViewportWidget* activeViewport() const;

    /**
     * @brief Check if crosshair linking between viewports is enabled
     */
    [[nodiscard]] bool isCrosshairLinkEnabled() const;

public slots:
    /**
     * @brief Switch layout mode
     * @param mode New layout mode
     */
    void setLayoutMode(LayoutMode mode);

    /**
     * @brief Set the active viewport by index
     * @param index 0-based viewport index
     */
    void setActiveViewport(int index);

    /**
     * @brief Enable or disable crosshair linking between viewports
     *
     * When enabled, clicking on any 2D viewport synchronizes the
     * crosshair position (and thus slice) across all other viewports.
     * MPR crosshair intersection lines are shown on 2D viewports.
     *
     * @param enabled True to enable linking
     */
    void setCrosshairLinkEnabled(bool enabled);

signals:
    /**
     * @brief Emitted when layout mode changes
     * @param mode New layout mode
     */
    void layoutModeChanged(LayoutMode mode);

    /**
     * @brief Emitted when active viewport changes
     * @param viewport The newly active viewport
     * @param index The viewport index
     */
    void activeViewportChanged(ViewportWidget* viewport, int index);

    /**
     * @brief Emitted when crosshair link mode changes
     * @param enabled True if linking is now enabled
     */
    void crosshairLinkEnabledChanged(bool enabled);

private:
    void buildSingleLayout();
    void buildDualLayout();
    void buildQuadLayout();
    void setupCrosshairLinking();
    void teardownCrosshairLinking();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

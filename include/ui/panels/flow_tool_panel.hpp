#pragma once

#include <cstdint>
#include <memory>
#include <QWidget>

namespace dicom_viewer::services {
class LabelManager;
}

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
 * @brief 2D hemodynamic overlay items toggleable in the tool panel
 */
enum class Display2DItem {
    Mask,              ///< Segmentation mask overlay
    Velocity,          ///< Velocity magnitude colormap
    Streamline,        ///< 2D flow streamlines
    EnergyLoss,        ///< Viscous dissipation rate
    Vorticity,         ///< Vorticity magnitude
    VelocityTexture    ///< Line Integral Convolution (LIC)
};

/**
 * @brief 3D visualization items toggleable in the tool panel
 */
enum class Display3DItem {
    MaskVolume,   ///< Segmentation mask volume rendering
    Surface,      ///< Isosurface mesh
    Cine,         ///< Cine playback in 3D
    Magnitude,    ///< Magnitude volume rendering
    Velocity,     ///< Velocity volume overlay
    ASC,          ///< Aortic sinus/cusp view
    Streamline,   ///< 3D streamlines
    EnergyLoss,   ///< Energy loss volume
    WSS,          ///< Wall Shear Stress surface coloring
    OSI,          ///< Oscillatory Shear Index surface coloring
    AFI,          ///< Aneurysm Formation Indicator surface coloring
    RRT,          ///< Relative Residence Time surface coloring
    Vorticity     ///< Vorticity volume
};

/**
 * @brief Left tool panel for 4D Flow analysis workflow
 *
 * Provides collapsible sections for Settings, Series selection,
 * Display 2D overlay checkboxes, and Display 3D visualization toggles.
 * Uses QToolBox for collapsible section management.
 *
 * Layout:
 * @code
 * Flow Tool Panel
 * +-- Settings (Phase/Slice info)
 * +-- Series (Mag/RL/AP/FH/PC-MRA toggle buttons)
 * +-- Mask (Mask list with checkboxes + color swatches, Load/Remove)
 * +-- Display 2D (Mask/Velocity/Streamline/EnergyLoss/Vorticity/VelTexture)
 * +-- Display 3D (MaskVol/Surface/Cine/Mag/Vel/ASC/Streamline/EL/WSS/OSI/AFI/RRT/Vorticity)
 * +-- 3D Object (Object visibility list)
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
     * @brief Check if a 2D display item is enabled
     */
    [[nodiscard]] bool isDisplay2DEnabled(Display2DItem item) const;

    /**
     * @brief Check if a 3D display item is enabled
     */
    [[nodiscard]] bool isDisplay3DEnabled(Display3DItem item) const;

    /**
     * @brief Get the number of masks in the list
     */
    [[nodiscard]] int maskCount() const;

    /**
     * @brief Get the number of 3D objects in the list
     */
    [[nodiscard]] int objectCount() const;

    /**
     * @brief Check if a named 3D object is visible
     */
    [[nodiscard]] bool isObjectVisible(const QString& name) const;

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

    /**
     * @brief Set a 2D display item checked/unchecked programmatically
     */
    void setDisplay2DEnabled(Display2DItem item, bool enabled);

    /**
     * @brief Set a 3D display item checked/unchecked programmatically
     */
    void setDisplay3DEnabled(Display3DItem item, bool enabled);

    /**
     * @brief Set the scalar range for a 3D display item programmatically
     * @param item The display item (must have colormap)
     * @param minVal Minimum scalar value
     * @param maxVal Maximum scalar value
     */
    void setDisplay3DRange(Display3DItem item, double minVal, double maxVal);

    /**
     * @brief Set the LabelManager for mask list synchronization
     */
    void setLabelManager(services::LabelManager* manager);

    /**
     * @brief Refresh mask list from current LabelManager state
     */
    void refreshMaskList();

    /**
     * @brief Add a named 3D object to the object list
     * @param name Object display name
     * @param visible Initial visibility
     */
    void addObject(const QString& name, bool visible = true);

    /**
     * @brief Remove a named 3D object from the object list
     */
    void removeObject(const QString& name);

    /**
     * @brief Set visibility of a named 3D object programmatically
     */
    void setObjectVisible(const QString& name, bool visible);

    // -- Loaded series management --

    /**
     * @brief Add a loaded series entry to the series list
     * @param name Display name (e.g., "CINE retro SA [CINE]")
     * @param seriesUid Series instance UID for identification
     * @param is4DFlow True for 4D Flow series (default text), false for non-4D (red text)
     */
    void addLoadedSeries(const QString& name, const QString& seriesUid, bool is4DFlow);

    /**
     * @brief Remove a loaded series by UID
     * @param seriesUid Series instance UID
     */
    void removeLoadedSeries(const QString& seriesUid);

    /**
     * @brief Clear all loaded series entries
     */
    void clearLoadedSeries();

    /**
     * @brief Get the number of loaded series
     */
    [[nodiscard]] int loadedSeriesCount() const;

signals:
    /**
     * @brief Emitted when the user selects a different velocity series
     * @param series Selected series component
     */
    void seriesSelectionChanged(FlowSeries series);

    /**
     * @brief Emitted when a 2D display checkbox is toggled
     * @param item The display item
     * @param enabled True if checked
     */
    void display2DToggled(Display2DItem item, bool enabled);

    /**
     * @brief Emitted when a 3D display checkbox is toggled
     * @param item The display item
     * @param enabled True if checked
     */
    void display3DToggled(Display3DItem item, bool enabled);

    /**
     * @brief Emitted when a 3D display item's scalar range is changed
     * @param item The display item
     * @param minVal New minimum scalar value
     * @param maxVal New maximum scalar value
     */
    void display3DRangeChanged(Display3DItem item, double minVal, double maxVal);

    /**
     * @brief Emitted when user clicks Load to import a mask file
     */
    void maskLoadRequested();

    /**
     * @brief Emitted when user clicks Remove for the selected mask
     * @param labelId Label ID to remove
     */
    void maskRemoveRequested(uint8_t labelId);

    /**
     * @brief Emitted when a mask visibility checkbox is toggled
     * @param labelId Label ID
     * @param visible New visibility state
     */
    void maskVisibilityToggled(uint8_t labelId, bool visible);

    /**
     * @brief Emitted when a 3D object visibility checkbox is toggled
     * @param name Object name
     * @param visible New visibility state
     */
    void objectVisibilityToggled(const QString& name, bool visible);

    /**
     * @brief Emitted when user clicks a loaded series entry
     * @param seriesUid Series instance UID
     */
    void loadedSeriesActivated(const QString& seriesUid);

private:
    void setupUI();
    void setupConnections();
    void createSettingsSection();
    void createSeriesSection();
    void createMaskSection();
    void createDisplay2DSection();
    void createDisplay3DSection();
    void create3DObjectSection();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

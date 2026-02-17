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
 * +-- Display 2D (Mask/Velocity/Streamline/EnergyLoss/Vorticity/VelTexture)
 * +-- Display 3D (MaskVol/Surface/Cine/Mag/Vel/ASC/Streamline/EL/WSS/OSI/AFI/RRT/Vorticity)
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

private:
    void setupUI();
    void setupConnections();
    void createSettingsSection();
    void createSeriesSection();
    void createDisplay2DSection();
    void createDisplay3DSection();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

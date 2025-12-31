#pragma once

#include <array>
#include <functional>
#include <memory>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>

namespace dicom_viewer::services {

/**
 * @brief MPR view plane orientation
 */
enum class MPRPlane {
    Axial,      // XY plane (top-down)
    Coronal,    // XZ plane (front view)
    Sagittal    // YZ plane (side view)
};

/**
 * @brief Thick slab rendering mode
 */
enum class SlabMode {
    None,       // Single slice
    MIP,        // Maximum Intensity Projection
    MinIP,      // Minimum Intensity Projection
    Average     // Average Intensity Projection
};

/**
 * @brief Callback for slice position changes
 */
using SlicePositionCallback = std::function<void(MPRPlane plane, double position)>;

/**
 * @brief Callback for crosshair position changes
 */
using CrosshairCallback = std::function<void(double x, double y, double z)>;

/**
 * @brief Multi-Planar Reconstruction (MPR) renderer
 *
 * Provides synchronized orthogonal views (Axial, Coronal, Sagittal)
 * with crosshair navigation and window/level adjustment.
 *
 * @trace SRS-FR-008, SRS-FR-009, SRS-FR-010, SRS-FR-011
 */
class MPRRenderer {
public:
    MPRRenderer();
    ~MPRRenderer();

    // Non-copyable, movable
    MPRRenderer(const MPRRenderer&) = delete;
    MPRRenderer& operator=(const MPRRenderer&) = delete;
    MPRRenderer(MPRRenderer&&) noexcept;
    MPRRenderer& operator=(MPRRenderer&&) noexcept;

    /**
     * @brief Set the input volume data
     * @param imageData VTK image data (3D volume)
     */
    void setInputData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Get renderer for a specific plane
     * @param plane MPR plane type
     * @return VTK renderer for the plane
     */
    vtkSmartPointer<vtkRenderer> getRenderer(MPRPlane plane) const;

    /**
     * @brief Set slice position for a plane
     * @param plane MPR plane type
     * @param position Position in world coordinates
     */
    void setSlicePosition(MPRPlane plane, double position);

    /**
     * @brief Get current slice position
     * @param plane MPR plane type
     * @return Position in world coordinates
     */
    double getSlicePosition(MPRPlane plane) const;

    /**
     * @brief Get slice range for a plane
     * @param plane MPR plane type
     * @return [min, max] position range
     */
    std::pair<double, double> getSliceRange(MPRPlane plane) const;

    /**
     * @brief Scroll slice by delta
     * @param plane MPR plane type
     * @param delta Number of slices to scroll (positive = forward)
     */
    void scrollSlice(MPRPlane plane, int delta);

    /**
     * @brief Set window/level for display
     * @param width Window width
     * @param center Window center (level)
     */
    void setWindowLevel(double width, double center);

    /**
     * @brief Get current window/level
     * @return {width, center}
     */
    std::pair<double, double> getWindowLevel() const;

    /**
     * @brief Set crosshair position (in world coordinates)
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     */
    void setCrosshairPosition(double x, double y, double z);

    /**
     * @brief Get crosshair position
     * @return [x, y, z] world coordinates
     */
    std::array<double, 3> getCrosshairPosition() const;

    /**
     * @brief Enable/disable crosshair display
     * @param visible True to show crosshairs
     */
    void setCrosshairVisible(bool visible);

    /**
     * @brief Set thick slab mode
     * @param mode Slab rendering mode
     * @param thickness Slab thickness in mm
     */
    void setSlabMode(SlabMode mode, double thickness = 1.0);

    /**
     * @brief Register callback for slice position changes
     */
    void setSlicePositionCallback(SlicePositionCallback callback);

    /**
     * @brief Register callback for crosshair changes
     */
    void setCrosshairCallback(CrosshairCallback callback);

    /**
     * @brief Update all views
     */
    void update();

    /**
     * @brief Reset views to default positions (center of volume)
     */
    void resetViews();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

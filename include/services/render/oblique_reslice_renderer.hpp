#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkMatrix4x4.h>

namespace dicom_viewer::services {

/**
 * @brief 3D point in world coordinates
 */
struct Point3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Point3D() = default;
    Point3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
};

/**
 * @brief 3D vector for direction/normal
 */
struct Vector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 1.0;

    Vector3D() = default;
    Vector3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    [[nodiscard]] double length() const noexcept;
    [[nodiscard]] Vector3D normalized() const noexcept;
};

/**
 * @brief Oblique plane definition using Euler angles or geometric primitives
 */
struct ObliquePlaneDefinition {
    // Rotation angles in degrees (Euler angles around X, Y, Z axes)
    double rotationX = 0.0;
    double rotationY = 0.0;
    double rotationZ = 0.0;

    // Center point of the plane
    Point3D center;

    // Slice offset along the normal direction
    double sliceOffset = 0.0;
};

/**
 * @brief Interpolation modes for reslicing
 */
enum class InterpolationMode {
    NearestNeighbor,
    Linear,
    Cubic
};

/**
 * @brief Options for oblique reslicing output
 */
struct ObliqueResliceOptions {
    InterpolationMode interpolation = InterpolationMode::Linear;
    std::array<int, 2> outputDimensions = {512, 512};
    double outputSpacing = -1.0;  // -1 = auto (based on input spacing)
    double backgroundValue = -1000.0;  // HU for air
};

/**
 * @brief Callback for plane orientation changes
 */
using PlaneChangedCallback = std::function<void(const ObliquePlaneDefinition& plane)>;

/**
 * @brief Callback for slice offset changes
 */
using SliceChangedCallback = std::function<void(double offset)>;

/**
 * @brief Oblique reslicing renderer for arbitrary angle MPR views
 *
 * Enables visualization of anatomical structures that don't align with
 * standard axial/coronal/sagittal planes. Essential for:
 * - Visualizing vessels at their true cross-section
 * - Aligning views with anatomical landmarks
 * - Cardiac imaging (short-axis, long-axis views)
 * - Spine imaging (parallel to disc spaces)
 *
 * @trace SRS-FR-010
 */
class ObliqueResliceRenderer {
public:
    using ImageType = vtkSmartPointer<vtkImageData>;

    ObliqueResliceRenderer();
    ~ObliqueResliceRenderer();

    // Non-copyable, movable
    ObliqueResliceRenderer(const ObliqueResliceRenderer&) = delete;
    ObliqueResliceRenderer& operator=(const ObliqueResliceRenderer&) = delete;
    ObliqueResliceRenderer(ObliqueResliceRenderer&&) noexcept;
    ObliqueResliceRenderer& operator=(ObliqueResliceRenderer&&) noexcept;

    // ==================== Input Configuration ====================

    /**
     * @brief Set the input volume data
     * @param imageData VTK image data (3D volume)
     */
    void setInputData(ImageType imageData);

    /**
     * @brief Get current input data
     * @return VTK image data or nullptr
     */
    [[nodiscard]] ImageType getInputData() const;

    // ==================== Plane Definition Methods ====================

    /**
     * @brief Set plane orientation by Euler rotation angles
     * @param rotX Rotation around X axis in degrees
     * @param rotY Rotation around Y axis in degrees
     * @param rotZ Rotation around Z axis in degrees
     */
    void setPlaneByRotation(double rotX, double rotY, double rotZ);

    /**
     * @brief Set plane by three points in space
     *
     * Defines a plane that passes through all three points.
     * The normal is computed as (p2-p1) x (p3-p1).
     *
     * @param p1 First point
     * @param p2 Second point
     * @param p3 Third point
     */
    void setPlaneByThreePoints(const Point3D& p1, const Point3D& p2, const Point3D& p3);

    /**
     * @brief Set plane by normal vector and center point
     * @param normal Normal vector of the plane
     * @param center Center point of the plane
     */
    void setPlaneByNormal(const Vector3D& normal, const Point3D& center);

    /**
     * @brief Set center point of the plane
     * @param center Center point in world coordinates
     */
    void setCenter(const Point3D& center);

    /**
     * @brief Get current center point
     * @return Center point
     */
    [[nodiscard]] Point3D getCenter() const;

    // ==================== Slice Navigation ====================

    /**
     * @brief Set slice offset along the normal direction
     * @param offset Offset in mm from the center plane
     */
    void setSliceOffset(double offset);

    /**
     * @brief Get current slice offset
     * @return Offset in mm
     */
    [[nodiscard]] double getSliceOffset() const;

    /**
     * @brief Get valid range of slice offsets
     * @return [min, max] offset range in mm
     */
    [[nodiscard]] std::pair<double, double> getSliceRange() const;

    /**
     * @brief Scroll by a number of slices
     * @param delta Number of slices to scroll (positive = forward along normal)
     */
    void scrollSlice(int delta);

    // ==================== Plane Query ====================

    /**
     * @brief Get current plane definition
     * @return Current oblique plane parameters
     */
    [[nodiscard]] ObliquePlaneDefinition getCurrentPlane() const;

    /**
     * @brief Get the current reslice transformation matrix
     * @return 4x4 transformation matrix
     */
    [[nodiscard]] vtkSmartPointer<vtkMatrix4x4> getResliceMatrix() const;

    /**
     * @brief Get the plane normal vector
     * @return Normal vector in world coordinates
     */
    [[nodiscard]] Vector3D getPlaneNormal() const;

    // ==================== Interactive Manipulation ====================

    /**
     * @brief Start interactive rotation from a mouse position
     * @param x Screen X coordinate
     * @param y Screen Y coordinate
     */
    void startInteractiveRotation(int x, int y);

    /**
     * @brief Update interactive rotation with current mouse position
     * @param x Screen X coordinate
     * @param y Screen Y coordinate
     */
    void updateInteractiveRotation(int x, int y);

    /**
     * @brief End interactive rotation
     */
    void endInteractiveRotation();

    /**
     * @brief Check if currently in interactive rotation mode
     * @return true if rotating
     */
    [[nodiscard]] bool isInteractiveRotationActive() const;

    // ==================== Preset Planes ====================

    /**
     * @brief Reset to standard axial plane (XY, looking down Z)
     */
    void setAxial();

    /**
     * @brief Reset to standard coronal plane (XZ, looking down Y)
     */
    void setCoronal();

    /**
     * @brief Reset to standard sagittal plane (YZ, looking down X)
     */
    void setSagittal();

    // ==================== Rendering ====================

    /**
     * @brief Set the VTK renderer for display
     * @param renderer VTK renderer
     */
    void setRenderer(vtkRenderer* renderer);

    /**
     * @brief Get current VTK renderer
     * @return VTK renderer or nullptr
     */
    [[nodiscard]] vtkRenderer* getRenderer() const;

    /**
     * @brief Set reslice options
     * @param options Reslice configuration
     */
    void setOptions(const ObliqueResliceOptions& options);

    /**
     * @brief Get current reslice options
     * @return Current options
     */
    [[nodiscard]] ObliqueResliceOptions getOptions() const;

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
    [[nodiscard]] std::pair<double, double> getWindowLevel() const;

    /**
     * @brief Update the rendering pipeline
     */
    void update();

    /**
     * @brief Reset view to center of volume with standard orientation
     */
    void resetView();

    // ==================== Coordinate Transforms ====================

    /**
     * @brief Convert screen coordinates to world coordinates on the plane
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @return World coordinates, or nullopt if invalid
     */
    [[nodiscard]] std::optional<Point3D> screenToWorld(int screenX, int screenY) const;

    /**
     * @brief Convert world coordinates to screen coordinates
     * @param world World point
     * @return Screen coordinates {x, y}, or nullopt if not visible
     */
    [[nodiscard]] std::optional<std::array<int, 2>> worldToScreen(const Point3D& world) const;

    // ==================== Callbacks ====================

    /**
     * @brief Register callback for plane orientation changes
     * @param callback Function to call when plane changes
     */
    void setPlaneChangedCallback(PlaneChangedCallback callback);

    /**
     * @brief Register callback for slice offset changes
     * @param callback Function to call when slice changes
     */
    void setSliceChangedCallback(SliceChangedCallback callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

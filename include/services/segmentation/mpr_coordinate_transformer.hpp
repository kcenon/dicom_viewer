#pragma once

#include <array>
#include <optional>

#include "services/mpr_renderer.hpp"
#include "manual_segmentation_controller.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief Utility for coordinate transformation between MPR views and 3D label map
 *
 * Handles conversion between:
 * - 2D screen coordinates (mouse position in pixels)
 * - 2D image coordinates on the current slice
 * - 3D world coordinates (mm)
 * - 3D label map indices (voxel indices)
 *
 * Each MPR plane (Axial, Coronal, Sagittal) has a different mapping
 * between 2D display coordinates and 3D volume indices.
 *
 * @trace SRS-FR-023, SRS-FR-008
 */
class MPRCoordinateTransformer {
public:
    /**
     * @brief 3D index in the label map (voxel coordinates)
     */
    struct Index3D {
        int x = 0;
        int y = 0;
        int z = 0;

        [[nodiscard]] bool isValid() const noexcept {
            return x >= 0 && y >= 0 && z >= 0;
        }

        [[nodiscard]] bool operator==(const Index3D& other) const noexcept {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    /**
     * @brief 3D world coordinates (in mm)
     */
    struct WorldPoint3D {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    /**
     * @brief Result of coordinate transformation for segmentation
     */
    struct SegmentationCoordinates {
        Point2D point2D;      ///< 2D point for ManualSegmentationController
        int sliceIndex;       ///< Slice index for the drawing plane
        Index3D index3D;      ///< 3D index in label map
    };

    MPRCoordinateTransformer();
    ~MPRCoordinateTransformer();

    // Non-copyable but movable
    MPRCoordinateTransformer(const MPRCoordinateTransformer&) = delete;
    MPRCoordinateTransformer& operator=(const MPRCoordinateTransformer&) = delete;
    MPRCoordinateTransformer(MPRCoordinateTransformer&&) noexcept;
    MPRCoordinateTransformer& operator=(MPRCoordinateTransformer&&) noexcept;

    /**
     * @brief Set the image data for coordinate calculations
     * @param imageData VTK image data with origin, spacing, and dimensions
     */
    void setImageData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Get image dimensions
     * @return {width, height, depth} or {0,0,0} if not set
     */
    [[nodiscard]] std::array<int, 3> getDimensions() const;

    /**
     * @brief Get image spacing
     * @return {spacingX, spacingY, spacingZ} in mm
     */
    [[nodiscard]] std::array<double, 3> getSpacing() const;

    /**
     * @brief Get image origin
     * @return {originX, originY, originZ} in world coordinates
     */
    [[nodiscard]] std::array<double, 3> getOrigin() const;

    /**
     * @brief Convert world coordinates to 3D label map index
     *
     * @param worldX World X coordinate (mm)
     * @param worldY World Y coordinate (mm)
     * @param worldZ World Z coordinate (mm)
     * @return 3D index if within bounds, nullopt otherwise
     */
    [[nodiscard]] std::optional<Index3D> worldToIndex(
        double worldX, double worldY, double worldZ) const;

    /**
     * @brief Convert 3D label map index to world coordinates
     *
     * @param index 3D index in label map
     * @return World coordinates (center of voxel)
     */
    [[nodiscard]] WorldPoint3D indexToWorld(const Index3D& index) const;

    /**
     * @brief Convert 2D coordinates on an MPR plane to 3D label map index
     *
     * Maps a 2D point on the specified MPR plane to the corresponding
     * 3D index in the label map.
     *
     * @param plane MPR plane type (Axial, Coronal, Sagittal)
     * @param x 2D X coordinate on the plane (in image pixels)
     * @param y 2D Y coordinate on the plane (in image pixels)
     * @param slicePosition Current slice position in world coordinates
     * @return 3D index if valid, nullopt otherwise
     */
    [[nodiscard]] std::optional<Index3D> planeCoordToIndex(
        MPRPlane plane, int x, int y, double slicePosition) const;

    /**
     * @brief Convert 3D label map index to 2D coordinates on an MPR plane
     *
     * @param plane MPR plane type
     * @param index 3D index in label map
     * @return 2D coordinates on the plane, or nullopt if not on current slice
     */
    [[nodiscard]] std::optional<Point2D> indexToPlaneCoord(
        MPRPlane plane, const Index3D& index) const;

    /**
     * @brief Get the slice index for a given world position on an MPR plane
     *
     * @param plane MPR plane type
     * @param worldPosition Position in world coordinates (mm)
     * @return Slice index (Z for Axial, Y for Coronal, X for Sagittal)
     */
    [[nodiscard]] int worldPositionToSliceIndex(
        MPRPlane plane, double worldPosition) const;

    /**
     * @brief Get world position for a given slice index on an MPR plane
     *
     * @param plane MPR plane type
     * @param sliceIndex Slice index
     * @return World position (mm)
     */
    [[nodiscard]] double sliceIndexToWorldPosition(
        MPRPlane plane, int sliceIndex) const;

    /**
     * @brief Convert MPR view coordinates to segmentation coordinates
     *
     * This is the main entry point for segmentation operations.
     * Takes 2D mouse coordinates on an MPR view and returns all
     * necessary coordinates for the ManualSegmentationController.
     *
     * @param plane MPR plane type
     * @param viewX X coordinate in view pixels
     * @param viewY Y coordinate in view pixels
     * @param slicePosition Current slice position in world coordinates
     * @return Segmentation coordinates if valid, nullopt otherwise
     */
    [[nodiscard]] std::optional<SegmentationCoordinates> transformForSegmentation(
        MPRPlane plane, int viewX, int viewY, double slicePosition) const;

    /**
     * @brief Get the slice range for a plane
     * @param plane MPR plane type
     * @return {minIndex, maxIndex} inclusive
     */
    [[nodiscard]] std::pair<int, int> getSliceRange(MPRPlane plane) const;

    /**
     * @brief Check if an index is within valid bounds
     * @param index 3D index to check
     * @return true if within bounds
     */
    [[nodiscard]] bool isValidIndex(const Index3D& index) const;

    /**
     * @brief Get axis indices for a plane
     *
     * Returns which axes of the 3D volume correspond to the 2D plane axes.
     * For example, Axial plane maps X→X, Y→Y with Z as slice axis.
     *
     * @param plane MPR plane type
     * @return {horizontalAxis, verticalAxis, sliceAxis} indices (0=X, 1=Y, 2=Z)
     */
    [[nodiscard]] std::array<int, 3> getPlaneAxisMapping(MPRPlane plane) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

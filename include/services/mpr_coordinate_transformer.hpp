#pragma once

#include "services/mpr_renderer.hpp"

#include <array>
#include <optional>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>

namespace dicom_viewer::services {

/**
 * @brief 2D screen coordinates for MPR view interaction
 */
struct ScreenCoordinate {
    double x = 0.0;
    double y = 0.0;

    ScreenCoordinate() = default;
    ScreenCoordinate(double px, double py) : x(px), y(py) {}
};

/**
 * @brief 3D volume coordinates
 */
struct VolumeCoordinate {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    VolumeCoordinate() = default;
    VolumeCoordinate(double px, double py, double pz) : x(px), y(py), z(pz) {}

    [[nodiscard]] std::array<double, 3> toArray() const {
        return {x, y, z};
    }
};

/**
 * @brief Image voxel indices
 */
struct VoxelIndex {
    int i = 0;
    int j = 0;
    int k = 0;

    VoxelIndex() = default;
    VoxelIndex(int pi, int pj, int pk) : i(pi), j(pj), k(pk) {}

    [[nodiscard]] bool isValid(const std::array<int, 3>& dimensions) const {
        return i >= 0 && i < dimensions[0] &&
               j >= 0 && j < dimensions[1] &&
               k >= 0 && k < dimensions[2];
    }
};

/**
 * @brief Transforms coordinates between MPR view screen space and volume space
 *
 * Provides conversion between:
 * - Screen coordinates (2D pixel position in MPR view)
 * - Volume coordinates (3D world coordinates)
 * - Voxel indices (integer indices into the image volume)
 *
 * @trace SRS-FR-023, SRS-FR-008
 */
class MPRCoordinateTransformer {
public:
    MPRCoordinateTransformer();
    ~MPRCoordinateTransformer();

    // Non-copyable, movable
    MPRCoordinateTransformer(const MPRCoordinateTransformer&) = delete;
    MPRCoordinateTransformer& operator=(const MPRCoordinateTransformer&) = delete;
    MPRCoordinateTransformer(MPRCoordinateTransformer&&) noexcept;
    MPRCoordinateTransformer& operator=(MPRCoordinateTransformer&&) noexcept;

    /**
     * @brief Set the input volume data for coordinate calculations
     * @param imageData VTK image data (3D volume)
     */
    void setImageData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Get image dimensions
     * @return [width, height, depth]
     */
    [[nodiscard]] std::array<int, 3> getDimensions() const;

    /**
     * @brief Get image spacing
     * @return [spacingX, spacingY, spacingZ]
     */
    [[nodiscard]] std::array<double, 3> getSpacing() const;

    /**
     * @brief Get image origin
     * @return [originX, originY, originZ]
     */
    [[nodiscard]] std::array<double, 3> getOrigin() const;

    /**
     * @brief Transform screen coordinates to volume coordinates
     *
     * Given a 2D screen position in an MPR view, calculates the
     * corresponding 3D volume coordinates based on the current
     * slice position.
     *
     * @param screen Screen coordinates in the MPR view
     * @param plane Current MPR plane (Axial, Coronal, Sagittal)
     * @param slicePosition Current slice position in world coordinates
     * @return Volume coordinates, or nullopt if image data not set
     */
    [[nodiscard]] std::optional<VolumeCoordinate>
    screenToVolume(const ScreenCoordinate& screen,
                   MPRPlane plane,
                   double slicePosition) const;

    /**
     * @brief Transform volume coordinates to screen coordinates
     *
     * @param volume Volume coordinates
     * @param plane Target MPR plane
     * @return Screen coordinates for the given plane
     */
    [[nodiscard]] std::optional<ScreenCoordinate>
    volumeToScreen(const VolumeCoordinate& volume, MPRPlane plane) const;

    /**
     * @brief Transform volume coordinates to voxel indices
     *
     * @param volume Volume coordinates
     * @return Voxel indices (may be outside image bounds)
     */
    [[nodiscard]] VoxelIndex volumeToVoxel(const VolumeCoordinate& volume) const;

    /**
     * @brief Transform voxel indices to volume coordinates
     *
     * @param voxel Voxel indices
     * @return Volume coordinates at the voxel center
     */
    [[nodiscard]] VolumeCoordinate voxelToVolume(const VoxelIndex& voxel) const;

    /**
     * @brief Transform screen coordinates directly to voxel indices
     *
     * Convenience method combining screenToVolume and volumeToVoxel.
     *
     * @param screen Screen coordinates
     * @param plane Current MPR plane
     * @param slicePosition Current slice position
     * @return Voxel indices, or nullopt if transformation fails
     */
    [[nodiscard]] std::optional<VoxelIndex>
    screenToVoxel(const ScreenCoordinate& screen,
                  MPRPlane plane,
                  double slicePosition) const;

    /**
     * @brief Get the slice index for a plane at given world position
     *
     * @param plane MPR plane
     * @param worldPosition Position in world coordinates
     * @return Slice index (clamped to valid range)
     */
    [[nodiscard]] int getSliceIndex(MPRPlane plane, double worldPosition) const;

    /**
     * @brief Get the world position for a plane at given slice index
     *
     * @param plane MPR plane
     * @param sliceIndex Slice index
     * @return World position
     */
    [[nodiscard]] double getWorldPosition(MPRPlane plane, int sliceIndex) const;

    /**
     * @brief Get the slice range for a plane
     *
     * @param plane MPR plane
     * @return [minIndex, maxIndex] slice range
     */
    [[nodiscard]] std::pair<int, int> getSliceRange(MPRPlane plane) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "coordinate_types.hpp"
#include "services/mpr_renderer.hpp"

#include <array>
#include <memory>
#include <optional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace dicom_viewer::services::coordinate {

/**
 * @brief Unified coordinate transformer for MPR views
 *
 * Transforms coordinates between:
 * - Screen coordinates (2D pixel position in MPR view)
 * - World coordinates (3D physical coordinates in mm)
 * - Voxel indices (integer indices into the image volume)
 *
 * Supports both rendering operations (screen ↔ world ↔ voxel) and
 * segmentation operations (with plane-aware transformations).
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

    // ==================== Core Setup ====================

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

    // ==================== World ↔ Voxel Transformations ====================

    /**
     * @brief Convert world coordinates to voxel indices
     * @param worldX World X coordinate (mm)
     * @param worldY World Y coordinate (mm)
     * @param worldZ World Z coordinate (mm)
     * @return Voxel indices if within bounds, nullopt otherwise
     */
    [[nodiscard]] std::optional<VoxelIndex> worldToVoxel(
        double worldX, double worldY, double worldZ) const;

    /**
     * @brief Convert world coordinates to voxel indices (overload)
     * @param world World coordinates
     * @return Voxel indices (may be outside image bounds)
     */
    [[nodiscard]] VoxelIndex worldToVoxel(const WorldCoordinate& world) const;

    /**
     * @brief Convert voxel indices to world coordinates
     * @param voxel Voxel indices
     * @return World coordinates at voxel center
     */
    [[nodiscard]] WorldCoordinate voxelToWorld(const VoxelIndex& voxel) const;

    // ==================== Screen ↔ World Transformations ====================

    /**
     * @brief Transform screen coordinates to world coordinates
     * @param screen Screen coordinates in the MPR view
     * @param plane Current MPR plane (Axial, Coronal, Sagittal)
     * @param slicePosition Current slice position in world coordinates
     * @return World coordinates, or nullopt if image data not set
     */
    [[nodiscard]] std::optional<WorldCoordinate> screenToWorld(
        const ScreenCoordinate& screen,
        MPRPlane plane,
        double slicePosition) const;

    /**
     * @brief Transform world coordinates to screen coordinates
     * @param world World coordinates
     * @param plane Target MPR plane
     * @return Screen coordinates for the given plane
     */
    [[nodiscard]] std::optional<ScreenCoordinate> worldToScreen(
        const WorldCoordinate& world,
        MPRPlane plane) const;

    // ==================== Screen ↔ Voxel Transformations ====================

    /**
     * @brief Transform screen coordinates directly to voxel indices
     * @param screen Screen coordinates
     * @param plane Current MPR plane
     * @param slicePosition Current slice position
     * @return Voxel indices, or nullopt if transformation fails
     */
    [[nodiscard]] std::optional<VoxelIndex> screenToVoxel(
        const ScreenCoordinate& screen,
        MPRPlane plane,
        double slicePosition) const;

    // ==================== Plane Coordinate ↔ Voxel Transformations ====================

    /**
     * @brief Convert 2D coordinates on an MPR plane to voxel indices
     * @param plane MPR plane type (Axial, Coronal, Sagittal)
     * @param x 2D X coordinate on the plane (in image pixels)
     * @param y 2D Y coordinate on the plane (in image pixels)
     * @param slicePosition Current slice position in world coordinates
     * @return Voxel indices if valid, nullopt otherwise
     */
    [[nodiscard]] std::optional<VoxelIndex> planeCoordToVoxel(
        MPRPlane plane, int x, int y, double slicePosition) const;

    /**
     * @brief Convert voxel indices to 2D coordinates on an MPR plane
     * @param plane MPR plane type
     * @param voxel Voxel indices
     * @return 2D coordinates on the plane, or nullopt if invalid
     */
    [[nodiscard]] std::optional<Point2D> voxelToPlaneCoord(
        MPRPlane plane, const VoxelIndex& voxel) const;

    // ==================== Slice Index Operations ====================

    /**
     * @brief Get the slice index for a plane at given world position
     * @param plane MPR plane
     * @param worldPosition Position in world coordinates
     * @return Slice index (clamped to valid range for getSliceIndex,
     *         raw for worldPositionToSliceIndex)
     */
    [[nodiscard]] int getSliceIndex(MPRPlane plane, double worldPosition) const;

    /**
     * @brief Get the world position for a plane at given slice index
     * @param plane MPR plane
     * @param sliceIndex Slice index
     * @return World position
     */
    [[nodiscard]] double getWorldPosition(MPRPlane plane, int sliceIndex) const;

    /**
     * @brief Get the slice range for a plane
     * @param plane MPR plane
     * @return [minIndex, maxIndex] slice range
     */
    [[nodiscard]] std::pair<int, int> getSliceRange(MPRPlane plane) const;

    // ==================== Segmentation Support ====================

    /**
     * @brief Convert MPR view coordinates to segmentation coordinates
     *
     * Main entry point for segmentation operations. Takes 2D mouse coordinates
     * on an MPR view and returns all necessary coordinates for
     * ManualSegmentationController.
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
     * @brief Get axis indices for a plane
     *
     * Returns which axes of the 3D volume correspond to the 2D plane axes.
     * For example, Axial plane maps X→X, Y→Y with Z as slice axis.
     *
     * @param plane MPR plane type
     * @return {horizontalAxis, verticalAxis, sliceAxis} indices (0=X, 1=Y, 2=Z)
     */
    [[nodiscard]] std::array<int, 3> getPlaneAxisMapping(MPRPlane plane) const;

    // ==================== Validation ====================

    /**
     * @brief Check if a voxel index is within valid bounds
     * @param voxel Voxel indices to check
     * @return true if within bounds
     */
    [[nodiscard]] bool isValidVoxel(const VoxelIndex& voxel) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services::coordinate

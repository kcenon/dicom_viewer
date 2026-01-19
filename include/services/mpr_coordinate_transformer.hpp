#pragma once

#include "services/coordinate/mpr_coordinate_transformer.hpp"
#include "services/mpr_renderer.hpp"

#include <array>
#include <memory>
#include <optional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace dicom_viewer::services {

// Type aliases for backward compatibility with rendering code
using ScreenCoordinate = coordinate::ScreenCoordinate;
using VolumeCoordinate = coordinate::WorldCoordinate;
using VoxelIndex = coordinate::VoxelIndex;

/**
 * @brief Backward-compatible wrapper for MPRCoordinateTransformer
 *
 * This class provides the original rendering-focused API while delegating
 * to the unified coordinate::MPRCoordinateTransformer implementation.
 *
 * @deprecated Use coordinate::MPRCoordinateTransformer directly for new code.
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

    void setImageData(vtkSmartPointer<vtkImageData> imageData);

    [[nodiscard]] std::array<int, 3> getDimensions() const;
    [[nodiscard]] std::array<double, 3> getSpacing() const;
    [[nodiscard]] std::array<double, 3> getOrigin() const;

    [[nodiscard]] std::optional<VolumeCoordinate>
    screenToVolume(const ScreenCoordinate& screen,
                   MPRPlane plane,
                   double slicePosition) const;

    [[nodiscard]] std::optional<ScreenCoordinate>
    volumeToScreen(const VolumeCoordinate& volume, MPRPlane plane) const;

    [[nodiscard]] VoxelIndex volumeToVoxel(const VolumeCoordinate& volume) const;

    [[nodiscard]] VolumeCoordinate voxelToVolume(const VoxelIndex& voxel) const;

    [[nodiscard]] std::optional<VoxelIndex>
    screenToVoxel(const ScreenCoordinate& screen,
                  MPRPlane plane,
                  double slicePosition) const;

    [[nodiscard]] int getSliceIndex(MPRPlane plane, double worldPosition) const;

    [[nodiscard]] double getWorldPosition(MPRPlane plane, int sliceIndex) const;

    [[nodiscard]] std::pair<int, int> getSliceRange(MPRPlane plane) const;

private:
    std::unique_ptr<coordinate::MPRCoordinateTransformer> impl_;
};

}  // namespace dicom_viewer::services

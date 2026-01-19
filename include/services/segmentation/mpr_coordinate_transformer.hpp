#pragma once

#include "services/coordinate/mpr_coordinate_transformer.hpp"
#include "services/mpr_renderer.hpp"
#include "manual_segmentation_controller.hpp"

#include <array>
#include <memory>
#include <optional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief Backward-compatible wrapper for segmentation coordinate transformer
 *
 * This class provides the original segmentation-focused API while delegating
 * to the unified coordinate::MPRCoordinateTransformer implementation.
 *
 * @deprecated Use coordinate::MPRCoordinateTransformer directly for new code.
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
        Point2D point2D;
        int sliceIndex;
        Index3D index3D;
    };

    MPRCoordinateTransformer();
    ~MPRCoordinateTransformer();

    // Non-copyable but movable
    MPRCoordinateTransformer(const MPRCoordinateTransformer&) = delete;
    MPRCoordinateTransformer& operator=(const MPRCoordinateTransformer&) = delete;
    MPRCoordinateTransformer(MPRCoordinateTransformer&&) noexcept;
    MPRCoordinateTransformer& operator=(MPRCoordinateTransformer&&) noexcept;

    void setImageData(vtkSmartPointer<vtkImageData> imageData);

    [[nodiscard]] std::array<int, 3> getDimensions() const;
    [[nodiscard]] std::array<double, 3> getSpacing() const;
    [[nodiscard]] std::array<double, 3> getOrigin() const;

    [[nodiscard]] std::optional<Index3D> worldToIndex(
        double worldX, double worldY, double worldZ) const;

    [[nodiscard]] WorldPoint3D indexToWorld(const Index3D& index) const;

    [[nodiscard]] std::optional<Index3D> planeCoordToIndex(
        MPRPlane plane, int x, int y, double slicePosition) const;

    [[nodiscard]] std::optional<Point2D> indexToPlaneCoord(
        MPRPlane plane, const Index3D& index) const;

    [[nodiscard]] int worldPositionToSliceIndex(
        MPRPlane plane, double worldPosition) const;

    [[nodiscard]] double sliceIndexToWorldPosition(
        MPRPlane plane, int sliceIndex) const;

    [[nodiscard]] std::optional<SegmentationCoordinates> transformForSegmentation(
        MPRPlane plane, int viewX, int viewY, double slicePosition) const;

    [[nodiscard]] std::pair<int, int> getSliceRange(MPRPlane plane) const;

    [[nodiscard]] bool isValidIndex(const Index3D& index) const;

    [[nodiscard]] std::array<int, 3> getPlaneAxisMapping(MPRPlane plane) const;

private:
    std::unique_ptr<coordinate::MPRCoordinateTransformer> impl_;
};

}  // namespace dicom_viewer::services

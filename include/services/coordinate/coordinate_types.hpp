#pragma once

#include <array>

namespace dicom_viewer::services::coordinate {

/**
 * @brief 2D screen/view coordinates for MPR view interaction
 */
struct ScreenCoordinate {
    double x = 0.0;
    double y = 0.0;

    ScreenCoordinate() = default;
    ScreenCoordinate(double px, double py) : x(px), y(py) {}

    [[nodiscard]] bool operator==(const ScreenCoordinate& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

/**
 * @brief 3D world coordinates (in mm, physical space)
 */
struct WorldCoordinate {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    WorldCoordinate() = default;
    WorldCoordinate(double px, double py, double pz) : x(px), y(py), z(pz) {}

    [[nodiscard]] std::array<double, 3> toArray() const {
        return {x, y, z};
    }

    [[nodiscard]] bool operator==(const WorldCoordinate& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @brief 3D voxel indices (integer indices into image volume)
 */
struct VoxelIndex {
    int i = 0;
    int j = 0;
    int k = 0;

    VoxelIndex() = default;
    VoxelIndex(int pi, int pj, int pk) : i(pi), j(pj), k(pk) {}

    [[nodiscard]] bool isValid() const noexcept {
        return i >= 0 && j >= 0 && k >= 0;
    }

    [[nodiscard]] bool isValid(const std::array<int, 3>& dimensions) const {
        return i >= 0 && i < dimensions[0] &&
               j >= 0 && j < dimensions[1] &&
               k >= 0 && k < dimensions[2];
    }

    [[nodiscard]] bool operator==(const VoxelIndex& other) const noexcept {
        return i == other.i && j == other.j && k == other.k;
    }
};

/**
 * @brief 2D point for drawing operations
 */
struct Point2D {
    int x = 0;
    int y = 0;

    Point2D() = default;
    Point2D(int px, int py) : x(px), y(py) {}

    [[nodiscard]] bool operator==(const Point2D& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

/**
 * @brief Result of coordinate transformation for segmentation operations
 */
struct SegmentationCoordinates {
    Point2D point2D;      ///< 2D point for ManualSegmentationController
    int sliceIndex;       ///< Slice index for the drawing plane
    VoxelIndex index3D;   ///< 3D index in label map
};

}  // namespace dicom_viewer::services::coordinate

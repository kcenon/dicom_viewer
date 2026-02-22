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

/**
 * @file coordinate_types.hpp
 * @brief Coordinate type definitions for the MPR coordinate system
 * @details Defines the ScreenCoordinate struct representing 2D viewport
 *          coordinates with x, y integer positions and equality
 *          comparison support.
 *
 * @author kcenon
 * @since 1.0.0
 */

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

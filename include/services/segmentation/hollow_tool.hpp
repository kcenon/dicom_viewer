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

#include "threshold_segmenter.hpp"

#include <expected>

#include <itkImage.h>

namespace dicom_viewer::services {

/**
 * @brief Shell extraction direction
 */
enum class HollowDirection {
    Inside,   ///< Shell grows inward from original surface
    Outside,  ///< Shell grows outward from original surface
    Both      ///< Shell extends in both directions
};

/**
 * @brief Creates hollow shell masks from solid segmentation masks
 *
 * Extracts the boundary region of a binary mask at a configurable
 * thickness. Useful for visualizing vessel walls and other thin
 * structures without the filled interior.
 *
 * Shell thickness is specified in millimeters and converted to voxels
 * using the image spacing. For anisotropic spacing, the minimum
 * spacing dimension is used (conservative approach).
 *
 * @trace SRS-FR-025
 */
class HollowTool {
public:
    using BinaryMaskType = itk::Image<uint8_t, 3>;

    /**
     * @brief Configuration for hollow operation
     */
    struct Config {
        double thicknessMm = 1.0;    ///< Shell thickness in millimeters
        HollowDirection direction = HollowDirection::Inside;
        uint8_t foregroundValue = 1;
    };

    /**
     * @brief Create a hollow shell from a solid mask
     *
     * @param input Solid binary mask
     * @param config Hollow operation configuration
     * @return Shell mask or error
     */
    [[nodiscard]] static std::expected<BinaryMaskType::Pointer, SegmentationError>
    makeHollow(BinaryMaskType::Pointer input, const Config& config);

    /**
     * @brief Create a hollow shell with default configuration (inside, 1mm)
     *
     * @param input Solid binary mask
     * @param thicknessMm Shell thickness in millimeters
     * @return Shell mask or error
     */
    [[nodiscard]] static std::expected<BinaryMaskType::Pointer, SegmentationError>
    makeHollow(BinaryMaskType::Pointer input, double thicknessMm = 1.0);

    /**
     * @brief Convert thickness in mm to voxel radius
     *
     * Uses the minimum spacing dimension for conservative estimation.
     *
     * @param image Image to get spacing from
     * @param thicknessMm Desired thickness in millimeters
     * @return Radius in voxels (minimum 1)
     */
    [[nodiscard]] static int mmToVoxelRadius(
        const BinaryMaskType* image, double thicknessMm);
};

}  // namespace dicom_viewer::services

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

#include <expected>
#include <vector>

#include <itkImage.h>

#include "services/segmentation/threshold_segmenter.hpp"

namespace dicom_viewer::services {

/**
 * @brief Boolean operations between segmentation label maps
 *
 * Provides voxel-wise set operations on 3D label maps:
 * - Union: A ∪ B (combine, A takes priority at overlap)
 * - Difference: A \ B (remove B-labeled voxels from A)
 * - Intersection: A ∩ B (keep only overlapping labeled voxels)
 *
 * All operations produce a NEW label map, preserving originals.
 * Input maps must have identical dimensions, spacing, and origin.
 *
 * @trace SRS-FR-023
 */
class MaskBooleanOperations {
public:
    using LabelMapType = itk::Image<uint8_t, 3>;

    /**
     * @brief Union of two label maps (A ∪ B)
     *
     * For each voxel:
     *   result = A if A != 0, else B
     *
     * @param maskA First label map (takes priority at overlap)
     * @param maskB Second label map
     * @return New label map with union result
     */
    [[nodiscard]] static std::expected<LabelMapType::Pointer, SegmentationError>
    computeUnion(LabelMapType::Pointer maskA,
                 LabelMapType::Pointer maskB);

    /**
     * @brief Difference of two label maps (A \ B)
     *
     * For each voxel:
     *   result = A if (A != 0 && B == 0), else 0
     *
     * @param maskA Label map to subtract from
     * @param maskB Label map to subtract
     * @return New label map with difference result
     */
    [[nodiscard]] static std::expected<LabelMapType::Pointer, SegmentationError>
    computeDifference(LabelMapType::Pointer maskA,
                      LabelMapType::Pointer maskB);

    /**
     * @brief Intersection of two label maps (A ∩ B)
     *
     * For each voxel:
     *   result = A if (A != 0 && B != 0), else 0
     *
     * @param maskA First label map (label values come from A)
     * @param maskB Second label map
     * @return New label map with intersection result
     */
    [[nodiscard]] static std::expected<LabelMapType::Pointer, SegmentationError>
    computeIntersection(LabelMapType::Pointer maskA,
                        LabelMapType::Pointer maskB);

    /**
     * @brief Union of multiple label maps
     *
     * Sequentially applies union: result = (...((m[0] ∪ m[1]) ∪ m[2]) ... ∪ m[n])
     * Earlier masks take priority at overlapping voxels.
     *
     * @param masks Vector of label maps (minimum 2)
     * @return New label map with combined union result
     */
    [[nodiscard]] static std::expected<LabelMapType::Pointer, SegmentationError>
    computeUnionMultiple(const std::vector<LabelMapType::Pointer>& masks);

private:
    /**
     * @brief Validate that two masks have compatible geometry
     */
    [[nodiscard]] static std::expected<void, SegmentationError>
    validateCompatibility(LabelMapType::Pointer maskA,
                          LabelMapType::Pointer maskB);

    /**
     * @brief Create a new label map with the same geometry as the source
     */
    [[nodiscard]] static LabelMapType::Pointer
    createOutputMap(LabelMapType::Pointer source);
};

}  // namespace dicom_viewer::services

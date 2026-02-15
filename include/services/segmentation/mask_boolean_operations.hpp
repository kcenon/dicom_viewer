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

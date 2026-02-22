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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <itkImage.h>

#include "services/enhanced_dicom/enhanced_dicom_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Sorts Enhanced DICOM frames using DimensionIndexSequence (0020,9222)
 *
 * Enhanced DICOM IODs organize multi-frame data along multiple dimensions
 * (e.g., spatial position, temporal phase, diffusion direction). This class
 * parses the DimensionIndexSequence to understand the intended organization
 * and sorts/groups frames accordingly.
 *
 * Common dimension patterns:
 * - Cardiac CT:  TemporalPosition -> InStackPosition
 * - Multi-stack MR: StackID -> InStackPosition
 * - Multi-echo MR: EchoNumber -> InStackPosition
 *
 * Falls back to spatial position-based sorting when DimensionIndexSequence
 * is absent.
 *
 * @trace SRS-FR-049, SDS-MOD-008
 */
class DimensionIndexSorter {
public:
    DimensionIndexSorter();
    ~DimensionIndexSorter();

    // Non-copyable, movable
    DimensionIndexSorter(const DimensionIndexSorter&) = delete;
    DimensionIndexSorter& operator=(const DimensionIndexSorter&) = delete;
    DimensionIndexSorter(DimensionIndexSorter&&) noexcept;
    DimensionIndexSorter& operator=(DimensionIndexSorter&&) noexcept;

    /**
     * @brief Parse DimensionIndexSequence (0020,9222) from a DICOM file
     *
     * Reads the top-level DimensionIndexSequence to determine the
     * multi-dimensional organization of frames. Each item defines
     * one dimension axis.
     *
     * @param filePath Path to the Enhanced DICOM file
     * @return DimensionOrganization on success, error if parsing fails
     */
    [[nodiscard]] std::expected<DimensionOrganization, EnhancedDicomError>
    parseDimensionIndex(const std::string& filePath);

    /**
     * @brief Sort frames according to dimension index values
     *
     * Uses lexicographic comparison on the dimension indices in the order
     * specified by DimensionOrganization. Frames are sorted in ascending
     * order along each dimension (outermost first).
     *
     * @param frames Frames to sort (modified in place)
     * @param dimOrg Dimension organization from parseDimensionIndex()
     * @return Sorted frame vector
     */
    [[nodiscard]] std::vector<EnhancedFrameInfo> sortFrames(
        const std::vector<EnhancedFrameInfo>& frames,
        const DimensionOrganization& dimOrg);

    /**
     * @brief Sort frames using spatial position fallback
     *
     * When DimensionIndexSequence is absent, falls back to sorting by
     * projection of ImagePositionPatient onto the slice normal.
     *
     * @param frames Frames to sort
     * @return Spatially sorted frame vector
     */
    [[nodiscard]] std::vector<EnhancedFrameInfo> sortFramesBySpatialPosition(
        const std::vector<EnhancedFrameInfo>& frames);

    /**
     * @brief Group frames by a specific dimension
     *
     * Partitions frames into groups where each group shares the same
     * value for the specified dimension pointer. Useful for separating
     * temporal phases, stacks, or echo numbers.
     *
     * @param frames Sorted frames
     * @param dimensionPointer DICOM tag identifying the dimension to group by
     * @return Map of dimension value to frames in that group
     */
    [[nodiscard]] std::map<int, std::vector<EnhancedFrameInfo>>
    groupByDimension(const std::vector<EnhancedFrameInfo>& frames,
                     uint32_t dimensionPointer);

    /**
     * @brief Reconstruct per-group 3D volumes from multi-dimensional data
     *
     * Groups frames by the outermost dimension and assembles each group
     * into a separate 3D volume. Used for multi-phase cardiac CT where
     * each temporal position becomes one 3D volume.
     *
     * @param info Series metadata
     * @param dimOrg Dimension organization
     * @return Map of outer dimension value to assembled 3D volume
     */
    [[nodiscard]] std::expected<
        std::map<int, itk::Image<short, 3>::Pointer>,
        EnhancedDicomError>
    reconstructVolumes(const EnhancedSeriesInfo& info,
                       const DimensionOrganization& dimOrg);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

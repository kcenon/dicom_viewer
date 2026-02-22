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

#include "services/segmentation/segmentation_command.hpp"

#include <cstdint>
#include <vector>

#include <itkImage.h>

namespace dicom_viewer::services {

/**
 * @brief Record of a single voxel change for diff-based undo
 */
struct VoxelChange {
    size_t linearIndex;   ///< Flat index into label map buffer
    uint8_t oldLabel;     ///< Label value before the operation
    uint8_t newLabel;     ///< Label value after the operation
};

/**
 * @brief Diff-based undoable command for brush stroke operations
 *
 * Stores only the voxels that were changed by the brush stroke,
 * making it memory-efficient for localized edits. Can be used
 * for Brush, Eraser, and Fill operations.
 *
 * Usage:
 * 1. Create command with label map reference
 * 2. Call recordChange() for each voxel modified during the stroke
 * 3. Pass to SegmentationCommandStack::execute()
 *    (execute() is a no-op since changes are recorded during drawing)
 *
 * @trace SRS-FR-023
 */
class BrushStrokeCommand : public ISegmentationCommand {
public:
    using LabelMapType = itk::Image<uint8_t, 3>;

    /**
     * @brief Construct with label map and operation description
     * @param labelMap Label map to modify on undo/redo
     * @param operationDescription Description (e.g., "Brush stroke")
     */
    BrushStrokeCommand(LabelMapType::Pointer labelMap,
                       std::string operationDescription);

    ~BrushStrokeCommand() override = default;

    /**
     * @brief Record a voxel change during the stroke
     *
     * Call this for each voxel modified during the drawing operation.
     * Only records if the old and new labels differ.
     *
     * @param linearIndex Flat index into label map buffer
     * @param oldLabel Previous label value
     * @param newLabel New label value
     */
    void recordChange(size_t linearIndex, uint8_t oldLabel, uint8_t newLabel);

    /**
     * @brief Get the number of recorded voxel changes
     */
    [[nodiscard]] size_t changeCount() const noexcept;

    /**
     * @brief Check if the command has any recorded changes
     */
    [[nodiscard]] bool hasChanges() const noexcept;

    // ISegmentationCommand interface
    void execute() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] size_t memoryUsage() const override;

private:
    LabelMapType::Pointer labelMap_;
    std::vector<VoxelChange> changes_;
    std::string description_;
};

} // namespace dicom_viewer::services

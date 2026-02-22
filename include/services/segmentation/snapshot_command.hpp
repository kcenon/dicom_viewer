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
 * @file snapshot_command.hpp
 * @brief Snapshot-based undoable command for bulk segmentation operations
 * @details Stores RLE-compressed before/after snapshots for operations
 *          modifying large regions (threshold, region growing, morphological).
 *          Captures 'before' on construction and 'after' after operation
 *          completion.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include "services/segmentation/segmentation_command.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <itkImage.h>

namespace dicom_viewer::services {

/**
 * @brief Snapshot-based undoable command for bulk segmentation operations
 *
 * Stores RLE-compressed before/after snapshots of the label map for
 * operations that modify large regions (Threshold, Region Growing,
 * Morphological ops). More memory-efficient than diff-based approach
 * when many voxels change simultaneously.
 *
 * Usage:
 * 1. Create command (captures "before" state automatically)
 * 2. Perform the bulk segmentation operation on the label map
 * 3. Call captureAfterState() to record the result
 * 4. Push to SegmentationCommandStack
 *
 * RLE compression format: [value(1 byte), count(4 bytes LE)] per run.
 * A 256^3 label map with mostly background compresses from 16MB to ~KB.
 *
 * @trace SRS-FR-023
 */
class SnapshotCommand : public ISegmentationCommand {
public:
    using LabelMapType = itk::Image<uint8_t, 3>;

    /**
     * @brief Construct and capture "before" state
     * @param labelMap Label map to snapshot (compressed copy taken immediately)
     * @param operationDescription Human-readable description
     */
    SnapshotCommand(LabelMapType::Pointer labelMap,
                    std::string operationDescription);

    ~SnapshotCommand() override = default;

    /**
     * @brief Capture "after" state of the label map
     *
     * Must be called after the bulk operation modifies the label map.
     * The command is incomplete until this is called.
     */
    void captureAfterState();

    /**
     * @brief Check if the after state has been captured
     */
    [[nodiscard]] bool isComplete() const noexcept;

    // ISegmentationCommand interface
    void execute() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] size_t memoryUsage() const override;

    // --- RLE compression utilities (public for testing) ---

    /**
     * @brief Compress data using Run-Length Encoding
     *
     * Format: sequence of (value, count) pairs where value is 1 byte
     * and count is 4 bytes little-endian. 5 bytes per run.
     *
     * @param data Input buffer
     * @param numElements Number of elements to compress
     * @return Compressed data
     */
    [[nodiscard]] static std::vector<uint8_t>
    compressRLE(const uint8_t* data, size_t numElements);

    /**
     * @brief Decompress RLE data back to raw buffer
     * @param compressed RLE-compressed data
     * @param output Output buffer (must be pre-allocated)
     * @param numElements Expected number of output elements
     */
    static void decompressRLE(const std::vector<uint8_t>& compressed,
                              uint8_t* output, size_t numElements);

private:
    void restoreState(const std::vector<uint8_t>& compressed);

    LabelMapType::Pointer labelMap_;
    std::vector<uint8_t> beforeState_;
    std::vector<uint8_t> afterState_;
    std::string description_;
    size_t totalVoxels_ = 0;
};

}  // namespace dicom_viewer::services

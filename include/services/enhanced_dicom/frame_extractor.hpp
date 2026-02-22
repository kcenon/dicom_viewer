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

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include <itkImage.h>

#include "services/enhanced_dicom/enhanced_dicom_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Extracts individual frames from Enhanced DICOM multi-frame pixel data
 *
 * Handles both native (uncompressed) and encapsulated (compressed) pixel data.
 * Uses the Basic Offset Table or Extended Offset Table to locate frame
 * boundaries within the pixel data element.
 *
 * @trace SRS-FR-049
 */
class FrameExtractor {
public:
    FrameExtractor();
    ~FrameExtractor();

    // Non-copyable, movable
    FrameExtractor(const FrameExtractor&) = delete;
    FrameExtractor& operator=(const FrameExtractor&) = delete;
    FrameExtractor(FrameExtractor&&) noexcept;
    FrameExtractor& operator=(FrameExtractor&&) noexcept;

    /**
     * @brief Extract a single frame's raw pixel data
     *
     * @param filePath Path to the Enhanced DICOM file
     * @param frameIndex 0-based frame index
     * @param info Series metadata for pixel format information
     * @return Raw pixel data buffer for the frame
     */
    [[nodiscard]] std::expected<std::vector<char>, EnhancedDicomError>
    extractFrame(const std::string& filePath,
                 int frameIndex,
                 const EnhancedSeriesInfo& info);

    /**
     * @brief Assemble all frames into a 3D ITK volume
     *
     * Reads all frames from the Enhanced DICOM file, applies per-frame
     * rescale parameters, and assembles into a spatially ordered 3D volume.
     *
     * @param filePath Path to the Enhanced DICOM file
     * @param info Series metadata with per-frame spatial information
     * @return 3D ITK image (signed short for CT, unsigned short for MR)
     */
    [[nodiscard]] std::expected<itk::Image<short, 3>::Pointer,
                                EnhancedDicomError>
    assembleVolume(const std::string& filePath,
                   const EnhancedSeriesInfo& info);

    /**
     * @brief Assemble a subset of frames into a 3D volume
     *
     * Used for multi-phase datasets where only a subset of frames
     * (e.g., one cardiac phase) should be assembled into a volume.
     *
     * @param filePath Path to the Enhanced DICOM file
     * @param info Series metadata
     * @param frameIndices Indices of frames to include (sorted by spatial position)
     * @return 3D ITK image for the selected frames
     */
    [[nodiscard]] std::expected<itk::Image<short, 3>::Pointer,
                                EnhancedDicomError>
    assembleVolumeFromFrames(const std::string& filePath,
                             const EnhancedSeriesInfo& info,
                             const std::vector<int>& frameIndices);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

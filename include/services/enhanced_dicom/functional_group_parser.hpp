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

#include <memory>
#include <string>
#include <vector>

#include "services/enhanced_dicom/enhanced_dicom_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Parser for DICOM Functional Group Sequences
 *
 * Extracts metadata from SharedFunctionalGroupsSequence (5200,9229) and
 * PerFrameFunctionalGroupsSequence (5200,9230) in Enhanced DICOM IODs.
 *
 * Shared groups contain metadata common to all frames (e.g., pixel spacing).
 * Per-frame groups contain metadata that varies per frame (e.g., position).
 * Per-frame values override shared values when both are present.
 *
 * @trace SRS-FR-049
 */
class FunctionalGroupParser {
public:
    FunctionalGroupParser();
    ~FunctionalGroupParser();

    // Non-copyable, movable
    FunctionalGroupParser(const FunctionalGroupParser&) = delete;
    FunctionalGroupParser& operator=(const FunctionalGroupParser&) = delete;
    FunctionalGroupParser(FunctionalGroupParser&&) noexcept;
    FunctionalGroupParser& operator=(FunctionalGroupParser&&) noexcept;

    /**
     * @brief Parse shared functional groups from a DICOM metadata dictionary
     *
     * Reads SharedFunctionalGroupsSequence (5200,9229) and extracts
     * common metadata: pixel spacing, pixel measures, pixel value
     * transformation, and frame content.
     *
     * @param filePath Path to the Enhanced DICOM file
     * @param[out] info Series info to populate with shared metadata
     */
    void parseSharedGroups(const std::string& filePath,
                           EnhancedSeriesInfo& info);

    /**
     * @brief Parse per-frame functional groups
     *
     * Reads PerFrameFunctionalGroupsSequence (5200,9230) and extracts
     * per-frame metadata: plane position, plane orientation, pixel value
     * transformation, and frame content.
     *
     * @param filePath Path to the Enhanced DICOM file
     * @param numberOfFrames Number of frames to parse
     * @param sharedInfo Shared metadata for fallback values
     * @return Vector of per-frame metadata
     */
    [[nodiscard]] std::vector<EnhancedFrameInfo> parsePerFrameGroups(
        const std::string& filePath,
        int numberOfFrames,
        const EnhancedSeriesInfo& sharedInfo);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

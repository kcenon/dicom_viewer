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
 * @file enhanced_dicom_parser.hpp
 * @brief Enhanced DICOM multi-frame IOD parser for modern scanner formats
 * @details Detects and parses Enhanced DICOM IODs containing multiple image
 *          frames with shared and per-frame metadata. Supports Enhanced CT,
 *          Enhanced MR, Enhanced XA with progress callbacks for long
 *          operations.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <itkImage.h>

#include "services/enhanced_dicom/enhanced_dicom_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Enhanced DICOM multi-frame IOD parser
 *
 * Detects and parses Enhanced (multi-frame) DICOM IODs where a single file
 * contains multiple image frames with shared and per-frame metadata.
 * Supports Enhanced CT, Enhanced MR, and Enhanced XA Image Storage.
 *
 * This parser works with files from modern scanners (Siemens MAGNETOM,
 * Philips Ingenia, GE Revolution) that output Enhanced IODs by default.
 *
 * @example
 * @code
 * EnhancedDicomParser parser;
 *
 * // Check if file is Enhanced DICOM
 * if (EnhancedDicomParser::isEnhancedDicom(filePath)) {
 *     auto result = parser.parseFile(filePath);
 *     if (result) {
 *         auto info = result.value();
 *         // Access info.numberOfFrames, info.frames, etc.
 *         auto volume = parser.assembleVolume(info);
 *     }
 * }
 * @endcode
 *
 * @trace SRS-FR-049
 */
class EnhancedDicomParser {
public:
    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    EnhancedDicomParser();
    ~EnhancedDicomParser();

    // Non-copyable, movable
    EnhancedDicomParser(const EnhancedDicomParser&) = delete;
    EnhancedDicomParser& operator=(const EnhancedDicomParser&) = delete;
    EnhancedDicomParser(EnhancedDicomParser&&) noexcept;
    EnhancedDicomParser& operator=(EnhancedDicomParser&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Detect if a DICOM file is an Enhanced multi-frame IOD
     *
     * Reads the SOP Class UID and checks against known Enhanced IOD UIDs.
     * Does not require reading the full file — only reads the header.
     *
     * @param filePath Path to a DICOM file
     * @return true if the file is an Enhanced multi-frame IOD
     */
    [[nodiscard]] static bool isEnhancedDicom(const std::string& filePath);

    /**
     * @brief Detect Enhanced IOD by SOP Class UID string
     *
     * @param sopClassUid SOP Class UID to check
     * @return true if this is a known Enhanced IOD SOP Class
     */
    [[nodiscard]] static bool detectEnhancedIOD(const std::string& sopClassUid);

    /**
     * @brief Parse an Enhanced DICOM file and extract all metadata
     *
     * Reads the entire Enhanced DICOM file, parses shared and per-frame
     * functional groups, DimensionIndexSequence, and returns complete
     * series metadata with frames sorted by dimension indices.
     *
     * @param filePath Path to the Enhanced DICOM file
     * @return EnhancedSeriesInfo on success, error on failure
     */
    [[nodiscard]] std::expected<EnhancedSeriesInfo, EnhancedDicomError>
    parseFile(const std::string& filePath);

    /**
     * @brief Get the dimension organization from the last parsed file
     *
     * Available after a successful parseFile() call.
     *
     * @return DimensionOrganization (empty if no DimensionIndexSequence)
     */
    [[nodiscard]] const DimensionOrganization& getDimensionOrganization() const;

    /**
     * @brief Reconstruct per-phase 3D volumes from multi-dimensional data
     *
     * Groups frames by the outermost dimension and assembles each group
     * into a separate 3D volume. Requires a prior successful parseFile().
     *
     * @param info Parsed series info from parseFile()
     * @return Map of outer dimension value to assembled 3D volume
     * @trace SRS-FR-049
     */
    [[nodiscard]] std::expected<
        std::map<int, itk::Image<short, 3>::Pointer>,
        EnhancedDicomError>
    reconstructMultiPhaseVolumes(const EnhancedSeriesInfo& info);

    /**
     * @brief Assemble all frames into a single 3D volume
     *
     * @param info Parsed series info from parseFile()
     * @return 3D ITK image on success
     */
    [[nodiscard]] std::expected<itk::Image<short, 3>::Pointer,
                                EnhancedDicomError>
    assembleVolume(const EnhancedSeriesInfo& info);

    /**
     * @brief Assemble a subset of frames into a 3D volume
     *
     * Useful for multi-phase datasets: assemble only one cardiac phase.
     *
     * @param info Parsed series info
     * @param frameIndices Subset of frame indices to assemble
     * @return 3D ITK image on success
     */
    [[nodiscard]] std::expected<itk::Image<short, 3>::Pointer,
                                EnhancedDicomError>
    assembleVolume(const EnhancedSeriesInfo& info,
                   const std::vector<int>& frameIndices);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

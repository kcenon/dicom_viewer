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
 * @file flow_dicom_parser.hpp
 * @brief 4D Flow MRI DICOM series parser with vendor-specific strategy
 * @details Identifies 4D Flow series from DICOM metadata, selects appropriate
 *          vendor-specific parser (Siemens, Philips, GE), and organizes
 *          frames into a cardiac_phase x velocity_component matrix.
 *          Uses Strategy pattern via IVendorFlowParser for vendor abstraction.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "services/flow/flow_dicom_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief 4D Flow MRI DICOM series parser with vendor-specific strategy
 *
 * Identifies 4D Flow series from DICOM metadata, selects the appropriate
 * vendor-specific parser (Siemens, Philips, GE), and organizes frames
 * into a (cardiac_phase x velocity_component) matrix.
 *
 * Uses the Strategy pattern via IVendorFlowParser for vendor abstraction.
 *
 * @example
 * @code
 * FlowDicomParser parser;
 *
 * // Check if a DICOM directory contains 4D Flow data
 * if (FlowDicomParser::is4DFlowSeries(dicomFiles)) {
 *     auto result = parser.parseSeries(dicomFiles);
 *     if (result) {
 *         auto info = result.value();
 *         // info.phaseCount, info.vendor, info.frameMatrix
 *     }
 * }
 * @endcode
 *
 * @trace SRS-FR-043
 */
class FlowDicomParser {
public:
    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    FlowDicomParser();
    ~FlowDicomParser();

    // Non-copyable, movable
    FlowDicomParser(const FlowDicomParser&) = delete;
    FlowDicomParser& operator=(const FlowDicomParser&) = delete;
    FlowDicomParser(FlowDicomParser&&) noexcept;
    FlowDicomParser& operator=(FlowDicomParser&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Detect if a set of DICOM files constitutes a 4D Flow series
     *
     * Checks DICOM tags:
     * - (0018,0020) Scanning Sequence contains "PC"
     * - (0018,9014) Phase Contrast = "YES"
     * - (0008,0008) Image Type contains "P" or "VELOCITY"
     *
     * @param dicomFiles List of DICOM file paths
     * @return true if the series is a 4D Flow MRI series
     */
    [[nodiscard]] static bool is4DFlowSeries(
        const std::vector<std::string>& dicomFiles);

    /**
     * @brief Detect scanner vendor from DICOM metadata
     *
     * Reads (0008,0070) Manufacturer tag to identify vendor.
     *
     * @param dicomFiles List of DICOM file paths (reads first file)
     * @return Detected vendor type
     */
    [[nodiscard]] static FlowVendorType detectVendor(
        const std::vector<std::string>& dicomFiles);

    /**
     * @brief Parse a complete 4D Flow DICOM series
     *
     * Performs vendor detection, reads all frame metadata, classifies
     * velocity components, and organizes into phase x component matrix.
     *
     * @param dicomFiles List of DICOM file paths
     * @return FlowSeriesInfo on success, FlowError on failure
     */
    [[nodiscard]] std::expected<FlowSeriesInfo, FlowError>
    parseSeries(const std::vector<std::string>& dicomFiles) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

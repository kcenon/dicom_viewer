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

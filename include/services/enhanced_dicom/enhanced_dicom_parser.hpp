#pragma once

#include <expected>
#include <functional>
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
     * functional groups, and returns complete series metadata.
     *
     * @param filePath Path to the Enhanced DICOM file
     * @return EnhancedSeriesInfo on success, error on failure
     */
    [[nodiscard]] std::expected<EnhancedSeriesInfo, EnhancedDicomError>
    parseFile(const std::string& filePath);

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

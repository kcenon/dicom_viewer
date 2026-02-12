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

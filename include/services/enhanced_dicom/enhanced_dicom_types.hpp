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
 * @file enhanced_dicom_types.hpp
 * @brief Data structures and error codes for Enhanced DICOM operations
 * @details Defines EnhancedDicomError with error code enum (Success,
 *          InvalidInput, NotEnhancedIOD, ParseFailed) and detailed
 *          error messages. Core types supporting Enhanced DICOM
 *          processing pipeline.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Error information for Enhanced DICOM operations
 *
 * @trace SRS-FR-049
 */
struct EnhancedDicomError {
    enum class Code {
        Success,
        InvalidInput,
        NotEnhancedIOD,
        ParseFailed,
        MissingTag,
        UnsupportedPixelFormat,
        FrameExtractionFailed,
        InconsistentData,
        InternalError
    };

    Code code = Code::Success;
    std::string message;

    [[nodiscard]] bool isSuccess() const noexcept {
        return code == Code::Success;
    }

    [[nodiscard]] std::string toString() const {
        switch (code) {
            case Code::Success: return "Success";
            case Code::InvalidInput: return "Invalid input: " + message;
            case Code::NotEnhancedIOD: return "Not an Enhanced IOD: " + message;
            case Code::ParseFailed: return "Parse failed: " + message;
            case Code::MissingTag: return "Missing DICOM tag: " + message;
            case Code::UnsupportedPixelFormat:
                return "Unsupported pixel format: " + message;
            case Code::FrameExtractionFailed:
                return "Frame extraction failed: " + message;
            case Code::InconsistentData:
                return "Inconsistent data: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Known Enhanced DICOM SOP Class UIDs
 *
 * @trace SRS-FR-049
 */
namespace enhanced_sop_class {
    inline constexpr const char* EnhancedCTImageStorage =
        "1.2.840.10008.5.1.4.1.1.2.1";
    inline constexpr const char* EnhancedMRImageStorage =
        "1.2.840.10008.5.1.4.1.1.4.1";
    inline constexpr const char* EnhancedXAImageStorage =
        "1.2.840.10008.5.1.4.1.1.12.1.1";
}  // namespace enhanced_sop_class

/**
 * @brief Per-frame metadata extracted from PerFrameFunctionalGroupsSequence
 *
 * Each frame in an Enhanced DICOM file has its own spatial position,
 * orientation, and pixel transformation parameters. These are extracted
 * from the per-frame functional groups and optionally the shared groups.
 *
 * @trace SRS-FR-049
 */
struct EnhancedFrameInfo {
    int frameIndex = 0;

    // Spatial information (from PlanePositionSequence / PlaneOrientationSequence)
    std::array<double, 3> imagePosition = {0.0, 0.0, 0.0};
    std::array<double, 6> imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    double sliceThickness = 1.0;

    // Pixel value transformation (from PixelValueTransformationSequence)
    double rescaleSlope = 1.0;
    double rescaleIntercept = 0.0;

    // Temporal information (optional, for multi-phase datasets)
    std::optional<double> triggerTime;
    std::optional<int> temporalPositionIndex;

    // DimensionIndex values: dimension tag → index value
    std::map<uint32_t, int> dimensionIndices;
};

/**
 * @brief Series-level metadata for Enhanced DICOM multi-frame files
 *
 * Represents the complete parsed result of an Enhanced DICOM file,
 * including shared metadata and per-frame information.
 *
 * @trace SRS-FR-049
 */
struct EnhancedSeriesInfo {
    std::string sopClassUid;
    std::string sopInstanceUid;
    int numberOfFrames = 0;

    // Image dimensions (common to all frames)
    int rows = 0;
    int columns = 0;
    int bitsAllocated = 0;
    int bitsStored = 0;
    int highBit = 0;
    int pixelRepresentation = 0;  // 0 = unsigned, 1 = signed

    // Shared spatial metadata (from SharedFunctionalGroupsSequence)
    double pixelSpacingX = 1.0;
    double pixelSpacingY = 1.0;

    // Per-frame metadata
    std::vector<EnhancedFrameInfo> frames;

    // Patient/Study/Series metadata (inherited from top-level dataset)
    std::string patientId;
    std::string patientName;
    std::string studyInstanceUid;
    std::string seriesInstanceUid;
    std::string seriesDescription;
    std::string modality;

    // Transfer syntax for pixel data handling
    std::string transferSyntaxUid;

    // Source file path
    std::string filePath;
};

/**
 * @brief Represents one dimension in DimensionIndexSequence (0020,9222)
 *
 * Each entry defines a dimension axis used to organize multi-frame data.
 * The dimension order (index in the vector) determines sorting priority:
 * first dimension = outermost loop, last = innermost.
 *
 * @trace SRS-FR-049, SDS-MOD-008
 */
struct DimensionDefinition {
    uint32_t dimensionIndexPointer = 0;       ///< DICOM tag this dimension references
    uint32_t functionalGroupPointer = 0;      ///< Functional group containing the tag
    std::string dimensionOrganizationUID;     ///< Optional grouping UID
    std::string dimensionDescription;         ///< Human-readable label
};

/**
 * @brief Complete dimension organization for an Enhanced DICOM file
 *
 * Parsed from DimensionIndexSequence (0020,9222). The dimension order
 * determines the sorting priority for frame ordering.
 *
 * @trace SRS-FR-049, SDS-MOD-008
 */
struct DimensionOrganization {
    std::vector<DimensionDefinition> dimensions;

    /// Check if a specific dimension pointer is present
    [[nodiscard]] bool hasDimension(uint32_t pointer) const {
        for (const auto& dim : dimensions) {
            if (dim.dimensionIndexPointer == pointer) {
                return true;
            }
        }
        return false;
    }

    /// Get the index position of a dimension (for sorting priority)
    [[nodiscard]] std::optional<size_t> dimensionIndex(uint32_t pointer) const {
        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (dimensions[i].dimensionIndexPointer == pointer) {
                return i;
            }
        }
        return std::nullopt;
    }
};

/// Well-known DICOM tags used as dimension index pointers
namespace dimension_tag {
    inline constexpr uint32_t InStackPositionNumber  = 0x00209057;
    inline constexpr uint32_t TemporalPositionIndex  = 0x00209128;
    inline constexpr uint32_t StackID                = 0x00209056;
    inline constexpr uint32_t DiffusionBValue        = 0x00189087;
    inline constexpr uint32_t EchoNumber             = 0x00180086;
}  // namespace dimension_tag

/**
 * @brief Check if a SOP Class UID is an Enhanced multi-frame IOD
 */
[[nodiscard]] inline bool isEnhancedSopClass(const std::string& sopClassUid) {
    return sopClassUid == enhanced_sop_class::EnhancedCTImageStorage
        || sopClassUid == enhanced_sop_class::EnhancedMRImageStorage
        || sopClassUid == enhanced_sop_class::EnhancedXAImageStorage;
}

/**
 * @brief Convert SOP Class UID to human-readable name
 */
[[nodiscard]] inline std::string enhancedSopClassName(
    const std::string& sopClassUid)
{
    if (sopClassUid == enhanced_sop_class::EnhancedCTImageStorage) {
        return "Enhanced CT Image Storage";
    }
    if (sopClassUid == enhanced_sop_class::EnhancedMRImageStorage) {
        return "Enhanced MR Image Storage";
    }
    if (sopClassUid == enhanced_sop_class::EnhancedXAImageStorage) {
        return "Enhanced XA Image Storage";
    }
    return "Unknown";
}

}  // namespace dicom_viewer::services

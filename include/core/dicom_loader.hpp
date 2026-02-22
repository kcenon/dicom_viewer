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
 * @file dicom_loader.hpp
 * @brief DICOM file loading and metadata extraction
 * @details Provides the DicomLoader class for parsing DICOM files and scanning
 *          directories to extract patient, study, and series metadata.
 *          Supports both single-file loading and batch directory scanning
 *          with progress reporting via callbacks.
 *
 * ## Thread Safety
 * - Directory scanning may be called from background threads
 * - DicomMetadata structs are safe to read from any thread after construction
 * - Individual file loading operations are not thread-safe
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <array>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::core {

/// DICOM metadata extracted from files
struct DicomMetadata {
    // Patient Module
    std::string patientName;
    std::string patientId;
    std::string patientBirthDate;
    std::string patientSex;

    // Study Module
    std::string studyInstanceUid;
    std::string studyDate;
    std::string studyTime;
    std::string studyDescription;
    std::string accessionNumber;

    // Series Module
    std::string seriesInstanceUid;
    std::string seriesNumber;
    std::string seriesDescription;
    std::string modality;

    // Image Module
    int rows = 0;
    int columns = 0;
    int bitsAllocated = 0;
    int bitsStored = 0;
    double pixelSpacingX = 1.0;
    double pixelSpacingY = 1.0;
    double sliceThickness = 1.0;

    // Rescale parameters for HU conversion
    double rescaleSlope = 1.0;
    double rescaleIntercept = 0.0;
};

/// Slice information for sorting
struct SliceInfo {
    std::filesystem::path filePath;
    double sliceLocation = 0.0;
    int instanceNumber = 0;
    std::array<double, 3> imagePosition = {0.0, 0.0, 0.0};
    std::array<double, 6> imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
};

/// Error types for DICOM loading
enum class DicomError {
    FileNotFound,
    InvalidDicomFormat,
    UnsupportedTransferSyntax,
    DecodingFailed,
    MetadataExtractionFailed,
    SeriesAssemblyFailed,
    MemoryAllocationFailed
};

/// Error result with message
struct DicomErrorInfo {
    DicomError code;
    std::string message;
};

/// Image types
using CTImageType = itk::Image<short, 3>;
using MRImageType = itk::Image<unsigned short, 3>;

/**
 * @brief DICOM file loader and series assembler
 *
 * Handles DICOM file parsing, metadata extraction, and 3D volume assembly.
 * Supports multiple transfer syntaxes and compression formats.
 *
 * @trace SRS-FR-001, SRS-FR-002, SRS-FR-003
 */
class DicomLoader {
public:
    DicomLoader();
    ~DicomLoader();

    // Non-copyable, movable
    DicomLoader(const DicomLoader&) = delete;
    DicomLoader& operator=(const DicomLoader&) = delete;
    DicomLoader(DicomLoader&&) noexcept;
    DicomLoader& operator=(DicomLoader&&) noexcept;

    /**
     * @brief Load a single DICOM file and extract metadata
     * @param filePath Path to the DICOM file
     * @return Metadata on success, error info on failure
     */
    std::expected<DicomMetadata, DicomErrorInfo>
    loadFile(const std::filesystem::path& filePath);

    /**
     * @brief Scan directory for DICOM files and group by series
     * @param directoryPath Path to directory containing DICOM files
     * @return Map of Series Instance UID to slice information
     */
    std::expected<std::map<std::string, std::vector<SliceInfo>>, DicomErrorInfo>
    scanDirectory(const std::filesystem::path& directoryPath);

    /**
     * @brief Load a complete CT series as 3D volume
     * @param slices Sorted slice information
     * @return ITK 3D image on success
     */
    std::expected<CTImageType::Pointer, DicomErrorInfo>
    loadCTSeries(const std::vector<SliceInfo>& slices);

    /**
     * @brief Load a complete MR series as 3D volume
     * @param slices Sorted slice information
     * @return ITK 3D image on success
     */
    std::expected<MRImageType::Pointer, DicomErrorInfo>
    loadMRSeries(const std::vector<SliceInfo>& slices);

    /**
     * @brief Check if a DICOM file is an Enhanced multi-frame IOD
     *
     * Reads the SOP Class UID from the file and checks against known
     * Enhanced IOD UIDs (Enhanced CT, Enhanced MR, Enhanced XA).
     * Use EnhancedDicomParser from services/enhanced_dicom/ to load these files.
     *
     * @param filePath Path to the DICOM file
     * @return true if the file uses an Enhanced SOP Class UID
     * @trace SRS-FR-049
     */
    [[nodiscard]] static bool isEnhancedDicom(const std::filesystem::path& filePath);

    /**
     * @brief Get the last loaded metadata
     */
    const DicomMetadata& getMetadata() const { return metadata_; }

    /**
     * @brief Check if a transfer syntax is supported
     */
    static bool isTransferSyntaxSupported(const std::string& transferSyntaxUid);

    /**
     * @brief Get list of supported transfer syntaxes
     */
    static std::vector<std::string> getSupportedTransferSyntaxes();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    DicomMetadata metadata_;

    /// Sort slices by spatial location
    static void sortSlices(std::vector<SliceInfo>& slices);

    /// Calculate Z position from image position and orientation
    static double calculateSlicePosition(const SliceInfo& slice);

    /// Calculate position along given normal direction
    static double calculateSlicePosition(const SliceInfo& slice,
                                         const std::array<double, 3>& normal);
};

} // namespace dicom_viewer::core

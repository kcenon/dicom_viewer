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

#include "services/export/matlab_exporter.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <numeric>

#include <itkImageRegionConstIterator.h>

namespace {

// =========================================================================
// MAT-file Level 5 constants
// =========================================================================

// Data type tags
constexpr int32_t miINT8      = 1;
constexpr int32_t miUINT8     = 2;
constexpr int32_t miINT32     = 5;
constexpr int32_t miUINT32    = 6;
constexpr int32_t miSINGLE    = 7;
constexpr int32_t miMATRIX    = 14;

// Array class flags (stored in upper byte of flags)
constexpr uint8_t mxDOUBLE_CLASS = 6;
constexpr uint8_t mxSINGLE_CLASS = 7;
constexpr uint8_t mxCHAR_CLASS   = 4;
constexpr uint8_t mxSTRUCT_CLASS = 2;

// MAT-file version and endian marker
constexpr uint16_t kMATVersion    = 0x0100;
constexpr uint16_t kEndianMarker  = 0x4D49;  // 'IM' in little-endian

/// Pad size to 8-byte boundary
size_t padTo8(size_t n) {
    return (n + 7) & ~static_cast<size_t>(7);
}

/// Write raw bytes to output buffer
void writeBytes(std::vector<uint8_t>& out, const void* data, size_t n) {
    auto p = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), p, p + n);
}

/// Write zero padding to reach 8-byte alignment
void writePadding(std::vector<uint8_t>& out, size_t dataSize) {
    size_t padded = padTo8(dataSize);
    if (padded > dataSize) {
        out.resize(out.size() + (padded - dataSize), 0);
    }
}

/// Write a data element tag (8 bytes: type + nbytes)
void writeTag(std::vector<uint8_t>& out, int32_t type, int32_t nbytes) {
    writeBytes(out, &type, 4);
    writeBytes(out, &nbytes, 4);
}

/// Write a small data element (tag compressed to 4 bytes when data <= 4 bytes)
void writeSmallElement(std::vector<uint8_t>& out, int32_t type,
                       const void* data, int32_t nbytes) {
    if (nbytes <= 4) {
        // Compressed format: upper 2 bytes = nbytes, lower 2 bytes = type
        uint32_t packed = (static_cast<uint32_t>(nbytes) << 16) |
                          (static_cast<uint32_t>(type) & 0xFFFF);
        writeBytes(out, &packed, 4);
        uint8_t buf[4] = {0};
        std::memcpy(buf, data, nbytes);
        writeBytes(out, buf, 4);
    } else {
        writeTag(out, type, nbytes);
        writeBytes(out, data, nbytes);
        writePadding(out, nbytes);
    }
}

/// Write array flags subelement (2 x uint32: [class|flags, 0])
void writeArrayFlags(std::vector<uint8_t>& out, uint8_t arrayClass) {
    writeTag(out, miUINT32, 8);
    uint32_t flags = arrayClass;
    writeBytes(out, &flags, 4);
    uint32_t zero = 0;
    writeBytes(out, &zero, 4);
}

/// Write dimensions subelement
void writeDimensions(std::vector<uint8_t>& out,
                     const std::vector<int32_t>& dims) {
    int32_t nbytes = static_cast<int32_t>(dims.size() * 4);
    writeTag(out, miINT32, nbytes);
    writeBytes(out, dims.data(), nbytes);
    writePadding(out, nbytes);
}

/// Write array name subelement
void writeArrayName(std::vector<uint8_t>& out, const std::string& name) {
    int32_t nbytes = static_cast<int32_t>(name.size());
    writeSmallElement(out, miINT8, name.data(), nbytes);
}

}  // anonymous namespace

namespace dicom_viewer::services {

// =========================================================================
// MAT-file header (128 bytes)
// =========================================================================

void MatlabExporter::writeHeader(std::vector<uint8_t>& out,
                                  const std::string& description) {
    // 116 bytes descriptive text (space-padded)
    std::string text = description;
    text.resize(116, ' ');
    writeBytes(out, text.data(), 116);

    // 8 bytes subsystem data offset (unused, set to 0)
    uint8_t zeros[8] = {0};
    writeBytes(out, zeros, 8);

    // 2 bytes version
    writeBytes(out, &kMATVersion, 2);

    // 2 bytes endian indicator
    writeBytes(out, &kEndianMarker, 2);
}

// =========================================================================
// Float array (miMATRIX with mxSINGLE_CLASS)
// =========================================================================

void MatlabExporter::writeFloatArray(std::vector<uint8_t>& out,
                                      const std::string& name,
                                      const std::vector<float>& data,
                                      const std::vector<int32_t>& dimensions) {
    // Build the matrix content into a temporary buffer
    std::vector<uint8_t> content;

    // 1. Array flags
    writeArrayFlags(content, mxSINGLE_CLASS);

    // 2. Dimensions
    writeDimensions(content, dimensions);

    // 3. Array name
    writeArrayName(content, name);

    // 4. Real part data (miSINGLE)
    int32_t dataBytes = static_cast<int32_t>(data.size() * sizeof(float));
    writeTag(content, miSINGLE, dataBytes);
    writeBytes(content, data.data(), dataBytes);
    writePadding(content, dataBytes);

    // Write the miMATRIX tag + content
    writeTag(out, miMATRIX, static_cast<int32_t>(content.size()));
    out.insert(out.end(), content.begin(), content.end());
}

// =========================================================================
// Struct (miMATRIX with mxSTRUCT_CLASS)
// =========================================================================

void MatlabExporter::writeStruct(std::vector<uint8_t>& out,
                                  const std::string& name,
                                  const std::map<std::string, std::string>& fields) {
    std::vector<uint8_t> content;

    // 1. Array flags
    writeArrayFlags(content, mxSTRUCT_CLASS);

    // 2. Dimensions (1x1 struct)
    std::vector<int32_t> dims = {1, 1};
    writeDimensions(content, dims);

    // 3. Struct name
    writeArrayName(content, name);

    // 4. Field name length (max field name length, padded to 8-byte multiple)
    int32_t maxNameLen = 0;
    for (const auto& [k, _] : fields) {
        maxNameLen = std::max(maxNameLen,
                              static_cast<int32_t>(k.size()));
    }
    // MATLAB requires field name length to be at least 32 for compatibility
    int32_t fieldNameLen = std::max(maxNameLen + 1, static_cast<int32_t>(32));
    writeSmallElement(content, miINT32, &fieldNameLen, 4);

    // 5. Field names (concatenated, each padded to fieldNameLen)
    {
        std::vector<char> nameData(fieldNameLen * fields.size(), '\0');
        int idx = 0;
        for (const auto& [k, _] : fields) {
            std::memcpy(&nameData[idx * fieldNameLen], k.data(),
                        std::min(static_cast<size_t>(fieldNameLen - 1), k.size()));
            ++idx;
        }
        int32_t nameBytes = static_cast<int32_t>(nameData.size());
        writeTag(content, miINT8, nameBytes);
        writeBytes(content, nameData.data(), nameBytes);
        writePadding(content, nameBytes);
    }

    // 6. Field values (each as a char array miMATRIX)
    for (const auto& [_, v] : fields) {
        std::vector<uint8_t> charArray;
        writeArrayFlags(charArray, mxCHAR_CLASS);

        std::vector<int32_t> charDims = {1, static_cast<int32_t>(v.size())};
        writeDimensions(charArray, charDims);

        // Empty name for struct field value
        writeArrayName(charArray, "");

        // Character data as miUINT8 (UTF-8 / ASCII)
        int32_t strBytes = static_cast<int32_t>(v.size());
        writeTag(charArray, miUINT8, strBytes);
        writeBytes(charArray, v.data(), strBytes);
        writePadding(charArray, strBytes);

        writeTag(content, miMATRIX, static_cast<int32_t>(charArray.size()));
        content.insert(content.end(), charArray.begin(), charArray.end());
    }

    // Write the miMATRIX tag + content
    writeTag(out, miMATRIX, static_cast<int32_t>(content.size()));
    out.insert(out.end(), content.begin(), content.end());
}

// =========================================================================
// ITK image to column-major conversion
// =========================================================================

std::vector<float> MatlabExporter::itkToColumnMajor(const FloatImage3D* image) {
    if (!image) {
        return {};
    }

    auto region = image->GetLargestPossibleRegion();
    auto size = region.GetSize();
    const int nx = static_cast<int>(size[0]);
    const int ny = static_cast<int>(size[1]);
    const int nz = static_cast<int>(size[2]);

    std::vector<float> result(nx * ny * nz);

    // MATLAB column-major: x varies fastest, then y, then z
    // ITK buffer is stored in row-major (z varies slowest, x fastest for a given row)
    itk::ImageRegionConstIterator<FloatImage3D> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        // Column-major index: x + y*nx + z*nx*ny
        size_t colIdx = idx[0] + idx[1] * nx + idx[2] * nx * ny;
        result[colIdx] = it.Get();
    }

    return result;
}

std::vector<float> MatlabExporter::extractComponentColumnMajor(
    const VectorImage3D* image, int component) {
    if (!image || component < 0 || component >= 3) {
        return {};
    }

    auto region = image->GetLargestPossibleRegion();
    auto size = region.GetSize();
    const int nx = static_cast<int>(size[0]);
    const int ny = static_cast<int>(size[1]);
    const int nz = static_cast<int>(size[2]);

    std::vector<float> result(nx * ny * nz);

    itk::ImageRegionConstIterator<VectorImage3D> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        size_t colIdx = idx[0] + idx[1] * nx + idx[2] * nx * ny;
        result[colIdx] = it.Get()[component];
    }

    return result;
}

// =========================================================================
// High-level velocity field export
// =========================================================================

std::expected<void, ExportError> MatlabExporter::exportVelocityFields(
    const std::vector<VectorImage3D::Pointer>& velocityPhases,
    const std::vector<FloatImage3D::Pointer>& magnitudePhases,
    const DicomMeta& meta,
    const ExportConfig& config) {

    if (velocityPhases.empty()) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "No velocity phases provided"});
    }

    if (!std::filesystem::exists(config.outputDir)) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            std::format("Output directory does not exist: {}",
                        config.outputDir.string())});
    }

    // Get dimensions from first phase
    auto size = velocityPhases[0]->GetLargestPossibleRegion().GetSize();
    const int nx = static_cast<int>(size[0]);
    const int ny = static_cast<int>(size[1]);
    const int nz = static_cast<int>(size[2]);
    const int nt = static_cast<int>(velocityPhases.size());

    std::vector<int32_t> dims4d = {
        static_cast<int32_t>(nx),
        static_cast<int32_t>(ny),
        static_cast<int32_t>(nz),
        static_cast<int32_t>(nt)
    };

    // Build metadata struct fields
    std::map<std::string, std::string> metaFields;
    metaFields["SeriesDescription"] = meta.seriesDescription;
    metaFields["SequenceName"] = meta.sequenceName;
    metaFields["ImageType"] = meta.imageType;
    metaFields["PixelSpacingX"] = std::format("{:.6f}", meta.pixelSpacingX);
    metaFields["PixelSpacingY"] = std::format("{:.6f}", meta.pixelSpacingY);
    metaFields["SliceThickness"] = std::format("{:.6f}", meta.sliceThickness);

    // Component names and file suffixes
    struct ComponentInfo {
        int index;
        std::string suffix;
    };
    std::vector<ComponentInfo> components = {
        {0, "vel_AP"},
        {1, "vel_FH"},
        {2, "vel_RL"}
    };

    // Export each velocity component
    for (const auto& comp : components) {
        // Concatenate all phases for this component into 4D array
        std::vector<float> data4d;
        data4d.reserve(nx * ny * nz * nt);

        for (const auto& phase : velocityPhases) {
            auto phaseData = extractComponentColumnMajor(phase.GetPointer(),
                                                         comp.index);
            data4d.insert(data4d.end(), phaseData.begin(), phaseData.end());
        }

        // Build MAT file
        std::vector<uint8_t> matFile;
        std::string desc = std::format("MATLAB 5.0 MAT-file, {}_{}",
                                        config.prefix, comp.suffix);
        writeHeader(matFile, desc);
        writeFloatArray(matFile, "data", data4d, dims4d);
        writeStruct(matFile, "metadata", metaFields);

        // Write to disk
        auto path = config.outputDir /
            std::format("{}_{}.mat", config.prefix, comp.suffix);
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(ExportError{
                ExportError::Code::FileAccessDenied,
                std::format("Cannot create file: {}", path.string())});
        }
        file.write(reinterpret_cast<const char*>(matFile.data()),
                    static_cast<std::streamsize>(matFile.size()));
    }

    // Export magnitude if requested
    if (config.exportMagnitude && !magnitudePhases.empty()) {
        std::vector<float> magData;
        magData.reserve(nx * ny * nz * nt);

        for (const auto& phase : magnitudePhases) {
            auto phaseData = itkToColumnMajor(phase.GetPointer());
            magData.insert(magData.end(), phaseData.begin(), phaseData.end());
        }

        std::vector<int32_t> magDims = dims4d;
        if (static_cast<int>(magnitudePhases.size()) != nt) {
            magDims[3] = static_cast<int32_t>(magnitudePhases.size());
        }

        std::vector<uint8_t> matFile;
        writeHeader(matFile,
            std::format("MATLAB 5.0 MAT-file, {}_M_FFE", config.prefix));
        writeFloatArray(matFile, "data", magData, magDims);
        writeStruct(matFile, "metadata", metaFields);

        auto path = config.outputDir /
            std::format("{}_M_FFE.mat", config.prefix);
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(ExportError{
                ExportError::Code::FileAccessDenied,
                std::format("Cannot create file: {}", path.string())});
        }
        file.write(reinterpret_cast<const char*>(matFile.data()),
                    static_cast<std::streamsize>(matFile.size()));
    }

    return {};
}

}  // namespace dicom_viewer::services

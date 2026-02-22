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

#include <cstring>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

namespace {

/// MAT-file v5 data type tags
constexpr int32_t miINT8    = 1;
constexpr int32_t miINT32   = 5;
constexpr int32_t miUINT32  = 6;
constexpr int32_t miSINGLE  = 7;
constexpr int32_t miMATRIX  = 14;

/// Read a little-endian int32 at offset
int32_t readInt32(const std::vector<uint8_t>& buf, size_t offset) {
    int32_t v = 0;
    std::memcpy(&v, &buf[offset], 4);
    return v;
}

/// Read a little-endian uint16 at offset
uint16_t readUint16(const std::vector<uint8_t>& buf, size_t offset) {
    uint16_t v = 0;
    std::memcpy(&v, &buf[offset], 2);
    return v;
}

/// Read a float at offset
float readFloat(const std::vector<uint8_t>& buf, size_t offset) {
    float v = 0;
    std::memcpy(&v, &buf[offset], 4);
    return v;
}

/// Create a 3D float ITK image with given value
MatlabExporter::FloatImage3D::Pointer createFloatImage(
    int sx, int sy, int sz, float value = 0.0f) {
    auto image = MatlabExporter::FloatImage3D::New();
    MatlabExporter::FloatImage3D::SizeType size = {{
        static_cast<unsigned long>(sx),
        static_cast<unsigned long>(sy),
        static_cast<unsigned long>(sz)
    }};
    MatlabExporter::FloatImage3D::IndexType start = {{0, 0, 0}};
    MatlabExporter::FloatImage3D::RegionType region{start, size};
    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(value);
    return image;
}

/// Create a 3-component vector image
MatlabExporter::VectorImage3D::Pointer createVectorImage(
    int sx, int sy, int sz) {
    auto image = MatlabExporter::VectorImage3D::New();
    MatlabExporter::VectorImage3D::SizeType size = {{
        static_cast<unsigned long>(sx),
        static_cast<unsigned long>(sy),
        static_cast<unsigned long>(sz)
    }};
    MatlabExporter::VectorImage3D::IndexType start = {{0, 0, 0}};
    MatlabExporter::VectorImage3D::RegionType region{start, size};
    image->SetRegions(region);
    image->SetNumberOfComponentsPerPixel(3);
    image->Allocate();

    // Fill with known pattern: voxel (x,y,z) → [x*10, y*10, z*10]
    itk::ImageRegionIterator<MatlabExporter::VectorImage3D> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        MatlabExporter::VectorImage3D::PixelType pixel(3);
        pixel[0] = static_cast<float>(idx[0] * 10);
        pixel[1] = static_cast<float>(idx[1] * 10);
        pixel[2] = static_cast<float>(idx[2] * 10);
        it.Set(pixel);
    }
    return image;
}

}  // anonymous namespace

// =============================================================================
// MAT-file header tests
// =============================================================================

TEST(MatlabExporterTest, HeaderIs128Bytes) {
    std::vector<uint8_t> buf;
    MatlabExporter::writeHeader(buf, "Test MAT file");
    EXPECT_EQ(buf.size(), 128u);
}

TEST(MatlabExporterTest, HeaderVersionAndEndian) {
    std::vector<uint8_t> buf;
    MatlabExporter::writeHeader(buf, "Test");

    // Version at offset 124
    uint16_t version = readUint16(buf, 124);
    EXPECT_EQ(version, 0x0100);

    // Endian marker at offset 126: 'IM' = 0x4D49 in little-endian
    uint16_t endian = readUint16(buf, 126);
    EXPECT_EQ(endian, 0x4D49);
}

TEST(MatlabExporterTest, HeaderDescriptionText) {
    std::vector<uint8_t> buf;
    MatlabExporter::writeHeader(buf, "MATLAB 5.0 MAT-file, 4DPC");

    // First 116 bytes are the description text (space-padded)
    std::string desc(reinterpret_cast<const char*>(buf.data()), 116);
    // Trim trailing spaces
    auto end = desc.find_last_not_of(' ');
    if (end != std::string::npos) desc.resize(end + 1);
    EXPECT_EQ(desc, "MATLAB 5.0 MAT-file, 4DPC");
}

// =============================================================================
// Float array tests
// =============================================================================

TEST(MatlabExporterTest, FloatArrayTag) {
    std::vector<uint8_t> buf;
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<int32_t> dims = {2, 2};

    MatlabExporter::writeFloatArray(buf, "test", data, dims);

    // First 8 bytes: miMATRIX tag
    EXPECT_EQ(readInt32(buf, 0), miMATRIX);
    int32_t totalSize = readInt32(buf, 4);
    EXPECT_GT(totalSize, 0);
    EXPECT_EQ(buf.size(), static_cast<size_t>(8 + totalSize));
}

TEST(MatlabExporterTest, FloatArrayDimensions) {
    std::vector<uint8_t> buf;
    std::vector<float> data(24, 0.0f);  // 2x3x4
    std::vector<int32_t> dims = {2, 3, 4};

    MatlabExporter::writeFloatArray(buf, "arr", data, dims);

    // After header tag (8) + array flags (16) = offset 24 should be dimensions
    // Dimensions tag: miINT32 (5), 12 bytes
    EXPECT_EQ(readInt32(buf, 8 + 16), miINT32);
    int32_t dimBytes = readInt32(buf, 8 + 16 + 4);
    EXPECT_EQ(dimBytes, 12);  // 3 dimensions * 4 bytes

    // Read dimension values
    EXPECT_EQ(readInt32(buf, 8 + 16 + 8), 2);      // nx
    EXPECT_EQ(readInt32(buf, 8 + 16 + 8 + 4), 3);  // ny
    EXPECT_EQ(readInt32(buf, 8 + 16 + 8 + 8), 4);  // nz
}

TEST(MatlabExporterTest, FloatArrayDataValues) {
    std::vector<uint8_t> buf;
    std::vector<float> data = {1.5f, 2.5f, 3.5f, 4.5f};
    std::vector<int32_t> dims = {4};

    MatlabExporter::writeFloatArray(buf, "v", data, dims);

    // Find the miSINGLE data tag (type=7)
    // Must check both tag type AND valid nbytes to avoid matching
    // mxSINGLE_CLASS (also value 7) inside array flags
    bool found = false;
    for (size_t i = 8; i + 24 <= buf.size(); i += 1) {
        if (readInt32(buf, i) == miSINGLE) {
            int32_t nbytes = readInt32(buf, i + 4);
            if (nbytes == 16) {  // 4 floats * 4 bytes
                EXPECT_FLOAT_EQ(readFloat(buf, i + 8), 1.5f);
                EXPECT_FLOAT_EQ(readFloat(buf, i + 12), 2.5f);
                EXPECT_FLOAT_EQ(readFloat(buf, i + 16), 3.5f);
                EXPECT_FLOAT_EQ(readFloat(buf, i + 20), 4.5f);
                found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found) << "miSINGLE data element not found";
}

// =============================================================================
// Struct tests
// =============================================================================

TEST(MatlabExporterTest, StructTag) {
    std::vector<uint8_t> buf;
    std::map<std::string, std::string> fields;
    fields["key1"] = "value1";

    MatlabExporter::writeStruct(buf, "meta", fields);

    EXPECT_EQ(readInt32(buf, 0), miMATRIX);
    int32_t totalSize = readInt32(buf, 4);
    EXPECT_GT(totalSize, 0);
}

TEST(MatlabExporterTest, StructMultipleFields) {
    std::vector<uint8_t> buf;
    std::map<std::string, std::string> fields;
    fields["alpha"] = "hello";
    fields["beta"] = "world";

    MatlabExporter::writeStruct(buf, "s", fields);

    // Should be a valid miMATRIX element
    EXPECT_EQ(readInt32(buf, 0), miMATRIX);
    EXPECT_GT(buf.size(), 128u);  // Struct with 2 fields needs meaningful space
}

// =============================================================================
// ITK to column-major conversion
// =============================================================================

TEST(MatlabExporterTest, ItkToColumnMajorOrdering) {
    auto image = createFloatImage(3, 4, 2, 0.0f);

    // Set specific voxels
    MatlabExporter::FloatImage3D::IndexType idx;
    idx[0] = 1; idx[1] = 2; idx[2] = 0;
    image->SetPixel(idx, 42.0f);

    idx[0] = 0; idx[1] = 0; idx[2] = 1;
    image->SetPixel(idx, 99.0f);

    auto result = MatlabExporter::itkToColumnMajor(image.GetPointer());
    EXPECT_EQ(result.size(), 24u);  // 3*4*2

    // Column-major index: x + y*nx + z*nx*ny
    // (1,2,0) → 1 + 2*3 + 0*3*4 = 7
    EXPECT_FLOAT_EQ(result[7], 42.0f);

    // (0,0,1) → 0 + 0*3 + 1*3*4 = 12
    EXPECT_FLOAT_EQ(result[12], 99.0f);
}

TEST(MatlabExporterTest, ItkToColumnMajorNullReturnsEmpty) {
    auto result = MatlabExporter::itkToColumnMajor(nullptr);
    EXPECT_TRUE(result.empty());
}

TEST(MatlabExporterTest, ExtractComponentColumnMajor) {
    auto image = createVectorImage(2, 2, 2);

    // Component 0 = x*10
    auto comp0 = MatlabExporter::extractComponentColumnMajor(
        image.GetPointer(), 0);
    EXPECT_EQ(comp0.size(), 8u);
    // Voxel (1,0,0): comp0 = 10.0, column-major index = 1
    EXPECT_FLOAT_EQ(comp0[1], 10.0f);

    // Component 1 = y*10
    auto comp1 = MatlabExporter::extractComponentColumnMajor(
        image.GetPointer(), 1);
    // Voxel (0,1,0): comp1 = 10.0, column-major index = 0 + 1*2 = 2
    EXPECT_FLOAT_EQ(comp1[2], 10.0f);

    // Component 2 = z*10
    auto comp2 = MatlabExporter::extractComponentColumnMajor(
        image.GetPointer(), 2);
    // Voxel (0,0,1): comp2 = 10.0, column-major index = 0 + 0 + 1*2*2 = 4
    EXPECT_FLOAT_EQ(comp2[4], 10.0f);
}

TEST(MatlabExporterTest, ExtractComponentInvalidReturnsEmpty) {
    auto image = createVectorImage(2, 2, 2);
    EXPECT_TRUE(MatlabExporter::extractComponentColumnMajor(nullptr, 0).empty());
    EXPECT_TRUE(MatlabExporter::extractComponentColumnMajor(
        image.GetPointer(), 3).empty());
    EXPECT_TRUE(MatlabExporter::extractComponentColumnMajor(
        image.GetPointer(), -1).empty());
}

// =============================================================================
// Full velocity export
// =============================================================================

TEST(MatlabExporterTest, ExportVelocityFieldsCreatesFiles) {
    auto tmpDir = std::filesystem::temp_directory_path() / "matlab_test";
    std::filesystem::create_directories(tmpDir);

    auto vel = createVectorImage(4, 4, 4);
    auto mag = createFloatImage(4, 4, 4, 100.0f);

    std::vector<MatlabExporter::VectorImage3D::Pointer> velPhases = {vel, vel};
    std::vector<MatlabExporter::FloatImage3D::Pointer> magPhases = {mag, mag};

    MatlabExporter::DicomMeta meta;
    meta.seriesDescription = "4D Flow";
    meta.sequenceName = "fl3d1r21";
    meta.imageType = "ORIGINAL\\PRIMARY\\P\\ND";
    meta.pixelSpacingX = 1.5;
    meta.pixelSpacingY = 1.5;
    meta.sliceThickness = 2.0;

    MatlabExporter::ExportConfig config;
    config.outputDir = tmpDir;
    config.prefix = "4DPC";
    config.exportMagnitude = true;

    auto result = MatlabExporter::exportVelocityFields(
        velPhases, magPhases, meta, config);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // Check files exist
    EXPECT_TRUE(std::filesystem::exists(tmpDir / "4DPC_vel_AP.mat"));
    EXPECT_TRUE(std::filesystem::exists(tmpDir / "4DPC_vel_FH.mat"));
    EXPECT_TRUE(std::filesystem::exists(tmpDir / "4DPC_vel_RL.mat"));
    EXPECT_TRUE(std::filesystem::exists(tmpDir / "4DPC_M_FFE.mat"));

    // Verify file size is reasonable (header 128 + data + metadata)
    auto fileSize = std::filesystem::file_size(tmpDir / "4DPC_vel_AP.mat");
    EXPECT_GT(fileSize, 128u);  // At least header
    // 4*4*4 * 2 phases * 4 bytes = 512 bytes of float data minimum
    EXPECT_GT(fileSize, 512u);

    // Cleanup
    std::filesystem::remove_all(tmpDir);
}

TEST(MatlabExporterTest, ExportVelocityFieldsValidatesHeader) {
    auto tmpDir = std::filesystem::temp_directory_path() / "matlab_hdr_test";
    std::filesystem::create_directories(tmpDir);

    auto vel = createVectorImage(2, 2, 2);
    std::vector<MatlabExporter::VectorImage3D::Pointer> velPhases = {vel};

    MatlabExporter::DicomMeta meta;
    MatlabExporter::ExportConfig config;
    config.outputDir = tmpDir;
    config.exportMagnitude = false;

    auto result = MatlabExporter::exportVelocityFields(
        velPhases, {}, meta, config);
    ASSERT_TRUE(result.has_value());

    // Read back and validate header
    auto path = tmpDir / "4DPC_vel_AP.mat";
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> buf(128);
    file.read(reinterpret_cast<char*>(buf.data()), 128);

    // Version
    EXPECT_EQ(readUint16(buf, 124), 0x0100);
    // Endian
    EXPECT_EQ(readUint16(buf, 126), 0x4D49);

    std::filesystem::remove_all(tmpDir);
}

TEST(MatlabExporterTest, ExportEmptyPhasesReturnsError) {
    std::vector<MatlabExporter::VectorImage3D::Pointer> empty;
    MatlabExporter::DicomMeta meta;
    MatlabExporter::ExportConfig config;
    config.outputDir = "/tmp/claude";

    auto result = MatlabExporter::exportVelocityFields(empty, {}, meta, config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(MatlabExporterTest, ExportNonexistentDirReturnsError) {
    auto vel = createVectorImage(2, 2, 2);
    std::vector<MatlabExporter::VectorImage3D::Pointer> velPhases = {vel};
    MatlabExporter::DicomMeta meta;
    MatlabExporter::ExportConfig config;
    config.outputDir = "/nonexistent/dir/that/should/not/exist";

    auto result = MatlabExporter::exportVelocityFields(
        velPhases, {}, meta, config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::FileAccessDenied);
}

// =============================================================================
// Complete MAT file binary structure
// =============================================================================

TEST(MatlabExporterTest, CompleteMATFileStructure) {
    std::vector<uint8_t> matFile;
    MatlabExporter::writeHeader(matFile, "Test file");

    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    std::vector<int32_t> dims = {3, 1};
    MatlabExporter::writeFloatArray(matFile, "x", data, dims);

    // Total size: 128 (header) + miMATRIX element
    EXPECT_GT(matFile.size(), 128u);

    // First data element starts at offset 128
    EXPECT_EQ(readInt32(matFile, 128), miMATRIX);

    // The miMATRIX size + 8 (tag) should reach end of buffer
    int32_t matrixSize = readInt32(matFile, 132);
    EXPECT_EQ(matFile.size(), static_cast<size_t>(128 + 8 + matrixSize));
}

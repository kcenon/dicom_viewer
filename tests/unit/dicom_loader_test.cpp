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

#include "core/dicom_loader.hpp"
#include "core/transfer_syntax_decoder.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace dicom_viewer::core::test {

// ============================================================================
// Test fixture
// ============================================================================
class DicomLoaderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        tempDir_ = std::filesystem::temp_directory_path() / "dicom_loader_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tempDir_);
    }

    /// Create a file with arbitrary content (not valid DICOM)
    std::filesystem::path createNonDicomFile(const std::string& name,
                                              const std::string& content = "NOT_DICOM")
    {
        auto path = tempDir_ / name;
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        return path;
    }

    /// Create an empty file
    std::filesystem::path createEmptyFile(const std::string& name)
    {
        auto path = tempDir_ / name;
        std::ofstream ofs(path);
        ofs.close();
        return path;
    }

    std::filesystem::path tempDir_;
};

// ============================================================================
// Construction & Lifecycle
// ============================================================================
TEST_F(DicomLoaderTest, DefaultConstructionAndDestruction)
{
    DicomLoader loader;
    // Should not throw — verifies Pimpl initialization
    (void)loader;
}

TEST_F(DicomLoaderTest, MoveConstruction)
{
    DicomLoader original;
    DicomLoader moved(std::move(original));
    // moved should be usable
    (void)moved;
}

TEST_F(DicomLoaderTest, MoveAssignment)
{
    DicomLoader original;
    DicomLoader target;
    target = std::move(original);
    (void)target;
}

TEST_F(DicomLoaderTest, DefaultMetadataIsEmpty)
{
    DicomLoader loader;
    const auto& meta = loader.getMetadata();
    EXPECT_TRUE(meta.patientName.empty());
    EXPECT_TRUE(meta.studyInstanceUid.empty());
    EXPECT_TRUE(meta.modality.empty());
    EXPECT_EQ(meta.rows, 0);
    EXPECT_EQ(meta.columns, 0);
    EXPECT_DOUBLE_EQ(meta.rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(meta.rescaleIntercept, 0.0);
}

// ============================================================================
// DicomMetadata default values
// ============================================================================
TEST_F(DicomLoaderTest, DicomMetadataDefaults)
{
    DicomMetadata meta;
    EXPECT_EQ(meta.rows, 0);
    EXPECT_EQ(meta.columns, 0);
    EXPECT_EQ(meta.bitsAllocated, 0);
    EXPECT_EQ(meta.bitsStored, 0);
    EXPECT_DOUBLE_EQ(meta.pixelSpacingX, 1.0);
    EXPECT_DOUBLE_EQ(meta.pixelSpacingY, 1.0);
    EXPECT_DOUBLE_EQ(meta.sliceThickness, 1.0);
    EXPECT_DOUBLE_EQ(meta.rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(meta.rescaleIntercept, 0.0);
}

// ============================================================================
// SliceInfo default values
// ============================================================================
TEST_F(DicomLoaderTest, SliceInfoDefaults)
{
    SliceInfo info;
    EXPECT_TRUE(info.filePath.empty());
    EXPECT_DOUBLE_EQ(info.sliceLocation, 0.0);
    EXPECT_EQ(info.instanceNumber, 0);

    // Default axial orientation
    EXPECT_DOUBLE_EQ(info.imagePosition[0], 0.0);
    EXPECT_DOUBLE_EQ(info.imagePosition[1], 0.0);
    EXPECT_DOUBLE_EQ(info.imagePosition[2], 0.0);

    EXPECT_DOUBLE_EQ(info.imageOrientation[0], 1.0);
    EXPECT_DOUBLE_EQ(info.imageOrientation[1], 0.0);
    EXPECT_DOUBLE_EQ(info.imageOrientation[2], 0.0);
    EXPECT_DOUBLE_EQ(info.imageOrientation[3], 0.0);
    EXPECT_DOUBLE_EQ(info.imageOrientation[4], 1.0);
    EXPECT_DOUBLE_EQ(info.imageOrientation[5], 0.0);
}

// ============================================================================
// DicomError enumeration
// ============================================================================
TEST_F(DicomLoaderTest, DicomErrorEnumCoversAllCodes)
{
    // Verify all error codes exist
    auto e1 = DicomError::FileNotFound;
    auto e2 = DicomError::InvalidDicomFormat;
    auto e3 = DicomError::UnsupportedTransferSyntax;
    auto e4 = DicomError::DecodingFailed;
    auto e5 = DicomError::MetadataExtractionFailed;
    auto e6 = DicomError::SeriesAssemblyFailed;
    auto e7 = DicomError::MemoryAllocationFailed;

    // All codes should be distinct
    EXPECT_NE(static_cast<int>(e1), static_cast<int>(e2));
    EXPECT_NE(static_cast<int>(e2), static_cast<int>(e3));
    EXPECT_NE(static_cast<int>(e3), static_cast<int>(e4));
    EXPECT_NE(static_cast<int>(e4), static_cast<int>(e5));
    EXPECT_NE(static_cast<int>(e5), static_cast<int>(e6));
    EXPECT_NE(static_cast<int>(e6), static_cast<int>(e7));
}

TEST_F(DicomLoaderTest, DicomErrorInfoContainsMessage)
{
    DicomErrorInfo info{DicomError::FileNotFound, "test error"};
    EXPECT_EQ(info.code, DicomError::FileNotFound);
    EXPECT_EQ(info.message, "test error");
}

// ============================================================================
// loadFile — error paths
// ============================================================================
TEST_F(DicomLoaderTest, LoadFileNonexistentPath)
{
    DicomLoader loader;
    auto result = loader.loadFile("/nonexistent/path/file.dcm");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
    EXPECT_FALSE(result.error().message.empty());
}

TEST_F(DicomLoaderTest, LoadFileEmptyPath)
{
    DicomLoader loader;
    auto result = loader.loadFile("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

TEST_F(DicomLoaderTest, LoadFileNonDicomFile)
{
    auto path = createNonDicomFile("not_dicom.txt", "This is plain text");
    DicomLoader loader;
    auto result = loader.loadFile(path);
    ASSERT_FALSE(result.has_value());
    // Non-DICOM file should fail during parsing
    EXPECT_EQ(result.error().code, DicomError::InvalidDicomFormat);
}

TEST_F(DicomLoaderTest, LoadFileEmptyFile)
{
    auto path = createEmptyFile("empty.dcm");
    DicomLoader loader;
    auto result = loader.loadFile(path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::InvalidDicomFormat);
}

TEST_F(DicomLoaderTest, LoadFileTruncatedContent)
{
    // Write some bytes that look like a DICOM preamble but are truncated
    auto path = tempDir_ / "truncated.dcm";
    std::ofstream ofs(path, std::ios::binary);
    // DICOM preamble: 128 zero bytes + "DICM" magic
    std::vector<char> preamble(128, 0);
    ofs.write(preamble.data(), preamble.size());
    ofs.write("DICM", 4);
    // Truncate here — missing actual tags
    ofs.close();

    DicomLoader loader;
    auto result = loader.loadFile(path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::InvalidDicomFormat);
}

TEST_F(DicomLoaderTest, LoadFileErrorMessageContainsPath)
{
    DicomLoader loader;
    auto result = loader.loadFile("/nonexistent/specific_test_path.dcm");
    ASSERT_FALSE(result.has_value());
    // Error message should reference the file path for debugging
    EXPECT_NE(result.error().message.find("specific_test_path"), std::string::npos);
}

TEST_F(DicomLoaderTest, LoadFileReadOnlyNonDicomFile)
{
    // Create a non-DICOM file and verify it fails gracefully even if readable
    auto path = createNonDicomFile("readonly.dcm", "NOT_A_DICOM_FILE");
    std::filesystem::permissions(path, std::filesystem::perms::owner_read);

    DicomLoader loader;
    auto result = loader.loadFile(path);
    ASSERT_FALSE(result.has_value());
    // Should fail as InvalidDicomFormat (file is readable but not valid DICOM)
    EXPECT_EQ(result.error().code, DicomError::InvalidDicomFormat);

    // Restore permissions for cleanup
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

// ============================================================================
// scanDirectory — error paths
// ============================================================================
TEST_F(DicomLoaderTest, ScanDirectoryNonexistentPath)
{
    DicomLoader loader;
    auto result = loader.scanDirectory("/nonexistent/directory");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

TEST_F(DicomLoaderTest, ScanDirectoryWithFilePath)
{
    // Pass a file path instead of directory
    auto path = createNonDicomFile("notadir.txt");
    DicomLoader loader;
    auto result = loader.scanDirectory(path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

TEST_F(DicomLoaderTest, ScanDirectoryEmptyDirectory)
{
    auto emptyDir = tempDir_ / "empty_dir";
    std::filesystem::create_directories(emptyDir);

    DicomLoader loader;
    auto result = loader.scanDirectory(emptyDir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(DicomLoaderTest, ScanDirectoryWithNonDicomFiles)
{
    auto dir = tempDir_ / "non_dicom_dir";
    std::filesystem::create_directories(dir);

    // Create non-DICOM files
    std::ofstream(dir / "readme.txt") << "not dicom";
    std::ofstream(dir / "data.csv") << "col1,col2\n1,2";
    std::ofstream(dir / "image.png") << "fake png";

    DicomLoader loader;
    auto result = loader.scanDirectory(dir);
    ASSERT_TRUE(result.has_value());
    // Non-DICOM files should be filtered out
    EXPECT_TRUE(result.value().empty());
}

TEST_F(DicomLoaderTest, ScanDirectoryWithNestedSubdirectories)
{
    auto dir = tempDir_ / "nested_dir";
    auto subdir = dir / "subdir1" / "subdir2";
    std::filesystem::create_directories(subdir);

    // Create non-DICOM files at different levels
    std::ofstream(dir / "readme.txt") << "not dicom";
    std::ofstream(subdir / "data.txt") << "not dicom either";

    DicomLoader loader;
    // scanDirectory with recursive=false should still succeed
    auto result = loader.scanDirectory(dir);
    ASSERT_TRUE(result.has_value());
    // No valid DICOM files at any level
    EXPECT_TRUE(result.value().empty());
}

// ============================================================================
// loadCTSeries — error paths
// ============================================================================
TEST_F(DicomLoaderTest, LoadCTSeriesEmptySlices)
{
    DicomLoader loader;
    std::vector<SliceInfo> empty;
    auto result = loader.loadCTSeries(empty);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

TEST_F(DicomLoaderTest, LoadCTSeriesNonexistentFiles)
{
    DicomLoader loader;
    std::vector<SliceInfo> slices;
    SliceInfo s1;
    s1.filePath = "/nonexistent/slice1.dcm";
    slices.push_back(s1);

    auto result = loader.loadCTSeries(slices);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

// ============================================================================
// loadMRSeries — error paths
// ============================================================================
TEST_F(DicomLoaderTest, LoadMRSeriesEmptySlices)
{
    DicomLoader loader;
    std::vector<SliceInfo> empty;
    auto result = loader.loadMRSeries(empty);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

TEST_F(DicomLoaderTest, LoadMRSeriesNonexistentFiles)
{
    DicomLoader loader;
    std::vector<SliceInfo> slices;
    SliceInfo s1;
    s1.filePath = "/nonexistent/slice1.dcm";
    slices.push_back(s1);

    auto result = loader.loadMRSeries(slices);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

// ============================================================================
// isEnhancedDicom — static method
// ============================================================================
TEST_F(DicomLoaderTest, IsEnhancedDicomNonexistentFile)
{
    EXPECT_FALSE(DicomLoader::isEnhancedDicom("/nonexistent/file.dcm"));
}

TEST_F(DicomLoaderTest, IsEnhancedDicomNonDicomFile)
{
    auto path = createNonDicomFile("plain.txt", "not dicom at all");
    EXPECT_FALSE(DicomLoader::isEnhancedDicom(path));
}

TEST_F(DicomLoaderTest, IsEnhancedDicomEmptyFile)
{
    auto path = createEmptyFile("empty.dcm");
    EXPECT_FALSE(DicomLoader::isEnhancedDicom(path));
}

TEST_F(DicomLoaderTest, IsEnhancedDicomTruncatedFile)
{
    auto path = tempDir_ / "truncated.dcm";
    std::ofstream ofs(path, std::ios::binary);
    std::vector<char> preamble(128, 0);
    ofs.write(preamble.data(), preamble.size());
    ofs.write("DICM", 4);
    ofs.close();

    EXPECT_FALSE(DicomLoader::isEnhancedDicom(path));
}

// ============================================================================
// isTransferSyntaxSupported — static method
// ============================================================================
TEST_F(DicomLoaderTest, ImplicitVRLittleEndianSupported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::ImplicitVRLittleEndian)));
}

TEST_F(DicomLoaderTest, ExplicitVRLittleEndianSupported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::ExplicitVRLittleEndian)));
}

TEST_F(DicomLoaderTest, JPEGBaselineSupported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::JPEGBaseline)));
}

TEST_F(DicomLoaderTest, JPEG2000Supported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::JPEG2000)));
}

TEST_F(DicomLoaderTest, JPEG2000LosslessSupported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::JPEG2000Lossless)));
}

TEST_F(DicomLoaderTest, JPEGLosslessSupported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::JPEGLossless)));
}

TEST_F(DicomLoaderTest, JPEGLSLosslessSupported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::JPEGLSLossless)));
}

TEST_F(DicomLoaderTest, RLELosslessSupported)
{
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(
        std::string(transfer_syntax::RLELossless)));
}

TEST_F(DicomLoaderTest, UnknownTransferSyntaxNotSupported)
{
    EXPECT_FALSE(DicomLoader::isTransferSyntaxSupported("1.2.3.4.5.6.7.8.9"));
}

TEST_F(DicomLoaderTest, EmptyTransferSyntaxNotSupported)
{
    EXPECT_FALSE(DicomLoader::isTransferSyntaxSupported(""));
}

TEST_F(DicomLoaderTest, InvalidTransferSyntaxNotSupported)
{
    EXPECT_FALSE(DicomLoader::isTransferSyntaxSupported("invalid_uid"));
}

// ============================================================================
// getSupportedTransferSyntaxes — static method
// ============================================================================
TEST_F(DicomLoaderTest, GetSupportedTransferSyntaxesReturnsNonEmpty)
{
    auto syntaxes = DicomLoader::getSupportedTransferSyntaxes();
    EXPECT_FALSE(syntaxes.empty());
    EXPECT_GE(syntaxes.size(), 8u);  // At least 8 standard syntaxes
}

TEST_F(DicomLoaderTest, GetSupportedTransferSyntaxesContainsCommonUIDs)
{
    auto syntaxes = DicomLoader::getSupportedTransferSyntaxes();

    auto contains = [&syntaxes](std::string_view uid) {
        return std::find(syntaxes.begin(), syntaxes.end(), uid) != syntaxes.end();
    };

    EXPECT_TRUE(contains(transfer_syntax::ImplicitVRLittleEndian));
    EXPECT_TRUE(contains(transfer_syntax::ExplicitVRLittleEndian));
    EXPECT_TRUE(contains(transfer_syntax::JPEGBaseline));
    EXPECT_TRUE(contains(transfer_syntax::JPEG2000Lossless));
}

TEST_F(DicomLoaderTest, GetSupportedTransferSyntaxesConsistentWithIsSupported)
{
    auto syntaxes = DicomLoader::getSupportedTransferSyntaxes();
    for (const auto& uid : syntaxes) {
        EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported(uid))
            << "getSupportedTransferSyntaxes() lists " << uid
            << " but isTransferSyntaxSupported() returns false";
    }
}

// ============================================================================
// Image type aliases
// ============================================================================
TEST_F(DicomLoaderTest, CTImageTypeIs3DShort)
{
    static_assert(CTImageType::ImageDimension == 3);
    static_assert(std::is_same_v<CTImageType::PixelType, short>);
}

TEST_F(DicomLoaderTest, MRImageTypeIs3DUnsignedShort)
{
    static_assert(MRImageType::ImageDimension == 3);
    static_assert(std::is_same_v<MRImageType::PixelType, unsigned short>);
}

} // namespace dicom_viewer::core::test

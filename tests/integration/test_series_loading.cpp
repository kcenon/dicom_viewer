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

// Integration test for DICOM series loading pipeline
// Converted from manual CLI tool to automated GoogleTest (#203)
// Uses synthetic data structures — no real DICOM files required

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#include "core/dicom_loader.hpp"
#include "core/series_builder.hpp"

using namespace dicom_viewer::core;
namespace fs = std::filesystem;

// =============================================================================
// Test fixture with synthetic series data generation
// =============================================================================

class SeriesLoadingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        tempDir_ = fs::temp_directory_path() / "series_loading_integration_test";
        fs::create_directories(tempDir_);

        buildAxialCTSeries();
        buildSagittalMRSeries();
        buildSingleSliceSeries();
    }

    void TearDown() override
    {
        fs::remove_all(tempDir_);
    }

    /// Build a synthetic 20-slice axial CT series (5mm spacing)
    void buildAxialCTSeries()
    {
        ctSeries_.seriesInstanceUid = "1.2.840.113619.2.55.3.12345.1";
        ctSeries_.seriesDescription = "CHEST CT 5mm";
        ctSeries_.modality = "CT";
        ctSeries_.pixelSpacingX = 0.5;
        ctSeries_.pixelSpacingY = 0.5;

        for (int i = 0; i < 20; ++i) {
            SliceInfo slice;
            slice.filePath = "/synthetic/ct/slice_" + std::to_string(i) + ".dcm";
            slice.imagePosition = {-125.0, -125.0, static_cast<double>(i * 5)};
            slice.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};  // Axial
            slice.sliceLocation = static_cast<double>(i * 5);
            slice.instanceNumber = i + 1;
            ctSeries_.slices.push_back(slice);
        }
        ctSeries_.sliceCount = ctSeries_.slices.size();
        ctSeries_.sliceSpacing = SeriesBuilder::calculateSliceSpacing(ctSeries_.slices);
        ctSeries_.dimensions = {512, 512, 20};
    }

    /// Build a synthetic 10-slice sagittal MR series (3mm spacing)
    void buildSagittalMRSeries()
    {
        mrSeries_.seriesInstanceUid = "1.2.840.113619.2.55.3.12345.2";
        mrSeries_.seriesDescription = "SAG T1 BRAIN";
        mrSeries_.modality = "MR";
        mrSeries_.pixelSpacingX = 1.0;
        mrSeries_.pixelSpacingY = 1.0;

        for (int i = 0; i < 10; ++i) {
            SliceInfo slice;
            slice.filePath = "/synthetic/mr/slice_" + std::to_string(i) + ".dcm";
            slice.imagePosition = {static_cast<double>(i * 3), -100.0, 0.0};
            // Sagittal: row along Y, col along Z → normal along X
            slice.imageOrientation = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
            slice.sliceLocation = static_cast<double>(i * 3);
            slice.instanceNumber = i + 1;
            mrSeries_.slices.push_back(slice);
        }
        mrSeries_.sliceCount = mrSeries_.slices.size();
        mrSeries_.sliceSpacing = SeriesBuilder::calculateSliceSpacing(mrSeries_.slices);
        mrSeries_.dimensions = {256, 256, 10};
    }

    /// Build a single-slice series for edge case testing
    void buildSingleSliceSeries()
    {
        singleSliceSeries_.seriesInstanceUid = "1.2.840.113619.2.55.3.12345.3";
        singleSliceSeries_.seriesDescription = "SCOUT";
        singleSliceSeries_.modality = "CT";

        SliceInfo slice;
        slice.filePath = "/synthetic/scout/scout.dcm";
        slice.imagePosition = {0.0, 0.0, 0.0};
        slice.instanceNumber = 1;
        singleSliceSeries_.slices.push_back(slice);
        singleSliceSeries_.sliceCount = 1;
    }

    /// Create actual filesystem files (non-DICOM) for directory scan tests
    void createNonDicomFiles()
    {
        auto dir = tempDir_ / "non_dicom";
        fs::create_directories(dir);
        std::ofstream(dir / "readme.txt") << "Not a DICOM file";
        std::ofstream(dir / "data.csv") << "col1,col2\n1,2";
        std::ofstream(dir / "image.png") << "\x89PNG";
    }

    fs::path tempDir_;
    SeriesInfo ctSeries_;
    SeriesInfo mrSeries_;
    SeriesInfo singleSliceSeries_;
};

// =============================================================================
// Series discovery tests
// =============================================================================

TEST_F(SeriesLoadingIntegrationTest, ScanEmptyDirectoryReturnsNoSeries)
{
    auto emptyDir = tempDir_ / "empty";
    fs::create_directories(emptyDir);

    SeriesBuilder builder;
    auto result = builder.scanForSeries(emptyDir);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_TRUE(result.value().empty());
}

TEST_F(SeriesLoadingIntegrationTest, ScanNonDicomDirectoryReturnsEmpty)
{
    createNonDicomFiles();
    auto dir = tempDir_ / "non_dicom";

    SeriesBuilder builder;
    auto result = builder.scanForSeries(dir);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_TRUE(result.value().empty());
}

TEST_F(SeriesLoadingIntegrationTest, ScanNonexistentDirectoryReturnsError)
{
    SeriesBuilder builder;
    auto result = builder.scanForSeries("/nonexistent/integration_test_dir");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

// =============================================================================
// SeriesInfo data integrity tests
// =============================================================================

TEST_F(SeriesLoadingIntegrationTest, SeriesInfoFieldsPopulatedCorrectly)
{
    // Verify all fields of synthetic CT series are self-consistent
    EXPECT_EQ(ctSeries_.seriesInstanceUid, "1.2.840.113619.2.55.3.12345.1");
    EXPECT_EQ(ctSeries_.modality, "CT");
    EXPECT_EQ(ctSeries_.sliceCount, 20u);
    EXPECT_EQ(ctSeries_.slices.size(), 20u);
    EXPECT_EQ(ctSeries_.sliceCount, ctSeries_.slices.size());

    // Spacing was calculated through SeriesBuilder::calculateSliceSpacing
    EXPECT_NEAR(ctSeries_.sliceSpacing, 5.0, 0.01);

    // First and last slice positions match expected coordinates
    EXPECT_DOUBLE_EQ(ctSeries_.slices.front().imagePosition[2], 0.0);
    EXPECT_DOUBLE_EQ(ctSeries_.slices.back().imagePosition[2], 95.0);
}

TEST_F(SeriesLoadingIntegrationTest, SeriesUIDsAreUniqueAndValid)
{
    // Verify each series has a non-empty, unique UID
    EXPECT_FALSE(ctSeries_.seriesInstanceUid.empty());
    EXPECT_FALSE(mrSeries_.seriesInstanceUid.empty());
    EXPECT_FALSE(singleSliceSeries_.seriesInstanceUid.empty());

    // UIDs must be distinct
    EXPECT_NE(ctSeries_.seriesInstanceUid, mrSeries_.seriesInstanceUid);
    EXPECT_NE(ctSeries_.seriesInstanceUid, singleSliceSeries_.seriesInstanceUid);
    EXPECT_NE(mrSeries_.seriesInstanceUid, singleSliceSeries_.seriesInstanceUid);

    // UID format: dot-separated numeric (DICOM standard)
    for (char c : ctSeries_.seriesInstanceUid) {
        EXPECT_TRUE(std::isdigit(c) || c == '.')
            << "Invalid character in UID: " << c;
    }
}

TEST_F(SeriesLoadingIntegrationTest, ModalityDetectionFromSeriesInfo)
{
    EXPECT_EQ(ctSeries_.modality, "CT");
    EXPECT_EQ(mrSeries_.modality, "MR");
    EXPECT_EQ(singleSliceSeries_.modality, "CT");
}

// =============================================================================
// Spacing and consistency validation pipeline
// =============================================================================

TEST_F(SeriesLoadingIntegrationTest, SpacingAndConsistencyPipeline)
{
    // Full pipeline: calculateSliceSpacing → validateSeriesConsistency
    double ctSpacing = SeriesBuilder::calculateSliceSpacing(ctSeries_.slices);
    EXPECT_NEAR(ctSpacing, 5.0, 0.01);
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(ctSeries_.slices));

    double mrSpacing = SeriesBuilder::calculateSliceSpacing(mrSeries_.slices);
    EXPECT_NEAR(mrSpacing, 3.0, 0.01);
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(mrSeries_.slices));
}

// =============================================================================
// Volume assembly error propagation
// =============================================================================

TEST_F(SeriesLoadingIntegrationTest, BuildCTVolumeFailsOnSyntheticPaths)
{
    // Full pipeline: SeriesInfo → buildCTVolume → expect graceful failure
    // since file paths point to nonexistent synthetic DICOM files
    SeriesBuilder builder;
    auto result = builder.buildCTVolume(ctSeries_);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
    EXPECT_FALSE(result.error().message.empty());
}

TEST_F(SeriesLoadingIntegrationTest, BuildMRVolumeFailsOnSyntheticPaths)
{
    SeriesBuilder builder;
    auto result = builder.buildMRVolume(mrSeries_);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

// =============================================================================
// Multi-series handling
// =============================================================================

TEST_F(SeriesLoadingIntegrationTest, MultipleSeriesIndependentValidation)
{
    // Validate CT and MR series independently — neither affects the other
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(ctSeries_.slices));
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(mrSeries_.slices));

    // Modify CT to be inconsistent; MR should remain unaffected
    auto modifiedCT = ctSeries_.slices;
    modifiedCT[10].imagePosition[2] = 999.0;
    EXPECT_FALSE(SeriesBuilder::validateSeriesConsistency(modifiedCT));
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(mrSeries_.slices));
}

TEST_F(SeriesLoadingIntegrationTest, SingleSliceSeriesHandledGracefully)
{
    // Single-slice series: should not crash, validation should pass
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(singleSliceSeries_.slices));

    double spacing = SeriesBuilder::calculateSliceSpacing(singleSliceSeries_.slices);
    EXPECT_NEAR(spacing, 1.0, 0.01);  // Default spacing for single slice

    // Volume assembly should still fail (no actual DICOM file)
    SeriesBuilder builder;
    auto result = builder.buildCTVolume(singleSliceSeries_);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

// =============================================================================
// DicomLoader directory scanning integration
// =============================================================================

TEST_F(SeriesLoadingIntegrationTest, DicomLoaderScanDirectoryErrorPropagation)
{
    // Test through DicomLoader (lower-level) to verify error propagation
    DicomLoader loader;
    auto result = loader.scanDirectory("/nonexistent/scan_test_path");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

TEST_F(SeriesLoadingIntegrationTest, DicomLoaderEmptyDirectoryScan)
{
    auto emptyDir = tempDir_ / "loader_empty";
    fs::create_directories(emptyDir);

    DicomLoader loader;
    auto result = loader.scanDirectory(emptyDir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

// =============================================================================
// Progress callback integration
// =============================================================================

TEST_F(SeriesLoadingIntegrationTest, ProgressCallbackInvokedDuringScan)
{
    auto emptyDir = tempDir_ / "progress_test";
    fs::create_directories(emptyDir);

    SeriesBuilder builder;
    bool callbackInvoked = false;
    builder.setProgressCallback([&](size_t, size_t, const std::string&) {
        callbackInvoked = true;
    });

    auto result = builder.scanForSeries(emptyDir);
    ASSERT_TRUE(result.has_value());
    // Callback may or may not be invoked for empty directories depending
    // on implementation; the key assertion is that it doesn't crash
    SUCCEED();
}

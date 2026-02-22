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

#include "core/series_builder.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace dicom_viewer::core::test {

class SeriesAssemblyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create test slice data
        createTestSlices();
    }

    void createTestSlices()
    {
        // Create slices with known positions for testing sorting
        for (int i = 0; i < 10; ++i) {
            SliceInfo slice;
            slice.filePath = "/test/slice_" + std::to_string(i) + ".dcm";
            slice.imagePosition = {0.0, 0.0, static_cast<double>(i * 5)};  // 5mm spacing
            slice.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};  // Axial
            slice.sliceLocation = static_cast<double>(i * 5);
            slice.instanceNumber = i + 1;
            orderedSlices_.push_back(slice);
        }

        // Create shuffled version for sorting tests
        shuffledSlices_ = orderedSlices_;
        // Manual shuffle: [0,9,1,8,2,7,3,6,4,5]
        std::swap(shuffledSlices_[1], shuffledSlices_[9]);
        std::swap(shuffledSlices_[3], shuffledSlices_[7]);
    }

    std::vector<SliceInfo> orderedSlices_;
    std::vector<SliceInfo> shuffledSlices_;
};

TEST_F(SeriesAssemblyTest, SliceSpacingCalculation)
{
    double spacing = SeriesBuilder::calculateSliceSpacing(orderedSlices_);
    EXPECT_NEAR(spacing, 5.0, 0.01);
}

TEST_F(SeriesAssemblyTest, SliceSpacingWithSingleSlice)
{
    std::vector<SliceInfo> singleSlice = {orderedSlices_[0]};
    double spacing = SeriesBuilder::calculateSliceSpacing(singleSlice);
    EXPECT_NEAR(spacing, 1.0, 0.01);  // Default spacing
}

TEST_F(SeriesAssemblyTest, SliceSpacingWithEmptyVector)
{
    std::vector<SliceInfo> empty;
    double spacing = SeriesBuilder::calculateSliceSpacing(empty);
    EXPECT_NEAR(spacing, 1.0, 0.01);  // Default spacing
}

TEST_F(SeriesAssemblyTest, ValidateConsistentSeries)
{
    bool isValid = SeriesBuilder::validateSeriesConsistency(orderedSlices_);
    EXPECT_TRUE(isValid);
}

TEST_F(SeriesAssemblyTest, ValidateInconsistentSpacing)
{
    // Modify one slice to have different spacing
    std::vector<SliceInfo> inconsistent = orderedSlices_;
    inconsistent[5].imagePosition[2] = 30.0;  // Should be 25.0

    bool isValid = SeriesBuilder::validateSeriesConsistency(inconsistent);
    EXPECT_FALSE(isValid);
}

TEST_F(SeriesAssemblyTest, ValidateInconsistentOrientation)
{
    std::vector<SliceInfo> inconsistent = orderedSlices_;
    inconsistent[5].imageOrientation = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};  // Different orientation

    bool isValid = SeriesBuilder::validateSeriesConsistency(inconsistent);
    EXPECT_FALSE(isValid);
}

TEST_F(SeriesAssemblyTest, ValidateSingleSliceSeries)
{
    std::vector<SliceInfo> single = {orderedSlices_[0]};
    bool isValid = SeriesBuilder::validateSeriesConsistency(single);
    EXPECT_TRUE(isValid);  // Single slice is always valid
}

TEST_F(SeriesAssemblyTest, SeriesBuilderCreation)
{
    SeriesBuilder builder;
    // Just verify it can be created without throwing
    SUCCEED();
}

TEST_F(SeriesAssemblyTest, ProgressCallbackSetup)
{
    SeriesBuilder builder;

    bool callbackInvoked = false;
    builder.setProgressCallback([&callbackInvoked](size_t, size_t, const std::string&) {
        callbackInvoked = true;
    });

    // The callback won't be invoked until actual operations
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(SeriesAssemblyTest, ScanNonExistentDirectory)
{
    SeriesBuilder builder;
    auto result = builder.scanForSeries("/nonexistent/path");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

TEST_F(SeriesAssemblyTest, BuildVolumeWithEmptySeries)
{
    SeriesBuilder builder;
    SeriesInfo emptySeries;

    auto result = builder.buildCTVolume(emptySeries);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

// Test for DicomLoader directly
class DicomLoaderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        loader_ = std::make_unique<DicomLoader>();
    }

    std::unique_ptr<DicomLoader> loader_;
};

TEST_F(DicomLoaderTest, LoadNonExistentFile)
{
    auto result = loader_->loadFile("/nonexistent/file.dcm");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

TEST_F(DicomLoaderTest, ScanNonExistentDirectory)
{
    auto result = loader_->scanDirectory("/nonexistent/directory");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::FileNotFound);
}

TEST_F(DicomLoaderTest, TransferSyntaxSupport)
{
    // Implicit VR Little Endian - always supported
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported("1.2.840.10008.1.2"));

    // Explicit VR Little Endian - always supported
    EXPECT_TRUE(DicomLoader::isTransferSyntaxSupported("1.2.840.10008.1.2.1"));

    // Unknown syntax
    EXPECT_FALSE(DicomLoader::isTransferSyntaxSupported("1.2.3.4.5.6.7.8.9"));
}

TEST_F(DicomLoaderTest, GetSupportedTransferSyntaxes)
{
    auto syntaxes = DicomLoader::getSupportedTransferSyntaxes();

    EXPECT_FALSE(syntaxes.empty());
    EXPECT_GE(syntaxes.size(), 2);  // At least ImplicitVR and ExplicitVR
}

TEST_F(DicomLoaderTest, LoadEmptySliceVector)
{
    std::vector<SliceInfo> empty;
    auto result = loader_->loadCTSeries(empty);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, DicomError::SeriesAssemblyFailed);
}

} // namespace dicom_viewer::core::test

#include <gtest/gtest.h>

#include "services/enhanced_dicom/enhanced_dicom_types.hpp"
#include "services/enhanced_dicom/frame_extractor.hpp"

#include <cstring>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

#include <gdcmDataElement.h>
#include <gdcmDataSet.h>
#include <gdcmFile.h>
#include <gdcmFileMetaInformation.h>
#include <gdcmItem.h>
#include <gdcmMediaStorage.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmTag.h>
#include <gdcmTransferSyntax.h>
#include <gdcmUIDGenerator.h>
#include <gdcmWriter.h>

using namespace dicom_viewer::services;

// =============================================================================
// DICOM tag constants for pixel data construction
// =============================================================================

namespace tags {
const gdcm::Tag SamplesPerPixel{0x0028, 0x0002};
const gdcm::Tag NumberOfFrames{0x0028, 0x0008};
const gdcm::Tag Rows{0x0028, 0x0010};
const gdcm::Tag Columns{0x0028, 0x0011};
const gdcm::Tag BitsAllocated{0x0028, 0x0100};
const gdcm::Tag BitsStored{0x0028, 0x0101};
const gdcm::Tag HighBit{0x0028, 0x0102};
const gdcm::Tag PixelRepresentation{0x0028, 0x0103};
const gdcm::Tag PixelData{0x7FE0, 0x0010};
const gdcm::Tag PhotometricInterpretation{0x0028, 0x0004};
const gdcm::Tag SOPClassUID{0x0008, 0x0016};
const gdcm::Tag SOPInstanceUID{0x0008, 0x0018};
const gdcm::Tag MediaStorageSOPClassUID{0x0002, 0x0002};
const gdcm::Tag MediaStorageSOPInstanceUID{0x0002, 0x0003};
const gdcm::Tag TransferSyntaxUID{0x0002, 0x0010};
const gdcm::Tag SharedFunctionalGroups{0x5200, 0x9229};
const gdcm::Tag PerFrameFunctionalGroups{0x5200, 0x9230};
const gdcm::Tag PlanePositionSequence{0x0020, 0x9113};
const gdcm::Tag PlaneOrientationSequence{0x0020, 0x9116};
const gdcm::Tag PixelMeasuresSequence{0x0028, 0x9110};
const gdcm::Tag PixelValueTransformationSequence{0x0028, 0x9145};
const gdcm::Tag ImagePositionPatient{0x0020, 0x0032};
const gdcm::Tag ImageOrientationPatient{0x0020, 0x0037};
const gdcm::Tag PixelSpacing{0x0028, 0x0030};
const gdcm::Tag SliceThickness{0x0018, 0x0050};
const gdcm::Tag RescaleIntercept{0x0028, 0x1052};
const gdcm::Tag RescaleSlope{0x0028, 0x1053};
}  // namespace tags

// =============================================================================
// Helpers for building synthetic DICOM files with pixel data
// =============================================================================

namespace {

void insertStringElement(gdcm::DataSet& ds, const gdcm::Tag& tag,
                         const std::string& value) {
    gdcm::DataElement de(tag);
    de.SetByteValue(value.c_str(), static_cast<uint32_t>(value.size()));
    ds.Insert(de);
}

void insertSequenceWithItem(gdcm::DataSet& parentDs, const gdcm::Tag& seqTag,
                            const gdcm::DataSet& itemDs) {
    auto sq = gdcm::SequenceOfItems::New();
    sq->SetLengthToUndefined();
    gdcm::Item item;
    item.SetNestedDataSet(itemDs);
    sq->AddItem(item);

    gdcm::DataElement de(seqTag);
    de.SetValue(*sq);
    de.SetVLToUndefined();
    parentDs.Insert(de);
}

void insertSequenceWithItems(gdcm::DataSet& parentDs, const gdcm::Tag& seqTag,
                             const std::vector<gdcm::DataSet>& items) {
    auto sq = gdcm::SequenceOfItems::New();
    sq->SetLengthToUndefined();
    for (const auto& itemDs : items) {
        gdcm::Item item;
        item.SetNestedDataSet(itemDs);
        sq->AddItem(item);
    }

    gdcm::DataElement de(seqTag);
    de.SetValue(*sq);
    de.SetVLToUndefined();
    parentDs.Insert(de);
}

/// Build a minimal EnhancedSeriesInfo for testing
EnhancedSeriesInfo makeSeriesInfo(int rows, int cols, int numFrames,
                                  int bitsAllocated = 16,
                                  int pixelRep = 1) {
    EnhancedSeriesInfo info;
    info.rows = rows;
    info.columns = cols;
    info.numberOfFrames = numFrames;
    info.bitsAllocated = bitsAllocated;
    info.bitsStored = bitsAllocated;
    info.highBit = bitsAllocated - 1;
    info.pixelRepresentation = pixelRep;
    info.pixelSpacingX = 1.0;
    info.pixelSpacingY = 1.0;

    info.frames.resize(numFrames);
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].frameIndex = i;
        info.frames[i].rescaleSlope = 1.0;
        info.frames[i].rescaleIntercept = 0.0;
        info.frames[i].sliceThickness = 1.0;
        info.frames[i].imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        info.frames[i].imagePosition = {0.0, 0.0, static_cast<double>(i)};
    }

    return info;
}

}  // anonymous namespace

// =============================================================================
// Test fixture: manages temporary DICOM file lifecycle
// =============================================================================

class FrameExtractorTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / "fe_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    /// Write a synthetic Enhanced DICOM file with 16-bit signed pixel data.
    /// Each pixel in frame i is set to (baseValue + i * frameIncrement).
    std::string writeSyntheticDicom16s(
        const std::string& filename,
        int rows, int cols, int numFrames,
        short baseValue = 100, short frameIncrement = 10)
    {
        auto path = (tempDir_ / filename).string();

        gdcm::Writer writer;
        writer.SetFileName(path.c_str());
        auto& file = writer.GetFile();
        auto& ds = file.GetDataSet();

        // Required pixel format attributes
        insertStringElement(ds, tags::SamplesPerPixel, "1");
        insertStringElement(ds, tags::NumberOfFrames,
                            std::to_string(numFrames));
        insertStringElement(ds, tags::Rows, std::to_string(rows));
        insertStringElement(ds, tags::Columns, std::to_string(cols));
        insertStringElement(ds, tags::BitsAllocated, "16");
        insertStringElement(ds, tags::BitsStored, "16");
        insertStringElement(ds, tags::HighBit, "15");
        insertStringElement(ds, tags::PixelRepresentation, "1");  // signed
        insertStringElement(ds, tags::PhotometricInterpretation, "MONOCHROME2");

        // SOP Class / Instance UIDs
        std::string sopClass = "1.2.840.10008.5.1.4.1.1.2.1";
        insertStringElement(ds, tags::SOPClassUID, sopClass);
        gdcm::UIDGenerator uidGen;
        insertStringElement(ds, tags::SOPInstanceUID, uidGen.Generate());

        // Generate pixel data: each frame has uniform pixel values
        size_t pixelsPerFrame = static_cast<size_t>(rows) * cols;
        size_t totalPixels = pixelsPerFrame * numFrames;
        std::vector<short> pixelBuffer(totalPixels);

        for (int f = 0; f < numFrames; ++f) {
            short val = static_cast<short>(baseValue + f * frameIncrement);
            for (size_t p = 0; p < pixelsPerFrame; ++p) {
                pixelBuffer[f * pixelsPerFrame + p] = val;
            }
        }

        gdcm::DataElement pixelData(tags::PixelData);
        pixelData.SetByteValue(
            reinterpret_cast<const char*>(pixelBuffer.data()),
            static_cast<uint32_t>(totalPixels * sizeof(short)));
        ds.Insert(pixelData);

        // File meta information
        auto& header = file.GetHeader();
        gdcm::DataElement msSop(tags::MediaStorageSOPClassUID);
        msSop.SetByteValue(sopClass.c_str(),
                           static_cast<uint32_t>(sopClass.size()));
        header.Insert(msSop);

        gdcm::DataElement msInstance(tags::MediaStorageSOPInstanceUID);
        std::string instUid = uidGen.Generate();
        msInstance.SetByteValue(instUid.c_str(),
                                static_cast<uint32_t>(instUid.size()));
        header.Insert(msInstance);

        std::string tsUid = "1.2.840.10008.1.2.1";  // Explicit VR Little Endian
        gdcm::DataElement tsElem(tags::TransferSyntaxUID);
        tsElem.SetByteValue(tsUid.c_str(),
                            static_cast<uint32_t>(tsUid.size()));
        header.Insert(tsElem);

        writer.Write();
        return path;
    }

    /// Write a synthetic Enhanced DICOM file with 16-bit unsigned pixel data.
    std::string writeSyntheticDicom16u(
        const std::string& filename,
        int rows, int cols, int numFrames,
        uint16_t baseValue = 200, uint16_t frameIncrement = 50)
    {
        auto path = (tempDir_ / filename).string();

        gdcm::Writer writer;
        writer.SetFileName(path.c_str());
        auto& file = writer.GetFile();
        auto& ds = file.GetDataSet();

        insertStringElement(ds, tags::SamplesPerPixel, "1");
        insertStringElement(ds, tags::NumberOfFrames,
                            std::to_string(numFrames));
        insertStringElement(ds, tags::Rows, std::to_string(rows));
        insertStringElement(ds, tags::Columns, std::to_string(cols));
        insertStringElement(ds, tags::BitsAllocated, "16");
        insertStringElement(ds, tags::BitsStored, "16");
        insertStringElement(ds, tags::HighBit, "15");
        insertStringElement(ds, tags::PixelRepresentation, "0");  // unsigned
        insertStringElement(ds, tags::PhotometricInterpretation, "MONOCHROME2");

        std::string sopClass = "1.2.840.10008.5.1.4.1.1.2.1";
        insertStringElement(ds, tags::SOPClassUID, sopClass);
        gdcm::UIDGenerator uidGen;
        insertStringElement(ds, tags::SOPInstanceUID, uidGen.Generate());

        size_t pixelsPerFrame = static_cast<size_t>(rows) * cols;
        size_t totalPixels = pixelsPerFrame * numFrames;
        std::vector<uint16_t> pixelBuffer(totalPixels);

        for (int f = 0; f < numFrames; ++f) {
            auto val = static_cast<uint16_t>(baseValue + f * frameIncrement);
            for (size_t p = 0; p < pixelsPerFrame; ++p) {
                pixelBuffer[f * pixelsPerFrame + p] = val;
            }
        }

        gdcm::DataElement pixelData(tags::PixelData);
        pixelData.SetByteValue(
            reinterpret_cast<const char*>(pixelBuffer.data()),
            static_cast<uint32_t>(totalPixels * sizeof(uint16_t)));
        ds.Insert(pixelData);

        auto& header = file.GetHeader();
        gdcm::DataElement msSop(tags::MediaStorageSOPClassUID);
        msSop.SetByteValue(sopClass.c_str(),
                           static_cast<uint32_t>(sopClass.size()));
        header.Insert(msSop);

        gdcm::DataElement msInstance(tags::MediaStorageSOPInstanceUID);
        std::string instUid = uidGen.Generate();
        msInstance.SetByteValue(instUid.c_str(),
                                static_cast<uint32_t>(instUid.size()));
        header.Insert(msInstance);

        std::string tsUid = "1.2.840.10008.1.2.1";
        gdcm::DataElement tsElem(tags::TransferSyntaxUID);
        tsElem.SetByteValue(tsUid.c_str(),
                            static_cast<uint32_t>(tsUid.size()));
        header.Insert(tsElem);

        writer.Write();
        return path;
    }

    /// Write a synthetic Enhanced DICOM file with 8-bit pixel data.
    std::string writeSyntheticDicom8(
        const std::string& filename,
        int rows, int cols, int numFrames,
        bool isSigned,
        int baseValue = 50, int frameIncrement = 10)
    {
        auto path = (tempDir_ / filename).string();

        gdcm::Writer writer;
        writer.SetFileName(path.c_str());
        auto& file = writer.GetFile();
        auto& ds = file.GetDataSet();

        insertStringElement(ds, tags::SamplesPerPixel, "1");
        insertStringElement(ds, tags::NumberOfFrames,
                            std::to_string(numFrames));
        insertStringElement(ds, tags::Rows, std::to_string(rows));
        insertStringElement(ds, tags::Columns, std::to_string(cols));
        insertStringElement(ds, tags::BitsAllocated, "8");
        insertStringElement(ds, tags::BitsStored, "8");
        insertStringElement(ds, tags::HighBit, "7");
        insertStringElement(ds, tags::PixelRepresentation,
                            isSigned ? "1" : "0");
        insertStringElement(ds, tags::PhotometricInterpretation, "MONOCHROME2");

        std::string sopClass = "1.2.840.10008.5.1.4.1.1.2.1";
        insertStringElement(ds, tags::SOPClassUID, sopClass);
        gdcm::UIDGenerator uidGen;
        insertStringElement(ds, tags::SOPInstanceUID, uidGen.Generate());

        size_t pixelsPerFrame = static_cast<size_t>(rows) * cols;
        size_t totalPixels = pixelsPerFrame * numFrames;
        std::vector<uint8_t> pixelBuffer(totalPixels);

        for (int f = 0; f < numFrames; ++f) {
            auto val = static_cast<uint8_t>(baseValue + f * frameIncrement);
            for (size_t p = 0; p < pixelsPerFrame; ++p) {
                pixelBuffer[f * pixelsPerFrame + p] = val;
            }
        }

        gdcm::DataElement pixelData(tags::PixelData);
        pixelData.SetByteValue(
            reinterpret_cast<const char*>(pixelBuffer.data()),
            static_cast<uint32_t>(totalPixels));
        ds.Insert(pixelData);

        auto& header = file.GetHeader();
        gdcm::DataElement msSop(tags::MediaStorageSOPClassUID);
        msSop.SetByteValue(sopClass.c_str(),
                           static_cast<uint32_t>(sopClass.size()));
        header.Insert(msSop);

        gdcm::DataElement msInstance(tags::MediaStorageSOPInstanceUID);
        std::string instUid = uidGen.Generate();
        msInstance.SetByteValue(instUid.c_str(),
                                static_cast<uint32_t>(instUid.size()));
        header.Insert(msInstance);

        std::string tsUid = "1.2.840.10008.1.2.1";
        gdcm::DataElement tsElem(tags::TransferSyntaxUID);
        tsElem.SetByteValue(tsUid.c_str(),
                            static_cast<uint32_t>(tsUid.size()));
        header.Insert(tsElem);

        writer.Write();
        return path;
    }

    /// Write a synthetic DICOM with full spatial metadata (functional groups)
    /// for volume assembly testing.
    std::string writeSyntheticVolumeFile(
        const std::string& filename,
        int rows, int cols, int numFrames,
        double pixelSpacingX, double pixelSpacingY,
        double sliceSpacing,
        short baseValue = 100, short frameIncrement = 10)
    {
        auto path = (tempDir_ / filename).string();

        gdcm::Writer writer;
        writer.SetFileName(path.c_str());
        auto& file = writer.GetFile();
        auto& ds = file.GetDataSet();

        // Pixel format attributes
        insertStringElement(ds, tags::SamplesPerPixel, "1");
        insertStringElement(ds, tags::NumberOfFrames,
                            std::to_string(numFrames));
        insertStringElement(ds, tags::Rows, std::to_string(rows));
        insertStringElement(ds, tags::Columns, std::to_string(cols));
        insertStringElement(ds, tags::BitsAllocated, "16");
        insertStringElement(ds, tags::BitsStored, "16");
        insertStringElement(ds, tags::HighBit, "15");
        insertStringElement(ds, tags::PixelRepresentation, "1");
        insertStringElement(ds, tags::PhotometricInterpretation, "MONOCHROME2");

        std::string sopClass = "1.2.840.10008.5.1.4.1.1.2.1";
        insertStringElement(ds, tags::SOPClassUID, sopClass);
        gdcm::UIDGenerator uidGen;
        insertStringElement(ds, tags::SOPInstanceUID, uidGen.Generate());

        // Pixel data
        size_t pixelsPerFrame = static_cast<size_t>(rows) * cols;
        size_t totalPixels = pixelsPerFrame * numFrames;
        std::vector<short> pixelBuffer(totalPixels);

        for (int f = 0; f < numFrames; ++f) {
            short val = static_cast<short>(baseValue + f * frameIncrement);
            for (size_t p = 0; p < pixelsPerFrame; ++p) {
                pixelBuffer[f * pixelsPerFrame + p] = val;
            }
        }

        gdcm::DataElement pixelData(tags::PixelData);
        pixelData.SetByteValue(
            reinterpret_cast<const char*>(pixelBuffer.data()),
            static_cast<uint32_t>(totalPixels * sizeof(short)));
        ds.Insert(pixelData);

        // Shared functional groups: pixel spacing + orientation
        gdcm::DataSet pixelMeasuresDs;
        insertStringElement(pixelMeasuresDs, tags::PixelSpacing,
                            std::to_string(pixelSpacingX) + "\\"
                            + std::to_string(pixelSpacingY));
        insertStringElement(pixelMeasuresDs, tags::SliceThickness,
                            std::to_string(sliceSpacing));

        gdcm::DataSet orientDs;
        insertStringElement(orientDs, tags::ImageOrientationPatient,
                            "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");

        gdcm::DataSet sharedGroupDs;
        insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                               pixelMeasuresDs);
        insertSequenceWithItem(sharedGroupDs, tags::PlaneOrientationSequence,
                               orientDs);
        insertSequenceWithItem(ds, tags::SharedFunctionalGroups,
                               sharedGroupDs);

        // Per-frame functional groups: position for each frame
        std::vector<gdcm::DataSet> perFrameItems;
        for (int f = 0; f < numFrames; ++f) {
            gdcm::DataSet planePosDs;
            insertStringElement(planePosDs, tags::ImagePositionPatient,
                                "0.0\\0.0\\"
                                + std::to_string(f * sliceSpacing));

            gdcm::DataSet frameItemDs;
            insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                                   planePosDs);
            perFrameItems.push_back(frameItemDs);
        }
        insertSequenceWithItems(ds, tags::PerFrameFunctionalGroups,
                                perFrameItems);

        // File meta information
        auto& header = file.GetHeader();
        gdcm::DataElement msSop(tags::MediaStorageSOPClassUID);
        msSop.SetByteValue(sopClass.c_str(),
                           static_cast<uint32_t>(sopClass.size()));
        header.Insert(msSop);

        gdcm::DataElement msInstance(tags::MediaStorageSOPInstanceUID);
        std::string instUid = uidGen.Generate();
        msInstance.SetByteValue(instUid.c_str(),
                                static_cast<uint32_t>(instUid.size()));
        header.Insert(msInstance);

        std::string tsUid = "1.2.840.10008.1.2.1";
        gdcm::DataElement tsElem(tags::TransferSyntaxUID);
        tsElem.SetByteValue(tsUid.c_str(),
                            static_cast<uint32_t>(tsUid.size()));
        header.Insert(tsElem);

        writer.Write();
        return path;
    }

    FrameExtractor extractor_;
    std::filesystem::path tempDir_;
};

// =============================================================================
// Construction / Lifecycle tests
// =============================================================================

TEST_F(FrameExtractorTest, ConstructionAndDestruction) {
    FrameExtractor extractor;
    // Verify no crash on construction/destruction
}

TEST_F(FrameExtractorTest, MoveConstruction) {
    FrameExtractor ext1;
    FrameExtractor ext2(std::move(ext1));
    // Verify no crash on move construction
}

TEST_F(FrameExtractorTest, MoveAssignment) {
    FrameExtractor ext1;
    FrameExtractor ext2;
    ext2 = std::move(ext1);
    // Verify no crash on move assignment
}

TEST_F(FrameExtractorTest, MoveConstructedExtractorIsUsable) {
    FrameExtractor ext1;
    FrameExtractor ext2(std::move(ext1));

    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 3);
    // The moved-to extractor should be usable for error paths
    auto result = ext2.extractFrame("/nonexistent/path.dcm", 0, info);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// extractFrame: invalid index error paths
// =============================================================================

TEST_F(FrameExtractorTest, ExtractFrameNegativeIndex) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 5);

    auto result = extractor_.extractFrame("/some/file.dcm", -1, info);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

TEST_F(FrameExtractorTest, ExtractFrameIndexEqualToCount) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 5);

    auto result = extractor_.extractFrame("/some/file.dcm", 5, info);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

TEST_F(FrameExtractorTest, ExtractFrameIndexBeyondCount) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 5);

    auto result = extractor_.extractFrame("/some/file.dcm", 100, info);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

TEST_F(FrameExtractorTest, ExtractFrameIndexZeroWithZeroFrames) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 0);

    auto result = extractor_.extractFrame("/some/file.dcm", 0, info);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

// =============================================================================
// extractFrame: nonexistent file error path
// =============================================================================

TEST_F(FrameExtractorTest, ExtractFrameNonexistentFile) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 5);

    auto result = extractor_.extractFrame("/nonexistent/file.dcm", 0, info);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::ParseFailed);
}

// =============================================================================
// extractFrame: synthetic DICOM positive path — 16-bit signed
// =============================================================================

TEST_F(FrameExtractorTest, ExtractFrameSigned16BitFirstFrame) {
    const int rows = 4, cols = 4, numFrames = 3;
    const short baseValue = 100, increment = 50;

    auto path = writeSyntheticDicom16s("signed16.dcm",
                                       rows, cols, numFrames,
                                       baseValue, increment);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 16, 1);

    auto result = extractor_.extractFrame(path, 0, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto& frameData = result.value();
    size_t expectedBytes = static_cast<size_t>(rows) * cols * sizeof(short);
    EXPECT_EQ(frameData.size(), expectedBytes);

    // Verify pixel values: frame 0 should have baseValue (100)
    const auto* pixels = reinterpret_cast<const short*>(frameData.data());
    for (int i = 0; i < rows * cols; ++i) {
        EXPECT_EQ(pixels[i], baseValue)
            << "Pixel " << i << " mismatch in frame 0";
    }
}

TEST_F(FrameExtractorTest, ExtractFrameSigned16BitLastFrame) {
    const int rows = 4, cols = 4, numFrames = 3;
    const short baseValue = 100, increment = 50;

    auto path = writeSyntheticDicom16s("signed16_last.dcm",
                                       rows, cols, numFrames,
                                       baseValue, increment);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 16, 1);

    auto result = extractor_.extractFrame(path, numFrames - 1, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto& frameData = result.value();
    const auto* pixels = reinterpret_cast<const short*>(frameData.data());
    short expectedVal = static_cast<short>(baseValue
                                           + (numFrames - 1) * increment);
    for (int i = 0; i < rows * cols; ++i) {
        EXPECT_EQ(pixels[i], expectedVal)
            << "Pixel " << i << " mismatch in last frame";
    }
}

TEST_F(FrameExtractorTest, ExtractFrameSigned16BitMiddleFrame) {
    const int rows = 4, cols = 4, numFrames = 5;
    const short baseValue = -100, increment = 25;

    auto path = writeSyntheticDicom16s("signed16_mid.dcm",
                                       rows, cols, numFrames,
                                       baseValue, increment);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 16, 1);

    auto result = extractor_.extractFrame(path, 2, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto& frameData = result.value();
    const auto* pixels = reinterpret_cast<const short*>(frameData.data());
    short expectedVal = static_cast<short>(baseValue + 2 * increment);  // -50
    for (int i = 0; i < rows * cols; ++i) {
        EXPECT_EQ(pixels[i], expectedVal);
    }
}

// =============================================================================
// extractFrame: 16-bit unsigned pixel data
// =============================================================================

TEST_F(FrameExtractorTest, ExtractFrameUnsigned16Bit) {
    const int rows = 4, cols = 4, numFrames = 3;

    auto path = writeSyntheticDicom16u("unsigned16.dcm",
                                       rows, cols, numFrames,
                                       200, 100);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 16, 0);

    auto result = extractor_.extractFrame(path, 1, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto& frameData = result.value();
    size_t expectedBytes = static_cast<size_t>(rows) * cols * sizeof(uint16_t);
    EXPECT_EQ(frameData.size(), expectedBytes);

    // Frame 1: value = 200 + 1*100 = 300
    const auto* pixels = reinterpret_cast<const uint16_t*>(frameData.data());
    for (int i = 0; i < rows * cols; ++i) {
        EXPECT_EQ(pixels[i], 300u);
    }
}

// =============================================================================
// extractFrame: 8-bit pixel data
// =============================================================================

TEST_F(FrameExtractorTest, ExtractFrameUnsigned8Bit) {
    const int rows = 4, cols = 4, numFrames = 2;

    auto path = writeSyntheticDicom8("unsigned8.dcm",
                                     rows, cols, numFrames,
                                     false, 50, 30);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 8, 0);

    auto result = extractor_.extractFrame(path, 0, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto& frameData = result.value();
    size_t expectedBytes = static_cast<size_t>(rows) * cols;
    EXPECT_EQ(frameData.size(), expectedBytes);

    // Frame 0: value = 50
    for (size_t i = 0; i < expectedBytes; ++i) {
        EXPECT_EQ(static_cast<uint8_t>(frameData[i]), 50u);
    }
}

TEST_F(FrameExtractorTest, ExtractFrameSigned8Bit) {
    const int rows = 4, cols = 4, numFrames = 2;

    auto path = writeSyntheticDicom8("signed8.dcm",
                                     rows, cols, numFrames,
                                     true, 50, 30);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 8, 1);

    auto result = extractor_.extractFrame(path, 1, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto& frameData = result.value();
    // Frame 1: value = 50 + 30 = 80
    for (size_t i = 0; i < frameData.size(); ++i) {
        EXPECT_EQ(static_cast<uint8_t>(frameData[i]), 80u);
    }
}

// =============================================================================
// extractFrame: pixel value preservation (negative values)
// =============================================================================

TEST_F(FrameExtractorTest, ExtractFramePreservesNegativeValues) {
    const int rows = 2, cols = 2, numFrames = 2;
    const short baseValue = -1024, increment = 512;

    auto path = writeSyntheticDicom16s("negative_vals.dcm",
                                       rows, cols, numFrames,
                                       baseValue, increment);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 16, 1);

    // Frame 0: -1024
    auto result0 = extractor_.extractFrame(path, 0, info);
    ASSERT_TRUE(result0.has_value());
    const auto* px0 = reinterpret_cast<const short*>(result0.value().data());
    EXPECT_EQ(px0[0], -1024);

    // Frame 1: -512
    auto result1 = extractor_.extractFrame(path, 1, info);
    ASSERT_TRUE(result1.has_value());
    const auto* px1 = reinterpret_cast<const short*>(result1.value().data());
    EXPECT_EQ(px1[0], -512);
}

// =============================================================================
// assembleVolumeFromFrames: empty frames error path
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeEmptyFrameIndices) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 5);
    std::vector<int> empty;

    auto result = extractor_.assembleVolumeFromFrames(
        "/some/file.dcm", info, empty);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

// =============================================================================
// assembleVolumeFromFrames: nonexistent file error path
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeNonexistentFile) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 5);
    std::vector<int> indices = {0, 1, 2};

    auto result = extractor_.assembleVolumeFromFrames(
        "/nonexistent/file.dcm", info, indices);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::ParseFailed);
}

// =============================================================================
// assembleVolume: delegates to assembleVolumeFromFrames
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeNonexistentFileDelegates) {
    EnhancedSeriesInfo info = makeSeriesInfo(4, 4, 3);

    auto result = extractor_.assembleVolume("/nonexistent/file.dcm", info);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::ParseFailed);
}

// =============================================================================
// assembleVolumeFromFrames: synthetic DICOM with spatial metadata
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeCorrectDimensions) {
    const int rows = 4, cols = 4, numFrames = 3;
    auto path = writeSyntheticVolumeFile("volume_dims.dcm",
                                         rows, cols, numFrames,
                                         0.5, 0.5, 2.0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    info.pixelSpacingX = 0.5;
    info.pixelSpacingY = 0.5;
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, i * 2.0};
        info.frames[i].sliceThickness = 2.0;
    }

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1, 2});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto size = volume->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], static_cast<unsigned int>(cols));
    EXPECT_EQ(size[1], static_cast<unsigned int>(rows));
    EXPECT_EQ(size[2], static_cast<unsigned int>(numFrames));
}

TEST_F(FrameExtractorTest, AssembleVolumeCorrectOrigin) {
    const int rows = 4, cols = 4, numFrames = 3;
    auto path = writeSyntheticVolumeFile("volume_origin.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 2.5);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    // Origin at first frame position after spatial sorting
    info.frames[0].imagePosition = {-10.0, -20.0, 0.0};
    info.frames[1].imagePosition = {-10.0, -20.0, 2.5};
    info.frames[2].imagePosition = {-10.0, -20.0, 5.0};

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1, 2});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto origin = volume->GetOrigin();
    EXPECT_DOUBLE_EQ(origin[0], -10.0);
    EXPECT_DOUBLE_EQ(origin[1], -20.0);
    EXPECT_DOUBLE_EQ(origin[2], 0.0);
}

TEST_F(FrameExtractorTest, AssembleVolumeCorrectSpacing) {
    const int rows = 4, cols = 4, numFrames = 3;
    auto path = writeSyntheticVolumeFile("volume_spacing.dcm",
                                         rows, cols, numFrames,
                                         0.5, 0.75, 2.5);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    info.pixelSpacingX = 0.5;
    info.pixelSpacingY = 0.75;
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, i * 2.5};
        info.frames[i].sliceThickness = 2.5;
    }

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1, 2});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto spacing = volume->GetSpacing();
    EXPECT_DOUBLE_EQ(spacing[0], 0.5);
    EXPECT_DOUBLE_EQ(spacing[1], 0.75);
    EXPECT_NEAR(spacing[2], 2.5, 0.01);
}

TEST_F(FrameExtractorTest, AssembleVolumeDirectionCosines) {
    const int rows = 4, cols = 4, numFrames = 2;
    auto path = writeSyntheticVolumeFile("volume_direction.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 3.0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    // Standard axial orientation: row=(1,0,0), col=(0,1,0)
    for (auto& frame : info.frames) {
        frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    }
    info.frames[0].imagePosition = {0.0, 0.0, 0.0};
    info.frames[1].imagePosition = {0.0, 0.0, 3.0};

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto direction = volume->GetDirection();
    // Row direction
    EXPECT_DOUBLE_EQ(direction[0][0], 1.0);
    EXPECT_DOUBLE_EQ(direction[1][0], 0.0);
    EXPECT_DOUBLE_EQ(direction[2][0], 0.0);
    // Column direction
    EXPECT_DOUBLE_EQ(direction[0][1], 0.0);
    EXPECT_DOUBLE_EQ(direction[1][1], 1.0);
    EXPECT_DOUBLE_EQ(direction[2][1], 0.0);
    // Slice normal (cross product)
    EXPECT_DOUBLE_EQ(direction[0][2], 0.0);
    EXPECT_DOUBLE_EQ(direction[1][2], 0.0);
    EXPECT_DOUBLE_EQ(direction[2][2], 1.0);
}

// =============================================================================
// assembleVolumeFromFrames: rescale parameter application
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeIdentityRescale) {
    const int rows = 2, cols = 2, numFrames = 2;
    const short baseValue = 500;
    auto path = writeSyntheticVolumeFile("vol_identity_rescale.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 1.0,
                                         baseValue, 100);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    // Identity rescale: slope=1, intercept=0 (default)
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, static_cast<double>(i)};
    }

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    // Frame 0: pixel value = 500 * 1.0 + 0.0 = 500
    itk::Image<short, 3>::IndexType idx = {{0, 0, 0}};
    EXPECT_EQ(volume->GetPixel(idx), baseValue);

    // Frame 1: pixel value = 600 * 1.0 + 0.0 = 600
    itk::Image<short, 3>::IndexType idx1 = {{0, 0, 1}};
    EXPECT_EQ(volume->GetPixel(idx1), static_cast<short>(baseValue + 100));
}

TEST_F(FrameExtractorTest, AssembleVolumeWithRescale) {
    const int rows = 2, cols = 2, numFrames = 2;
    const short rawValue = 1000;
    auto path = writeSyntheticVolumeFile("vol_rescale.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 1.0,
                                         rawValue, 0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, static_cast<double>(i)};
        info.frames[i].rescaleSlope = 1.0;
        info.frames[i].rescaleIntercept = -1024.0;
    }

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    // HU = 1000 * 1.0 + (-1024.0) = -24
    itk::Image<short, 3>::IndexType idx = {{0, 0, 0}};
    EXPECT_EQ(volume->GetPixel(idx), -24);
}

TEST_F(FrameExtractorTest, AssembleVolumePerFrameRescaleVariation) {
    const int rows = 2, cols = 2, numFrames = 2;
    const short rawValue = 500;
    auto path = writeSyntheticVolumeFile("vol_perframe_rescale.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 1.0,
                                         rawValue, 0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    info.frames[0].imagePosition = {0.0, 0.0, 0.0};
    info.frames[0].rescaleSlope = 2.0;
    info.frames[0].rescaleIntercept = -500.0;
    // Frame 0: HU = 500 * 2.0 + (-500.0) = 500

    info.frames[1].imagePosition = {0.0, 0.0, 1.0};
    info.frames[1].rescaleSlope = 0.5;
    info.frames[1].rescaleIntercept = 100.0;
    // Frame 1: HU = 500 * 0.5 + 100.0 = 350

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    itk::Image<short, 3>::IndexType idx0 = {{0, 0, 0}};
    EXPECT_EQ(volume->GetPixel(idx0), 500);

    itk::Image<short, 3>::IndexType idx1 = {{0, 0, 1}};
    EXPECT_EQ(volume->GetPixel(idx1), 350);
}

TEST_F(FrameExtractorTest, AssembleVolumeRescaleClampToShortRange) {
    const int rows = 2, cols = 2, numFrames = 1;
    const short rawValue = 30000;
    auto path = writeSyntheticVolumeFile("vol_clamp.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 1.0,
                                         rawValue, 0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    info.frames[0].imagePosition = {0.0, 0.0, 0.0};
    info.frames[0].rescaleSlope = 2.0;
    info.frames[0].rescaleIntercept = 0.0;
    // HU = 30000 * 2.0 = 60000 → clamped to 32767

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    itk::Image<short, 3>::IndexType idx = {{0, 0, 0}};
    EXPECT_EQ(volume->GetPixel(idx), 32767);
}

// =============================================================================
// assembleVolumeFromFrames: spatial sorting
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeSortsByPosition) {
    // Provide frames in reverse spatial order; verify sorting corrects it
    const int rows = 2, cols = 2, numFrames = 3;
    auto path = writeSyntheticVolumeFile("vol_sort.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 5.0,
                                         100, 100);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    // Reverse order positions: frame 0 at z=10, frame 1 at z=5, frame 2 at z=0
    info.frames[0].imagePosition = {0.0, 0.0, 10.0};
    info.frames[1].imagePosition = {0.0, 0.0, 5.0};
    info.frames[2].imagePosition = {0.0, 0.0, 0.0};

    // Pass indices in original order — sorting should reorder
    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1, 2});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    // After sorting: slice 0 = frame 2 (z=0, val=300),
    //               slice 1 = frame 1 (z=5, val=200),
    //               slice 2 = frame 0 (z=10, val=100)
    itk::Image<short, 3>::IndexType idx0 = {{0, 0, 0}};
    itk::Image<short, 3>::IndexType idx2 = {{0, 0, 2}};

    // Origin should be from the lowest z position
    auto origin = volume->GetOrigin();
    EXPECT_DOUBLE_EQ(origin[2], 0.0);

    // Verify spatial ordering is reflected in pixel values
    short slice0Val = volume->GetPixel(idx0);
    short slice2Val = volume->GetPixel(idx2);
    // After sort: slice 0 = frame 2 (raw=300), slice 2 = frame 0 (raw=100)
    EXPECT_EQ(slice0Val, 300);
    EXPECT_EQ(slice2Val, 100);
}

// =============================================================================
// assembleVolumeFromFrames: subset of frames
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeFromSubset) {
    const int rows = 2, cols = 2, numFrames = 5;
    auto path = writeSyntheticVolumeFile("vol_subset.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 1.0,
                                         100, 50);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, static_cast<double>(i)};
    }

    // Only use frames 1, 3 (skip 0, 2, 4)
    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {1, 3});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto size = volume->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[2], 2u);  // Only 2 slices

    // Frame 1: raw=150, frame 3: raw=250
    itk::Image<short, 3>::IndexType idx0 = {{0, 0, 0}};
    itk::Image<short, 3>::IndexType idx1 = {{0, 0, 1}};
    EXPECT_EQ(volume->GetPixel(idx0), 150);
    EXPECT_EQ(volume->GetPixel(idx1), 250);
}

// =============================================================================
// assembleVolume: single-frame degenerate case
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeSingleFrame) {
    const int rows = 4, cols = 4, numFrames = 1;
    auto path = writeSyntheticVolumeFile("vol_single.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 3.0,
                                         42, 0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    info.frames[0].imagePosition = {-5.0, -5.0, 0.0};
    info.frames[0].sliceThickness = 3.0;

    auto result = extractor_.assembleVolume(path, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto size = volume->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], 4u);
    EXPECT_EQ(size[1], 4u);
    EXPECT_EQ(size[2], 1u);

    // Z spacing should use sliceThickness when only 1 frame
    auto spacing = volume->GetSpacing();
    EXPECT_DOUBLE_EQ(spacing[2], 3.0);

    // Pixel value check
    itk::Image<short, 3>::IndexType idx = {{0, 0, 0}};
    EXPECT_EQ(volume->GetPixel(idx), 42);
}

// =============================================================================
// assembleVolumeFromFrames: Z spacing from positions vs sliceThickness
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeZSpacingFromPositions) {
    const int rows = 2, cols = 2, numFrames = 3;
    auto path = writeSyntheticVolumeFile("vol_zspacing.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 2.5);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, i * 2.5};
        info.frames[i].sliceThickness = 5.0;  // Different from spacing
    }

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1, 2});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto spacing = volume->GetSpacing();
    // Z spacing should be computed from positions (2.5), not sliceThickness (5.0)
    EXPECT_NEAR(spacing[2], 2.5, 0.01);
}

TEST_F(FrameExtractorTest, AssembleVolumeZSpacingFallbackToThickness) {
    // When slices are at same Z position (zDist < 0.001), use sliceThickness
    const int rows = 2, cols = 2, numFrames = 2;
    auto path = writeSyntheticVolumeFile("vol_zfallback.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 3.0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    // Both frames at nearly same Z position
    info.frames[0].imagePosition = {0.0, 0.0, 0.0};
    info.frames[1].imagePosition = {0.0, 0.0, 0.0001};
    info.frames[0].sliceThickness = 3.0;
    info.frames[1].sliceThickness = 3.0;

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto spacing = volume->GetSpacing();
    // zDist = 0.0001 < 0.001 → fallback to sliceThickness
    EXPECT_DOUBLE_EQ(spacing[2], 3.0);
}

// =============================================================================
// assembleVolumeFromFrames: coronal orientation direction cosines
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeCoronalOrientation) {
    const int rows = 2, cols = 2, numFrames = 2;
    auto path = writeSyntheticVolumeFile("vol_coronal.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 3.0);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    // Coronal orientation: row=(1,0,0), col=(0,0,-1)
    for (auto& frame : info.frames) {
        frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 0.0, -1.0};
    }
    // Positions along the coronal slice normal (Y axis)
    info.frames[0].imagePosition = {0.0, 0.0, 0.0};
    info.frames[1].imagePosition = {0.0, 3.0, 0.0};

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto direction = volume->GetDirection();
    // Row direction: (1, 0, 0)
    EXPECT_DOUBLE_EQ(direction[0][0], 1.0);
    // Column direction: (0, 0, -1)
    EXPECT_DOUBLE_EQ(direction[2][1], -1.0);
    // Slice normal = cross(row, col) = (0,0,0)x... = (0,1,0)
    EXPECT_NEAR(direction[1][2], 1.0, 0.01);
}

// =============================================================================
// assembleVolume: large frame count
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeLargeFrameCount) {
    const int rows = 2, cols = 2, numFrames = 100;
    auto path = writeSyntheticVolumeFile("vol_large.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 1.0,
                                         0, 1);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, static_cast<double>(i)};
    }

    std::vector<int> allIndices(numFrames);
    std::iota(allIndices.begin(), allIndices.end(), 0);

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, allIndices);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto size = volume->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[2], static_cast<unsigned int>(numFrames));

    // Verify first, middle, last slices
    itk::Image<short, 3>::IndexType idxFirst = {{0, 0, 0}};
    itk::Image<short, 3>::IndexType idxMid = {{0, 0, 49}};
    itk::Image<short, 3>::IndexType idxLast = {{0, 0, 99}};
    EXPECT_EQ(volume->GetPixel(idxFirst), 0);
    EXPECT_EQ(volume->GetPixel(idxMid), 49);
    EXPECT_EQ(volume->GetPixel(idxLast), 99);
}

// =============================================================================
// assembleVolume: pixel data integrity — multiple pixels per frame
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeMultiplePixelsPerFrame) {
    // Create a file with a gradient pattern within each frame
    const int rows = 2, cols = 3, numFrames = 2;
    auto path = (tempDir_ / "vol_gradient.dcm").string();

    gdcm::Writer writer;
    writer.SetFileName(path.c_str());
    auto& file = writer.GetFile();
    auto& ds = file.GetDataSet();

    insertStringElement(ds, tags::SamplesPerPixel, "1");
    insertStringElement(ds, tags::NumberOfFrames,
                        std::to_string(numFrames));
    insertStringElement(ds, tags::Rows, std::to_string(rows));
    insertStringElement(ds, tags::Columns, std::to_string(cols));
    insertStringElement(ds, tags::BitsAllocated, "16");
    insertStringElement(ds, tags::BitsStored, "16");
    insertStringElement(ds, tags::HighBit, "15");
    insertStringElement(ds, tags::PixelRepresentation, "1");
    insertStringElement(ds, tags::PhotometricInterpretation, "MONOCHROME2");

    std::string sopClass = "1.2.840.10008.5.1.4.1.1.2.1";
    insertStringElement(ds, tags::SOPClassUID, sopClass);
    gdcm::UIDGenerator uidGen;
    insertStringElement(ds, tags::SOPInstanceUID, uidGen.Generate());

    // Frame 0: pixels = [10, 20, 30, 40, 50, 60]
    // Frame 1: pixels = [110, 120, 130, 140, 150, 160]
    size_t pixelsPerFrame = static_cast<size_t>(rows) * cols;
    std::vector<short> pixelBuffer;
    for (int f = 0; f < numFrames; ++f) {
        for (size_t p = 0; p < pixelsPerFrame; ++p) {
            pixelBuffer.push_back(
                static_cast<short>(f * 100 + (p + 1) * 10));
        }
    }

    gdcm::DataElement pixelData(tags::PixelData);
    pixelData.SetByteValue(
        reinterpret_cast<const char*>(pixelBuffer.data()),
        static_cast<uint32_t>(pixelBuffer.size() * sizeof(short)));
    ds.Insert(pixelData);

    // Functional groups for volume assembly
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "1.0\\1.0");
    gdcm::DataSet orientDs;
    insertStringElement(orientDs, tags::ImageOrientationPatient,
                        "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");
    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);
    insertSequenceWithItem(sharedGroupDs, tags::PlaneOrientationSequence,
                           orientDs);
    insertSequenceWithItem(ds, tags::SharedFunctionalGroups, sharedGroupDs);

    std::vector<gdcm::DataSet> perFrameItems;
    for (int f = 0; f < numFrames; ++f) {
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "0.0\\0.0\\" + std::to_string(f * 1.0));
        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                               planePosDs);
        perFrameItems.push_back(frameItemDs);
    }
    insertSequenceWithItems(ds, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    auto& header = file.GetHeader();
    gdcm::DataElement msSop(tags::MediaStorageSOPClassUID);
    msSop.SetByteValue(sopClass.c_str(),
                       static_cast<uint32_t>(sopClass.size()));
    header.Insert(msSop);
    gdcm::DataElement msInstance(tags::MediaStorageSOPInstanceUID);
    std::string instUid = uidGen.Generate();
    msInstance.SetByteValue(instUid.c_str(),
                            static_cast<uint32_t>(instUid.size()));
    header.Insert(msInstance);
    std::string tsUid = "1.2.840.10008.1.2.1";
    gdcm::DataElement tsElem(tags::TransferSyntaxUID);
    tsElem.SetByteValue(tsUid.c_str(),
                        static_cast<uint32_t>(tsUid.size()));
    header.Insert(tsElem);
    writer.Write();

    // Now assemble volume
    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, i * 1.0};
    }

    auto result = extractor_.assembleVolumeFromFrames(
        path, info, {0, 1});
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    // Check specific pixels in frame 0 (slice 0)
    // Pixel layout: [col, row, slice] → raw index in buffer
    // ITK indexing: [x=col, y=row, z=slice]
    // Frame 0, row 0, col 0 → raw index 0 → value 10
    itk::Image<short, 3>::IndexType idx00 = {{0, 0, 0}};
    EXPECT_EQ(volume->GetPixel(idx00), 10);
    // Frame 0, row 0, col 2 → raw index 2 → value 30
    itk::Image<short, 3>::IndexType idx20 = {{2, 0, 0}};
    EXPECT_EQ(volume->GetPixel(idx20), 30);
    // Frame 1, row 0, col 0 → value 110
    itk::Image<short, 3>::IndexType idx01 = {{0, 0, 1}};
    EXPECT_EQ(volume->GetPixel(idx01), 110);
}

// =============================================================================
// assembleVolume: all frames via assembleVolume() convenience method
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeConvenienceMethod) {
    const int rows = 2, cols = 2, numFrames = 3;
    auto path = writeSyntheticVolumeFile("vol_convenience.dcm",
                                         rows, cols, numFrames,
                                         1.0, 1.0, 2.0,
                                         10, 20);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames);
    for (int i = 0; i < numFrames; ++i) {
        info.frames[i].imagePosition = {0.0, 0.0, i * 2.0};
    }

    auto result = extractor_.assembleVolume(path, info);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto volume = result.value();
    auto size = volume->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[2], static_cast<unsigned int>(numFrames));

    // Verify all 3 slices: values 10, 30, 50
    itk::Image<short, 3>::IndexType idx0 = {{0, 0, 0}};
    itk::Image<short, 3>::IndexType idx1 = {{0, 0, 1}};
    itk::Image<short, 3>::IndexType idx2 = {{0, 0, 2}};
    EXPECT_EQ(volume->GetPixel(idx0), 10);
    EXPECT_EQ(volume->GetPixel(idx1), 30);
    EXPECT_EQ(volume->GetPixel(idx2), 50);
}

// =============================================================================
// sortFramesBySpatialPosition: verified through assembleVolume ordering
// =============================================================================

TEST_F(FrameExtractorTest, AssembleVolumeSortFramesBySpatialPositionEmpty) {
    // sortFramesBySpatialPosition handles empty indices internally.
    // Tested via assembleVolumeFromFrames empty indices error.
    EnhancedSeriesInfo info = makeSeriesInfo(2, 2, 3);
    std::vector<int> empty;
    auto result = extractor_.assembleVolumeFromFrames(
        "/nonexistent.dcm", info, empty);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

// =============================================================================
// extractFrame: frame size consistency across multiple frames
// =============================================================================

TEST_F(FrameExtractorTest, ExtractAllFramesSameSize) {
    const int rows = 4, cols = 4, numFrames = 4;
    auto path = writeSyntheticDicom16s("multi_frames.dcm",
                                       rows, cols, numFrames,
                                       0, 100);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 16, 1);
    size_t expectedBytes = static_cast<size_t>(rows) * cols * sizeof(short);

    for (int f = 0; f < numFrames; ++f) {
        auto result = extractor_.extractFrame(path, f, info);
        ASSERT_TRUE(result.has_value())
            << "Frame " << f << ": " << result.error().toString();
        EXPECT_EQ(result.value().size(), expectedBytes)
            << "Frame " << f << " size mismatch";
    }
}

// =============================================================================
// extractFrame: frame data independence (each frame different values)
// =============================================================================

TEST_F(FrameExtractorTest, ExtractFrameDataIndependence) {
    const int rows = 2, cols = 2, numFrames = 3;
    auto path = writeSyntheticDicom16s("independence.dcm",
                                       rows, cols, numFrames,
                                       100, 200);

    EnhancedSeriesInfo info = makeSeriesInfo(rows, cols, numFrames, 16, 1);

    for (int f = 0; f < numFrames; ++f) {
        auto result = extractor_.extractFrame(path, f, info);
        ASSERT_TRUE(result.has_value());
        const auto* pixels =
            reinterpret_cast<const short*>(result.value().data());
        short expected = static_cast<short>(100 + f * 200);
        EXPECT_EQ(pixels[0], expected) << "Frame " << f;
    }
}

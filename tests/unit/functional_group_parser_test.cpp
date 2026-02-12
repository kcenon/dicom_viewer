#include <gtest/gtest.h>

#include "services/enhanced_dicom/enhanced_dicom_types.hpp"
#include "services/enhanced_dicom/functional_group_parser.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <gdcmDataElement.h>
#include <gdcmDataSet.h>
#include <gdcmFile.h>
#include <gdcmItem.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmTag.h>
#include <gdcmWriter.h>

using namespace dicom_viewer::services;

// =============================================================================
// Helper: DICOM tag constants (mirroring functional_group_parser.cpp)
// =============================================================================

namespace tags {
const gdcm::Tag SharedFunctionalGroups{0x5200, 0x9229};
const gdcm::Tag PerFrameFunctionalGroups{0x5200, 0x9230};
const gdcm::Tag PlanePositionSequence{0x0020, 0x9113};
const gdcm::Tag PlaneOrientationSequence{0x0020, 0x9116};
const gdcm::Tag PixelMeasuresSequence{0x0028, 0x9110};
const gdcm::Tag PixelValueTransformationSequence{0x0028, 0x9145};
const gdcm::Tag FrameContentSequence{0x0020, 0x9111};
const gdcm::Tag ImagePositionPatient{0x0020, 0x0032};
const gdcm::Tag ImageOrientationPatient{0x0020, 0x0037};
const gdcm::Tag PixelSpacing{0x0028, 0x0030};
const gdcm::Tag SliceThickness{0x0018, 0x0050};
const gdcm::Tag RescaleIntercept{0x0028, 0x1052};
const gdcm::Tag RescaleSlope{0x0028, 0x1053};
const gdcm::Tag DimensionIndexValues{0x0020, 0x9157};
const gdcm::Tag TemporalPositionIndex{0x0020, 0x9128};
const gdcm::Tag TriggerTime{0x0018, 0x1060};
const gdcm::Tag InStackPositionNumber{0x0020, 0x9057};
const gdcm::Tag NumberOfFrames{0x0028, 0x0008};
}  // namespace tags

// =============================================================================
// Helper: insert a string-valued data element into a DataSet
// =============================================================================

namespace {

void insertStringElement(gdcm::DataSet& ds, const gdcm::Tag& tag,
                         const std::string& value) {
    gdcm::DataElement de(tag);
    de.SetByteValue(value.c_str(), static_cast<uint32_t>(value.size()));
    ds.Insert(de);
}

void insertUint32Array(gdcm::DataSet& ds, const gdcm::Tag& tag,
                       const std::vector<uint32_t>& values) {
    gdcm::DataElement de(tag);
    de.SetByteValue(reinterpret_cast<const char*>(values.data()),
                    static_cast<uint32_t>(values.size() * sizeof(uint32_t)));
    ds.Insert(de);
}

/// Create a sequence with a single item containing the given DataSet
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

/// Create a sequence with multiple items
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

/// Insert an empty sequence (no items)
void insertEmptySequence(gdcm::DataSet& parentDs, const gdcm::Tag& seqTag) {
    auto sq = gdcm::SequenceOfItems::New();
    sq->SetLengthToUndefined();

    gdcm::DataElement de(seqTag);
    de.SetValue(*sq);
    de.SetVLToUndefined();
    parentDs.Insert(de);
}

}  // anonymous namespace

// =============================================================================
// Test fixture: manages temporary DICOM file lifecycle
// =============================================================================

class FunctionalGroupParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / "fgp_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    /// Write a gdcm::File to a temporary path and return the path
    std::string writeDicomFile(const gdcm::DataSet& ds,
                               const std::string& filename) {
        auto path = (tempDir_ / filename).string();
        gdcm::Writer writer;
        writer.SetFileName(path.c_str());
        writer.GetFile().GetDataSet() = ds;
        // Set minimal file meta information
        auto& header = writer.GetFile().GetHeader();
        gdcm::DataElement mediaStorage(gdcm::Tag(0x0002, 0x0002));
        std::string sopClass = "1.2.840.10008.5.1.4.1.1.2.1";  // Enhanced CT
        mediaStorage.SetByteValue(sopClass.c_str(),
                                  static_cast<uint32_t>(sopClass.size()));
        header.Insert(mediaStorage);
        writer.Write();
        return path;
    }

    FunctionalGroupParser parser_;
    std::filesystem::path tempDir_;
};

// =============================================================================
// Construction / Lifecycle tests
// =============================================================================

TEST_F(FunctionalGroupParserTest, ConstructionAndDestruction) {
    FunctionalGroupParser parser;
    // Verify no crash on construction/destruction
}

TEST_F(FunctionalGroupParserTest, MoveConstruction) {
    FunctionalGroupParser parser1;
    FunctionalGroupParser parser2(std::move(parser1));
    // Verify no crash on move construction
}

TEST_F(FunctionalGroupParserTest, MoveAssignment) {
    FunctionalGroupParser parser1;
    FunctionalGroupParser parser2;
    parser2 = std::move(parser1);
    // Verify no crash on move assignment
}

TEST_F(FunctionalGroupParserTest, MoveConstructedParserIsUsable) {
    FunctionalGroupParser parser1;
    FunctionalGroupParser parser2(std::move(parser1));

    EnhancedSeriesInfo info;
    // The moved-to parser should be usable
    auto frames = parser2.parsePerFrameGroups("/nonexistent/path.dcm", 3, info);
    EXPECT_EQ(frames.size(), 3u);
}

// =============================================================================
// parsePerFrameGroups: nonexistent file tests
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParsePerFrameGroupsNonexistentFile) {
    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups("/nonexistent/file.dcm", 5,
                                              sharedInfo);
    EXPECT_EQ(frames.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(frames[i].frameIndex, i);
        EXPECT_DOUBLE_EQ(frames[i].rescaleSlope, 1.0);
        EXPECT_DOUBLE_EQ(frames[i].rescaleIntercept, 0.0);
    }
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameGroupsZeroFrames) {
    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups("/nonexistent/file.dcm", 0,
                                              sharedInfo);
    EXPECT_TRUE(frames.empty());
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameGroupsSingleFrame) {
    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups("/nonexistent/file.dcm", 1,
                                              sharedInfo);
    EXPECT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].frameIndex, 0);
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameGroupsLargeFrameCount) {
    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups("/nonexistent/file.dcm", 500,
                                              sharedInfo);
    EXPECT_EQ(frames.size(), 500u);
    // Verify sequential indexing
    for (int i = 0; i < 500; ++i) {
        EXPECT_EQ(frames[i].frameIndex, i);
    }
}

// =============================================================================
// parseSharedGroups: nonexistent file tests
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedGroupsNonexistentFile) {
    EnhancedSeriesInfo info;
    info.pixelSpacingX = 0.75;
    info.pixelSpacingY = 0.75;

    parser_.parseSharedGroups("/nonexistent/file.dcm", info);

    // Should not modify existing info on failure
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.75);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.75);
}

// =============================================================================
// parseSharedGroups: synthetic DICOM file — PixelMeasures
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedPixelSpacing) {
    // Build PixelMeasuresSequence item with PixelSpacing
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "0.5\\0.5");

    // Build shared functional group item containing PixelMeasuresSequence
    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    // Build top-level DataSet with SharedFunctionalGroupsSequence
    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_pixel_spacing.dcm");

    EnhancedSeriesInfo info;
    parser_.parseSharedGroups(path, info);

    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.5);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.5);
}

TEST_F(FunctionalGroupParserTest, ParseSharedSliceThickness) {
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::SliceThickness, "2.5");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_slice_thickness.dcm");

    // Pre-populate frames so parseSharedGroups can apply thickness to them
    EnhancedSeriesInfo info;
    info.frames.resize(3);
    for (int i = 0; i < 3; ++i) {
        info.frames[i].frameIndex = i;
    }

    parser_.parseSharedGroups(path, info);

    for (const auto& frame : info.frames) {
        EXPECT_DOUBLE_EQ(frame.sliceThickness, 2.5);
    }
}

TEST_F(FunctionalGroupParserTest, ParseSharedPixelSpacingAndSliceThickness) {
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "0.35\\0.35");
    insertStringElement(pixelMeasuresDs, tags::SliceThickness, "1.25");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_combined.dcm");

    EnhancedSeriesInfo info;
    info.frames.resize(2);
    parser_.parseSharedGroups(path, info);

    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.35);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.35);
    for (const auto& frame : info.frames) {
        EXPECT_DOUBLE_EQ(frame.sliceThickness, 1.25);
    }
}

// =============================================================================
// parseSharedGroups: rescale parameters
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedRescaleParameters) {
    gdcm::DataSet pvtDs;
    insertStringElement(pvtDs, tags::RescaleSlope, "2.0");
    insertStringElement(pvtDs, tags::RescaleIntercept, "-1024.0");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs,
                           tags::PixelValueTransformationSequence, pvtDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_rescale.dcm");

    EnhancedSeriesInfo info;
    info.frames.resize(3);
    parser_.parseSharedGroups(path, info);

    for (const auto& frame : info.frames) {
        EXPECT_DOUBLE_EQ(frame.rescaleSlope, 2.0);
        EXPECT_DOUBLE_EQ(frame.rescaleIntercept, -1024.0);
    }
}

TEST_F(FunctionalGroupParserTest, ParseSharedRescaleSlopeOnly) {
    gdcm::DataSet pvtDs;
    insertStringElement(pvtDs, tags::RescaleSlope, "0.5");
    // No intercept

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs,
                           tags::PixelValueTransformationSequence, pvtDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_slope_only.dcm");

    EnhancedSeriesInfo info;
    info.frames.resize(2);
    parser_.parseSharedGroups(path, info);

    for (const auto& frame : info.frames) {
        EXPECT_DOUBLE_EQ(frame.rescaleSlope, 0.5);
        EXPECT_DOUBLE_EQ(frame.rescaleIntercept, 0.0);
    }
}

// =============================================================================
// parseSharedGroups: plane orientation
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedPlaneOrientation) {
    gdcm::DataSet orientDs;
    insertStringElement(orientDs, tags::ImageOrientationPatient,
                        "1.0\\0.0\\0.0\\0.0\\0.0\\-1.0");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PlaneOrientationSequence,
                           orientDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_orientation.dcm");

    EnhancedSeriesInfo info;
    info.frames.resize(2);
    parser_.parseSharedGroups(path, info);

    for (const auto& frame : info.frames) {
        EXPECT_DOUBLE_EQ(frame.imageOrientation[0], 1.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[1], 0.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[2], 0.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[3], 0.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[4], 0.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[5], -1.0);
    }
}

// =============================================================================
// parseSharedGroups: empty/missing sequences
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedEmptySequence) {
    gdcm::DataSet topDs;
    insertEmptySequence(topDs, tags::SharedFunctionalGroups);

    std::string path = writeDicomFile(topDs, "shared_empty_seq.dcm");

    EnhancedSeriesInfo info;
    info.pixelSpacingX = 1.5;
    info.pixelSpacingY = 1.5;

    parser_.parseSharedGroups(path, info);

    // Should not modify existing values when shared sequence is empty
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 1.5);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 1.5);
}

TEST_F(FunctionalGroupParserTest, ParseSharedNoSharedSequence) {
    // File with no SharedFunctionalGroupsSequence at all
    gdcm::DataSet topDs;
    insertStringElement(topDs, tags::NumberOfFrames, "5");

    std::string path = writeDicomFile(topDs, "no_shared_seq.dcm");

    EnhancedSeriesInfo info;
    info.pixelSpacingX = 0.8;

    parser_.parseSharedGroups(path, info);

    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.8);
}

TEST_F(FunctionalGroupParserTest, ParseSharedMissingPixelMeasures) {
    // Shared group item without PixelMeasuresSequence
    gdcm::DataSet sharedGroupDs;
    // Only insert rescale, no pixel measures
    gdcm::DataSet pvtDs;
    insertStringElement(pvtDs, tags::RescaleSlope, "1.5");
    insertSequenceWithItem(sharedGroupDs,
                           tags::PixelValueTransformationSequence, pvtDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_no_pixel_measures.dcm");

    EnhancedSeriesInfo info;
    info.pixelSpacingX = 1.0;
    info.pixelSpacingY = 1.0;
    info.frames.resize(1);
    parser_.parseSharedGroups(path, info);

    // Pixel spacing should remain default (no PixelMeasuresSequence)
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 1.0);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 1.0);
    // But rescale should be parsed
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleSlope, 1.5);
}

// =============================================================================
// parseSharedGroups: combined metadata
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedAllMetadataCombined) {
    // PixelMeasuresSequence
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "0.625\\0.625");
    insertStringElement(pixelMeasuresDs, tags::SliceThickness, "3.0");

    // PixelValueTransformationSequence
    gdcm::DataSet pvtDs;
    insertStringElement(pvtDs, tags::RescaleSlope, "1.0");
    insertStringElement(pvtDs, tags::RescaleIntercept, "-1024.0");

    // PlaneOrientationSequence (sagittal orientation)
    gdcm::DataSet orientDs;
    insertStringElement(orientDs, tags::ImageOrientationPatient,
                        "0.0\\1.0\\0.0\\0.0\\0.0\\-1.0");

    // Build shared group item
    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);
    insertSequenceWithItem(sharedGroupDs,
                           tags::PixelValueTransformationSequence, pvtDs);
    insertSequenceWithItem(sharedGroupDs, tags::PlaneOrientationSequence,
                           orientDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "shared_combined_all.dcm");

    EnhancedSeriesInfo info;
    info.frames.resize(4);
    for (int i = 0; i < 4; ++i) {
        info.frames[i].frameIndex = i;
    }
    parser_.parseSharedGroups(path, info);

    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.625);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.625);

    for (const auto& frame : info.frames) {
        EXPECT_DOUBLE_EQ(frame.sliceThickness, 3.0);
        EXPECT_DOUBLE_EQ(frame.rescaleSlope, 1.0);
        EXPECT_DOUBLE_EQ(frame.rescaleIntercept, -1024.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[0], 0.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[1], 1.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[5], -1.0);
    }
}

// =============================================================================
// parsePerFrameGroups: synthetic DICOM with per-frame positions
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParsePerFrameImagePosition) {
    // Create per-frame items with different ImagePositionPatient
    std::vector<gdcm::DataSet> perFrameItems;

    for (int i = 0; i < 3; ++i) {
        gdcm::DataSet planePosDs;
        std::string posStr = "10.0\\20.0\\" + std::to_string(i * 5.0);
        insertStringElement(planePosDs, tags::ImagePositionPatient, posStr);

        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                               planePosDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_position.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 3, sharedInfo);

    ASSERT_EQ(frames.size(), 3u);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(frames[i].frameIndex, i);
        EXPECT_DOUBLE_EQ(frames[i].imagePosition[0], 10.0);
        EXPECT_DOUBLE_EQ(frames[i].imagePosition[1], 20.0);
        EXPECT_DOUBLE_EQ(frames[i].imagePosition[2], i * 5.0);
    }
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameImageOrientation) {
    // Frame 0: axial, Frame 1: coronal
    std::vector<gdcm::DataSet> perFrameItems;

    // Axial orientation
    gdcm::DataSet orientDs0;
    insertStringElement(orientDs0, tags::ImageOrientationPatient,
                        "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");
    gdcm::DataSet frame0;
    insertSequenceWithItem(frame0, tags::PlaneOrientationSequence, orientDs0);
    perFrameItems.push_back(frame0);

    // Coronal orientation
    gdcm::DataSet orientDs1;
    insertStringElement(orientDs1, tags::ImageOrientationPatient,
                        "1.0\\0.0\\0.0\\0.0\\0.0\\-1.0");
    gdcm::DataSet frame1;
    insertSequenceWithItem(frame1, tags::PlaneOrientationSequence, orientDs1);
    perFrameItems.push_back(frame1);

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_orientation.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 2, sharedInfo);

    ASSERT_EQ(frames.size(), 2u);
    // Axial
    EXPECT_DOUBLE_EQ(frames[0].imageOrientation[4], 1.0);
    EXPECT_DOUBLE_EQ(frames[0].imageOrientation[5], 0.0);
    // Coronal
    EXPECT_DOUBLE_EQ(frames[1].imageOrientation[4], 0.0);
    EXPECT_DOUBLE_EQ(frames[1].imageOrientation[5], -1.0);
}

// =============================================================================
// parsePerFrameGroups: per-frame rescale overrides
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParsePerFrameRescaleOverride) {
    std::vector<gdcm::DataSet> perFrameItems;

    for (int i = 0; i < 3; ++i) {
        gdcm::DataSet pvtDs;
        insertStringElement(pvtDs, tags::RescaleSlope,
                            std::to_string(1.0 + i * 0.5));
        insertStringElement(pvtDs, tags::RescaleIntercept,
                            std::to_string(-100.0 * i));

        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs,
                               tags::PixelValueTransformationSequence, pvtDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_rescale.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 3, sharedInfo);

    ASSERT_EQ(frames.size(), 3u);
    EXPECT_DOUBLE_EQ(frames[0].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(frames[0].rescaleIntercept, 0.0);

    EXPECT_DOUBLE_EQ(frames[1].rescaleSlope, 1.5);
    EXPECT_DOUBLE_EQ(frames[1].rescaleIntercept, -100.0);

    EXPECT_DOUBLE_EQ(frames[2].rescaleSlope, 2.0);
    EXPECT_DOUBLE_EQ(frames[2].rescaleIntercept, -200.0);
}

// =============================================================================
// parsePerFrameGroups: FrameContentSequence (DimensionIndexValues, temporal)
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParsePerFrameDimensionIndexValues) {
    std::vector<gdcm::DataSet> perFrameItems;

    for (int i = 0; i < 2; ++i) {
        gdcm::DataSet frameContentDs;
        std::vector<uint32_t> dimValues = {
            static_cast<uint32_t>(i + 1),
            static_cast<uint32_t>(i * 3 + 1)};
        insertUint32Array(frameContentDs, tags::DimensionIndexValues, dimValues);

        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::FrameContentSequence,
                               frameContentDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_dimension.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 2, sharedInfo);

    ASSERT_EQ(frames.size(), 2u);

    // Frame 0: dimension indices {0: 1, 1: 1}
    EXPECT_EQ(frames[0].dimensionIndices.at(0), 1);
    EXPECT_EQ(frames[0].dimensionIndices.at(1), 1);

    // Frame 1: dimension indices {0: 2, 1: 4}
    EXPECT_EQ(frames[1].dimensionIndices.at(0), 2);
    EXPECT_EQ(frames[1].dimensionIndices.at(1), 4);
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameTemporalPositionIndex) {
    std::vector<gdcm::DataSet> perFrameItems;

    for (int i = 0; i < 3; ++i) {
        gdcm::DataSet frameContentDs;
        insertStringElement(frameContentDs, tags::TemporalPositionIndex,
                            std::to_string(i + 1));

        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::FrameContentSequence,
                               frameContentDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_temporal.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 3, sharedInfo);

    ASSERT_EQ(frames.size(), 3u);
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(frames[i].temporalPositionIndex.has_value());
        EXPECT_EQ(frames[i].temporalPositionIndex.value(), i + 1);
    }
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameInStackPositionNumber) {
    std::vector<gdcm::DataSet> perFrameItems;

    for (int i = 0; i < 2; ++i) {
        gdcm::DataSet frameContentDs;
        insertStringElement(frameContentDs, tags::InStackPositionNumber,
                            std::to_string(i + 1));

        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::FrameContentSequence,
                               frameContentDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_instack.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 2, sharedInfo);

    ASSERT_EQ(frames.size(), 2u);
    // InStackPositionNumber stored with element tag key 0x9057
    uint32_t inStackKey = tags::InStackPositionNumber.GetElementTag();
    EXPECT_EQ(frames[0].dimensionIndices.at(inStackKey), 1);
    EXPECT_EQ(frames[1].dimensionIndices.at(inStackKey), 2);
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameTriggerTime) {
    std::vector<gdcm::DataSet> perFrameItems;

    double triggerTimes[] = {0.0, 33.3, 66.7, 100.0};
    for (int i = 0; i < 4; ++i) {
        gdcm::DataSet frameItemDs;
        insertStringElement(frameItemDs, tags::TriggerTime,
                            std::to_string(triggerTimes[i]));
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_trigger.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 4, sharedInfo);

    ASSERT_EQ(frames.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(frames[i].triggerTime.has_value());
        EXPECT_NEAR(frames[i].triggerTime.value(), triggerTimes[i], 0.1);
    }
}

// =============================================================================
// parsePerFrameGroups: frame count mismatch with sequence item count
// =============================================================================

TEST_F(FunctionalGroupParserTest, MoreFramesThanSequenceItems) {
    // Sequence has 2 items but numberOfFrames is 4
    std::vector<gdcm::DataSet> perFrameItems;
    for (int i = 0; i < 2; ++i) {
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "0.0\\0.0\\" + std::to_string(i * 10.0));
        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                               planePosDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "fewer_items.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 4, sharedInfo);

    ASSERT_EQ(frames.size(), 4u);
    // First 2 frames should have parsed positions
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[2], 0.0);
    EXPECT_DOUBLE_EQ(frames[1].imagePosition[2], 10.0);
    // Remaining frames should have default position
    EXPECT_DOUBLE_EQ(frames[2].imagePosition[2], 0.0);
    EXPECT_DOUBLE_EQ(frames[3].imagePosition[2], 0.0);
}

TEST_F(FunctionalGroupParserTest, FewerFramesThanSequenceItems) {
    // Sequence has 5 items but numberOfFrames is 2
    std::vector<gdcm::DataSet> perFrameItems;
    for (int i = 0; i < 5; ++i) {
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "0.0\\0.0\\" + std::to_string(i * 3.0));
        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                               planePosDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "more_items.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 2, sharedInfo);

    // Only 2 frames should be returned, matching numberOfFrames
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[2], 0.0);
    EXPECT_DOUBLE_EQ(frames[1].imagePosition[2], 3.0);
}

// =============================================================================
// parsePerFrameGroups: no per-frame sequence
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParsePerFrameNoSequence) {
    // File exists but has no PerFrameFunctionalGroupsSequence
    gdcm::DataSet topDs;
    insertStringElement(topDs, tags::NumberOfFrames, "3");

    std::string path = writeDicomFile(topDs, "no_perframe_seq.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 3, sharedInfo);

    ASSERT_EQ(frames.size(), 3u);
    // All frames should have default values
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(frames[i].frameIndex, i);
        EXPECT_DOUBLE_EQ(frames[i].rescaleSlope, 1.0);
        EXPECT_DOUBLE_EQ(frames[i].rescaleIntercept, 0.0);
        EXPECT_DOUBLE_EQ(frames[i].imagePosition[0], 0.0);
        EXPECT_FALSE(frames[i].triggerTime.has_value());
        EXPECT_FALSE(frames[i].temporalPositionIndex.has_value());
        EXPECT_TRUE(frames[i].dimensionIndices.empty());
    }
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameEmptySequence) {
    gdcm::DataSet topDs;
    insertEmptySequence(topDs, tags::PerFrameFunctionalGroups);

    std::string path = writeDicomFile(topDs, "empty_perframe_seq.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 2, sharedInfo);

    ASSERT_EQ(frames.size(), 2u);
    for (const auto& frame : frames) {
        EXPECT_DOUBLE_EQ(frame.rescaleSlope, 1.0);
    }
}

// =============================================================================
// parsePerFrameGroups: mixed metadata per frame
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParsePerFrameMixedMetadata) {
    std::vector<gdcm::DataSet> perFrameItems;

    // Frame 0: has position, rescale, temporal
    {
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "-100.0\\-150.0\\0.0");
        gdcm::DataSet pvtDs;
        insertStringElement(pvtDs, tags::RescaleSlope, "1.0");
        insertStringElement(pvtDs, tags::RescaleIntercept, "-1024.0");
        gdcm::DataSet frameContentDs;
        insertStringElement(frameContentDs, tags::TemporalPositionIndex, "1");

        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                               planePosDs);
        insertSequenceWithItem(frameItemDs,
                               tags::PixelValueTransformationSequence, pvtDs);
        insertSequenceWithItem(frameItemDs, tags::FrameContentSequence,
                               frameContentDs);
        perFrameItems.push_back(frameItemDs);
    }

    // Frame 1: has only position (no rescale, no temporal)
    {
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "-100.0\\-150.0\\5.0");
        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                               planePosDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "perframe_mixed.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 2, sharedInfo);

    ASSERT_EQ(frames.size(), 2u);

    // Frame 0: fully populated
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[0], -100.0);
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[2], 0.0);
    EXPECT_DOUBLE_EQ(frames[0].rescaleIntercept, -1024.0);
    ASSERT_TRUE(frames[0].temporalPositionIndex.has_value());
    EXPECT_EQ(frames[0].temporalPositionIndex.value(), 1);

    // Frame 1: only position, defaults for rest
    EXPECT_DOUBLE_EQ(frames[1].imagePosition[2], 5.0);
    EXPECT_DOUBLE_EQ(frames[1].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(frames[1].rescaleIntercept, 0.0);
    EXPECT_FALSE(frames[1].temporalPositionIndex.has_value());
}

// =============================================================================
// Edge cases: malformed values
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedMalformedPixelSpacing) {
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "abc\\def");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "malformed_spacing.dcm");

    EnhancedSeriesInfo info;
    // parseDoubleValues falls back to 0.0 for non-numeric tokens
    parser_.parseSharedGroups(path, info);

    // With malformed values, parseDoubleValues returns 0.0 for each token
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.0);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.0);
}

TEST_F(FunctionalGroupParserTest, ParseSharedMalformedSliceThickness) {
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::SliceThickness, "not_a_number");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "malformed_thickness.dcm");

    EnhancedSeriesInfo info;
    info.frames.resize(1);
    info.frames[0].sliceThickness = 1.0;  // default

    parser_.parseSharedGroups(path, info);

    // Should retain default since parsing fails
    EXPECT_DOUBLE_EQ(info.frames[0].sliceThickness, 1.0);
}

TEST_F(FunctionalGroupParserTest, ParsePerFrameMalformedRescale) {
    std::vector<gdcm::DataSet> perFrameItems;

    gdcm::DataSet pvtDs;
    insertStringElement(pvtDs, tags::RescaleSlope, "invalid");
    insertStringElement(pvtDs, tags::RescaleIntercept, "bad_value");

    gdcm::DataSet frameItemDs;
    insertSequenceWithItem(frameItemDs,
                           tags::PixelValueTransformationSequence, pvtDs);
    perFrameItems.push_back(frameItemDs);

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "malformed_rescale.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 1, sharedInfo);

    ASSERT_EQ(frames.size(), 1u);
    // Should retain default values due to parsing failure
    EXPECT_DOUBLE_EQ(frames[0].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(frames[0].rescaleIntercept, 0.0);
}

TEST_F(FunctionalGroupParserTest, ParsePerFramePartialPosition) {
    // Only 2 components instead of 3
    std::vector<gdcm::DataSet> perFrameItems;

    gdcm::DataSet planePosDs;
    insertStringElement(planePosDs, tags::ImagePositionPatient, "10.0\\20.0");

    gdcm::DataSet frameItemDs;
    insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                           planePosDs);
    perFrameItems.push_back(frameItemDs);

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "partial_position.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, 1, sharedInfo);

    ASSERT_EQ(frames.size(), 1u);
    // With only 2 values, position should remain at defaults
    // (parseDoubleValues returns 2 values, code checks size >= 3)
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[0], 0.0);
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[1], 0.0);
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[2], 0.0);
}

TEST_F(FunctionalGroupParserTest, ParseSharedPartialOrientation) {
    // Only 4 components instead of 6
    gdcm::DataSet orientDs;
    insertStringElement(orientDs, tags::ImageOrientationPatient,
                        "1.0\\0.0\\0.0\\0.0");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PlaneOrientationSequence,
                           orientDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "partial_orientation.dcm");

    EnhancedSeriesInfo info;
    info.frames.resize(1);
    // Default orientation
    info.frames[0].imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};

    parser_.parseSharedGroups(path, info);

    // Should retain default since only 4 components were provided (need >= 6)
    EXPECT_DOUBLE_EQ(info.frames[0].imageOrientation[4], 1.0);
    EXPECT_DOUBLE_EQ(info.frames[0].imageOrientation[5], 0.0);
}

// =============================================================================
// Edge cases: empty string values
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedEmptyStringValues) {
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "");
    insertStringElement(pixelMeasuresDs, tags::SliceThickness, "");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "empty_strings.dcm");

    EnhancedSeriesInfo info;
    info.pixelSpacingX = 1.0;
    info.pixelSpacingY = 1.0;
    info.frames.resize(1);
    info.frames[0].sliceThickness = 1.0;

    parser_.parseSharedGroups(path, info);

    // Empty strings should not modify existing values
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 1.0);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 1.0);
    EXPECT_DOUBLE_EQ(info.frames[0].sliceThickness, 1.0);
}

// =============================================================================
// Metadata Precedence: shared + per-frame interaction workflow
// =============================================================================

TEST_F(FunctionalGroupParserTest, SharedThenPerFrameWorkflow) {
    // Create a synthetic DICOM file with BOTH shared AND per-frame sequences.
    // This tests the intended DICOM Enhanced IOD workflow:
    // 1. parseSharedGroups() sets shared defaults on all frames
    // 2. parsePerFrameGroups() returns new frames with per-frame overrides

    // Shared: rescale slope=1.0, intercept=-1024.0
    gdcm::DataSet pvtShared;
    insertStringElement(pvtShared, tags::RescaleSlope, "1.0");
    insertStringElement(pvtShared, tags::RescaleIntercept, "-1024.0");
    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs,
                           tags::PixelValueTransformationSequence, pvtShared);
    // Shared pixel spacing
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "0.5\\0.5");
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    // Per-frame: frame 0 has rescale slope=2.0, intercept=-500.0
    //            frame 1 has NO rescale (should retain defaults)
    std::vector<gdcm::DataSet> perFrameItems;
    {
        gdcm::DataSet pvtPerFrame;
        insertStringElement(pvtPerFrame, tags::RescaleSlope, "2.0");
        insertStringElement(pvtPerFrame, tags::RescaleIntercept, "-500.0");
        gdcm::DataSet frame0;
        insertSequenceWithItem(frame0,
                               tags::PixelValueTransformationSequence,
                               pvtPerFrame);
        perFrameItems.push_back(frame0);
    }
    {
        // Frame 1: position only, no rescale override
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "0.0\\0.0\\5.0");
        gdcm::DataSet frame1;
        insertSequenceWithItem(frame1, tags::PlanePositionSequence,
                               planePosDs);
        perFrameItems.push_back(frame1);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "precedence_both.dcm");

    // Workflow: parse per-frame first → then assign and apply shared
    EnhancedSeriesInfo info;
    auto frames = parser_.parsePerFrameGroups(path, 2, info);
    info.frames = frames;

    // Per-frame results: frame 0 has override, frame 1 has defaults
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleSlope, 2.0);
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleIntercept, -500.0);
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleSlope, 1.0);      // default
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleIntercept, 0.0);   // default

    // Now apply shared groups (overwrites ALL frames)
    parser_.parseSharedGroups(path, info);

    // After shared: pixel spacing set, rescale overwritten on all frames
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.5);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.5);
    // Shared rescale overwrites per-frame — this documents actual behavior
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleIntercept, -1024.0);
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleIntercept, -1024.0);
}

TEST_F(FunctionalGroupParserTest, PerFrameOverridesWhenCalledAfterShared) {
    // Alternative workflow: shared first, then per-frame replaces frames.
    // This achieves the DICOM-intended semantics where per-frame overrides shared.

    // Shared: rescale slope=1.0, intercept=-1024.0, orientation=axial
    gdcm::DataSet pvtShared;
    insertStringElement(pvtShared, tags::RescaleSlope, "1.0");
    insertStringElement(pvtShared, tags::RescaleIntercept, "-1024.0");
    gdcm::DataSet orientShared;
    insertStringElement(orientShared, tags::ImageOrientationPatient,
                        "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");
    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs,
                           tags::PixelValueTransformationSequence, pvtShared);
    insertSequenceWithItem(sharedGroupDs, tags::PlaneOrientationSequence,
                           orientShared);

    // Per-frame: frame 0 has rescale=3.0/-500, frame 1 has no rescale
    std::vector<gdcm::DataSet> perFrameItems;
    {
        gdcm::DataSet pvtPerFrame;
        insertStringElement(pvtPerFrame, tags::RescaleSlope, "3.0");
        insertStringElement(pvtPerFrame, tags::RescaleIntercept, "-500.0");
        gdcm::DataSet frame0;
        insertSequenceWithItem(frame0,
                               tags::PixelValueTransformationSequence,
                               pvtPerFrame);
        perFrameItems.push_back(frame0);
    }
    {
        gdcm::DataSet frame1;  // empty — no per-frame overrides
        perFrameItems.push_back(frame1);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "precedence_override.dcm");

    // Workflow: shared first → per-frame second (replaces frame vector)
    EnhancedSeriesInfo info;
    info.frames.resize(2);
    parser_.parseSharedGroups(path, info);

    // After shared: both frames have shared rescale and orientation
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleIntercept, -1024.0);
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(info.frames[1].imageOrientation[0], 1.0);

    // Now per-frame replaces the frame vector entirely
    info.frames = parser_.parsePerFrameGroups(path, 2, info);

    // Frame 0: per-frame rescale overrides (3.0/-500.0)
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleSlope, 3.0);
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleIntercept, -500.0);
    // Frame 1: no per-frame rescale → hardcoded defaults (1.0/0.0),
    // NOT shared values (since parsePerFrameGroups creates fresh frames)
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleIntercept, 0.0);
}

TEST_F(FunctionalGroupParserTest, MixedPresencePerFrameAndShared) {
    // Tests the scenario where:
    // - Shared has orientation + pixel spacing
    // - Per-frame frame 0 has position + rescale override
    // - Per-frame frame 1 has only position (no rescale)
    // - Per-frame frame 2 has nothing (empty item)

    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "0.75\\0.75");
    gdcm::DataSet orientDs;
    insertStringElement(orientDs, tags::ImageOrientationPatient,
                        "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");
    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);
    insertSequenceWithItem(sharedGroupDs, tags::PlaneOrientationSequence,
                           orientDs);

    std::vector<gdcm::DataSet> perFrameItems;
    // Frame 0: position + rescale
    {
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "-120.0\\-120.0\\0.0");
        gdcm::DataSet pvtDs;
        insertStringElement(pvtDs, tags::RescaleSlope, "2.0");
        insertStringElement(pvtDs, tags::RescaleIntercept, "-500.0");
        gdcm::DataSet frame;
        insertSequenceWithItem(frame, tags::PlanePositionSequence, planePosDs);
        insertSequenceWithItem(frame,
                               tags::PixelValueTransformationSequence, pvtDs);
        perFrameItems.push_back(frame);
    }
    // Frame 1: position only
    {
        gdcm::DataSet planePosDs;
        insertStringElement(planePosDs, tags::ImagePositionPatient,
                            "-120.0\\-120.0\\3.0");
        gdcm::DataSet frame;
        insertSequenceWithItem(frame, tags::PlanePositionSequence, planePosDs);
        perFrameItems.push_back(frame);
    }
    // Frame 2: empty item
    {
        gdcm::DataSet frame;
        perFrameItems.push_back(frame);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "mixed_presence.dcm");

    EnhancedSeriesInfo info;
    info.frames = parser_.parsePerFrameGroups(path, 3, info);

    // Frame 0: has position and rescale from per-frame
    EXPECT_DOUBLE_EQ(info.frames[0].imagePosition[0], -120.0);
    EXPECT_DOUBLE_EQ(info.frames[0].imagePosition[2], 0.0);
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleSlope, 2.0);
    EXPECT_DOUBLE_EQ(info.frames[0].rescaleIntercept, -500.0);

    // Frame 1: has position from per-frame, default rescale
    EXPECT_DOUBLE_EQ(info.frames[1].imagePosition[2], 3.0);
    EXPECT_DOUBLE_EQ(info.frames[1].rescaleSlope, 1.0);

    // Frame 2: all defaults (empty per-frame item)
    EXPECT_DOUBLE_EQ(info.frames[2].imagePosition[0], 0.0);
    EXPECT_DOUBLE_EQ(info.frames[2].rescaleSlope, 1.0);

    // Now apply shared — pixel spacing set, orientation applied to all
    parser_.parseSharedGroups(path, info);
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.75);
    for (const auto& frame : info.frames) {
        EXPECT_DOUBLE_EQ(frame.imageOrientation[0], 1.0);
        EXPECT_DOUBLE_EQ(frame.imageOrientation[4], 1.0);
    }
}

// =============================================================================
// Edge case: very large NumberOfFrames (1000+) with synthetic DICOM
// =============================================================================

TEST_F(FunctionalGroupParserTest, LargeFrameCountSyntheticDicom) {
    const int numFrames = 1000;
    std::vector<gdcm::DataSet> perFrameItems;

    for (int i = 0; i < numFrames; ++i) {
        gdcm::DataSet planePosDs;
        std::string posStr = "0.0\\0.0\\" + std::to_string(i * 2.5);
        insertStringElement(planePosDs, tags::ImagePositionPatient, posStr);
        gdcm::DataSet frameItemDs;
        insertSequenceWithItem(frameItemDs, tags::PlanePositionSequence,
                               planePosDs);
        perFrameItems.push_back(frameItemDs);
    }

    gdcm::DataSet topDs;
    insertSequenceWithItems(topDs, tags::PerFrameFunctionalGroups,
                            perFrameItems);

    std::string path = writeDicomFile(topDs, "large_frame_count.dcm");

    EnhancedSeriesInfo sharedInfo;
    auto frames = parser_.parsePerFrameGroups(path, numFrames, sharedInfo);

    ASSERT_EQ(frames.size(), static_cast<size_t>(numFrames));
    // Verify first, middle, and last frames
    EXPECT_DOUBLE_EQ(frames[0].imagePosition[2], 0.0);
    EXPECT_NEAR(frames[499].imagePosition[2], 499 * 2.5, 0.1);
    EXPECT_NEAR(frames[999].imagePosition[2], 999 * 2.5, 0.1);
    // Verify sequential indexing
    for (int i = 0; i < numFrames; ++i) {
        EXPECT_EQ(frames[i].frameIndex, i);
    }
}

// =============================================================================
// Edge case: asymmetric pixel spacing
// =============================================================================

TEST_F(FunctionalGroupParserTest, ParseSharedAsymmetricPixelSpacing) {
    gdcm::DataSet pixelMeasuresDs;
    insertStringElement(pixelMeasuresDs, tags::PixelSpacing, "0.5\\0.75");

    gdcm::DataSet sharedGroupDs;
    insertSequenceWithItem(sharedGroupDs, tags::PixelMeasuresSequence,
                           pixelMeasuresDs);

    gdcm::DataSet topDs;
    insertSequenceWithItem(topDs, tags::SharedFunctionalGroups, sharedGroupDs);

    std::string path = writeDicomFile(topDs, "asymmetric_spacing.dcm");

    EnhancedSeriesInfo info;
    parser_.parseSharedGroups(path, info);

    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.5);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.75);
}

// =============================================================================
// DimensionOrganization struct tests (pure data structure, no I/O)
// =============================================================================

TEST(DimensionOrganizationTest, EmptyOrganization) {
    DimensionOrganization org;
    EXPECT_TRUE(org.dimensions.empty());
    EXPECT_FALSE(org.hasDimension(dimension_tag::InStackPositionNumber));
    EXPECT_FALSE(org.dimensionIndex(dimension_tag::InStackPositionNumber)
                     .has_value());
}

TEST(DimensionOrganizationTest, HasDimension) {
    DimensionOrganization org;
    org.dimensions.push_back(
        {dimension_tag::InStackPositionNumber, 0x00209111, "", ""});
    org.dimensions.push_back(
        {dimension_tag::TemporalPositionIndex, 0x00209111, "", ""});

    EXPECT_TRUE(org.hasDimension(dimension_tag::InStackPositionNumber));
    EXPECT_TRUE(org.hasDimension(dimension_tag::TemporalPositionIndex));
    EXPECT_FALSE(org.hasDimension(dimension_tag::StackID));
    EXPECT_FALSE(org.hasDimension(dimension_tag::DiffusionBValue));
}

TEST(DimensionOrganizationTest, DimensionIndex) {
    DimensionOrganization org;
    org.dimensions.push_back(
        {dimension_tag::StackID, 0x00209111, "", "Stack"});
    org.dimensions.push_back(
        {dimension_tag::InStackPositionNumber, 0x00209111, "", "Position"});
    org.dimensions.push_back(
        {dimension_tag::TemporalPositionIndex, 0x00209111, "", "Temporal"});

    auto stackIdx = org.dimensionIndex(dimension_tag::StackID);
    ASSERT_TRUE(stackIdx.has_value());
    EXPECT_EQ(stackIdx.value(), 0u);

    auto posIdx = org.dimensionIndex(dimension_tag::InStackPositionNumber);
    ASSERT_TRUE(posIdx.has_value());
    EXPECT_EQ(posIdx.value(), 1u);

    auto tempIdx = org.dimensionIndex(dimension_tag::TemporalPositionIndex);
    ASSERT_TRUE(tempIdx.has_value());
    EXPECT_EQ(tempIdx.value(), 2u);

    auto noIdx = org.dimensionIndex(dimension_tag::EchoNumber);
    EXPECT_FALSE(noIdx.has_value());
}

TEST(DimensionOrganizationTest, DimensionDefinitionFields) {
    DimensionDefinition def;
    def.dimensionIndexPointer = dimension_tag::InStackPositionNumber;
    def.functionalGroupPointer = 0x00209111;
    def.dimensionOrganizationUID = "1.2.3.4.5";
    def.dimensionDescription = "In-Stack Position";

    EXPECT_EQ(def.dimensionIndexPointer, dimension_tag::InStackPositionNumber);
    EXPECT_EQ(def.functionalGroupPointer, 0x00209111u);
    EXPECT_EQ(def.dimensionOrganizationUID, "1.2.3.4.5");
    EXPECT_EQ(def.dimensionDescription, "In-Stack Position");
}

TEST(DimensionOrganizationTest, DimensionDefinitionDefaultValues) {
    DimensionDefinition def;
    EXPECT_EQ(def.dimensionIndexPointer, 0u);
    EXPECT_EQ(def.functionalGroupPointer, 0u);
    EXPECT_TRUE(def.dimensionOrganizationUID.empty());
    EXPECT_TRUE(def.dimensionDescription.empty());
}

// =============================================================================
// EnhancedFrameInfo default value tests (complementary to existing)
// =============================================================================

TEST(EnhancedFrameInfoTest, OptionalFieldsDefault) {
    EnhancedFrameInfo frame;
    EXPECT_FALSE(frame.triggerTime.has_value());
    EXPECT_FALSE(frame.temporalPositionIndex.has_value());
    EXPECT_TRUE(frame.dimensionIndices.empty());
}

TEST(EnhancedFrameInfoTest, DimensionIndicesStorage) {
    EnhancedFrameInfo frame;
    frame.dimensionIndices[0x00209057] = 5;   // InStackPositionNumber
    frame.dimensionIndices[0x00209128] = 3;   // TemporalPositionIndex

    EXPECT_EQ(frame.dimensionIndices.size(), 2u);
    EXPECT_EQ(frame.dimensionIndices.at(0x00209057), 5);
    EXPECT_EQ(frame.dimensionIndices.at(0x00209128), 3);
}

TEST(EnhancedFrameInfoTest, TemporalFieldsAssignment) {
    EnhancedFrameInfo frame;
    frame.triggerTime = 45.5;
    frame.temporalPositionIndex = 2;

    ASSERT_TRUE(frame.triggerTime.has_value());
    EXPECT_DOUBLE_EQ(frame.triggerTime.value(), 45.5);
    ASSERT_TRUE(frame.temporalPositionIndex.has_value());
    EXPECT_EQ(frame.temporalPositionIndex.value(), 2);
}

// =============================================================================
// dimension_tag namespace constant verification
// =============================================================================

TEST(DimensionTagTest, ConstantValues) {
    EXPECT_EQ(dimension_tag::InStackPositionNumber, 0x00209057u);
    EXPECT_EQ(dimension_tag::TemporalPositionIndex, 0x00209128u);
    EXPECT_EQ(dimension_tag::StackID, 0x00209056u);
    EXPECT_EQ(dimension_tag::DiffusionBValue, 0x00189087u);
    EXPECT_EQ(dimension_tag::EchoNumber, 0x00180086u);
}

TEST(DimensionTagTest, ConstantsAreDistinct) {
    // Verify all constants are unique
    std::vector<uint32_t> allTags = {
        dimension_tag::InStackPositionNumber,
        dimension_tag::TemporalPositionIndex,
        dimension_tag::StackID,
        dimension_tag::DiffusionBValue,
        dimension_tag::EchoNumber};

    for (size_t i = 0; i < allTags.size(); ++i) {
        for (size_t j = i + 1; j < allTags.size(); ++j) {
            EXPECT_NE(allTags[i], allTags[j])
                << "Tags at index " << i << " and " << j << " are equal";
        }
    }
}

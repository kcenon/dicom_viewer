#include <gtest/gtest.h>

#include "services/enhanced_dicom/enhanced_dicom_parser.hpp"
#include "services/enhanced_dicom/enhanced_dicom_types.hpp"
#include "services/enhanced_dicom/frame_extractor.hpp"
#include "services/enhanced_dicom/functional_group_parser.hpp"

using namespace dicom_viewer::services;

// =============================================================================
// EnhancedDicomError tests
// =============================================================================

TEST(EnhancedDicomErrorTest, DefaultIsSuccess) {
    EnhancedDicomError err;
    EXPECT_TRUE(err.isSuccess());
    EXPECT_EQ(err.code, EnhancedDicomError::Code::Success);
    EXPECT_EQ(err.toString(), "Success");
}

TEST(EnhancedDicomErrorTest, ErrorCodes) {
    EnhancedDicomError invalidInput{
        EnhancedDicomError::Code::InvalidInput, "bad frame index"};
    EXPECT_FALSE(invalidInput.isSuccess());
    EXPECT_TRUE(invalidInput.toString().find("Invalid input") !=
                std::string::npos);
    EXPECT_TRUE(invalidInput.toString().find("bad frame index") !=
                std::string::npos);

    EnhancedDicomError notEnhanced{
        EnhancedDicomError::Code::NotEnhancedIOD, "1.2.840.10008.5.1.4.1.1.2"};
    EXPECT_TRUE(notEnhanced.toString().find("Not an Enhanced IOD") !=
                std::string::npos);

    EnhancedDicomError parseFailed{
        EnhancedDicomError::Code::ParseFailed, "corrupt file"};
    EXPECT_TRUE(parseFailed.toString().find("Parse failed") !=
                std::string::npos);

    EnhancedDicomError missingTag{
        EnhancedDicomError::Code::MissingTag, "(0028,0010)"};
    EXPECT_TRUE(missingTag.toString().find("Missing DICOM tag") !=
                std::string::npos);

    EnhancedDicomError unsupportedPixel{
        EnhancedDicomError::Code::UnsupportedPixelFormat, "32-bit float"};
    EXPECT_TRUE(unsupportedPixel.toString().find("Unsupported pixel format") !=
                std::string::npos);

    EnhancedDicomError frameExtract{
        EnhancedDicomError::Code::FrameExtractionFailed, "buffer overflow"};
    EXPECT_TRUE(frameExtract.toString().find("Frame extraction failed") !=
                std::string::npos);

    EnhancedDicomError inconsistent{
        EnhancedDicomError::Code::InconsistentData, "frame count mismatch"};
    EXPECT_TRUE(inconsistent.toString().find("Inconsistent data") !=
                std::string::npos);

    EnhancedDicomError internal{
        EnhancedDicomError::Code::InternalError, "null pointer"};
    EXPECT_TRUE(internal.toString().find("Internal error") !=
                std::string::npos);
}

// =============================================================================
// SOP Class UID detection tests
// =============================================================================

TEST(EnhancedSopClassTest, DetectsEnhancedCT) {
    EXPECT_TRUE(isEnhancedSopClass(
        enhanced_sop_class::EnhancedCTImageStorage));
    EXPECT_TRUE(isEnhancedSopClass("1.2.840.10008.5.1.4.1.1.2.1"));
}

TEST(EnhancedSopClassTest, DetectsEnhancedMR) {
    EXPECT_TRUE(isEnhancedSopClass(
        enhanced_sop_class::EnhancedMRImageStorage));
    EXPECT_TRUE(isEnhancedSopClass("1.2.840.10008.5.1.4.1.1.4.1"));
}

TEST(EnhancedSopClassTest, DetectsEnhancedXA) {
    EXPECT_TRUE(isEnhancedSopClass(
        enhanced_sop_class::EnhancedXAImageStorage));
    EXPECT_TRUE(isEnhancedSopClass("1.2.840.10008.5.1.4.1.1.12.1.1"));
}

TEST(EnhancedSopClassTest, RejectsClassicCT) {
    // Classic CT Image Storage
    EXPECT_FALSE(isEnhancedSopClass("1.2.840.10008.5.1.4.1.1.2"));
}

TEST(EnhancedSopClassTest, RejectsClassicMR) {
    // Classic MR Image Storage
    EXPECT_FALSE(isEnhancedSopClass("1.2.840.10008.5.1.4.1.1.4"));
}

TEST(EnhancedSopClassTest, RejectsEmptyString) {
    EXPECT_FALSE(isEnhancedSopClass(""));
}

TEST(EnhancedSopClassTest, RejectsArbitraryString) {
    EXPECT_FALSE(isEnhancedSopClass("not.a.uid"));
}

// =============================================================================
// SOP Class name resolution tests
// =============================================================================

TEST(EnhancedSopClassNameTest, ReturnsCorrectNames) {
    EXPECT_EQ(enhancedSopClassName(enhanced_sop_class::EnhancedCTImageStorage),
              "Enhanced CT Image Storage");
    EXPECT_EQ(enhancedSopClassName(enhanced_sop_class::EnhancedMRImageStorage),
              "Enhanced MR Image Storage");
    EXPECT_EQ(enhancedSopClassName(enhanced_sop_class::EnhancedXAImageStorage),
              "Enhanced XA Image Storage");
}

TEST(EnhancedSopClassNameTest, ReturnsUnknownForInvalid) {
    EXPECT_EQ(enhancedSopClassName("1.2.840.10008.5.1.4.1.1.2"), "Unknown");
    EXPECT_EQ(enhancedSopClassName(""), "Unknown");
}

// =============================================================================
// EnhancedFrameInfo default value tests
// =============================================================================

TEST(EnhancedFrameInfoTest, DefaultValues) {
    EnhancedFrameInfo frame;
    EXPECT_EQ(frame.frameIndex, 0);
    EXPECT_DOUBLE_EQ(frame.imagePosition[0], 0.0);
    EXPECT_DOUBLE_EQ(frame.imagePosition[1], 0.0);
    EXPECT_DOUBLE_EQ(frame.imagePosition[2], 0.0);
    EXPECT_DOUBLE_EQ(frame.imageOrientation[0], 1.0);
    EXPECT_DOUBLE_EQ(frame.imageOrientation[1], 0.0);
    EXPECT_DOUBLE_EQ(frame.imageOrientation[2], 0.0);
    EXPECT_DOUBLE_EQ(frame.imageOrientation[3], 0.0);
    EXPECT_DOUBLE_EQ(frame.imageOrientation[4], 1.0);
    EXPECT_DOUBLE_EQ(frame.imageOrientation[5], 0.0);
    EXPECT_DOUBLE_EQ(frame.sliceThickness, 1.0);
    EXPECT_DOUBLE_EQ(frame.rescaleSlope, 1.0);
    EXPECT_DOUBLE_EQ(frame.rescaleIntercept, 0.0);
    EXPECT_FALSE(frame.triggerTime.has_value());
    EXPECT_FALSE(frame.temporalPositionIndex.has_value());
    EXPECT_TRUE(frame.dimensionIndices.empty());
}

// =============================================================================
// EnhancedSeriesInfo default value tests
// =============================================================================

TEST(EnhancedSeriesInfoTest, DefaultValues) {
    EnhancedSeriesInfo info;
    EXPECT_TRUE(info.sopClassUid.empty());
    EXPECT_TRUE(info.sopInstanceUid.empty());
    EXPECT_EQ(info.numberOfFrames, 0);
    EXPECT_EQ(info.rows, 0);
    EXPECT_EQ(info.columns, 0);
    EXPECT_EQ(info.bitsAllocated, 0);
    EXPECT_EQ(info.bitsStored, 0);
    EXPECT_EQ(info.highBit, 0);
    EXPECT_EQ(info.pixelRepresentation, 0);
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 1.0);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 1.0);
    EXPECT_TRUE(info.frames.empty());
    EXPECT_TRUE(info.patientId.empty());
    EXPECT_TRUE(info.modality.empty());
    EXPECT_TRUE(info.transferSyntaxUid.empty());
    EXPECT_TRUE(info.filePath.empty());
}

// =============================================================================
// EnhancedDicomParser construction and static method tests
// =============================================================================

TEST(EnhancedDicomParserTest, ConstructionAndDestruction) {
    EnhancedDicomParser parser;
    // Verify no crash on construction/destruction
}

TEST(EnhancedDicomParserTest, MoveConstruction) {
    EnhancedDicomParser parser1;
    EnhancedDicomParser parser2(std::move(parser1));
    // Verify no crash on move construction
}

TEST(EnhancedDicomParserTest, MoveAssignment) {
    EnhancedDicomParser parser1;
    EnhancedDicomParser parser2;
    parser2 = std::move(parser1);
    // Verify no crash on move assignment
}

TEST(EnhancedDicomParserTest, DetectEnhancedIOD) {
    EXPECT_TRUE(EnhancedDicomParser::detectEnhancedIOD(
        "1.2.840.10008.5.1.4.1.1.2.1"));
    EXPECT_TRUE(EnhancedDicomParser::detectEnhancedIOD(
        "1.2.840.10008.5.1.4.1.1.4.1"));
    EXPECT_TRUE(EnhancedDicomParser::detectEnhancedIOD(
        "1.2.840.10008.5.1.4.1.1.12.1.1"));
    EXPECT_FALSE(EnhancedDicomParser::detectEnhancedIOD(
        "1.2.840.10008.5.1.4.1.1.2"));
    EXPECT_FALSE(EnhancedDicomParser::detectEnhancedIOD(""));
}

TEST(EnhancedDicomParserTest, IsEnhancedDicomNonexistentFile) {
    EXPECT_FALSE(EnhancedDicomParser::isEnhancedDicom(
        "/nonexistent/path/file.dcm"));
}

TEST(EnhancedDicomParserTest, ParseFileNonexistent) {
    EnhancedDicomParser parser;
    auto result = parser.parseFile("/nonexistent/path/file.dcm");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::ParseFailed);
}

TEST(EnhancedDicomParserTest, ProgressCallback) {
    EnhancedDicomParser parser;
    std::vector<double> progressValues;
    parser.setProgressCallback([&progressValues](double p) {
        progressValues.push_back(p);
    });

    // Parse nonexistent file - should still report initial progress
    auto result = parser.parseFile("/nonexistent/path/file.dcm");
    EXPECT_FALSE(result.has_value());
    // At least the initial 0.0 progress should have been reported
    EXPECT_FALSE(progressValues.empty());
    EXPECT_DOUBLE_EQ(progressValues.front(), 0.0);
}

// =============================================================================
// FrameExtractor tests
// =============================================================================

TEST(FrameExtractorTest, ConstructionAndDestruction) {
    FrameExtractor extractor;
    // Verify no crash
}

TEST(FrameExtractorTest, MoveConstruction) {
    FrameExtractor ext1;
    FrameExtractor ext2(std::move(ext1));
}

TEST(FrameExtractorTest, ExtractFrameInvalidIndex) {
    FrameExtractor extractor;
    EnhancedSeriesInfo info;
    info.numberOfFrames = 5;

    // Negative index
    auto result1 = extractor.extractFrame("/some/file.dcm", -1, info);
    EXPECT_FALSE(result1.has_value());
    EXPECT_EQ(result1.error().code, EnhancedDicomError::Code::InvalidInput);

    // Out of range index
    auto result2 = extractor.extractFrame("/some/file.dcm", 5, info);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().code, EnhancedDicomError::Code::InvalidInput);
}

TEST(FrameExtractorTest, AssembleVolumeEmptyFrames) {
    FrameExtractor extractor;
    EnhancedSeriesInfo info;
    std::vector<int> emptyIndices;

    auto result = extractor.assembleVolumeFromFrames(
        "/some/file.dcm", info, emptyIndices);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EnhancedDicomError::Code::InvalidInput);
}

// =============================================================================
// FunctionalGroupParser tests
// =============================================================================

TEST(FunctionalGroupParserTest, ConstructionAndDestruction) {
    FunctionalGroupParser parser;
    // Verify no crash
}

TEST(FunctionalGroupParserTest, MoveConstruction) {
    FunctionalGroupParser parser1;
    FunctionalGroupParser parser2(std::move(parser1));
}

TEST(FunctionalGroupParserTest, ParsePerFrameGroupsNonexistentFile) {
    FunctionalGroupParser parser;
    EnhancedSeriesInfo sharedInfo;

    auto frames = parser.parsePerFrameGroups(
        "/nonexistent/file.dcm", 10, sharedInfo);

    // Should return vector with default-initialized frames
    EXPECT_EQ(frames.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(frames[i].frameIndex, i);
        EXPECT_DOUBLE_EQ(frames[i].rescaleSlope, 1.0);
        EXPECT_DOUBLE_EQ(frames[i].rescaleIntercept, 0.0);
    }
}

TEST(FunctionalGroupParserTest, ParseSharedGroupsNonexistentFile) {
    FunctionalGroupParser parser;
    EnhancedSeriesInfo info;
    info.pixelSpacingX = 0.5;
    info.pixelSpacingY = 0.5;

    // Should not crash and should not modify info
    parser.parseSharedGroups("/nonexistent/file.dcm", info);
    EXPECT_DOUBLE_EQ(info.pixelSpacingX, 0.5);
    EXPECT_DOUBLE_EQ(info.pixelSpacingY, 0.5);
}

// =============================================================================
// SOP Class UID constant verification
// =============================================================================

TEST(SopClassConstantsTest, UidFormats) {
    // Verify UIDs follow DICOM UID format (dot-separated numeric)
    std::string enhancedCT = enhanced_sop_class::EnhancedCTImageStorage;
    std::string enhancedMR = enhanced_sop_class::EnhancedMRImageStorage;
    std::string enhancedXA = enhanced_sop_class::EnhancedXAImageStorage;

    // All should start with "1.2.840.10008"
    EXPECT_EQ(enhancedCT.substr(0, 13), "1.2.840.10008");
    EXPECT_EQ(enhancedMR.substr(0, 13), "1.2.840.10008");
    EXPECT_EQ(enhancedXA.substr(0, 13), "1.2.840.10008");

    // Enhanced CT ends with .2.1 (vs Classic CT .2)
    EXPECT_TRUE(enhancedCT.find(".2.1") != std::string::npos);
    // Enhanced MR ends with .4.1 (vs Classic MR .4)
    EXPECT_TRUE(enhancedMR.find(".4.1") != std::string::npos);
}

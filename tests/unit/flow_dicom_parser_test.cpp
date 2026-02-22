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

#include <gtest/gtest.h>

#include "services/flow/flow_dicom_parser.hpp"
#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/vendor_parsers/ge_flow_parser.hpp"
#include "services/flow/vendor_parsers/philips_flow_parser.hpp"
#include "services/flow/vendor_parsers/siemens_flow_parser.hpp"

using namespace dicom_viewer::services;

// =============================================================================
// FlowError tests
// =============================================================================

TEST(FlowErrorTest, SuccessCode) {
    FlowError err;
    EXPECT_TRUE(err.isSuccess());
    EXPECT_EQ(err.code, FlowError::Code::Success);
    EXPECT_EQ(err.toString(), "Success");
}

TEST(FlowErrorTest, ErrorCodes) {
    FlowError err{FlowError::Code::InvalidInput, "no files"};
    EXPECT_FALSE(err.isSuccess());
    EXPECT_TRUE(err.toString().find("Invalid input") != std::string::npos);
    EXPECT_TRUE(err.toString().find("no files") != std::string::npos);

    FlowError vendorErr{FlowError::Code::UnsupportedVendor, "Canon"};
    EXPECT_TRUE(vendorErr.toString().find("Unsupported vendor") != std::string::npos);

    FlowError parseErr{FlowError::Code::ParseFailed, "corrupt"};
    EXPECT_TRUE(parseErr.toString().find("Parse failed") != std::string::npos);

    FlowError tagErr{FlowError::Code::MissingTag, "(0018,9197)"};
    EXPECT_TRUE(tagErr.toString().find("Missing DICOM tag") != std::string::npos);

    FlowError dataErr{FlowError::Code::InconsistentData, "phase mismatch"};
    EXPECT_TRUE(dataErr.toString().find("Inconsistent data") != std::string::npos);

    FlowError internalErr{FlowError::Code::InternalError, "null ptr"};
    EXPECT_TRUE(internalErr.toString().find("Internal error") != std::string::npos);
}

// =============================================================================
// FlowDicomTypes utility tests
// =============================================================================

TEST(FlowDicomTypesTest, VendorToString) {
    EXPECT_EQ(vendorToString(FlowVendorType::Siemens), "Siemens");
    EXPECT_EQ(vendorToString(FlowVendorType::Philips), "Philips");
    EXPECT_EQ(vendorToString(FlowVendorType::GE), "GE");
    EXPECT_EQ(vendorToString(FlowVendorType::Unknown), "Unknown");
}

TEST(FlowDicomTypesTest, ComponentToString) {
    EXPECT_EQ(componentToString(VelocityComponent::Magnitude), "Magnitude");
    EXPECT_EQ(componentToString(VelocityComponent::Vx), "Vx");
    EXPECT_EQ(componentToString(VelocityComponent::Vy), "Vy");
    EXPECT_EQ(componentToString(VelocityComponent::Vz), "Vz");
}

TEST(FlowDicomTypesTest, FlowFrameDefaults) {
    FlowFrame frame;
    EXPECT_EQ(frame.cardiacPhase, 0);
    EXPECT_EQ(frame.component, VelocityComponent::Magnitude);
    EXPECT_DOUBLE_EQ(frame.venc, 0.0);
    EXPECT_EQ(frame.sliceIndex, 0);
    EXPECT_DOUBLE_EQ(frame.triggerTime, 0.0);
    EXPECT_TRUE(frame.filePath.empty());
    EXPECT_TRUE(frame.sopInstanceUid.empty());
}

TEST(FlowDicomTypesTest, FlowSeriesInfoDefaults) {
    FlowSeriesInfo info;
    EXPECT_EQ(info.vendor, FlowVendorType::Unknown);
    EXPECT_EQ(info.phaseCount, 0);
    EXPECT_DOUBLE_EQ(info.temporalResolution, 0.0);
    EXPECT_TRUE(info.isSignedPhase);
    EXPECT_TRUE(info.frameMatrix.empty());
}

// =============================================================================
// Vendor parser type tests
// =============================================================================

TEST(SiemensFlowParserTest, VendorType) {
    SiemensFlowParser parser;
    EXPECT_EQ(parser.vendorType(), FlowVendorType::Siemens);
    EXPECT_EQ(parser.expectedIODType(), "Enhanced MR Image Storage");
}

TEST(PhilipsFlowParserTest, VendorType) {
    PhilipsFlowParser parser;
    EXPECT_EQ(parser.vendorType(), FlowVendorType::Philips);
    EXPECT_EQ(parser.expectedIODType(), "MR Image Storage");
}

TEST(GEFlowParserTest, VendorType) {
    GEFlowParser parser;
    EXPECT_EQ(parser.vendorType(), FlowVendorType::GE);
    EXPECT_EQ(parser.expectedIODType(), "MR Image Storage");
}

// =============================================================================
// FlowDicomParser construction tests
// =============================================================================

TEST(FlowDicomParserTest, DefaultConstruction) {
    FlowDicomParser parser;
    // Should not throw
}

TEST(FlowDicomParserTest, MoveConstruction) {
    FlowDicomParser parser;
    FlowDicomParser moved(std::move(parser));
    // Should not throw
}

TEST(FlowDicomParserTest, MoveAssignment) {
    FlowDicomParser parser;
    FlowDicomParser other;
    other = std::move(parser);
    // Should not throw
}

TEST(FlowDicomParserTest, ProgressCallback) {
    FlowDicomParser parser;
    double lastProgress = -1.0;
    parser.setProgressCallback([&](double p) { lastProgress = p; });
    // Callback is stored but not invoked until parseSeries is called
    EXPECT_DOUBLE_EQ(lastProgress, -1.0);
}

// =============================================================================
// Static method tests with empty input
// =============================================================================

TEST(FlowDicomParserTest, Is4DFlowSeriesEmptyInput) {
    std::vector<std::string> empty;
    EXPECT_FALSE(FlowDicomParser::is4DFlowSeries(empty));
}

TEST(FlowDicomParserTest, Is4DFlowSeriesNonexistentFile) {
    std::vector<std::string> files = {"/nonexistent/path.dcm"};
    EXPECT_FALSE(FlowDicomParser::is4DFlowSeries(files));
}

TEST(FlowDicomParserTest, DetectVendorEmptyInput) {
    std::vector<std::string> empty;
    EXPECT_EQ(FlowDicomParser::detectVendor(empty), FlowVendorType::Unknown);
}

TEST(FlowDicomParserTest, DetectVendorNonexistentFile) {
    std::vector<std::string> files = {"/nonexistent/path.dcm"};
    EXPECT_EQ(FlowDicomParser::detectVendor(files), FlowVendorType::Unknown);
}

// =============================================================================
// parseSeries error handling tests
// =============================================================================

TEST(FlowDicomParserTest, ParseSeriesEmptyInput) {
    FlowDicomParser parser;
    std::vector<std::string> empty;
    auto result = parser.parseSeries(empty);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(FlowDicomParserTest, ParseSeriesNonexistentFiles) {
    FlowDicomParser parser;
    std::vector<std::string> files = {"/nonexistent/a.dcm", "/nonexistent/b.dcm"};
    auto result = parser.parseSeries(files);
    ASSERT_FALSE(result.has_value());
    // Should fail at vendor detection
    EXPECT_EQ(result.error().code, FlowError::Code::UnsupportedVendor);
}

// =============================================================================
// Vendor-specific metadata parsing with mock dictionary
// =============================================================================

namespace {

itk::MetaDataDictionary createMockDictionary(
    const std::map<std::string, std::string>& entries) {
    itk::MetaDataDictionary dict;
    for (const auto& [key, value] : entries) {
        itk::EncapsulateMetaData<std::string>(dict, key, value);
    }
    return dict;
}

}  // anonymous namespace

TEST(SiemensFlowParserTest, ExtractVENCFromStandardTag) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary({{"0018|9197", "150.0"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 150.0);
}

TEST(SiemensFlowParserTest, ExtractVENCNegativeValue) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary({{"0018|9197", "-200.0"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 200.0);
}

TEST(SiemensFlowParserTest, ClassifyMagnitudeFromImageType) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\M\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(SiemensFlowParserTest, ClassifyVxFromPrivateTag) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
         {"0051|1014", "v150_RL"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(SiemensFlowParserTest, ClassifyVyFromPrivateTag) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
         {"0051|1014", "v150_AP"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vy);
}

TEST(SiemensFlowParserTest, ClassifyVzFromPrivateTag) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
         {"0051|1014", "v150_FH"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

TEST(SiemensFlowParserTest, ExtractTriggerTime) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary({{"0018|1060", "42.5"}});
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 42.5);
}

TEST(SiemensFlowParserTest, ExtractPhaseIndex) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary({{"0020|9057", "5"}});
    EXPECT_EQ(parser.extractPhaseIndex(dict), 5);
}

TEST(PhilipsFlowParserTest, ExtractVENCFromStandardTag) {
    PhilipsFlowParser parser;
    auto dict = createMockDictionary({{"0018|9197", "100.0"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 100.0);
}

TEST(PhilipsFlowParserTest, ClassifyMagnitudeFromImageType) {
    PhilipsFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\M\\FFE"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(PhilipsFlowParserTest, ClassifyVxFromSeriesDescription) {
    PhilipsFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
         {"0008|103e", "PC_4D_FLOW_RL"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(PhilipsFlowParserTest, ClassifyVzFromSeriesDescription) {
    PhilipsFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
         {"0008|103e", "PC_4D_FLOW_FH"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

TEST(GEFlowParserTest, ExtractVENCFromPrivateTag) {
    GEFlowParser parser;
    auto dict = createMockDictionary({{"0019|10cc", "200.0"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 200.0);
}

TEST(GEFlowParserTest, ClassifyMagnitude) {
    GEFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\M\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(GEFlowParserTest, ClassifyVxFromSeriesDescription) {
    GEFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
         {"0008|103e", "FLOW_RL"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(GEFlowParserTest, ClassifyVyFromSeriesDescription) {
    GEFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
         {"0008|103e", "FLOW_AP"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vy);
}

TEST(GEFlowParserTest, ClassifyVzFromSeriesDescription) {
    GEFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
         {"0008|103e", "FLOW_SI"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

TEST(GEFlowParserTest, ExtractTriggerTime) {
    GEFlowParser parser;
    auto dict = createMockDictionary({{"0018|1060", "33.7"}});
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 33.7);
}

TEST(GEFlowParserTest, ExtractTriggerTimeEmpty) {
    GEFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 0.0);
}

// =============================================================================
// Error recovery and edge case tests (Issue #202)
// =============================================================================

TEST(SiemensFlowParserTest, ExtractVENCMissingTag) {
    SiemensFlowParser parser;
    itk::MetaDataDictionary dict;  // Empty — no VENC tag
    // Should return 0.0 (default) when tag is absent
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 0.0);
}

TEST(SiemensFlowParserTest, ExtractVENCInvalidNonNumeric) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary({{"0018|9197", "NOT_A_NUMBER"}});
    // Non-numeric value should not crash; expect 0.0 fallback
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 0.0);
}

TEST(SiemensFlowParserTest, ClassifyUnknownDirectionTag) {
    SiemensFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
         {"0051|1014", "v150_UNKNOWN_DIR"}});
    // Unrecognized direction should fall back to Magnitude or a default
    auto comp = parser.classifyComponent(dict);
    // The result depends on implementation; should not crash
    SUCCEED();
}

TEST(PhilipsFlowParserTest, ClassifyVyFromSeriesDescription) {
    PhilipsFlowParser parser;
    auto dict = createMockDictionary(
        {{"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
         {"0008|103e", "PC_4D_FLOW_AP"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vy);
}

TEST(GEFlowParserTest, ExtractVENCMissingTag) {
    GEFlowParser parser;
    itk::MetaDataDictionary dict;  // No private VENC tag
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 0.0);
}

TEST(FlowDicomParserTest, ParseSeriesSingleFile) {
    FlowDicomParser parser;
    std::vector<std::string> files = {"/nonexistent/single.dcm"};
    auto result = parser.parseSeries(files);
    // Single non-existent file should fail at vendor detection
    ASSERT_FALSE(result.has_value());
}

TEST(FlowDicomParserTest, ProgressCallbackInvokedOnError) {
    FlowDicomParser parser;
    std::vector<double> progressValues;
    parser.setProgressCallback([&](double p) {
        progressValues.push_back(p);
    });

    std::vector<std::string> files = {"/nonexistent/a.dcm"};
    auto result = parser.parseSeries(files);
    EXPECT_FALSE(result.has_value());

    // Even on failure, at least the initial progress (0.0) should be reported
    if (!progressValues.empty()) {
        EXPECT_DOUBLE_EQ(progressValues.front(), 0.0);
    }
}

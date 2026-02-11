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

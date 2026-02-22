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

#include <itkMetaDataObject.h>

#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/vendor_parsers/ge_flow_parser.hpp"
#include "services/flow/vendor_parsers/philips_flow_parser.hpp"
#include "services/flow/vendor_parsers/siemens_flow_parser.hpp"

using namespace dicom_viewer::services;

namespace {

itk::MetaDataDictionary makeDictionary(
    const std::map<std::string, std::string>& entries) {
    itk::MetaDataDictionary dict;
    for (const auto& [key, value] : entries) {
        itk::EncapsulateMetaData<std::string>(dict, key, value);
    }
    return dict;
}

}  // anonymous namespace

// =============================================================================
// Siemens: VENC extraction fallback and edge cases
// =============================================================================

TEST(SiemensVendorParserTest, ExtractVENCFromPrivateTag) {
    SiemensFlowParser parser;
    // Siemens private tag (0051,1014) encodes as "v{VENC}cm/s"
    auto dict = makeDictionary({{"0051|1014", "v150cm/s"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 150.0);
}

TEST(SiemensVendorParserTest, ExtractVENCPrivateTagWithoutUnit) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0051|1014", "v200"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 200.0);
}

TEST(SiemensVendorParserTest, ExtractVENCStandardTagPrioritized) {
    SiemensFlowParser parser;
    // Standard tag should be checked first
    auto dict = makeDictionary({
        {"0018|9197", "100.0"},
        {"0051|1014", "v200cm/s"}
    });
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 100.0);
}

TEST(SiemensVendorParserTest, ExtractVENCEmptyDictionary) {
    SiemensFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 0.0);
}

TEST(SiemensVendorParserTest, ExtractVENCWhitespaceValue) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0018|9197", "  150.0  "}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 150.0);
}

// =============================================================================
// Siemens: component classification alternative patterns
// =============================================================================

TEST(SiemensVendorParserTest, ClassifyMagnitudeFromMAG) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PRIMARY\\MAG\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(SiemensVendorParserTest, ClassifyMagnitudeFromM_Underscore) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PRIMARY\\M_FFE"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(SiemensVendorParserTest, ClassifyVxFromAP_RL) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
        {"0051|1014", "v150_AP_RL"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(SiemensVendorParserTest, ClassifyVzFromSI) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
        {"0051|1014", "v150_SI"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

TEST(SiemensVendorParserTest, ClassifyFallbackPhaseImageNoDirection) {
    SiemensFlowParser parser;
    // Phase image with no direction info in private tag → Vx fallback
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(SiemensVendorParserTest, ClassifyFallbackVELOCITY) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\VELOCITY\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(SiemensVendorParserTest, ClassifyFallbackPHASE) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PHASE\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(SiemensVendorParserTest, ClassifyEmptyDictionaryDefaultsMagnitude) {
    SiemensFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(SiemensVendorParserTest, ClassifyCaseInsensitive) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "original\\primary\\m\\nd"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

// =============================================================================
// Siemens: phase index fallback
// =============================================================================

TEST(SiemensVendorParserTest, ExtractPhaseIndexFallbackInstanceNumber) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0020|0013", "12"}});
    EXPECT_EQ(parser.extractPhaseIndex(dict), 12);
}

TEST(SiemensVendorParserTest, ExtractPhaseIndexStackPositionPriority) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({
        {"0020|9057", "3"},
        {"0020|0013", "99"}
    });
    // Stack position should take priority
    EXPECT_EQ(parser.extractPhaseIndex(dict), 3);
}

TEST(SiemensVendorParserTest, ExtractPhaseIndexEmptyDictionary) {
    SiemensFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_EQ(parser.extractPhaseIndex(dict), 0);
}

TEST(SiemensVendorParserTest, ExtractPhaseIndexInvalidValue) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0020|9057", "abc"}});
    EXPECT_EQ(parser.extractPhaseIndex(dict), 0);
}

// =============================================================================
// Siemens: trigger time fallback
// =============================================================================

TEST(SiemensVendorParserTest, ExtractTriggerTimeFallbackNominal) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({{"0020|9153", "75.3"}});
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 75.3);
}

TEST(SiemensVendorParserTest, ExtractTriggerTimePrimaryPriority) {
    SiemensFlowParser parser;
    auto dict = makeDictionary({
        {"0018|1060", "42.5"},
        {"0020|9153", "75.3"}
    });
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 42.5);
}

TEST(SiemensVendorParserTest, ExtractTriggerTimeEmptyDictionary) {
    SiemensFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 0.0);
}

// =============================================================================
// Philips: VENC extraction
// =============================================================================

TEST(PhilipsVendorParserTest, ExtractVENCFromPrivateTag) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"2001|101a", "120.5"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 120.5);
}

TEST(PhilipsVendorParserTest, ExtractVENCPrivateTagNegative) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"2001|101a", "-180.0"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 180.0);
}

TEST(PhilipsVendorParserTest, ExtractVENCStandardTagPrioritized) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0018|9197", "100.0"},
        {"2001|101a", "200.0"}
    });
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 100.0);
}

TEST(PhilipsVendorParserTest, ExtractVENCEmptyDictionary) {
    PhilipsFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 0.0);
}

// =============================================================================
// Philips: component classification alternative patterns
// =============================================================================

TEST(PhilipsVendorParserTest, ClassifyMagnitudeFromFFE_M) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PRIMARY\\FFE_M\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(PhilipsVendorParserTest, ClassifyVxFromLR) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
        {"0008|103e", "PC_4D_FLOW_LR"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(PhilipsVendorParserTest, ClassifyVxFromVX) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
        {"0008|103e", "4DFLOW_VX"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(PhilipsVendorParserTest, ClassifyVyFromPA) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
        {"0008|103e", "PC_4D_FLOW_PA"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vy);
}

TEST(PhilipsVendorParserTest, ClassifyVyFromVY) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
        {"0008|103e", "4DFLOW_VY"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vy);
}

TEST(PhilipsVendorParserTest, ClassifyVzFromHF) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
        {"0008|103e", "PC_4D_FLOW_HF"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

TEST(PhilipsVendorParserTest, ClassifyVzFromVZ) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
        {"0008|103e", "4DFLOW_VZ"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

TEST(PhilipsVendorParserTest, ClassifyFallbackPhaseNoDirection) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\FFE"},
        {"0008|103e", "PC_4D_FLOW"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(PhilipsVendorParserTest, ClassifyFallbackPHASE) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PHASE\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(PhilipsVendorParserTest, ClassifyEmptyDictionaryDefaultsMagnitude) {
    PhilipsFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(PhilipsVendorParserTest, ClassifyCaseInsensitive) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "original\\primary\\m\\ffe"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

// =============================================================================
// Philips: phase index and trigger time
// =============================================================================

TEST(PhilipsVendorParserTest, ExtractPhaseIndex) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"0020|0013", "7"}});
    EXPECT_EQ(parser.extractPhaseIndex(dict), 7);
}

TEST(PhilipsVendorParserTest, ExtractPhaseIndexEmptyDictionary) {
    PhilipsFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_EQ(parser.extractPhaseIndex(dict), 0);
}

TEST(PhilipsVendorParserTest, ExtractPhaseIndexInvalidValue) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"0020|0013", "not_a_number"}});
    EXPECT_EQ(parser.extractPhaseIndex(dict), 0);
}

TEST(PhilipsVendorParserTest, ExtractTriggerTime) {
    PhilipsFlowParser parser;
    auto dict = makeDictionary({{"0018|1060", "55.8"}});
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 55.8);
}

TEST(PhilipsVendorParserTest, ExtractTriggerTimeEmptyDictionary) {
    PhilipsFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 0.0);
}

// =============================================================================
// GE: VENC extraction fallback
// =============================================================================

TEST(GEVendorParserTest, ExtractVENCFallbackStandardTag) {
    GEFlowParser parser;
    auto dict = makeDictionary({{"0018|9197", "150.0"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 150.0);
}

TEST(GEVendorParserTest, ExtractVENCPrivateTagPrioritized) {
    GEFlowParser parser;
    auto dict = makeDictionary({
        {"0019|10cc", "200.0"},
        {"0018|9197", "100.0"}
    });
    // GE private tag should be checked first
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 200.0);
}

TEST(GEVendorParserTest, ExtractVENCNegativeValue) {
    GEFlowParser parser;
    auto dict = makeDictionary({{"0019|10cc", "-250.0"}});
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 250.0);
}

TEST(GEVendorParserTest, ExtractVENCEmptyDictionary) {
    GEFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_DOUBLE_EQ(parser.extractVENC(dict), 0.0);
}

// =============================================================================
// GE: component classification alternative patterns
// =============================================================================

TEST(GEVendorParserTest, ClassifyVxFromUnderscoreX) {
    GEFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
        {"0008|103e", "4DFLOW_X"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(GEVendorParserTest, ClassifyVyFromUnderscoreY) {
    GEFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
        {"0008|103e", "4DFLOW_Y"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vy);
}

TEST(GEVendorParserTest, ClassifyVzFromUnderscoreZ) {
    GEFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\P\\ND"},
        {"0008|103e", "4DFLOW_Z"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

TEST(GEVendorParserTest, ClassifyFromPrivateTagDirectionUnknown) {
    GEFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "ORIGINAL\\PRIMARY\\OTHER\\ND"},
        {"0008|103e", "SOME_SERIES"},
        {"0019|10cc", "150"}
    });
    // Phase image with private tag but no recognizable direction → Vx
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(GEVendorParserTest, ClassifyFallbackPHASE) {
    GEFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PHASE\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(GEVendorParserTest, ClassifyFallbackVELOCITY) {
    GEFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\VELOCITY\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vx);
}

TEST(GEVendorParserTest, ClassifyEmptyDictionaryDefaultsMagnitude) {
    GEFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(GEVendorParserTest, ClassifyMagnitudeFromMAG) {
    GEFlowParser parser;
    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PRIMARY\\MAG\\ND"}});
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Magnitude);
}

TEST(GEVendorParserTest, ClassifyCaseInsensitive) {
    GEFlowParser parser;
    auto dict = makeDictionary({
        {"0008|0008", "original\\primary\\p\\nd"},
        {"0008|103e", "flow_si"}
    });
    EXPECT_EQ(parser.classifyComponent(dict), VelocityComponent::Vz);
}

// =============================================================================
// GE: phase index and trigger time
// =============================================================================

TEST(GEVendorParserTest, ExtractPhaseIndex) {
    GEFlowParser parser;
    auto dict = makeDictionary({{"0020|0013", "15"}});
    EXPECT_EQ(parser.extractPhaseIndex(dict), 15);
}

TEST(GEVendorParserTest, ExtractPhaseIndexEmptyDictionary) {
    GEFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_EQ(parser.extractPhaseIndex(dict), 0);
}

TEST(GEVendorParserTest, ExtractTriggerTimeEmptyDictionary) {
    GEFlowParser parser;
    itk::MetaDataDictionary dict;
    EXPECT_DOUBLE_EQ(parser.extractTriggerTime(dict), 0.0);
}

// =============================================================================
// Cross-vendor: polymorphic behavior through interface
// =============================================================================

TEST(VendorFlowParserInterfaceTest, PolymorphicAccess) {
    std::vector<std::unique_ptr<IVendorFlowParser>> parsers;
    parsers.push_back(std::make_unique<SiemensFlowParser>());
    parsers.push_back(std::make_unique<PhilipsFlowParser>());
    parsers.push_back(std::make_unique<GEFlowParser>());

    EXPECT_EQ(parsers[0]->vendorType(), FlowVendorType::Siemens);
    EXPECT_EQ(parsers[1]->vendorType(), FlowVendorType::Philips);
    EXPECT_EQ(parsers[2]->vendorType(), FlowVendorType::GE);
}

TEST(VendorFlowParserInterfaceTest, AllParsersHandleEmptyDictionary) {
    SiemensFlowParser siemens;
    PhilipsFlowParser philips;
    GEFlowParser ge;

    itk::MetaDataDictionary empty;

    // All should return safe defaults for empty metadata
    EXPECT_DOUBLE_EQ(siemens.extractVENC(empty), 0.0);
    EXPECT_DOUBLE_EQ(philips.extractVENC(empty), 0.0);
    EXPECT_DOUBLE_EQ(ge.extractVENC(empty), 0.0);

    EXPECT_EQ(siemens.classifyComponent(empty), VelocityComponent::Magnitude);
    EXPECT_EQ(philips.classifyComponent(empty), VelocityComponent::Magnitude);
    EXPECT_EQ(ge.classifyComponent(empty), VelocityComponent::Magnitude);

    EXPECT_EQ(siemens.extractPhaseIndex(empty), 0);
    EXPECT_EQ(philips.extractPhaseIndex(empty), 0);
    EXPECT_EQ(ge.extractPhaseIndex(empty), 0);

    EXPECT_DOUBLE_EQ(siemens.extractTriggerTime(empty), 0.0);
    EXPECT_DOUBLE_EQ(philips.extractTriggerTime(empty), 0.0);
    EXPECT_DOUBLE_EQ(ge.extractTriggerTime(empty), 0.0);
}

TEST(VendorFlowParserInterfaceTest, AllParsersExpectedIODTypes) {
    SiemensFlowParser siemens;
    PhilipsFlowParser philips;
    GEFlowParser ge;

    // Siemens uses Enhanced MR, others use Classic MR
    EXPECT_EQ(siemens.expectedIODType(), "Enhanced MR Image Storage");
    EXPECT_EQ(philips.expectedIODType(), "MR Image Storage");
    EXPECT_EQ(ge.expectedIODType(), "MR Image Storage");
}

TEST(VendorFlowParserInterfaceTest, CommonTriggerTimeTag) {
    // All vendors share the standard trigger time tag (0018,1060)
    SiemensFlowParser siemens;
    PhilipsFlowParser philips;
    GEFlowParser ge;

    auto dict = makeDictionary({{"0018|1060", "100.5"}});

    EXPECT_DOUBLE_EQ(siemens.extractTriggerTime(dict), 100.5);
    EXPECT_DOUBLE_EQ(philips.extractTriggerTime(dict), 100.5);
    EXPECT_DOUBLE_EQ(ge.extractTriggerTime(dict), 100.5);
}

TEST(VendorFlowParserInterfaceTest, CommonMagnitudeDetection) {
    // All vendors should detect \M\ in Image Type as Magnitude
    SiemensFlowParser siemens;
    PhilipsFlowParser philips;
    GEFlowParser ge;

    auto dict = makeDictionary({{"0008|0008", "ORIGINAL\\PRIMARY\\M\\ND"}});

    EXPECT_EQ(siemens.classifyComponent(dict), VelocityComponent::Magnitude);
    EXPECT_EQ(philips.classifyComponent(dict), VelocityComponent::Magnitude);
    EXPECT_EQ(ge.classifyComponent(dict), VelocityComponent::Magnitude);
}

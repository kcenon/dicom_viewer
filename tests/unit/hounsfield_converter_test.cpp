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
#include <cmath>

#include "core/hounsfield_converter.hpp"

using namespace dicom_viewer::core;

class HounsfieldConverterTest : public ::testing::Test {
protected:
    static constexpr double kTolerance = 0.001;
};

// Test basic HU conversion formula
TEST_F(HounsfieldConverterTest, ConvertWithDefaultParameters) {
    // Default: slope=1, intercept=0
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(0, 1.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(100, 1.0, 0.0), 100.0);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(-1000, 1.0, 0.0), -1000.0);
}

TEST_F(HounsfieldConverterTest, ConvertWithCustomSlope) {
    // StoredValue * 2 + 0 = HU
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(50, 2.0, 0.0), 100.0);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(-500, 2.0, 0.0), -1000.0);
}

TEST_F(HounsfieldConverterTest, ConvertWithCustomIntercept) {
    // StoredValue * 1 + (-1024) = HU
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(0, 1.0, -1024.0), -1024.0);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(1024, 1.0, -1024.0), 0.0);
}

TEST_F(HounsfieldConverterTest, ConvertWithBothParameters) {
    // StoredValue * 0.5 + (-500) = HU
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(1000, 0.5, -500.0), 0.0);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(0, 0.5, -500.0), -500.0);
}

TEST_F(HounsfieldConverterTest, ConvertUsingRescaleParameters) {
    HounsfieldConverter::RescaleParameters params{2.0, -1024.0};
    EXPECT_DOUBLE_EQ(HounsfieldConverter::convert(512, params), 0.0);
}

// Test inverse conversion
TEST_F(HounsfieldConverterTest, ConvertToStoredValueIsInverse) {
    double slope = 0.5;
    double intercept = -500.0;

    int stored = 1000;
    double hu = HounsfieldConverter::convert(stored, slope, intercept);
    int recovered = HounsfieldConverter::convertToStoredValue(hu, slope, intercept);

    EXPECT_EQ(stored, recovered);
}

TEST_F(HounsfieldConverterTest, ConvertToStoredValueHandlesZeroSlope) {
    int result = HounsfieldConverter::convertToStoredValue(100.0, 0.0, 0.0);
    EXPECT_EQ(result, 0);
}

// Test clamping
TEST_F(HounsfieldConverterTest, ClampHUReturnsValueInRange) {
    EXPECT_DOUBLE_EQ(HounsfieldConverter::clampHU(0.0), 0.0);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::clampHU(-1024.0), -1024.0);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::clampHU(3071.0), 3071.0);
}

TEST_F(HounsfieldConverterTest, ClampHUClampsOutOfRangeValues) {
    EXPECT_DOUBLE_EQ(HounsfieldConverter::clampHU(-2000.0), hounsfield::MinHU);
    EXPECT_DOUBLE_EQ(HounsfieldConverter::clampHU(5000.0), hounsfield::MaxHU);
}

// Test parameter validation
TEST_F(HounsfieldConverterTest, ValidateParametersAcceptsValidInputs) {
    EXPECT_TRUE(HounsfieldConverter::validateParameters(1.0, 0.0));
    EXPECT_TRUE(HounsfieldConverter::validateParameters(0.5, -1024.0));
    EXPECT_TRUE(HounsfieldConverter::validateParameters(-1.0, 100.0));
}

TEST_F(HounsfieldConverterTest, ValidateParametersRejectsZeroSlope) {
    EXPECT_FALSE(HounsfieldConverter::validateParameters(0.0, 0.0));
}

TEST_F(HounsfieldConverterTest, ValidateParametersRejectsNaN) {
    EXPECT_FALSE(HounsfieldConverter::validateParameters(std::nan(""), 0.0));
    EXPECT_FALSE(HounsfieldConverter::validateParameters(1.0, std::nan("")));
}

TEST_F(HounsfieldConverterTest, ValidateParametersRejectsInfinity) {
    EXPECT_FALSE(HounsfieldConverter::validateParameters(
        std::numeric_limits<double>::infinity(), 0.0));
    EXPECT_FALSE(HounsfieldConverter::validateParameters(
        1.0, std::numeric_limits<double>::infinity()));
}

// Test RescaleParameters
TEST_F(HounsfieldConverterTest, RescaleParametersDefaultsAreValid) {
    auto params = HounsfieldConverter::getDefaultParameters();
    EXPECT_DOUBLE_EQ(params.slope, hounsfield::DefaultSlope);
    EXPECT_DOUBLE_EQ(params.intercept, hounsfield::DefaultIntercept);
    EXPECT_TRUE(params.isValid());
}

TEST_F(HounsfieldConverterTest, RescaleParametersIsValidReturnsFalseForZeroSlope) {
    HounsfieldConverter::RescaleParameters params{0.0, 0.0};
    EXPECT_FALSE(params.isValid());
}

// Test tissue HU validation
TEST_F(HounsfieldConverterTest, IsValidHUReturnsTrueForValidRange) {
    EXPECT_TRUE(hounsfield::isValidHU(-1024.0));
    EXPECT_TRUE(hounsfield::isValidHU(0.0));
    EXPECT_TRUE(hounsfield::isValidHU(3071.0));
}

TEST_F(HounsfieldConverterTest, IsValidHUReturnsFalseForOutOfRange) {
    EXPECT_FALSE(hounsfield::isValidHU(-2000.0));
    EXPECT_FALSE(hounsfield::isValidHU(5000.0));
}

// Test tissue range detection
TEST_F(HounsfieldConverterTest, IsInTissueRangeDetectsLung) {
    EXPECT_TRUE(hounsfield::isInTissueRange(-700.0, hounsfield::Lung));
    EXPECT_FALSE(hounsfield::isInTissueRange(0.0, hounsfield::Lung));
}

TEST_F(HounsfieldConverterTest, IsInTissueRangeDetectsBone) {
    EXPECT_TRUE(hounsfield::isInTissueRange(1000.0, hounsfield::CorticalBone));
    EXPECT_FALSE(hounsfield::isInTissueRange(0.0, hounsfield::CorticalBone));
}

// Test tissue type identification
TEST_F(HounsfieldConverterTest, GetTissueTypeNameReturnsAir) {
    std::string tissue = hounsfield::getTissueTypeName(-1000.0);
    EXPECT_EQ(tissue, "Air");
}

TEST_F(HounsfieldConverterTest, GetTissueTypeNameReturnsWater) {
    std::string tissue = hounsfield::getTissueTypeName(0.0);
    EXPECT_EQ(tissue, "Water");
}

TEST_F(HounsfieldConverterTest, GetTissueTypeNameReturnsLung) {
    std::string tissue = hounsfield::getTissueTypeName(-700.0);
    EXPECT_EQ(tissue, "Lung");
}

TEST_F(HounsfieldConverterTest, GetTissueTypeNameReturnsFat) {
    std::string tissue = hounsfield::getTissueTypeName(-80.0);
    EXPECT_EQ(tissue, "Fat");
}

TEST_F(HounsfieldConverterTest, GetTissueTypeNameReturnsSoftTissue) {
    std::string tissue = hounsfield::getTissueTypeName(65.0);
    EXPECT_EQ(tissue, "Soft Tissue");
}

TEST_F(HounsfieldConverterTest, GetTissueTypeNameReturnsCorticalBone) {
    std::string tissue = hounsfield::getTissueTypeName(1000.0);
    EXPECT_EQ(tissue, "Cortical Bone");
}

// Test reference HU values
TEST_F(HounsfieldConverterTest, ReferenceValuesAreCorrect) {
    EXPECT_DOUBLE_EQ(hounsfield::Air, -1000.0);
    EXPECT_DOUBLE_EQ(hounsfield::Water, 0.0);
}

TEST_F(HounsfieldConverterTest, TissueRangesAreConsistent) {
    // Fat range
    EXPECT_LT(hounsfield::Fat.min, hounsfield::Fat.max);
    EXPECT_EQ(hounsfield::Fat.min, -100.0);
    EXPECT_EQ(hounsfield::Fat.max, -50.0);

    // Lung range
    EXPECT_LT(hounsfield::Lung.min, hounsfield::Lung.max);
    EXPECT_EQ(hounsfield::Lung.min, -900.0);
    EXPECT_EQ(hounsfield::Lung.max, -500.0);

    // Soft tissue range
    EXPECT_LT(hounsfield::SoftTissue.min, hounsfield::SoftTissue.max);
    EXPECT_EQ(hounsfield::SoftTissue.min, 10.0);
    EXPECT_EQ(hounsfield::SoftTissue.max, 80.0);

    // Cortical bone range
    EXPECT_LT(hounsfield::CorticalBone.min, hounsfield::CorticalBone.max);
    EXPECT_EQ(hounsfield::CorticalBone.min, 300.0);
    EXPECT_EQ(hounsfield::CorticalBone.max, 3000.0);
}

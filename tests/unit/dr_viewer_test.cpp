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

#include "ui/dr_viewer.hpp"
#include "core/dicom_loader.hpp"

#include <QApplication>
#include <QPointF>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>

namespace dicom_viewer::ui {
namespace {

class DRViewerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test image
        testImage_ = vtkSmartPointer<vtkImageData>::New();
        testImage_->SetDimensions(512, 512, 1);
        testImage_->SetSpacing(0.5, 0.5, 1.0);
        testImage_->SetOrigin(0, 0, 0);
        testImage_->AllocateScalars(VTK_SHORT, 1);

        // Fill with test data
        short* ptr = static_cast<short*>(testImage_->GetScalarPointer());
        for (int i = 0; i < 512 * 512; ++i) {
            ptr[i] = static_cast<short>(i % 4096);
        }
    }

    vtkSmartPointer<vtkImageData> testImage_;
};

// Test standard presets
TEST(DRPresetsTest, StandardPresetsAvailable) {
    auto presets = getStandardDRPresets();

    EXPECT_FALSE(presets.empty());
    EXPECT_GE(presets.size(), 5u);

    // Check for essential presets
    bool hasChest = false;
    bool hasBone = false;
    bool hasSoftTissue = false;

    for (const auto& preset : presets) {
        if (preset.name == "Chest") hasChest = true;
        if (preset.name == "Bone") hasBone = true;
        if (preset.name == "Soft Tissue") hasSoftTissue = true;
    }

    EXPECT_TRUE(hasChest);
    EXPECT_TRUE(hasBone);
    EXPECT_TRUE(hasSoftTissue);
}

// Test preset window values
TEST(DRPresetsTest, PresetWindowValues) {
    auto presets = getStandardDRPresets();

    for (const auto& preset : presets) {
        // Window width should be positive
        EXPECT_GT(preset.windowWidth, 0) << "Preset: " << preset.name.toStdString();

        // Names should not be empty
        EXPECT_FALSE(preset.name.isEmpty());
        EXPECT_FALSE(preset.description.isEmpty());
    }
}

// Test modality detection
TEST(DRModalityTest, DetectDRModality) {
    EXPECT_TRUE(isDRorCRModality("CR"));
    EXPECT_TRUE(isDRorCRModality("DX"));
    EXPECT_TRUE(isDRorCRModality("DR"));
    EXPECT_TRUE(isDRorCRModality("RG"));
    EXPECT_TRUE(isDRorCRModality("RF"));

    EXPECT_FALSE(isDRorCRModality("CT"));
    EXPECT_FALSE(isDRorCRModality("MR"));
    EXPECT_FALSE(isDRorCRModality("US"));
    EXPECT_FALSE(isDRorCRModality("PT"));
    EXPECT_FALSE(isDRorCRModality("NM"));
}

// Test annotation types
TEST(DRAnnotationTest, AnnotationTypes) {
    DRAnnotation textAnnotation;
    textAnnotation.type = DRAnnotationType::Text;
    textAnnotation.text = "Test";
    textAnnotation.position = QPointF(100, 100);

    EXPECT_EQ(textAnnotation.type, DRAnnotationType::Text);
    EXPECT_EQ(textAnnotation.text, "Test");
    EXPECT_EQ(textAnnotation.position.x(), 100);
    EXPECT_EQ(textAnnotation.position.y(), 100);

    DRAnnotation arrowAnnotation;
    arrowAnnotation.type = DRAnnotationType::Arrow;
    arrowAnnotation.position = QPointF(0, 0);
    arrowAnnotation.endPosition = QPointF(100, 100);

    EXPECT_EQ(arrowAnnotation.type, DRAnnotationType::Arrow);
    EXPECT_EQ(arrowAnnotation.endPosition.x(), 100);
    EXPECT_EQ(arrowAnnotation.endPosition.y(), 100);

    DRAnnotation markerAnnotation;
    markerAnnotation.type = DRAnnotationType::Marker;
    markerAnnotation.markerNumber = 5;
    markerAnnotation.position = QPointF(50, 50);

    EXPECT_EQ(markerAnnotation.type, DRAnnotationType::Marker);
    EXPECT_EQ(markerAnnotation.markerNumber, 5);
}

// Test viewer options defaults
TEST(DRViewerOptionsTest, DefaultValues) {
    DRViewerOptions options;

    EXPECT_TRUE(options.showOrientationMarkers);
    EXPECT_TRUE(options.showPatientInfo);
    EXPECT_TRUE(options.showStudyInfo);
    EXPECT_TRUE(options.showScaleBar);
    EXPECT_TRUE(options.autoDetectMagnification);
    EXPECT_EQ(options.manualPixelSpacing, -1.0);
    EXPECT_EQ(options.defaultPreset, "Chest");
    EXPECT_TRUE(options.enableComparison);
    EXPECT_EQ(options.comparisonLayout, ComparisonLayout::SideBySide);
    EXPECT_TRUE(options.persistAnnotations);
}

// Test comparison layout enum
TEST(ComparisonLayoutTest, LayoutValues) {
    EXPECT_EQ(static_cast<int>(ComparisonLayout::SideBySide), 0);
    EXPECT_EQ(static_cast<int>(ComparisonLayout::TopBottom), 1);
    EXPECT_EQ(static_cast<int>(ComparisonLayout::Overlay), 2);
}

// Test DRPreset structure
TEST(DRPresetTest, PresetStructure) {
    DRPreset preset;
    preset.name = "TestPreset";
    preset.windowWidth = 1000;
    preset.windowCenter = 500;
    preset.description = "Test description";

    EXPECT_EQ(preset.name, "TestPreset");
    EXPECT_EQ(preset.windowWidth, 1000);
    EXPECT_EQ(preset.windowCenter, 500);
    EXPECT_EQ(preset.description, "Test description");
}

// =============================================================================
// Error recovery and boundary tests (Issue #205)
// =============================================================================

TEST(DRPresetsTest, PresetWindowValuesArePositive) {
    auto presets = getStandardDRPresets();
    for (const auto& preset : presets) {
        EXPECT_GT(preset.windowWidth, 0.0)
            << "Preset '" << preset.name.toStdString()
            << "' has non-positive window width";
    }
}

TEST(DRModalityTest, EmptyModalityIsNotDR) {
    EXPECT_FALSE(isDRorCRModality(""));
    EXPECT_FALSE(isDRorCRModality("CT"));
    EXPECT_FALSE(isDRorCRModality("MR"));
    EXPECT_FALSE(isDRorCRModality("US"));
}

TEST(DRViewerOptionsTest, ManualPixelSpacingEdgeCases) {
    DRViewerOptions options;

    // Default should be negative (auto-detect)
    EXPECT_LT(options.manualPixelSpacing, 0.0);

    // Very small pixel spacing (high resolution DR)
    options.manualPixelSpacing = 0.05;
    EXPECT_DOUBLE_EQ(options.manualPixelSpacing, 0.05);

    // Typical CR spacing
    options.manualPixelSpacing = 0.2;
    EXPECT_DOUBLE_EQ(options.manualPixelSpacing, 0.2);

    // Large spacing (low resolution)
    options.manualPixelSpacing = 1.0;
    EXPECT_DOUBLE_EQ(options.manualPixelSpacing, 1.0);
}

} // anonymous namespace
} // namespace dicom_viewer::ui

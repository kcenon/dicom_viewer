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

#include "services/volume_renderer.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

using namespace dicom_viewer::services;

class VolumeRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = std::make_unique<VolumeRenderer>();
    }

    void TearDown() override {
        renderer.reset();
    }

    vtkSmartPointer<vtkImageData> createTestVolume(int dims = 64) {
        auto imageData = vtkSmartPointer<vtkImageData>::New();
        imageData->SetDimensions(dims, dims, dims);
        imageData->AllocateScalars(VTK_SHORT, 1);

        // Fill with test data
        short* ptr = static_cast<short*>(imageData->GetScalarPointer());
        for (int i = 0; i < dims * dims * dims; ++i) {
            ptr[i] = static_cast<short>(i % 1000 - 500);
        }

        return imageData;
    }

    std::unique_ptr<VolumeRenderer> renderer;
};

// Test construction
TEST_F(VolumeRendererTest, DefaultConstruction) {
    EXPECT_NE(renderer, nullptr);
    EXPECT_NE(renderer->getVolume(), nullptr);
}

// Test move semantics
TEST_F(VolumeRendererTest, MoveConstructor) {
    auto volume1 = renderer->getVolume();
    VolumeRenderer moved(std::move(*renderer));
    EXPECT_EQ(moved.getVolume(), volume1);
}

TEST_F(VolumeRendererTest, MoveAssignment) {
    auto volume1 = renderer->getVolume();
    VolumeRenderer other;
    other = std::move(*renderer);
    EXPECT_EQ(other.getVolume(), volume1);
}

// Test input data
TEST_F(VolumeRendererTest, SetInputDataAcceptsValidVolume) {
    auto volume = createTestVolume();
    EXPECT_NO_THROW(renderer->setInputData(volume));
}

TEST_F(VolumeRendererTest, SetInputDataAcceptsNullptr) {
    EXPECT_NO_THROW(renderer->setInputData(nullptr));
}

// Test GPU rendering control
TEST_F(VolumeRendererTest, GPURenderingDefaultState) {
    // Without validation, GPU is not enabled
    EXPECT_FALSE(renderer->isGPURenderingEnabled());
}

TEST_F(VolumeRendererTest, SetGPURenderingEnabledWithoutValidation) {
    bool result = renderer->setGPURenderingEnabled(true);
    // Returns false because GPU was never validated
    EXPECT_FALSE(result);
}

TEST_F(VolumeRendererTest, ValidateGPUSupportWithNullWindow) {
    bool result = renderer->validateGPUSupport(nullptr);
    EXPECT_FALSE(result);
    EXPECT_FALSE(renderer->isGPURenderingEnabled());
}

// Test blend modes
TEST_F(VolumeRendererTest, SetBlendModeComposite) {
    EXPECT_NO_THROW(renderer->setBlendMode(BlendMode::Composite));
}

TEST_F(VolumeRendererTest, SetBlendModeMIP) {
    EXPECT_NO_THROW(renderer->setBlendMode(BlendMode::MaximumIntensity));
}

TEST_F(VolumeRendererTest, SetBlendModeMinIP) {
    EXPECT_NO_THROW(renderer->setBlendMode(BlendMode::MinimumIntensity));
}

TEST_F(VolumeRendererTest, SetBlendModeAverage) {
    EXPECT_NO_THROW(renderer->setBlendMode(BlendMode::Average));
}

// Test window/level
TEST_F(VolumeRendererTest, SetWindowLevelValidValues) {
    EXPECT_NO_THROW(renderer->setWindowLevel(400.0, 40.0));
}

TEST_F(VolumeRendererTest, SetWindowLevelZeroWidth) {
    EXPECT_NO_THROW(renderer->setWindowLevel(0.0, 40.0));
}

// Test LOD
TEST_F(VolumeRendererTest, SetInteractiveLODEnabled) {
    EXPECT_NO_THROW(renderer->setInteractiveLODEnabled(true));
}

TEST_F(VolumeRendererTest, SetInteractiveLODDisabled) {
    EXPECT_NO_THROW(renderer->setInteractiveLODEnabled(false));
}

// Test clipping planes
TEST_F(VolumeRendererTest, SetClippingPlanesValidBounds) {
    std::array<double, 6> planes = {-100, 100, -100, 100, -100, 100};
    EXPECT_NO_THROW(renderer->setClippingPlanes(planes));
}

TEST_F(VolumeRendererTest, ClearClippingPlanes) {
    std::array<double, 6> planes = {-100, 100, -100, 100, -100, 100};
    renderer->setClippingPlanes(planes);
    EXPECT_NO_THROW(renderer->clearClippingPlanes());
}

// Test update
TEST_F(VolumeRendererTest, UpdateDoesNotThrow) {
    EXPECT_NO_THROW(renderer->update());
}

// Test preset transfer functions
TEST_F(VolumeRendererTest, GetPresetCTBone) {
    auto preset = VolumeRenderer::getPresetCTBone();
    EXPECT_EQ(preset.name, "CT Bone");
    EXPECT_EQ(preset.windowWidth, 2000);
    EXPECT_EQ(preset.windowCenter, 400);
    EXPECT_FALSE(preset.colorPoints.empty());
    EXPECT_FALSE(preset.opacityPoints.empty());
}

TEST_F(VolumeRendererTest, GetPresetCTSoftTissue) {
    auto preset = VolumeRenderer::getPresetCTSoftTissue();
    EXPECT_EQ(preset.name, "CT Soft Tissue");
    EXPECT_EQ(preset.windowWidth, 400);
    EXPECT_EQ(preset.windowCenter, 40);
}

TEST_F(VolumeRendererTest, GetPresetCTLung) {
    auto preset = VolumeRenderer::getPresetCTLung();
    EXPECT_EQ(preset.name, "CT Lung");
    EXPECT_EQ(preset.windowWidth, 1500);
    EXPECT_EQ(preset.windowCenter, -600);
}

TEST_F(VolumeRendererTest, GetPresetCTAngio) {
    auto preset = VolumeRenderer::getPresetCTAngio();
    EXPECT_EQ(preset.name, "CT Angio");
    EXPECT_EQ(preset.windowWidth, 400);
    EXPECT_EQ(preset.windowCenter, 200);
}

TEST_F(VolumeRendererTest, GetPresetCTAbdomen) {
    auto preset = VolumeRenderer::getPresetCTAbdomen();
    EXPECT_EQ(preset.name, "CT Abdomen");
    EXPECT_EQ(preset.windowWidth, 400);
    EXPECT_EQ(preset.windowCenter, 50);
}

TEST_F(VolumeRendererTest, GetPresetMRIDefault) {
    auto preset = VolumeRenderer::getPresetMRIDefault();
    EXPECT_EQ(preset.name, "MRI Default");
}

// Test applying presets
TEST_F(VolumeRendererTest, ApplyPresetCTBone) {
    auto preset = VolumeRenderer::getPresetCTBone();
    EXPECT_NO_THROW(renderer->applyPreset(preset));
}

TEST_F(VolumeRendererTest, ApplyPresetWithGradientOpacity) {
    TransferFunctionPreset preset{
        .name = "Test",
        .windowWidth = 400,
        .windowCenter = 40,
        .colorPoints = {{0, 0, 0, 0}, {100, 1, 1, 1}},
        .opacityPoints = {{0, 0}, {100, 1}},
        .gradientOpacityPoints = {{0, 0}, {100, 1}}
    };
    EXPECT_NO_THROW(renderer->applyPreset(preset));
}

TEST_F(VolumeRendererTest, ApplyPresetEmptyPoints) {
    TransferFunctionPreset preset{
        .name = "Empty",
        .windowWidth = 400,
        .windowCenter = 40,
        .colorPoints = {},
        .opacityPoints = {}
    };
    EXPECT_NO_THROW(renderer->applyPreset(preset));
}

// =============================================================================
// Error recovery and boundary tests (Issue #205)
// =============================================================================

TEST_F(VolumeRendererTest, ZeroSizeVolumeHandledGracefully) {
    auto zeroVolume = vtkSmartPointer<vtkImageData>::New();
    zeroVolume->SetDimensions(0, 0, 0);
    zeroVolume->AllocateScalars(VTK_SHORT, 1);

    EXPECT_NO_THROW(renderer->setInputData(zeroVolume));
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(VolumeRendererTest, LargeVolume512CubedDoesNotCrash) {
    auto largeVolume = createTestVolume(512);

    EXPECT_NO_THROW(renderer->setInputData(largeVolume));
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(VolumeRendererTest, ExtremeWindowLevelValues) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    // Minimal window width (1) — should not crash
    EXPECT_NO_THROW(renderer->setWindowLevel(1.0, 0.0));
    EXPECT_NO_THROW(renderer->update());

    // Zero window width — edge case
    EXPECT_NO_THROW(renderer->setWindowLevel(0.0, 0.0));
    EXPECT_NO_THROW(renderer->update());

    // Very large values
    EXPECT_NO_THROW(renderer->setWindowLevel(100000.0, -50000.0));
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(VolumeRendererTest, NullTransferFunctionHandled) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    // Apply preset with null/empty gradient opacity
    TransferFunctionPreset preset{
        .name = "NullGradient",
        .windowWidth = 400,
        .windowCenter = 40,
        .colorPoints = {{0, 0, 0, 0}, {400, 1, 1, 1}},
        .opacityPoints = {{0, 0}, {400, 1}},
        .gradientOpacityPoints = {}
    };
    EXPECT_NO_THROW(renderer->applyPreset(preset));
    EXPECT_NO_THROW(renderer->update());

    // Apply preset with single point (degenerate)
    TransferFunctionPreset singlePoint{
        .name = "SinglePoint",
        .windowWidth = 1,
        .windowCenter = 0,
        .colorPoints = {{0, 0.5, 0.5, 0.5}},
        .opacityPoints = {{0, 0.5}}
    };
    EXPECT_NO_THROW(renderer->applyPreset(singlePoint));
    EXPECT_NO_THROW(renderer->update());
}

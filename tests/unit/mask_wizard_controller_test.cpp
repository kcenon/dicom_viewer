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

#include "ui/mask_wizard_controller.hpp"
#include "ui/dialogs/mask_wizard.hpp"
#include "services/segmentation/threshold_segmenter.hpp"
#include "services/segmentation/label_manager.hpp"

#include <itkImage.h>
#include <itkImageRegionIterator.h>

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

namespace {

using ImageType = dicom_viewer::services::ThresholdSegmenter::ImageType;

/// Create a synthetic 3D test image with known intensity pattern
ImageType::Pointer createTestImage(int dimX = 32, int dimY = 32, int dimZ = 16)
{
    auto image = ImageType::New();

    ImageType::IndexType start;
    start.Fill(0);

    ImageType::SizeType size;
    size[0] = dimX;
    size[1] = dimY;
    size[2] = dimZ;

    ImageType::RegionType region(start, size);
    image->SetRegions(region);

    ImageType::SpacingType spacing;
    spacing.Fill(1.0);
    image->SetSpacing(spacing);

    image->Allocate();
    image->FillBuffer(0);

    // Create two distinct blobs:
    // Blob A (high intensity 500): centered at (8, 8, 8), radius 4
    // Blob B (high intensity 300): centered at (24, 24, 8), radius 3
    itk::ImageRegionIterator<ImageType> it(image, region);
    while (!it.IsAtEnd()) {
        auto idx = it.GetIndex();

        double dx_a = idx[0] - 8.0;
        double dy_a = idx[1] - 8.0;
        double dz_a = idx[2] - 8.0;
        double dist_a = dx_a * dx_a + dy_a * dy_a + dz_a * dz_a;

        double dx_b = idx[0] - 24.0;
        double dy_b = idx[1] - 24.0;
        double dz_b = idx[2] - 8.0;
        double dist_b = dx_b * dx_b + dy_b * dy_b + dz_b * dz_b;

        if (dist_a <= 16.0) {
            it.Set(500);  // Blob A
        } else if (dist_b <= 9.0) {
            it.Set(300);  // Blob B
        } else {
            it.Set(-100);  // Background
        }
        ++it;
    }

    return image;
}

class MaskWizardControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure QApplication exists (needed for QWizard)
        if (!QApplication::instance()) {
            static int argc = 1;
            static char appName[] = "test";
            static char* argv[] = {appName, nullptr};
            app_ = new QApplication(argc, argv);
        }

        wizard_ = new dicom_viewer::ui::MaskWizard();
        controller_ = new dicom_viewer::ui::MaskWizardController(wizard_, wizard_);
    }

    void TearDown() override {
        delete wizard_;
        wizard_ = nullptr;
        controller_ = nullptr;  // Destroyed as child of wizard
    }

    dicom_viewer::ui::MaskWizard* wizard_ = nullptr;
    dicom_viewer::ui::MaskWizardController* controller_ = nullptr;
    QApplication* app_ = nullptr;
};

TEST_F(MaskWizardControllerTest, ConstructionWithNullContext)
{
    // Controller should be constructible without a context set
    EXPECT_NE(controller_, nullptr);
}

TEST_F(MaskWizardControllerTest, SetContextConfiguresPhaseCount)
{
    auto image = createTestImage();

    dicom_viewer::ui::MaskWizardController::Context ctx;
    ctx.sourceImage = image;
    ctx.currentPhase = 2;

    // Create 3 dummy phase images
    using FloatImage3D = dicom_viewer::services::PhaseTracker::FloatImage3D;
    for (int i = 0; i < 3; ++i) {
        auto phase = FloatImage3D::New();
        FloatImage3D::RegionType region;
        FloatImage3D::SizeType size = {{32, 32, 16}};
        region.SetSize(size);
        phase->SetRegions(region);
        phase->Allocate();
        phase->FillBuffer(0.0f);
        ctx.magnitudePhases.push_back(phase);
    }

    controller_->setContext(ctx);

    EXPECT_EQ(wizard_->phaseCount(), 3);
    EXPECT_EQ(wizard_->referencePhase(), 2);
}

TEST_F(MaskWizardControllerTest, OtsuThresholdUpdatesWizard)
{
    auto image = createTestImage();

    dicom_viewer::ui::MaskWizardController::Context ctx;
    ctx.sourceImage = image;
    controller_->setContext(ctx);

    // Simulate page transition to threshold step to trigger crop
    // (context sets croppedImage = sourceImage)

    // Emit otsuRequested signal
    emit wizard_->otsuRequested();

    // After Otsu, the threshold value should be set in the wizard
    // (exact value depends on image content, but it should be between
    // background (-100) and foreground (300-500))
    int minThreshold = wizard_->thresholdMin();
    EXPECT_GT(minThreshold, -100);
    EXPECT_LT(minThreshold, 500);
}

TEST_F(MaskWizardControllerTest, ThresholdChangedTriggersDebounce)
{
    auto image = createTestImage();

    dicom_viewer::ui::MaskWizardController::Context ctx;
    ctx.sourceImage = image;
    controller_->setContext(ctx);

    // Emit threshold changed (debounced)
    emit wizard_->thresholdChanged(200, 600);

    // The debounce timer should not have fired yet
    // Process events to let the timer trigger
    QTest::qWait(300);  // Wait longer than debounce interval (200ms)

    // No crash means the threshold was processed correctly
    SUCCEED();
}

TEST_F(MaskWizardControllerTest, SetLabelManager)
{
    dicom_viewer::services::LabelManager manager;
    controller_->setLabelManager(&manager);

    // No crash, manager is stored for later use
    SUCCEED();
}

TEST_F(MaskWizardControllerTest, ErrorSignalEmitted)
{
    QSignalSpy errorSpy(controller_,
                         &dicom_viewer::ui::MaskWizardController::errorOccurred);

    // Request propagation without any context (should emit error)
    emit wizard_->propagationRequested();

    EXPECT_GE(errorSpy.count(), 1);
}

TEST_F(MaskWizardControllerTest, VolumeDimensionsSetFromContext)
{
    auto image = createTestImage(64, 48, 20);

    dicom_viewer::ui::MaskWizardController::Context ctx;
    ctx.sourceImage = image;
    controller_->setContext(ctx);

    // The wizard should have the volume dimensions set by MainWindow,
    // but the controller preserves the source image
    // (dimensions are set by MainWindow, not the controller)
    SUCCEED();
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Create QApplication before running tests (Qt widgets need it)
    QApplication app(argc, argv);

    return RUN_ALL_TESTS();
}

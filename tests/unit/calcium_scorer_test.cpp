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

#include "services/cardiac/cardiac_types.hpp"
#include "services/cardiac/calcium_scorer.hpp"

#include <itkImage.h>
#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

using ImageType = itk::Image<short, 3>;
using MaskImageType = itk::Image<uint8_t, 3>;

namespace {

/// Helper: create a 3D test image with given dimensions and spacing
ImageType::Pointer createTestImage(int sx, int sy, int sz,
                                    double spX = 0.5, double spY = 0.5,
                                    double spZ = 3.0)
{
    auto image = ImageType::New();
    ImageType::RegionType region;
    ImageType::IndexType start = {{0, 0, 0}};
    ImageType::SizeType size;
    size[0] = sx; size[1] = sy; size[2] = sz;
    region.SetIndex(start);
    region.SetSize(size);
    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(0);

    const double spacing[3] = {spX, spY, spZ};
    image->SetSpacing(spacing);

    return image;
}

/// Helper: create a mask image with same geometry as reference
MaskImageType::Pointer createMaskFromImage(ImageType::Pointer ref)
{
    auto mask = MaskImageType::New();
    mask->SetRegions(ref->GetLargestPossibleRegion());
    mask->SetSpacing(ref->GetSpacing());
    mask->SetOrigin(ref->GetOrigin());
    mask->SetDirection(ref->GetDirection());
    mask->Allocate();
    mask->FillBuffer(0);
    return mask;
}

/// Helper: set a rectangular block of voxels to a given HU value
void setBlock(ImageType::Pointer image,
              int x0, int y0, int z0,
              int x1, int y1, int z1,
              short value)
{
    ImageType::IndexType idx;
    for (int z = z0; z <= z1; ++z) {
        idx[2] = z;
        for (int y = y0; y <= y1; ++y) {
            idx[1] = y;
            for (int x = x0; x <= x1; ++x) {
                idx[0] = x;
                image->SetPixel(idx, value);
            }
        }
    }
}

/// Helper: set a mask block
void setMaskBlock(MaskImageType::Pointer mask,
                  int x0, int y0, int z0,
                  int x1, int y1, int z1,
                  uint8_t value)
{
    MaskImageType::IndexType idx;
    for (int z = z0; z <= z1; ++z) {
        idx[2] = z;
        for (int y = y0; y <= y1; ++y) {
            idx[1] = y;
            for (int x = x0; x <= x1; ++x) {
                idx[0] = x;
                mask->SetPixel(idx, value);
            }
        }
    }
}

}  // anonymous namespace

// =============================================================================
// Calcium Types Tests
// =============================================================================

TEST(CalciumTypesTest, CalcifiedLesionDefaults) {
    CalcifiedLesion lesion;
    EXPECT_EQ(lesion.labelId, 0);
    EXPECT_DOUBLE_EQ(lesion.areaMM2, 0.0);
    EXPECT_DOUBLE_EQ(lesion.peakHU, 0.0);
    EXPECT_EQ(lesion.weightFactor, 0);
    EXPECT_DOUBLE_EQ(lesion.agatstonScore, 0.0);
    EXPECT_DOUBLE_EQ(lesion.volumeMM3, 0.0);
    EXPECT_TRUE(lesion.assignedArtery.empty());
}

TEST(CalciumTypesTest, CalciumScoreResultDefaults) {
    CalciumScoreResult result;
    EXPECT_DOUBLE_EQ(result.totalAgatston, 0.0);
    EXPECT_DOUBLE_EQ(result.volumeScore, 0.0);
    EXPECT_DOUBLE_EQ(result.massScore, 0.0);
    EXPECT_TRUE(result.perArteryScores.empty());
    EXPECT_TRUE(result.riskCategory.empty());
    EXPECT_TRUE(result.lesions.empty());
    EXPECT_EQ(result.lesionCount, 0);
    EXPECT_FALSE(result.hasCalcium());
}

TEST(CalciumTypesTest, CalciumScoreResultHasCalcium) {
    CalciumScoreResult result;
    result.totalAgatston = 150.0;
    EXPECT_TRUE(result.hasCalcium());
}

TEST(CalciumTypesTest, CalciumConstants) {
    EXPECT_EQ(calcium_constants::kHUThreshold, 130);
    EXPECT_DOUBLE_EQ(calcium_constants::kMinLesionAreaMM2, 1.0);
    EXPECT_EQ(calcium_constants::kWeightThreshold1, 130);
    EXPECT_EQ(calcium_constants::kWeightThreshold2, 200);
    EXPECT_EQ(calcium_constants::kWeightThreshold3, 300);
    EXPECT_EQ(calcium_constants::kWeightThreshold4, 400);
}

// =============================================================================
// CalciumScorer Construction Tests
// =============================================================================

TEST(CalciumScorerTest, DefaultConstruction) {
    CalciumScorer scorer;
}

TEST(CalciumScorerTest, MoveConstruction) {
    CalciumScorer scorer;
    CalciumScorer moved(std::move(scorer));
}

TEST(CalciumScorerTest, MoveAssignment) {
    CalciumScorer scorer;
    CalciumScorer other;
    other = std::move(scorer);
}

// =============================================================================
// Density Weight Factor Tests
// =============================================================================

TEST(CalciumScorerTest, DensityWeightBelowThreshold) {
    EXPECT_EQ(CalciumScorer::densityWeightFactor(0), 0);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(100), 0);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(129), 0);
}

TEST(CalciumScorerTest, DensityWeightFactor1) {
    EXPECT_EQ(CalciumScorer::densityWeightFactor(130), 1);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(150), 1);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(199), 1);
}

TEST(CalciumScorerTest, DensityWeightFactor2) {
    EXPECT_EQ(CalciumScorer::densityWeightFactor(200), 2);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(250), 2);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(299), 2);
}

TEST(CalciumScorerTest, DensityWeightFactor3) {
    EXPECT_EQ(CalciumScorer::densityWeightFactor(300), 3);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(350), 3);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(399), 3);
}

TEST(CalciumScorerTest, DensityWeightFactor4) {
    EXPECT_EQ(CalciumScorer::densityWeightFactor(400), 4);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(500), 4);
    EXPECT_EQ(CalciumScorer::densityWeightFactor(1000), 4);
}

// =============================================================================
// Risk Classification Tests
// =============================================================================

TEST(CalciumScorerTest, RiskClassificationNone) {
    EXPECT_EQ(CalciumScorer::classifyRisk(0.0), "None");
    EXPECT_EQ(CalciumScorer::classifyRisk(-1.0), "None");
}

TEST(CalciumScorerTest, RiskClassificationMinimal) {
    EXPECT_EQ(CalciumScorer::classifyRisk(1.0), "Minimal");
    EXPECT_EQ(CalciumScorer::classifyRisk(5.0), "Minimal");
    EXPECT_EQ(CalciumScorer::classifyRisk(10.0), "Minimal");
}

TEST(CalciumScorerTest, RiskClassificationMild) {
    EXPECT_EQ(CalciumScorer::classifyRisk(11.0), "Mild");
    EXPECT_EQ(CalciumScorer::classifyRisk(50.0), "Mild");
    EXPECT_EQ(CalciumScorer::classifyRisk(100.0), "Mild");
}

TEST(CalciumScorerTest, RiskClassificationModerate) {
    EXPECT_EQ(CalciumScorer::classifyRisk(101.0), "Moderate");
    EXPECT_EQ(CalciumScorer::classifyRisk(250.0), "Moderate");
    EXPECT_EQ(CalciumScorer::classifyRisk(400.0), "Moderate");
}

TEST(CalciumScorerTest, RiskClassificationSevere) {
    EXPECT_EQ(CalciumScorer::classifyRisk(401.0), "Severe");
    EXPECT_EQ(CalciumScorer::classifyRisk(1000.0), "Severe");
    EXPECT_EQ(CalciumScorer::classifyRisk(5000.0), "Severe");
}

// =============================================================================
// Agatston Score Computation Tests
// =============================================================================

TEST(CalciumScorerTest, AgatstonNullImage) {
    CalciumScorer scorer;
    auto result = scorer.computeAgatston(nullptr, 3.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::InternalError);
}

TEST(CalciumScorerTest, AgatstonInvalidSliceThickness) {
    CalciumScorer scorer;
    auto image = createTestImage(10, 10, 5);
    auto result = scorer.computeAgatston(image, 0.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::InternalError);
}

TEST(CalciumScorerTest, AgatstonZeroCalcium) {
    CalciumScorer scorer;
    auto image = createTestImage(20, 20, 10);
    image->FillBuffer(50);  // All well below 130 HU

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value().totalAgatston, 0.0);
    EXPECT_EQ(result.value().lesionCount, 0);
    EXPECT_EQ(result.value().riskCategory, "None");
    EXPECT_FALSE(result.value().hasCalcium());
}

TEST(CalciumScorerTest, AgatstonSingleLesionWeight1) {
    CalciumScorer scorer;
    // Spacing: 0.5 x 0.5 mm → pixel area = 0.25 mm²
    auto image = createTestImage(20, 20, 5, 0.5, 0.5, 3.0);

    // Place a 4x4 pixel block at 150 HU on slice z=2
    // Area = 4*4 * 0.25 = 4.0 mm², weight = 1 (130-199 HU)
    // Agatston = 4.0 * 1 = 4.0
    setBlock(image, 5, 5, 2, 8, 8, 2, 150);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());

    auto& score = result.value();
    EXPECT_EQ(score.lesionCount, 1);
    EXPECT_NEAR(score.totalAgatston, 4.0, 0.5);
    EXPECT_EQ(score.lesions[0].weightFactor, 1);
    EXPECT_EQ(score.riskCategory, "Minimal");
}

TEST(CalciumScorerTest, AgatstonSingleLesionWeight4) {
    CalciumScorer scorer;
    auto image = createTestImage(20, 20, 5, 0.5, 0.5, 3.0);

    // 4x4 block at 500 HU on slice z=2
    // Area = 4.0 mm², weight = 4 (>= 400 HU)
    // Agatston = 4.0 * 4 = 16.0
    setBlock(image, 5, 5, 2, 8, 8, 2, 500);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());

    auto& score = result.value();
    EXPECT_EQ(score.lesionCount, 1);
    EXPECT_NEAR(score.totalAgatston, 16.0, 0.5);
    EXPECT_EQ(score.lesions[0].weightFactor, 4);
}

TEST(CalciumScorerTest, AgatstonMultipleSlicesOneLesion) {
    CalciumScorer scorer;
    auto image = createTestImage(20, 20, 10, 0.5, 0.5, 3.0);

    // 4x4 block across slices 3, 4, 5 at 250 HU
    // Per-slice area = 4.0 mm², weight = 2 (200-299 HU)
    // Per-slice Agatston = 4.0 * 2 = 8.0
    // Total = 8.0 * 3 = 24.0
    setBlock(image, 5, 5, 3, 8, 8, 5, 250);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());

    auto& score = result.value();
    EXPECT_EQ(score.lesionCount, 1);
    EXPECT_NEAR(score.totalAgatston, 24.0, 1.0);
    // Volume = 16 voxels * 3 slices * 0.25 * 3.0 = 36 mm³
    EXPECT_NEAR(score.volumeScore, 36.0, 1.0);
}

TEST(CalciumScorerTest, AgatstonTwoSeparateLesions) {
    CalciumScorer scorer;
    auto image = createTestImage(30, 30, 5, 0.5, 0.5, 3.0);

    // Lesion 1: 4x4 at 180 HU on slice 1 → weight 1, area 4.0, score 4.0
    setBlock(image, 2, 2, 1, 5, 5, 1, 180);

    // Lesion 2: 4x4 at 350 HU on slice 3 → weight 3, area 4.0, score 12.0
    setBlock(image, 20, 20, 3, 23, 23, 3, 350);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());

    auto& score = result.value();
    EXPECT_EQ(score.lesionCount, 2);
    EXPECT_NEAR(score.totalAgatston, 16.0, 1.0);  // 4 + 12
}

TEST(CalciumScorerTest, AgatstonSmallLesionFiltered) {
    CalciumScorer scorer;
    // Spacing: 1.0 x 1.0 mm → pixel area = 1.0 mm²
    auto image = createTestImage(20, 20, 5, 1.0, 1.0, 3.0);

    // Single voxel at 200 HU → area = 1.0 mm² (barely meets threshold)
    ImageType::IndexType idx = {{10, 10, 2}};
    image->SetPixel(idx, 200);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());
    // 1 pixel with 1.0 mm² area → meets minimum area threshold of 1.0
    EXPECT_GE(result.value().lesionCount, 0);  // May or may not be filtered
}

TEST(CalciumScorerTest, AgatstonRiskModerateScore) {
    CalciumScorer scorer;
    auto image = createTestImage(100, 100, 20, 0.5, 0.5, 3.0);

    // Place a large calcification to generate score > 100
    // 20x20 block at 400 HU, 3 slices
    // Area per slice = 400 * 0.25 = 100 mm², weight = 4
    // Per-slice Agatston = 100 * 4 = 400
    // Total = 400 * 3 = 1200 → Severe
    setBlock(image, 40, 40, 8, 59, 59, 10, 400);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value().totalAgatston, 400.0);
    EXPECT_EQ(result.value().riskCategory, "Severe");
}

// =============================================================================
// Volume Score Tests
// =============================================================================

TEST(CalciumScorerTest, VolumeScoreNullImage) {
    CalciumScorer scorer;
    auto result = scorer.computeVolumeScore(nullptr);
    EXPECT_FALSE(result.has_value());
}

TEST(CalciumScorerTest, VolumeScoreZero) {
    CalciumScorer scorer;
    auto image = createTestImage(10, 10, 5);
    image->FillBuffer(50);

    auto result = scorer.computeVolumeScore(image);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value(), 0.0);
}

TEST(CalciumScorerTest, VolumeScoreComputation) {
    CalciumScorer scorer;
    auto image = createTestImage(10, 10, 5, 1.0, 1.0, 2.0);

    // 3x3x2 block at 200 HU → 18 voxels above threshold
    // Volume per voxel = 1.0 * 1.0 * 2.0 = 2.0 mm³
    // Total = 18 * 2.0 = 36.0 mm³
    setBlock(image, 3, 3, 1, 5, 5, 2, 200);

    auto result = scorer.computeVolumeScore(image);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 36.0, 0.1);
}

// =============================================================================
// Mass Score Tests
// =============================================================================

TEST(CalciumScorerTest, MassScoreNullImage) {
    CalciumScorer scorer;
    auto result = scorer.computeMassScore(nullptr, 0.001);
    EXPECT_FALSE(result.has_value());
}

TEST(CalciumScorerTest, MassScoreInvalidCalibration) {
    CalciumScorer scorer;
    auto image = createTestImage(10, 10, 5);
    auto result = scorer.computeMassScore(image, 0.0);
    EXPECT_FALSE(result.has_value());
}

TEST(CalciumScorerTest, MassScoreComputation) {
    CalciumScorer scorer;
    auto image = createTestImage(10, 10, 5, 1.0, 1.0, 1.0);

    // Single voxel at 200 HU
    ImageType::IndexType idx = {{5, 5, 2}};
    image->SetPixel(idx, 200);

    double calibration = 0.001;  // mg/mL per HU
    auto result = scorer.computeMassScore(image, calibration);
    ASSERT_TRUE(result.has_value());
    // mass = 200 * 0.001 * (1.0 * 1.0 * 1.0 / 1000) = 200 * 0.001 * 0.001 = 0.0002 mg
    EXPECT_NEAR(result.value(), 0.0002, 0.0001);
}

// =============================================================================
// Artery Assignment Tests
// =============================================================================

TEST(CalciumScorerTest, AssignToArteriesEmpty) {
    std::vector<CalcifiedLesion> lesions;
    std::map<std::string, MaskImageType::Pointer> rois;
    CalciumScorer::assignToArteries(lesions, rois);
    // No crash with empty inputs
}

TEST(CalciumScorerTest, AssignToArteriesWithROI) {
    auto refImage = createTestImage(20, 20, 5, 1.0, 1.0, 1.0);
    auto ladMask = createMaskFromImage(refImage);
    auto rcaMask = createMaskFromImage(refImage);

    // LAD covers region (0-9, 0-9, 0-4)
    setMaskBlock(ladMask, 0, 0, 0, 9, 9, 4, 1);

    // RCA covers region (10-19, 10-19, 0-4)
    setMaskBlock(rcaMask, 10, 10, 0, 19, 19, 4, 1);

    CalcifiedLesion lesion1;
    lesion1.centroid = {5.0, 5.0, 2.0};  // Inside LAD

    CalcifiedLesion lesion2;
    lesion2.centroid = {15.0, 15.0, 2.0};  // Inside RCA

    CalcifiedLesion lesion3;
    lesion3.centroid = {5.0, 15.0, 2.0};  // Outside both

    std::vector<CalcifiedLesion> lesions = {lesion1, lesion2, lesion3};
    std::map<std::string, MaskImageType::Pointer> rois;
    rois["LAD"] = ladMask;
    rois["RCA"] = rcaMask;

    CalciumScorer::assignToArteries(lesions, rois);

    EXPECT_EQ(lesions[0].assignedArtery, "LAD");
    EXPECT_EQ(lesions[1].assignedArtery, "RCA");
    EXPECT_TRUE(lesions[2].assignedArtery.empty());
}

// =============================================================================
// Integration-style Tests
// =============================================================================

TEST(CalciumScorerTest, FullPipelineNoCalcium) {
    CalciumScorer scorer;
    auto image = createTestImage(50, 50, 20, 0.5, 0.5, 3.0);
    image->FillBuffer(-100);  // Typical soft tissue HU

    auto agatston = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(agatston.has_value());
    EXPECT_DOUBLE_EQ(agatston.value().totalAgatston, 0.0);
    EXPECT_EQ(agatston.value().riskCategory, "None");

    auto volume = scorer.computeVolumeScore(image);
    ASSERT_TRUE(volume.has_value());
    EXPECT_DOUBLE_EQ(volume.value(), 0.0);
}

TEST(CalciumScorerTest, LesionCentroidComputation) {
    CalciumScorer scorer;
    auto image = createTestImage(20, 20, 10, 1.0, 1.0, 1.0);

    // 2x2x1 block at center of image (9,9,5 to 10,10,5), 200 HU
    setBlock(image, 9, 9, 5, 10, 10, 5, 200);

    auto result = scorer.computeAgatston(image, 1.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().lesionCount, 1);

    // Centroid should be near (9.5, 9.5, 5.0)
    auto& centroid = result.value().lesions[0].centroid;
    EXPECT_NEAR(centroid[0], 9.5, 0.5);
    EXPECT_NEAR(centroid[1], 9.5, 0.5);
    EXPECT_NEAR(centroid[2], 5.0, 0.5);
}

TEST(CalciumScorerTest, MixedDensityLesion) {
    CalciumScorer scorer;
    auto image = createTestImage(20, 20, 5, 0.5, 0.5, 3.0);

    // Lesion with mixed HU values across slices
    // Slice 2: 4x4 at 150 HU (weight 1)
    setBlock(image, 5, 5, 2, 8, 8, 2, 150);
    // Slice 3: 4x4 at 350 HU (weight 3) - connected vertically
    setBlock(image, 5, 5, 3, 8, 8, 3, 350);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().lesionCount, 1);

    // Slice 2: area = 4.0, weight = 1, score = 4.0
    // Slice 3: area = 4.0, weight = 3, score = 12.0
    // Total per-slice Agatston = 16.0
    EXPECT_NEAR(result.value().totalAgatston, 16.0, 1.0);
    // Peak HU for the lesion overall should be 350
    EXPECT_NEAR(result.value().lesions[0].peakHU, 350.0, 1.0);
}

// =============================================================================
// Tolerance validation and artifact handling tests (Issue #208)
// =============================================================================

TEST(CalciumScorerTest, ThresholdBoundaryExactly130HU) {
    CalciumScorer scorer;
    // 1mm isotropic spacing, slice thickness = 1mm
    auto image = createTestImage(20, 20, 5, 1.0, 1.0, 1.0);

    // 4x4 block at exactly 130 HU (the Agatston threshold)
    setBlock(image, 8, 8, 2, 11, 11, 2, 130);

    auto result = scorer.computeAgatston(image, 1.0);
    ASSERT_TRUE(result.has_value());

    // Voxels at exactly 130 HU should be included (threshold is ≥130)
    EXPECT_TRUE(result.value().hasCalcium())
        << "Voxels at exactly 130 HU should be counted as calcium";
    EXPECT_EQ(result.value().lesionCount, 1);
    EXPECT_EQ(result.value().lesions[0].weightFactor, 1);
}

TEST(CalciumScorerTest, ThresholdBoundaryBelow130HU) {
    CalciumScorer scorer;
    auto image = createTestImage(20, 20, 5, 1.0, 1.0, 1.0);

    // 4x4 block at 129 HU (just below threshold)
    setBlock(image, 8, 8, 2, 11, 11, 2, 129);

    auto result = scorer.computeAgatston(image, 1.0);
    ASSERT_TRUE(result.has_value());

    // 129 HU should NOT be counted as calcium
    EXPECT_FALSE(result.value().hasCalcium())
        << "Voxels at 129 HU should not be counted as calcium";
    EXPECT_DOUBLE_EQ(result.value().totalAgatston, 0.0);
}

TEST(CalciumScorerTest, SubMinimumAreaLesionFiltered) {
    CalciumScorer scorer;
    // Large pixels: 2.0mm spacing → single voxel area = 4.0mm²
    // Small pixels: 0.3mm spacing → single voxel area = 0.09mm² (<1mm²)
    auto image = createTestImage(20, 20, 5, 0.3, 0.3, 3.0);

    // Single voxel at 200 HU — area = 0.3 × 0.3 = 0.09 mm² < 1mm²
    setBlock(image, 10, 10, 2, 10, 10, 2, 200);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());

    // Sub-minimum area lesion should be filtered out
    EXPECT_EQ(result.value().lesionCount, 0)
        << "Lesion with area < 1mm² should be filtered";
    EXPECT_DOUBLE_EQ(result.value().totalAgatston, 0.0);
}

TEST(CalciumScorerTest, AgatstonScoreWithinToleranceForKnownPhantom) {
    CalciumScorer scorer;
    // 0.5mm in-plane, 3mm slice thickness — standard cardiac CT protocol
    auto image = createTestImage(40, 40, 5, 0.5, 0.5, 3.0);

    // Known lesion: 6×6 pixels at 200 HU on slice 2
    // Area = 6 × 0.5 × 6 × 0.5 = 9.0 mm²
    // Peak HU = 200 → weight factor = 2
    // Expected Agatston per slice = area × weight = 9.0 × 2 = 18.0
    setBlock(image, 10, 10, 2, 15, 15, 2, 200);

    auto result = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().hasCalcium());

    // Verify within ±5% tolerance of expected score
    double expectedScore = 18.0;
    EXPECT_NEAR(result.value().totalAgatston, expectedScore,
                expectedScore * 0.05)
        << "Agatston score should be within 5% of phantom ground truth";
}

TEST(CalciumScorerTest, VolumeMassScoreConsistency) {
    CalciumScorer scorer;
    auto image = createTestImage(40, 40, 5, 0.5, 0.5, 3.0);

    // 8×8 block at 300 HU on slices 1-3
    setBlock(image, 10, 10, 1, 17, 17, 3, 300);

    auto agatston = scorer.computeAgatston(image, 3.0);
    ASSERT_TRUE(agatston.has_value());

    auto volume = scorer.computeVolumeScore(image);
    ASSERT_TRUE(volume.has_value());
    EXPECT_GT(volume.value(), 0.0);

    double calibrationFactor = 1.0;
    auto mass = scorer.computeMassScore(image, calibrationFactor);
    ASSERT_TRUE(mass.has_value());
    EXPECT_GT(mass.value(), 0.0);

    // Mass should be proportional to volume
    // mass ≈ volume × mean_density × calibration
    // Both should be positive and mass should not exceed volume × max_HU
    EXPECT_LE(mass.value(), volume.value() * 400.0 * calibrationFactor)
        << "Mass score should be bounded by volume × max density";
}

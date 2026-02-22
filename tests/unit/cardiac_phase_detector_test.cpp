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
#include "services/cardiac/cardiac_phase_detector.hpp"
#include "services/enhanced_dicom/enhanced_dicom_types.hpp"
#include "core/dicom_loader.hpp"

#include <cmath>
#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;

// =============================================================================
// CardiacTypes tests
// =============================================================================

TEST(CardiacTypesTest, PhaseTargetEnumValues) {
    EXPECT_NE(static_cast<int>(PhaseTarget::Diastole),
              static_cast<int>(PhaseTarget::Systole));
    EXPECT_NE(static_cast<int>(PhaseTarget::Systole),
              static_cast<int>(PhaseTarget::Custom));
}

TEST(CardiacTypesTest, CardiacPhaseInfoDefaults) {
    CardiacPhaseInfo info;
    EXPECT_EQ(info.phaseIndex, 0);
    EXPECT_DOUBLE_EQ(info.triggerTime, 0.0);
    EXPECT_DOUBLE_EQ(info.nominalPercentage, 0.0);
    EXPECT_TRUE(info.phaseLabel.empty());
    EXPECT_TRUE(info.frameIndices.empty());
}

TEST(CardiacTypesTest, CardiacPhaseInfoDiastolicCheck) {
    CardiacPhaseInfo info;
    info.nominalPercentage = 75.0;
    EXPECT_TRUE(info.isDiastolic());
    EXPECT_FALSE(info.isSystolic());
}

TEST(CardiacTypesTest, CardiacPhaseInfoSystolicCheck) {
    CardiacPhaseInfo info;
    info.nominalPercentage = 40.0;
    EXPECT_FALSE(info.isDiastolic());
    EXPECT_TRUE(info.isSystolic());
}

TEST(CardiacTypesTest, CardiacPhaseInfoBoundary) {
    CardiacPhaseInfo info;
    // At exactly 50%, isDiastolic should be true (>= 50)
    info.nominalPercentage = 50.0;
    EXPECT_TRUE(info.isDiastolic());
    EXPECT_FALSE(info.isSystolic());
}

TEST(CardiacTypesTest, CardiacPhaseResultDefaults) {
    CardiacPhaseResult result;
    EXPECT_EQ(result.bestDiastolicPhase, -1);
    EXPECT_EQ(result.bestSystolicPhase, -1);
    EXPECT_DOUBLE_EQ(result.rrInterval, 0.0);
    EXPECT_EQ(result.slicesPerPhase, 0);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.phaseCount(), 0);
}

TEST(CardiacTypesTest, CardiacPhaseResultValid) {
    CardiacPhaseResult result;
    result.phases.push_back(CardiacPhaseInfo{});
    result.slicesPerPhase = 20;
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.phaseCount(), 1);
}

TEST(CardiacTypesTest, CardiacErrorCodes) {
    CardiacError err;
    EXPECT_TRUE(err.isSuccess());

    CardiacError notGated{CardiacError::Code::NotCardiacGated, "test"};
    EXPECT_FALSE(notGated.isSuccess());
    EXPECT_NE(notGated.toString().find("Not a cardiac-gated"), std::string::npos);

    CardiacError insufficient{CardiacError::Code::InsufficientPhases, "test"};
    EXPECT_NE(insufficient.toString().find("Insufficient"), std::string::npos);

    CardiacError missing{CardiacError::Code::MissingTemporalData, "test"};
    EXPECT_NE(missing.toString().find("Missing temporal"), std::string::npos);

    CardiacError inconsistent{CardiacError::Code::InconsistentFrameCount, "test"};
    EXPECT_NE(inconsistent.toString().find("Inconsistent"), std::string::npos);

    CardiacError assembly{CardiacError::Code::VolumeAssemblyFailed, "test"};
    EXPECT_NE(assembly.toString().find("Volume assembly"), std::string::npos);

    CardiacError internal{CardiacError::Code::InternalError, "test"};
    EXPECT_NE(internal.toString().find("Internal error"), std::string::npos);
}

TEST(CardiacTypesTest, CardiacTagConstants) {
    EXPECT_EQ(cardiac_tag::TriggerTime, 0x00181060u);
    EXPECT_EQ(cardiac_tag::CardiacSyncTechnique, 0x00189037u);
    EXPECT_EQ(cardiac_tag::NominalPercentage, 0x00189241u);
    EXPECT_EQ(cardiac_tag::LowRRValue, 0x00181081u);
    EXPECT_EQ(cardiac_tag::HighRRValue, 0x00181082u);
    EXPECT_EQ(cardiac_tag::IntervalsAcquired, 0x00181083u);
    EXPECT_EQ(cardiac_tag::HeartRate, 0x00181088u);
}

TEST(CardiacTypesTest, CardiacConstants) {
    EXPECT_DOUBLE_EQ(cardiac_constants::kDiastoleRangeMin, 70.0);
    EXPECT_DOUBLE_EQ(cardiac_constants::kDiastoleRangeMax, 80.0);
    EXPECT_DOUBLE_EQ(cardiac_constants::kDiastoleOptimal, 75.0);
    EXPECT_DOUBLE_EQ(cardiac_constants::kSystoleRangeMin, 35.0);
    EXPECT_DOUBLE_EQ(cardiac_constants::kSystoleRangeMax, 45.0);
    EXPECT_DOUBLE_EQ(cardiac_constants::kSystoleOptimal, 40.0);
    EXPECT_DOUBLE_EQ(cardiac_constants::kTriggerTimeToleranceMs, 10.0);
}

// =============================================================================
// CardiacPhaseDetector construction tests
// =============================================================================

TEST(CardiacPhaseDetectorTest, DefaultConstruction) {
    CardiacPhaseDetector detector;
    // Should construct without throwing
}

TEST(CardiacPhaseDetectorTest, MoveConstruction) {
    CardiacPhaseDetector detector;
    CardiacPhaseDetector moved(std::move(detector));
    // Should move without throwing
}

TEST(CardiacPhaseDetectorTest, MoveAssignment) {
    CardiacPhaseDetector detector;
    CardiacPhaseDetector other;
    other = std::move(detector);
    // Should move-assign without throwing
}

// =============================================================================
// ECG gating detection tests
// =============================================================================

TEST(CardiacPhaseDetectorTest, DetectECGGatingWithTriggerTime) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Add frames with trigger time
    for (int i = 0; i < 10; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.triggerTime = i * 80.0;  // 80ms apart
        series.frames.push_back(frame);
    }

    EXPECT_TRUE(detector.detectECGGating(series));
}

TEST(CardiacPhaseDetectorTest, DetectECGGatingWithTemporalIndex) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    for (int i = 0; i < 10; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.temporalPositionIndex = i / 5;  // 2 phases
        series.frames.push_back(frame);
    }

    EXPECT_TRUE(detector.detectECGGating(series));
}

TEST(CardiacPhaseDetectorTest, DetectECGGatingWithNominalPercentage) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    for (int i = 0; i < 10; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.dimensionIndices[cardiac_tag::NominalPercentage] = (i / 5) * 50;
        series.frames.push_back(frame);
    }

    EXPECT_TRUE(detector.detectECGGating(series));
}

TEST(CardiacPhaseDetectorTest, DetectECGGatingNegative) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Frames without any temporal information
    for (int i = 0; i < 10; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.imagePosition = {0.0, 0.0, static_cast<double>(i)};
        series.frames.push_back(frame);
    }

    EXPECT_FALSE(detector.detectECGGating(series));
}

TEST(CardiacPhaseDetectorTest, DetectECGGatingEmptySeries) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;
    EXPECT_FALSE(detector.detectECGGating(series));
}

TEST(CardiacPhaseDetectorTest, DetectECGGatingClassicEmpty) {
    CardiacPhaseDetector detector;
    std::vector<dicom_viewer::core::DicomMetadata> classic;
    EXPECT_FALSE(detector.detectECGGating(classic));
}

// =============================================================================
// Phase separation tests
// =============================================================================

TEST(CardiacPhaseDetectorTest, SeparatePhasesEmpty) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;
    auto result = detector.separatePhases(series);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::MissingTemporalData);
}

TEST(CardiacPhaseDetectorTest, SeparatePhasesByTriggerTime) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Simulate 10 phases x 5 slices = 50 frames
    // R-R interval ~800ms, 10 phases → 80ms per phase
    int numPhases = 10;
    int slicesPerPhase = 5;
    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.triggerTime = phase * 80.0;  // Same trigger time within phase
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 3.0};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = static_cast<int>(series.frames.size());

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());

    auto& phaseResult = result.value();
    EXPECT_EQ(phaseResult.phaseCount(), numPhases);
    EXPECT_EQ(phaseResult.slicesPerPhase, slicesPerPhase);
    EXPECT_TRUE(phaseResult.isValid());

    // Check that phases are sorted by trigger time
    for (int i = 1; i < phaseResult.phaseCount(); ++i) {
        EXPECT_GT(phaseResult.phases[i].triggerTime,
                  phaseResult.phases[i - 1].triggerTime);
    }

    // Best phases should be selected
    EXPECT_GE(phaseResult.bestDiastolicPhase, 0);
    EXPECT_GE(phaseResult.bestSystolicPhase, 0);
}

TEST(CardiacPhaseDetectorTest, SeparatePhasesByNominalPercentage) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // 10 phases at 0%, 10%, ..., 90%
    int numPhases = 10;
    int slicesPerPhase = 3;
    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.dimensionIndices[cardiac_tag::NominalPercentage] =
                phase * 10;  // 0, 10, 20, ..., 90
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 2.5};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = static_cast<int>(series.frames.size());

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());

    auto& phaseResult = result.value();
    EXPECT_EQ(phaseResult.phaseCount(), numPhases);
    EXPECT_EQ(phaseResult.slicesPerPhase, slicesPerPhase);

    // Check nominal percentages
    EXPECT_NEAR(phaseResult.phases[0].nominalPercentage, 0.0, 1.0);
    EXPECT_NEAR(phaseResult.phases[7].nominalPercentage, 70.0, 1.0);

    // Each phase should have the correct frame count
    for (const auto& phase : phaseResult.phases) {
        EXPECT_EQ(static_cast<int>(phase.frameIndices.size()), slicesPerPhase);
    }
}

TEST(CardiacPhaseDetectorTest, SeparatePhasesByTemporalIndex) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Use temporal position index (no trigger time, no nominal percentage)
    int numPhases = 5;
    int slicesPerPhase = 4;
    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.temporalPositionIndex = phase;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 2.0};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = static_cast<int>(series.frames.size());

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());

    auto& phaseResult = result.value();
    EXPECT_EQ(phaseResult.phaseCount(), numPhases);
    EXPECT_EQ(phaseResult.slicesPerPhase, slicesPerPhase);
}

TEST(CardiacPhaseDetectorTest, SeparatePhasesNoTemporalData) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Frames without any temporal information
    for (int i = 0; i < 20; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.imagePosition = {0.0, 0.0, static_cast<double>(i) * 2.0};
        series.frames.push_back(frame);
    }
    series.numberOfFrames = 20;

    auto result = detector.separatePhases(series);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::NotCardiacGated);
}

TEST(CardiacPhaseDetectorTest, SeparatePhasesSinglePhase) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // All frames have the same trigger time → 1 phase → not enough
    for (int i = 0; i < 10; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.triggerTime = 100.0;  // All same
        frame.imagePosition = {0.0, 0.0, static_cast<double>(i)};
        series.frames.push_back(frame);
    }
    series.numberOfFrames = 10;

    auto result = detector.separatePhases(series);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Best phase selection tests
// =============================================================================

TEST(CardiacPhaseDetectorTest, SelectBestPhaseDiastole) {
    CardiacPhaseDetector detector;
    CardiacPhaseResult result;

    // Create phases at 0%, 10%, ..., 90%
    for (int i = 0; i < 10; ++i) {
        CardiacPhaseInfo phase;
        phase.phaseIndex = i;
        phase.nominalPercentage = i * 10.0;
        result.phases.push_back(phase);
    }

    int best = detector.selectBestPhase(result, PhaseTarget::Diastole);
    // 75% optimal → closest is 70% (index 7) or 80% (index 8)
    EXPECT_TRUE(best == 7 || best == 8);
}

TEST(CardiacPhaseDetectorTest, SelectBestPhaseSystole) {
    CardiacPhaseDetector detector;
    CardiacPhaseResult result;

    for (int i = 0; i < 10; ++i) {
        CardiacPhaseInfo phase;
        phase.phaseIndex = i;
        phase.nominalPercentage = i * 10.0;
        result.phases.push_back(phase);
    }

    int best = detector.selectBestPhase(result, PhaseTarget::Systole);
    // 40% optimal → index 4
    EXPECT_EQ(best, 4);
}

TEST(CardiacPhaseDetectorTest, SelectBestPhaseCustom) {
    CardiacPhaseDetector detector;
    CardiacPhaseResult result;

    for (int i = 0; i < 10; ++i) {
        CardiacPhaseInfo phase;
        phase.phaseIndex = i;
        phase.nominalPercentage = i * 10.0;
        result.phases.push_back(phase);
    }

    int best = detector.selectBestPhase(result, PhaseTarget::Custom, 55.0);
    // 55% → closest is 50% (index 5) or 60% (index 6)
    EXPECT_TRUE(best == 5 || best == 6);
}

TEST(CardiacPhaseDetectorTest, SelectBestPhaseEmpty) {
    CardiacPhaseDetector detector;
    CardiacPhaseResult result;
    int best = detector.selectBestPhase(result, PhaseTarget::Diastole);
    EXPECT_EQ(best, -1);
}

// =============================================================================
// Ejection fraction estimation tests
// =============================================================================

TEST(CardiacPhaseDetectorTest, EstimateEjectionFractionNullPointers) {
    CardiacPhaseDetector detector;
    auto result = detector.estimateEjectionFraction(nullptr, nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::InternalError);
}

TEST(CardiacPhaseDetectorTest, EstimateEjectionFractionBasic) {
    CardiacPhaseDetector detector;

    // Create two small test volumes
    using ImageType = itk::Image<short, 3>;
    auto edImage = ImageType::New();
    auto esImage = ImageType::New();

    ImageType::RegionType region;
    ImageType::IndexType start = {{0, 0, 0}};
    ImageType::SizeType size = {{10, 10, 10}};
    region.SetIndex(start);
    region.SetSize(size);

    edImage->SetRegions(region);
    edImage->Allocate();
    const double spacing[3] = {1.0, 1.0, 1.0};
    edImage->SetSpacing(spacing);

    esImage->SetRegions(region);
    esImage->Allocate();
    esImage->SetSpacing(spacing);

    // Fill ED volume: all voxels above threshold (simulate large blood pool)
    edImage->FillBuffer(300);

    // Fill ES volume: fewer voxels above threshold
    esImage->FillBuffer(100);  // All below threshold of 200

    auto result = detector.estimateEjectionFraction(edImage, esImage, 200);
    ASSERT_TRUE(result.has_value());
    // ED has 1000 voxels above threshold, ES has 0 → EF = 100%
    EXPECT_NEAR(result.value(), 100.0, 0.1);
}

TEST(CardiacPhaseDetectorTest, EstimateEjectionFractionPartial) {
    CardiacPhaseDetector detector;

    using ImageType = itk::Image<short, 3>;
    auto edImage = ImageType::New();
    auto esImage = ImageType::New();

    ImageType::RegionType region;
    ImageType::IndexType start = {{0, 0, 0}};
    ImageType::SizeType size = {{10, 10, 10}};
    region.SetIndex(start);
    region.SetSize(size);

    edImage->SetRegions(region);
    edImage->Allocate();
    const double spacing[3] = {1.0, 1.0, 1.0};
    edImage->SetSpacing(spacing);

    esImage->SetRegions(region);
    esImage->Allocate();
    esImage->SetSpacing(spacing);

    // ED: all above threshold
    edImage->FillBuffer(300);

    // ES: half above threshold (simulating ~50% EF)
    esImage->FillBuffer(300);
    // Set half the voxels below threshold
    itk::ImageRegionIterator<ImageType> it(esImage,
                                           esImage->GetLargestPossibleRegion());
    int count = 0;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (count % 2 == 0) {
            it.Set(100);  // Below threshold
        }
        ++count;
    }

    auto result = detector.estimateEjectionFraction(edImage, esImage, 200);
    ASSERT_TRUE(result.has_value());
    // ED = 1000 voxels, ES = 500 voxels → EF = (1000-500)/1000 * 100 = 50%
    EXPECT_NEAR(result.value(), 50.0, 1.0);
}

TEST(CardiacPhaseDetectorTest, EstimateEjectionFractionZeroEDV) {
    CardiacPhaseDetector detector;

    using ImageType = itk::Image<short, 3>;
    auto edImage = ImageType::New();
    auto esImage = ImageType::New();

    ImageType::RegionType region;
    ImageType::IndexType start = {{0, 0, 0}};
    ImageType::SizeType size = {{5, 5, 5}};
    region.SetIndex(start);
    region.SetSize(size);

    edImage->SetRegions(region);
    edImage->Allocate();
    const double spacing[3] = {1.0, 1.0, 1.0};
    edImage->SetSpacing(spacing);
    edImage->FillBuffer(0);  // All below threshold

    esImage->SetRegions(region);
    esImage->Allocate();
    esImage->SetSpacing(spacing);
    esImage->FillBuffer(0);

    auto result = detector.estimateEjectionFraction(edImage, esImage, 200);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::InternalError);
}

// =============================================================================
// Phase label and R-R interval tests
// =============================================================================

TEST(CardiacPhaseDetectorTest, PhaseLabelGeneration) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // 2 phases: 0% and 75%
    for (int phase = 0; phase < 2; ++phase) {
        for (int slice = 0; slice < 3; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * 3 + slice;
            frame.dimensionIndices[cardiac_tag::NominalPercentage] =
                phase == 0 ? 0 : 75;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice)};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = 6;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());

    auto& phaseResult = result.value();
    EXPECT_EQ(phaseResult.phaseCount(), 2);
    // First phase at 0% should have "systole" in label
    EXPECT_NE(phaseResult.phases[0].phaseLabel.find("systole"),
              std::string::npos);
    // Second phase at 75% should have "diastole" in label
    EXPECT_NE(phaseResult.phases[1].phaseLabel.find("diastole"),
              std::string::npos);
}

TEST(CardiacPhaseDetectorTest, RRIntervalEstimation) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // 10 phases, trigger times 0, 80, 160, ..., 720ms
    // → R-R ≈ 720 * 10/9 ≈ 800ms
    int numPhases = 10;
    int slicesPerPhase = 2;
    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.triggerTime = phase * 80.0;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 3.0};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = numPhases * slicesPerPhase;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());
    // R-R interval should be approximately 800ms
    EXPECT_NEAR(result.value().rrInterval, 800.0, 10.0);
}

// =============================================================================
// Spatial ordering within phases
// =============================================================================

TEST(CardiacPhaseDetectorTest, SpatialOrderingWithinPhase) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Create frames where spatial order is reversed within each phase
    int numPhases = 2;
    int slicesPerPhase = 4;
    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = slicesPerPhase - 1; slice >= 0; --slice) {
            // Add frames in reverse spatial order
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + (slicesPerPhase - 1 - slice);
            frame.triggerTime = phase * 400.0;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 2.5};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = numPhases * slicesPerPhase;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());

    // Each phase's frames should be sorted by z-position (ascending)
    for (const auto& phase : result.value().phases) {
        EXPECT_EQ(static_cast<int>(phase.frameIndices.size()), slicesPerPhase);
        // Verify spatial ordering by checking z-positions
        double prevZ = -1e10;
        for (int idx : phase.frameIndices) {
            for (const auto& f : series.frames) {
                if (f.frameIndex == idx) {
                    EXPECT_GE(f.imagePosition[2], prevZ);
                    prevZ = f.imagePosition[2];
                    break;
                }
            }
        }
    }
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(CardiacPhaseDetectorTest, TriggerTimeClustering) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Create frames with slightly varying trigger times (within tolerance)
    // Phase 1: trigger times around 0ms
    // Phase 2: trigger times around 400ms
    for (int i = 0; i < 5; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.triggerTime = 0.0 + i * 1.5;  // 0, 1.5, 3, 4.5, 6ms
        frame.imagePosition = {0.0, 0.0, static_cast<double>(i) * 2.0};
        series.frames.push_back(frame);
    }
    for (int i = 0; i < 5; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = 5 + i;
        frame.triggerTime = 400.0 + i * 1.5;  // 400, 401.5, ...
        frame.imagePosition = {0.0, 0.0, static_cast<double>(i) * 2.0};
        series.frames.push_back(frame);
    }
    series.numberOfFrames = 10;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().phaseCount(), 2);
    EXPECT_EQ(result.value().slicesPerPhase, 5);
}

TEST(CardiacPhaseDetectorTest, MixedTemporalAndDimensionIndex) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Frames with temporal position index in dimensionIndices map
    int numPhases = 3;
    int slicesPerPhase = 4;
    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.dimensionIndices[dimension_tag::TemporalPositionIndex] = phase;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 2.0};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = numPhases * slicesPerPhase;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().phaseCount(), numPhases);
}

// =============================================================================
// Tolerance validation and artifact handling tests (Issue #208)
// =============================================================================

TEST(CardiacPhaseDetectorTest, ArrhythmiaIrregularPhaseSpacing) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Simulate irregular R-R intervals (arrhythmia)
    // Normal phases at 0, 80, 160ms but with irregular spacing
    // Phase 0: ~0ms, Phase 1: ~90ms (long), Phase 2: ~150ms (short)
    double irregularTriggers[] = {0.0, 90.0, 150.0, 240.0, 340.0};
    int numPhases = 5;
    int slicesPerPhase = 4;

    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.triggerTime = irregularTriggers[phase];
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 3.0};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = numPhases * slicesPerPhase;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value())
        << "Phase separation should succeed despite irregular R-R intervals";
    EXPECT_EQ(result.value().phaseCount(), numPhases);
}

TEST(CardiacPhaseDetectorTest, VeryFastHeartRateNarrowPhases) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // HR ~150 bpm → R-R ~400ms, 10 phases → 40ms spacing
    int numPhases = 10;
    int slicesPerPhase = 3;
    double phaseInterval = 40.0;  // Very narrow phase spacing

    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.triggerTime = phase * phaseInterval;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 3.0};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = numPhases * slicesPerPhase;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value())
        << "Phase separation should handle fast heart rates (>120 bpm)";
    EXPECT_EQ(result.value().phaseCount(), numPhases);
    EXPECT_NEAR(result.value().rrInterval, 400.0, 50.0);
}

TEST(CardiacPhaseDetectorTest, VerySlowHeartRateWidePhases) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // HR ~35 bpm → R-R ~1714ms, 8 phases → ~214ms spacing
    int numPhases = 8;
    int slicesPerPhase = 3;
    double phaseInterval = 214.0;

    for (int phase = 0; phase < numPhases; ++phase) {
        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * slicesPerPhase + slice;
            frame.triggerTime = phase * phaseInterval;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 3.0};
            series.frames.push_back(frame);
        }
    }
    series.numberOfFrames = numPhases * slicesPerPhase;

    auto result = detector.separatePhases(series);
    ASSERT_TRUE(result.has_value())
        << "Phase separation should handle slow heart rates (<40 bpm)";
    EXPECT_EQ(result.value().phaseCount(), numPhases);
    EXPECT_GT(result.value().rrInterval, 1500.0);
}

TEST(CardiacPhaseDetectorTest, IncompletePhaseWithFewFrames) {
    CardiacPhaseDetector detector;
    EnhancedSeriesInfo series;

    // Phase 0 and 1 have 5 slices, Phase 2 has only 1 slice (incomplete)
    int fullPhaseSlices = 5;
    for (int phase = 0; phase < 2; ++phase) {
        for (int slice = 0; slice < fullPhaseSlices; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = static_cast<int>(series.frames.size());
            frame.triggerTime = phase * 400.0;
            frame.imagePosition = {0.0, 0.0, static_cast<double>(slice) * 3.0};
            series.frames.push_back(frame);
        }
    }
    // Incomplete phase with only 1 frame
    {
        EnhancedFrameInfo frame;
        frame.frameIndex = static_cast<int>(series.frames.size());
        frame.triggerTime = 800.0;
        frame.imagePosition = {0.0, 0.0, 0.0};
        series.frames.push_back(frame);
    }
    series.numberOfFrames = static_cast<int>(series.frames.size());

    // Should either succeed with variable frame counts or fail gracefully
    auto result = detector.separatePhases(series);
    if (result.has_value()) {
        EXPECT_GE(result.value().phaseCount(), 2);
    } else {
        EXPECT_EQ(result.error().code,
                  CardiacError::Code::InconsistentFrameCount);
    }
}

TEST(CardiacPhaseDetectorTest, PhaseBoundaryNearZeroAndHundredPercent) {
    CardiacPhaseDetector detector;
    CardiacPhaseResult phaseResult;

    // Create phases at boundary percentages: 0%, 50%, and 99%
    CardiacPhaseInfo p0;
    p0.phaseIndex = 0;
    p0.nominalPercentage = 0.0;
    phaseResult.phases.push_back(p0);

    CardiacPhaseInfo p1;
    p1.phaseIndex = 1;
    p1.nominalPercentage = 50.0;
    phaseResult.phases.push_back(p1);

    CardiacPhaseInfo p2;
    p2.phaseIndex = 2;
    p2.nominalPercentage = 99.0;
    phaseResult.phases.push_back(p2);

    // Diastole optimal ~75% → closest is 50% (index 1) or 99% (index 2)
    int bestDiastole = detector.selectBestPhase(phaseResult, PhaseTarget::Diastole);
    EXPECT_GE(bestDiastole, 0);
    EXPECT_LT(bestDiastole, 3);

    // Systole optimal ~40% → closest is 50% (index 1)
    int bestSystole = detector.selectBestPhase(phaseResult, PhaseTarget::Systole);
    EXPECT_EQ(bestSystole, 1);
}

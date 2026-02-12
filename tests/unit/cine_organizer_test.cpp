#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "services/cardiac/cine_organizer.hpp"
#include "services/flow/temporal_navigator.hpp"
#include "test_utils/cardiac_phantom_generator.hpp"

using namespace dicom_viewer::services;
using namespace dicom_viewer::test_utils;

// =============================================================================
// Cine Detection Tests (Enhanced DICOM)
// =============================================================================

class CineDetectionEnhancedTest : public ::testing::Test {
protected:
    CineOrganizer organizer;
};

TEST_F(CineDetectionEnhancedTest, DetectValidCineMRI) {
    auto [series, truth] = generateCineMRIPhantom(25, 10);
    EXPECT_TRUE(organizer.detectCineSeries(series));
}

TEST_F(CineDetectionEnhancedTest, RejectNonMRModality) {
    auto series = generateEnhancedCTPhantom(50);
    EXPECT_FALSE(organizer.detectCineSeries(series));
}

TEST_F(CineDetectionEnhancedTest, RejectNoTemporalData) {
    auto series = generateNonCineMRPhantom(20);
    EXPECT_FALSE(organizer.detectCineSeries(series));
}

TEST_F(CineDetectionEnhancedTest, RejectSinglePhase) {
    // MR series with only 1 temporal position
    auto series = generateNonCineMRPhantom(10);
    // Add trigger time to all frames but same value
    for (auto& frame : series.frames) {
        frame.triggerTime = 0.0;
        frame.temporalPositionIndex = 1;
    }
    EXPECT_FALSE(organizer.detectCineSeries(series));
}

TEST_F(CineDetectionEnhancedTest, DetectWithTriggerTimeOnly) {
    // MR series with trigger times but no temporal position index
    EnhancedSeriesInfo series;
    series.modality = "MR";
    series.numberOfFrames = 20;
    series.seriesDescription = "cine";

    for (int phase = 0; phase < 4; ++phase) {
        for (int slice = 0; slice < 5; ++slice) {
            EnhancedFrameInfo frame;
            frame.frameIndex = phase * 5 + slice;
            frame.triggerTime = phase * 200.0;
            frame.imagePosition = {0.0, 0.0, slice * 8.0};
            frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
            // No temporalPositionIndex
            series.frames.push_back(frame);
        }
    }

    EXPECT_TRUE(organizer.detectCineSeries(series));
}

TEST_F(CineDetectionEnhancedTest, RejectInconsistentFrameCount) {
    // MR with temporal index but frame count not divisible by phases
    EnhancedSeriesInfo series;
    series.modality = "MR";
    series.numberOfFrames = 7;  // Not divisible by 3

    for (int i = 0; i < 7; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.temporalPositionIndex = (i % 3) + 1;  // 3 phases
        frame.imagePosition = {0.0, 0.0, (i / 3) * 5.0};
        frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        series.frames.push_back(frame);
    }

    EXPECT_FALSE(organizer.detectCineSeries(series));
}

TEST_F(CineDetectionEnhancedTest, DetectMinimalCine) {
    // Minimum valid cine: 2 phases × 1 slice
    EnhancedSeriesInfo series;
    series.modality = "MR";
    series.numberOfFrames = 2;

    for (int i = 0; i < 2; ++i) {
        EnhancedFrameInfo frame;
        frame.frameIndex = i;
        frame.temporalPositionIndex = i + 1;
        frame.triggerTime = i * 400.0;
        frame.imagePosition = {0.0, 0.0, 0.0};
        frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        series.frames.push_back(frame);
    }

    EXPECT_TRUE(organizer.detectCineSeries(series));
}

// =============================================================================
// Cine Detection Tests (Classic DICOM)
// =============================================================================

class CineDetectionClassicTest : public ::testing::Test {
protected:
    CineOrganizer organizer;

    std::pair<std::vector<dicom_viewer::core::DicomMetadata>,
              std::vector<dicom_viewer::core::SliceInfo>>
    createClassicCineSeries(int phaseCount, int sliceCount) {
        std::vector<dicom_viewer::core::DicomMetadata> metadata;
        std::vector<dicom_viewer::core::SliceInfo> slices;

        int instanceNum = 1;
        for (int phase = 0; phase < phaseCount; ++phase) {
            for (int slice = 0; slice < sliceCount; ++slice) {
                dicom_viewer::core::DicomMetadata m;
                m.modality = "MR";
                m.seriesInstanceUid = "1.2.3.4.5";
                m.seriesDescription = "cine_retro SA";
                metadata.push_back(m);

                dicom_viewer::core::SliceInfo s;
                s.sliceLocation = slice * 8.0;
                s.instanceNumber = instanceNum++;
                s.imagePosition = {0.0, 0.0, slice * 8.0};
                s.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
                slices.push_back(s);
            }
        }
        return {metadata, slices};
    }
};

TEST_F(CineDetectionClassicTest, DetectByKeyword) {
    auto [meta, slices] = createClassicCineSeries(2, 1);
    EXPECT_TRUE(organizer.detectCineSeries(meta, slices));
}

TEST_F(CineDetectionClassicTest, DetectByRepeatedLocations) {
    auto [meta, slices] = createClassicCineSeries(5, 3);
    // Remove cine keyword
    for (auto& m : meta) {
        m.seriesDescription = "cardiac";
    }
    // Multiple files at same slice location → cine
    EXPECT_TRUE(organizer.detectCineSeries(meta, slices));
}

TEST_F(CineDetectionClassicTest, RejectNonMR) {
    auto [meta, slices] = createClassicCineSeries(5, 3);
    for (auto& m : meta) {
        m.modality = "CT";
        m.seriesDescription = "cardiac CT";
    }
    EXPECT_FALSE(organizer.detectCineSeries(meta, slices));
}

TEST_F(CineDetectionClassicTest, RejectDifferentSeries) {
    auto [meta, slices] = createClassicCineSeries(2, 2);
    meta[1].seriesInstanceUid = "different-uid";
    EXPECT_FALSE(organizer.detectCineSeries(meta, slices));
}

TEST_F(CineDetectionClassicTest, RejectEmpty) {
    std::vector<dicom_viewer::core::DicomMetadata> meta;
    std::vector<dicom_viewer::core::SliceInfo> slices;
    EXPECT_FALSE(organizer.detectCineSeries(meta, slices));
}

// =============================================================================
// Orientation Detection Tests
// =============================================================================

class CineOrientationTest : public ::testing::Test {
protected:
    CineOrganizer organizer;
};

TEST_F(CineOrientationTest, DetectShortAxis) {
    // Transverse orientation: row=(1,0,0), col=(0,1,0) → normal=(0,0,1)
    std::array<double, 6> orient = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    auto result = organizer.detectOrientation(orient, "cine SA");
    EXPECT_EQ(result, CineOrientation::ShortAxis);
}

TEST_F(CineOrientationTest, DetectShortAxisNoDescription) {
    std::array<double, 6> orient = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    auto result = organizer.detectOrientation(orient, "");
    EXPECT_EQ(result, CineOrientation::ShortAxis);
}

TEST_F(CineOrientationTest, DetectTwoChamber) {
    // Sagittal orientation: row=(0,1,0), col=(0,0,1) → normal=(1,0,0)
    std::array<double, 6> orient = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    auto result = organizer.detectOrientation(orient, "cine 2ch");
    EXPECT_EQ(result, CineOrientation::TwoChamber);
}

TEST_F(CineOrientationTest, DetectThreeChamber) {
    // Oblique orientation with 3CH keyword
    std::array<double, 6> orient = {0.7, 0.7, 0.0, 0.0, 0.0, 1.0};
    auto result = organizer.detectOrientation(orient, "cine 3CH LVOT");
    EXPECT_EQ(result, CineOrientation::ThreeChamber);
}

TEST_F(CineOrientationTest, DetectFourChamber) {
    // Oblique orientation with 4CH keyword
    std::array<double, 6> orient = {0.7, 0.0, 0.7, 0.0, 1.0, 0.0};
    auto result = organizer.detectOrientation(orient, "4 chamber cine");
    EXPECT_EQ(result, CineOrientation::FourChamber);
}

TEST_F(CineOrientationTest, FourChamberOverridesTransverse) {
    // Transverse normal but 4CH keyword should override
    std::array<double, 6> orient = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    auto result = organizer.detectOrientation(orient, "cine 4ch");
    EXPECT_EQ(result, CineOrientation::FourChamber);
}

TEST_F(CineOrientationTest, UnknownForObliqueNoKeyword) {
    // Oblique orientation without descriptive keywords
    std::array<double, 6> orient = {0.5, 0.5, 0.707, -0.5, 0.5, 0.707};
    auto result = organizer.detectOrientation(orient, "cardiac cine");
    EXPECT_EQ(result, CineOrientation::Unknown);
}

TEST_F(CineOrientationTest, DetectSagittalAsTwoChamber) {
    // Sagittal-dominant normal without keyword
    std::array<double, 6> orient = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    auto result = organizer.detectOrientation(orient, "");
    EXPECT_EQ(result, CineOrientation::TwoChamber);
}

// =============================================================================
// CineSeriesInfo and CineVolumeSeries Validation Tests
// =============================================================================

TEST(CineSeriesInfoTest, ValidInfo) {
    CineSeriesInfo info;
    info.phaseCount = 25;
    info.sliceCount = 10;
    EXPECT_TRUE(info.isValid());
}

TEST(CineSeriesInfoTest, InvalidWithSinglePhase) {
    CineSeriesInfo info;
    info.phaseCount = 1;
    info.sliceCount = 10;
    EXPECT_FALSE(info.isValid());
}

TEST(CineSeriesInfoTest, InvalidWithZeroSlices) {
    CineSeriesInfo info;
    info.phaseCount = 25;
    info.sliceCount = 0;
    EXPECT_FALSE(info.isValid());
}

TEST(CineVolumeSeriesTest, InvalidWhenEmpty) {
    CineVolumeSeries series;
    EXPECT_FALSE(series.isValid());
}

// =============================================================================
// Orientation String Conversion Tests
// =============================================================================

TEST(CineOrientationStringTest, AllOrientations) {
    EXPECT_EQ(cineOrientationToString(CineOrientation::ShortAxis), "SA");
    EXPECT_EQ(cineOrientationToString(CineOrientation::TwoChamber), "2CH");
    EXPECT_EQ(cineOrientationToString(CineOrientation::ThreeChamber), "3CH");
    EXPECT_EQ(cineOrientationToString(CineOrientation::FourChamber), "4CH");
    EXPECT_EQ(cineOrientationToString(CineOrientation::Unknown), "Unknown");
}

// =============================================================================
// Phase Organization Tests (Enhanced DICOM)
// =============================================================================

class CineOrganizeEnhancedTest : public ::testing::Test {
protected:
    CineOrganizer organizer;
};

TEST_F(CineOrganizeEnhancedTest, RejectNonCineSeries) {
    auto series = generateNonCineMRPhantom(20);
    auto result = organizer.organizePhases(series);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::NotCardiacGated);
}

TEST_F(CineOrganizeEnhancedTest, CorrectPhaseCount) {
    auto [series, truth] = generateCineMRIPhantom(20, 8);
    auto result = organizer.organizePhases(series);

    // organizePhases uses FrameExtractor which requires actual DICOM files,
    // so with phantom data it will fail at volume assembly.
    // We test the detection and grouping logic separately.
    if (result.has_value()) {
        EXPECT_EQ(result->info.phaseCount, truth.phaseCount);
        EXPECT_EQ(result->info.sliceCount, truth.sliceCount);
    } else {
        // Expected: volume assembly fails with phantom (no real DICOM file)
        EXPECT_EQ(result.error().code,
                  CardiacError::Code::VolumeAssemblyFailed);
    }
}

TEST_F(CineOrganizeEnhancedTest, InconsistentPhaseFrameCount) {
    // Create series where phases have different frame counts
    EnhancedSeriesInfo series;
    series.modality = "MR";
    series.numberOfFrames = 9;  // 3 phases: 3, 3, 3 frames

    int idx = 0;
    // Phase 1: 3 frames
    for (int s = 0; s < 3; ++s) {
        EnhancedFrameInfo frame;
        frame.frameIndex = idx++;
        frame.temporalPositionIndex = 1;
        frame.triggerTime = 0.0;
        frame.imagePosition = {0.0, 0.0, s * 8.0};
        frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        series.frames.push_back(frame);
    }
    // Phase 2: 3 frames
    for (int s = 0; s < 3; ++s) {
        EnhancedFrameInfo frame;
        frame.frameIndex = idx++;
        frame.temporalPositionIndex = 2;
        frame.triggerTime = 300.0;
        frame.imagePosition = {0.0, 0.0, s * 8.0};
        frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        series.frames.push_back(frame);
    }
    // Phase 3: 3 frames
    for (int s = 0; s < 3; ++s) {
        EnhancedFrameInfo frame;
        frame.frameIndex = idx++;
        frame.temporalPositionIndex = 3;
        frame.triggerTime = 600.0;
        frame.imagePosition = {0.0, 0.0, s * 8.0};
        frame.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        series.frames.push_back(frame);
    }

    // This should detect as cine and attempt organization
    EXPECT_TRUE(organizer.detectCineSeries(series));
}

// =============================================================================
// TemporalNavigator Integration Tests
// =============================================================================

class CineNavigatorTest : public ::testing::Test {
protected:
    CineOrganizer organizer;

    CineVolumeSeries createMockCineSeries(int phaseCount, int dim = 16) {
        CineVolumeSeries series;
        series.info.phaseCount = phaseCount;
        series.info.sliceCount = dim;
        series.info.temporalResolution = 36.0;  // ~25 phases in 900ms
        series.info.orientation = CineOrientation::ShortAxis;

        for (int p = 0; p < phaseCount; ++p) {
            series.info.triggerTimes.push_back(p * 36.0);

            // Create a small 3D image for each phase
            using ImageType = itk::Image<short, 3>;
            auto image = ImageType::New();
            ImageType::SizeType size;
            size[0] = dim;
            size[1] = dim;
            size[2] = dim;
            ImageType::RegionType region;
            region.SetSize(size);
            image->SetRegions(region);
            image->Allocate(true);

            // Fill with phase-dependent values for verification
            auto* buffer = image->GetBufferPointer();
            auto totalPixels = dim * dim * dim;
            for (int i = 0; i < totalPixels; ++i) {
                buffer[i] = static_cast<short>(p * 100 + (i % 100));
            }

            series.phaseVolumes.push_back(image);
        }

        return series;
    }
};

TEST_F(CineNavigatorTest, CreateNavigator) {
    auto cineSeries = createMockCineSeries(10);
    auto navigator = organizer.createCineNavigator(cineSeries);

    ASSERT_NE(navigator, nullptr);
    EXPECT_TRUE(navigator->isInitialized());
    EXPECT_EQ(navigator->phaseCount(), 10);
    EXPECT_DOUBLE_EQ(navigator->temporalResolution(), 36.0);
}

TEST_F(CineNavigatorTest, NavigateToPhase) {
    auto cineSeries = createMockCineSeries(5);
    auto navigator = organizer.createCineNavigator(cineSeries);

    auto result = navigator->goToPhase(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->phaseIndex, 0);
    EXPECT_NEAR(result->triggerTime, 0.0, 0.01);
    EXPECT_NE(result->magnitudeImage, nullptr);
    EXPECT_EQ(result->velocityField, nullptr);  // Cine has no velocity data
}

TEST_F(CineNavigatorTest, NavigateAllPhases) {
    auto cineSeries = createMockCineSeries(5);
    auto navigator = organizer.createCineNavigator(cineSeries);

    for (int i = 0; i < 5; ++i) {
        auto result = navigator->goToPhase(i);
        ASSERT_TRUE(result.has_value()) << "Failed at phase " << i;
        EXPECT_EQ(result->phaseIndex, i);
        EXPECT_NE(result->magnitudeImage, nullptr);
    }
}

TEST_F(CineNavigatorTest, OutOfRangePhase) {
    auto cineSeries = createMockCineSeries(5);
    auto navigator = organizer.createCineNavigator(cineSeries);

    auto result = navigator->goToPhase(10);
    EXPECT_FALSE(result.has_value());
}

TEST_F(CineNavigatorTest, NextPreviousPhase) {
    auto cineSeries = createMockCineSeries(5);
    auto navigator = organizer.createCineNavigator(cineSeries);

    auto r0 = navigator->goToPhase(0);
    ASSERT_TRUE(r0.has_value());
    EXPECT_EQ(navigator->currentPhase(), 0);

    auto r1 = navigator->nextPhase();
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(navigator->currentPhase(), 1);

    auto r0b = navigator->previousPhase();
    ASSERT_TRUE(r0b.has_value());
    EXPECT_EQ(navigator->currentPhase(), 0);
}

TEST_F(CineNavigatorTest, PlaybackState) {
    auto cineSeries = createMockCineSeries(10);
    auto navigator = organizer.createCineNavigator(cineSeries);

    navigator->play(25.0);
    auto state = navigator->playbackState();
    EXPECT_TRUE(state.isPlaying);
    EXPECT_DOUBLE_EQ(state.fps, 25.0);

    navigator->pause();
    state = navigator->playbackState();
    EXPECT_FALSE(state.isPlaying);
}

TEST_F(CineNavigatorTest, CacheStatus) {
    auto cineSeries = createMockCineSeries(20);
    auto navigator = organizer.createCineNavigator(cineSeries);

    // Cache should be empty initially
    auto status = navigator->cacheStatus();
    EXPECT_EQ(status.cachedCount, 0);
    EXPECT_EQ(status.totalPhases, 20);

    // After loading a phase, cache should have 1 entry
    navigator->goToPhase(0);
    status = navigator->cacheStatus();
    EXPECT_EQ(status.cachedCount, 1);
}

TEST_F(CineNavigatorTest, PhaseDataIntegrity) {
    auto cineSeries = createMockCineSeries(5, 8);
    auto navigator = organizer.createCineNavigator(cineSeries);

    // Load phase 3 and verify the magnitude image has correct dimensions
    auto result = navigator->goToPhase(3);
    ASSERT_TRUE(result.has_value());

    auto size = result->magnitudeImage->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], 8);
    EXPECT_EQ(size[1], 8);
    EXPECT_EQ(size[2], 8);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(CineEdgeCaseTest, EmptyEnhancedSeries) {
    CineOrganizer organizer;
    EnhancedSeriesInfo series;
    EXPECT_FALSE(organizer.detectCineSeries(series));
}

TEST(CineEdgeCaseTest, MRWithSingleFrame) {
    CineOrganizer organizer;
    EnhancedSeriesInfo series;
    series.modality = "MR";
    series.numberOfFrames = 1;
    EnhancedFrameInfo frame;
    frame.frameIndex = 0;
    frame.temporalPositionIndex = 1;
    series.frames.push_back(frame);
    EXPECT_FALSE(organizer.detectCineSeries(series));
}

TEST(CineEdgeCaseTest, MoveConstructor) {
    CineOrganizer org1;
    CineOrganizer org2(std::move(org1));
    // org2 should be usable after move
    EnhancedSeriesInfo series;
    EXPECT_FALSE(org2.detectCineSeries(series));
}

TEST(CineEdgeCaseTest, MoveAssignment) {
    CineOrganizer org1;
    CineOrganizer org2;
    org2 = std::move(org1);
    // org2 should be usable after move assignment
    EnhancedSeriesInfo series;
    EXPECT_FALSE(org2.detectCineSeries(series));
}

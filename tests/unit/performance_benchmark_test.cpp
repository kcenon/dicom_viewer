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

#include "core/hounsfield_converter.hpp"
#include "services/cardiac/calcium_scorer.hpp"
#include "services/flow/flow_quantifier.hpp"
#include "services/preprocessing/anisotropic_diffusion_filter.hpp"
#include "services/preprocessing/gaussian_smoother.hpp"
#include "services/segmentation/morphological_processor.hpp"
#include "services/segmentation/region_growing_segmenter.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include "../test_utils/benchmark_fixture.hpp"
#include "../test_utils/flow_phantom_generator.hpp"
#include "../test_utils/volume_generator.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

namespace dicom_viewer::services {
namespace {

using test_utils::PerformanceBenchmark;
using test_utils::createBinaryMaskVolume;
using test_utils::createSphereVolume;
using test_utils::createSyntheticCTVolume;
using test_utils::createVolume;

// =============================================================================
// Core Module Benchmarks
// =============================================================================

class CoreBenchmarkTest : public PerformanceBenchmark {
protected:
    void SetUp() override {
        volume128_ = createSyntheticCTVolume(128);
    }

    test_utils::ShortImageType::Pointer volume128_;
};

TEST_F(CoreBenchmarkTest, HounsfieldConversion128Cube) {
    // Create a fresh volume (applyToImage modifies in-place)
    auto volume = createVolume(128);

    auto elapsed = measureTime([&] {
        core::HounsfieldConverter::applyToImage(volume, 1.0, -1024.0);
    });

    assertWithinThreshold(elapsed, 500, "HU conversion 128^3");
}

TEST_F(CoreBenchmarkTest, HounsfieldConversionScaling) {
    // Measure scaling behavior: 64^3 vs 128^3
    auto volume64 = createVolume(64);
    auto volume128 = createVolume(128);

    auto elapsed64 = measureTime([&] {
        core::HounsfieldConverter::applyToImage(volume64, 1.0, -1024.0);
    });

    auto elapsed128 = measureTime([&] {
        core::HounsfieldConverter::applyToImage(volume128, 1.0, -1024.0);
    });

    std::cout << "[BENCHMARK] HU conversion scaling: "
              << "64^3=" << elapsed64.count() << "ms, "
              << "128^3=" << elapsed128.count() << "ms" << std::endl;

    // 128^3 / 64^3 = 8x voxels, expect roughly linear scaling
    // Allow generous factor for cache effects and overhead
    if (elapsed64.count() > 0) {
        double ratio =
            static_cast<double>(elapsed128.count()) / elapsed64.count();
        EXPECT_LT(ratio, 20.0)
            << "Scaling ratio " << ratio
            << " suggests worse than O(n) complexity";
    }
}

TEST_F(CoreBenchmarkTest, VolumeCreation64Cube) {
    auto elapsed = measureTime([&] {
        auto vol = createVolume(64);
        ASSERT_NE(vol, nullptr);
    });

    assertWithinThreshold(elapsed, 200, "Volume creation 64^3");
}

TEST_F(CoreBenchmarkTest, VolumeCreation128Cube) {
    auto elapsed = measureTime([&] {
        auto vol = createVolume(128);
        ASSERT_NE(vol, nullptr);
    });

    assertWithinThreshold(elapsed, 1000, "Volume creation 128^3");
}

// =============================================================================
// Processing Benchmarks
// =============================================================================

class ProcessingBenchmarkTest : public PerformanceBenchmark {
protected:
    void SetUp() override {
        volume128_ = createSyntheticCTVolume(128);
        volume64_ = createSyntheticCTVolume(64);
    }

    test_utils::ShortImageType::Pointer volume128_;
    test_utils::ShortImageType::Pointer volume64_;
};

TEST_F(ProcessingBenchmarkTest, GaussianFilter128Cube) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 2.0;

    std::chrono::milliseconds elapsed;
    auto result =
        measureTimeWithResult([&] { return smoother.apply(volume128_, params); },
                              elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 5000, "Gaussian filter 128^3 sigma=2.0");
}

TEST_F(ProcessingBenchmarkTest, GaussianFilterScaling) {
    GaussianSmoother smoother;
    GaussianSmoother::Parameters params;
    params.variance = 1.0;

    auto elapsed64 = measureTime([&] {
        auto r = smoother.apply(volume64_, params);
        ASSERT_TRUE(r.has_value());
    });

    auto elapsed128 = measureTime([&] {
        auto r = smoother.apply(volume128_, params);
        ASSERT_TRUE(r.has_value());
    });

    std::cout << "[BENCHMARK] Gaussian scaling: "
              << "64^3=" << elapsed64.count() << "ms, "
              << "128^3=" << elapsed128.count() << "ms" << std::endl;

    if (elapsed64.count() > 0) {
        double ratio =
            static_cast<double>(elapsed128.count()) / elapsed64.count();
        EXPECT_LT(ratio, 20.0)
            << "Gaussian scaling ratio " << ratio
            << " suggests worse than O(n) complexity";
    }
}

TEST_F(ProcessingBenchmarkTest, AnisotropicDiffusion64Cube) {
    AnisotropicDiffusionFilter filter;
    AnisotropicDiffusionFilter::Parameters params;
    params.numberOfIterations = 5;
    params.conductance = 3.0;

    std::chrono::milliseconds elapsed;
    auto result =
        measureTimeWithResult([&] { return filter.apply(volume64_, params); },
                              elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 10000,
                          "Anisotropic diffusion 64^3, 5 iterations");
}

TEST_F(ProcessingBenchmarkTest, OtsuThreshold128Cube) {
    ThresholdSegmenter segmenter;

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] { return segmenter.otsuThreshold(volume128_); }, elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 3000, "Otsu threshold 128^3");
}

TEST_F(ProcessingBenchmarkTest, RegionGrowing128Cube) {
    RegionGrowingSegmenter segmenter;

    // Seed at center of the volume (inside the soft tissue region)
    std::vector<SeedPoint> seeds = {{64, 64, 64}};

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] {
            return segmenter.connectedThreshold(volume128_, seeds, -200.0,
                                                200.0);
        },
        elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 5000, "Region growing 128^3");
}

TEST_F(ProcessingBenchmarkTest, MorphologicalDilation128Cube) {
    MorphologicalProcessor processor;
    auto mask = createBinaryMaskVolume(128, 40.0);

    MorphologicalProcessor::Parameters params;
    params.radius = 3;
    params.structuringElement = StructuringElementShape::Ball;

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] {
            return processor.apply(mask, MorphologicalOperation::Dilation,
                                   params);
        },
        elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 5000,
                          "Morphological dilation 128^3 radius=3");
}

// =============================================================================
// Clinical Pipeline Benchmarks
// =============================================================================

class ClinicalBenchmarkTest : public PerformanceBenchmark {};

TEST_F(ClinicalBenchmarkTest, CalciumScoring128Cube) {
    // Create a phantom with embedded calcium lesions (HU > 130)
    auto volume = createSphereVolume(128, 5.0, 400, 30);

    CalciumScorer scorer;

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] { return scorer.computeAgatston(volume, 3.0); }, elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 10000, "Calcium scoring 128^3");
}

TEST_F(ClinicalBenchmarkTest, FlowQuantificationSinglePhase) {
    // Generate a Poiseuille pipe phantom at 32^3 (flow needs vector image)
    auto [phase, truth] =
        test_utils::generatePoiseuillePipe(32, 100.0, 10.0, 0);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {15.5, 15.5, 15.5};
    plane.normal = {0.0, 0.0, 1.0};
    plane.radius = 12.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] { return quantifier.measureFlow(phase); }, elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 500, "Flow quantification single phase 32^3");
}

TEST_F(ClinicalBenchmarkTest, FlowQuantificationMultiPhase) {
    // Generate 10-phase pulsatile flow
    std::vector<VelocityPhase> phases;
    for (int i = 0; i < 10; ++i) {
        auto [phase, truth] =
            test_utils::generatePoiseuillePipe(32, 80.0, 10.0, i);
        phases.push_back(std::move(phase));
    }

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {15.5, 15.5, 15.5};
    plane.normal = {0.0, 0.0, 1.0};
    plane.radius = 12.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] { return quantifier.computeTimeVelocityCurve(phases, 40.0); },
        elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    assertWithinThreshold(elapsed, 2000,
                          "Flow time-velocity curve 10 phases 32^3");
}

// =============================================================================
// Memory Stress Tests
// =============================================================================

class MemoryStressTest : public PerformanceBenchmark {};

TEST_F(MemoryStressTest, ConcurrentVolumeAllocation) {
    // Allocate and process 3 concurrent 64^3 volumes
    auto elapsed = measureTime([&] {
        auto vol1 = createSyntheticCTVolume(64);
        auto vol2 = createSyntheticCTVolume(64);
        auto vol3 = createSyntheticCTVolume(64);

        // Ensure all three are valid and distinct
        ASSERT_NE(vol1, nullptr);
        ASSERT_NE(vol2, nullptr);
        ASSERT_NE(vol3, nullptr);
        ASSERT_NE(vol1.GetPointer(), vol2.GetPointer());
        ASSERT_NE(vol2.GetPointer(), vol3.GetPointer());

        // Apply a processing step to each
        GaussianSmoother smoother;
        auto r1 = smoother.apply(vol1);
        auto r2 = smoother.apply(vol2);
        auto r3 = smoother.apply(vol3);

        ASSERT_TRUE(r1.has_value());
        ASSERT_TRUE(r2.has_value());
        ASSERT_TRUE(r3.has_value());
    });

    assertWithinThreshold(elapsed, 10000,
                          "3 concurrent 64^3 volume processing");
}

TEST_F(MemoryStressTest, AllocationDeallocationCycles) {
    constexpr int kCycles = 100;

    auto elapsed = measureTime([&] {
        for (int i = 0; i < kCycles; ++i) {
            auto vol = createVolume(64);
            ASSERT_NE(vol, nullptr);
            // vol goes out of scope and is deallocated
        }
    });

    assertWithinThreshold(elapsed, 5000,
                          "100 allocation/deallocation cycles (64^3)");
}

TEST_F(MemoryStressTest, LargeVolumeAllocation) {
    // Verify 256^3 allocation completes without issues
    auto elapsed = measureTime([&] {
        auto vol = createVolume(256);
        ASSERT_NE(vol, nullptr);

        // Verify the volume dimensions
        auto size = vol->GetLargestPossibleRegion().GetSize();
        EXPECT_EQ(size[0], 256u);
        EXPECT_EQ(size[1], 256u);
        EXPECT_EQ(size[2], 256u);
    });

    assertWithinThreshold(elapsed, 3000, "256^3 volume allocation");
}

TEST_F(MemoryStressTest, SequentialPipelineProcessing) {
    // Process a volume through multiple filters sequentially
    auto volume = createSyntheticCTVolume(64);

    auto elapsed = measureTime([&] {
        // Step 1: Gaussian smoothing
        GaussianSmoother smoother;
        auto smoothed = smoother.apply(volume);
        ASSERT_TRUE(smoothed.has_value());

        // Step 2: Threshold segmentation
        ThresholdSegmenter segmenter;
        auto segmented =
            segmenter.manualThreshold(smoothed.value(), -100.0, 100.0);
        ASSERT_TRUE(segmented.has_value());

        // Step 3: Morphological closing
        MorphologicalProcessor processor;
        auto cleaned = processor.apply(segmented.value(),
                                       MorphologicalOperation::Closing, 2);
        ASSERT_TRUE(cleaned.has_value());
    });

    assertWithinThreshold(elapsed, 5000,
                          "Sequential pipeline (smooth→segment→morph) 64^3");
}

}  // namespace
}  // namespace dicom_viewer::services

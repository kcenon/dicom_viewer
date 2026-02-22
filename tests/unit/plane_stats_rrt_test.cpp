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
#include <vector>

#include <itkVectorImage.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include "services/flow/flow_quantifier.hpp"
#include "services/flow/vessel_analyzer.hpp"

#include "../test_utils/flow_phantom_generator.hpp"

using namespace dicom_viewer::services;
namespace phantom = dicom_viewer::test_utils;

// =============================================================================
// FlowMeasurement — Extended statistics
// =============================================================================

TEST(PlaneStatsTest, UniformFlowHasZeroStdVelocity) {
    // All voxels have the same velocity → std = 0
    constexpr int kDim = 16;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    int numPixels = kDim * kDim * kDim;
    for (int i = 0; i < numPixels; ++i) {
        buf[i * 3]     = 0.0f;
        buf[i * 3 + 1] = 0.0f;
        buf[i * 3 + 2] = 50.0f;
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {7.5, 7.5, 7.5};
    plane.normal = {0, 0, 1};
    plane.radius = 6.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto result = quantifier.measureFlow(phase);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_NEAR(result->meanVelocity, 50.0, 1.0);
    EXPECT_NEAR(result->maxVelocity, 50.0, 1.0);
    EXPECT_NEAR(result->minVelocity, 50.0, 1.0);
    EXPECT_NEAR(result->stdVelocity, 0.0, 0.1);
}

TEST(PlaneStatsTest, PoiseuilleFlowHasNonZeroStd) {
    // Parabolic velocity profile → nonzero std
    constexpr int kDim = 32;
    auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, 80.0, 10.0);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    double center = (kDim - 1) / 2.0;
    plane.center = {center, center, center};
    plane.normal = {0, 0, 1};
    plane.radius = 10.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto result = quantifier.measureFlow(phase);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_GT(result->stdVelocity, 0.0)
        << "Parabolic profile should have nonzero velocity std";
    EXPECT_GE(result->maxVelocity, result->meanVelocity);
    EXPECT_LE(result->minVelocity, std::abs(result->meanVelocity));
}

TEST(PlaneStatsTest, MinVelocityLessOrEqualMax) {
    constexpr int kDim = 16;
    auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, 60.0, 5.0);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    double center = (kDim - 1) / 2.0;
    plane.center = {center, center, center};
    plane.normal = {0, 0, 1};
    plane.radius = 6.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto result = quantifier.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    EXPECT_LE(result->minVelocity, result->maxVelocity);
}

// =============================================================================
// ROI Area
// =============================================================================

TEST(PlaneStatsTest, ROIAreaInMm2) {
    constexpr int kDim = 32;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    int numPixels = kDim * kDim * kDim;
    for (int i = 0; i < numPixels; ++i) {
        buf[i * 3 + 2] = 30.0f;
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {15.5, 15.5, 15.5};
    plane.normal = {0, 0, 1};
    plane.radius = 5.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto result = quantifier.measureFlow(phase);
    ASSERT_TRUE(result.has_value());

    // ROI area = sampleCount * spacing^2 mm^2
    EXPECT_NEAR(result->roiAreaMm2, result->sampleCount * 1.0, 0.01);
    EXPECT_GT(result->roiAreaMm2, 0.0);

    // Cross-section area in cm^2 should be roiAreaMm2 / 100
    EXPECT_NEAR(result->crossSectionArea, result->roiAreaMm2 / 100.0, 0.01);
}

// =============================================================================
// TimeVelocityCurve — Extended statistics
// =============================================================================

TEST(PlaneStatsTest, TVCContainsExtendedFields) {
    constexpr int kDim = 16;
    constexpr int kPhases = 5;
    constexpr double kTemporalRes = 40.0;

    auto [phases, truth] = phantom::generatePulsatileFlow(
        kDim, kPhases, 50.0, 20.0, kTemporalRes);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    double center = (kDim - 1) / 2.0;
    plane.center = {center, center, center};
    plane.normal = {0, 0, 1};
    plane.radius = 5.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto result = quantifier.computeTimeVelocityCurve(phases, kTemporalRes);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(result->minVelocities.size(), kPhases);
    EXPECT_EQ(result->stdVelocities.size(), kPhases);
    EXPECT_EQ(result->minFlowRates.size(), kPhases);
    EXPECT_EQ(result->stdFlowRates.size(), kPhases);
    EXPECT_GT(result->meanRoiArea, 0.0);
}

TEST(PlaneStatsTest, TVCUniformFlowStdIsZero) {
    // Uniform flow at all phases → std velocity should be ~0 per phase
    constexpr int kDim = 8;
    std::vector<VelocityPhase> phases;
    for (int p = 0; p < 3; ++p) {
        auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
        auto* buf = velocity->GetBufferPointer();
        int numPixels = kDim * kDim * kDim;
        float vz = 40.0f + p * 10.0f;
        for (int i = 0; i < numPixels; ++i) {
            buf[i * 3 + 2] = vz;
        }
        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = p;
        phase.triggerTime = p * 50.0;
        phases.push_back(std::move(phase));
    }

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {3.5, 3.5, 3.5};
    plane.normal = {0, 0, 1};
    plane.radius = 3.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto result = quantifier.computeTimeVelocityCurve(phases, 50.0);
    ASSERT_TRUE(result.has_value());

    // Each phase has uniform flow → std per phase should be ~0
    for (size_t i = 0; i < result->stdVelocities.size(); ++i) {
        EXPECT_NEAR(result->stdVelocities[i], 0.0, 0.1)
            << "Phase " << i << " should have ~zero std velocity";
    }
}

TEST(PlaneStatsTest, TVCMeanRoiAreaConsistent) {
    constexpr int kDim = 16;
    constexpr int kPhases = 3;

    auto [phases, truth] = phantom::generatePulsatileFlow(
        kDim, kPhases, 50.0, 10.0, 40.0);

    FlowQuantifier quantifier;
    MeasurementPlane plane;
    plane.center = {7.5, 7.5, 7.5};
    plane.normal = {0, 0, 1};
    plane.radius = 4.0;
    plane.sampleSpacing = 1.0;
    quantifier.setMeasurementPlane(plane);

    auto result = quantifier.computeTimeVelocityCurve(phases, 40.0);
    ASSERT_TRUE(result.has_value());

    // For uniform coverage, all phases should have same ROI area
    // So meanRoiArea should match any single phase
    auto singleResult = quantifier.measureFlow(phases[0]);
    ASSERT_TRUE(singleResult.has_value());
    EXPECT_NEAR(result->meanRoiArea, singleResult->roiAreaMm2, 0.01);
}

// =============================================================================
// RRT — Relative Residence Time
// =============================================================================

namespace {

/// Create a surface mesh with OSI and TAWSS point data arrays
vtkSmartPointer<vtkPolyData> createSurfaceWithWSS(
    int numPoints, double osiValue, double tawssValue) {
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto osiArray = vtkSmartPointer<vtkFloatArray>::New();
    osiArray->SetName("OSI");
    osiArray->SetNumberOfTuples(numPoints);
    auto tawssArray = vtkSmartPointer<vtkFloatArray>::New();
    tawssArray->SetName("TAWSS");
    tawssArray->SetNumberOfTuples(numPoints);

    for (int i = 0; i < numPoints; ++i) {
        points->InsertNextPoint(i * 1.0, 0.0, 0.0);
        osiArray->SetValue(i, static_cast<float>(osiValue));
        tawssArray->SetValue(i, static_cast<float>(tawssValue));
    }

    auto surface = vtkSmartPointer<vtkPolyData>::New();
    surface->SetPoints(points);
    surface->GetPointData()->AddArray(osiArray);
    surface->GetPointData()->AddArray(tawssArray);
    return surface;
}

}  // anonymous namespace

TEST(RRTTest, NullSurfaceReturnsError) {
    VesselAnalyzer analyzer;
    auto result = analyzer.computeRRT(nullptr);
    EXPECT_FALSE(result.has_value());
}

TEST(RRTTest, MissingArraysReturnsError) {
    VesselAnalyzer analyzer;
    auto surface = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(0, 0, 0);
    surface->SetPoints(points);

    auto result = analyzer.computeRRT(surface);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(RRTTest, ZeroOSIComputesCorrectRRT) {
    // OSI = 0 → RRT = 1 / ((1-0) * TAWSS) = 1 / TAWSS
    VesselAnalyzer analyzer;
    auto surface = createSurfaceWithWSS(10, 0.0, 2.0);  // OSI=0, TAWSS=2.0

    auto result = analyzer.computeRRT(surface);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    auto* rrtArray = result.value()->GetPointData()->GetArray("RRT");
    ASSERT_NE(rrtArray, nullptr);
    EXPECT_EQ(rrtArray->GetNumberOfTuples(), 10);

    // RRT = 1 / (1.0 * 2.0) = 0.5
    for (int i = 0; i < 10; ++i) {
        EXPECT_NEAR(rrtArray->GetTuple1(i), 0.5, 0.001);
    }
}

TEST(RRTTest, HighOSIIncreasesRRT) {
    // OSI = 0.4 → RRT = 1 / ((1 - 0.8) * TAWSS) = 1 / (0.2 * TAWSS)
    VesselAnalyzer analyzer;
    auto surface = createSurfaceWithWSS(5, 0.4, 1.0);  // OSI=0.4, TAWSS=1.0

    auto result = analyzer.computeRRT(surface);
    ASSERT_TRUE(result.has_value());

    auto* rrtArray = result.value()->GetPointData()->GetArray("RRT");
    ASSERT_NE(rrtArray, nullptr);

    // RRT = 1 / (0.2 * 1.0) = 5.0
    for (int i = 0; i < 5; ++i) {
        EXPECT_NEAR(rrtArray->GetTuple1(i), 5.0, 0.01);
    }
}

TEST(RRTTest, ZeroTAWSSProducesZeroRRT) {
    // TAWSS = 0 → denominator = 0 → RRT should be 0 (clamped)
    VesselAnalyzer analyzer;
    auto surface = createSurfaceWithWSS(5, 0.1, 0.0);

    auto result = analyzer.computeRRT(surface);
    ASSERT_TRUE(result.has_value());

    auto* rrtArray = result.value()->GetPointData()->GetArray("RRT");
    for (int i = 0; i < 5; ++i) {
        EXPECT_NEAR(rrtArray->GetTuple1(i), 0.0, 0.001);
    }
}

TEST(RRTTest, MaxOSIProducesZeroRRT) {
    // OSI = 0.5 → denominator = (1 - 1.0) * TAWSS = 0 → RRT = 0
    VesselAnalyzer analyzer;
    auto surface = createSurfaceWithWSS(5, 0.5, 2.0);

    auto result = analyzer.computeRRT(surface);
    ASSERT_TRUE(result.has_value());

    auto* rrtArray = result.value()->GetPointData()->GetArray("RRT");
    for (int i = 0; i < 5; ++i) {
        EXPECT_NEAR(rrtArray->GetTuple1(i), 0.0, 0.001);
    }
}

TEST(RRTTest, PreservesExistingArrays) {
    // RRT should be added without removing OSI and TAWSS
    VesselAnalyzer analyzer;
    auto surface = createSurfaceWithWSS(3, 0.1, 1.5);

    auto result = analyzer.computeRRT(surface);
    ASSERT_TRUE(result.has_value());

    auto* out = result.value().Get();
    EXPECT_NE(out->GetPointData()->GetArray("OSI"), nullptr);
    EXPECT_NE(out->GetPointData()->GetArray("TAWSS"), nullptr);
    EXPECT_NE(out->GetPointData()->GetArray("RRT"), nullptr);
}

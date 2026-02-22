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

// Integration test for slice ordering verification pipeline
// Converted from manual CLI tool to automated GoogleTest (#203)
// Uses synthetic SliceInfo vectors — no real DICOM files required

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include "core/dicom_loader.hpp"
#include "core/series_builder.hpp"

using namespace dicom_viewer::core;

// =============================================================================
// Test fixture with synthetic slice data generation
// =============================================================================

class SliceOrderingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        createAxialSlices();
        createSagittalSlices();
        createCoronalSlices();
    }

    /// Create 20 axial slices at 5mm spacing
    void createAxialSlices()
    {
        axialSlices_.clear();
        for (int i = 0; i < 20; ++i) {
            SliceInfo slice;
            slice.filePath = "/synthetic/axial_" + std::to_string(i) + ".dcm";
            slice.imagePosition = {-100.0, -100.0, static_cast<double>(i * 5)};
            slice.imageOrientation = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
            slice.sliceLocation = static_cast<double>(i * 5);
            slice.instanceNumber = i + 1;
            axialSlices_.push_back(slice);
        }
    }

    /// Create 15 sagittal slices at 3mm spacing (normal along X-axis)
    void createSagittalSlices()
    {
        sagittalSlices_.clear();
        for (int i = 0; i < 15; ++i) {
            SliceInfo slice;
            slice.filePath = "/synthetic/sag_" + std::to_string(i) + ".dcm";
            // Sagittal: varying X position
            slice.imagePosition = {static_cast<double>(i * 3), -100.0, 0.0};
            // Sagittal orientation: row along Y, col along Z
            slice.imageOrientation = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
            slice.sliceLocation = static_cast<double>(i * 3);
            slice.instanceNumber = i + 1;
            sagittalSlices_.push_back(slice);
        }
    }

    /// Create 12 coronal slices at 4mm spacing (normal along Y-axis)
    void createCoronalSlices()
    {
        coronalSlices_.clear();
        for (int i = 0; i < 12; ++i) {
            SliceInfo slice;
            slice.filePath = "/synthetic/cor_" + std::to_string(i) + ".dcm";
            // Coronal: varying Y position
            slice.imagePosition = {-100.0, static_cast<double>(i * 4), 0.0};
            // Coronal orientation: row along X, col along Z
            slice.imageOrientation = {1.0, 0.0, 0.0, 0.0, 0.0, 1.0};
            slice.sliceLocation = static_cast<double>(i * 4);
            slice.instanceNumber = i + 1;
            coronalSlices_.push_back(slice);
        }
    }

    /// Shuffle a copy of slices using deterministic seed
    static std::vector<SliceInfo> shuffleSlices(const std::vector<SliceInfo>& slices,
                                                 unsigned int seed = 42)
    {
        auto shuffled = slices;
        std::mt19937 rng(seed);
        std::shuffle(shuffled.begin(), shuffled.end(), rng);
        return shuffled;
    }

    /// Calculate spacing variability (coefficient of variation) from slice positions
    static double calculateSpacingCV(const std::vector<SliceInfo>& sortedSlices)
    {
        if (sortedSlices.size() < 2) return 0.0;

        std::vector<double> spacings;
        for (size_t i = 1; i < sortedSlices.size(); ++i) {
            double dz = sortedSlices[i].imagePosition[2]
                      - sortedSlices[i - 1].imagePosition[2];
            spacings.push_back(std::abs(dz));
        }

        double mean = std::accumulate(spacings.begin(), spacings.end(), 0.0)
                     / static_cast<double>(spacings.size());
        if (mean < 1e-9) return 0.0;

        double variance = 0.0;
        for (double s : spacings) {
            variance += (s - mean) * (s - mean);
        }
        variance /= static_cast<double>(spacings.size());
        return std::sqrt(variance) / mean * 100.0;  // CV in percent
    }

    std::vector<SliceInfo> axialSlices_;
    std::vector<SliceInfo> sagittalSlices_;
    std::vector<SliceInfo> coronalSlices_;
};

// =============================================================================
// Monotonic Z-ordering validation
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, OrderedSlicesPassMonotonicCheck)
{
    // Pre-sorted slices should have monotonically increasing Z positions
    for (size_t i = 1; i < axialSlices_.size(); ++i) {
        EXPECT_GT(axialSlices_[i].imagePosition[2],
                  axialSlices_[i - 1].imagePosition[2])
            << "Non-monotonic at index " << i;
    }
}

TEST_F(SliceOrderingIntegrationTest, ShuffledSlicesYieldSameSpacing)
{
    // Shuffle slices, then verify calculateSliceSpacing returns same result
    // as the ordered version (spacing calculation is position-based, not
    // order-dependent if it sorts internally)
    auto shuffled = shuffleSlices(axialSlices_);

    double orderedSpacing = SeriesBuilder::calculateSliceSpacing(axialSlices_);
    double shuffledSpacing = SeriesBuilder::calculateSliceSpacing(shuffled);

    EXPECT_NEAR(orderedSpacing, 5.0, 0.01);
    EXPECT_NEAR(shuffledSpacing, 5.0, 0.01);
}

// =============================================================================
// Instance number correlation
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, InstanceNumbersCorrelateWithPosition)
{
    // For our synthetic data, instance numbers should increase with Z position
    for (size_t i = 1; i < axialSlices_.size(); ++i) {
        EXPECT_GT(axialSlices_[i].instanceNumber,
                  axialSlices_[i - 1].instanceNumber)
            << "Instance numbers not monotonically increasing at index " << i;
    }

    // Also verify instance number matches position index + 1
    for (size_t i = 0; i < axialSlices_.size(); ++i) {
        EXPECT_EQ(axialSlices_[i].instanceNumber, static_cast<int>(i + 1));
    }
}

// =============================================================================
// Spacing consistency tests
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, UniformSpacingPassesConsistency)
{
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(axialSlices_));
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(sagittalSlices_));
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(coronalSlices_));
}

TEST_F(SliceOrderingIntegrationTest, NonUniformSpacingFailsConsistency)
{
    // Inject one displaced slice (index 10: Z should be 50, set to 65)
    auto modified = axialSlices_;
    modified[10].imagePosition[2] = 65.0;

    EXPECT_FALSE(SeriesBuilder::validateSeriesConsistency(modified));
}

TEST_F(SliceOrderingIntegrationTest, SpacingVariabilityBelowThreshold)
{
    // For uniform 5mm spacing, CV should be ~0%
    double cv = calculateSpacingCV(axialSlices_);
    EXPECT_LT(cv, 1.0) << "Spacing CV for uniform series should be < 1%";

    // Verify with non-uniform series
    auto nonUniform = axialSlices_;
    nonUniform[5].imagePosition[2] = 30.0;  // Shift from 25 to 30
    double nonUniformCV = calculateSpacingCV(nonUniform);
    EXPECT_GT(nonUniformCV, 10.0) << "Non-uniform series should have CV > 10%";
}

// =============================================================================
// Non-monotonic detection
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, NonMonotonicDetectedAfterInjection)
{
    // Create a non-monotonic series by reversing two slices
    auto nonMono = axialSlices_;
    std::swap(nonMono[8], nonMono[12]);  // Swap slices at Z=40 and Z=60

    // Check that monotonic ordering is broken
    bool isMonotonic = true;
    double prevZ = nonMono[0].imagePosition[2];
    for (size_t i = 1; i < nonMono.size(); ++i) {
        if (nonMono[i].imagePosition[2] <= prevZ) {
            isMonotonic = false;
            break;
        }
        prevZ = nonMono[i].imagePosition[2];
    }

    EXPECT_FALSE(isMonotonic) << "Swapped slices should break monotonic ordering";

    // Consistency check should also detect the disruption
    EXPECT_FALSE(SeriesBuilder::validateSeriesConsistency(nonMono));
}

// =============================================================================
// Non-axial orientation sorting
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, SagittalOrientationSpacingCalculation)
{
    double spacing = SeriesBuilder::calculateSliceSpacing(sagittalSlices_);
    EXPECT_NEAR(spacing, 3.0, 0.01) << "Sagittal 3mm spacing not calculated correctly";
}

TEST_F(SliceOrderingIntegrationTest, CoronalOrientationSpacingCalculation)
{
    double spacing = SeriesBuilder::calculateSliceSpacing(coronalSlices_);
    EXPECT_NEAR(spacing, 4.0, 0.01) << "Coronal 4mm spacing not calculated correctly";
}

// =============================================================================
// Edge cases
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, EmptySliceVectorHandledGracefully)
{
    std::vector<SliceInfo> empty;
    double spacing = SeriesBuilder::calculateSliceSpacing(empty);
    EXPECT_NEAR(spacing, 1.0, 0.01);  // Default spacing

    bool valid = SeriesBuilder::validateSeriesConsistency(empty);
    // Empty vector: implementation may return true or false; should not crash
    (void)valid;
    SUCCEED();
}

TEST_F(SliceOrderingIntegrationTest, SingleSliceIsAlwaysConsistent)
{
    std::vector<SliceInfo> single = {axialSlices_[0]};
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(single));
}

TEST_F(SliceOrderingIntegrationTest, TwoSliceMinimalSeries)
{
    std::vector<SliceInfo> twoSlices = {axialSlices_[0], axialSlices_[1]};
    double spacing = SeriesBuilder::calculateSliceSpacing(twoSlices);
    EXPECT_NEAR(spacing, 5.0, 0.01);
    EXPECT_TRUE(SeriesBuilder::validateSeriesConsistency(twoSlices));
}

// =============================================================================
// Spacing statistics (replicates manual test's median/min/max analysis)
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, SpacingStatisticsMatchExpected)
{
    // For uniform 5mm axial series, all spacings should be identical
    std::vector<double> spacings;
    for (size_t i = 1; i < axialSlices_.size(); ++i) {
        double dz = axialSlices_[i].imagePosition[2]
                  - axialSlices_[i - 1].imagePosition[2];
        spacings.push_back(std::abs(dz));
    }

    ASSERT_FALSE(spacings.empty());
    std::sort(spacings.begin(), spacings.end());

    double median = spacings[spacings.size() / 2];
    double minSpacing = spacings.front();
    double maxSpacing = spacings.back();

    EXPECT_NEAR(median, 5.0, 0.001);
    EXPECT_NEAR(minSpacing, 5.0, 0.001);
    EXPECT_NEAR(maxSpacing, 5.0, 0.001);

    // Variability should be 0 for perfectly uniform spacing
    double variability = (maxSpacing - minSpacing) / median * 100.0;
    EXPECT_LT(variability, 0.1) << "Uniform series should have ~0% variability";
}

// =============================================================================
// Mixed orientation detection
// =============================================================================

TEST_F(SliceOrderingIntegrationTest, MixedOrientationDetectedAsInconsistent)
{
    // Combine axial and sagittal slices into one series
    auto mixed = axialSlices_;
    mixed.push_back(sagittalSlices_[0]);
    mixed.push_back(sagittalSlices_[1]);

    EXPECT_FALSE(SeriesBuilder::validateSeriesConsistency(mixed))
        << "Mixed axial+sagittal orientations should fail consistency check";
}

#include "services/segmentation/phase_tracker.hpp"

#include <cmath>
#include <vector>

#include <gtest/gtest.h>
#include <itkImageRegionIterator.h>

using namespace dicom_viewer::services;
using FloatImage3D = PhaseTracker::FloatImage3D;
using LabelMapType = PhaseTracker::LabelMapType;
using DisplacementFieldType = PhaseTracker::DisplacementFieldType;

namespace {

/// Create a float 3D image of given size with uniform value
FloatImage3D::Pointer createFloatImage(int sx, int sy, int sz, float value = 0.0f) {
    auto image = FloatImage3D::New();
    FloatImage3D::SizeType size = {{
        static_cast<unsigned long>(sx),
        static_cast<unsigned long>(sy),
        static_cast<unsigned long>(sz)
    }};
    FloatImage3D::IndexType start = {{0, 0, 0}};
    FloatImage3D::RegionType region{start, size};
    image->SetRegions(region);

    FloatImage3D::SpacingType spacing;
    spacing.Fill(1.0);
    image->SetSpacing(spacing);

    FloatImage3D::PointType origin;
    origin.Fill(0.0);
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(value);
    return image;
}

/// Create a label map of given size with all zeros
LabelMapType::Pointer createLabelMap(int sx, int sy, int sz) {
    auto image = LabelMapType::New();
    LabelMapType::SizeType size = {{
        static_cast<unsigned long>(sx),
        static_cast<unsigned long>(sy),
        static_cast<unsigned long>(sz)
    }};
    LabelMapType::IndexType start = {{0, 0, 0}};
    LabelMapType::RegionType region{start, size};
    image->SetRegions(region);

    LabelMapType::SpacingType spacing;
    spacing.Fill(1.0);
    image->SetSpacing(spacing);

    LabelMapType::PointType origin;
    origin.Fill(0.0);
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(0);
    return image;
}

/// Draw a filled sphere in a float image
void drawSphereFloat(FloatImage3D::Pointer image,
                     double cx, double cy, double cz,
                     double radius, float intensity) {
    auto region = image->GetLargestPossibleRegion();
    itk::ImageRegionIterator<FloatImage3D> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        double dx = idx[0] - cx;
        double dy = idx[1] - cy;
        double dz = idx[2] - cz;
        if (dx*dx + dy*dy + dz*dz <= radius*radius) {
            it.Set(intensity);
        }
    }
}

/// Draw a filled sphere in a label map
void drawSphereLabel(LabelMapType::Pointer image,
                     double cx, double cy, double cz,
                     double radius, uint8_t label) {
    auto region = image->GetLargestPossibleRegion();
    itk::ImageRegionIterator<LabelMapType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        double dx = idx[0] - cx;
        double dy = idx[1] - cy;
        double dz = idx[2] - cz;
        if (dx*dx + dy*dy + dz*dz <= radius*radius) {
            it.Set(label);
        }
    }
}

/// Create a constant displacement field
DisplacementFieldType::Pointer createConstantField(
    int sx, int sy, int sz,
    float dx, float dy, float dz) {
    auto field = DisplacementFieldType::New();
    DisplacementFieldType::SizeType size = {{
        static_cast<unsigned long>(sx),
        static_cast<unsigned long>(sy),
        static_cast<unsigned long>(sz)
    }};
    DisplacementFieldType::IndexType start = {{0, 0, 0}};
    DisplacementFieldType::RegionType region{start, size};
    field->SetRegions(region);

    DisplacementFieldType::SpacingType spacing;
    spacing.Fill(1.0);
    field->SetSpacing(spacing);

    DisplacementFieldType::PointType origin;
    origin.Fill(0.0);
    field->SetOrigin(origin);

    field->Allocate();

    itk::Vector<float, 3> displacement;
    displacement[0] = dx;
    displacement[1] = dy;
    displacement[2] = dz;
    field->FillBuffer(displacement);

    return field;
}

}  // anonymous namespace

// =============================================================================
// Static method tests
// =============================================================================

TEST(PhaseTrackerTest, CountNonZeroVoxels) {
    auto mask = createLabelMap(10, 10, 10);
    EXPECT_EQ(PhaseTracker::countNonZeroVoxels(mask), 0u);

    drawSphereLabel(mask, 5.0, 5.0, 5.0, 3.0, 1);
    size_t count = PhaseTracker::countNonZeroVoxels(mask);
    EXPECT_GT(count, 0u);
    EXPECT_LT(count, 1000u);
}

TEST(PhaseTrackerTest, CountNonZeroVoxelsNullReturnsZero) {
    EXPECT_EQ(PhaseTracker::countNonZeroVoxels(nullptr), 0u);
}

TEST(PhaseTrackerTest, WarpMaskWithConstantField) {
    const int dim = 20;
    auto mask = createLabelMap(dim, dim, dim);
    drawSphereLabel(mask, 10.0, 10.0, 10.0, 4.0, 1);

    size_t originalCount = PhaseTracker::countNonZeroVoxels(mask);
    EXPECT_GT(originalCount, 50u);

    // Shift mask by (2, 0, 0) using a constant displacement field
    auto field = createConstantField(dim, dim, dim, 2.0f, 0.0f, 0.0f);
    auto result = PhaseTracker::warpMask(mask, field);

    ASSERT_TRUE(result.has_value());
    auto warped = result.value();
    size_t warpedCount = PhaseTracker::countNonZeroVoxels(warped);

    // Warped mask should have similar voxel count (some boundary loss expected)
    EXPECT_GT(warpedCount, originalCount / 2);

    // Check that center shifted: original center (10,10,10) → new center ~(12,10,10)
    // The original center should now be empty or the new center should be filled
    LabelMapType::IndexType shiftedCenter = {{12, 10, 10}};
    EXPECT_EQ(warped->GetPixel(shiftedCenter), 1);
}

TEST(PhaseTrackerTest, WarpMaskNullInputReturnsError) {
    auto mask = createLabelMap(10, 10, 10);
    auto result = PhaseTracker::warpMask(nullptr, nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(PhaseTrackerTest, ApplyClosingFillsSmallGaps) {
    auto mask = createLabelMap(20, 20, 20);
    drawSphereLabel(mask, 10.0, 10.0, 10.0, 5.0, 1);

    // Remove center voxel to create a gap
    LabelMapType::IndexType center = {{10, 10, 10}};
    mask->SetPixel(center, 0);
    EXPECT_EQ(mask->GetPixel(center), 0);

    auto closed = PhaseTracker::applyClosing(mask, 1);
    EXPECT_NE(closed, nullptr);
    // After closing, the gap should be filled
    EXPECT_EQ(closed->GetPixel(center), 1);
}

TEST(PhaseTrackerTest, ApplyClosingNullReturnsNull) {
    auto result = PhaseTracker::applyClosing(nullptr, 1);
    EXPECT_EQ(result, nullptr);
}

TEST(PhaseTrackerTest, ApplyClosingZeroRadiusReturnsOriginal) {
    auto mask = createLabelMap(10, 10, 10);
    auto result = PhaseTracker::applyClosing(mask, 0);
    EXPECT_EQ(result.GetPointer(), mask.GetPointer());
}

// =============================================================================
// Displacement field computation
// =============================================================================

TEST(PhaseTrackerTest, ComputeDisplacementFieldIdenticalImages) {
    const int dim = 16;
    auto img = createFloatImage(dim, dim, dim, 0.0f);
    drawSphereFloat(img, 8.0, 8.0, 8.0, 4.0, 100.0f);

    // Identical images → displacement field should be near zero
    auto result = PhaseTracker::computeDisplacementField(img, img, 10, 1.0);
    ASSERT_TRUE(result.has_value());

    auto field = result.value();
    DisplacementFieldType::IndexType center = {{8, 8, 8}};
    auto disp = field->GetPixel(center);

    // Each component should be near zero
    EXPECT_NEAR(disp[0], 0.0f, 0.5f);
    EXPECT_NEAR(disp[1], 0.0f, 0.5f);
    EXPECT_NEAR(disp[2], 0.0f, 0.5f);
}

TEST(PhaseTrackerTest, ComputeDisplacementFieldNullReturnsError) {
    auto result = PhaseTracker::computeDisplacementField(
        nullptr, nullptr, 10, 1.0);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

// =============================================================================
// Full propagation pipeline
// =============================================================================

TEST(PhaseTrackerTest, PropagateMaskNullMaskReturnsError) {
    std::vector<FloatImage3D::Pointer> phases;
    phases.push_back(createFloatImage(10, 10, 10));
    phases.push_back(createFloatImage(10, 10, 10));

    PhaseTracker tracker;
    PhaseTracker::TrackingConfig config;
    config.referencePhase = 0;

    auto result = tracker.propagateMask(nullptr, phases, config);
    EXPECT_FALSE(result.has_value());
}

TEST(PhaseTrackerTest, PropagateMaskTooFewPhasesReturnsError) {
    auto mask = createLabelMap(10, 10, 10);
    std::vector<FloatImage3D::Pointer> phases;
    phases.push_back(createFloatImage(10, 10, 10));

    PhaseTracker tracker;
    PhaseTracker::TrackingConfig config;
    auto result = tracker.propagateMask(mask, phases, config);
    EXPECT_FALSE(result.has_value());
}

TEST(PhaseTrackerTest, PropagateMaskInvalidReferenceReturnsError) {
    auto mask = createLabelMap(10, 10, 10);
    std::vector<FloatImage3D::Pointer> phases;
    phases.push_back(createFloatImage(10, 10, 10));
    phases.push_back(createFloatImage(10, 10, 10));

    PhaseTracker tracker;
    PhaseTracker::TrackingConfig config;
    config.referencePhase = 5;  // Out of range

    auto result = tracker.propagateMask(mask, phases, config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SegmentationError::Code::InvalidInput);
}

TEST(PhaseTrackerTest, PropagateMaskStaticPhases) {
    // All phases have the same image → mask should propagate unchanged
    const int dim = 16;
    const int numPhases = 3;

    auto mag = createFloatImage(dim, dim, dim, 0.0f);
    drawSphereFloat(mag, 8.0, 8.0, 8.0, 4.0, 100.0f);

    std::vector<FloatImage3D::Pointer> phases(numPhases, mag);

    auto mask = createLabelMap(dim, dim, dim);
    drawSphereLabel(mask, 8.0, 8.0, 8.0, 3.0, 1);

    PhaseTracker tracker;
    PhaseTracker::TrackingConfig config;
    config.referencePhase = 1;  // Middle phase
    config.registrationIterations = 10;
    config.smoothingSigma = 1.0;
    config.applyMorphologicalClosing = false;

    auto result = tracker.propagateMask(mask, phases, config);
    ASSERT_TRUE(result.has_value());

    auto& tracking = result.value();
    EXPECT_EQ(tracking.phases.size(), static_cast<size_t>(numPhases));
    EXPECT_EQ(tracking.referencePhase, 1);

    // All phases should have masks
    for (int i = 0; i < numPhases; ++i) {
        ASSERT_NE(tracking.phases[i].mask, nullptr);
        size_t count = PhaseTracker::countNonZeroVoxels(tracking.phases[i].mask);
        EXPECT_GT(count, 0u) << "Phase " << i << " has empty mask";
    }

    // Volume ratios should be close to 1.0 for static phases
    size_t refCount = PhaseTracker::countNonZeroVoxels(mask);
    for (int i = 0; i < numPhases; ++i) {
        EXPECT_NEAR(tracking.phases[i].volumeRatio, 1.0, 0.3)
            << "Phase " << i << " volume ratio out of range";
    }
}

TEST(PhaseTrackerTest, ProgressCallbackInvoked) {
    const int dim = 16;
    const int numPhases = 3;

    auto mag = createFloatImage(dim, dim, dim, 50.0f);
    std::vector<FloatImage3D::Pointer> phases(numPhases, mag);

    auto mask = createLabelMap(dim, dim, dim);
    drawSphereLabel(mask, 8.0, 8.0, 8.0, 3.0, 1);

    PhaseTracker tracker;
    int callCount = 0;
    tracker.setProgressCallback([&](int current, int total) {
        ++callCount;
        EXPECT_LE(current, total);
    });

    PhaseTracker::TrackingConfig config;
    config.referencePhase = 1;
    config.registrationIterations = 5;
    config.applyMorphologicalClosing = false;

    auto result = tracker.propagateMask(mask, phases, config);
    ASSERT_TRUE(result.has_value());

    // Should be called once per non-reference phase (numPhases - 1)
    EXPECT_EQ(callCount, numPhases - 1);
}

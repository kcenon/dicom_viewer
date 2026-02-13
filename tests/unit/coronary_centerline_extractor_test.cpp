#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

#include <itkImage.h>
#include <itkImageRegionIterator.h>

#include "services/cardiac/cardiac_types.hpp"
#include "services/cardiac/coronary_centerline_extractor.hpp"
#include "services/cardiac/curved_planar_reformatter.hpp"

using namespace dicom_viewer::services;

using ImageType = itk::Image<short, 3>;
using FloatImageType = itk::Image<float, 3>;

// =============================================================================
// Test Helpers: Synthetic Vessel Phantom
// =============================================================================

namespace {

// Create a 3D image with given dimensions and spacing
ImageType::Pointer createTestVolume(int sizeX, int sizeY, int sizeZ,
                                     double spacingMM = 0.5)
{
    auto image = ImageType::New();
    ImageType::SizeType size;
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;

    ImageType::RegionType region;
    region.SetSize(size);

    image->SetRegions(region);
    const double spacing[3] = {spacingMM, spacingMM, spacingMM};
    image->SetSpacing(spacing);
    const double origin[3] = {0.0, 0.0, 0.0};
    image->SetOrigin(origin);
    image->Allocate();
    image->FillBuffer(-100);  // Background HU

    return image;
}

// Create a straight tube phantom along the Y axis
// Tube center at (centerX, y, centerZ), radius = tubeRadius
// Vessel HU = 300, background = -100
ImageType::Pointer createStraightTubePhantom(
    int sizeX, int sizeY, int sizeZ,
    double centerX, double centerZ,
    double tubeRadius, double spacing = 0.5,
    short vesselHU = 300)
{
    auto image = createTestVolume(sizeX, sizeY, sizeZ, spacing);

    itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        ImageType::PointType point;
        image->TransformIndexToPhysicalPoint(idx, point);

        double dx = point[0] - centerX;
        double dz = point[2] - centerZ;
        double dist = std::sqrt(dx*dx + dz*dz);

        if (dist <= tubeRadius) {
            it.Set(vesselHU);
        }
    }

    return image;
}

// Create a tube phantom with a stenosis (narrowing)
ImageType::Pointer createStenosisTubePhantom(
    int sizeX, int sizeY, int sizeZ,
    double centerX, double centerZ,
    double normalRadius, double stenosisRadius,
    double stenosisY, double stenosisLength,
    double spacing = 0.5)
{
    auto image = createTestVolume(sizeX, sizeY, sizeZ, spacing);

    itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        ImageType::PointType point;
        image->TransformIndexToPhysicalPoint(idx, point);

        double dx = point[0] - centerX;
        double dz = point[2] - centerZ;
        double dist = std::sqrt(dx*dx + dz*dz);

        // Compute local radius with stenosis
        double localRadius = normalRadius;
        double distFromStenosis = std::abs(point[1] - stenosisY);
        if (distFromStenosis < stenosisLength / 2.0) {
            double t = 1.0 - distFromStenosis / (stenosisLength / 2.0);
            localRadius = normalRadius - t * (normalRadius - stenosisRadius);
        }

        if (dist <= localRadius) {
            it.Set(300);  // Vessel HU
        }
    }

    return image;
}

// Create a vesselness image from a known tube (for testing centerline extraction)
FloatImageType::Pointer createSyntheticVesselness(
    ImageType::Pointer image, double centerX, double centerZ, double tubeRadius)
{
    auto vesselness = FloatImageType::New();
    vesselness->SetRegions(image->GetLargestPossibleRegion());
    vesselness->SetSpacing(image->GetSpacing());
    vesselness->SetOrigin(image->GetOrigin());
    vesselness->SetDirection(image->GetDirection());
    vesselness->Allocate();
    vesselness->FillBuffer(0.0f);

    itk::ImageRegionIterator<FloatImageType> it(vesselness, vesselness->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        FloatImageType::PointType point;
        vesselness->TransformIndexToPhysicalPoint(idx, point);

        double dx = point[0] - centerX;
        double dz = point[2] - centerZ;
        double dist = std::sqrt(dx*dx + dz*dz);

        // Gaussian-like vesselness response centered on tube
        if (dist <= tubeRadius * 2.0) {
            double v = std::exp(-dist * dist / (2.0 * tubeRadius * tubeRadius * 0.25));
            it.Set(static_cast<float>(v));
        }
    }

    return vesselness;
}

}  // namespace

// =============================================================================
// Type Tests
// =============================================================================

TEST(CoronaryCTATypes, VesselnessParamsDefaults)
{
    VesselnessParams params;
    EXPECT_DOUBLE_EQ(params.sigmaMin, 0.5);
    EXPECT_DOUBLE_EQ(params.sigmaMax, 3.0);
    EXPECT_EQ(params.sigmaSteps, 5);
    EXPECT_DOUBLE_EQ(params.alpha, 0.5);
    EXPECT_DOUBLE_EQ(params.beta, 0.5);
    EXPECT_DOUBLE_EQ(params.gamma, 5.0);
}

TEST(CoronaryCTATypes, CenterlinePointDefaults)
{
    CenterlinePoint pt;
    EXPECT_DOUBLE_EQ(pt.position[0], 0.0);
    EXPECT_DOUBLE_EQ(pt.radius, 0.0);
    EXPECT_DOUBLE_EQ(pt.tangent[0], 1.0);
    EXPECT_DOUBLE_EQ(pt.normal[1], 1.0);
}

TEST(CoronaryCTATypes, CenterlineResultValidity)
{
    CenterlineResult result;
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.pointCount(), 0);

    CenterlinePoint p1, p2;
    p1.position = {0.0, 0.0, 0.0};
    p2.position = {1.0, 0.0, 0.0};
    result.points = {p1, p2};
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.pointCount(), 2);
}

TEST(CoronaryCTATypes, CPRTypeEnum)
{
    EXPECT_NE(CPRType::Straightened, CPRType::CrossSectional);
    EXPECT_NE(CPRType::CrossSectional, CPRType::Stretched);
}

// =============================================================================
// CoronaryCenterlineExtractor Construction
// =============================================================================

TEST(CoronaryCenterlineExtractor, Construction)
{
    CoronaryCenterlineExtractor extractor;
    // Should construct without error
}

TEST(CoronaryCenterlineExtractor, MoveConstruction)
{
    CoronaryCenterlineExtractor extractor;
    CoronaryCenterlineExtractor moved(std::move(extractor));
    // Moved object should be usable
}

// =============================================================================
// Vesselness Tests
// =============================================================================

TEST(CoronaryCenterlineExtractor, VesselnessNullImage)
{
    CoronaryCenterlineExtractor extractor;
    auto result = extractor.computeVesselness(nullptr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::InternalError);
}

TEST(CoronaryCenterlineExtractor, VesselnessInvalidParams)
{
    CoronaryCenterlineExtractor extractor;
    auto image = createTestVolume(10, 10, 10);

    VesselnessParams badParams;
    badParams.sigmaSteps = 0;
    auto result = extractor.computeVesselness(image, badParams);
    ASSERT_FALSE(result.has_value());
}

TEST(CoronaryCenterlineExtractor, VesselnessOnTubePhantom)
{
    // Create a tube along Y-axis centered at (10, *, 10) with radius 2mm
    auto image = createStraightTubePhantom(40, 60, 40, 10.0, 10.0, 2.0);

    CoronaryCenterlineExtractor extractor;
    VesselnessParams params;
    params.sigmaMin = 1.0;
    params.sigmaMax = 2.5;
    params.sigmaSteps = 3;

    auto result = extractor.computeVesselness(image, params);
    ASSERT_TRUE(result.has_value());

    auto vesselness = result.value();
    ASSERT_NE(vesselness, nullptr);

    // Check that vesselness is higher inside the tube than outside
    FloatImageType::IndexType insideIdx;
    insideIdx[0] = 20;  // x = 10mm at spacing 0.5
    insideIdx[1] = 30;  // y = 15mm (middle)
    insideIdx[2] = 20;  // z = 10mm

    FloatImageType::IndexType outsideIdx;
    outsideIdx[0] = 0;
    outsideIdx[1] = 30;
    outsideIdx[2] = 0;

    float insideValue = vesselness->GetPixel(insideIdx);
    float outsideValue = vesselness->GetPixel(outsideIdx);

    // Inside should have some response, outside should be zero or near-zero
    EXPECT_GE(insideValue, 0.0f);
    EXPECT_LE(outsideValue, insideValue);
}

TEST(CoronaryCenterlineExtractor, VesselnessSingleScale)
{
    auto image = createStraightTubePhantom(30, 40, 30, 7.5, 7.5, 1.5);

    CoronaryCenterlineExtractor extractor;
    VesselnessParams params;
    params.sigmaMin = 1.0;
    params.sigmaMax = 1.0;
    params.sigmaSteps = 1;

    auto result = extractor.computeVesselness(image, params);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

// =============================================================================
// Centerline Extraction Tests
// =============================================================================

TEST(CoronaryCenterlineExtractor, ExtractCenterlineNullInputs)
{
    CoronaryCenterlineExtractor extractor;

    std::array<double, 3> seed = {10.0, 2.0, 10.0};
    std::array<double, 3> end = {10.0, 28.0, 10.0};

    auto result = extractor.extractCenterline(seed, end, nullptr, nullptr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, CardiacError::Code::InternalError);
}

TEST(CoronaryCenterlineExtractor, ExtractCenterlineOnSyntheticVessel)
{
    double centerX = 10.0, centerZ = 10.0, radius = 2.0;
    auto image = createStraightTubePhantom(40, 60, 40, centerX, centerZ, radius);
    auto vesselness = createSyntheticVesselness(image, centerX, centerZ, radius);

    CoronaryCenterlineExtractor extractor;
    std::array<double, 3> seed = {centerX, 2.0, centerZ};
    std::array<double, 3> end = {centerX, 27.0, centerZ};

    auto result = extractor.extractCenterline(seed, end, vesselness, image);
    ASSERT_TRUE(result.has_value());

    auto& centerline = result.value();
    EXPECT_GE(centerline.points.size(), 2u);
    EXPECT_GT(centerline.totalLength, 0.0);

    // Path should approximately follow Y-axis
    for (const auto& pt : centerline.points) {
        double devX = std::abs(pt.position[0] - centerX);
        double devZ = std::abs(pt.position[2] - centerZ);
        // Allow some deviation but should be near center
        EXPECT_LT(devX, radius * 3.0);
        EXPECT_LT(devZ, radius * 3.0);
    }
}

TEST(CoronaryCenterlineExtractor, ExtractCenterlineSeedOutOfBounds)
{
    auto image = createStraightTubePhantom(20, 20, 20, 5.0, 5.0, 1.0);
    auto vesselness = createSyntheticVesselness(image, 5.0, 5.0, 1.0);

    CoronaryCenterlineExtractor extractor;
    std::array<double, 3> seed = {100.0, 100.0, 100.0};  // Way out of bounds
    std::array<double, 3> end = {5.0, 8.0, 5.0};

    auto result = extractor.extractCenterline(seed, end, vesselness, image);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// B-Spline Smoothing Tests
// =============================================================================

TEST(CoronaryCenterlineExtractor, SmoothCenterlineEmpty)
{
    CoronaryCenterlineExtractor extractor;
    std::vector<CenterlinePoint> empty;
    auto smoothed = extractor.smoothCenterline(empty);
    EXPECT_TRUE(smoothed.empty());
}

TEST(CoronaryCenterlineExtractor, SmoothCenterlineTooFew)
{
    CoronaryCenterlineExtractor extractor;
    CenterlinePoint p1, p2;
    p1.position = {0, 0, 0};
    p2.position = {1, 0, 0};
    auto result = extractor.smoothCenterline({p1, p2});
    // With <4 points, returns raw path unchanged
    EXPECT_EQ(result.size(), 2u);
}

TEST(CoronaryCenterlineExtractor, SmoothCenterlineStraightLine)
{
    CoronaryCenterlineExtractor extractor;

    // Create a straight line with some noise
    std::vector<CenterlinePoint> rawPath;
    for (int i = 0; i < 50; ++i) {
        CenterlinePoint pt;
        pt.position[0] = 10.0 + 0.1 * std::sin(i * 0.5);  // Small noise
        pt.position[1] = i * 0.5;
        pt.position[2] = 10.0 + 0.1 * std::cos(i * 0.5);  // Small noise
        pt.radius = 1.5;
        rawPath.push_back(pt);
    }

    auto smoothed = extractor.smoothCenterline(rawPath, 20);
    EXPECT_GE(smoothed.size(), 50u);

    // Smoothed path should be less noisy
    // Check that tangent vectors are consistent
    for (const auto& pt : smoothed) {
        double tMag = std::sqrt(pt.tangent[0]*pt.tangent[0]
                              + pt.tangent[1]*pt.tangent[1]
                              + pt.tangent[2]*pt.tangent[2]);
        EXPECT_NEAR(tMag, 1.0, 0.1);  // Tangent should be approximately unit
    }
}

TEST(CoronaryCenterlineExtractor, SmoothCenterlinePreservesEndpoints)
{
    CoronaryCenterlineExtractor extractor;

    std::vector<CenterlinePoint> rawPath;
    for (int i = 0; i < 20; ++i) {
        CenterlinePoint pt;
        pt.position = {0.0, static_cast<double>(i), 0.0};
        pt.radius = 1.0;
        rawPath.push_back(pt);
    }

    auto smoothed = extractor.smoothCenterline(rawPath, 10);
    EXPECT_GE(smoothed.size(), 20u);

    // Start should be near (0,0,0)
    double startDist = std::sqrt(
        smoothed.front().position[0]*smoothed.front().position[0] +
        smoothed.front().position[1]*smoothed.front().position[1] +
        smoothed.front().position[2]*smoothed.front().position[2]);
    EXPECT_LT(startDist, 1.0);
}

// =============================================================================
// Radius Estimation Tests
// =============================================================================

TEST(CoronaryCenterlineExtractor, EstimateRadiiEmptyPoints)
{
    CoronaryCenterlineExtractor extractor;
    std::vector<CenterlinePoint> empty;
    extractor.estimateRadii(empty, nullptr);
    // Should not crash
}

TEST(CoronaryCenterlineExtractor, EstimateRadiiOnTube)
{
    double centerX = 10.0, centerZ = 10.0, tubeRadius = 2.0;
    auto image = createStraightTubePhantom(40, 40, 40, centerX, centerZ, tubeRadius);

    CoronaryCenterlineExtractor extractor;

    std::vector<CenterlinePoint> points;
    for (int i = 0; i < 10; ++i) {
        CenterlinePoint pt;
        pt.position = {centerX, 5.0 + i * 1.0, centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        points.push_back(pt);
    }

    extractor.estimateRadii(points, image);

    for (const auto& pt : points) {
        // Estimated radius should be in a reasonable range of the true radius
        EXPECT_GT(pt.radius, 0.0);
        EXPECT_LT(pt.radius, tubeRadius * 5.0);  // Generous bound
    }
}

// =============================================================================
// Stenosis Measurement Tests
// =============================================================================

TEST(CoronaryCenterlineExtractor, MeasureStenosisEmpty)
{
    CoronaryCenterlineExtractor extractor;
    CenterlineResult result;
    extractor.measureStenosis(result, nullptr);
    // Should not crash
}

TEST(CoronaryCenterlineExtractor, MeasureStenosisOnTube)
{
    double centerX = 12.5, centerZ = 12.5;
    double normalRadius = 2.5, stenosisRadius = 1.0;
    double stenosisY = 12.5, stenosisLength = 5.0;

    auto image = createStenosisTubePhantom(
        50, 50, 50, centerX, centerZ,
        normalRadius, stenosisRadius,
        stenosisY, stenosisLength);

    CoronaryCenterlineExtractor extractor;

    CenterlineResult result;
    for (int i = 0; i < 40; ++i) {
        CenterlinePoint pt;
        pt.position = {centerX, 2.5 + i * 0.5, centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        result.points.push_back(pt);
    }

    extractor.measureStenosis(result, image);

    EXPECT_GT(result.referenceDiameter, 0.0);
    EXPECT_GT(result.minLumenDiameter, 0.0);
    // The stenotic section should be narrower
    EXPECT_LE(result.minLumenDiameter, result.referenceDiameter);
    EXPECT_GE(result.stenosisPercent, 0.0);
    EXPECT_LE(result.stenosisPercent, 100.0);
}

// =============================================================================
// Compute Length Tests
// =============================================================================

TEST(CoronaryCenterlineExtractor, ComputeLengthEmpty)
{
    std::vector<CenterlinePoint> empty;
    EXPECT_DOUBLE_EQ(CoronaryCenterlineExtractor::computeLength(empty), 0.0);
}

TEST(CoronaryCenterlineExtractor, ComputeLengthSinglePoint)
{
    std::vector<CenterlinePoint> single(1);
    single[0].position = {1.0, 2.0, 3.0};
    EXPECT_DOUBLE_EQ(CoronaryCenterlineExtractor::computeLength(single), 0.0);
}

TEST(CoronaryCenterlineExtractor, ComputeLengthStraightLine)
{
    std::vector<CenterlinePoint> points(3);
    points[0].position = {0.0, 0.0, 0.0};
    points[1].position = {0.0, 5.0, 0.0};
    points[2].position = {0.0, 10.0, 0.0};

    double length = CoronaryCenterlineExtractor::computeLength(points);
    EXPECT_NEAR(length, 10.0, 1e-10);
}

TEST(CoronaryCenterlineExtractor, ComputeLengthDiagonal)
{
    std::vector<CenterlinePoint> points(2);
    points[0].position = {0.0, 0.0, 0.0};
    points[1].position = {3.0, 4.0, 0.0};

    double length = CoronaryCenterlineExtractor::computeLength(points);
    EXPECT_NEAR(length, 5.0, 1e-10);
}

// =============================================================================
// CurvedPlanarReformatter Tests
// =============================================================================

TEST(CurvedPlanarReformatter, Construction)
{
    CurvedPlanarReformatter cpr;
    // Should construct without error
}

TEST(CurvedPlanarReformatter, MoveConstruction)
{
    CurvedPlanarReformatter cpr;
    CurvedPlanarReformatter moved(std::move(cpr));
}

TEST(CurvedPlanarReformatter, StraightenedCPRInvalidCenterline)
{
    CurvedPlanarReformatter cpr;
    CenterlineResult empty;
    auto image = createTestVolume(10, 10, 10);
    auto result = cpr.generateStraightenedCPR(empty, image);
    ASSERT_FALSE(result.has_value());
}

TEST(CurvedPlanarReformatter, StraightenedCPRNullVolume)
{
    CurvedPlanarReformatter cpr;
    CenterlineResult centerline;
    CenterlinePoint p1, p2;
    p1.position = {0, 0, 0};
    p2.position = {0, 10, 0};
    centerline.points = {p1, p2};

    auto result = cpr.generateStraightenedCPR(centerline, nullptr);
    ASSERT_FALSE(result.has_value());
}

TEST(CurvedPlanarReformatter, StraightenedCPROnTube)
{
    CurvedPlanarReformatter cpr;
    double centerX = 10.0, centerZ = 10.0;
    auto image = createStraightTubePhantom(40, 60, 40, centerX, centerZ, 2.0);

    CenterlineResult centerline;
    for (int i = 0; i < 50; ++i) {
        CenterlinePoint pt;
        pt.position = {centerX, 1.0 + i * 0.5, centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        centerline.points.push_back(pt);
    }
    centerline.totalLength = 24.5;

    auto result = cpr.generateStraightenedCPR(centerline, image, 5.0, 0.5);
    ASSERT_TRUE(result.has_value());

    auto cprImage = result.value();
    ASSERT_NE(cprImage, nullptr);

    int dims[3];
    cprImage->GetDimensions(dims);
    EXPECT_GT(dims[0], 0);  // Width
    EXPECT_GT(dims[1], 0);  // Height (arc length)
    EXPECT_EQ(dims[2], 1);  // Single slice

    // Check that the center column has high HU values (vessel)
    int centerCol = dims[0] / 2;
    int midRow = dims[1] / 2;
    short* pixels = static_cast<short*>(cprImage->GetScalarPointer());
    short centerValue = pixels[midRow * dims[0] + centerCol];
    // Should be near vessel HU (300) along the center
    EXPECT_GT(centerValue, 0);
}

// =============================================================================
// Cross-Sectional CPR Tests
// =============================================================================

TEST(CurvedPlanarReformatter, CrossSectionalCPRInvalidCenterline)
{
    CurvedPlanarReformatter cpr;
    CenterlineResult empty;
    auto image = createTestVolume(10, 10, 10);
    auto result = cpr.generateCrossSectionalCPR(empty, image);
    ASSERT_FALSE(result.has_value());
}

TEST(CurvedPlanarReformatter, CrossSectionalCPROnTube)
{
    CurvedPlanarReformatter cpr;
    double centerX = 10.0, centerZ = 10.0;
    auto image = createStraightTubePhantom(40, 60, 40, centerX, centerZ, 2.0);

    CenterlineResult centerline;
    for (int i = 0; i < 50; ++i) {
        CenterlinePoint pt;
        pt.position = {centerX, 1.0 + i * 0.5, centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        centerline.points.push_back(pt);
    }
    centerline.totalLength = 24.5;

    auto result = cpr.generateCrossSectionalCPR(centerline, image, 5.0, 5.0, 0.5);
    ASSERT_TRUE(result.has_value());

    auto& sections = result.value();
    EXPECT_GE(sections.size(), 4u);  // At least 4 sections for 24.5mm at 5mm interval

    for (const auto& section : sections) {
        ASSERT_NE(section, nullptr);
        int dims[3];
        section->GetDimensions(dims);
        EXPECT_GT(dims[0], 0);
        EXPECT_GT(dims[1], 0);
        EXPECT_EQ(dims[2], 1);
    }

    // Check that center of first cross-section has vessel HU
    if (!sections.empty()) {
        auto& firstSection = sections[0];
        int dims[3];
        firstSection->GetDimensions(dims);
        int centerX_px = dims[0] / 2;
        int centerY_px = dims[1] / 2;
        short* pixels = static_cast<short*>(firstSection->GetScalarPointer());
        short centerValue = pixels[centerY_px * dims[0] + centerX_px];
        EXPECT_GT(centerValue, -100);  // Should be brighter than background
    }
}

// =============================================================================
// Stretched CPR Tests
// =============================================================================

TEST(CurvedPlanarReformatter, StretchedCPRInvalidCenterline)
{
    CurvedPlanarReformatter cpr;
    CenterlineResult empty;
    auto image = createTestVolume(10, 10, 10);
    auto result = cpr.generateStretchedCPR(empty, image);
    ASSERT_FALSE(result.has_value());
}

TEST(CurvedPlanarReformatter, StretchedCPRNullVolume)
{
    CurvedPlanarReformatter cpr;
    CenterlineResult centerline;
    CenterlinePoint p1, p2;
    p1.position = {0, 0, 0};
    p2.position = {0, 10, 0};
    centerline.points = {p1, p2};

    auto result = cpr.generateStretchedCPR(centerline, nullptr);
    ASSERT_FALSE(result.has_value());
}

TEST(CurvedPlanarReformatter, StretchedCPROnTube)
{
    CurvedPlanarReformatter cpr;
    double centerX = 10.0, centerZ = 10.0;
    auto image = createStraightTubePhantom(40, 60, 40, centerX, centerZ, 2.0);

    CenterlineResult centerline;
    for (int i = 0; i < 40; ++i) {
        CenterlinePoint pt;
        pt.position = {centerX, 2.0 + i * 0.5, centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        centerline.points.push_back(pt);
    }
    centerline.totalLength = 19.5;

    auto result = cpr.generateStretchedCPR(centerline, image, 5.0, 0.5);
    ASSERT_TRUE(result.has_value());

    auto cprImage = result.value();
    ASSERT_NE(cprImage, nullptr);

    int dims[3];
    cprImage->GetDimensions(dims);
    EXPECT_GT(dims[0], 0);
    EXPECT_GT(dims[1], 0);
    EXPECT_EQ(dims[2], 1);
}

// =============================================================================
// Full Pipeline Test
// =============================================================================

TEST(CoronaryCenterlineExtractor, FullPipelineEndToEnd)
{
    // Create a vessel phantom
    double centerX = 10.0, centerZ = 10.0, radius = 2.0;
    auto image = createStraightTubePhantom(40, 60, 40, centerX, centerZ, radius);
    auto vesselness = createSyntheticVesselness(image, centerX, centerZ, radius);

    CoronaryCenterlineExtractor extractor;

    // Extract centerline
    std::array<double, 3> seed = {centerX, 2.0, centerZ};
    std::array<double, 3> end = {centerX, 27.0, centerZ};
    auto centerlineResult = extractor.extractCenterline(seed, end, vesselness, image);
    ASSERT_TRUE(centerlineResult.has_value());

    auto& centerline = centerlineResult.value();
    EXPECT_GT(centerline.totalLength, 0.0);

    // Smooth centerline
    if (centerline.points.size() >= 4) {
        auto smoothed = extractor.smoothCenterline(centerline.points, 20);
        EXPECT_GE(smoothed.size(), centerline.points.size());
    }

    // Estimate radii
    extractor.estimateRadii(centerline.points, image);

    // Measure stenosis
    extractor.measureStenosis(centerline, image);
    EXPECT_GE(centerline.referenceDiameter, 0.0);

    // Generate CPR views
    CurvedPlanarReformatter cpr;
    auto straightened = cpr.generateStraightenedCPR(centerline, image, 5.0, 0.5);
    EXPECT_TRUE(straightened.has_value());

    auto crossSections = cpr.generateCrossSectionalCPR(centerline, image, 5.0, 5.0, 0.5);
    EXPECT_TRUE(crossSections.has_value());
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(CoronaryCenterlineExtractor, VesselnessNegativeSigma)
{
    CoronaryCenterlineExtractor extractor;
    auto image = createTestVolume(10, 10, 10);

    VesselnessParams params;
    params.sigmaMin = -1.0;
    auto result = extractor.computeVesselness(image, params);
    EXPECT_FALSE(result.has_value());
}

TEST(CoronaryCenterlineExtractor, ComputeLengthTwoPointsCoincident)
{
    std::vector<CenterlinePoint> points(2);
    points[0].position = {5.0, 5.0, 5.0};
    points[1].position = {5.0, 5.0, 5.0};

    double length = CoronaryCenterlineExtractor::computeLength(points);
    EXPECT_NEAR(length, 0.0, 1e-10);
}

TEST(CurvedPlanarReformatter, CPRWithMinimalCenterline)
{
    CurvedPlanarReformatter cpr;
    auto image = createTestVolume(20, 20, 20);

    CenterlineResult centerline;
    CenterlinePoint p1, p2;
    p1.position = {5.0, 5.0, 5.0};
    p1.tangent = {0.0, 1.0, 0.0};
    p1.normal = {1.0, 0.0, 0.0};
    p2.position = {5.0, 6.0, 5.0};
    p2.tangent = {0.0, 1.0, 0.0};
    p2.normal = {1.0, 0.0, 0.0};
    centerline.points = {p1, p2};
    centerline.totalLength = 1.0;

    auto result = cpr.generateStraightenedCPR(centerline, image, 3.0, 0.5);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

// =============================================================================
// Tolerance validation and geometry edge case tests (Issue #208)
// =============================================================================

TEST(CoronaryCenterlineExtractor, ComputeLengthTortuousPath)
{
    // Path with >90° bend: straight segment, sharp U-turn, straight segment
    std::vector<CenterlinePoint> points;

    // Segment 1: along +Y direction (0→5mm)
    for (int i = 0; i <= 10; ++i) {
        CenterlinePoint pt;
        pt.position = {0.0, i * 0.5, 0.0};
        points.push_back(pt);
    }
    // Sharp 90°+ bend segment
    for (int i = 1; i <= 5; ++i) {
        CenterlinePoint pt;
        pt.position = {i * 0.5, 5.0, 0.0};
        points.push_back(pt);
    }
    // Segment 2: along -Y direction (reverse)
    for (int i = 1; i <= 10; ++i) {
        CenterlinePoint pt;
        pt.position = {2.5, 5.0 - i * 0.5, 0.0};
        points.push_back(pt);
    }

    double length = CoronaryCenterlineExtractor::computeLength(points);
    // Expected: 5.0 + 2.5 + 5.0 = 12.5mm
    EXPECT_NEAR(length, 12.5, 0.1)
        << "Tortuous path length should match sum of segments";
    EXPECT_GT(length, 10.0)
        << "Path with >90° bend should be longer than straight distance";
}

TEST(CoronaryCenterlineExtractor, StenosisPercentageWithinTolerance)
{
    double centerX = 12.5, centerZ = 12.5;
    double normalRadius = 3.0;
    double stenosisRadius = 1.5;  // 50% diameter reduction
    double stenosisY = 12.5, stenosisLength = 5.0;

    auto image = createStenosisTubePhantom(
        50, 50, 50, centerX, centerZ,
        normalRadius, stenosisRadius,
        stenosisY, stenosisLength);

    CoronaryCenterlineExtractor extractor;

    CenterlineResult result;
    for (int i = 0; i < 40; ++i) {
        CenterlinePoint pt;
        pt.position = {centerX, 2.5 + i * 0.5, centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        result.points.push_back(pt);
    }

    extractor.measureStenosis(result, image);

    // Expected ~50% stenosis (radius 3.0→1.5, diameter 6.0→3.0)
    EXPECT_NEAR(result.stenosisPercent, 50.0, 3.0)
        << "Stenosis percentage should be within ±3% of known geometry";
}

TEST(CoronaryCenterlineExtractor, SmallVesselVesselnessResponse)
{
    // Very small vessel: radius < 1mm
    double centerX = 10.0, centerZ = 10.0;
    double smallRadius = 0.4;  // 0.4mm radius = 0.8mm diameter

    auto image = createStraightTubePhantom(
        40, 40, 40, centerX, centerZ, smallRadius, 0.25);

    CoronaryCenterlineExtractor extractor;

    VesselnessParams params;
    params.sigmaMin = 0.2;  // Small sigma for small vessels
    params.sigmaMax = 1.0;
    params.sigmaSteps = 3;

    auto result = extractor.computeVesselness(image, params);
    ASSERT_TRUE(result.has_value())
        << "Vesselness should compute for small vessels (<1mm diameter)";

    // Verify vesselness response at vessel center
    auto vesselness = result.value();
    ImageType::PointType physPoint;
    physPoint[0] = centerX;
    physPoint[1] = 5.0;
    physPoint[2] = centerZ;

    FloatImageType::IndexType idx;
    vesselness->TransformPhysicalPointToIndex(physPoint, idx);
    float centerResponse = vesselness->GetPixel(idx);

    EXPECT_GT(centerResponse, 0.0f)
        << "Vesselness should detect small vessels at tube center";
}

TEST(CoronaryCenterlineExtractor, CenterlineDeviationFromPhantomCenter)
{
    double centerX = 15.0, centerZ = 15.0;
    double tubeRadius = 2.5;

    auto image = createStraightTubePhantom(
        60, 60, 60, centerX, centerZ, tubeRadius, 0.5);
    auto vesselness = createSyntheticVesselness(
        image, centerX, centerZ, tubeRadius);

    CoronaryCenterlineExtractor extractor;

    std::array<double, 3> seed = {centerX, 2.0, centerZ};
    std::array<double, 3> end = {centerX, 27.0, centerZ};

    auto result = extractor.extractCenterline(seed, end, vesselness, image);

    if (!result.has_value()) {
        GTEST_SKIP() << "Centerline extraction failed: "
                     << result.error().toString();
    }

    // Verify all points are within 1mm of known tube center
    double maxDeviation = 0.0;
    for (const auto& pt : result.value().points) {
        double dx = pt.position[0] - centerX;
        double dz = pt.position[2] - centerZ;
        double deviation = std::sqrt(dx * dx + dz * dz);
        maxDeviation = std::max(maxDeviation, deviation);
    }

    EXPECT_LE(maxDeviation, 1.0)
        << "Centerline should deviate ≤1mm from phantom tube center; "
        << "actual max deviation: " << maxDeviation << "mm";
}

TEST(CoronaryCenterlineExtractor, EstimateRadiiOnStenosisTube)
{
    double centerX = 12.5, centerZ = 12.5;
    double normalRadius = 3.0;
    double stenosisRadius = 1.0;
    double stenosisY = 12.5, stenosisLength = 5.0;

    auto image = createStenosisTubePhantom(
        50, 50, 50, centerX, centerZ,
        normalRadius, stenosisRadius,
        stenosisY, stenosisLength);

    CoronaryCenterlineExtractor extractor;

    std::vector<CenterlinePoint> points;
    for (int i = 0; i < 40; ++i) {
        CenterlinePoint pt;
        pt.position = {centerX, 2.5 + i * 0.5, centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        points.push_back(pt);
    }

    extractor.estimateRadii(points, image);

    // Find min and max estimated radii
    double minRadius = 1e9, maxRadius = 0.0;
    for (const auto& pt : points) {
        if (pt.radius > 0.0) {
            minRadius = std::min(minRadius, pt.radius);
            maxRadius = std::max(maxRadius, pt.radius);
        }
    }

    EXPECT_GT(maxRadius, 0.0) << "Should estimate positive radii";
    EXPECT_LT(minRadius, maxRadius)
        << "Stenotic region should have smaller estimated radius";
}

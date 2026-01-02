#include "services/measurement/area_measurement_tool.hpp"
#include "services/measurement/measurement_types.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

namespace dicom_viewer::services {
namespace {

// =============================================================================
// AreaMeasurementTool basic tests (without VTK renderer/interactor)
// =============================================================================

class AreaMeasurementToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        tool_ = std::make_unique<AreaMeasurementTool>();
    }

    std::unique_ptr<AreaMeasurementTool> tool_;
};

TEST_F(AreaMeasurementToolTest, InitialStateHasNoMeasurements) {
    EXPECT_EQ(tool_->getMeasurementCount(), 0);
}

TEST_F(AreaMeasurementToolTest, InitialStateNotDrawing) {
    EXPECT_FALSE(tool_->isDrawing());
}

TEST_F(AreaMeasurementToolTest, InitialStateNoActiveRoiType) {
    EXPECT_FALSE(tool_->getCurrentRoiType().has_value());
}

TEST_F(AreaMeasurementToolTest, GetMeasurementsReturnsEmptyVector) {
    auto measurements = tool_->getMeasurements();
    EXPECT_TRUE(measurements.empty());
}

TEST_F(AreaMeasurementToolTest, GetMeasurementReturnsNulloptForInvalidId) {
    auto measurement = tool_->getMeasurement(999);
    EXPECT_FALSE(measurement.has_value());
}

TEST_F(AreaMeasurementToolTest, DeleteMeasurementFailsForInvalidId) {
    auto result = tool_->deleteMeasurement(999);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

TEST_F(AreaMeasurementToolTest, StartRoiDrawingFailsWithoutRenderer) {
    auto result = tool_->startRoiDrawing(RoiType::Rectangle);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::NoActiveRenderer);
}

TEST_F(AreaMeasurementToolTest, DefaultDisplayParams) {
    auto params = tool_->getDisplayParams();

    EXPECT_FLOAT_EQ(params.lineWidth, 2.0f);
    EXPECT_EQ(params.fontSize, 12);
    EXPECT_DOUBLE_EQ(params.areaFillOpacity, 0.2);
    EXPECT_EQ(params.areaDecimals, 2);
}

TEST_F(AreaMeasurementToolTest, SetDisplayParamsUpdatesValues) {
    MeasurementDisplayParams params;
    params.lineWidth = 3.0f;
    params.fontSize = 14;
    params.areaFillOpacity = 0.5;
    params.areaDecimals = 3;

    tool_->setDisplayParams(params);
    auto retrieved = tool_->getDisplayParams();

    EXPECT_FLOAT_EQ(retrieved.lineWidth, 3.0f);
    EXPECT_EQ(retrieved.fontSize, 14);
    EXPECT_DOUBLE_EQ(retrieved.areaFillOpacity, 0.5);
    EXPECT_EQ(retrieved.areaDecimals, 3);
}

TEST_F(AreaMeasurementToolTest, SetPixelSpacingAcceptsValues) {
    // Should not throw
    tool_->setPixelSpacing(0.5, 0.5, 1.0);
    SUCCEED();
}

TEST_F(AreaMeasurementToolTest, SetCurrentSliceAcceptsValues) {
    // Should not throw
    tool_->setCurrentSlice(50);
    SUCCEED();
}

TEST_F(AreaMeasurementToolTest, CancelCurrentRoiDoesNotThrowWhenNoRoi) {
    // Should not throw when there's no active ROI
    tool_->cancelCurrentRoi();
    SUCCEED();
}

TEST_F(AreaMeasurementToolTest, CompleteCurrentRoiDoesNotThrowWhenNoRoi) {
    // Should not throw when there's no active ROI
    tool_->completeCurrentRoi();
    SUCCEED();
}

TEST_F(AreaMeasurementToolTest, DeleteAllMeasurementsDoesNotThrowWhenEmpty) {
    // Should not throw when there are no measurements
    tool_->deleteAllMeasurements();
    EXPECT_EQ(tool_->getMeasurementCount(), 0);
}

// =============================================================================
// copyRoiToSliceRange tests (logic tests without VTK)
// =============================================================================

TEST_F(AreaMeasurementToolTest, CopyRoiToSliceRangeFailsWithInvalidRange) {
    auto result = tool_->copyRoiToSliceRange(1, 10, 5);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::InvalidParameters);
}

TEST_F(AreaMeasurementToolTest, CopyRoiToSliceRangeFailsWithNonexistentMeasurement) {
    auto result = tool_->copyRoiToSliceRange(999, 0, 10);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

TEST_F(AreaMeasurementToolTest, CopyRoiToSliceFailsWithNonexistentMeasurement) {
    auto result = tool_->copyRoiToSlice(999, 5);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

TEST_F(AreaMeasurementToolTest, UpdateLabelFailsWithInvalidId) {
    auto result = tool_->updateLabel(999, "New Label");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

// =============================================================================
// MeasurementError tests
// =============================================================================

TEST(MeasurementErrorTest, SuccessCodeIsSuccess) {
    MeasurementError error{MeasurementError::Code::Success, ""};
    EXPECT_TRUE(error.isSuccess());
}

TEST(MeasurementErrorTest, InvalidInputIsNotSuccess) {
    MeasurementError error{MeasurementError::Code::InvalidInput, "test"};
    EXPECT_FALSE(error.isSuccess());
}

TEST(MeasurementErrorTest, ToStringContainsMessage) {
    MeasurementError error{MeasurementError::Code::InvalidInput, "test message"};
    std::string str = error.toString();
    EXPECT_NE(str.find("test message"), std::string::npos);
}

TEST(MeasurementErrorTest, AllCodesHaveDistinctValues) {
    std::vector<MeasurementError::Code> codes = {
        MeasurementError::Code::Success,
        MeasurementError::Code::InvalidInput,
        MeasurementError::Code::InvalidParameters,
        MeasurementError::Code::WidgetCreationFailed,
        MeasurementError::Code::NoActiveRenderer,
        MeasurementError::Code::MeasurementNotFound,
        MeasurementError::Code::InternalError
    };

    for (size_t i = 0; i < codes.size(); ++i) {
        for (size_t j = i + 1; j < codes.size(); ++j) {
            EXPECT_NE(static_cast<int>(codes[i]), static_cast<int>(codes[j]));
        }
    }
}

// =============================================================================
// Polygon area calculation tests (Shoelace formula)
// =============================================================================

namespace {
double calculatePolygonArea(const std::vector<Point3D>& points) {
    if (points.size() < 3) return 0.0;

    double area = 0.0;
    size_t n = points.size();

    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += points[i][0] * points[j][1];
        area -= points[j][0] * points[i][1];
    }

    return std::abs(area) / 2.0;
}

double calculatePolygonPerimeter(const std::vector<Point3D>& points) {
    if (points.size() < 2) return 0.0;

    double perimeter = 0.0;
    size_t n = points.size();

    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        double dx = points[j][0] - points[i][0];
        double dy = points[j][1] - points[i][1];
        perimeter += std::sqrt(dx * dx + dy * dy);
    }

    return perimeter;
}

Point3D calculateCentroid(const std::vector<Point3D>& points) {
    if (points.empty()) return {0.0, 0.0, 0.0};

    double sumX = 0.0, sumY = 0.0, sumZ = 0.0;
    for (const auto& p : points) {
        sumX += p[0];
        sumY += p[1];
        sumZ += p[2];
    }

    size_t n = points.size();
    return {sumX / static_cast<double>(n),
            sumY / static_cast<double>(n),
            sumZ / static_cast<double>(n)};
}
}  // namespace

class PolygonGeometryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unit square (0,0) -> (1,0) -> (1,1) -> (0,1)
        unitSquare_ = {
            {0.0, 0.0, 0.0},
            {1.0, 0.0, 0.0},
            {1.0, 1.0, 0.0},
            {0.0, 1.0, 0.0}
        };

        // Create a right triangle (0,0) -> (3,0) -> (0,4)
        rightTriangle_ = {
            {0.0, 0.0, 0.0},
            {3.0, 0.0, 0.0},
            {0.0, 4.0, 0.0}
        };

        // Create a rectangle 10x5
        rectangle10x5_ = {
            {0.0, 0.0, 0.0},
            {10.0, 0.0, 0.0},
            {10.0, 5.0, 0.0},
            {0.0, 5.0, 0.0}
        };
    }

    std::vector<Point3D> unitSquare_;
    std::vector<Point3D> rightTriangle_;
    std::vector<Point3D> rectangle10x5_;
};

TEST_F(PolygonGeometryTest, UnitSquareAreaIsOne) {
    double area = calculatePolygonArea(unitSquare_);
    EXPECT_NEAR(area, 1.0, 1e-10);
}

TEST_F(PolygonGeometryTest, UnitSquarePerimeterIsFour) {
    double perimeter = calculatePolygonPerimeter(unitSquare_);
    EXPECT_NEAR(perimeter, 4.0, 1e-10);
}

TEST_F(PolygonGeometryTest, UnitSquareCentroidIsCenter) {
    Point3D centroid = calculateCentroid(unitSquare_);
    EXPECT_NEAR(centroid[0], 0.5, 1e-10);
    EXPECT_NEAR(centroid[1], 0.5, 1e-10);
}

TEST_F(PolygonGeometryTest, RightTriangleAreaIsHalfBaseTimesHeight) {
    double area = calculatePolygonArea(rightTriangle_);
    // Area = 0.5 * base * height = 0.5 * 3 * 4 = 6
    EXPECT_NEAR(area, 6.0, 1e-10);
}

TEST_F(PolygonGeometryTest, RightTrianglePerimeter) {
    double perimeter = calculatePolygonPerimeter(rightTriangle_);
    // Perimeter = 3 + 4 + 5 = 12 (3-4-5 right triangle)
    EXPECT_NEAR(perimeter, 12.0, 1e-10);
}

TEST_F(PolygonGeometryTest, RectangleAreaIsWidthTimesHeight) {
    double area = calculatePolygonArea(rectangle10x5_);
    EXPECT_NEAR(area, 50.0, 1e-10);
}

TEST_F(PolygonGeometryTest, RectanglePerimeter) {
    double perimeter = calculatePolygonPerimeter(rectangle10x5_);
    // Perimeter = 2 * (10 + 5) = 30
    EXPECT_NEAR(perimeter, 30.0, 1e-10);
}

TEST_F(PolygonGeometryTest, RectangleCentroid) {
    Point3D centroid = calculateCentroid(rectangle10x5_);
    EXPECT_NEAR(centroid[0], 5.0, 1e-10);
    EXPECT_NEAR(centroid[1], 2.5, 1e-10);
}

TEST_F(PolygonGeometryTest, EmptyPolygonAreaIsZero) {
    std::vector<Point3D> empty;
    double area = calculatePolygonArea(empty);
    EXPECT_DOUBLE_EQ(area, 0.0);
}

TEST_F(PolygonGeometryTest, SinglePointAreaIsZero) {
    std::vector<Point3D> single = {{1.0, 2.0, 0.0}};
    double area = calculatePolygonArea(single);
    EXPECT_DOUBLE_EQ(area, 0.0);
}

TEST_F(PolygonGeometryTest, TwoPointsAreaIsZero) {
    std::vector<Point3D> two = {{0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}};
    double area = calculatePolygonArea(two);
    EXPECT_DOUBLE_EQ(area, 0.0);
}

TEST_F(PolygonGeometryTest, TwoPointsPerimeterIsDistance) {
    std::vector<Point3D> two = {{0.0, 0.0, 0.0}, {3.0, 4.0, 0.0}};
    double perimeter = calculatePolygonPerimeter(two);
    // For 2 points, we get distance from 0 to 1 plus distance from 1 to 0 = 2 * 5 = 10
    EXPECT_NEAR(perimeter, 10.0, 1e-10);
}

// =============================================================================
// AreaMeasurement struct tests
// =============================================================================

TEST(AreaMeasurementTest, DefaultValues) {
    AreaMeasurement m;
    EXPECT_EQ(m.id, 0);
    EXPECT_EQ(m.type, RoiType::Rectangle);
    EXPECT_TRUE(m.points.empty());
    EXPECT_DOUBLE_EQ(m.areaMm2, 0.0);
    EXPECT_DOUBLE_EQ(m.areaCm2, 0.0);
    EXPECT_DOUBLE_EQ(m.perimeterMm, 0.0);
    EXPECT_TRUE(m.label.empty());
    EXPECT_TRUE(m.visible);
    EXPECT_EQ(m.sliceIndex, -1);
}

TEST(AreaMeasurementTest, CentroidDefaultIsZero) {
    AreaMeasurement m;
    EXPECT_DOUBLE_EQ(m.centroid[0], 0.0);
    EXPECT_DOUBLE_EQ(m.centroid[1], 0.0);
    EXPECT_DOUBLE_EQ(m.centroid[2], 0.0);
}

TEST(AreaMeasurementTest, RectangleSpecificDefaultsAreZero) {
    AreaMeasurement m;
    EXPECT_DOUBLE_EQ(m.width, 0.0);
    EXPECT_DOUBLE_EQ(m.height, 0.0);
}

TEST(AreaMeasurementTest, EllipseSpecificDefaultsAreZero) {
    AreaMeasurement m;
    EXPECT_DOUBLE_EQ(m.semiAxisA, 0.0);
    EXPECT_DOUBLE_EQ(m.semiAxisB, 0.0);
}

}  // namespace
}  // namespace dicom_viewer::services

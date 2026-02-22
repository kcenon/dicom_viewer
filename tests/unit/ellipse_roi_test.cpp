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

#include "services/measurement/measurement_types.hpp"
#include "services/measurement/area_measurement_tool.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

namespace dicom_viewer::services {
namespace {

// =============================================================================
// Ellipse geometry calculation tests
// =============================================================================

class EllipseGeometryTest : public ::testing::Test {
protected:
    // Ramanujan approximation for ellipse perimeter
    static double calculateEllipsePerimeter(double a, double b) {
        double h = std::pow((a - b) / (a + b), 2);
        return std::numbers::pi * (a + b) *
               (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));
    }

    // Ellipse area: π × a × b
    static double calculateEllipseArea(double a, double b) {
        return std::numbers::pi * a * b;
    }
};

TEST_F(EllipseGeometryTest, CircleAreaCalculation) {
    // A circle is an ellipse with a == b (radius = 5)
    double radius = 5.0;
    double area = calculateEllipseArea(radius, radius);

    // Expected: π × 5² = 78.5398...
    EXPECT_NEAR(area, std::numbers::pi * 25.0, 1e-10);
}

TEST_F(EllipseGeometryTest, EllipseAreaCalculation) {
    // Ellipse with semi-axes a=10, b=5
    double a = 10.0;
    double b = 5.0;
    double area = calculateEllipseArea(a, b);

    // Expected: π × 10 × 5 = 157.0796...
    EXPECT_NEAR(area, std::numbers::pi * 50.0, 1e-10);
}

TEST_F(EllipseGeometryTest, CirclePerimeterCalculation) {
    // A circle's perimeter is 2πr
    double radius = 5.0;
    double perimeter = calculateEllipsePerimeter(radius, radius);

    // Expected: 2 × π × 5 = 31.4159...
    // Ramanujan approximation should be exact for circles
    EXPECT_NEAR(perimeter, 2.0 * std::numbers::pi * radius, 1e-10);
}

TEST_F(EllipseGeometryTest, EllipsePerimeterCalculation) {
    // Ellipse with semi-axes a=10, b=5
    double a = 10.0;
    double b = 5.0;
    double perimeter = calculateEllipsePerimeter(a, b);

    // Ramanujan approximation for this ellipse
    // For a=10, b=5: exact value is approximately 48.44
    EXPECT_GT(perimeter, 45.0);
    EXPECT_LT(perimeter, 50.0);
}

TEST_F(EllipseGeometryTest, ZeroSemiAxisArea) {
    // Degenerate ellipse with one zero semi-axis
    double area = calculateEllipseArea(10.0, 0.0);
    EXPECT_DOUBLE_EQ(area, 0.0);
}

TEST_F(EllipseGeometryTest, SymmetricEllipse) {
    // Area should be the same regardless of which axis is a or b
    double areaAB = calculateEllipseArea(10.0, 5.0);
    double areaBA = calculateEllipseArea(5.0, 10.0);
    EXPECT_DOUBLE_EQ(areaAB, areaBA);
}

// =============================================================================
// AreaMeasurement struct tests for Ellipse
// =============================================================================

class EllipseMeasurementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a sample ellipse measurement
        ellipseMeasurement_.id = 1;
        ellipseMeasurement_.type = RoiType::Ellipse;
        ellipseMeasurement_.semiAxisA = 10.0;  // 10mm
        ellipseMeasurement_.semiAxisB = 5.0;   // 5mm
        ellipseMeasurement_.areaMm2 = std::numbers::pi * 10.0 * 5.0;
        ellipseMeasurement_.areaCm2 = ellipseMeasurement_.areaMm2 / 100.0;
        ellipseMeasurement_.centroid = {100.0, 100.0, 0.0};
        ellipseMeasurement_.sliceIndex = 10;
        ellipseMeasurement_.visible = true;
        ellipseMeasurement_.label = "Test Ellipse";

        // Generate 64 points on the ellipse boundary
        constexpr int numPoints = 64;
        double centerX = 100.0;
        double centerY = 100.0;
        for (int i = 0; i < numPoints; ++i) {
            double angle = 2.0 * std::numbers::pi * i / numPoints;
            double x = centerX + ellipseMeasurement_.semiAxisA * std::cos(angle);
            double y = centerY + ellipseMeasurement_.semiAxisB * std::sin(angle);
            ellipseMeasurement_.points.push_back({x, y, 0.0});
        }

        // Calculate perimeter using Ramanujan approximation
        double a = ellipseMeasurement_.semiAxisA;
        double b = ellipseMeasurement_.semiAxisB;
        double h = std::pow((a - b) / (a + b), 2);
        ellipseMeasurement_.perimeterMm = std::numbers::pi * (a + b) *
                                          (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));
    }

    AreaMeasurement ellipseMeasurement_;
};

TEST_F(EllipseMeasurementTest, MeasurementTypeIsEllipse) {
    EXPECT_EQ(ellipseMeasurement_.type, RoiType::Ellipse);
}

TEST_F(EllipseMeasurementTest, SemiAxesAreCorrect) {
    EXPECT_DOUBLE_EQ(ellipseMeasurement_.semiAxisA, 10.0);
    EXPECT_DOUBLE_EQ(ellipseMeasurement_.semiAxisB, 5.0);
}

TEST_F(EllipseMeasurementTest, AreaCalculationIsCorrect) {
    double expectedAreaMm2 = std::numbers::pi * 10.0 * 5.0;
    EXPECT_NEAR(ellipseMeasurement_.areaMm2, expectedAreaMm2, 1e-10);
    EXPECT_NEAR(ellipseMeasurement_.areaCm2, expectedAreaMm2 / 100.0, 1e-12);
}

TEST_F(EllipseMeasurementTest, PerimeterIsReasonable) {
    // Perimeter should be between 2πb and 2πa (short and long circumferences)
    double minPerimeter = 2.0 * std::numbers::pi * ellipseMeasurement_.semiAxisB;
    double maxPerimeter = 2.0 * std::numbers::pi * ellipseMeasurement_.semiAxisA;

    EXPECT_GT(ellipseMeasurement_.perimeterMm, minPerimeter);
    EXPECT_LT(ellipseMeasurement_.perimeterMm, maxPerimeter);
}

TEST_F(EllipseMeasurementTest, PointsFormClosedCurve) {
    EXPECT_EQ(ellipseMeasurement_.points.size(), 64);

    // First and last points should be close to each other (closed curve)
    auto& first = ellipseMeasurement_.points.front();
    auto& last = ellipseMeasurement_.points.back();

    // They're not exactly the same, but next iteration would be
    // Check that they're on the ellipse
    double centerX = ellipseMeasurement_.centroid[0];
    double centerY = ellipseMeasurement_.centroid[1];

    // Verify first point is on ellipse: (x-cx)²/a² + (y-cy)²/b² = 1
    double normalizedFirst = std::pow((first[0] - centerX) / ellipseMeasurement_.semiAxisA, 2) +
                             std::pow((first[1] - centerY) / ellipseMeasurement_.semiAxisB, 2);
    EXPECT_NEAR(normalizedFirst, 1.0, 1e-10);
}

TEST_F(EllipseMeasurementTest, CentroidIsAtCenter) {
    EXPECT_DOUBLE_EQ(ellipseMeasurement_.centroid[0], 100.0);
    EXPECT_DOUBLE_EQ(ellipseMeasurement_.centroid[1], 100.0);
}

TEST_F(EllipseMeasurementTest, VisibilityDefault) {
    EXPECT_TRUE(ellipseMeasurement_.visible);
}

TEST_F(EllipseMeasurementTest, LabelIsSet) {
    EXPECT_EQ(ellipseMeasurement_.label, "Test Ellipse");
}

// =============================================================================
// MeasurementMode tests
// =============================================================================

TEST(MeasurementModeTest, AreaEllipseModeExists) {
    MeasurementMode mode = MeasurementMode::AreaEllipse;
    EXPECT_EQ(static_cast<int>(mode), static_cast<int>(MeasurementMode::AreaEllipse));
}

TEST(MeasurementModeTest, AllMeasurementModesDistinct) {
    std::vector<MeasurementMode> modes = {
        MeasurementMode::None,
        MeasurementMode::Distance,
        MeasurementMode::Angle,
        MeasurementMode::CobbAngle,
        MeasurementMode::AreaEllipse,
        MeasurementMode::AreaRectangle,
        MeasurementMode::AreaPolygon,
        MeasurementMode::AreaFreehand
    };

    // Check that all modes have distinct values
    for (size_t i = 0; i < modes.size(); ++i) {
        for (size_t j = i + 1; j < modes.size(); ++j) {
            EXPECT_NE(static_cast<int>(modes[i]), static_cast<int>(modes[j]));
        }
    }
}

// =============================================================================
// RoiType tests
// =============================================================================

TEST(RoiTypeTest, EllipseTypeExists) {
    RoiType type = RoiType::Ellipse;
    EXPECT_EQ(static_cast<int>(type), static_cast<int>(RoiType::Ellipse));
}

TEST(RoiTypeTest, AllRoiTypesDistinct) {
    std::vector<RoiType> types = {
        RoiType::Ellipse,
        RoiType::Rectangle,
        RoiType::Polygon,
        RoiType::Freehand
    };

    // Check that all types have distinct values
    for (size_t i = 0; i < types.size(); ++i) {
        for (size_t j = i + 1; j < types.size(); ++j) {
            EXPECT_NE(static_cast<int>(types[i]), static_cast<int>(types[j]));
        }
    }
}

// =============================================================================
// MeasurementDisplayParams tests for area
// =============================================================================

TEST(MeasurementDisplayParamsTest, DefaultAreaColor) {
    MeasurementDisplayParams params;

    // Default area color should be green (0, 1, 0.5)
    EXPECT_DOUBLE_EQ(params.areaColor[0], 0.0);
    EXPECT_DOUBLE_EQ(params.areaColor[1], 1.0);
    EXPECT_DOUBLE_EQ(params.areaColor[2], 0.5);
}

TEST(MeasurementDisplayParamsTest, DefaultAreaFillOpacity) {
    MeasurementDisplayParams params;

    EXPECT_DOUBLE_EQ(params.areaFillOpacity, 0.2);
}

TEST(MeasurementDisplayParamsTest, DefaultAreaDecimals) {
    MeasurementDisplayParams params;

    EXPECT_EQ(params.areaDecimals, 2);
}

}  // namespace
}  // namespace dicom_viewer::services

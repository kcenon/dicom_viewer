#include "services/measurement/linear_measurement_tool.hpp"
#include "services/measurement/measurement_types.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

namespace dicom_viewer::services {
namespace {

// =============================================================================
// Reference implementations for geometry validation
// These replicate the anonymous-namespace functions in linear_measurement_tool.cpp
// to serve as analytical ground truth for measurement accuracy tests.
// =============================================================================

namespace reference {

double calculateDistance(const Point3D& p1, const Point3D& p2) {
    double dx = p2[0] - p1[0];
    double dy = p2[1] - p1[1];
    double dz = p2[2] - p1[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double calculateAngle(const Point3D& p1, const Point3D& vertex, const Point3D& p2) {
    double v1x = p1[0] - vertex[0];
    double v1y = p1[1] - vertex[1];
    double v1z = p1[2] - vertex[2];

    double v2x = p2[0] - vertex[0];
    double v2y = p2[1] - vertex[1];
    double v2z = p2[2] - vertex[2];

    double dot = v1x * v2x + v1y * v2y + v1z * v2z;

    double mag1 = std::sqrt(v1x * v1x + v1y * v1y + v1z * v1z);
    double mag2 = std::sqrt(v2x * v2x + v2y * v2y + v2z * v2z);

    if (mag1 < 1e-10 || mag2 < 1e-10) {
        return 0.0;
    }

    double cosAngle = dot / (mag1 * mag2);
    cosAngle = std::max(-1.0, std::min(1.0, cosAngle));

    return std::acos(cosAngle) * 180.0 / std::numbers::pi;
}

}  // namespace reference

// =============================================================================
// DistanceMeasurement struct tests
// =============================================================================

TEST(DistanceMeasurementTest, DefaultValues) {
    DistanceMeasurement m;
    EXPECT_EQ(m.id, 0);
    EXPECT_DOUBLE_EQ(m.point1[0], 0.0);
    EXPECT_DOUBLE_EQ(m.point1[1], 0.0);
    EXPECT_DOUBLE_EQ(m.point1[2], 0.0);
    EXPECT_DOUBLE_EQ(m.point2[0], 0.0);
    EXPECT_DOUBLE_EQ(m.point2[1], 0.0);
    EXPECT_DOUBLE_EQ(m.point2[2], 0.0);
    EXPECT_DOUBLE_EQ(m.distanceMm, 0.0);
    EXPECT_TRUE(m.label.empty());
    EXPECT_TRUE(m.visible);
    EXPECT_EQ(m.sliceIndex, -1);
}

TEST(DistanceMeasurementTest, CanSetValues) {
    DistanceMeasurement m;
    m.id = 42;
    m.point1 = {1.0, 2.0, 3.0};
    m.point2 = {4.0, 5.0, 6.0};
    m.distanceMm = 5.196;
    m.label = "Tumor diameter";
    m.visible = false;
    m.sliceIndex = 10;

    EXPECT_EQ(m.id, 42);
    EXPECT_DOUBLE_EQ(m.point1[0], 1.0);
    EXPECT_DOUBLE_EQ(m.point2[2], 6.0);
    EXPECT_DOUBLE_EQ(m.distanceMm, 5.196);
    EXPECT_EQ(m.label, "Tumor diameter");
    EXPECT_FALSE(m.visible);
    EXPECT_EQ(m.sliceIndex, 10);
}

// =============================================================================
// AngleMeasurement struct tests
// =============================================================================

TEST(AngleMeasurementTest, DefaultValues) {
    AngleMeasurement m;
    EXPECT_EQ(m.id, 0);
    EXPECT_DOUBLE_EQ(m.vertex[0], 0.0);
    EXPECT_DOUBLE_EQ(m.vertex[1], 0.0);
    EXPECT_DOUBLE_EQ(m.vertex[2], 0.0);
    EXPECT_DOUBLE_EQ(m.point1[0], 0.0);
    EXPECT_DOUBLE_EQ(m.point1[1], 0.0);
    EXPECT_DOUBLE_EQ(m.point1[2], 0.0);
    EXPECT_DOUBLE_EQ(m.point2[0], 0.0);
    EXPECT_DOUBLE_EQ(m.point2[1], 0.0);
    EXPECT_DOUBLE_EQ(m.point2[2], 0.0);
    EXPECT_DOUBLE_EQ(m.angleDegrees, 0.0);
    EXPECT_TRUE(m.label.empty());
    EXPECT_TRUE(m.visible);
    EXPECT_EQ(m.sliceIndex, -1);
    EXPECT_FALSE(m.isCobbAngle);
}

TEST(AngleMeasurementTest, CobbAngleFlag) {
    AngleMeasurement m;
    m.isCobbAngle = true;
    EXPECT_TRUE(m.isCobbAngle);
}

// =============================================================================
// MeasurementMode enum tests
// =============================================================================

TEST(MeasurementModeTest, EnumValuesAreDistinct) {
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

    for (size_t i = 0; i < modes.size(); ++i) {
        for (size_t j = i + 1; j < modes.size(); ++j) {
            EXPECT_NE(static_cast<int>(modes[i]), static_cast<int>(modes[j]));
        }
    }
}

// =============================================================================
// MeasurementDisplayParams struct tests
// =============================================================================

TEST(MeasurementDisplayParamsTest, DefaultValues) {
    MeasurementDisplayParams params;
    EXPECT_FLOAT_EQ(params.lineWidth, 2.0f);
    EXPECT_EQ(params.fontSize, 12);

    // Distance color: Yellow
    EXPECT_DOUBLE_EQ(params.distanceColor[0], 1.0);
    EXPECT_DOUBLE_EQ(params.distanceColor[1], 1.0);
    EXPECT_DOUBLE_EQ(params.distanceColor[2], 0.0);

    // Angle color: Cyan
    EXPECT_DOUBLE_EQ(params.angleColor[0], 0.0);
    EXPECT_DOUBLE_EQ(params.angleColor[1], 1.0);
    EXPECT_DOUBLE_EQ(params.angleColor[2], 1.0);

    // Selected color: Orange
    EXPECT_DOUBLE_EQ(params.selectedColor[0], 1.0);
    EXPECT_DOUBLE_EQ(params.selectedColor[1], 0.5);
    EXPECT_DOUBLE_EQ(params.selectedColor[2], 0.0);

    // Area color: Green
    EXPECT_DOUBLE_EQ(params.areaColor[0], 0.0);
    EXPECT_DOUBLE_EQ(params.areaColor[1], 1.0);
    EXPECT_DOUBLE_EQ(params.areaColor[2], 0.5);

    EXPECT_DOUBLE_EQ(params.areaFillOpacity, 0.2);
    EXPECT_EQ(params.distanceDecimals, 2);
    EXPECT_EQ(params.angleDecimals, 1);
    EXPECT_EQ(params.areaDecimals, 2);
}

// =============================================================================
// LinearMeasurementTool — Construction & Lifecycle
// =============================================================================

class LinearMeasurementToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        tool_ = std::make_unique<LinearMeasurementTool>();
    }

    std::unique_ptr<LinearMeasurementTool> tool_;
};

TEST_F(LinearMeasurementToolTest, DefaultConstruction) {
    LinearMeasurementTool tool;
    // Verify no crash on construction/destruction
}

TEST_F(LinearMeasurementToolTest, MoveConstruction) {
    LinearMeasurementTool tool1;
    LinearMeasurementTool tool2(std::move(tool1));
    // Verify no crash on move construction
}

TEST_F(LinearMeasurementToolTest, MoveAssignment) {
    LinearMeasurementTool tool1;
    LinearMeasurementTool tool2;
    tool2 = std::move(tool1);
    // Verify no crash on move assignment
}

TEST_F(LinearMeasurementToolTest, MoveConstructionTargetIsUsable) {
    LinearMeasurementTool tool1;
    tool1.setPixelSpacing(0.5, 0.5, 2.0);
    tool1.setCurrentSlice(10);

    LinearMeasurementTool tool2(std::move(tool1));

    // Moved-to tool should be fully functional
    EXPECT_EQ(tool2.getMode(), MeasurementMode::None);
    EXPECT_FALSE(tool2.isMeasuring());
    EXPECT_EQ(tool2.getMeasurementCount(), 0u);
    EXPECT_TRUE(tool2.getDistanceMeasurements().empty());
    EXPECT_TRUE(tool2.getAngleMeasurements().empty());
}

TEST_F(LinearMeasurementToolTest, MoveAssignmentTargetIsUsable) {
    LinearMeasurementTool tool1;
    tool1.setPixelSpacing(0.3, 0.3, 1.5);

    LinearMeasurementTool tool2;
    tool2 = std::move(tool1);

    // Moved-to tool should be fully functional
    EXPECT_EQ(tool2.getMode(), MeasurementMode::None);
    EXPECT_EQ(tool2.getMeasurementCount(), 0u);
    auto params = tool2.getDisplayParams();
    EXPECT_FLOAT_EQ(params.lineWidth, 2.0f);
}

// =============================================================================
// LinearMeasurementTool — Initial State
// =============================================================================

TEST_F(LinearMeasurementToolTest, InitialModeIsNone) {
    EXPECT_EQ(tool_->getMode(), MeasurementMode::None);
}

TEST_F(LinearMeasurementToolTest, InitialNotMeasuring) {
    EXPECT_FALSE(tool_->isMeasuring());
}

TEST_F(LinearMeasurementToolTest, InitialMeasurementCountIsZero) {
    EXPECT_EQ(tool_->getMeasurementCount(), 0u);
}

TEST_F(LinearMeasurementToolTest, InitialDistanceMeasurementsEmpty) {
    auto measurements = tool_->getDistanceMeasurements();
    EXPECT_TRUE(measurements.empty());
}

TEST_F(LinearMeasurementToolTest, InitialAngleMeasurementsEmpty) {
    auto measurements = tool_->getAngleMeasurements();
    EXPECT_TRUE(measurements.empty());
}

// =============================================================================
// LinearMeasurementTool — Error Paths (no renderer/interactor)
// =============================================================================

TEST_F(LinearMeasurementToolTest, StartDistanceMeasurementFailsWithoutRenderer) {
    auto result = tool_->startDistanceMeasurement();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::NoActiveRenderer);
}

TEST_F(LinearMeasurementToolTest, StartAngleMeasurementFailsWithoutRenderer) {
    auto result = tool_->startAngleMeasurement();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::NoActiveRenderer);
}

TEST_F(LinearMeasurementToolTest, StartCobbAngleMeasurementFailsWithoutRenderer) {
    auto result = tool_->startCobbAngleMeasurement();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::NoActiveRenderer);
}

TEST_F(LinearMeasurementToolTest, StartDistanceMeasurementErrorMessageMentionsRenderer) {
    auto result = tool_->startDistanceMeasurement();
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("Renderer"), std::string::npos);
}

// --- Renderer set, but interactor not set ---

TEST_F(LinearMeasurementToolTest, StartDistanceMeasurementFailsWithoutInteractor) {
    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    tool_->setRenderer(renderer);
    // Interactor not set
    auto result = tool_->startDistanceMeasurement();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::NoActiveRenderer);
    EXPECT_NE(result.error().message.find("Interactor"), std::string::npos);
}

TEST_F(LinearMeasurementToolTest, StartAngleMeasurementFailsWithoutInteractor) {
    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    tool_->setRenderer(renderer);
    auto result = tool_->startAngleMeasurement();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::NoActiveRenderer);
    EXPECT_NE(result.error().message.find("Interactor"), std::string::npos);
}

TEST_F(LinearMeasurementToolTest, StartCobbAngleMeasurementFailsWithoutInteractor) {
    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    tool_->setRenderer(renderer);
    auto result = tool_->startCobbAngleMeasurement();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::NoActiveRenderer);
}

// --- setRenderer/setInteractor with nullptr ---

TEST_F(LinearMeasurementToolTest, SetRendererNullptrDoesNotCrash) {
    tool_->setRenderer(nullptr);
    EXPECT_EQ(tool_->getMode(), MeasurementMode::None);
}

TEST_F(LinearMeasurementToolTest, SetInteractorNullptrDoesNotCrash) {
    tool_->setInteractor(nullptr);
    EXPECT_EQ(tool_->getMode(), MeasurementMode::None);
}

// =============================================================================
// LinearMeasurementTool — Display Parameters
// =============================================================================

TEST_F(LinearMeasurementToolTest, GetDefaultDisplayParams) {
    auto params = tool_->getDisplayParams();
    EXPECT_FLOAT_EQ(params.lineWidth, 2.0f);
    EXPECT_EQ(params.fontSize, 12);
    EXPECT_EQ(params.distanceDecimals, 2);
    EXPECT_EQ(params.angleDecimals, 1);
}

TEST_F(LinearMeasurementToolTest, SetDisplayParamsUpdatesValues) {
    MeasurementDisplayParams params;
    params.lineWidth = 4.0f;
    params.fontSize = 16;
    params.distanceDecimals = 3;
    params.angleDecimals = 2;
    params.distanceColor = {1.0, 0.0, 0.0};
    params.angleColor = {0.0, 0.0, 1.0};

    tool_->setDisplayParams(params);
    auto retrieved = tool_->getDisplayParams();

    EXPECT_FLOAT_EQ(retrieved.lineWidth, 4.0f);
    EXPECT_EQ(retrieved.fontSize, 16);
    EXPECT_EQ(retrieved.distanceDecimals, 3);
    EXPECT_EQ(retrieved.angleDecimals, 2);
    EXPECT_DOUBLE_EQ(retrieved.distanceColor[0], 1.0);
    EXPECT_DOUBLE_EQ(retrieved.distanceColor[1], 0.0);
    EXPECT_DOUBLE_EQ(retrieved.distanceColor[2], 0.0);
    EXPECT_DOUBLE_EQ(retrieved.angleColor[0], 0.0);
    EXPECT_DOUBLE_EQ(retrieved.angleColor[1], 0.0);
    EXPECT_DOUBLE_EQ(retrieved.angleColor[2], 1.0);
}

// =============================================================================
// LinearMeasurementTool — Pixel Spacing & Slice
// =============================================================================

TEST_F(LinearMeasurementToolTest, SetPixelSpacingAcceptsValues) {
    tool_->setPixelSpacing(0.5, 0.5, 2.0);
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, SetPixelSpacingNonIsotropic) {
    // Non-isotropic spacing common in CT (e.g., 0.5×0.5×2.0)
    tool_->setPixelSpacing(0.488, 0.488, 2.5);
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, SetCurrentSliceAcceptsValues) {
    tool_->setCurrentSlice(100);
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, SetCurrentSliceZero) {
    tool_->setCurrentSlice(0);
    SUCCEED();
}

// =============================================================================
// LinearMeasurementTool — Cancel/Complete without active measurement
// =============================================================================

TEST_F(LinearMeasurementToolTest, CancelMeasurementDoesNotCrashWhenIdle) {
    tool_->cancelMeasurement();
    EXPECT_EQ(tool_->getMode(), MeasurementMode::None);
    EXPECT_FALSE(tool_->isMeasuring());
}

TEST_F(LinearMeasurementToolTest, CompleteMeasurementDoesNotCrashWhenIdle) {
    tool_->completeMeasurement();
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, CancelAfterCancelDoesNotCrash) {
    tool_->cancelMeasurement();
    tool_->cancelMeasurement();
    EXPECT_EQ(tool_->getMode(), MeasurementMode::None);
}

// =============================================================================
// LinearMeasurementTool — Delete Operations
// =============================================================================

TEST_F(LinearMeasurementToolTest, DeleteDistanceMeasurementFailsForInvalidId) {
    auto result = tool_->deleteDistanceMeasurement(999);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

TEST_F(LinearMeasurementToolTest, DeleteAngleMeasurementFailsForInvalidId) {
    auto result = tool_->deleteAngleMeasurement(999);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

TEST_F(LinearMeasurementToolTest, DeleteDistanceMeasurementErrorContainsId) {
    auto result = tool_->deleteDistanceMeasurement(42);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("42"), std::string::npos);
}

TEST_F(LinearMeasurementToolTest, DeleteAngleMeasurementErrorContainsId) {
    auto result = tool_->deleteAngleMeasurement(77);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("77"), std::string::npos);
}

TEST_F(LinearMeasurementToolTest, DeleteAllMeasurementsDoesNotCrashWhenEmpty) {
    tool_->deleteAllMeasurements();
    EXPECT_EQ(tool_->getMeasurementCount(), 0u);
}

TEST_F(LinearMeasurementToolTest, DeleteAllMeasurementsTwiceDoesNotCrash) {
    tool_->deleteAllMeasurements();
    tool_->deleteAllMeasurements();
    EXPECT_EQ(tool_->getMeasurementCount(), 0u);
}

// =============================================================================
// LinearMeasurementTool — Get Operations
// =============================================================================

TEST_F(LinearMeasurementToolTest, GetDistanceMeasurementReturnsNulloptForInvalidId) {
    auto result = tool_->getDistanceMeasurement(999);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LinearMeasurementToolTest, GetAngleMeasurementReturnsNulloptForInvalidId) {
    auto result = tool_->getAngleMeasurement(999);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LinearMeasurementToolTest, GetDistanceMeasurementReturnsNulloptForZeroId) {
    auto result = tool_->getDistanceMeasurement(0);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LinearMeasurementToolTest, GetAngleMeasurementReturnsNulloptForNegativeId) {
    auto result = tool_->getAngleMeasurement(-1);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// LinearMeasurementTool — Label Update Operations
// =============================================================================

TEST_F(LinearMeasurementToolTest, UpdateDistanceLabelFailsForInvalidId) {
    auto result = tool_->updateDistanceLabel(999, "New Label");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

TEST_F(LinearMeasurementToolTest, UpdateAngleLabelFailsForInvalidId) {
    auto result = tool_->updateAngleLabel(999, "New Label");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MeasurementError::Code::MeasurementNotFound);
}

TEST_F(LinearMeasurementToolTest, UpdateDistanceLabelErrorContainsId) {
    auto result = tool_->updateDistanceLabel(55, "Label");
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("55"), std::string::npos);
}

TEST_F(LinearMeasurementToolTest, UpdateAngleLabelErrorContainsId) {
    auto result = tool_->updateAngleLabel(88, "Label");
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("88"), std::string::npos);
}

// =============================================================================
// LinearMeasurementTool — Visibility Operations
// =============================================================================

TEST_F(LinearMeasurementToolTest, SetDistanceVisibilityDoesNotCrashForInvalidId) {
    tool_->setDistanceMeasurementVisibility(999, false);
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, SetAngleVisibilityDoesNotCrashForInvalidId) {
    tool_->setAngleMeasurementVisibility(999, false);
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, ShowMeasurementsForSliceDoesNotCrashWhenEmpty) {
    tool_->showMeasurementsForSlice(5);
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, ShowAllMeasurementsDoesNotCrashWhenEmpty) {
    // sliceIndex = -1 means show all
    tool_->showMeasurementsForSlice(-1);
    SUCCEED();
}

// =============================================================================
// LinearMeasurementTool — Callback Registration
// =============================================================================

TEST_F(LinearMeasurementToolTest, SetDistanceCallbackDoesNotCrash) {
    tool_->setDistanceCompletedCallback([](const DistanceMeasurement& m) {
        // Intentionally empty
    });
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, SetAngleCallbackDoesNotCrash) {
    tool_->setAngleCompletedCallback([](const AngleMeasurement& m) {
        // Intentionally empty
    });
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, SetNullDistanceCallbackDoesNotCrash) {
    tool_->setDistanceCompletedCallback(nullptr);
    SUCCEED();
}

TEST_F(LinearMeasurementToolTest, SetNullAngleCallbackDoesNotCrash) {
    tool_->setAngleCompletedCallback(nullptr);
    SUCCEED();
}

// =============================================================================
// LinearMeasurementTool — Render without renderer
// =============================================================================

TEST_F(LinearMeasurementToolTest, RenderDoesNotCrashWithoutRenderer) {
    tool_->render();
    SUCCEED();
}

// =============================================================================
// Distance Calculation — Analytical Ground Truth
// =============================================================================

class DistanceCalculationTest : public ::testing::Test {};

TEST_F(DistanceCalculationTest, ZeroDistanceSamePoint) {
    Point3D p = {5.0, 10.0, 15.0};
    double dist = reference::calculateDistance(p, p);
    EXPECT_DOUBLE_EQ(dist, 0.0);
}

TEST_F(DistanceCalculationTest, UnitDistanceAlongXAxis) {
    Point3D p1 = {0.0, 0.0, 0.0};
    Point3D p2 = {1.0, 0.0, 0.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_DOUBLE_EQ(dist, 1.0);
}

TEST_F(DistanceCalculationTest, UnitDistanceAlongYAxis) {
    Point3D p1 = {0.0, 0.0, 0.0};
    Point3D p2 = {0.0, 1.0, 0.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_DOUBLE_EQ(dist, 1.0);
}

TEST_F(DistanceCalculationTest, UnitDistanceAlongZAxis) {
    Point3D p1 = {0.0, 0.0, 0.0};
    Point3D p2 = {0.0, 0.0, 1.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_DOUBLE_EQ(dist, 1.0);
}

TEST_F(DistanceCalculationTest, PythagoreanTriangle3D) {
    // 3-4-5 triangle extension to 3D: sqrt(3^2 + 4^2 + 0^2) = 5
    Point3D p1 = {0.0, 0.0, 0.0};
    Point3D p2 = {3.0, 4.0, 0.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_NEAR(dist, 5.0, 1e-10);
}

TEST_F(DistanceCalculationTest, Diagonal3D) {
    // sqrt(1^2 + 1^2 + 1^2) = sqrt(3)
    Point3D p1 = {0.0, 0.0, 0.0};
    Point3D p2 = {1.0, 1.0, 1.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_NEAR(dist, std::sqrt(3.0), 1e-10);
}

TEST_F(DistanceCalculationTest, NegativeCoordinates) {
    Point3D p1 = {-3.0, -4.0, 0.0};
    Point3D p2 = {0.0, 0.0, 0.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_NEAR(dist, 5.0, 1e-10);
}

TEST_F(DistanceCalculationTest, SymmetryProperty) {
    Point3D p1 = {1.5, 2.7, 3.9};
    Point3D p2 = {4.1, 6.3, 8.5};
    double dist1 = reference::calculateDistance(p1, p2);
    double dist2 = reference::calculateDistance(p2, p1);
    EXPECT_DOUBLE_EQ(dist1, dist2);
}

TEST_F(DistanceCalculationTest, LargeDistance) {
    // Simulate measurement across large CT image (500mm)
    Point3D p1 = {0.0, 0.0, 0.0};
    Point3D p2 = {300.0, 400.0, 0.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_NEAR(dist, 500.0, 1e-10);
}

TEST_F(DistanceCalculationTest, SubMillimeterDistance) {
    // Sub-millimeter precision for fine measurements
    Point3D p1 = {0.0, 0.0, 0.0};
    Point3D p2 = {0.001, 0.0, 0.0};
    double dist = reference::calculateDistance(p1, p2);
    EXPECT_NEAR(dist, 0.001, 1e-12);
}

TEST_F(DistanceCalculationTest, NonIsotropicSpacingSimulation) {
    // Simulate distance with non-isotropic spacing (0.5×0.5×2.0 mm)
    // Point (10, 20, 5) in pixel -> (5.0, 10.0, 10.0) in world
    // Point (30, 40, 10) in pixel -> (15.0, 20.0, 20.0) in world
    Point3D p1 = {5.0, 10.0, 10.0};
    Point3D p2 = {15.0, 20.0, 20.0};
    double dist = reference::calculateDistance(p1, p2);
    // sqrt(10^2 + 10^2 + 10^2) = sqrt(300)
    EXPECT_NEAR(dist, std::sqrt(300.0), 1e-10);
}

// =============================================================================
// Angle Calculation — Analytical Ground Truth
// =============================================================================

class AngleCalculationTest : public ::testing::Test {};

TEST_F(AngleCalculationTest, RightAngle90Degrees) {
    // Right angle: X-axis and Y-axis from origin
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {0.0, 1.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 90.0, 0.01);
}

TEST_F(AngleCalculationTest, AcuteAngle45Degrees) {
    // 45 degrees: X-axis and diagonal
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {1.0, 1.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 45.0, 0.01);
}

TEST_F(AngleCalculationTest, AcuteAngle60Degrees) {
    // 60 degrees
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {0.5, std::sqrt(3.0) / 2.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 60.0, 0.01);
}

TEST_F(AngleCalculationTest, ObtuseAngle120Degrees) {
    // 120 degrees
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {-0.5, std::sqrt(3.0) / 2.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 120.0, 0.01);
}

TEST_F(AngleCalculationTest, StraightAngle180Degrees) {
    // 180 degrees (straight line)
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {-1.0, 0.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 180.0, 0.01);
}

TEST_F(AngleCalculationTest, ZeroAngleCollinearPoints) {
    // 0 degrees (same direction)
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {2.0, 0.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 0.0, 0.01);
}

TEST_F(AngleCalculationTest, DegenerateZeroLengthVector) {
    // One arm has zero length (degenerate case)
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {0.0, 0.0, 0.0};  // Same as vertex
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_DOUBLE_EQ(angle, 0.0);
}

TEST_F(AngleCalculationTest, Angle3DNotInPlane) {
    // 3D angle: X-axis and Z-axis
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {0.0, 0.0, 1.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 90.0, 0.01);
}

TEST_F(AngleCalculationTest, SymmetryProperty) {
    // Angle should be the same regardless of arm order
    Point3D p1 = {3.0, 1.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {1.0, 3.0, 0.0};
    double angle1 = reference::calculateAngle(p1, vertex, p2);
    double angle2 = reference::calculateAngle(p2, vertex, p1);
    EXPECT_DOUBLE_EQ(angle1, angle2);
}

TEST_F(AngleCalculationTest, AnglePrecisionWithinTenth) {
    // Verify precision is within ±0.1 degrees
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {std::cos(37.5 * std::numbers::pi / 180.0),
                  std::sin(37.5 * std::numbers::pi / 180.0), 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 37.5, 0.1);
}

// =============================================================================
// Cobb Angle — Analytical Ground Truth
// =============================================================================

class CobbAngleCalculationTest : public ::testing::Test {};

TEST_F(CobbAngleCalculationTest, PerpendicularLines90Degrees) {
    // Two perpendicular lines → Cobb angle = 90°
    // Line 1 direction: along X-axis
    // Line 2 direction: along Y-axis
    // Cobb angle = angle between their perpendiculars = 90°
    // (For perpendicular lines, the angle between them equals the Cobb angle)
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {0.0, 1.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 90.0, 0.01);
}

TEST_F(CobbAngleCalculationTest, ParallelLines0Degrees) {
    // Two parallel lines → Cobb angle = 0°
    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {2.0, 0.0, 0.0};
    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 0.0, 0.01);
}

TEST_F(CobbAngleCalculationTest, StandardScoliosisMeasurement) {
    // Typical scoliosis Cobb angle: ~25° (mild)
    // Line 1 at 0° from horizontal, Line 2 at 25° from horizontal
    double cobbDeg = 25.0;
    double radians = cobbDeg * std::numbers::pi / 180.0;

    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {std::cos(radians), std::sin(radians), 0.0};

    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 25.0, 0.1);
}

TEST_F(CobbAngleCalculationTest, SevereScoliosisMeasurement) {
    // Severe scoliosis: ~50°
    double cobbDeg = 50.0;
    double radians = cobbDeg * std::numbers::pi / 180.0;

    Point3D p1 = {1.0, 0.0, 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {std::cos(radians), std::sin(radians), 0.0};

    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 50.0, 0.1);
}

TEST_F(CobbAngleCalculationTest, ObliqueLinesClinicalRange) {
    // Cobb angle between two oblique endplate lines
    // Line 1 tilted at 10° from horizontal, Line 2 tilted at 45° → Cobb = 35°
    double tilt1 = 10.0 * std::numbers::pi / 180.0;
    double tilt2 = 45.0 * std::numbers::pi / 180.0;

    Point3D p1 = {std::cos(tilt1), std::sin(tilt1), 0.0};
    Point3D vertex = {0.0, 0.0, 0.0};
    Point3D p2 = {std::cos(tilt2), std::sin(tilt2), 0.0};

    double angle = reference::calculateAngle(p1, vertex, p2);
    EXPECT_NEAR(angle, 35.0, 0.1);
}

// =============================================================================
// MeasurementError — Comprehensive toString tests
// =============================================================================

TEST(MeasurementErrorToStringTest, SuccessMessage) {
    MeasurementError err{MeasurementError::Code::Success, ""};
    EXPECT_EQ(err.toString(), "Success");
}

TEST(MeasurementErrorToStringTest, InvalidInputMessage) {
    MeasurementError err{MeasurementError::Code::InvalidInput, "bad coords"};
    EXPECT_NE(err.toString().find("Invalid input"), std::string::npos);
    EXPECT_NE(err.toString().find("bad coords"), std::string::npos);
}

TEST(MeasurementErrorToStringTest, InvalidParametersMessage) {
    MeasurementError err{MeasurementError::Code::InvalidParameters, "negative spacing"};
    EXPECT_NE(err.toString().find("Invalid parameters"), std::string::npos);
    EXPECT_NE(err.toString().find("negative spacing"), std::string::npos);
}

TEST(MeasurementErrorToStringTest, WidgetCreationFailedMessage) {
    MeasurementError err{MeasurementError::Code::WidgetCreationFailed, "VTK error"};
    EXPECT_NE(err.toString().find("Widget creation failed"), std::string::npos);
}

TEST(MeasurementErrorToStringTest, NoActiveRendererMessage) {
    MeasurementError err{MeasurementError::Code::NoActiveRenderer, "not set"};
    EXPECT_NE(err.toString().find("No active renderer"), std::string::npos);
}

TEST(MeasurementErrorToStringTest, MeasurementNotFoundMessage) {
    MeasurementError err{MeasurementError::Code::MeasurementNotFound, "ID 42"};
    EXPECT_NE(err.toString().find("Measurement not found"), std::string::npos);
    EXPECT_NE(err.toString().find("ID 42"), std::string::npos);
}

TEST(MeasurementErrorToStringTest, InternalErrorMessage) {
    MeasurementError err{MeasurementError::Code::InternalError, "null pointer"};
    EXPECT_NE(err.toString().find("Internal error"), std::string::npos);
    EXPECT_NE(err.toString().find("null pointer"), std::string::npos);
}

}  // namespace
}  // namespace dicom_viewer::services

#pragma once

#include <array>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Error information for measurement operations
 */
struct MeasurementError {
    enum class Code {
        Success,
        InvalidInput,
        InvalidParameters,
        WidgetCreationFailed,
        NoActiveRenderer,
        MeasurementNotFound,
        InternalError
    };

    Code code = Code::Success;
    std::string message;

    [[nodiscard]] bool isSuccess() const noexcept {
        return code == Code::Success;
    }

    [[nodiscard]] std::string toString() const {
        switch (code) {
            case Code::Success: return "Success";
            case Code::InvalidInput: return "Invalid input: " + message;
            case Code::InvalidParameters: return "Invalid parameters: " + message;
            case Code::WidgetCreationFailed: return "Widget creation failed: " + message;
            case Code::NoActiveRenderer: return "No active renderer: " + message;
            case Code::MeasurementNotFound: return "Measurement not found: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief 3D point type for measurements
 */
using Point3D = std::array<double, 3>;

/**
 * @brief Distance measurement data
 */
struct DistanceMeasurement {
    /// Unique identifier for this measurement
    int id = 0;

    /// First endpoint in world coordinates (mm)
    Point3D point1{0.0, 0.0, 0.0};

    /// Second endpoint in world coordinates (mm)
    Point3D point2{0.0, 0.0, 0.0};

    /// Calculated distance in millimeters
    double distanceMm = 0.0;

    /// User-defined label for the measurement
    std::string label;

    /// Visibility state
    bool visible = true;

    /// Slice index where measurement was created (-1 for 3D)
    int sliceIndex = -1;
};

/**
 * @brief Angle measurement data
 */
struct AngleMeasurement {
    /// Unique identifier for this measurement
    int id = 0;

    /// Vertex point (center of angle) in world coordinates
    Point3D vertex{0.0, 0.0, 0.0};

    /// First arm endpoint in world coordinates
    Point3D point1{0.0, 0.0, 0.0};

    /// Second arm endpoint in world coordinates
    Point3D point2{0.0, 0.0, 0.0};

    /// Calculated angle in degrees
    double angleDegrees = 0.0;

    /// User-defined label for the measurement
    std::string label;

    /// Visibility state
    bool visible = true;

    /// Slice index where measurement was created (-1 for 3D)
    int sliceIndex = -1;

    /// Flag for Cobb angle measurement (spine)
    bool isCobbAngle = false;
};

/**
 * @brief ROI type for area measurements
 */
enum class RoiType {
    Ellipse,    ///< Ellipse ROI (π × a × b)
    Rectangle,  ///< Rectangle ROI (width × height)
    Polygon,    ///< Polygon ROI (Shoelace formula)
    Freehand    ///< Freehand ROI (polygon approximation)
};

/**
 * @brief Area measurement data
 */
struct AreaMeasurement {
    /// Unique identifier for this measurement
    int id = 0;

    /// ROI type
    RoiType type = RoiType::Rectangle;

    /// Points defining the ROI boundary in world coordinates (mm)
    std::vector<Point3D> points;

    /// Calculated area in square millimeters
    double areaMm2 = 0.0;

    /// Calculated area in square centimeters
    double areaCm2 = 0.0;

    /// Calculated perimeter in millimeters
    double perimeterMm = 0.0;

    /// Centroid position in world coordinates
    Point3D centroid{0.0, 0.0, 0.0};

    /// User-defined label for the measurement
    std::string label;

    /// Visibility state
    bool visible = true;

    /// Slice index where measurement was created (-1 for 3D)
    int sliceIndex = -1;

    /// For ellipse: semi-axis a (horizontal)
    double semiAxisA = 0.0;

    /// For ellipse: semi-axis b (vertical)
    double semiAxisB = 0.0;

    /// For rectangle: width
    double width = 0.0;

    /// For rectangle: height
    double height = 0.0;
};

/**
 * @brief Measurement tool mode
 */
enum class MeasurementMode {
    None,             ///< No measurement active
    Distance,         ///< Distance measurement mode
    Angle,            ///< Angle measurement mode
    CobbAngle,        ///< Cobb angle measurement mode (spine)
    AreaEllipse,      ///< Ellipse area measurement mode
    AreaRectangle,    ///< Rectangle area measurement mode
    AreaPolygon,      ///< Polygon area measurement mode
    AreaFreehand,     ///< Freehand area measurement mode
    PlanePositioning  ///< Interactive 2D measurement plane positioning
};

/**
 * @brief Measurement display parameters
 */
struct MeasurementDisplayParams {
    /// Line width for measurement lines (pixels)
    float lineWidth = 2.0f;

    /// Font size for measurement labels (pixels)
    int fontSize = 12;

    /// Color for distance measurements (RGB, 0-1)
    std::array<double, 3> distanceColor{1.0, 1.0, 0.0};  // Yellow

    /// Color for angle measurements (RGB, 0-1)
    std::array<double, 3> angleColor{0.0, 1.0, 1.0};     // Cyan

    /// Color for selected measurements (RGB, 0-1)
    std::array<double, 3> selectedColor{1.0, 0.5, 0.0};  // Orange

    /// Color for area measurements (RGB, 0-1)
    std::array<double, 3> areaColor{0.0, 1.0, 0.5};  // Green

    /// Area fill opacity (0-1)
    double areaFillOpacity = 0.2;

    /// Number of decimal places for distance display
    int distanceDecimals = 2;

    /// Number of decimal places for angle display
    int angleDecimals = 1;

    /// Number of decimal places for area display
    int areaDecimals = 2;
};

}  // namespace dicom_viewer::services

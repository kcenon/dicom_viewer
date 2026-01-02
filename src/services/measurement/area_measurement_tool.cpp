#include "services/measurement/area_measurement_tool.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <sstream>

#include <vtkActor2D.h>
#include <vtkBorderRepresentation.h>
#include <vtkBorderWidget.h>
#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkContourRepresentation.h>
#include <vtkContourWidget.h>
#include <vtkCoordinate.h>
#include <vtkOrientedGlyphContourRepresentation.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>

namespace dicom_viewer::services {

namespace {

/**
 * @brief Calculate polygon area using Shoelace formula
 * @param points Vector of 3D points (z-coordinate ignored for 2D calculation)
 * @return Area in world coordinate units
 */
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

/**
 * @brief Calculate polygon perimeter
 * @param points Vector of 3D points
 * @return Perimeter in world coordinate units
 */
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

/**
 * @brief Calculate centroid of polygon
 * @param points Vector of 3D points
 * @return Centroid point
 */
Point3D calculateCentroid(const std::vector<Point3D>& points) {
    if (points.empty()) return {0.0, 0.0, 0.0};

    double sumX = 0.0, sumY = 0.0, sumZ = 0.0;
    for (const auto& p : points) {
        sumX += p[0];
        sumY += p[1];
        sumZ += p[2];
    }

    size_t n = points.size();
    return {sumX / n, sumY / n, sumZ / n};
}

/**
 * @brief Calculate ellipse perimeter using Ramanujan approximation
 * @param a Semi-major axis
 * @param b Semi-minor axis
 * @return Approximate perimeter
 */
double calculateEllipsePerimeter(double a, double b) {
    // Ramanujan's second approximation
    double h = std::pow((a - b) / (a + b), 2);
    return std::numbers::pi * (a + b) * (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));
}

}  // namespace

// Widget info for tracking area measurements
struct RectangleWidgetInfo {
    vtkSmartPointer<vtkBorderWidget> widget;
    vtkSmartPointer<vtkTextActor> labelActor;
    AreaMeasurement measurement;
};

struct EllipseWidgetInfo {
    vtkSmartPointer<vtkBorderWidget> widget;       // Bounding box for interaction
    vtkSmartPointer<vtkActor2D> ellipseActor;      // Visual ellipse outline
    vtkSmartPointer<vtkTextActor> labelActor;
    AreaMeasurement measurement;
};

struct ContourWidgetInfo {
    vtkSmartPointer<vtkContourWidget> widget;
    vtkSmartPointer<vtkTextActor> labelActor;
    AreaMeasurement measurement;
};

class AreaMeasurementTool::Impl {
public:
    vtkRenderer* renderer = nullptr;
    vtkRenderWindowInteractor* interactor = nullptr;

    std::optional<RoiType> currentRoiType;
    MeasurementDisplayParams displayParams;

    // Pixel spacing (default to 1mm)
    double spacingX = 1.0;
    double spacingY = 1.0;
    double spacingZ = 1.0;

    int currentSlice = 0;
    int nextMeasurementId = 1;

    // Current active widgets (during drawing)
    vtkSmartPointer<vtkBorderWidget> activeRectangleWidget;
    vtkSmartPointer<vtkBorderWidget> activeEllipseBorderWidget;
    vtkSmartPointer<vtkActor2D> activeEllipseActor;
    vtkSmartPointer<vtkContourWidget> activeContourWidget;
    vtkSmartPointer<vtkTextActor> activeLabelActor;

    // Stored measurements by type
    std::map<int, RectangleWidgetInfo> rectangleWidgets;
    std::map<int, EllipseWidgetInfo> ellipseWidgets;
    std::map<int, ContourWidgetInfo> contourWidgets;

    // Callbacks
    AreaCallback areaCallback;

    // VTK callbacks for interaction events
    vtkSmartPointer<vtkCallbackCommand> rectangleEndCallback;
    vtkSmartPointer<vtkCallbackCommand> ellipseEndCallback;
    vtkSmartPointer<vtkCallbackCommand> contourEndCallback;
    vtkSmartPointer<vtkCallbackCommand> rectangleInteractionCallback;
    vtkSmartPointer<vtkCallbackCommand> ellipseInteractionCallback;

    bool isDrawing = false;

    Impl() {
        rectangleEndCallback = vtkSmartPointer<vtkCallbackCommand>::New();
        ellipseEndCallback = vtkSmartPointer<vtkCallbackCommand>::New();
        contourEndCallback = vtkSmartPointer<vtkCallbackCommand>::New();
        rectangleInteractionCallback = vtkSmartPointer<vtkCallbackCommand>::New();
        ellipseInteractionCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    }

    void configureRectangleWidget(vtkBorderWidget* widget) {
        auto rep = vtkBorderRepresentation::SafeDownCast(widget->GetRepresentation());
        if (!rep) return;

        // Configure border appearance
        auto borderProperty = rep->GetBorderProperty();
        borderProperty->SetColor(displayParams.areaColor[0],
                                 displayParams.areaColor[1],
                                 displayParams.areaColor[2]);
        borderProperty->SetLineWidth(displayParams.lineWidth);

        // Enable resizing from all edges
        rep->SetShowBorderToOn();
        rep->SetProportionalResize(false);
    }

    void configureEllipseBorderWidget(vtkBorderWidget* widget) {
        auto rep = vtkBorderRepresentation::SafeDownCast(widget->GetRepresentation());
        if (!rep) return;

        // Configure border appearance (dashed for ellipse bounding box)
        auto borderProperty = rep->GetBorderProperty();
        borderProperty->SetColor(displayParams.areaColor[0] * 0.5,
                                 displayParams.areaColor[1] * 0.5,
                                 displayParams.areaColor[2] * 0.5);
        borderProperty->SetLineWidth(1.0f);
        borderProperty->SetLineStipplePattern(0xAAAA);  // Dashed line

        // Enable resizing from all edges
        rep->SetShowBorderToOn();
        rep->SetProportionalResize(false);
    }

    vtkSmartPointer<vtkActor2D> createEllipseActor(double centerX, double centerY,
                                                    double semiAxisA, double semiAxisB) {
        // Generate ellipse points
        constexpr int numPoints = 64;
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto lines = vtkSmartPointer<vtkCellArray>::New();

        for (int i = 0; i < numPoints; ++i) {
            double angle = 2.0 * std::numbers::pi * i / numPoints;
            double x = centerX + semiAxisA * std::cos(angle);
            double y = centerY + semiAxisB * std::sin(angle);
            points->InsertNextPoint(x, y, 0.0);
        }

        // Create closed polyline
        lines->InsertNextCell(numPoints + 1);
        for (int i = 0; i < numPoints; ++i) {
            lines->InsertCellPoint(i);
        }
        lines->InsertCellPoint(0);  // Close the loop

        auto polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->SetPoints(points);
        polyData->SetLines(lines);

        auto mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        mapper->SetInputData(polyData);

        // Use world coordinates
        auto coordinate = vtkSmartPointer<vtkCoordinate>::New();
        coordinate->SetCoordinateSystemToWorld();
        mapper->SetTransformCoordinate(coordinate);

        auto actor = vtkSmartPointer<vtkActor2D>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(displayParams.areaColor[0],
                                       displayParams.areaColor[1],
                                       displayParams.areaColor[2]);
        actor->GetProperty()->SetLineWidth(displayParams.lineWidth);

        return actor;
    }

    void updateEllipseActor(vtkActor2D* actor, double centerX, double centerY,
                            double semiAxisA, double semiAxisB) {
        if (!actor) return;

        auto mapper = vtkPolyDataMapper2D::SafeDownCast(actor->GetMapper());
        if (!mapper) return;

        auto polyData = vtkPolyData::SafeDownCast(mapper->GetInput());
        if (!polyData) return;

        auto points = polyData->GetPoints();
        if (!points) return;

        constexpr int numPoints = 64;
        for (int i = 0; i < numPoints; ++i) {
            double angle = 2.0 * std::numbers::pi * i / numPoints;
            double x = centerX + semiAxisA * std::cos(angle);
            double y = centerY + semiAxisB * std::sin(angle);
            points->SetPoint(i, x, y, 0.0);
        }
        points->Modified();
        polyData->Modified();
    }

    AreaMeasurement extractEllipseMeasurement(vtkBorderWidget* widget) {
        AreaMeasurement m;
        m.type = RoiType::Ellipse;

        auto rep = vtkBorderRepresentation::SafeDownCast(widget->GetRepresentation());
        if (!rep) return m;

        // Get position in normalized viewport coordinates
        double* pos = rep->GetPosition();
        double* pos2 = rep->GetPosition2();

        // Convert to world coordinates using renderer
        if (renderer) {
            vtkSmartPointer<vtkCoordinate> coord = vtkSmartPointer<vtkCoordinate>::New();
            coord->SetCoordinateSystemToNormalizedViewport();
            coord->SetViewport(renderer);

            // Get viewport size
            int* size = renderer->GetSize();
            double viewportWidth = size[0];
            double viewportHeight = size[1];

            // Calculate corners in display coordinates
            double displayX1 = pos[0] * viewportWidth;
            double displayY1 = pos[1] * viewportHeight;
            double displayX2 = (pos[0] + pos2[0]) * viewportWidth;
            double displayY2 = (pos[1] + pos2[1]) * viewportHeight;

            // Convert to world coordinates
            coord->SetCoordinateSystemToDisplay();

            coord->SetValue(displayX1, displayY1, 0);
            double* world1 = coord->GetComputedWorldValue(renderer);
            double x1 = world1[0], y1 = world1[1], z1 = world1[2];

            coord->SetValue(displayX2, displayY2, 0);
            double* world2 = coord->GetComputedWorldValue(renderer);
            double x2 = world2[0], y2 = world2[1];

            // Calculate center and semi-axes
            double centerX = (x1 + x2) / 2.0;
            double centerY = (y1 + y2) / 2.0;
            m.semiAxisA = std::abs(x2 - x1) / 2.0;
            m.semiAxisB = std::abs(y2 - y1) / 2.0;

            // Generate ellipse points for ROI definition
            constexpr int numPoints = 64;
            for (int i = 0; i < numPoints; ++i) {
                double angle = 2.0 * std::numbers::pi * i / numPoints;
                double x = centerX + m.semiAxisA * std::cos(angle);
                double y = centerY + m.semiAxisB * std::sin(angle);
                m.points.push_back({x, y, z1});
            }

            // Calculate area: π × a × b
            m.areaMm2 = std::numbers::pi * m.semiAxisA * m.semiAxisB;
            m.areaCm2 = m.areaMm2 / 100.0;

            // Calculate perimeter using Ramanujan approximation
            m.perimeterMm = calculateEllipsePerimeter(m.semiAxisA, m.semiAxisB);

            // Centroid is the center
            m.centroid = {centerX, centerY, z1};
        }

        m.sliceIndex = currentSlice;
        return m;
    }

    void configureContourWidget(vtkContourWidget* widget, RoiType type) {
        auto rep = vtkOrientedGlyphContourRepresentation::SafeDownCast(
            widget->GetRepresentation());
        if (!rep) return;

        // Configure line appearance
        auto lineProperty = rep->GetLinesProperty();
        lineProperty->SetColor(displayParams.areaColor[0],
                               displayParams.areaColor[1],
                               displayParams.areaColor[2]);
        lineProperty->SetLineWidth(displayParams.lineWidth);

        // Configure for different ROI types
        if (type == RoiType::Freehand) {
            // Continuous interaction for freehand
            widget->ContinuousDrawOn();
        } else {
            widget->ContinuousDrawOff();
        }
    }

    vtkSmartPointer<vtkTextActor> createLabelActor(const AreaMeasurement& measurement) {
        auto actor = vtkSmartPointer<vtkTextActor>::New();

        // Format label text
        std::ostringstream label;
        label.precision(displayParams.areaDecimals);
        label << std::fixed << measurement.areaMm2 << " mm²";
        if (measurement.areaCm2 >= 0.01) {
            label << " (" << measurement.areaCm2 << " cm²)";
        }

        actor->SetInput(label.str().c_str());

        // Configure text properties
        auto textProp = actor->GetTextProperty();
        textProp->SetColor(displayParams.areaColor[0],
                          displayParams.areaColor[1],
                          displayParams.areaColor[2]);
        textProp->SetFontSize(displayParams.fontSize);
        textProp->SetBold(true);
        textProp->SetShadow(true);
        textProp->SetJustificationToCentered();

        // Position at centroid
        actor->SetPosition(measurement.centroid[0], measurement.centroid[1]);

        return actor;
    }

    void updateLabelActor(vtkTextActor* actor, const AreaMeasurement& measurement) {
        std::ostringstream label;
        label.precision(displayParams.areaDecimals);
        label << std::fixed << measurement.areaMm2 << " mm²";
        if (measurement.areaCm2 >= 0.01) {
            label << " (" << measurement.areaCm2 << " cm²)";
        }

        actor->SetInput(label.str().c_str());
        actor->SetPosition(measurement.centroid[0], measurement.centroid[1]);
    }

    AreaMeasurement extractRectangleMeasurement(vtkBorderWidget* widget) {
        AreaMeasurement m;
        m.type = RoiType::Rectangle;

        auto rep = vtkBorderRepresentation::SafeDownCast(widget->GetRepresentation());
        if (!rep) return m;

        // Get position in normalized viewport coordinates
        double* pos = rep->GetPosition();
        double* pos2 = rep->GetPosition2();

        // Convert to world coordinates using renderer
        if (renderer) {
            vtkSmartPointer<vtkCoordinate> coord = vtkSmartPointer<vtkCoordinate>::New();
            coord->SetCoordinateSystemToNormalizedViewport();
            coord->SetViewport(renderer);

            // Get viewport size
            int* size = renderer->GetSize();
            double viewportWidth = size[0];
            double viewportHeight = size[1];

            // Calculate corners in display coordinates
            double displayX1 = pos[0] * viewportWidth;
            double displayY1 = pos[1] * viewportHeight;
            double displayX2 = (pos[0] + pos2[0]) * viewportWidth;
            double displayY2 = (pos[1] + pos2[1]) * viewportHeight;

            // Convert to world coordinates
            coord->SetCoordinateSystemToDisplay();

            coord->SetValue(displayX1, displayY1, 0);
            double* world1 = coord->GetComputedWorldValue(renderer);
            Point3D p1 = {world1[0], world1[1], world1[2]};

            coord->SetValue(displayX2, displayY1, 0);
            double* world2 = coord->GetComputedWorldValue(renderer);
            Point3D p2 = {world2[0], world2[1], world2[2]};

            coord->SetValue(displayX2, displayY2, 0);
            double* world3 = coord->GetComputedWorldValue(renderer);
            Point3D p3 = {world3[0], world3[1], world3[2]};

            coord->SetValue(displayX1, displayY2, 0);
            double* world4 = coord->GetComputedWorldValue(renderer);
            Point3D p4 = {world4[0], world4[1], world4[2]};

            m.points = {p1, p2, p3, p4};

            // Calculate dimensions
            m.width = std::sqrt(std::pow(p2[0] - p1[0], 2) + std::pow(p2[1] - p1[1], 2));
            m.height = std::sqrt(std::pow(p4[0] - p1[0], 2) + std::pow(p4[1] - p1[1], 2));

            // Calculate area and perimeter
            m.areaMm2 = m.width * m.height;
            m.areaCm2 = m.areaMm2 / 100.0;
            m.perimeterMm = 2.0 * (m.width + m.height);

            // Calculate centroid
            m.centroid = calculateCentroid(m.points);
        }

        m.sliceIndex = currentSlice;
        return m;
    }

    AreaMeasurement extractContourMeasurement(vtkContourWidget* widget, RoiType type) {
        AreaMeasurement m;
        m.type = type;

        auto rep = widget->GetContourRepresentation();
        if (!rep) return m;

        vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
        rep->GetNodePolyData(polyData);

        if (!polyData || polyData->GetNumberOfPoints() < 3) return m;

        // Extract points
        for (vtkIdType i = 0; i < polyData->GetNumberOfPoints(); ++i) {
            double p[3];
            polyData->GetPoint(i, p);
            m.points.push_back({p[0], p[1], p[2]});
        }

        // Calculate area using Shoelace formula
        m.areaMm2 = calculatePolygonArea(m.points);
        m.areaCm2 = m.areaMm2 / 100.0;

        // Calculate perimeter
        m.perimeterMm = calculatePolygonPerimeter(m.points);

        // Calculate centroid
        m.centroid = calculateCentroid(m.points);

        // For ellipse type, calculate semi-axes from bounding box and use π×a×b formula
        if (type == RoiType::Ellipse && !m.points.empty()) {
            double minX = m.points[0][0], maxX = m.points[0][0];
            double minY = m.points[0][1], maxY = m.points[0][1];
            for (const auto& p : m.points) {
                minX = std::min(minX, p[0]);
                maxX = std::max(maxX, p[0]);
                minY = std::min(minY, p[1]);
                maxY = std::max(maxY, p[1]);
            }
            m.semiAxisA = (maxX - minX) / 2.0;
            m.semiAxisB = (maxY - minY) / 2.0;

            // Recalculate area using ellipse formula: π × a × b
            m.areaMm2 = std::numbers::pi * m.semiAxisA * m.semiAxisB;
            m.areaCm2 = m.areaMm2 / 100.0;

            // Recalculate perimeter using Ramanujan approximation
            m.perimeterMm = calculateEllipsePerimeter(m.semiAxisA, m.semiAxisB);
        }

        m.sliceIndex = currentSlice;
        return m;
    }

    AreaMeasurement createEllipseMeasurement(double centerX, double centerY,
                                              double semiAxisA, double semiAxisB) {
        AreaMeasurement m;
        m.type = RoiType::Ellipse;
        m.semiAxisA = semiAxisA;
        m.semiAxisB = semiAxisB;

        // Generate ellipse points for visualization and area calculation
        constexpr int numPoints = 64;
        for (int i = 0; i < numPoints; ++i) {
            double angle = 2.0 * std::numbers::pi * i / numPoints;
            double x = centerX + semiAxisA * std::cos(angle);
            double y = centerY + semiAxisB * std::sin(angle);
            m.points.push_back({x, y, 0.0});
        }

        // Calculate area: π × a × b
        m.areaMm2 = std::numbers::pi * semiAxisA * semiAxisB;
        m.areaCm2 = m.areaMm2 / 100.0;

        // Calculate perimeter using Ramanujan approximation
        m.perimeterMm = calculateEllipsePerimeter(semiAxisA, semiAxisB);

        // Centroid is the center
        m.centroid = {centerX, centerY, 0.0};
        m.sliceIndex = currentSlice;

        return m;
    }
};

AreaMeasurementTool::AreaMeasurementTool()
    : impl_(std::make_unique<Impl>())
{
}

AreaMeasurementTool::~AreaMeasurementTool() {
    deleteAllMeasurements();
}

AreaMeasurementTool::AreaMeasurementTool(AreaMeasurementTool&&) noexcept = default;
AreaMeasurementTool& AreaMeasurementTool::operator=(AreaMeasurementTool&&) noexcept = default;

void AreaMeasurementTool::setRenderer(vtkRenderer* renderer) {
    impl_->renderer = renderer;
}

void AreaMeasurementTool::setInteractor(vtkRenderWindowInteractor* interactor) {
    impl_->interactor = interactor;
}

void AreaMeasurementTool::setPixelSpacing(double spacingX, double spacingY, double spacingZ) {
    impl_->spacingX = spacingX;
    impl_->spacingY = spacingY;
    impl_->spacingZ = spacingZ;
}

void AreaMeasurementTool::setCurrentSlice(int sliceIndex) {
    impl_->currentSlice = sliceIndex;
}

std::optional<RoiType> AreaMeasurementTool::getCurrentRoiType() const noexcept {
    return impl_->currentRoiType;
}

std::expected<void, MeasurementError> AreaMeasurementTool::startRoiDrawing(RoiType type) {
    if (!impl_->renderer) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::NoActiveRenderer,
            "Renderer not set"
        });
    }

    if (!impl_->interactor) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::NoActiveRenderer,
            "Interactor not set"
        });
    }

    // Cancel any existing drawing
    cancelCurrentRoi();

    switch (type) {
        case RoiType::Rectangle: {
            impl_->activeRectangleWidget = vtkSmartPointer<vtkBorderWidget>::New();
            impl_->activeRectangleWidget->SetInteractor(impl_->interactor);
            impl_->activeRectangleWidget->CreateDefaultRepresentation();
            impl_->configureRectangleWidget(impl_->activeRectangleWidget);

            // Set up interaction callback
            impl_->rectangleInteractionCallback->SetCallback(
                [](vtkObject* caller, unsigned long /*eventId*/, void* clientData, void* /*callData*/) {
                    auto* impl = static_cast<AreaMeasurementTool::Impl*>(clientData);
                    if (impl->activeRectangleWidget && impl->activeLabelActor) {
                        auto m = impl->extractRectangleMeasurement(impl->activeRectangleWidget);
                        impl->updateLabelActor(impl->activeLabelActor, m);
                    }
                });
            impl_->rectangleInteractionCallback->SetClientData(impl_.get());

            // Create label actor for live updates
            AreaMeasurement initialMeasurement;
            initialMeasurement.areaMm2 = 0.0;
            initialMeasurement.areaCm2 = 0.0;
            impl_->activeLabelActor = impl_->createLabelActor(initialMeasurement);
            impl_->renderer->AddViewProp(impl_->activeLabelActor);

            impl_->activeRectangleWidget->AddObserver(
                vtkCommand::InteractionEvent, impl_->rectangleInteractionCallback);

            // Set up end callback
            impl_->rectangleEndCallback->SetCallback(
                [](vtkObject* /*caller*/, unsigned long eventId, void* clientData, void* /*callData*/) {
                    if (eventId == vtkCommand::EndInteractionEvent) {
                        auto* impl = static_cast<AreaMeasurementTool::Impl*>(clientData);
                        if (impl->activeRectangleWidget && impl->isDrawing) {
                            auto measurement = impl->extractRectangleMeasurement(
                                impl->activeRectangleWidget);
                            measurement.id = impl->nextMeasurementId++;

                            // Store widget and measurement
                            RectangleWidgetInfo info;
                            info.widget = impl->activeRectangleWidget;
                            info.labelActor = impl->activeLabelActor;
                            info.measurement = measurement;

                            // Update label with final measurement
                            impl->updateLabelActor(info.labelActor, measurement);

                            impl->rectangleWidgets[measurement.id] = info;

                            // Call callback
                            if (impl->areaCallback) {
                                impl->areaCallback(measurement);
                            }

                            impl->activeRectangleWidget = nullptr;
                            impl->activeLabelActor = nullptr;
                            impl->isDrawing = false;
                            impl->currentRoiType = std::nullopt;
                        }
                    }
                });
            impl_->rectangleEndCallback->SetClientData(impl_.get());

            impl_->activeRectangleWidget->AddObserver(
                vtkCommand::EndInteractionEvent, impl_->rectangleEndCallback);

            impl_->activeRectangleWidget->On();
            impl_->activeRectangleWidget->SelectableOn();
            break;
        }

        case RoiType::Ellipse: {
            // Create border widget for ellipse bounding box
            impl_->activeEllipseBorderWidget = vtkSmartPointer<vtkBorderWidget>::New();
            impl_->activeEllipseBorderWidget->SetInteractor(impl_->interactor);
            impl_->activeEllipseBorderWidget->CreateDefaultRepresentation();
            impl_->configureEllipseBorderWidget(impl_->activeEllipseBorderWidget);

            // Create initial ellipse actor (will be updated during interaction)
            impl_->activeEllipseActor = impl_->createEllipseActor(0, 0, 1, 1);
            impl_->renderer->AddViewProp(impl_->activeEllipseActor);

            // Set up interaction callback for live ellipse updates
            impl_->ellipseInteractionCallback->SetCallback(
                [](vtkObject* /*caller*/, unsigned long /*eventId*/, void* clientData, void* /*callData*/) {
                    auto* impl = static_cast<AreaMeasurementTool::Impl*>(clientData);
                    if (impl->activeEllipseBorderWidget && impl->activeEllipseActor) {
                        auto m = impl->extractEllipseMeasurement(impl->activeEllipseBorderWidget);
                        impl->updateEllipseActor(impl->activeEllipseActor,
                                                 m.centroid[0], m.centroid[1],
                                                 m.semiAxisA, m.semiAxisB);
                        if (impl->activeLabelActor) {
                            impl->updateLabelActor(impl->activeLabelActor, m);
                        }
                    }
                });
            impl_->ellipseInteractionCallback->SetClientData(impl_.get());

            // Create label actor for live updates
            AreaMeasurement initialMeasurement;
            initialMeasurement.areaMm2 = 0.0;
            initialMeasurement.areaCm2 = 0.0;
            impl_->activeLabelActor = impl_->createLabelActor(initialMeasurement);
            impl_->renderer->AddViewProp(impl_->activeLabelActor);

            impl_->activeEllipseBorderWidget->AddObserver(
                vtkCommand::InteractionEvent, impl_->ellipseInteractionCallback);

            // Set up end callback
            impl_->ellipseEndCallback->SetCallback(
                [](vtkObject* /*caller*/, unsigned long eventId, void* clientData, void* /*callData*/) {
                    if (eventId == vtkCommand::EndInteractionEvent) {
                        auto* impl = static_cast<AreaMeasurementTool::Impl*>(clientData);
                        if (impl->activeEllipseBorderWidget && impl->isDrawing) {
                            auto measurement = impl->extractEllipseMeasurement(
                                impl->activeEllipseBorderWidget);
                            measurement.id = impl->nextMeasurementId++;

                            // Store widget and measurement
                            EllipseWidgetInfo info;
                            info.widget = impl->activeEllipseBorderWidget;
                            info.ellipseActor = impl->activeEllipseActor;
                            info.labelActor = impl->activeLabelActor;
                            info.measurement = measurement;

                            // Update label and ellipse with final measurement
                            impl->updateLabelActor(info.labelActor, measurement);
                            impl->updateEllipseActor(info.ellipseActor,
                                                     measurement.centroid[0],
                                                     measurement.centroid[1],
                                                     measurement.semiAxisA,
                                                     measurement.semiAxisB);

                            impl->ellipseWidgets[measurement.id] = info;

                            // Call callback
                            if (impl->areaCallback) {
                                impl->areaCallback(measurement);
                            }

                            impl->activeEllipseBorderWidget = nullptr;
                            impl->activeEllipseActor = nullptr;
                            impl->activeLabelActor = nullptr;
                            impl->isDrawing = false;
                            impl->currentRoiType = std::nullopt;
                        }
                    }
                });
            impl_->ellipseEndCallback->SetClientData(impl_.get());

            impl_->activeEllipseBorderWidget->AddObserver(
                vtkCommand::EndInteractionEvent, impl_->ellipseEndCallback);

            impl_->activeEllipseBorderWidget->On();
            impl_->activeEllipseBorderWidget->SelectableOn();
            break;
        }

        case RoiType::Polygon:
        case RoiType::Freehand: {
            impl_->activeContourWidget = vtkSmartPointer<vtkContourWidget>::New();
            impl_->activeContourWidget->SetInteractor(impl_->interactor);

            auto rep = vtkSmartPointer<vtkOrientedGlyphContourRepresentation>::New();
            impl_->activeContourWidget->SetRepresentation(rep);
            impl_->configureContourWidget(impl_->activeContourWidget, type);

            // Set up end callback
            impl_->contourEndCallback->SetCallback(
                [](vtkObject* /*caller*/, unsigned long eventId, void* clientData, void* /*callData*/) {
                    if (eventId == vtkCommand::EndInteractionEvent) {
                        auto* impl = static_cast<AreaMeasurementTool::Impl*>(clientData);
                        if (impl->activeContourWidget && impl->isDrawing &&
                            impl->currentRoiType.has_value()) {
                            auto measurement = impl->extractContourMeasurement(
                                impl->activeContourWidget, impl->currentRoiType.value());

                            if (measurement.points.size() >= 3) {
                                measurement.id = impl->nextMeasurementId++;

                                // Create label
                                auto labelActor = impl->createLabelActor(measurement);
                                impl->renderer->AddViewProp(labelActor);

                                // Store widget and measurement
                                ContourWidgetInfo info;
                                info.widget = impl->activeContourWidget;
                                info.labelActor = labelActor;
                                info.measurement = measurement;
                                impl->contourWidgets[measurement.id] = info;

                                // Call callback
                                if (impl->areaCallback) {
                                    impl->areaCallback(measurement);
                                }
                            }

                            impl->activeContourWidget = nullptr;
                            impl->isDrawing = false;
                            impl->currentRoiType = std::nullopt;
                        }
                    }
                });
            impl_->contourEndCallback->SetClientData(impl_.get());

            impl_->activeContourWidget->AddObserver(
                vtkCommand::EndInteractionEvent, impl_->contourEndCallback);

            impl_->activeContourWidget->On();
            break;
        }
    }

    impl_->currentRoiType = type;
    impl_->isDrawing = true;

    return {};
}

void AreaMeasurementTool::cancelCurrentRoi() {
    if (impl_->activeRectangleWidget) {
        impl_->activeRectangleWidget->Off();
        impl_->activeRectangleWidget = nullptr;
    }
    if (impl_->activeEllipseBorderWidget) {
        impl_->activeEllipseBorderWidget->Off();
        impl_->activeEllipseBorderWidget = nullptr;
    }
    if (impl_->activeEllipseActor && impl_->renderer) {
        impl_->renderer->RemoveViewProp(impl_->activeEllipseActor);
        impl_->activeEllipseActor = nullptr;
    }
    if (impl_->activeContourWidget) {
        impl_->activeContourWidget->Off();
        impl_->activeContourWidget = nullptr;
    }
    if (impl_->activeLabelActor && impl_->renderer) {
        impl_->renderer->RemoveViewProp(impl_->activeLabelActor);
        impl_->activeLabelActor = nullptr;
    }
    impl_->currentRoiType = std::nullopt;
    impl_->isDrawing = false;
}

void AreaMeasurementTool::completeCurrentRoi() {
    if (impl_->activeRectangleWidget) {
        impl_->activeRectangleWidget->InvokeEvent(vtkCommand::EndInteractionEvent);
    }
    if (impl_->activeEllipseBorderWidget) {
        impl_->activeEllipseBorderWidget->InvokeEvent(vtkCommand::EndInteractionEvent);
    }
    if (impl_->activeContourWidget) {
        impl_->activeContourWidget->InvokeEvent(vtkCommand::EndInteractionEvent);
    }
}

std::vector<AreaMeasurement> AreaMeasurementTool::getMeasurements() const {
    std::vector<AreaMeasurement> result;
    result.reserve(impl_->rectangleWidgets.size() + impl_->ellipseWidgets.size() +
                   impl_->contourWidgets.size());

    for (const auto& [id, info] : impl_->rectangleWidgets) {
        result.push_back(info.measurement);
    }
    for (const auto& [id, info] : impl_->ellipseWidgets) {
        result.push_back(info.measurement);
    }
    for (const auto& [id, info] : impl_->contourWidgets) {
        result.push_back(info.measurement);
    }

    return result;
}

std::optional<AreaMeasurement> AreaMeasurementTool::getMeasurement(int id) const {
    auto rectIt = impl_->rectangleWidgets.find(id);
    if (rectIt != impl_->rectangleWidgets.end()) {
        return rectIt->second.measurement;
    }

    auto ellipseIt = impl_->ellipseWidgets.find(id);
    if (ellipseIt != impl_->ellipseWidgets.end()) {
        return ellipseIt->second.measurement;
    }

    auto contourIt = impl_->contourWidgets.find(id);
    if (contourIt != impl_->contourWidgets.end()) {
        return contourIt->second.measurement;
    }

    return std::nullopt;
}

std::expected<void, MeasurementError> AreaMeasurementTool::deleteMeasurement(int id) {
    auto rectIt = impl_->rectangleWidgets.find(id);
    if (rectIt != impl_->rectangleWidgets.end()) {
        rectIt->second.widget->Off();
        if (impl_->renderer && rectIt->second.labelActor) {
            impl_->renderer->RemoveViewProp(rectIt->second.labelActor);
        }
        impl_->rectangleWidgets.erase(rectIt);
        render();
        return {};
    }

    auto ellipseIt = impl_->ellipseWidgets.find(id);
    if (ellipseIt != impl_->ellipseWidgets.end()) {
        ellipseIt->second.widget->Off();
        if (impl_->renderer) {
            if (ellipseIt->second.ellipseActor) {
                impl_->renderer->RemoveViewProp(ellipseIt->second.ellipseActor);
            }
            if (ellipseIt->second.labelActor) {
                impl_->renderer->RemoveViewProp(ellipseIt->second.labelActor);
            }
        }
        impl_->ellipseWidgets.erase(ellipseIt);
        render();
        return {};
    }

    auto contourIt = impl_->contourWidgets.find(id);
    if (contourIt != impl_->contourWidgets.end()) {
        contourIt->second.widget->Off();
        if (impl_->renderer && contourIt->second.labelActor) {
            impl_->renderer->RemoveViewProp(contourIt->second.labelActor);
        }
        impl_->contourWidgets.erase(contourIt);
        render();
        return {};
    }

    return std::unexpected(MeasurementError{
        MeasurementError::Code::MeasurementNotFound,
        "Area measurement with ID " + std::to_string(id) + " not found"
    });
}

void AreaMeasurementTool::deleteAllMeasurements() {
    // Turn off all rectangle widgets
    for (auto& [id, info] : impl_->rectangleWidgets) {
        info.widget->Off();
        if (impl_->renderer && info.labelActor) {
            impl_->renderer->RemoveViewProp(info.labelActor);
        }
    }
    impl_->rectangleWidgets.clear();

    // Turn off all ellipse widgets
    for (auto& [id, info] : impl_->ellipseWidgets) {
        info.widget->Off();
        if (impl_->renderer) {
            if (info.ellipseActor) {
                impl_->renderer->RemoveViewProp(info.ellipseActor);
            }
            if (info.labelActor) {
                impl_->renderer->RemoveViewProp(info.labelActor);
            }
        }
    }
    impl_->ellipseWidgets.clear();

    // Turn off all contour widgets
    for (auto& [id, info] : impl_->contourWidgets) {
        info.widget->Off();
        if (impl_->renderer && info.labelActor) {
            impl_->renderer->RemoveViewProp(info.labelActor);
        }
    }
    impl_->contourWidgets.clear();

    cancelCurrentRoi();
    render();
}

void AreaMeasurementTool::setMeasurementVisibility(int id, bool visible) {
    auto rectIt = impl_->rectangleWidgets.find(id);
    if (rectIt != impl_->rectangleWidgets.end()) {
        rectIt->second.measurement.visible = visible;
        if (visible) {
            rectIt->second.widget->On();
            if (rectIt->second.labelActor) {
                rectIt->second.labelActor->SetVisibility(true);
            }
        } else {
            rectIt->second.widget->Off();
            if (rectIt->second.labelActor) {
                rectIt->second.labelActor->SetVisibility(false);
            }
        }
        render();
        return;
    }

    auto ellipseIt = impl_->ellipseWidgets.find(id);
    if (ellipseIt != impl_->ellipseWidgets.end()) {
        ellipseIt->second.measurement.visible = visible;
        if (visible) {
            ellipseIt->second.widget->On();
            if (ellipseIt->second.ellipseActor) {
                ellipseIt->second.ellipseActor->SetVisibility(true);
            }
            if (ellipseIt->second.labelActor) {
                ellipseIt->second.labelActor->SetVisibility(true);
            }
        } else {
            ellipseIt->second.widget->Off();
            if (ellipseIt->second.ellipseActor) {
                ellipseIt->second.ellipseActor->SetVisibility(false);
            }
            if (ellipseIt->second.labelActor) {
                ellipseIt->second.labelActor->SetVisibility(false);
            }
        }
        render();
        return;
    }

    auto contourIt = impl_->contourWidgets.find(id);
    if (contourIt != impl_->contourWidgets.end()) {
        contourIt->second.measurement.visible = visible;
        if (visible) {
            contourIt->second.widget->On();
            if (contourIt->second.labelActor) {
                contourIt->second.labelActor->SetVisibility(true);
            }
        } else {
            contourIt->second.widget->Off();
            if (contourIt->second.labelActor) {
                contourIt->second.labelActor->SetVisibility(false);
            }
        }
        render();
    }
}

void AreaMeasurementTool::showMeasurementsForSlice(int sliceIndex) {
    for (auto& [id, info] : impl_->rectangleWidgets) {
        bool show = (sliceIndex == -1 || info.measurement.sliceIndex == sliceIndex);
        if (show && info.measurement.visible) {
            info.widget->On();
            if (info.labelActor) info.labelActor->SetVisibility(true);
        } else {
            info.widget->Off();
            if (info.labelActor) info.labelActor->SetVisibility(false);
        }
    }

    for (auto& [id, info] : impl_->ellipseWidgets) {
        bool show = (sliceIndex == -1 || info.measurement.sliceIndex == sliceIndex);
        if (show && info.measurement.visible) {
            info.widget->On();
            if (info.ellipseActor) info.ellipseActor->SetVisibility(true);
            if (info.labelActor) info.labelActor->SetVisibility(true);
        } else {
            info.widget->Off();
            if (info.ellipseActor) info.ellipseActor->SetVisibility(false);
            if (info.labelActor) info.labelActor->SetVisibility(false);
        }
    }

    for (auto& [id, info] : impl_->contourWidgets) {
        bool show = (sliceIndex == -1 || info.measurement.sliceIndex == sliceIndex);
        if (show && info.measurement.visible) {
            info.widget->On();
            if (info.labelActor) info.labelActor->SetVisibility(true);
        } else {
            info.widget->Off();
            if (info.labelActor) info.labelActor->SetVisibility(false);
        }
    }

    render();
}

std::expected<int, MeasurementError>
AreaMeasurementTool::copyRoiToSlice(int measurementId, int targetSlice) {
    auto measurementOpt = getMeasurement(measurementId);
    if (!measurementOpt) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::MeasurementNotFound,
            "Measurement with ID " + std::to_string(measurementId) + " not found"
        });
    }

    auto measurement = measurementOpt.value();
    int originalSlice = measurement.sliceIndex;
    measurement.id = impl_->nextMeasurementId++;
    measurement.sliceIndex = targetSlice;

    // Adjust Z coordinates for new slice (simplified - assumes uniform spacing)
    double zOffset = (targetSlice - originalSlice) * impl_->spacingZ;
    for (auto& p : measurement.points) {
        p[2] += zOffset;
    }
    measurement.centroid[2] += zOffset;

    if (!impl_->interactor || !impl_->renderer) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::NoActiveRenderer,
            "Renderer or interactor not set"
        });
    }

    // Handle ellipse ROI type separately
    if (measurement.type == RoiType::Ellipse) {
        // Create border widget for ellipse
        auto widget = vtkSmartPointer<vtkBorderWidget>::New();
        widget->SetInteractor(impl_->interactor);
        widget->CreateDefaultRepresentation();
        impl_->configureEllipseBorderWidget(widget);

        // Create ellipse actor
        auto ellipseActor = impl_->createEllipseActor(
            measurement.centroid[0], measurement.centroid[1],
            measurement.semiAxisA, measurement.semiAxisB);
        impl_->renderer->AddViewProp(ellipseActor);

        // Create label
        auto labelActor = impl_->createLabelActor(measurement);
        impl_->renderer->AddViewProp(labelActor);

        EllipseWidgetInfo info;
        info.widget = widget;
        info.ellipseActor = ellipseActor;
        info.labelActor = labelActor;
        info.measurement = measurement;
        impl_->ellipseWidgets[measurement.id] = info;

        widget->On();
    } else {
        // Store as a contour widget for other ROI types
        auto widget = vtkSmartPointer<vtkContourWidget>::New();
        widget->SetInteractor(impl_->interactor);

        auto rep = vtkSmartPointer<vtkOrientedGlyphContourRepresentation>::New();
        widget->SetRepresentation(rep);
        impl_->configureContourWidget(widget, measurement.type);

        // Initialize with points
        widget->Initialize();
        for (const auto& p : measurement.points) {
            rep->AddNodeAtWorldPosition(p[0], p[1], p[2]);
        }
        widget->CloseLoop();

        // Create label
        auto labelActor = impl_->createLabelActor(measurement);
        impl_->renderer->AddViewProp(labelActor);

        ContourWidgetInfo info;
        info.widget = widget;
        info.labelActor = labelActor;
        info.measurement = measurement;
        impl_->contourWidgets[measurement.id] = info;

        widget->On();
    }

    render();
    return measurement.id;
}

std::expected<std::vector<int>, MeasurementError>
AreaMeasurementTool::copyRoiToSliceRange(int measurementId, int startSlice, int endSlice) {
    // Validate range
    if (startSlice > endSlice) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::InvalidParameters,
            "Start slice (" + std::to_string(startSlice) +
            ") must be less than or equal to end slice (" +
            std::to_string(endSlice) + ")"
        });
    }

    // Check if measurement exists
    auto measurementOpt = getMeasurement(measurementId);
    if (!measurementOpt) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::MeasurementNotFound,
            "Measurement with ID " + std::to_string(measurementId) + " not found"
        });
    }

    std::vector<int> newIds;
    newIds.reserve(static_cast<size_t>(endSlice - startSlice + 1));

    // Copy to each slice in range
    for (int slice = startSlice; slice <= endSlice; ++slice) {
        // Skip the original slice to avoid duplication
        if (slice == measurementOpt->sliceIndex) {
            continue;
        }

        auto result = copyRoiToSlice(measurementId, slice);
        if (!result) {
            // If one copy fails, delete all previously created copies and return error
            for (int id : newIds) {
                deleteMeasurement(id);
            }
            return std::unexpected(result.error());
        }
        newIds.push_back(result.value());
    }

    return newIds;
}

void AreaMeasurementTool::setDisplayParams(const MeasurementDisplayParams& params) {
    impl_->displayParams = params;

    // Update existing widgets
    for (auto& [id, info] : impl_->rectangleWidgets) {
        impl_->configureRectangleWidget(info.widget);
        if (info.labelActor) {
            impl_->updateLabelActor(info.labelActor, info.measurement);
        }
    }
    for (auto& [id, info] : impl_->ellipseWidgets) {
        impl_->configureEllipseBorderWidget(info.widget);
        if (info.ellipseActor) {
            info.ellipseActor->GetProperty()->SetColor(params.areaColor[0],
                                                       params.areaColor[1],
                                                       params.areaColor[2]);
            info.ellipseActor->GetProperty()->SetLineWidth(params.lineWidth);
        }
        if (info.labelActor) {
            impl_->updateLabelActor(info.labelActor, info.measurement);
        }
    }
    for (auto& [id, info] : impl_->contourWidgets) {
        impl_->configureContourWidget(info.widget, info.measurement.type);
        if (info.labelActor) {
            impl_->updateLabelActor(info.labelActor, info.measurement);
        }
    }

    render();
}

MeasurementDisplayParams AreaMeasurementTool::getDisplayParams() const {
    return impl_->displayParams;
}

void AreaMeasurementTool::setMeasurementCompletedCallback(AreaCallback callback) {
    impl_->areaCallback = std::move(callback);
}

std::expected<void, MeasurementError>
AreaMeasurementTool::updateLabel(int id, const std::string& label) {
    auto rectIt = impl_->rectangleWidgets.find(id);
    if (rectIt != impl_->rectangleWidgets.end()) {
        rectIt->second.measurement.label = label;
        return {};
    }

    auto ellipseIt = impl_->ellipseWidgets.find(id);
    if (ellipseIt != impl_->ellipseWidgets.end()) {
        ellipseIt->second.measurement.label = label;
        return {};
    }

    auto contourIt = impl_->contourWidgets.find(id);
    if (contourIt != impl_->contourWidgets.end()) {
        contourIt->second.measurement.label = label;
        return {};
    }

    return std::unexpected(MeasurementError{
        MeasurementError::Code::MeasurementNotFound,
        "Area measurement with ID " + std::to_string(id) + " not found"
    });
}

size_t AreaMeasurementTool::getMeasurementCount() const noexcept {
    return impl_->rectangleWidgets.size() + impl_->ellipseWidgets.size() +
           impl_->contourWidgets.size();
}

bool AreaMeasurementTool::isDrawing() const noexcept {
    return impl_->isDrawing;
}

void AreaMeasurementTool::render() {
    if (impl_->renderer && impl_->renderer->GetRenderWindow()) {
        impl_->renderer->GetRenderWindow()->Render();
    }
}

}  // namespace dicom_viewer::services

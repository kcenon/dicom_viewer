#include "services/measurement/linear_measurement_tool.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

#include <vtkAngleRepresentation2D.h>
#include <vtkAngleWidget.h>
#include <vtkAxisActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkDistanceRepresentation2D.h>
#include <vtkDistanceWidget.h>
#include <vtkHandleWidget.h>
#include <vtkLeaderActor2D.h>
#include <vtkPointHandleRepresentation2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTextProperty.h>

namespace dicom_viewer::services {

namespace {

// Calculate distance between two 3D points
double calculateDistance(const Point3D& p1, const Point3D& p2) {
    double dx = p2[0] - p1[0];
    double dy = p2[1] - p1[1];
    double dz = p2[2] - p1[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Calculate angle between three points (vertex at p2)
double calculateAngle(const Point3D& p1, const Point3D& vertex, const Point3D& p2) {
    // Vectors from vertex to endpoints
    double v1x = p1[0] - vertex[0];
    double v1y = p1[1] - vertex[1];
    double v1z = p1[2] - vertex[2];

    double v2x = p2[0] - vertex[0];
    double v2y = p2[1] - vertex[1];
    double v2z = p2[2] - vertex[2];

    // Dot product
    double dot = v1x * v2x + v1y * v2y + v1z * v2z;

    // Magnitudes
    double mag1 = std::sqrt(v1x * v1x + v1y * v1y + v1z * v1z);
    double mag2 = std::sqrt(v2x * v2x + v2y * v2y + v2z * v2z);

    if (mag1 < 1e-10 || mag2 < 1e-10) {
        return 0.0;
    }

    double cosAngle = dot / (mag1 * mag2);
    // Clamp to [-1, 1] to handle floating point errors
    cosAngle = std::max(-1.0, std::min(1.0, cosAngle));

    return std::acos(cosAngle) * 180.0 / M_PI;  // Convert to degrees
}

}  // namespace

// Widget info for tracking measurements
struct DistanceWidgetInfo {
    vtkSmartPointer<vtkDistanceWidget> widget;
    DistanceMeasurement measurement;
};

struct AngleWidgetInfo {
    vtkSmartPointer<vtkAngleWidget> widget;
    AngleMeasurement measurement;
};

class LinearMeasurementTool::Impl {
public:
    vtkRenderer* renderer = nullptr;
    vtkRenderWindowInteractor* interactor = nullptr;

    MeasurementMode mode = MeasurementMode::None;
    MeasurementDisplayParams displayParams;

    // Pixel spacing (default to 1mm)
    double spacingX = 1.0;
    double spacingY = 1.0;
    double spacingZ = 1.0;

    int currentSlice = 0;
    int nextDistanceId = 1;
    int nextAngleId = 1;

    // Current active widget (during measurement)
    vtkSmartPointer<vtkDistanceWidget> activeDistanceWidget;
    vtkSmartPointer<vtkAngleWidget> activeAngleWidget;

    // Stored measurements
    std::map<int, DistanceWidgetInfo> distanceWidgets;
    std::map<int, AngleWidgetInfo> angleWidgets;

    // Callbacks
    DistanceCallback distanceCallback;
    AngleCallback angleCallback;

    // VTK callback for distance measurement placement
    vtkSmartPointer<vtkCallbackCommand> distanceEndCallback;
    vtkSmartPointer<vtkCallbackCommand> angleEndCallback;

    bool isMeasuring = false;

    Impl() {
        distanceEndCallback = vtkSmartPointer<vtkCallbackCommand>::New();
        angleEndCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    }

    void configureDistanceWidget(vtkDistanceWidget* widget) {
        auto rep = vtkDistanceRepresentation2D::SafeDownCast(widget->GetRepresentation());
        if (!rep) return;

        // Configure appearance
        auto axisProperty = rep->GetAxisProperty();
        axisProperty->SetColor(displayParams.distanceColor[0],
                              displayParams.distanceColor[1],
                              displayParams.distanceColor[2]);
        axisProperty->SetLineWidth(displayParams.lineWidth);

        // Configure label format
        std::ostringstream format;
        format << "%." << displayParams.distanceDecimals << "f mm";
        rep->SetLabelFormat(format.str().c_str());

        // Point handle appearance
        auto handleRep = vtkPointHandleRepresentation2D::SafeDownCast(
            rep->GetPoint1Representation());
        if (handleRep) {
            handleRep->GetProperty()->SetColor(displayParams.distanceColor[0],
                                               displayParams.distanceColor[1],
                                               displayParams.distanceColor[2]);
        }
        handleRep = vtkPointHandleRepresentation2D::SafeDownCast(
            rep->GetPoint2Representation());
        if (handleRep) {
            handleRep->GetProperty()->SetColor(displayParams.distanceColor[0],
                                               displayParams.distanceColor[1],
                                               displayParams.distanceColor[2]);
        }
    }

    void configureAngleWidget(vtkAngleWidget* widget) {
        auto rep = vtkAngleRepresentation2D::SafeDownCast(widget->GetRepresentation());
        if (!rep) return;

        // Configure ray appearance
        auto ray1 = rep->GetRay1();
        auto ray2 = rep->GetRay2();

        if (ray1) {
            ray1->GetProperty()->SetColor(displayParams.angleColor[0],
                                          displayParams.angleColor[1],
                                          displayParams.angleColor[2]);
            ray1->GetProperty()->SetLineWidth(displayParams.lineWidth);
        }

        if (ray2) {
            ray2->GetProperty()->SetColor(displayParams.angleColor[0],
                                          displayParams.angleColor[1],
                                          displayParams.angleColor[2]);
            ray2->GetProperty()->SetLineWidth(displayParams.lineWidth);
        }

        // Configure arc
        auto arc = rep->GetArc();
        if (arc) {
            arc->GetProperty()->SetColor(displayParams.angleColor[0],
                                         displayParams.angleColor[1],
                                         displayParams.angleColor[2]);
        }

        // Label format
        std::ostringstream format;
        format << "%." << displayParams.angleDecimals << "f°";
        rep->SetLabelFormat(format.str().c_str());
    }

    DistanceMeasurement extractDistanceMeasurement(vtkDistanceWidget* widget) {
        DistanceMeasurement m;
        auto rep = vtkDistanceRepresentation2D::SafeDownCast(widget->GetRepresentation());
        if (rep) {
            double p1[3], p2[3];
            rep->GetPoint1WorldPosition(p1);
            rep->GetPoint2WorldPosition(p2);

            m.point1 = {p1[0], p1[1], p1[2]};
            m.point2 = {p2[0], p2[1], p2[2]};
            m.distanceMm = rep->GetDistance();
            m.sliceIndex = currentSlice;
        }
        return m;
    }

    AngleMeasurement extractAngleMeasurement(vtkAngleWidget* widget) {
        AngleMeasurement m;
        auto rep = vtkAngleRepresentation2D::SafeDownCast(widget->GetRepresentation());
        if (rep) {
            double p1[3], vertex[3], p2[3];
            rep->GetPoint1WorldPosition(p1);
            rep->GetCenterWorldPosition(vertex);
            rep->GetPoint2WorldPosition(p2);

            m.point1 = {p1[0], p1[1], p1[2]};
            m.vertex = {vertex[0], vertex[1], vertex[2]};
            m.point2 = {p2[0], p2[1], p2[2]};
            m.angleDegrees = rep->GetAngle() * 180.0 / M_PI;
            m.sliceIndex = currentSlice;
        }
        return m;
    }
};

LinearMeasurementTool::LinearMeasurementTool()
    : impl_(std::make_unique<Impl>())
{
}

LinearMeasurementTool::~LinearMeasurementTool() {
    deleteAllMeasurements();
}

LinearMeasurementTool::LinearMeasurementTool(LinearMeasurementTool&&) noexcept = default;
LinearMeasurementTool& LinearMeasurementTool::operator=(LinearMeasurementTool&&) noexcept = default;

void LinearMeasurementTool::setRenderer(vtkRenderer* renderer) {
    impl_->renderer = renderer;
}

void LinearMeasurementTool::setInteractor(vtkRenderWindowInteractor* interactor) {
    impl_->interactor = interactor;
}

void LinearMeasurementTool::setPixelSpacing(double spacingX, double spacingY, double spacingZ) {
    impl_->spacingX = spacingX;
    impl_->spacingY = spacingY;
    impl_->spacingZ = spacingZ;
}

void LinearMeasurementTool::setCurrentSlice(int sliceIndex) {
    impl_->currentSlice = sliceIndex;
}

MeasurementMode LinearMeasurementTool::getMode() const noexcept {
    return impl_->mode;
}

std::expected<void, MeasurementError> LinearMeasurementTool::startDistanceMeasurement() {
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

    // Cancel any existing measurement
    cancelMeasurement();

    // Create new distance widget
    impl_->activeDistanceWidget = vtkSmartPointer<vtkDistanceWidget>::New();
    impl_->activeDistanceWidget->SetInteractor(impl_->interactor);
    impl_->activeDistanceWidget->CreateDefaultRepresentation();
    impl_->configureDistanceWidget(impl_->activeDistanceWidget);

    // Set up callback for when measurement is placed
    impl_->distanceEndCallback->SetCallback([](vtkObject* caller, unsigned long eventId,
                                               void* clientData, void* /*callData*/) {
        if (eventId == vtkCommand::EndInteractionEvent) {
            auto* impl = static_cast<LinearMeasurementTool::Impl*>(clientData);
            if (impl->activeDistanceWidget && impl->isMeasuring) {
                // Extract measurement data
                auto measurement = impl->extractDistanceMeasurement(
                    impl->activeDistanceWidget);
                measurement.id = impl->nextDistanceId++;

                // Store widget and measurement
                DistanceWidgetInfo info;
                info.widget = impl->activeDistanceWidget;
                info.measurement = measurement;
                impl->distanceWidgets[measurement.id] = info;

                // Call callback
                if (impl->distanceCallback) {
                    impl->distanceCallback(measurement);
                }

                impl->activeDistanceWidget = nullptr;
                impl->isMeasuring = false;
                impl->mode = MeasurementMode::None;
            }
        }
    });
    impl_->distanceEndCallback->SetClientData(impl_.get());

    impl_->activeDistanceWidget->AddObserver(
        vtkCommand::EndInteractionEvent, impl_->distanceEndCallback);

    impl_->activeDistanceWidget->On();
    impl_->mode = MeasurementMode::Distance;
    impl_->isMeasuring = true;

    return {};
}

std::expected<void, MeasurementError> LinearMeasurementTool::startAngleMeasurement() {
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

    // Cancel any existing measurement
    cancelMeasurement();

    // Create new angle widget
    impl_->activeAngleWidget = vtkSmartPointer<vtkAngleWidget>::New();
    impl_->activeAngleWidget->SetInteractor(impl_->interactor);
    impl_->activeAngleWidget->CreateDefaultRepresentation();
    impl_->configureAngleWidget(impl_->activeAngleWidget);

    // Set up callback for when measurement is placed
    impl_->angleEndCallback->SetCallback([](vtkObject* /*caller*/, unsigned long eventId,
                                            void* clientData, void* /*callData*/) {
        if (eventId == vtkCommand::EndInteractionEvent) {
            auto* impl = static_cast<LinearMeasurementTool::Impl*>(clientData);
            if (impl->activeAngleWidget && impl->isMeasuring) {
                // Extract measurement data
                auto measurement = impl->extractAngleMeasurement(impl->activeAngleWidget);
                measurement.id = impl->nextAngleId++;

                // Store widget and measurement
                AngleWidgetInfo info;
                info.widget = impl->activeAngleWidget;
                info.measurement = measurement;
                impl->angleWidgets[measurement.id] = info;

                // Call callback
                if (impl->angleCallback) {
                    impl->angleCallback(measurement);
                }

                impl->activeAngleWidget = nullptr;
                impl->isMeasuring = false;
                impl->mode = MeasurementMode::None;
            }
        }
    });
    impl_->angleEndCallback->SetClientData(impl_.get());

    impl_->activeAngleWidget->AddObserver(
        vtkCommand::EndInteractionEvent, impl_->angleEndCallback);

    impl_->activeAngleWidget->On();
    impl_->mode = MeasurementMode::Angle;
    impl_->isMeasuring = true;

    return {};
}

std::expected<void, MeasurementError> LinearMeasurementTool::startCobbAngleMeasurement() {
    auto result = startAngleMeasurement();
    if (result) {
        impl_->mode = MeasurementMode::CobbAngle;
    }
    return result;
}

void LinearMeasurementTool::cancelMeasurement() {
    if (impl_->activeDistanceWidget) {
        impl_->activeDistanceWidget->Off();
        impl_->activeDistanceWidget = nullptr;
    }
    if (impl_->activeAngleWidget) {
        impl_->activeAngleWidget->Off();
        impl_->activeAngleWidget = nullptr;
    }
    impl_->mode = MeasurementMode::None;
    impl_->isMeasuring = false;
}

void LinearMeasurementTool::completeMeasurement() {
    // Force end interaction on active widgets
    if (impl_->activeDistanceWidget) {
        impl_->activeDistanceWidget->InvokeEvent(vtkCommand::EndInteractionEvent);
    }
    if (impl_->activeAngleWidget) {
        impl_->activeAngleWidget->InvokeEvent(vtkCommand::EndInteractionEvent);
    }
}

std::vector<DistanceMeasurement> LinearMeasurementTool::getDistanceMeasurements() const {
    std::vector<DistanceMeasurement> result;
    result.reserve(impl_->distanceWidgets.size());
    for (const auto& [id, info] : impl_->distanceWidgets) {
        result.push_back(info.measurement);
    }
    return result;
}

std::vector<AngleMeasurement> LinearMeasurementTool::getAngleMeasurements() const {
    std::vector<AngleMeasurement> result;
    result.reserve(impl_->angleWidgets.size());
    for (const auto& [id, info] : impl_->angleWidgets) {
        result.push_back(info.measurement);
    }
    return result;
}

std::optional<DistanceMeasurement> LinearMeasurementTool::getDistanceMeasurement(int id) const {
    auto it = impl_->distanceWidgets.find(id);
    if (it != impl_->distanceWidgets.end()) {
        return it->second.measurement;
    }
    return std::nullopt;
}

std::optional<AngleMeasurement> LinearMeasurementTool::getAngleMeasurement(int id) const {
    auto it = impl_->angleWidgets.find(id);
    if (it != impl_->angleWidgets.end()) {
        return it->second.measurement;
    }
    return std::nullopt;
}

std::expected<void, MeasurementError> LinearMeasurementTool::deleteDistanceMeasurement(int id) {
    auto it = impl_->distanceWidgets.find(id);
    if (it == impl_->distanceWidgets.end()) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::MeasurementNotFound,
            "Distance measurement with ID " + std::to_string(id) + " not found"
        });
    }

    // Turn off and remove widget
    it->second.widget->Off();
    impl_->distanceWidgets.erase(it);

    render();
    return {};
}

std::expected<void, MeasurementError> LinearMeasurementTool::deleteAngleMeasurement(int id) {
    auto it = impl_->angleWidgets.find(id);
    if (it == impl_->angleWidgets.end()) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::MeasurementNotFound,
            "Angle measurement with ID " + std::to_string(id) + " not found"
        });
    }

    // Turn off and remove widget
    it->second.widget->Off();
    impl_->angleWidgets.erase(it);

    render();
    return {};
}

void LinearMeasurementTool::deleteAllMeasurements() {
    // Turn off all distance widgets
    for (auto& [id, info] : impl_->distanceWidgets) {
        info.widget->Off();
    }
    impl_->distanceWidgets.clear();

    // Turn off all angle widgets
    for (auto& [id, info] : impl_->angleWidgets) {
        info.widget->Off();
    }
    impl_->angleWidgets.clear();

    cancelMeasurement();
    render();
}

void LinearMeasurementTool::setDistanceMeasurementVisibility(int id, bool visible) {
    auto it = impl_->distanceWidgets.find(id);
    if (it != impl_->distanceWidgets.end()) {
        it->second.measurement.visible = visible;
        if (visible) {
            it->second.widget->On();
        } else {
            it->second.widget->Off();
        }
        render();
    }
}

void LinearMeasurementTool::setAngleMeasurementVisibility(int id, bool visible) {
    auto it = impl_->angleWidgets.find(id);
    if (it != impl_->angleWidgets.end()) {
        it->second.measurement.visible = visible;
        if (visible) {
            it->second.widget->On();
        } else {
            it->second.widget->Off();
        }
        render();
    }
}

void LinearMeasurementTool::showMeasurementsForSlice(int sliceIndex) {
    for (auto& [id, info] : impl_->distanceWidgets) {
        bool show = (sliceIndex == -1 || info.measurement.sliceIndex == sliceIndex);
        if (show && info.measurement.visible) {
            info.widget->On();
        } else {
            info.widget->Off();
        }
    }

    for (auto& [id, info] : impl_->angleWidgets) {
        bool show = (sliceIndex == -1 || info.measurement.sliceIndex == sliceIndex);
        if (show && info.measurement.visible) {
            info.widget->On();
        } else {
            info.widget->Off();
        }
    }

    render();
}

void LinearMeasurementTool::setDisplayParams(const MeasurementDisplayParams& params) {
    impl_->displayParams = params;

    // Update existing widgets
    for (auto& [id, info] : impl_->distanceWidgets) {
        impl_->configureDistanceWidget(info.widget);
    }
    for (auto& [id, info] : impl_->angleWidgets) {
        impl_->configureAngleWidget(info.widget);
    }

    render();
}

MeasurementDisplayParams LinearMeasurementTool::getDisplayParams() const {
    return impl_->displayParams;
}

void LinearMeasurementTool::setDistanceCompletedCallback(DistanceCallback callback) {
    impl_->distanceCallback = std::move(callback);
}

void LinearMeasurementTool::setAngleCompletedCallback(AngleCallback callback) {
    impl_->angleCallback = std::move(callback);
}

std::expected<void, MeasurementError>
LinearMeasurementTool::updateDistanceLabel(int id, const std::string& label) {
    auto it = impl_->distanceWidgets.find(id);
    if (it == impl_->distanceWidgets.end()) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::MeasurementNotFound,
            "Distance measurement with ID " + std::to_string(id) + " not found"
        });
    }

    it->second.measurement.label = label;
    return {};
}

std::expected<void, MeasurementError>
LinearMeasurementTool::updateAngleLabel(int id, const std::string& label) {
    auto it = impl_->angleWidgets.find(id);
    if (it == impl_->angleWidgets.end()) {
        return std::unexpected(MeasurementError{
            MeasurementError::Code::MeasurementNotFound,
            "Angle measurement with ID " + std::to_string(id) + " not found"
        });
    }

    it->second.measurement.label = label;
    return {};
}

size_t LinearMeasurementTool::getMeasurementCount() const noexcept {
    return impl_->distanceWidgets.size() + impl_->angleWidgets.size();
}

bool LinearMeasurementTool::isMeasuring() const noexcept {
    return impl_->isMeasuring;
}

void LinearMeasurementTool::render() {
    if (impl_->renderer && impl_->renderer->GetRenderWindow()) {
        impl_->renderer->GetRenderWindow()->Render();
    }
}

}  // namespace dicom_viewer::services

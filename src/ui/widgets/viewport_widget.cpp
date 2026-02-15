#include "ui/viewport_widget.hpp"
#include "services/measurement/linear_measurement_tool.hpp"
#include "services/measurement/area_measurement_tool.hpp"
#include "services/segmentation/manual_segmentation_controller.hpp"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkInteractorStyleImage.h>
#include <vtkImageSliceMapper.h>
#include <vtkImageSlice.h>
#include <vtkImageProperty.h>
#include <vtkCamera.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkImageData.h>
#include <vtkPointPicker.h>
#include <vtkCellPicker.h>
#include <vtkEventQtSlotConnect.h>

namespace dicom_viewer::ui {

class ViewportWidget::Impl {
public:
    QVTKOpenGLNativeWidget* vtkWidget = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkInteractorStyleTrackballCamera> trackballStyle;
    vtkSmartPointer<vtkInteractorStyleImage> imageStyle;
    vtkSmartPointer<vtkImageSliceMapper> sliceMapper;
    vtkSmartPointer<vtkImageSlice> imageSlice;
    vtkSmartPointer<vtkImageProperty> imageProperty;
    vtkSmartPointer<vtkImageData> imageData;
    vtkSmartPointer<vtkPointPicker> pointPicker;
    vtkSmartPointer<vtkEventQtSlotConnect> connections;

    // Measurement tools
    std::unique_ptr<services::LinearMeasurementTool> measurementTool;
    std::unique_ptr<services::AreaMeasurementTool> areaMeasurementTool;

    // Segmentation controller
    std::unique_ptr<services::ManualSegmentationController> segmentationController;

    ViewportMode mode = ViewportMode::SingleSlice;
    double windowWidth = 400.0;
    double windowCenter = 40.0;
    int currentSlice = 0;

    Impl() {
        renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        renderer = vtkSmartPointer<vtkRenderer>::New();
        trackballStyle = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        imageStyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
        sliceMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        imageProperty = vtkSmartPointer<vtkImageProperty>::New();
        pointPicker = vtkSmartPointer<vtkPointPicker>::New();
        connections = vtkSmartPointer<vtkEventQtSlotConnect>::New();

        // Setup renderer
        renderer->SetBackground(0.1, 0.1, 0.1);
        renderWindow->AddRenderer(renderer);

        // Setup image property for window/level
        imageProperty->SetColorWindow(windowWidth);
        imageProperty->SetColorLevel(windowCenter);
        imageProperty->SetInterpolationTypeToLinear();

        // Setup image slice
        imageSlice->SetMapper(sliceMapper);
        imageSlice->SetProperty(imageProperty);

        // Setup measurement tools
        measurementTool = std::make_unique<services::LinearMeasurementTool>();
        measurementTool->setRenderer(renderer);

        areaMeasurementTool = std::make_unique<services::AreaMeasurementTool>();
        areaMeasurementTool->setRenderer(renderer);

        // Setup segmentation controller
        segmentationController = std::make_unique<services::ManualSegmentationController>();
    }

    void updateInteractorStyle() {
        auto interactor = renderWindow->GetInteractor();
        if (!interactor) return;

        switch (mode) {
            case ViewportMode::VolumeRendering:
            case ViewportMode::SurfaceRendering:
                interactor->SetInteractorStyle(trackballStyle);
                break;
            case ViewportMode::MPR:
            case ViewportMode::SingleSlice:
                interactor->SetInteractorStyle(imageStyle);
                break;
        }
    }

    void setupForSliceView() {
        renderer->RemoveAllViewProps();
        if (imageData) {
            sliceMapper->SetInputData(imageData);
            sliceMapper->SetSliceNumber(currentSlice);
            renderer->AddViewProp(imageSlice);
        }
    }
};

ViewportWidget::ViewportWidget(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    // Create layout
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create VTK widget
    impl_->vtkWidget = new QVTKOpenGLNativeWidget(this);
    impl_->vtkWidget->setRenderWindow(impl_->renderWindow);
    layout->addWidget(impl_->vtkWidget);

    // Setup interactor
    auto interactor = impl_->renderWindow->GetInteractor();
    interactor->SetPicker(impl_->pointPicker);
    impl_->updateInteractorStyle();

    // Set interactor for measurement tools
    impl_->measurementTool->setInteractor(interactor);
    impl_->areaMeasurementTool->setInteractor(interactor);

    // Setup measurement callbacks
    impl_->measurementTool->setDistanceCompletedCallback(
        [this](const services::DistanceMeasurement& m) {
            emit distanceMeasurementCompleted(m.distanceMm, m.id);
        });

    impl_->measurementTool->setAngleCompletedCallback(
        [this](const services::AngleMeasurement& m) {
            emit angleMeasurementCompleted(m.angleDegrees, m.id);
        });

    impl_->areaMeasurementTool->setMeasurementCompletedCallback(
        [this](const services::AreaMeasurement& m) {
            emit areaMeasurementCompleted(m.areaMm2, m.areaCm2, m.id);
        });

    setLayout(layout);
}

ViewportWidget::~ViewportWidget() = default;

void ViewportWidget::setImageData(vtkSmartPointer<vtkImageData> imageData)
{
    impl_->imageData = imageData;

    if (imageData) {
        int* dims = imageData->GetDimensions();
        impl_->currentSlice = dims[2] / 2;

        // Update pixel spacing for measurement tools
        double* spacing = imageData->GetSpacing();
        impl_->measurementTool->setPixelSpacing(spacing[0], spacing[1], spacing[2]);
        impl_->measurementTool->setCurrentSlice(impl_->currentSlice);
        impl_->areaMeasurementTool->setPixelSpacing(spacing[0], spacing[1], spacing[2]);
        impl_->areaMeasurementTool->setCurrentSlice(impl_->currentSlice);

        if (impl_->mode == ViewportMode::SingleSlice ||
            impl_->mode == ViewportMode::MPR) {
            impl_->setupForSliceView();
        }
        resetCamera();
    }
    impl_->vtkWidget->renderWindow()->Render();
}

void ViewportWidget::setMode(ViewportMode mode)
{
    impl_->mode = mode;
    impl_->updateInteractorStyle();

    if (impl_->imageData) {
        switch (mode) {
            case ViewportMode::SingleSlice:
            case ViewportMode::MPR:
                impl_->setupForSliceView();
                break;
            case ViewportMode::VolumeRendering:
            case ViewportMode::SurfaceRendering:
                // Volume/Surface rendering actors added externally
                impl_->renderer->RemoveViewProp(impl_->imageSlice);
                break;
        }
    }
    impl_->vtkWidget->renderWindow()->Render();
}

ViewportMode ViewportWidget::getMode() const
{
    return impl_->mode;
}

void ViewportWidget::setWindowLevel(double width, double center)
{
    impl_->windowWidth = width;
    impl_->windowCenter = center;
    impl_->imageProperty->SetColorWindow(width);
    impl_->imageProperty->SetColorLevel(center);
    impl_->vtkWidget->renderWindow()->Render();

    emit windowLevelChanged(width, center);
}

void ViewportWidget::applyPreset(const QString& presetName)
{
    // Common CT presets
    if (presetName == "CT Bone") {
        setWindowLevel(2000.0, 500.0);
    } else if (presetName == "CT Lung") {
        setWindowLevel(1500.0, -600.0);
    } else if (presetName == "CT Abdomen") {
        setWindowLevel(400.0, 40.0);
    } else if (presetName == "CT Brain") {
        setWindowLevel(80.0, 40.0);
    } else if (presetName == "CT Soft Tissue") {
        setWindowLevel(350.0, 50.0);
    }
}

void ViewportWidget::resetCamera()
{
    impl_->renderer->ResetCamera();
    impl_->vtkWidget->renderWindow()->Render();
}

bool ViewportWidget::captureScreenshot(const QString& filePath)
{
    auto windowToImage = vtkSmartPointer<vtkWindowToImageFilter>::New();
    windowToImage->SetInput(impl_->renderWindow);
    windowToImage->SetScale(1);
    windowToImage->SetInputBufferTypeToRGBA();
    windowToImage->ReadFrontBufferOff();
    windowToImage->Update();

    auto writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(filePath.toStdString().c_str());
    writer->SetInputConnection(windowToImage->GetOutputPort());
    writer->Write();

    return true;
}

void ViewportWidget::setCrosshairPosition(double x, double y, double z)
{
    if (!impl_->imageData) return;

    double* spacing = impl_->imageData->GetSpacing();
    double* origin = impl_->imageData->GetOrigin();

    int slice = static_cast<int>((z - origin[2]) / spacing[2]);
    int* dims = impl_->imageData->GetDimensions();

    if (slice >= 0 && slice < dims[2]) {
        impl_->currentSlice = slice;
        impl_->sliceMapper->SetSliceNumber(slice);
        impl_->vtkWidget->renderWindow()->Render();
    }

    emit crosshairPositionChanged(x, y, z);
}

void ViewportWidget::setPhaseIndex(int phaseIndex)
{
    // Phase display update is handled by the controller layer
    // which swaps the image data for each phase. This slot
    // serves as a notification point for phase-aware rendering.
    emit phaseIndexChanged(phaseIndex);
}

void ViewportWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (impl_->vtkWidget) {
        impl_->vtkWidget->renderWindow()->Render();
    }
}

void ViewportWidget::startDistanceMeasurement()
{
    auto result = impl_->measurementTool->startDistanceMeasurement();
    if (result) {
        emit measurementModeChanged(services::MeasurementMode::Distance);
    }
}

void ViewportWidget::startAngleMeasurement()
{
    auto result = impl_->measurementTool->startAngleMeasurement();
    if (result) {
        emit measurementModeChanged(services::MeasurementMode::Angle);
    }
}

void ViewportWidget::startAreaMeasurement(services::RoiType type)
{
    // Cancel any existing linear measurement first
    impl_->measurementTool->cancelMeasurement();

    auto result = impl_->areaMeasurementTool->startRoiDrawing(type);
    if (result) {
        services::MeasurementMode mode = services::MeasurementMode::None;
        switch (type) {
            case services::RoiType::Ellipse:
                mode = services::MeasurementMode::AreaEllipse;
                break;
            case services::RoiType::Rectangle:
                mode = services::MeasurementMode::AreaRectangle;
                break;
            case services::RoiType::Polygon:
                mode = services::MeasurementMode::AreaPolygon;
                break;
            case services::RoiType::Freehand:
                mode = services::MeasurementMode::AreaFreehand;
                break;
        }
        emit measurementModeChanged(mode);
    }
}

void ViewportWidget::cancelMeasurement()
{
    impl_->measurementTool->cancelMeasurement();
    impl_->areaMeasurementTool->cancelCurrentRoi();
    emit measurementModeChanged(services::MeasurementMode::None);
}

void ViewportWidget::deleteAllMeasurements()
{
    impl_->measurementTool->deleteAllMeasurements();
    impl_->areaMeasurementTool->deleteAllMeasurements();
}

void ViewportWidget::deleteAllAreaMeasurements()
{
    impl_->areaMeasurementTool->deleteAllMeasurements();
}

services::MeasurementMode ViewportWidget::getMeasurementMode() const
{
    return impl_->measurementTool->getMode();
}

std::vector<services::AreaMeasurement> ViewportWidget::getAreaMeasurements() const
{
    return impl_->areaMeasurementTool->getMeasurements();
}

std::optional<services::AreaMeasurement> ViewportWidget::getAreaMeasurement(int id) const
{
    return impl_->areaMeasurementTool->getMeasurement(id);
}

int ViewportWidget::getCurrentSlice() const
{
    return impl_->currentSlice;
}

vtkSmartPointer<vtkImageData> ViewportWidget::getImageData() const
{
    return impl_->imageData;
}

void ViewportWidget::setSegmentationTool(services::SegmentationTool tool)
{
    impl_->segmentationController->setActiveTool(tool);
    emit segmentationToolChanged(tool);
}

services::SegmentationTool ViewportWidget::getSegmentationTool() const
{
    return impl_->segmentationController->getActiveTool();
}

void ViewportWidget::setSegmentationBrushSize(int size)
{
    impl_->segmentationController->setBrushSize(size);
}

void ViewportWidget::setSegmentationBrushShape(services::BrushShape shape)
{
    impl_->segmentationController->setBrushShape(shape);
}

void ViewportWidget::setSegmentationActiveLabel(uint8_t labelId)
{
    impl_->segmentationController->setActiveLabel(labelId);
}

void ViewportWidget::undoSegmentationOperation()
{
    auto tool = impl_->segmentationController->getActiveTool();
    if (tool == services::SegmentationTool::Polygon) {
        impl_->segmentationController->undoLastPolygonVertex();
    } else if (tool == services::SegmentationTool::SmartScissors) {
        impl_->segmentationController->undoLastSmartScissorsAnchor();
    }
}

void ViewportWidget::completeSegmentationOperation()
{
    auto tool = impl_->segmentationController->getActiveTool();
    if (tool == services::SegmentationTool::Polygon) {
        impl_->segmentationController->completePolygon(impl_->currentSlice);
        emit segmentationModified(impl_->currentSlice);
    } else if (tool == services::SegmentationTool::SmartScissors) {
        impl_->segmentationController->completeSmartScissors(impl_->currentSlice);
        emit segmentationModified(impl_->currentSlice);
    }
}

void ViewportWidget::clearAllSegmentation()
{
    impl_->segmentationController->clearAll();
    emit segmentationModified(impl_->currentSlice);
}

bool ViewportWidget::isSegmentationModeActive() const
{
    return impl_->segmentationController->getActiveTool() != services::SegmentationTool::None;
}

} // namespace dicom_viewer::ui

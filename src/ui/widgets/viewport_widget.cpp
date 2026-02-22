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

#include "ui/viewport_widget.hpp"
#include "ui/widgets/sp_mode_toggle.hpp"
#include "services/measurement/linear_measurement_tool.hpp"
#include "services/measurement/area_measurement_tool.hpp"
#include "services/segmentation/manual_segmentation_controller.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
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
#include <vtkCoordinate.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkAppendPolyData.h>

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
    SliceOrientation sliceOrientation = SliceOrientation::Axial;
    ScrollMode scrollMode = ScrollMode::Slice;
    QLabel* spIndicator = nullptr;
    double windowWidth = 400.0;
    double windowCenter = 40.0;
    int currentSlice = 0;

    // MPR crosshair lines (intersection of other planes)
    vtkSmartPointer<vtkActor> crosshairHLine;
    vtkSmartPointer<vtkActor> crosshairVLine;
    vtkSmartPointer<vtkLineSource> crosshairHSource;
    vtkSmartPointer<vtkLineSource> crosshairVSource;
    bool crosshairLinesVisible = false;
    double crosshairWorldPos[3] = {0.0, 0.0, 0.0};

    // Measurement plane overlay line
    vtkSmartPointer<vtkActor> planeLineActor;
    vtkSmartPointer<vtkLineSource> planeLineSource;

    // Plane positioning interaction state
    bool planePositioningMode = false;
    bool planeDragging = false;
    double planeCenterWorld[3] = {0.0, 0.0, 0.0};
    double planeDragWorld[3] = {0.0, 0.0, 0.0};

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

        // Setup crosshair line actors
        crosshairHSource = vtkSmartPointer<vtkLineSource>::New();
        crosshairVSource = vtkSmartPointer<vtkLineSource>::New();

        auto hMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        hMapper->SetInputConnection(crosshairHSource->GetOutputPort());
        crosshairHLine = vtkSmartPointer<vtkActor>::New();
        crosshairHLine->SetMapper(hMapper);
        crosshairHLine->GetProperty()->SetLineWidth(1.0);
        crosshairHLine->SetVisibility(false);

        auto vMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        vMapper->SetInputConnection(crosshairVSource->GetOutputPort());
        crosshairVLine = vtkSmartPointer<vtkActor>::New();
        crosshairVLine->SetMapper(vMapper);
        crosshairVLine->GetProperty()->SetLineWidth(1.0);
        crosshairVLine->SetVisibility(false);

        // Setup plane line overlay actor
        planeLineSource = vtkSmartPointer<vtkLineSource>::New();
        auto planeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        planeMapper->SetInputConnection(planeLineSource->GetOutputPort());
        planeLineActor = vtkSmartPointer<vtkActor>::New();
        planeLineActor->SetMapper(planeMapper);
        planeLineActor->GetProperty()->SetLineWidth(2.0);
        planeLineActor->GetProperty()->SetColor(1.0, 0.3, 0.3);
        planeLineActor->SetVisibility(false);
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
        // Add crosshair line actors (visibility controlled separately)
        renderer->AddActor(crosshairHLine);
        renderer->AddActor(crosshairVLine);
        renderer->AddActor(planeLineActor);
    }

    bool screenToWorld(int screenX, int screenY, double worldOut[3]) {
        if (!renderer) return false;
        auto* coordinate = vtkCoordinate::New();
        coordinate->SetCoordinateSystemToDisplay();
        coordinate->SetValue(screenX, screenY, 0);
        double* world = coordinate->GetComputedWorldValue(renderer);
        worldOut[0] = world[0];
        worldOut[1] = world[1];
        worldOut[2] = world[2];
        coordinate->Delete();
        return true;
    }

    void updatePlaneLineFromDrag() {
        if (!imageData) return;

        double cx = planeCenterWorld[0];
        double cy = planeCenterWorld[1];
        double cz = planeCenterWorld[2];
        double dx = planeDragWorld[0] - cx;
        double dy = planeDragWorld[1] - cy;
        double dz = planeDragWorld[2] - cz;

        double len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1e-6) {
            // No drag yet — use default horizontal direction
            dx = 1.0; dy = 0.0; dz = 0.0;
            len = 1.0;
        }

        // Normalize direction
        dx /= len; dy /= len; dz /= len;

        // Extend line from center in both directions
        double extent = 50.0;
        if (len > 5.0) extent = len;

        double p1[3] = {cx - dx * extent, cy - dy * extent, cz - dz * extent};
        double p2[3] = {cx + dx * extent, cy + dy * extent, cz + dz * extent};

        // Clamp to current slice Z for axial views
        switch (sliceOrientation) {
            case SliceOrientation::Axial:
                p1[2] = cz; p2[2] = cz;
                break;
            case SliceOrientation::Coronal:
                p1[1] = cy; p2[1] = cy;
                break;
            case SliceOrientation::Sagittal:
                p1[0] = cx; p2[0] = cx;
                break;
        }

        planeLineSource->SetPoint1(p1);
        planeLineSource->SetPoint2(p2);
        planeLineSource->Update();
        planeLineActor->SetVisibility(true);
    }

    void updateCrosshairLines() {
        if (!crosshairLinesVisible || !imageData) {
            crosshairHLine->SetVisibility(false);
            crosshairVLine->SetVisibility(false);
            return;
        }

        double* origin = imageData->GetOrigin();
        double* spacing = imageData->GetSpacing();
        int* dims = imageData->GetDimensions();

        double xMin = origin[0];
        double xMax = origin[0] + (dims[0] - 1) * spacing[0];
        double yMin = origin[1];
        double yMax = origin[1] + (dims[1] - 1) * spacing[1];
        double zMin = origin[2];
        double zMax = origin[2] + (dims[2] - 1) * spacing[2];

        double cx = crosshairWorldPos[0];
        double cy = crosshairWorldPos[1];
        double cz = crosshairWorldPos[2];

        // Lines represent intersection of other planes on this view
        // Axial (XY): H-line=Coronal(blue), V-line=Sagittal(green)
        // Coronal (XZ): H-line=Axial(red), V-line=Sagittal(green)
        // Sagittal (YZ): H-line=Axial(red), V-line=Coronal(blue)
        switch (sliceOrientation) {
            case SliceOrientation::Axial:
                crosshairHSource->SetPoint1(xMin, cy, cz);
                crosshairHSource->SetPoint2(xMax, cy, cz);
                crosshairVSource->SetPoint1(cx, yMin, cz);
                crosshairVSource->SetPoint2(cx, yMax, cz);
                // H = Coronal (blue), V = Sagittal (green)
                crosshairHLine->GetProperty()->SetColor(0.3, 0.5, 1.0);
                crosshairVLine->GetProperty()->SetColor(0.3, 1.0, 0.3);
                break;
            case SliceOrientation::Coronal:
                crosshairHSource->SetPoint1(xMin, cy, cz);
                crosshairHSource->SetPoint2(xMax, cy, cz);
                crosshairVSource->SetPoint1(cx, cy, zMin);
                crosshairVSource->SetPoint2(cx, cy, zMax);
                // H = Axial (red), V = Sagittal (green)
                crosshairHLine->GetProperty()->SetColor(1.0, 0.3, 0.3);
                crosshairVLine->GetProperty()->SetColor(0.3, 1.0, 0.3);
                break;
            case SliceOrientation::Sagittal:
                crosshairHSource->SetPoint1(cx, yMin, cz);
                crosshairHSource->SetPoint2(cx, yMax, cz);
                crosshairVSource->SetPoint1(cx, cy, zMin);
                crosshairVSource->SetPoint2(cx, cy, zMax);
                // H = Axial (red), V = Coronal (blue)
                crosshairHLine->GetProperty()->SetColor(1.0, 0.3, 0.3);
                crosshairVLine->GetProperty()->SetColor(0.3, 0.5, 1.0);
                break;
        }

        crosshairHSource->Update();
        crosshairVSource->Update();
        crosshairHLine->SetVisibility(true);
        crosshairVLine->SetVisibility(true);
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

    // Install event filter on VTK widget to intercept scroll in Phase mode
    impl_->vtkWidget->installEventFilter(this);

    // S/P mode indicator overlay (top-right corner)
    impl_->spIndicator = new QLabel("S", this);
    impl_->spIndicator->setFixedSize(24, 24);
    impl_->spIndicator->setAlignment(Qt::AlignCenter);
    impl_->spIndicator->setStyleSheet(
        "QLabel { background-color: rgba(42, 130, 218, 180); color: white; "
        "font-weight: bold; font-size: 12px; border-radius: 4px; }");
    impl_->spIndicator->raise();
    impl_->spIndicator->hide();  // Hidden until 4D data is loaded

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

    // Wire segmentation undo/redo availability signal
    impl_->segmentationController->setUndoRedoCallback(
        [this](bool canUndo, bool canRedo) {
            emit segmentationUndoRedoChanged(canUndo, canRedo);
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

void ViewportWidget::setSliceOrientation(SliceOrientation orientation)
{
    impl_->sliceOrientation = orientation;

    // Map enum to VTK orientation: 0=YZ(Sagittal), 1=XZ(Coronal), 2=XY(Axial)
    int vtkOrientation = 2;  // Axial default
    switch (orientation) {
        case SliceOrientation::Sagittal: vtkOrientation = 0; break;
        case SliceOrientation::Coronal:  vtkOrientation = 1; break;
        case SliceOrientation::Axial:    vtkOrientation = 2; break;
    }
    impl_->sliceMapper->SetOrientation(vtkOrientation);

    // Reset slice to middle of the new orientation axis
    if (impl_->imageData) {
        int* dims = impl_->imageData->GetDimensions();
        impl_->currentSlice = dims[vtkOrientation] / 2;
        impl_->sliceMapper->SetSliceNumber(impl_->currentSlice);
        resetCamera();
    }
}

SliceOrientation ViewportWidget::getSliceOrientation() const
{
    return impl_->sliceOrientation;
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

vtkRenderWindow* ViewportWidget::getRenderWindow() const
{
    return impl_->renderWindow;
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

    impl_->crosshairWorldPos[0] = x;
    impl_->crosshairWorldPos[1] = y;
    impl_->crosshairWorldPos[2] = z;

    double* spacing = impl_->imageData->GetSpacing();
    double* origin = impl_->imageData->GetOrigin();
    int* dims = impl_->imageData->GetDimensions();

    // Map world coordinate to slice index based on orientation
    int slice = 0;
    int maxSlice = 0;
    switch (impl_->sliceOrientation) {
        case SliceOrientation::Axial:    // Slices along Z
            slice = static_cast<int>((z - origin[2]) / spacing[2]);
            maxSlice = dims[2];
            break;
        case SliceOrientation::Coronal:  // Slices along Y
            slice = static_cast<int>((y - origin[1]) / spacing[1]);
            maxSlice = dims[1];
            break;
        case SliceOrientation::Sagittal: // Slices along X
            slice = static_cast<int>((x - origin[0]) / spacing[0]);
            maxSlice = dims[0];
            break;
    }

    if (slice >= 0 && slice < maxSlice) {
        impl_->currentSlice = slice;
        impl_->sliceMapper->SetSliceNumber(slice);
    }

    impl_->updateCrosshairLines();
    impl_->vtkWidget->renderWindow()->Render();

    emit crosshairPositionChanged(x, y, z);
}

void ViewportWidget::setCrosshairLinesVisible(bool visible)
{
    impl_->crosshairLinesVisible = visible;
    impl_->updateCrosshairLines();
    if (impl_->vtkWidget && impl_->vtkWidget->renderWindow()) {
        impl_->vtkWidget->renderWindow()->Render();
    }
}

bool ViewportWidget::isCrosshairLinesVisible() const
{
    return impl_->crosshairLinesVisible;
}

void ViewportWidget::setPhaseIndex(int phaseIndex)
{
    // Phase display update is handled by the controller layer
    // which swaps the image data for each phase. This slot
    // serves as a notification point for phase-aware rendering.
    emit phaseIndexChanged(phaseIndex);
}

void ViewportWidget::setScrollMode(ScrollMode mode)
{
    impl_->scrollMode = mode;
    impl_->spIndicator->setText(mode == ScrollMode::Phase ? "P" : "S");
}

bool ViewportWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != impl_->vtkWidget) {
        return QWidget::eventFilter(watched, event);
    }

    // Phase scroll handling
    if (event->type() == QEvent::Wheel) {
        if (impl_->scrollMode == ScrollMode::Phase) {
            auto* wheelEvent = static_cast<QWheelEvent*>(event);
            int delta = wheelEvent->angleDelta().y() > 0 ? -1 : 1;
            emit phaseScrollRequested(delta);
            return true;
        }
    }

    // Plane positioning mouse handling
    if (impl_->planePositioningMode) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // Convert screen to world coordinates
                // Qt Y is top-down, VTK Y is bottom-up
                int vtkY = impl_->vtkWidget->height() - me->pos().y();
                if (impl_->screenToWorld(me->pos().x(), vtkY,
                                         impl_->planeCenterWorld)) {
                    // Snap center Z to current slice position
                    double* origin = impl_->imageData->GetOrigin();
                    double* spacing = impl_->imageData->GetSpacing();
                    switch (impl_->sliceOrientation) {
                        case SliceOrientation::Axial:
                            impl_->planeCenterWorld[2] =
                                origin[2] + impl_->currentSlice * spacing[2];
                            break;
                        case SliceOrientation::Coronal:
                            impl_->planeCenterWorld[1] =
                                origin[1] + impl_->currentSlice * spacing[1];
                            break;
                        case SliceOrientation::Sagittal:
                            impl_->planeCenterWorld[0] =
                                origin[0] + impl_->currentSlice * spacing[0];
                            break;
                    }
                    impl_->planeDragWorld[0] = impl_->planeCenterWorld[0];
                    impl_->planeDragWorld[1] = impl_->planeCenterWorld[1];
                    impl_->planeDragWorld[2] = impl_->planeCenterWorld[2];
                    impl_->planeDragging = true;
                    impl_->updatePlaneLineFromDrag();
                    impl_->vtkWidget->renderWindow()->Render();
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && impl_->planeDragging) {
            auto* me = static_cast<QMouseEvent*>(event);
            int vtkY = impl_->vtkWidget->height() - me->pos().y();
            impl_->screenToWorld(me->pos().x(), vtkY, impl_->planeDragWorld);
            impl_->updatePlaneLineFromDrag();
            impl_->vtkWidget->renderWindow()->Render();
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && impl_->planeDragging) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                impl_->planeDragging = false;
                impl_->planePositioningMode = false;

                // Compute PlanePosition from center and drag direction
                double cx = impl_->planeCenterWorld[0];
                double cy = impl_->planeCenterWorld[1];
                double cz = impl_->planeCenterWorld[2];
                double dx = impl_->planeDragWorld[0] - cx;
                double dy = impl_->planeDragWorld[1] - cy;
                double dz = impl_->planeDragWorld[2] - cz;
                double len = std::sqrt(dx * dx + dy * dy + dz * dz);

                PlanePosition pos;
                pos.centerX = cx;
                pos.centerY = cy;
                pos.centerZ = cz;
                pos.extent = std::max(len, 10.0);

                // Normal is perpendicular to drag direction within the slice plane
                if (len > 1e-6) {
                    dx /= len; dy /= len; dz /= len;
                    switch (impl_->sliceOrientation) {
                        case SliceOrientation::Axial:
                            pos.normalX = -dy;
                            pos.normalY = dx;
                            pos.normalZ = 0.0;
                            break;
                        case SliceOrientation::Coronal:
                            pos.normalX = -dz;
                            pos.normalY = 0.0;
                            pos.normalZ = dx;
                            break;
                        case SliceOrientation::Sagittal:
                            pos.normalX = 0.0;
                            pos.normalY = -dz;
                            pos.normalZ = dy;
                            break;
                    }
                }

                emit planePositioned(pos);
                emit measurementModeChanged(services::MeasurementMode::None);
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ViewportWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (impl_->spIndicator) {
        impl_->spIndicator->move(width() - impl_->spIndicator->width() - 8, 8);
    }
    if (impl_->vtkWidget) {
        impl_->vtkWidget->renderWindow()->Render();
    }
}

void ViewportWidget::startPlanePositioning()
{
    // Cancel existing measurements
    impl_->measurementTool->cancelMeasurement();
    impl_->areaMeasurementTool->cancelCurrentRoi();

    impl_->planePositioningMode = true;
    impl_->planeDragging = false;
    impl_->planeLineActor->SetVisibility(false);
    emit measurementModeChanged(services::MeasurementMode::PlanePositioning);
}

void ViewportWidget::showPlaneOverlay(const PlanePosition& position,
                                       double r, double g, double b)
{
    if (!impl_->imageData) return;

    // Compute line endpoints from PlanePosition normal and center
    // The line direction is perpendicular to the normal within the slice plane
    double nx = position.normalX;
    double ny = position.normalY;
    double nz = position.normalZ;

    // Determine line direction in the slice plane (perpendicular to normal)
    double lx = 0.0, ly = 0.0, lz = 0.0;
    switch (impl_->sliceOrientation) {
        case SliceOrientation::Axial:
            // In XY plane: line perpendicular to (nx,ny) projected onto XY
            lx = -ny; ly = nx; lz = 0.0;
            break;
        case SliceOrientation::Coronal:
            // In XZ plane: line perpendicular to (nx,nz) projected onto XZ
            lx = -nz; ly = 0.0; lz = nx;
            break;
        case SliceOrientation::Sagittal:
            // In YZ plane: line perpendicular to (ny,nz) projected onto YZ
            lx = 0.0; ly = -nz; lz = ny;
            break;
    }

    double len = std::sqrt(lx * lx + ly * ly + lz * lz);
    if (len < 1e-6) {
        // Normal is along the slice axis — plane is parallel to the slice
        impl_->planeLineActor->SetVisibility(false);
        impl_->vtkWidget->renderWindow()->Render();
        return;
    }
    lx /= len; ly /= len; lz /= len;

    double ext = position.extent;
    double p1[3] = {position.centerX - lx * ext,
                    position.centerY - ly * ext,
                    position.centerZ - lz * ext};
    double p2[3] = {position.centerX + lx * ext,
                    position.centerY + ly * ext,
                    position.centerZ + lz * ext};

    impl_->planeLineSource->SetPoint1(p1);
    impl_->planeLineSource->SetPoint2(p2);
    impl_->planeLineSource->Update();
    impl_->planeLineActor->GetProperty()->SetColor(r, g, b);
    impl_->planeLineActor->SetVisibility(true);
    impl_->vtkWidget->renderWindow()->Render();
}

void ViewportWidget::hidePlaneOverlay()
{
    impl_->planeLineActor->SetVisibility(false);
    impl_->vtkWidget->renderWindow()->Render();
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
    impl_->planePositioningMode = false;
    impl_->planeDragging = false;
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

bool ViewportWidget::undoSegmentationCommand()
{
    bool result = impl_->segmentationController->undo();
    if (result) {
        emit segmentationModified(impl_->currentSlice);
    }
    return result;
}

bool ViewportWidget::redoSegmentationCommand()
{
    bool result = impl_->segmentationController->redo();
    if (result) {
        emit segmentationModified(impl_->currentSlice);
    }
    return result;
}

bool ViewportWidget::isSegmentationModeActive() const
{
    return impl_->segmentationController->getActiveTool() != services::SegmentationTool::None;
}

} // namespace dicom_viewer::ui

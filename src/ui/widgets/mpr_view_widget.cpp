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

#include "ui/mpr_view_widget.hpp"
#include "ui/widgets/sp_mode_toggle.hpp"

#include <QGridLayout>
#include <QMouseEvent>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleImage.h>
#include <vtkCamera.h>
#include <vtkPointPicker.h>
#include <vtkCommand.h>
#include <vtkCallbackCommand.h>
#include <vtkCoordinate.h>

namespace dicom_viewer::ui {

// Helper class for VTK interaction callbacks (uses void* to avoid circular dependency)
class MPRInteractorCallback : public vtkCommand {
public:
    static MPRInteractorCallback* New() {
        return new MPRInteractorCallback;
    }

    void SetWidget(void* impl, services::MPRPlane plane) {
        widgetImpl_ = impl;
        plane_ = plane;
    }

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override;

private:
    void* widgetImpl_ = nullptr;
    services::MPRPlane plane_ = services::MPRPlane::Axial;
};

struct PlaneViewData {
    QVTKOpenGLNativeWidget* vtkWidget = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkPointPicker> picker;
    vtkSmartPointer<MPRInteractorCallback> callback;
    services::MPRPlane plane;
    bool isMouseDown = false;
    services::Point2D lastMousePos;
};

class MPRViewWidget::Impl {
public:
    MPRViewWidget* widget = nullptr;

    // MPR renderer for synchronized views
    std::unique_ptr<services::MPRRenderer> mprRenderer;

    // Coordinate transformer (using unified coordinate service)
    std::unique_ptr<services::coordinate::MPRCoordinateTransformer> transformer;

    // Segmentation controller
    std::unique_ptr<services::ManualSegmentationController> segmentationController;

    // Label map overlay
    std::unique_ptr<services::LabelMapOverlay> labelMapOverlay;

    // View data for each plane
    std::array<PlaneViewData, 3> planeViews;

    // Image data
    vtkSmartPointer<vtkImageData> imageData;

    // Active plane (last interacted)
    services::MPRPlane activePlane = services::MPRPlane::Axial;

    // Scroll mode (Slice or Phase)
    ScrollMode scrollMode = ScrollMode::Slice;

    Impl(MPRViewWidget* w) : widget(w) {
        mprRenderer = std::make_unique<services::MPRRenderer>();
        transformer = std::make_unique<services::coordinate::MPRCoordinateTransformer>();
        segmentationController = std::make_unique<services::ManualSegmentationController>();
        labelMapOverlay = std::make_unique<services::LabelMapOverlay>();

        // Setup segmentation modification callback
        segmentationController->setModificationCallback(
            [this](int sliceIndex) {
                labelMapOverlay->notifySliceModified(sliceIndex);
                emit widget->segmentationModified(sliceIndex);
                updateAllViews();
            });
    }

    void setupPlaneView(services::MPRPlane plane, QWidget* parent) {
        int index = static_cast<int>(plane);
        auto& view = planeViews[index];

        view.plane = plane;
        view.renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        view.picker = vtkSmartPointer<vtkPointPicker>::New();

        // Create VTK widget
        view.vtkWidget = new QVTKOpenGLNativeWidget(parent);
        view.vtkWidget->setRenderWindow(view.renderWindow);

        // Add renderer from MPR renderer
        view.renderWindow->AddRenderer(mprRenderer->getRenderer(plane));

        // Setup interactor
        auto interactor = view.renderWindow->GetInteractor();
        auto style = vtkSmartPointer<vtkInteractorStyleImage>::New();
        interactor->SetInteractorStyle(style);
        interactor->SetPicker(view.picker);

        // Setup callback for mouse events
        view.callback = vtkSmartPointer<MPRInteractorCallback>::New();
        view.callback->SetWidget(this, plane);
        interactor->AddObserver(vtkCommand::LeftButtonPressEvent, view.callback.Get());
        interactor->AddObserver(vtkCommand::LeftButtonReleaseEvent, view.callback.Get());
        interactor->AddObserver(vtkCommand::MouseMoveEvent, view.callback.Get());
        interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, view.callback.Get());
        interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, view.callback.Get());
    }

    services::coordinate::ScreenCoordinate getScreenCoordinate(services::MPRPlane plane) {
        int index = static_cast<int>(plane);
        auto& view = planeViews[index];

        auto interactor = view.renderWindow->GetInteractor();
        int* pos = interactor->GetEventPosition();

        // Convert display coordinates to world coordinates using the renderer
        auto renderer = mprRenderer->getRenderer(plane);
        auto coordinate = vtkSmartPointer<vtkCoordinate>::New();
        coordinate->SetCoordinateSystemToDisplay();
        coordinate->SetValue(pos[0], pos[1], 0);

        double* worldPos = coordinate->GetComputedWorldValue(renderer);

        return services::coordinate::ScreenCoordinate(worldPos[0], worldPos[1]);
    }

    void handleMousePress(services::MPRPlane plane) {
        int index = static_cast<int>(plane);
        auto& view = planeViews[index];
        view.isMouseDown = true;
        activePlane = plane;

        if (segmentationController->getActiveTool() == services::SegmentationTool::None) {
            return;
        }

        auto screenCoord = getScreenCoordinate(plane);
        double slicePos = mprRenderer->getSlicePosition(plane);

        auto volumeCoord = transformer->screenToWorld(screenCoord, plane, slicePos);
        if (!volumeCoord) {
            return;
        }

        auto voxel = transformer->worldToVoxel(*volumeCoord);
        int sliceIndex = transformer->getSliceIndex(plane, slicePos);

        // Map 2D position based on plane
        services::Point2D pos2D = mapVoxelTo2D(voxel, plane);

        segmentationController->onMousePress(pos2D, sliceIndex);
        view.lastMousePos = pos2D;
    }

    void handleMouseMove(services::MPRPlane plane) {
        int index = static_cast<int>(plane);
        auto& view = planeViews[index];

        if (!view.isMouseDown ||
            segmentationController->getActiveTool() == services::SegmentationTool::None) {
            return;
        }

        auto screenCoord = getScreenCoordinate(plane);
        double slicePos = mprRenderer->getSlicePosition(plane);

        auto volumeCoord = transformer->screenToWorld(screenCoord, plane, slicePos);
        if (!volumeCoord) {
            return;
        }

        auto voxel = transformer->worldToVoxel(*volumeCoord);
        int sliceIndex = transformer->getSliceIndex(plane, slicePos);

        services::Point2D pos2D = mapVoxelTo2D(voxel, plane);

        if (pos2D != view.lastMousePos) {
            segmentationController->onMouseMove(pos2D, sliceIndex);
            view.lastMousePos = pos2D;
        }
    }

    void handleMouseRelease(services::MPRPlane plane) {
        int index = static_cast<int>(plane);
        auto& view = planeViews[index];

        if (!view.isMouseDown) {
            return;
        }

        view.isMouseDown = false;

        if (segmentationController->getActiveTool() == services::SegmentationTool::None) {
            return;
        }

        auto screenCoord = getScreenCoordinate(plane);
        double slicePos = mprRenderer->getSlicePosition(plane);

        auto volumeCoord = transformer->screenToWorld(screenCoord, plane, slicePos);
        if (!volumeCoord) {
            return;
        }

        auto voxel = transformer->worldToVoxel(*volumeCoord);
        int sliceIndex = transformer->getSliceIndex(plane, slicePos);

        services::Point2D pos2D = mapVoxelTo2D(voxel, plane);

        segmentationController->onMouseRelease(pos2D, sliceIndex);
    }

    void handleMouseWheel(services::MPRPlane plane, int delta) {
        if (scrollMode == ScrollMode::Phase) {
            // In Phase mode, scroll wheel navigates cardiac phases
            emit widget->phaseScrollRequested(delta);
            return;
        }

        // Default Slice mode: scroll through slices
        mprRenderer->scrollSlice(plane, delta);

        // Update overlay for new slice position
        double slicePos = mprRenderer->getSlicePosition(plane);
        labelMapOverlay->updateSlice(plane, slicePos);

        emit widget->slicePositionChanged(plane, slicePos);
        updateView(plane);
    }

    // Map 3D voxel index to 2D position based on plane orientation
    services::Point2D mapVoxelTo2D(const services::coordinate::VoxelIndex& voxel,
                                    services::MPRPlane plane) {
        switch (plane) {
            case services::MPRPlane::Axial:
                // Axial: X, Y visible
                return services::Point2D(voxel.i, voxel.j);
            case services::MPRPlane::Coronal:
                // Coronal: X, Z visible
                return services::Point2D(voxel.i, voxel.k);
            case services::MPRPlane::Sagittal:
                // Sagittal: Y, Z visible
                return services::Point2D(voxel.j, voxel.k);
        }
        return services::Point2D(0, 0);
    }

    void updateView(services::MPRPlane plane) {
        int index = static_cast<int>(plane);
        if (planeViews[index].vtkWidget) {
            planeViews[index].renderWindow->Render();
        }
    }

    void updateAllViews() {
        mprRenderer->update();
        for (int i = 0; i < 3; ++i) {
            if (planeViews[i].vtkWidget) {
                planeViews[i].renderWindow->Render();
            }
        }
    }
};

// MPRInteractorCallback::Execute implementation (after Impl is defined)
void MPRInteractorCallback::Execute(vtkObject* caller, unsigned long eventId,
                                     void* /*callData*/) {
    if (!widgetImpl_) {
        return;
    }

    auto* impl = static_cast<MPRViewWidget::Impl*>(widgetImpl_);

    switch (eventId) {
        case vtkCommand::LeftButtonPressEvent:
            impl->handleMousePress(plane_);
            break;
        case vtkCommand::LeftButtonReleaseEvent:
            impl->handleMouseRelease(plane_);
            break;
        case vtkCommand::MouseMoveEvent:
            impl->handleMouseMove(plane_);
            break;
        case vtkCommand::MouseWheelForwardEvent:
            impl->handleMouseWheel(plane_, 1);
            break;
        case vtkCommand::MouseWheelBackwardEvent:
            impl->handleMouseWheel(plane_, -1);
            break;
    }
}

MPRViewWidget::MPRViewWidget(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>(this))
{
    // Create 2x2 layout for MPR views
    auto layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Setup three plane views
    impl_->setupPlaneView(services::MPRPlane::Axial, this);
    impl_->setupPlaneView(services::MPRPlane::Coronal, this);
    impl_->setupPlaneView(services::MPRPlane::Sagittal, this);

    // Arrange in grid: Axial (top-left), Sagittal (top-right), Coronal (bottom-left)
    layout->addWidget(impl_->planeViews[0].vtkWidget, 0, 0);  // Axial
    layout->addWidget(impl_->planeViews[2].vtkWidget, 0, 1);  // Sagittal
    layout->addWidget(impl_->planeViews[1].vtkWidget, 1, 0);  // Coronal

    setLayout(layout);

    // Setup MPR renderer callbacks
    impl_->mprRenderer->setSlicePositionCallback(
        [this](services::MPRPlane plane, double position) {
            impl_->labelMapOverlay->updateSlice(plane, position);
            emit slicePositionChanged(plane, position);
        });

    impl_->mprRenderer->setCrosshairCallback(
        [this](double x, double y, double z) {
            emit crosshairPositionChanged(x, y, z);
        });
}

MPRViewWidget::~MPRViewWidget() = default;

void MPRViewWidget::setImageData(vtkSmartPointer<vtkImageData> imageData) {
    impl_->imageData = imageData;
    impl_->mprRenderer->setInputData(imageData);
    impl_->transformer->setImageData(imageData);

    if (imageData) {
        int* dims = imageData->GetDimensions();

        // Initialize segmentation controller with image dimensions
        auto result = impl_->segmentationController->initializeLabelMap(
            dims[0], dims[1], dims[2]);

        if (result) {
            // Setup label map overlay
            impl_->labelMapOverlay->setLabelMap(
                impl_->segmentationController->getLabelMap());

            // Attach overlay to each renderer
            for (int i = 0; i < 3; ++i) {
                auto plane = static_cast<services::MPRPlane>(i);
                impl_->labelMapOverlay->attachToRenderer(
                    impl_->mprRenderer->getRenderer(plane), plane);
            }
        }
    }

    impl_->updateAllViews();
}

vtkSmartPointer<vtkImageData> MPRViewWidget::getImageData() const {
    return impl_->imageData;
}

void MPRViewWidget::setWindowLevel(double width, double center) {
    impl_->mprRenderer->setWindowLevel(width, center);
    impl_->updateAllViews();
    emit windowLevelChanged(width, center);
}

std::pair<double, double> MPRViewWidget::getWindowLevel() const {
    return impl_->mprRenderer->getWindowLevel();
}

void MPRViewWidget::resetViews() {
    impl_->mprRenderer->resetViews();

    // Update overlay for new positions
    for (int i = 0; i < 3; ++i) {
        auto plane = static_cast<services::MPRPlane>(i);
        double pos = impl_->mprRenderer->getSlicePosition(plane);
        impl_->labelMapOverlay->updateSlice(plane, pos);
    }

    impl_->updateAllViews();
}

void MPRViewWidget::setSegmentationTool(services::SegmentationTool tool) {
    impl_->segmentationController->setActiveTool(tool);
    emit segmentationToolChanged(tool);
}

services::SegmentationTool MPRViewWidget::getSegmentationTool() const {
    return impl_->segmentationController->getActiveTool();
}

void MPRViewWidget::setSegmentationBrushSize(int size) {
    impl_->segmentationController->setBrushSize(size);
}

int MPRViewWidget::getSegmentationBrushSize() const {
    return impl_->segmentationController->getBrushSize();
}

void MPRViewWidget::setSegmentationBrushShape(services::BrushShape shape) {
    impl_->segmentationController->setBrushShape(shape);
}

services::BrushShape MPRViewWidget::getSegmentationBrushShape() const {
    return impl_->segmentationController->getBrushShape();
}

void MPRViewWidget::setSegmentationActiveLabel(uint8_t labelId) {
    impl_->segmentationController->setActiveLabel(labelId);
}

uint8_t MPRViewWidget::getSegmentationActiveLabel() const {
    return impl_->segmentationController->getActiveLabel();
}

void MPRViewWidget::setLabelColor(uint8_t labelId, const services::LabelColor& color) {
    impl_->labelMapOverlay->setLabelColor(labelId, color);
    impl_->updateAllViews();
}

void MPRViewWidget::undoSegmentationOperation() {
    auto tool = impl_->segmentationController->getActiveTool();
    if (tool == services::SegmentationTool::Polygon) {
        impl_->segmentationController->undoLastPolygonVertex();
    } else if (tool == services::SegmentationTool::SmartScissors) {
        impl_->segmentationController->undoLastSmartScissorsAnchor();
    }
}

void MPRViewWidget::completeSegmentationOperation() {
    auto tool = impl_->segmentationController->getActiveTool();
    int sliceIndex = getSliceIndex(impl_->activePlane);

    if (tool == services::SegmentationTool::Polygon) {
        impl_->segmentationController->completePolygon(sliceIndex);
    } else if (tool == services::SegmentationTool::SmartScissors) {
        impl_->segmentationController->completeSmartScissors(sliceIndex);
    }
}

void MPRViewWidget::clearAllSegmentation() {
    impl_->segmentationController->clearAll();
    impl_->labelMapOverlay->updateAll();
    impl_->updateAllViews();
}

bool MPRViewWidget::isSegmentationModeActive() const {
    return impl_->segmentationController->getActiveTool() !=
           services::SegmentationTool::None;
}

void MPRViewWidget::setOverlayVisible(bool visible) {
    impl_->labelMapOverlay->setVisible(visible);
    impl_->updateAllViews();
}

void MPRViewWidget::setOverlayOpacity(double opacity) {
    impl_->labelMapOverlay->setOpacity(opacity);
    impl_->updateAllViews();
}

int MPRViewWidget::getSliceIndex(services::MPRPlane plane) const {
    double slicePos = impl_->mprRenderer->getSlicePosition(plane);
    return impl_->transformer->getSliceIndex(plane, slicePos);
}

services::MPRPlane MPRViewWidget::getActivePlane() const {
    return impl_->activePlane;
}

// ==================== Thick Slab Rendering Implementation ====================

void MPRViewWidget::setSlabMode(services::SlabMode mode, double thickness) {
    impl_->mprRenderer->setSlabMode(mode, thickness);
    impl_->updateAllViews();
    emit slabModeChanged(mode, thickness);
}

services::SlabMode MPRViewWidget::getSlabMode() const {
    return impl_->mprRenderer->getSlabMode();
}

double MPRViewWidget::getSlabThickness() const {
    return impl_->mprRenderer->getSlabThickness();
}

void MPRViewWidget::setPlaneSlabMode(services::MPRPlane plane, services::SlabMode mode, double thickness) {
    impl_->mprRenderer->setPlaneSlabMode(plane, mode, thickness);
    impl_->updateView(plane);
}

services::SlabMode MPRViewWidget::getPlaneSlabMode(services::MPRPlane plane) const {
    return impl_->mprRenderer->getPlaneSlabMode(plane);
}

double MPRViewWidget::getPlaneSlabThickness(services::MPRPlane plane) const {
    return impl_->mprRenderer->getPlaneSlabThickness(plane);
}

int MPRViewWidget::getEffectiveSliceCount(services::MPRPlane plane) const {
    return impl_->mprRenderer->getEffectiveSliceCount(plane);
}

void MPRViewWidget::setCrosshairPosition(double x, double y, double z) {
    impl_->mprRenderer->setCrosshairPosition(x, y, z);

    // Update overlays for new positions
    for (int i = 0; i < 3; ++i) {
        auto plane = static_cast<services::MPRPlane>(i);
        double pos = impl_->mprRenderer->getSlicePosition(plane);
        impl_->labelMapOverlay->updateSlice(plane, pos);
    }

    impl_->updateAllViews();
    emit crosshairPositionChanged(x, y, z);
}

void MPRViewWidget::setScrollMode(ScrollMode mode) {
    impl_->scrollMode = mode;
}

void MPRViewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    impl_->updateAllViews();
}

} // namespace dicom_viewer::ui

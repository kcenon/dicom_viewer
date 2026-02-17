#include "services/render/asc_view_controller.hpp"

#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

namespace dicom_viewer::services {

class AscViewController::Impl {
public:
    vtkSmartPointer<vtkImageData> imageData;
    vtkRenderer* renderer = nullptr;

    // Three orthogonal plane pipelines
    struct PlaneState {
        vtkSmartPointer<vtkImageSliceMapper> mapper;
        vtkSmartPointer<vtkImageSlice> slice;
        int currentSlice = 0;
    };

    PlaneState axial;    // Z orientation
    PlaneState coronal;  // Y orientation
    PlaneState sagittal; // X orientation

    vtkSmartPointer<vtkImageProperty> sharedProperty;

    bool visible = false;
    bool addedToRenderer = false;
    double opacity = 1.0;

    Impl() {
        // Create mappers
        axial.mapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        coronal.mapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        sagittal.mapper = vtkSmartPointer<vtkImageSliceMapper>::New();

        axial.mapper->SetOrientationToZ();
        coronal.mapper->SetOrientationToY();
        sagittal.mapper->SetOrientationToX();

        // Create image slices (actors)
        axial.slice = vtkSmartPointer<vtkImageSlice>::New();
        coronal.slice = vtkSmartPointer<vtkImageSlice>::New();
        sagittal.slice = vtkSmartPointer<vtkImageSlice>::New();

        axial.slice->SetMapper(axial.mapper);
        coronal.slice->SetMapper(coronal.mapper);
        sagittal.slice->SetMapper(sagittal.mapper);

        // Shared image property for W/L
        sharedProperty = vtkSmartPointer<vtkImageProperty>::New();
        sharedProperty->SetColorWindow(400.0);
        sharedProperty->SetColorLevel(40.0);
        sharedProperty->SetInterpolationTypeToLinear();

        axial.slice->SetProperty(sharedProperty);
        coronal.slice->SetProperty(sharedProperty);
        sagittal.slice->SetProperty(sharedProperty);

        // Start hidden
        axial.slice->SetVisibility(0);
        coronal.slice->SetVisibility(0);
        sagittal.slice->SetVisibility(0);
    }

    void addToRenderer() {
        if (!renderer || !imageData || addedToRenderer) return;
        renderer->AddViewProp(axial.slice);
        renderer->AddViewProp(coronal.slice);
        renderer->AddViewProp(sagittal.slice);
        addedToRenderer = true;
    }

    void removeFromRenderer() {
        if (!renderer || !addedToRenderer) return;
        renderer->RemoveViewProp(axial.slice);
        renderer->RemoveViewProp(coronal.slice);
        renderer->RemoveViewProp(sagittal.slice);
        addedToRenderer = false;
    }

    void updateVisibility() {
        int vis = visible ? 1 : 0;
        axial.slice->SetVisibility(vis);
        coronal.slice->SetVisibility(vis);
        sagittal.slice->SetVisibility(vis);
    }

    void centerSlices() {
        if (!imageData) return;
        int dims[3];
        imageData->GetDimensions(dims);
        axial.currentSlice = dims[2] / 2;
        coronal.currentSlice = dims[1] / 2;
        sagittal.currentSlice = dims[0] / 2;
        axial.mapper->SetSliceNumber(axial.currentSlice);
        coronal.mapper->SetSliceNumber(coronal.currentSlice);
        sagittal.mapper->SetSliceNumber(sagittal.currentSlice);
    }
};

AscViewController::AscViewController()
    : impl_(std::make_unique<Impl>())
{}

AscViewController::~AscViewController() {
    if (impl_) {
        impl_->removeFromRenderer();
    }
}

AscViewController::AscViewController(AscViewController&&) noexcept = default;
AscViewController& AscViewController::operator=(AscViewController&&) noexcept = default;

void AscViewController::setInputData(vtkSmartPointer<vtkImageData> imageData) {
    impl_->imageData = std::move(imageData);
    impl_->axial.mapper->SetInputData(impl_->imageData);
    impl_->coronal.mapper->SetInputData(impl_->imageData);
    impl_->sagittal.mapper->SetInputData(impl_->imageData);
    impl_->centerSlices();
    // If renderer was set before data, add props now
    if (impl_->renderer && !impl_->addedToRenderer) {
        impl_->addToRenderer();
        impl_->updateVisibility();
    }
}

vtkSmartPointer<vtkImageData> AscViewController::getInputData() const {
    return impl_->imageData;
}

void AscViewController::setRenderer(vtkRenderer* renderer) {
    impl_->removeFromRenderer();
    impl_->renderer = renderer;
    if (renderer) {
        impl_->addToRenderer();
        impl_->updateVisibility();
    }
}

vtkRenderer* AscViewController::getRenderer() const {
    return impl_->renderer;
}

void AscViewController::setVisible(bool visible) {
    impl_->visible = visible;
    impl_->updateVisibility();
}

bool AscViewController::isVisible() const {
    return impl_->visible;
}

void AscViewController::setAxialSlice(int slice) {
    impl_->axial.currentSlice = slice;
    impl_->axial.mapper->SetSliceNumber(slice);
}

void AscViewController::setCoronalSlice(int slice) {
    impl_->coronal.currentSlice = slice;
    impl_->coronal.mapper->SetSliceNumber(slice);
}

void AscViewController::setSagittalSlice(int slice) {
    impl_->sagittal.currentSlice = slice;
    impl_->sagittal.mapper->SetSliceNumber(slice);
}

void AscViewController::setSlicePositions(int axial, int coronal, int sagittal) {
    setAxialSlice(axial);
    setCoronalSlice(coronal);
    setSagittalSlice(sagittal);
}

int AscViewController::axialSlice() const {
    return impl_->axial.currentSlice;
}

int AscViewController::coronalSlice() const {
    return impl_->coronal.currentSlice;
}

int AscViewController::sagittalSlice() const {
    return impl_->sagittal.currentSlice;
}

std::array<int, 3> AscViewController::dimensions() const {
    if (!impl_->imageData) return {0, 0, 0};
    int dims[3];
    impl_->imageData->GetDimensions(dims);
    return {dims[0], dims[1], dims[2]};
}

void AscViewController::setWindowLevel(double width, double center) {
    impl_->sharedProperty->SetColorWindow(width);
    impl_->sharedProperty->SetColorLevel(center);
}

std::pair<double, double> AscViewController::getWindowLevel() const {
    return {
        impl_->sharedProperty->GetColorWindow(),
        impl_->sharedProperty->GetColorLevel()
    };
}

void AscViewController::setOpacity(double opacity) {
    impl_->opacity = opacity;
    impl_->sharedProperty->SetOpacity(opacity);
}

double AscViewController::getOpacity() const {
    return impl_->opacity;
}

void AscViewController::update() {
    if (impl_->renderer && impl_->renderer->GetRenderWindow()) {
        impl_->renderer->GetRenderWindow()->Render();
    }
}

}  // namespace dicom_viewer::services

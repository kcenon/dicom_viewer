#include "services/mpr_renderer.hpp"

#include <vtkImageReslice.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageMapToColors.h>
#include <vtkLookupTable.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkLineSource.h>
#include <vtkAppendPolyData.h>
#include <vtkNew.h>

#include <algorithm>
#include <cmath>

namespace dicom_viewer::services {

class MPRRenderer::Impl {
public:
    // Input data
    vtkSmartPointer<vtkImageData> inputData;

    // Renderers for each plane (Axial=0, Coronal=1, Sagittal=2)
    std::array<vtkSmartPointer<vtkRenderer>, 3> renderers;

    // Reslice filters for extracting slices
    std::array<vtkSmartPointer<vtkImageReslice>, 3> reslicers;

    // Color mapping for window/level
    std::array<vtkSmartPointer<vtkImageMapToColors>, 3> colorMappers;

    // Image actors for displaying slices
    std::array<vtkSmartPointer<vtkImageActor>, 3> imageActors;

    // Lookup table for window/level
    vtkSmartPointer<vtkLookupTable> lookupTable;

    // Crosshair actors for each plane
    std::array<vtkSmartPointer<vtkActor>, 3> crosshairActors;

    // Slice positions in world coordinates
    std::array<double, 3> slicePositions = {0.0, 0.0, 0.0};

    // Crosshair position in world coordinates
    std::array<double, 3> crosshairPosition = {0.0, 0.0, 0.0};

    // Volume bounds [xmin, xmax, ymin, ymax, zmin, zmax]
    std::array<double, 6> bounds = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    // Volume spacing
    std::array<double, 3> spacing = {1.0, 1.0, 1.0};

    // Window/Level
    double windowWidth = 400.0;
    double windowCenter = 40.0;

    // Slab mode settings
    SlabMode slabMode = SlabMode::None;
    double slabThickness = 1.0;

    // Crosshair visibility
    bool crosshairVisible = true;

    // Callbacks
    SlicePositionCallback slicePositionCallback;
    CrosshairCallback crosshairCallback;

    Impl() {
        // Initialize lookup table
        lookupTable = vtkSmartPointer<vtkLookupTable>::New();
        lookupTable->SetTableRange(0, 1);
        lookupTable->SetSaturationRange(0, 0);
        lookupTable->SetHueRange(0, 0);
        lookupTable->SetValueRange(0, 1);
        lookupTable->Build();

        // Initialize components for each plane
        for (int i = 0; i < 3; ++i) {
            renderers[i] = vtkSmartPointer<vtkRenderer>::New();
            renderers[i]->SetBackground(0.0, 0.0, 0.0);

            reslicers[i] = vtkSmartPointer<vtkImageReslice>::New();
            reslicers[i]->SetOutputDimensionality(2);
            reslicers[i]->SetInterpolationModeToLinear();

            colorMappers[i] = vtkSmartPointer<vtkImageMapToColors>::New();
            colorMappers[i]->SetLookupTable(lookupTable);
            colorMappers[i]->SetInputConnection(reslicers[i]->GetOutputPort());

            imageActors[i] = vtkSmartPointer<vtkImageActor>::New();
            imageActors[i]->GetMapper()->SetInputConnection(
                colorMappers[i]->GetOutputPort());

            renderers[i]->AddActor(imageActors[i]);

            // Initialize crosshair actor
            crosshairActors[i] = createCrosshairActor();
            renderers[i]->AddActor(crosshairActors[i]);
        }

        // Setup reslice matrices for each orientation
        setupResliceMatrices();
    }

    void setupResliceMatrices() {
        // Axial (XY plane): Default orientation - viewing from top (superior)
        auto axialMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        axialMatrix->Identity();
        // Default orientation: X right, Y anterior, Z superior
        reslicers[0]->SetResliceAxes(axialMatrix);

        // Coronal (XZ plane): Viewing from front (anterior)
        auto coronalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        coronalMatrix->Identity();
        // Rotate -90 degrees around X axis
        coronalMatrix->SetElement(1, 1, 0);
        coronalMatrix->SetElement(1, 2, 1);
        coronalMatrix->SetElement(2, 1, -1);
        coronalMatrix->SetElement(2, 2, 0);
        reslicers[1]->SetResliceAxes(coronalMatrix);

        // Sagittal (YZ plane): Viewing from right (lateral)
        auto sagittalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        sagittalMatrix->Identity();
        // Rotate 90 degrees around Y axis
        sagittalMatrix->SetElement(0, 0, 0);
        sagittalMatrix->SetElement(0, 2, -1);
        sagittalMatrix->SetElement(2, 0, 1);
        sagittalMatrix->SetElement(2, 2, 0);
        reslicers[2]->SetResliceAxes(sagittalMatrix);
    }

    vtkSmartPointer<vtkActor> createCrosshairActor() {
        auto actor = vtkSmartPointer<vtkActor>::New();
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(1.0, 1.0, 0.0);  // Yellow crosshair
        actor->GetProperty()->SetLineWidth(1.0);
        return actor;
    }

    void updateSlicePosition(int planeIndex) {
        if (!inputData) {
            return;
        }

        auto matrix = reslicers[planeIndex]->GetResliceAxes();
        double position = slicePositions[planeIndex];

        // Update the translation component based on plane
        switch (planeIndex) {
            case 0:  // Axial - translate along Z
                matrix->SetElement(2, 3, position);
                break;
            case 1:  // Coronal - translate along Y
                matrix->SetElement(1, 3, position);
                break;
            case 2:  // Sagittal - translate along X
                matrix->SetElement(0, 3, position);
                break;
        }

        reslicers[planeIndex]->Modified();
    }

    void updateWindowLevel() {
        double lower = windowCenter - windowWidth / 2.0;
        double upper = windowCenter + windowWidth / 2.0;

        lookupTable->SetTableRange(lower, upper);
        lookupTable->Build();

        for (int i = 0; i < 3; ++i) {
            colorMappers[i]->Modified();
        }
    }

    void updateCrosshair(int planeIndex) {
        if (!inputData || !crosshairVisible) {
            crosshairActors[planeIndex]->SetVisibility(false);
            return;
        }

        crosshairActors[planeIndex]->SetVisibility(true);

        // Create crosshair lines based on the plane
        auto appendPolyData = vtkSmartPointer<vtkAppendPolyData>::New();

        double cx = crosshairPosition[0];
        double cy = crosshairPosition[1];
        double cz = crosshairPosition[2];

        // Get image extent for line length
        int extent[6];
        inputData->GetExtent(extent);
        double* origin = inputData->GetOrigin();

        double xMin = origin[0] + extent[0] * spacing[0];
        double xMax = origin[0] + extent[1] * spacing[0];
        double yMin = origin[1] + extent[2] * spacing[1];
        double yMax = origin[1] + extent[3] * spacing[1];
        double zMin = origin[2] + extent[4] * spacing[2];
        double zMax = origin[2] + extent[5] * spacing[2];

        vtkSmartPointer<vtkLineSource> hLine = vtkSmartPointer<vtkLineSource>::New();
        vtkSmartPointer<vtkLineSource> vLine = vtkSmartPointer<vtkLineSource>::New();

        switch (planeIndex) {
            case 0:  // Axial (XY plane) - horizontal=X, vertical=Y
                hLine->SetPoint1(xMin, cy, cz);
                hLine->SetPoint2(xMax, cy, cz);
                vLine->SetPoint1(cx, yMin, cz);
                vLine->SetPoint2(cx, yMax, cz);
                break;
            case 1:  // Coronal (XZ plane) - horizontal=X, vertical=Z
                hLine->SetPoint1(xMin, cy, cz);
                hLine->SetPoint2(xMax, cy, cz);
                vLine->SetPoint1(cx, cy, zMin);
                vLine->SetPoint2(cx, cy, zMax);
                break;
            case 2:  // Sagittal (YZ plane) - horizontal=Y, vertical=Z
                hLine->SetPoint1(cx, yMin, cz);
                hLine->SetPoint2(cx, yMax, cz);
                vLine->SetPoint1(cx, cy, zMin);
                vLine->SetPoint2(cx, cy, zMax);
                break;
        }

        hLine->Update();
        vLine->Update();

        appendPolyData->AddInputData(hLine->GetOutput());
        appendPolyData->AddInputData(vLine->GetOutput());
        appendPolyData->Update();

        auto mapper = vtkPolyDataMapper::SafeDownCast(
            crosshairActors[planeIndex]->GetMapper());
        mapper->SetInputData(appendPolyData->GetOutput());
    }

    void setupCamera(int planeIndex) {
        if (!inputData) {
            return;
        }

        auto camera = renderers[planeIndex]->GetActiveCamera();
        camera->ParallelProjectionOn();

        double* center = inputData->GetCenter();
        double* bounds = inputData->GetBounds();

        // Calculate appropriate parallel scale
        double maxDim = 0;

        switch (planeIndex) {
            case 0:  // Axial - looking down Z axis
                camera->SetPosition(center[0], center[1], bounds[5] + 100);
                camera->SetFocalPoint(center[0], center[1], center[2]);
                camera->SetViewUp(0, 1, 0);
                maxDim = std::max(bounds[1] - bounds[0], bounds[3] - bounds[2]);
                break;
            case 1:  // Coronal - looking down Y axis
                camera->SetPosition(center[0], bounds[3] + 100, center[2]);
                camera->SetFocalPoint(center[0], center[1], center[2]);
                camera->SetViewUp(0, 0, 1);
                maxDim = std::max(bounds[1] - bounds[0], bounds[5] - bounds[4]);
                break;
            case 2:  // Sagittal - looking down X axis
                camera->SetPosition(bounds[1] + 100, center[1], center[2]);
                camera->SetFocalPoint(center[0], center[1], center[2]);
                camera->SetViewUp(0, 0, 1);
                maxDim = std::max(bounds[3] - bounds[2], bounds[5] - bounds[4]);
                break;
        }

        camera->SetParallelScale(maxDim / 2.0 * 1.1);
        renderers[planeIndex]->ResetCameraClippingRange();
    }

    void updateSlabMode() {
        for (int i = 0; i < 3; ++i) {
            switch (slabMode) {
                case SlabMode::None:
                    reslicers[i]->SetSlabModeToMean();
                    reslicers[i]->SetSlabNumberOfSlices(1);
                    break;
                case SlabMode::MIP:
                    reslicers[i]->SetSlabModeToMax();
                    reslicers[i]->SetSlabNumberOfSlices(
                        static_cast<int>(slabThickness / spacing[i % 3]));
                    break;
                case SlabMode::MinIP:
                    reslicers[i]->SetSlabModeToMin();
                    reslicers[i]->SetSlabNumberOfSlices(
                        static_cast<int>(slabThickness / spacing[i % 3]));
                    break;
                case SlabMode::Average:
                    reslicers[i]->SetSlabModeToMean();
                    reslicers[i]->SetSlabNumberOfSlices(
                        static_cast<int>(slabThickness / spacing[i % 3]));
                    break;
            }
            reslicers[i]->Modified();
        }
    }
};

// Constructor
MPRRenderer::MPRRenderer() : impl_(std::make_unique<Impl>()) {}

// Destructor
MPRRenderer::~MPRRenderer() = default;

// Move constructor
MPRRenderer::MPRRenderer(MPRRenderer&&) noexcept = default;

// Move assignment
MPRRenderer& MPRRenderer::operator=(MPRRenderer&&) noexcept = default;

void MPRRenderer::setInputData(vtkSmartPointer<vtkImageData> imageData) {
    impl_->inputData = imageData;

    if (imageData) {
        // Get bounds and spacing
        imageData->GetBounds(impl_->bounds.data());
        imageData->GetSpacing(impl_->spacing.data());

        // Set input for all reslicers
        for (int i = 0; i < 3; ++i) {
            impl_->reslicers[i]->SetInputData(imageData);
        }

        // Reset views to center of volume
        resetViews();
    }
}

vtkSmartPointer<vtkRenderer> MPRRenderer::getRenderer(MPRPlane plane) const {
    return impl_->renderers[static_cast<int>(plane)];
}

void MPRRenderer::setSlicePosition(MPRPlane plane, double position) {
    int planeIndex = static_cast<int>(plane);

    // Clamp position to valid range
    auto [minPos, maxPos] = getSliceRange(plane);
    position = std::clamp(position, minPos, maxPos);

    impl_->slicePositions[planeIndex] = position;
    impl_->updateSlicePosition(planeIndex);

    // Update crosshair position for synchronization
    impl_->crosshairPosition[planeIndex] = position;
    for (int i = 0; i < 3; ++i) {
        impl_->updateCrosshair(i);
    }

    // Notify callback
    if (impl_->slicePositionCallback) {
        impl_->slicePositionCallback(plane, position);
    }
}

double MPRRenderer::getSlicePosition(MPRPlane plane) const {
    return impl_->slicePositions[static_cast<int>(plane)];
}

std::pair<double, double> MPRRenderer::getSliceRange(MPRPlane plane) const {
    if (!impl_->inputData) {
        return {0.0, 0.0};
    }

    switch (plane) {
        case MPRPlane::Axial:
            return {impl_->bounds[4], impl_->bounds[5]};  // Z range
        case MPRPlane::Coronal:
            return {impl_->bounds[2], impl_->bounds[3]};  // Y range
        case MPRPlane::Sagittal:
            return {impl_->bounds[0], impl_->bounds[1]};  // X range
    }
    return {0.0, 0.0};
}

void MPRRenderer::scrollSlice(MPRPlane plane, int delta) {
    int planeIndex = static_cast<int>(plane);
    double currentPos = impl_->slicePositions[planeIndex];
    double spacing = impl_->spacing[planeIndex];

    double newPos = currentPos + delta * spacing;
    setSlicePosition(plane, newPos);
}

void MPRRenderer::setWindowLevel(double width, double center) {
    impl_->windowWidth = width;
    impl_->windowCenter = center;
    impl_->updateWindowLevel();
}

std::pair<double, double> MPRRenderer::getWindowLevel() const {
    return {impl_->windowWidth, impl_->windowCenter};
}

void MPRRenderer::setCrosshairPosition(double x, double y, double z) {
    impl_->crosshairPosition = {x, y, z};

    // Update slice positions to match crosshair
    impl_->slicePositions = {x, y, z};

    for (int i = 0; i < 3; ++i) {
        impl_->updateSlicePosition(i);
        impl_->updateCrosshair(i);
    }

    // Notify callback
    if (impl_->crosshairCallback) {
        impl_->crosshairCallback(x, y, z);
    }
}

std::array<double, 3> MPRRenderer::getCrosshairPosition() const {
    return impl_->crosshairPosition;
}

void MPRRenderer::setCrosshairVisible(bool visible) {
    impl_->crosshairVisible = visible;
    for (int i = 0; i < 3; ++i) {
        impl_->crosshairActors[i]->SetVisibility(visible);
    }
}

void MPRRenderer::setSlabMode(SlabMode mode, double thickness) {
    impl_->slabMode = mode;
    impl_->slabThickness = thickness;
    impl_->updateSlabMode();
}

void MPRRenderer::setSlicePositionCallback(SlicePositionCallback callback) {
    impl_->slicePositionCallback = std::move(callback);
}

void MPRRenderer::setCrosshairCallback(CrosshairCallback callback) {
    impl_->crosshairCallback = std::move(callback);
}

void MPRRenderer::update() {
    for (int i = 0; i < 3; ++i) {
        impl_->reslicers[i]->Update();
        impl_->colorMappers[i]->Update();
        impl_->renderers[i]->Modified();
    }
}

void MPRRenderer::resetViews() {
    if (!impl_->inputData) {
        return;
    }

    // Set slice positions to center of volume
    double* center = impl_->inputData->GetCenter();
    impl_->slicePositions = {center[0], center[1], center[2]};
    impl_->crosshairPosition = {center[0], center[1], center[2]};

    // Update each plane
    for (int i = 0; i < 3; ++i) {
        impl_->updateSlicePosition(i);
        impl_->setupCamera(i);
        impl_->updateCrosshair(i);
    }

    // Apply default window/level
    impl_->updateWindowLevel();
}

} // namespace dicom_viewer::services

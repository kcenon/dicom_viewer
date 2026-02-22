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

#include "services/render/oblique_reslice_renderer.hpp"

#include <vtkImageReslice.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageMapToColors.h>
#include <vtkLookupTable.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkNew.h>

#include <algorithm>
#include <cmath>

namespace dicom_viewer::services {

// ==================== Vector3D Implementation ====================

double Vector3D::length() const noexcept {
    return std::sqrt(x * x + y * y + z * z);
}

Vector3D Vector3D::normalized() const noexcept {
    double len = length();
    if (len < 1e-10) {
        return Vector3D{0.0, 0.0, 1.0};
    }
    return Vector3D{x / len, y / len, z / len};
}

// ==================== Implementation Class ====================

class ObliqueResliceRenderer::Impl {
public:
    // Input data
    ImageType inputData;

    // Current plane definition
    ObliquePlaneDefinition planeDef;

    // VTK pipeline components
    vtkSmartPointer<vtkImageReslice> reslice;
    vtkSmartPointer<vtkImageMapToColors> colorMapper;
    vtkSmartPointer<vtkLookupTable> lookupTable;
    vtkSmartPointer<vtkImageActor> imageActor;
    vtkRenderer* renderer = nullptr;

    // Options
    ObliqueResliceOptions options;

    // Window/Level
    double windowWidth = 400.0;
    double windowCenter = 40.0;

    // Volume bounds
    std::array<double, 6> bounds = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 3> spacing = {1.0, 1.0, 1.0};

    // Interactive rotation state
    bool interactiveRotationActive = false;
    int startX = 0;
    int startY = 0;
    double startRotX = 0.0;
    double startRotY = 0.0;
    double startRotZ = 0.0;

    // Callbacks
    PlaneChangedCallback planeChangedCallback;
    SliceChangedCallback sliceChangedCallback;

    Impl() {
        // Initialize VTK pipeline
        reslice = vtkSmartPointer<vtkImageReslice>::New();
        reslice->SetOutputDimensionality(2);
        reslice->SetInterpolationModeToLinear();
        reslice->SetBackgroundLevel(-1000.0);

        lookupTable = vtkSmartPointer<vtkLookupTable>::New();
        lookupTable->SetTableRange(0, 1);
        lookupTable->SetSaturationRange(0, 0);
        lookupTable->SetHueRange(0, 0);
        lookupTable->SetValueRange(0, 1);
        lookupTable->Build();

        colorMapper = vtkSmartPointer<vtkImageMapToColors>::New();
        colorMapper->SetLookupTable(lookupTable);
        colorMapper->SetInputConnection(reslice->GetOutputPort());

        imageActor = vtkSmartPointer<vtkImageActor>::New();
        imageActor->GetMapper()->SetInputConnection(colorMapper->GetOutputPort());
    }

    void updateResliceMatrix() {
        if (!inputData) {
            return;
        }

        auto matrix = vtkSmartPointer<vtkMatrix4x4>::New();
        matrix->Identity();

        // Get center point (use volume center if not set)
        double cx = planeDef.center.x;
        double cy = planeDef.center.y;
        double cz = planeDef.center.z;

        if (cx == 0.0 && cy == 0.0 && cz == 0.0) {
            double* center = inputData->GetCenter();
            cx = center[0];
            cy = center[1];
            cz = center[2];
        }

        // Build transformation: translate to center, rotate, translate back
        auto transform = vtkSmartPointer<vtkTransform>::New();
        transform->Identity();

        // Apply rotations in XYZ order
        transform->RotateX(planeDef.rotationX);
        transform->RotateY(planeDef.rotationY);
        transform->RotateZ(planeDef.rotationZ);

        // Get rotation matrix
        vtkMatrix4x4* rotMatrix = transform->GetMatrix();

        // Compute normal for slice offset
        double normal[3] = {
            rotMatrix->GetElement(0, 2),
            rotMatrix->GetElement(1, 2),
            rotMatrix->GetElement(2, 2)
        };

        // Apply slice offset along normal
        double offsetX = cx + planeDef.sliceOffset * normal[0];
        double offsetY = cy + planeDef.sliceOffset * normal[1];
        double offsetZ = cz + planeDef.sliceOffset * normal[2];

        // Set up reslice matrix
        // Row 0: X axis of the output image
        matrix->SetElement(0, 0, rotMatrix->GetElement(0, 0));
        matrix->SetElement(0, 1, rotMatrix->GetElement(0, 1));
        matrix->SetElement(0, 2, rotMatrix->GetElement(0, 2));

        // Row 1: Y axis of the output image
        matrix->SetElement(1, 0, rotMatrix->GetElement(1, 0));
        matrix->SetElement(1, 1, rotMatrix->GetElement(1, 1));
        matrix->SetElement(1, 2, rotMatrix->GetElement(1, 2));

        // Row 2: Normal (Z axis / slice direction)
        matrix->SetElement(2, 0, rotMatrix->GetElement(2, 0));
        matrix->SetElement(2, 1, rotMatrix->GetElement(2, 1));
        matrix->SetElement(2, 2, rotMatrix->GetElement(2, 2));

        // Translation (slice position)
        matrix->SetElement(0, 3, offsetX);
        matrix->SetElement(1, 3, offsetY);
        matrix->SetElement(2, 3, offsetZ);

        reslice->SetResliceAxes(matrix);
        reslice->Modified();
    }

    void updateWindowLevel() {
        double lower = windowCenter - windowWidth / 2.0;
        double upper = windowCenter + windowWidth / 2.0;

        lookupTable->SetTableRange(lower, upper);
        lookupTable->Build();
        colorMapper->Modified();
    }

    void updateInterpolation() {
        switch (options.interpolation) {
            case InterpolationMode::NearestNeighbor:
                reslice->SetInterpolationModeToNearestNeighbor();
                break;
            case InterpolationMode::Linear:
                reslice->SetInterpolationModeToLinear();
                break;
            case InterpolationMode::Cubic:
                reslice->SetInterpolationModeToCubic();
                break;
        }
        reslice->SetBackgroundLevel(options.backgroundValue);
    }

    void setupCamera() {
        if (!renderer || !inputData) {
            return;
        }

        auto camera = renderer->GetActiveCamera();
        camera->ParallelProjectionOn();

        double* center = inputData->GetCenter();
        double maxDim = std::max({
            bounds[1] - bounds[0],
            bounds[3] - bounds[2],
            bounds[5] - bounds[4]
        });

        // Position camera based on current rotation
        auto transform = vtkSmartPointer<vtkTransform>::New();
        transform->RotateX(planeDef.rotationX);
        transform->RotateY(planeDef.rotationY);
        transform->RotateZ(planeDef.rotationZ);

        double viewDir[3] = {0, 0, 1};
        double upDir[3] = {0, 1, 0};
        transform->TransformVector(viewDir, viewDir);
        transform->TransformVector(upDir, upDir);

        double distance = maxDim * 2.0;
        camera->SetPosition(
            center[0] - viewDir[0] * distance,
            center[1] - viewDir[1] * distance,
            center[2] - viewDir[2] * distance
        );
        camera->SetFocalPoint(center[0], center[1], center[2]);
        camera->SetViewUp(upDir[0], upDir[1], upDir[2]);
        camera->SetParallelScale(maxDim / 2.0 * 1.1);

        renderer->ResetCameraClippingRange();
    }

    double computeSliceRange() const {
        if (!inputData) {
            return 0.0;
        }

        // Compute diagonal of the bounding box
        double dx = bounds[1] - bounds[0];
        double dy = bounds[3] - bounds[2];
        double dz = bounds[5] - bounds[4];
        return std::sqrt(dx * dx + dy * dy + dz * dz) / 2.0;
    }

    void notifyPlaneChanged() {
        if (planeChangedCallback) {
            planeChangedCallback(planeDef);
        }
    }

    void notifySliceChanged() {
        if (sliceChangedCallback) {
            sliceChangedCallback(planeDef.sliceOffset);
        }
    }
};

// ==================== ObliqueResliceRenderer Implementation ====================

ObliqueResliceRenderer::ObliqueResliceRenderer()
    : impl_(std::make_unique<Impl>()) {}

ObliqueResliceRenderer::~ObliqueResliceRenderer() = default;

ObliqueResliceRenderer::ObliqueResliceRenderer(ObliqueResliceRenderer&&) noexcept = default;
ObliqueResliceRenderer& ObliqueResliceRenderer::operator=(ObliqueResliceRenderer&&) noexcept = default;

void ObliqueResliceRenderer::setInputData(ImageType imageData) {
    impl_->inputData = imageData;

    if (imageData) {
        imageData->GetBounds(impl_->bounds.data());
        imageData->GetSpacing(impl_->spacing.data());
        impl_->reslice->SetInputData(imageData);

        // Set default center to volume center
        double* center = imageData->GetCenter();
        impl_->planeDef.center = Point3D{center[0], center[1], center[2]};

        impl_->updateResliceMatrix();
        impl_->updateWindowLevel();
    }
}

ObliqueResliceRenderer::ImageType ObliqueResliceRenderer::getInputData() const {
    return impl_->inputData;
}

void ObliqueResliceRenderer::setPlaneByRotation(double rotX, double rotY, double rotZ) {
    impl_->planeDef.rotationX = rotX;
    impl_->planeDef.rotationY = rotY;
    impl_->planeDef.rotationZ = rotZ;

    impl_->updateResliceMatrix();
    impl_->setupCamera();
    impl_->notifyPlaneChanged();
}

void ObliqueResliceRenderer::setPlaneByThreePoints(
    const Point3D& p1, const Point3D& p2, const Point3D& p3) {

    // Compute vectors from p1 to p2 and p1 to p3
    double v1[3] = {p2.x - p1.x, p2.y - p1.y, p2.z - p1.z};
    double v2[3] = {p3.x - p1.x, p3.y - p1.y, p3.z - p1.z};

    // Compute normal as cross product v1 x v2
    Vector3D normal{
        v1[1] * v2[2] - v1[2] * v2[1],
        v1[2] * v2[0] - v1[0] * v2[2],
        v1[0] * v2[1] - v1[1] * v2[0]
    };
    normal = normal.normalized();

    // Compute center as centroid
    Point3D center{
        (p1.x + p2.x + p3.x) / 3.0,
        (p1.y + p2.y + p3.y) / 3.0,
        (p1.z + p2.z + p3.z) / 3.0
    };

    setPlaneByNormal(normal, center);
}

void ObliqueResliceRenderer::setPlaneByNormal(const Vector3D& normal, const Point3D& center) {
    Vector3D n = normal.normalized();
    impl_->planeDef.center = center;

    // Convert normal to Euler angles
    // Assuming normal is the Z-axis of the rotated coordinate system

    // rotationY = atan2(nx, nz)
    double rotY = std::atan2(n.x, n.z) * 180.0 / M_PI;

    // rotationX = -asin(ny)
    double rotX = -std::asin(std::clamp(n.y, -1.0, 1.0)) * 180.0 / M_PI;

    // rotationZ typically stays at 0 unless in-plane rotation is needed
    double rotZ = 0.0;

    impl_->planeDef.rotationX = rotX;
    impl_->planeDef.rotationY = rotY;
    impl_->planeDef.rotationZ = rotZ;

    impl_->updateResliceMatrix();
    impl_->setupCamera();
    impl_->notifyPlaneChanged();
}

void ObliqueResliceRenderer::setCenter(const Point3D& center) {
    impl_->planeDef.center = center;
    impl_->updateResliceMatrix();
    impl_->notifyPlaneChanged();
}

Point3D ObliqueResliceRenderer::getCenter() const {
    return impl_->planeDef.center;
}

void ObliqueResliceRenderer::setSliceOffset(double offset) {
    double maxRange = impl_->computeSliceRange();
    offset = std::clamp(offset, -maxRange, maxRange);

    impl_->planeDef.sliceOffset = offset;
    impl_->updateResliceMatrix();
    impl_->notifySliceChanged();
}

double ObliqueResliceRenderer::getSliceOffset() const {
    return impl_->planeDef.sliceOffset;
}

std::pair<double, double> ObliqueResliceRenderer::getSliceRange() const {
    double maxRange = impl_->computeSliceRange();
    return {-maxRange, maxRange};
}

void ObliqueResliceRenderer::scrollSlice(int delta) {
    // Use average spacing as scroll increment
    double avgSpacing = (impl_->spacing[0] + impl_->spacing[1] + impl_->spacing[2]) / 3.0;
    double newOffset = impl_->planeDef.sliceOffset + delta * avgSpacing;
    setSliceOffset(newOffset);
}

ObliquePlaneDefinition ObliqueResliceRenderer::getCurrentPlane() const {
    return impl_->planeDef;
}

vtkSmartPointer<vtkMatrix4x4> ObliqueResliceRenderer::getResliceMatrix() const {
    return impl_->reslice->GetResliceAxes();
}

Vector3D ObliqueResliceRenderer::getPlaneNormal() const {
    auto transform = vtkSmartPointer<vtkTransform>::New();
    transform->RotateX(impl_->planeDef.rotationX);
    transform->RotateY(impl_->planeDef.rotationY);
    transform->RotateZ(impl_->planeDef.rotationZ);

    double normal[3] = {0, 0, 1};
    transform->TransformVector(normal, normal);

    return Vector3D{normal[0], normal[1], normal[2]};
}

void ObliqueResliceRenderer::startInteractiveRotation(int x, int y) {
    impl_->interactiveRotationActive = true;
    impl_->startX = x;
    impl_->startY = y;
    impl_->startRotX = impl_->planeDef.rotationX;
    impl_->startRotY = impl_->planeDef.rotationY;
    impl_->startRotZ = impl_->planeDef.rotationZ;
}

void ObliqueResliceRenderer::updateInteractiveRotation(int x, int y) {
    if (!impl_->interactiveRotationActive) {
        return;
    }

    // Compute delta from start position
    int deltaX = x - impl_->startX;
    int deltaY = y - impl_->startY;

    // Convert pixel movement to rotation (0.5 degrees per pixel)
    double rotationSensitivity = 0.5;
    double newRotY = impl_->startRotY + deltaX * rotationSensitivity;
    double newRotX = impl_->startRotX + deltaY * rotationSensitivity;

    // Clamp X rotation to avoid gimbal lock issues
    newRotX = std::clamp(newRotX, -89.0, 89.0);

    impl_->planeDef.rotationX = newRotX;
    impl_->planeDef.rotationY = newRotY;

    impl_->updateResliceMatrix();
    impl_->setupCamera();
    impl_->notifyPlaneChanged();
}

void ObliqueResliceRenderer::endInteractiveRotation() {
    impl_->interactiveRotationActive = false;
}

bool ObliqueResliceRenderer::isInteractiveRotationActive() const {
    return impl_->interactiveRotationActive;
}

void ObliqueResliceRenderer::setAxial() {
    impl_->planeDef.rotationX = 0.0;
    impl_->planeDef.rotationY = 0.0;
    impl_->planeDef.rotationZ = 0.0;
    impl_->planeDef.sliceOffset = 0.0;

    impl_->updateResliceMatrix();
    impl_->setupCamera();
    impl_->notifyPlaneChanged();
}

void ObliqueResliceRenderer::setCoronal() {
    impl_->planeDef.rotationX = -90.0;
    impl_->planeDef.rotationY = 0.0;
    impl_->planeDef.rotationZ = 0.0;
    impl_->planeDef.sliceOffset = 0.0;

    impl_->updateResliceMatrix();
    impl_->setupCamera();
    impl_->notifyPlaneChanged();
}

void ObliqueResliceRenderer::setSagittal() {
    impl_->planeDef.rotationX = 0.0;
    impl_->planeDef.rotationY = 90.0;
    impl_->planeDef.rotationZ = 0.0;
    impl_->planeDef.sliceOffset = 0.0;

    impl_->updateResliceMatrix();
    impl_->setupCamera();
    impl_->notifyPlaneChanged();
}

void ObliqueResliceRenderer::setRenderer(vtkRenderer* renderer) {
    // Remove actor from old renderer
    if (impl_->renderer && impl_->imageActor) {
        impl_->renderer->RemoveActor(impl_->imageActor);
    }

    impl_->renderer = renderer;

    // Add actor to new renderer
    if (renderer && impl_->imageActor) {
        renderer->AddActor(impl_->imageActor);
        impl_->setupCamera();
    }
}

vtkRenderer* ObliqueResliceRenderer::getRenderer() const {
    return impl_->renderer;
}

void ObliqueResliceRenderer::setOptions(const ObliqueResliceOptions& options) {
    impl_->options = options;
    impl_->updateInterpolation();

    if (options.outputSpacing > 0) {
        impl_->reslice->SetOutputSpacing(
            options.outputSpacing, options.outputSpacing, 1.0);
    }
}

ObliqueResliceOptions ObliqueResliceRenderer::getOptions() const {
    return impl_->options;
}

void ObliqueResliceRenderer::setWindowLevel(double width, double center) {
    impl_->windowWidth = width;
    impl_->windowCenter = center;
    impl_->updateWindowLevel();
}

std::pair<double, double> ObliqueResliceRenderer::getWindowLevel() const {
    return {impl_->windowWidth, impl_->windowCenter};
}

void ObliqueResliceRenderer::update() {
    if (impl_->reslice) {
        impl_->reslice->Update();
    }
    if (impl_->colorMapper) {
        impl_->colorMapper->Update();
    }
    if (impl_->renderer) {
        impl_->renderer->Modified();
    }
}

void ObliqueResliceRenderer::resetView() {
    if (!impl_->inputData) {
        return;
    }

    double* center = impl_->inputData->GetCenter();
    impl_->planeDef.center = Point3D{center[0], center[1], center[2]};
    impl_->planeDef.rotationX = 0.0;
    impl_->planeDef.rotationY = 0.0;
    impl_->planeDef.rotationZ = 0.0;
    impl_->planeDef.sliceOffset = 0.0;

    impl_->updateResliceMatrix();
    impl_->updateWindowLevel();
    impl_->setupCamera();
    impl_->notifyPlaneChanged();
}

std::optional<Point3D> ObliqueResliceRenderer::screenToWorld(int screenX, int screenY) const {
    if (!impl_->renderer || !impl_->inputData) {
        return std::nullopt;
    }

    // Get the reslice matrix and use it to transform screen coords
    auto matrix = impl_->reslice->GetResliceAxes();
    if (!matrix) {
        return std::nullopt;
    }

    // Get renderer dimensions
    int* size = impl_->renderer->GetSize();
    if (size[0] <= 0 || size[1] <= 0) {
        return std::nullopt;
    }

    // Convert screen to normalized coordinates
    double normX = (2.0 * screenX / size[0]) - 1.0;
    double normY = (2.0 * screenY / size[1]) - 1.0;

    // Get camera parallel scale for coordinate mapping
    auto camera = impl_->renderer->GetActiveCamera();
    double scale = camera->GetParallelScale();

    // Map to world coordinates on the plane
    double planeX = normX * scale;
    double planeY = normY * scale;

    // Transform using reslice matrix
    double point[4] = {planeX, planeY, 0.0, 1.0};
    double result[4];
    matrix->MultiplyPoint(point, result);

    return Point3D{result[0], result[1], result[2]};
}

std::optional<std::array<int, 2>> ObliqueResliceRenderer::worldToScreen(
    const Point3D& world) const {

    if (!impl_->renderer) {
        return std::nullopt;
    }

    // Use VTK's world to display coordinate conversion
    double worldCoords[4] = {world.x, world.y, world.z, 1.0};
    double displayCoords[3];

    impl_->renderer->SetWorldPoint(worldCoords);
    impl_->renderer->WorldToDisplay();
    impl_->renderer->GetDisplayPoint(displayCoords);

    return std::array<int, 2>{
        static_cast<int>(displayCoords[0]),
        static_cast<int>(displayCoords[1])
    };
}

void ObliqueResliceRenderer::setPlaneChangedCallback(PlaneChangedCallback callback) {
    impl_->planeChangedCallback = std::move(callback);
}

void ObliqueResliceRenderer::setSliceChangedCallback(SliceChangedCallback callback) {
    impl_->sliceChangedCallback = std::move(callback);
}

} // namespace dicom_viewer::services

#include "services/render/hemodynamic_overlay_renderer.hpp"
#include "services/mpr_renderer.hpp"

#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageMapToColors.h>
#include <vtkLookupTable.h>
#include <vtkRenderer.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>

#include <algorithm>
#include <cmath>

namespace dicom_viewer::services {

// =============================================================================
// Colormap builders
// =============================================================================

namespace {

void buildJetColormap(vtkLookupTable* lut) {
    lut->SetNumberOfTableValues(256);
    lut->SetHueRange(0.667, 0.0);  // Blue to Red
    lut->SetSaturationRange(1.0, 1.0);
    lut->SetValueRange(1.0, 1.0);
    lut->Build();
}

void buildHotMetalColormap(vtkLookupTable* lut) {
    lut->SetNumberOfTableValues(256);
    for (int i = 0; i < 256; ++i) {
        double t = static_cast<double>(i) / 255.0;
        double r = std::clamp(t * 3.0, 0.0, 1.0);
        double g = std::clamp((t - 0.333) * 3.0, 0.0, 1.0);
        double b = std::clamp((t - 0.667) * 3.0, 0.0, 1.0);
        lut->SetTableValue(i, r, g, b, 1.0);
    }
    lut->Build();
}

void buildCoolWarmColormap(vtkLookupTable* lut) {
    lut->SetNumberOfTableValues(256);
    for (int i = 0; i < 256; ++i) {
        double t = static_cast<double>(i) / 255.0;
        // Cool (blue) to warm (red) through white at midpoint
        double r, g, b;
        if (t < 0.5) {
            double s = t * 2.0;
            r = s;
            g = s;
            b = 1.0;
        } else {
            double s = (t - 0.5) * 2.0;
            r = 1.0;
            g = 1.0 - s;
            b = 1.0 - s;
        }
        lut->SetTableValue(i, r, g, b, 1.0);
    }
    lut->Build();
}

void buildViridisColormap(vtkLookupTable* lut) {
    // Simplified Viridis: dark purple → blue → green → yellow
    lut->SetNumberOfTableValues(256);
    for (int i = 0; i < 256; ++i) {
        double t = static_cast<double>(i) / 255.0;
        double r = std::clamp(t * 1.5 - 0.25, 0.0, 1.0) * 0.9 + 0.1 * t;
        double g = std::clamp(t * 1.2 - 0.1, 0.0, 1.0) * 0.8;
        double b = std::clamp(0.6 - t * 0.8, 0.0, 0.7);
        // Approximate viridis character
        r = 0.267 + t * (0.993 - 0.267);
        g = 0.004 + t * (0.906 - 0.004);
        b = 0.329 + (t < 0.5 ? t * 0.6 : (1.0 - t) * 0.6);
        lut->SetTableValue(i, r, g, b, 1.0);
    }
    lut->Build();
}

vtkSmartPointer<vtkMatrix4x4> createResliceMatrix(MPRPlane plane, double position) {
    auto matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    matrix->Identity();

    switch (plane) {
        case MPRPlane::Axial:
            // XY plane, slice along Z
            matrix->SetElement(2, 3, position);
            break;
        case MPRPlane::Coronal:
            // XZ plane, slice along Y
            matrix->SetElement(1, 1, 0);
            matrix->SetElement(1, 2, 1);
            matrix->SetElement(2, 1, -1);
            matrix->SetElement(2, 2, 0);
            matrix->SetElement(1, 3, position);
            break;
        case MPRPlane::Sagittal:
            // YZ plane, slice along X
            matrix->SetElement(0, 0, 0);
            matrix->SetElement(0, 2, -1);
            matrix->SetElement(2, 0, 1);
            matrix->SetElement(2, 2, 0);
            matrix->SetElement(0, 3, position);
            break;
    }

    return matrix;
}

} // anonymous namespace

// =============================================================================
// Implementation
// =============================================================================

class HemodynamicOverlayRenderer::Impl {
public:
    // Input data
    vtkSmartPointer<vtkImageData> scalarField;

    // Overlay settings
    OverlayType overlayType = OverlayType::VelocityMagnitude;
    bool visible = true;
    double overlayOpacity = 0.5;

    // Colormap settings
    ColormapPreset colormapPreset = ColormapPreset::Jet;
    double scalarMin = 0.0;
    double scalarMax = 100.0;

    // VTK pipeline per plane (Axial=0, Coronal=1, Sagittal=2)
    std::array<vtkSmartPointer<vtkRenderer>, 3> renderers;
    std::array<vtkSmartPointer<vtkImageReslice>, 3> reslicers;
    std::array<vtkSmartPointer<vtkImageMapToColors>, 3> colorMappers;
    std::array<vtkSmartPointer<vtkImageActor>, 3> imageActors;
    std::array<double, 3> slicePositions = {0.0, 0.0, 0.0};

    // Shared lookup table
    vtkSmartPointer<vtkLookupTable> lookupTable;

    Impl() {
        lookupTable = vtkSmartPointer<vtkLookupTable>::New();
        rebuildColormap();

        for (int i = 0; i < 3; ++i) {
            reslicers[i] = vtkSmartPointer<vtkImageReslice>::New();
            reslicers[i]->SetOutputDimensionality(2);
            reslicers[i]->SetInterpolationModeToLinear();

            colorMappers[i] = vtkSmartPointer<vtkImageMapToColors>::New();
            colorMappers[i]->SetLookupTable(lookupTable);
            colorMappers[i]->SetInputConnection(reslicers[i]->GetOutputPort());

            imageActors[i] = vtkSmartPointer<vtkImageActor>::New();
            imageActors[i]->GetMapper()->SetInputConnection(
                colorMappers[i]->GetOutputPort());
            imageActors[i]->SetOpacity(overlayOpacity);
            imageActors[i]->SetVisibility(false);
        }

        setupResliceMatrices();
    }

    void setupResliceMatrices() {
        // Axial
        auto axialMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        axialMatrix->Identity();
        reslicers[0]->SetResliceAxes(axialMatrix);

        // Coronal
        auto coronalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        coronalMatrix->Identity();
        coronalMatrix->SetElement(1, 1, 0);
        coronalMatrix->SetElement(1, 2, 1);
        coronalMatrix->SetElement(2, 1, -1);
        coronalMatrix->SetElement(2, 2, 0);
        reslicers[1]->SetResliceAxes(coronalMatrix);

        // Sagittal
        auto sagittalMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        sagittalMatrix->Identity();
        sagittalMatrix->SetElement(0, 0, 0);
        sagittalMatrix->SetElement(0, 2, -1);
        sagittalMatrix->SetElement(2, 0, 1);
        sagittalMatrix->SetElement(2, 2, 0);
        reslicers[2]->SetResliceAxes(sagittalMatrix);
    }

    void rebuildColormap() {
        lookupTable->SetTableRange(scalarMin, scalarMax);

        switch (colormapPreset) {
            case ColormapPreset::Jet:
                buildJetColormap(lookupTable);
                break;
            case ColormapPreset::HotMetal:
                buildHotMetalColormap(lookupTable);
                break;
            case ColormapPreset::CoolWarm:
                buildCoolWarmColormap(lookupTable);
                break;
            case ColormapPreset::Viridis:
                buildViridisColormap(lookupTable);
                break;
        }

        lookupTable->SetTableRange(scalarMin, scalarMax);
    }

    void updateSlicePosition(int planeIndex) {
        if (!scalarField) {
            return;
        }

        auto matrix = reslicers[planeIndex]->GetResliceAxes();
        double position = slicePositions[planeIndex];

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

    void applyVisibility() {
        for (int i = 0; i < 3; ++i) {
            imageActors[i]->SetVisibility(visible && scalarField != nullptr);
        }
    }

    void attachToRenderers() {
        for (int i = 0; i < 3; ++i) {
            if (renderers[i]) {
                renderers[i]->AddViewProp(imageActors[i]);
            }
        }
    }

    void detachFromRenderers() {
        for (int i = 0; i < 3; ++i) {
            if (renderers[i]) {
                renderers[i]->RemoveViewProp(imageActors[i]);
            }
        }
    }
};

// =============================================================================
// Public API
// =============================================================================

HemodynamicOverlayRenderer::HemodynamicOverlayRenderer()
    : impl_(std::make_unique<Impl>()) {}

HemodynamicOverlayRenderer::~HemodynamicOverlayRenderer() {
    if (impl_) {
        impl_->detachFromRenderers();
    }
}

HemodynamicOverlayRenderer::HemodynamicOverlayRenderer(HemodynamicOverlayRenderer&&) noexcept = default;
HemodynamicOverlayRenderer& HemodynamicOverlayRenderer::operator=(HemodynamicOverlayRenderer&&) noexcept = default;

void HemodynamicOverlayRenderer::setScalarField(vtkSmartPointer<vtkImageData> scalarField) {
    impl_->scalarField = scalarField;

    if (scalarField) {
        for (int i = 0; i < 3; ++i) {
            impl_->reslicers[i]->SetInputData(scalarField);
        }
    }

    impl_->applyVisibility();
}

bool HemodynamicOverlayRenderer::hasScalarField() const noexcept {
    return impl_->scalarField != nullptr;
}

void HemodynamicOverlayRenderer::setOverlayType(OverlayType type) {
    impl_->overlayType = type;
    // Automatically apply default colormap for the new overlay type
    setColormapPreset(defaultColormapForType(type));
}

OverlayType HemodynamicOverlayRenderer::overlayType() const noexcept {
    return impl_->overlayType;
}

void HemodynamicOverlayRenderer::setVisible(bool visible) {
    impl_->visible = visible;
    impl_->applyVisibility();
}

bool HemodynamicOverlayRenderer::isVisible() const noexcept {
    return impl_->visible;
}

void HemodynamicOverlayRenderer::setOpacity(double opacity) {
    impl_->overlayOpacity = std::clamp(opacity, 0.0, 1.0);
    for (int i = 0; i < 3; ++i) {
        impl_->imageActors[i]->SetOpacity(impl_->overlayOpacity);
    }
}

double HemodynamicOverlayRenderer::opacity() const noexcept {
    return impl_->overlayOpacity;
}

void HemodynamicOverlayRenderer::setColormapPreset(ColormapPreset preset) {
    impl_->colormapPreset = preset;
    impl_->rebuildColormap();

    for (int i = 0; i < 3; ++i) {
        impl_->colorMappers[i]->Modified();
    }
}

ColormapPreset HemodynamicOverlayRenderer::colormapPreset() const noexcept {
    return impl_->colormapPreset;
}

void HemodynamicOverlayRenderer::setScalarRange(double minVal, double maxVal) {
    impl_->scalarMin = minVal;
    impl_->scalarMax = maxVal;
    impl_->rebuildColormap();

    for (int i = 0; i < 3; ++i) {
        impl_->colorMappers[i]->Modified();
    }
}

std::pair<double, double> HemodynamicOverlayRenderer::scalarRange() const noexcept {
    return {impl_->scalarMin, impl_->scalarMax};
}

vtkSmartPointer<vtkLookupTable> HemodynamicOverlayRenderer::getLookupTable() const {
    return impl_->lookupTable;
}

void HemodynamicOverlayRenderer::setRenderers(
    vtkSmartPointer<vtkRenderer> axial,
    vtkSmartPointer<vtkRenderer> coronal,
    vtkSmartPointer<vtkRenderer> sagittal) {

    impl_->detachFromRenderers();

    impl_->renderers[0] = axial;
    impl_->renderers[1] = coronal;
    impl_->renderers[2] = sagittal;

    impl_->attachToRenderers();
    impl_->applyVisibility();
}

std::expected<void, OverlayError>
HemodynamicOverlayRenderer::setSlicePosition(MPRPlane plane, double worldPosition) {
    if (!impl_->scalarField) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int planeIndex = static_cast<int>(plane);
    if (planeIndex < 0 || planeIndex > 2) {
        return std::unexpected(OverlayError::InvalidPlane);
    }

    impl_->slicePositions[planeIndex] = worldPosition;
    impl_->updateSlicePosition(planeIndex);
    return {};
}

void HemodynamicOverlayRenderer::update() {
    for (int i = 0; i < 3; ++i) {
        if (impl_->scalarField) {
            impl_->reslicers[i]->Update();
            impl_->colorMappers[i]->Update();
        }
    }
}

void HemodynamicOverlayRenderer::updatePlane(MPRPlane plane) {
    int idx = static_cast<int>(plane);
    if (idx >= 0 && idx <= 2 && impl_->scalarField) {
        impl_->reslicers[idx]->Update();
        impl_->colorMappers[idx]->Update();
    }
}

// =============================================================================
// Static utility methods
// =============================================================================

std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
HemodynamicOverlayRenderer::computeVelocityMagnitude(
    vtkSmartPointer<vtkImageData> velocityField) {

    if (!velocityField) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int numComponents = velocityField->GetNumberOfScalarComponents();
    if (numComponents < 3) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int* dims = velocityField->GetDimensions();
    double* spacing = velocityField->GetSpacing();
    double* origin = velocityField->GetOrigin();

    auto magnitude = vtkSmartPointer<vtkImageData>::New();
    magnitude->SetDimensions(dims);
    magnitude->SetSpacing(spacing);
    magnitude->SetOrigin(origin);
    magnitude->AllocateScalars(VTK_FLOAT, 1);

    vtkIdType numVoxels = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];

    auto* inData = velocityField->GetPointData()->GetScalars();
    auto* outPtr = static_cast<float*>(magnitude->GetScalarPointer());

    for (vtkIdType i = 0; i < numVoxels; ++i) {
        double vx = inData->GetComponent(i, 0);
        double vy = inData->GetComponent(i, 1);
        double vz = inData->GetComponent(i, 2);
        outPtr[i] = static_cast<float>(std::sqrt(vx * vx + vy * vy + vz * vz));
    }

    return magnitude;
}

std::expected<vtkSmartPointer<vtkImageData>, OverlayError>
HemodynamicOverlayRenderer::extractComponent(
    vtkSmartPointer<vtkImageData> velocityField, int component) {

    if (!velocityField) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int numComponents = velocityField->GetNumberOfScalarComponents();
    if (component < 0 || component >= numComponents) {
        return std::unexpected(OverlayError::NoScalarField);
    }

    int* dims = velocityField->GetDimensions();
    double* spacing = velocityField->GetSpacing();
    double* origin = velocityField->GetOrigin();

    auto result = vtkSmartPointer<vtkImageData>::New();
    result->SetDimensions(dims);
    result->SetSpacing(spacing);
    result->SetOrigin(origin);
    result->AllocateScalars(VTK_FLOAT, 1);

    vtkIdType numVoxels = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];

    auto* inData = velocityField->GetPointData()->GetScalars();
    auto* outPtr = static_cast<float*>(result->GetScalarPointer());

    for (vtkIdType i = 0; i < numVoxels; ++i) {
        outPtr[i] = static_cast<float>(inData->GetComponent(i, component));
    }

    return result;
}

ColormapPreset HemodynamicOverlayRenderer::defaultColormapForType(OverlayType type) noexcept {
    switch (type) {
        case OverlayType::VelocityMagnitude:
            return ColormapPreset::Jet;
        case OverlayType::VelocityX:
        case OverlayType::VelocityY:
        case OverlayType::VelocityZ:
        case OverlayType::Vorticity:
            return ColormapPreset::CoolWarm;
        case OverlayType::EnergyLoss:
            return ColormapPreset::HotMetal;
        case OverlayType::Streamline:
            return ColormapPreset::Jet;
        case OverlayType::VelocityTexture:
            return ColormapPreset::Viridis;
    }
    return ColormapPreset::Jet;
}

} // namespace dicom_viewer::services

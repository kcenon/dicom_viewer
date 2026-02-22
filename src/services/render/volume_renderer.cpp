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

#include "services/volume_renderer.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPlanes.h>
#include <vtkPlane.h>
#include <vtkPoints.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>

#include <format>
#include <map>

namespace dicom_viewer::services {

// =============================================================================
// Scalar overlay entry
// =============================================================================

struct ScalarOverlayEntry {
    std::string name;
    vtkSmartPointer<vtkVolume> volume;
    vtkSmartPointer<vtkVolumeProperty> property;
    vtkSmartPointer<vtkSmartVolumeMapper> mapper;
    vtkSmartPointer<vtkColorTransferFunction> colorTF;
    vtkSmartPointer<vtkPiecewiseFunction> opacityTF;
    bool visible = true;
};

class VolumeRenderer::Impl {
public:
    vtkSmartPointer<vtkVolume> volume;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> gpuMapper;
    vtkSmartPointer<vtkSmartVolumeMapper> smartMapper;
    vtkSmartPointer<vtkVolumeProperty> property;
    vtkSmartPointer<vtkColorTransferFunction> colorTF;
    vtkSmartPointer<vtkPiecewiseFunction> opacityTF;
    vtkSmartPointer<vtkPiecewiseFunction> gradientOpacityTF;
    vtkSmartPointer<vtkPlanes> clippingPlanes;

    vtkSmartPointer<vtkImageData> inputData;
    bool useGPU = true;
    bool useLOD = true;
    bool gpuValidated = false;

    // Scalar overlays (name → entry)
    std::map<std::string, ScalarOverlayEntry> overlays;

    Impl() {
        volume = vtkSmartPointer<vtkVolume>::New();
        gpuMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
        smartMapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
        property = vtkSmartPointer<vtkVolumeProperty>::New();
        colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
        opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
        gradientOpacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
        clippingPlanes = vtkSmartPointer<vtkPlanes>::New();

        // Setup property
        property->SetInterpolationTypeToLinear();
        property->ShadeOn();
        property->SetAmbient(0.1);
        property->SetDiffuse(0.9);
        property->SetSpecular(0.2);
        property->SetSpecularPower(10.0);

        // Configure GPU mapper for optimal performance
        gpuMapper->SetAutoAdjustSampleDistances(1);
        gpuMapper->SetSampleDistance(0.5);

        // Configure smart mapper as fallback
        smartMapper->SetRequestedRenderModeToRayCast();

        volume->SetProperty(property);
    }

    void updateMapper() {
        if (useGPU && gpuValidated) {
            if (inputData) {
                gpuMapper->SetInputData(inputData);
            }
            volume->SetMapper(gpuMapper);
        } else {
            if (inputData) {
                smartMapper->SetInputData(inputData);
            }
            volume->SetMapper(smartMapper);
        }
    }
};

VolumeRenderer::VolumeRenderer() : impl_(std::make_unique<Impl>()) {}
VolumeRenderer::~VolumeRenderer() = default;
VolumeRenderer::VolumeRenderer(VolumeRenderer&&) noexcept = default;
VolumeRenderer& VolumeRenderer::operator=(VolumeRenderer&&) noexcept = default;

void VolumeRenderer::setInputData(vtkSmartPointer<vtkImageData> imageData)
{
    impl_->inputData = imageData;
    if (imageData) {
        int* dims = imageData->GetDimensions();
        LOG_INFO(std::format("Volume data set: {}x{}x{}", dims[0], dims[1], dims[2]));
    }
    impl_->updateMapper();
}

vtkSmartPointer<vtkVolume> VolumeRenderer::getVolume() const
{
    return impl_->volume;
}

void VolumeRenderer::applyPreset(const TransferFunctionPreset& preset)
{
    LOG_INFO(std::format("Applying preset: {}", preset.name));

    impl_->colorTF->RemoveAllPoints();
    for (const auto& [value, r, g, b] : preset.colorPoints) {
        impl_->colorTF->AddRGBPoint(value, r, g, b);
    }

    impl_->opacityTF->RemoveAllPoints();
    for (const auto& [value, opacity] : preset.opacityPoints) {
        impl_->opacityTF->AddPoint(value, opacity);
    }

    if (!preset.gradientOpacityPoints.empty()) {
        impl_->gradientOpacityTF->RemoveAllPoints();
        for (const auto& [value, opacity] : preset.gradientOpacityPoints) {
            impl_->gradientOpacityTF->AddPoint(value, opacity);
        }
        impl_->property->SetGradientOpacity(impl_->gradientOpacityTF);
    }

    impl_->property->SetColor(impl_->colorTF);
    impl_->property->SetScalarOpacity(impl_->opacityTF);
}

void VolumeRenderer::setWindowLevel(double width, double center)
{
    double lower = center - width / 2.0;
    double upper = center + width / 2.0;

    impl_->opacityTF->RemoveAllPoints();
    impl_->opacityTF->AddPoint(lower - 1, 0.0);
    impl_->opacityTF->AddPoint(lower, 0.0);
    impl_->opacityTF->AddPoint(upper, 1.0);
    impl_->opacityTF->AddPoint(upper + 1, 1.0);
}

void VolumeRenderer::setBlendMode(BlendMode mode)
{
    int vtkMode = vtkVolumeMapper::COMPOSITE_BLEND;
    switch (mode) {
        case BlendMode::Composite:
            vtkMode = vtkVolumeMapper::COMPOSITE_BLEND;
            break;
        case BlendMode::MaximumIntensity:
            vtkMode = vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND;
            break;
        case BlendMode::MinimumIntensity:
            vtkMode = vtkVolumeMapper::MINIMUM_INTENSITY_BLEND;
            break;
        case BlendMode::Average:
            vtkMode = vtkVolumeMapper::AVERAGE_INTENSITY_BLEND;
            break;
    }

    impl_->gpuMapper->SetBlendMode(vtkMode);
    impl_->smartMapper->SetBlendMode(vtkMode);
}

bool VolumeRenderer::setGPURenderingEnabled(bool enable)
{
    impl_->useGPU = enable;
    impl_->updateMapper();
    return impl_->useGPU && impl_->gpuValidated;
}

bool VolumeRenderer::isGPURenderingEnabled() const
{
    return impl_->useGPU && impl_->gpuValidated;
}

bool VolumeRenderer::validateGPUSupport(vtkSmartPointer<vtkRenderWindow> renderWindow)
{
    if (!renderWindow) {
        LOG_WARNING("No render window provided for GPU validation");
        impl_->gpuValidated = false;
        impl_->updateMapper();
        return false;
    }

    bool gpuSupported = impl_->gpuMapper->IsRenderSupported(
        renderWindow, impl_->property);

    impl_->gpuValidated = gpuSupported;
    LOG_INFO(std::format("GPU rendering {}", gpuSupported ? "enabled" : "not supported, using CPU fallback"));
    impl_->updateMapper();
    return impl_->gpuValidated;
}

void VolumeRenderer::setInteractiveLODEnabled(bool enable)
{
    impl_->useLOD = enable;
    if (enable) {
        impl_->gpuMapper->SetAutoAdjustSampleDistances(1);
    } else {
        impl_->gpuMapper->SetAutoAdjustSampleDistances(0);
    }
}

void VolumeRenderer::setClippingPlanes(const std::array<double, 6>& planes)
{
    // Create 6 clipping planes for a bounding box
    // planes = [xmin, xmax, ymin, ymax, zmin, zmax]
    auto clippingPlanes = vtkSmartPointer<vtkPlanes>::New();

    // Define plane normals and points for box clipping
    // Each plane is defined by a point on the plane and its normal
    vtkNew<vtkPoints> points;
    vtkNew<vtkDoubleArray> normals;
    normals->SetNumberOfComponents(3);
    normals->SetNumberOfTuples(6);

    // X-min plane (normal pointing +X)
    points->InsertNextPoint(planes[0], 0, 0);
    normals->SetTuple3(0, 1, 0, 0);

    // X-max plane (normal pointing -X)
    points->InsertNextPoint(planes[1], 0, 0);
    normals->SetTuple3(1, -1, 0, 0);

    // Y-min plane (normal pointing +Y)
    points->InsertNextPoint(0, planes[2], 0);
    normals->SetTuple3(2, 0, 1, 0);

    // Y-max plane (normal pointing -Y)
    points->InsertNextPoint(0, planes[3], 0);
    normals->SetTuple3(3, 0, -1, 0);

    // Z-min plane (normal pointing +Z)
    points->InsertNextPoint(0, 0, planes[4]);
    normals->SetTuple3(4, 0, 0, 1);

    // Z-max plane (normal pointing -Z)
    points->InsertNextPoint(0, 0, planes[5]);
    normals->SetTuple3(5, 0, 0, -1);

    clippingPlanes->SetPoints(points);
    clippingPlanes->SetNormals(normals);

    impl_->clippingPlanes = clippingPlanes;
    impl_->gpuMapper->SetClippingPlanes(clippingPlanes);
    impl_->smartMapper->SetClippingPlanes(clippingPlanes);
}

void VolumeRenderer::clearClippingPlanes()
{
    impl_->gpuMapper->RemoveAllClippingPlanes();
    impl_->smartMapper->RemoveAllClippingPlanes();
}

void VolumeRenderer::update()
{
    impl_->volume->Modified();
}

// Preset definitions
TransferFunctionPreset VolumeRenderer::getPresetCTBone()
{
    return TransferFunctionPreset{
        .name = "CT Bone",
        .windowWidth = 2000,
        .windowCenter = 400,
        .colorPoints = {
            {-1000, 0.0, 0.0, 0.0},
            {200, 0.8, 0.6, 0.4},
            {500, 1.0, 1.0, 0.9},
            {3000, 1.0, 1.0, 1.0}
        },
        .opacityPoints = {
            {-1000, 0.0},
            {200, 0.0},
            {500, 0.5},
            {3000, 1.0}
        }
    };
}

TransferFunctionPreset VolumeRenderer::getPresetCTSoftTissue()
{
    return TransferFunctionPreset{
        .name = "CT Soft Tissue",
        .windowWidth = 400,
        .windowCenter = 40,
        .colorPoints = {
            {-160, 0.0, 0.0, 0.0},
            {40, 0.8, 0.6, 0.5},
            {240, 1.0, 0.9, 0.8}
        },
        .opacityPoints = {
            {-160, 0.0},
            {40, 0.3},
            {240, 0.8}
        }
    };
}

TransferFunctionPreset VolumeRenderer::getPresetCTLung()
{
    return TransferFunctionPreset{
        .name = "CT Lung",
        .windowWidth = 1500,
        .windowCenter = -600,
        .colorPoints = {
            {-1350, 0.0, 0.0, 0.0},
            {-600, 0.3, 0.3, 0.3},
            {150, 0.8, 0.8, 0.8}
        },
        .opacityPoints = {
            {-1350, 0.0},
            {-600, 0.1},
            {150, 0.5}
        }
    };
}

TransferFunctionPreset VolumeRenderer::getPresetCTAngio()
{
    return TransferFunctionPreset{
        .name = "CT Angio",
        .windowWidth = 400,
        .windowCenter = 200,
        .colorPoints = {
            {0, 0.0, 0.0, 0.0},
            {200, 0.8, 0.2, 0.1},
            {400, 1.0, 0.4, 0.3}
        },
        .opacityPoints = {
            {0, 0.0},
            {150, 0.0},
            {200, 0.5},
            {400, 1.0}
        }
    };
}

TransferFunctionPreset VolumeRenderer::getPresetCTAbdomen()
{
    return TransferFunctionPreset{
        .name = "CT Abdomen",
        .windowWidth = 400,
        .windowCenter = 50,
        .colorPoints = {
            {-150, 0.0, 0.0, 0.0},
            {50, 0.7, 0.5, 0.4},
            {250, 0.9, 0.8, 0.7}
        },
        .opacityPoints = {
            {-150, 0.0},
            {50, 0.3},
            {250, 0.7}
        }
    };
}

TransferFunctionPreset VolumeRenderer::getPresetMRIDefault()
{
    return TransferFunctionPreset{
        .name = "MRI Default",
        .windowWidth = 0, // Auto
        .windowCenter = 0, // Auto
        .colorPoints = {
            {0, 0.0, 0.0, 0.0},
            {500, 0.5, 0.5, 0.5},
            {1000, 1.0, 1.0, 1.0}
        },
        .opacityPoints = {
            {0, 0.0},
            {500, 0.3},
            {1000, 0.8}
        }
    };
}

// =============================================================================
// Scalar Overlay Implementation
// =============================================================================

void VolumeRenderer::addScalarOverlay(
    const std::string& name,
    vtkSmartPointer<vtkImageData> scalarField,
    vtkSmartPointer<vtkColorTransferFunction> colorTF,
    vtkSmartPointer<vtkPiecewiseFunction> opacityTF)
{
    // Remove existing overlay with same name
    removeScalarOverlay(name);

    ScalarOverlayEntry entry;
    entry.name = name;
    entry.colorTF = colorTF;
    entry.opacityTF = opacityTF;

    entry.property = vtkSmartPointer<vtkVolumeProperty>::New();
    entry.property->SetInterpolationTypeToLinear();
    entry.property->ShadeOff();  // No shading for scalar overlays
    entry.property->SetColor(colorTF);
    entry.property->SetScalarOpacity(opacityTF);

    entry.mapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
    entry.mapper->SetInputData(scalarField);
    entry.mapper->SetRequestedRenderModeToRayCast();

    entry.volume = vtkSmartPointer<vtkVolume>::New();
    entry.volume->SetMapper(entry.mapper);
    entry.volume->SetProperty(entry.property);

    impl_->overlays[name] = std::move(entry);
    LOG_INFO(std::format("Added scalar overlay: {}", name));
}

bool VolumeRenderer::removeScalarOverlay(const std::string& name)
{
    auto it = impl_->overlays.find(name);
    if (it == impl_->overlays.end()) {
        return false;
    }

    impl_->overlays.erase(it);
    LOG_INFO(std::format("Removed scalar overlay: {}", name));
    return true;
}

void VolumeRenderer::removeAllScalarOverlays()
{
    impl_->overlays.clear();
}

bool VolumeRenderer::hasOverlay(const std::string& name) const
{
    return impl_->overlays.contains(name);
}

std::vector<std::string> VolumeRenderer::overlayNames() const
{
    std::vector<std::string> names;
    names.reserve(impl_->overlays.size());
    for (const auto& [name, _] : impl_->overlays) {
        names.push_back(name);
    }
    return names;
}

void VolumeRenderer::setOverlayVisible(const std::string& name, bool visible)
{
    auto it = impl_->overlays.find(name);
    if (it != impl_->overlays.end()) {
        it->second.visible = visible;
        it->second.volume->SetVisibility(visible);
    }
}

void VolumeRenderer::setOverlayOpacity(const std::string& name, double opacity)
{
    auto it = impl_->overlays.find(name);
    if (it != impl_->overlays.end()) {
        // Scale the opacity transfer function by the global factor
        it->second.volume->GetProperty()->SetScalarOpacityUnitDistance(
            1.0 / std::max(opacity, 0.01));
    }
}

vtkSmartPointer<vtkVolume> VolumeRenderer::getOverlayVolume(const std::string& name) const
{
    auto it = impl_->overlays.find(name);
    if (it != impl_->overlays.end()) {
        return it->second.volume;
    }
    return nullptr;
}

bool VolumeRenderer::updateOverlayTransferFunctions(
    const std::string& name,
    vtkSmartPointer<vtkColorTransferFunction> colorTF,
    vtkSmartPointer<vtkPiecewiseFunction> opacityTF)
{
    auto it = impl_->overlays.find(name);
    if (it == impl_->overlays.end()) {
        return false;
    }

    it->second.colorTF = colorTF;
    it->second.opacityTF = opacityTF;
    it->second.property->SetColor(colorTF);
    it->second.property->SetScalarOpacity(opacityTF);
    it->second.volume->Modified();
    return true;
}

vtkSmartPointer<vtkColorTransferFunction>
VolumeRenderer::createVelocityColorFunction(double maxVelocity)
{
    auto colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    // Jet colormap: blue → cyan → green → yellow → red
    colorTF->AddRGBPoint(0.0, 0.0, 0.0, 0.5);                      // Dark blue
    colorTF->AddRGBPoint(maxVelocity * 0.25, 0.0, 0.0, 1.0);       // Blue
    colorTF->AddRGBPoint(maxVelocity * 0.375, 0.0, 1.0, 1.0);      // Cyan
    colorTF->AddRGBPoint(maxVelocity * 0.5, 0.0, 1.0, 0.0);        // Green
    colorTF->AddRGBPoint(maxVelocity * 0.625, 1.0, 1.0, 0.0);      // Yellow
    colorTF->AddRGBPoint(maxVelocity * 0.75, 1.0, 0.5, 0.0);       // Orange
    colorTF->AddRGBPoint(maxVelocity, 1.0, 0.0, 0.0);              // Red
    return colorTF;
}

vtkSmartPointer<vtkPiecewiseFunction>
VolumeRenderer::createVelocityOpacityFunction(double maxVelocity, double baseOpacity)
{
    auto opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
    // Low velocity = transparent, increasing opacity for higher velocity
    opacityTF->AddPoint(0.0, 0.0);
    opacityTF->AddPoint(maxVelocity * 0.1, 0.0);                    // Below 10% → invisible
    opacityTF->AddPoint(maxVelocity * 0.2, baseOpacity * 0.3);      // Fade in
    opacityTF->AddPoint(maxVelocity * 0.5, baseOpacity * 0.6);      // Mid range
    opacityTF->AddPoint(maxVelocity, baseOpacity);                   // Full opacity at max
    return opacityTF;
}

vtkSmartPointer<vtkColorTransferFunction>
VolumeRenderer::createVorticityColorFunction(double maxVorticity)
{
    auto colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    // Blue-white-red colormap for vorticity magnitude
    colorTF->AddRGBPoint(0.0, 0.0, 0.0, 0.5);                       // Dark blue
    colorTF->AddRGBPoint(maxVorticity * 0.15, 0.0, 0.0, 1.0);       // Blue
    colorTF->AddRGBPoint(maxVorticity * 0.35, 0.5, 0.5, 1.0);       // Light blue
    colorTF->AddRGBPoint(maxVorticity * 0.5, 1.0, 1.0, 1.0);        // White
    colorTF->AddRGBPoint(maxVorticity * 0.65, 1.0, 0.5, 0.5);       // Light red
    colorTF->AddRGBPoint(maxVorticity * 0.85, 1.0, 0.0, 0.0);       // Red
    colorTF->AddRGBPoint(maxVorticity, 0.5, 0.0, 0.0);              // Dark red
    return colorTF;
}

vtkSmartPointer<vtkPiecewiseFunction>
VolumeRenderer::createVorticityOpacityFunction(double maxVorticity, double baseOpacity)
{
    auto opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
    // Low vorticity = transparent, high vorticity = visible
    opacityTF->AddPoint(0.0, 0.0);
    opacityTF->AddPoint(maxVorticity * 0.1, 0.0);                    // Below 10% → invisible
    opacityTF->AddPoint(maxVorticity * 0.2, baseOpacity * 0.2);      // Fade in
    opacityTF->AddPoint(maxVorticity * 0.5, baseOpacity * 0.5);      // Mid range
    opacityTF->AddPoint(maxVorticity, baseOpacity);                   // Full opacity at max
    return opacityTF;
}

vtkSmartPointer<vtkColorTransferFunction>
VolumeRenderer::createEnergyLossColorFunction(double maxEnergyLoss)
{
    auto colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    // Hot metal colormap: black → red → yellow → white
    colorTF->AddRGBPoint(0.0, 0.0, 0.0, 0.0);                       // Black
    colorTF->AddRGBPoint(maxEnergyLoss * 0.25, 0.5, 0.0, 0.0);      // Dark red
    colorTF->AddRGBPoint(maxEnergyLoss * 0.5, 1.0, 0.0, 0.0);       // Red
    colorTF->AddRGBPoint(maxEnergyLoss * 0.75, 1.0, 0.75, 0.0);     // Orange-yellow
    colorTF->AddRGBPoint(maxEnergyLoss, 1.0, 1.0, 0.8);             // Near white
    return colorTF;
}

vtkSmartPointer<vtkPiecewiseFunction>
VolumeRenderer::createEnergyLossOpacityFunction(double maxEnergyLoss, double baseOpacity)
{
    auto opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
    // Low energy loss = transparent, high energy loss = visible
    opacityTF->AddPoint(0.0, 0.0);
    opacityTF->AddPoint(maxEnergyLoss * 0.05, 0.0);                  // Below 5% → invisible
    opacityTF->AddPoint(maxEnergyLoss * 0.15, baseOpacity * 0.2);    // Fade in
    opacityTF->AddPoint(maxEnergyLoss * 0.5, baseOpacity * 0.6);     // Mid range
    opacityTF->AddPoint(maxEnergyLoss, baseOpacity);                  // Full opacity at max
    return opacityTF;
}

} // namespace dicom_viewer::services

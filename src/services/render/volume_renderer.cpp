#include "services/volume_renderer.hpp"

#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>

namespace dicom_viewer::services {

class VolumeRenderer::Impl {
public:
    vtkSmartPointer<vtkVolume> volume;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> gpuMapper;
    vtkSmartPointer<vtkSmartVolumeMapper> smartMapper;
    vtkSmartPointer<vtkVolumeProperty> property;
    vtkSmartPointer<vtkColorTransferFunction> colorTF;
    vtkSmartPointer<vtkPiecewiseFunction> opacityTF;
    vtkSmartPointer<vtkPiecewiseFunction> gradientOpacityTF;

    bool useGPU = true;
    bool useLOD = true;

    Impl() {
        volume = vtkSmartPointer<vtkVolume>::New();
        gpuMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
        smartMapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
        property = vtkSmartPointer<vtkVolumeProperty>::New();
        colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
        opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
        gradientOpacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();

        // Setup property
        property->SetInterpolationTypeToLinear();
        property->ShadeOn();
        property->SetAmbient(0.1);
        property->SetDiffuse(0.9);
        property->SetSpecular(0.2);
        property->SetSpecularPower(10.0);

        volume->SetProperty(property);
    }
};

VolumeRenderer::VolumeRenderer() : impl_(std::make_unique<Impl>()) {}
VolumeRenderer::~VolumeRenderer() = default;
VolumeRenderer::VolumeRenderer(VolumeRenderer&&) noexcept = default;
VolumeRenderer& VolumeRenderer::operator=(VolumeRenderer&&) noexcept = default;

void VolumeRenderer::setInputData(vtkSmartPointer<vtkImageData> imageData)
{
    if (impl_->useGPU) {
        impl_->gpuMapper->SetInputData(imageData);
        impl_->volume->SetMapper(impl_->gpuMapper);
    } else {
        impl_->smartMapper->SetInputData(imageData);
        impl_->volume->SetMapper(impl_->smartMapper);
    }
}

vtkSmartPointer<vtkVolume> VolumeRenderer::getVolume() const
{
    return impl_->volume;
}

void VolumeRenderer::applyPreset(const TransferFunctionPreset& preset)
{
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
    return enable;
}

bool VolumeRenderer::isGPURenderingEnabled() const
{
    return impl_->useGPU;
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
    // TODO: Implement clipping with vtkPlanes
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

} // namespace dicom_viewer::services

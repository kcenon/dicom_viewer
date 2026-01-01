#include "services/volume_renderer.hpp"

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
    vtkSmartPointer<vtkPlanes> clippingPlanes;

    vtkSmartPointer<vtkImageData> inputData;
    bool useGPU = true;
    bool useLOD = true;
    bool gpuValidated = false;

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
    impl_->updateMapper();
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
        impl_->gpuValidated = false;
        impl_->updateMapper();
        return false;
    }

    // Check if GPU volume ray casting is supported
    bool gpuSupported = impl_->gpuMapper->IsRenderSupported(
        renderWindow, impl_->property);

    impl_->gpuValidated = gpuSupported;
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

} // namespace dicom_viewer::services

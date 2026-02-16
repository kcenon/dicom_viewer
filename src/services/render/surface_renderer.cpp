#include "services/surface_renderer.hpp"
#include "core/logging.hpp"

#include <vtkMarchingCubes.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkDecimatePro.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkMassProperties.h>
#include <vtkTriangleFilter.h>
#include <vtkLookupTable.h>
#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkNew.h>

#include <algorithm>
#include <stdexcept>

namespace dicom_viewer::services {

struct SurfaceEntry {
    SurfaceConfig config;
    vtkSmartPointer<vtkActor> actor;
    vtkSmartPointer<vtkMarchingCubes> marchingCubes;
    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother;
    vtkSmartPointer<vtkDecimatePro> decimator;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
    size_t triangleCount = 0;
    double surfaceArea = 0.0;
    double volume = 0.0;
    bool needsUpdate = true;
    bool isScalarSurface = false;
    std::string activeScalarArray;
};

class SurfaceRenderer::Impl {
public:
    vtkSmartPointer<vtkImageData> inputData;
    std::vector<SurfaceEntry> surfaces;
    SurfaceQuality quality = SurfaceQuality::Medium;
    std::shared_ptr<spdlog::logger> logger;

    Impl() : logger(logging::LoggerFactory::create("SurfaceRenderer")) {}

    SurfaceEntry createSurfaceEntry(const SurfaceConfig& config) {
        SurfaceEntry entry;
        entry.config = config;

        entry.marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
        entry.marchingCubes->SetValue(0, config.isovalue);
        entry.marchingCubes->ComputeNormalsOn();
        entry.marchingCubes->ComputeGradientsOff();
        entry.marchingCubes->ComputeScalarsOff();

        entry.smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
        entry.smoother->SetInputConnection(entry.marchingCubes->GetOutputPort());
        entry.smoother->SetNumberOfIterations(config.smoothingIterations);
        entry.smoother->SetPassBand(config.smoothingPassBand);
        entry.smoother->BoundarySmoothingOff();
        entry.smoother->FeatureEdgeSmoothingOff();
        entry.smoother->NonManifoldSmoothingOn();
        entry.smoother->NormalizeCoordinatesOn();

        entry.decimator = vtkSmartPointer<vtkDecimatePro>::New();
        entry.decimator->SetTargetReduction(config.decimationReduction);
        entry.decimator->PreserveTopologyOn();
        entry.decimator->SetFeatureAngle(60.0);
        entry.decimator->SplittingOff();
        entry.decimator->BoundaryVertexDeletionOff();

        entry.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        entry.mapper->ScalarVisibilityOff();

        entry.actor = vtkSmartPointer<vtkActor>::New();
        entry.actor->SetMapper(entry.mapper);

        auto property = entry.actor->GetProperty();
        property->SetColor(config.color[0], config.color[1], config.color[2]);
        property->SetOpacity(config.opacity);
        property->SetInterpolationToPhong();
        property->SetAmbient(0.1);
        property->SetDiffuse(0.7);
        property->SetSpecular(0.3);
        property->SetSpecularPower(20.0);

        entry.actor->SetVisibility(config.visible ? 1 : 0);

        return entry;
    }

    void updatePipeline(SurfaceEntry& entry) {
        if (!inputData) {
            return;
        }

        entry.marchingCubes->SetInputData(inputData);
        entry.marchingCubes->SetValue(0, entry.config.isovalue);

        vtkAlgorithmOutput* currentOutput = entry.marchingCubes->GetOutputPort();

        if (entry.config.smoothingEnabled) {
            entry.smoother->SetInputConnection(currentOutput);
            entry.smoother->SetNumberOfIterations(entry.config.smoothingIterations);
            entry.smoother->SetPassBand(entry.config.smoothingPassBand);
            currentOutput = entry.smoother->GetOutputPort();
        }

        if (entry.config.decimationEnabled) {
            entry.decimator->SetInputConnection(currentOutput);
            entry.decimator->SetTargetReduction(entry.config.decimationReduction);
            currentOutput = entry.decimator->GetOutputPort();
        }

        entry.mapper->SetInputConnection(currentOutput);
        entry.needsUpdate = false;
    }

    void computeStatistics(SurfaceEntry& entry) {
        if (!entry.mapper->GetInput()) {
            entry.mapper->Update();
        }

        auto polyData = entry.mapper->GetInput();
        if (polyData) {
            entry.triangleCount = polyData->GetNumberOfPolys();

            vtkNew<vtkTriangleFilter> triangleFilter;
            triangleFilter->SetInputData(polyData);
            triangleFilter->Update();

            vtkNew<vtkMassProperties> massProperties;
            massProperties->SetInputData(triangleFilter->GetOutput());
            massProperties->Update();

            entry.surfaceArea = massProperties->GetSurfaceArea();
            entry.volume = massProperties->GetVolume();
        }
    }

    void applyQualitySettings() {
        int iterations;
        double passband;
        double reduction;

        switch (quality) {
            case SurfaceQuality::Low:
                iterations = 10;
                passband = 0.1;
                reduction = 0.7;
                break;
            case SurfaceQuality::High:
                iterations = 40;
                passband = 0.001;
                reduction = 0.3;
                break;
            case SurfaceQuality::Medium:
            default:
                iterations = 20;
                passband = 0.01;
                reduction = 0.5;
                break;
        }

        for (auto& entry : surfaces) {
            if (entry.config.smoothingEnabled) {
                entry.smoother->SetNumberOfIterations(iterations);
                entry.smoother->SetPassBand(passband);
            }
            if (entry.config.decimationEnabled) {
                entry.decimator->SetTargetReduction(reduction);
            }
            entry.needsUpdate = true;
        }
    }
};

SurfaceRenderer::SurfaceRenderer() : impl_(std::make_unique<Impl>()) {}
SurfaceRenderer::~SurfaceRenderer() = default;
SurfaceRenderer::SurfaceRenderer(SurfaceRenderer&&) noexcept = default;
SurfaceRenderer& SurfaceRenderer::operator=(SurfaceRenderer&&) noexcept = default;

void SurfaceRenderer::setInputData(vtkSmartPointer<vtkImageData> imageData)
{
    impl_->inputData = imageData;
    if (imageData) {
        int* dims = imageData->GetDimensions();
        impl_->logger->info("Surface renderer input: {}x{}x{}", dims[0], dims[1], dims[2]);
    }
    for (auto& entry : impl_->surfaces) {
        entry.needsUpdate = true;
    }
}

size_t SurfaceRenderer::addSurface(const SurfaceConfig& config)
{
    impl_->logger->info("Adding surface '{}' with isovalue: {}", config.name, config.isovalue);
    auto entry = impl_->createSurfaceEntry(config);
    entry.needsUpdate = true;
    impl_->surfaces.push_back(std::move(entry));
    return impl_->surfaces.size() - 1;
}

size_t SurfaceRenderer::addPresetSurface(TissueType tissue)
{
    SurfaceConfig config;
    switch (tissue) {
        case TissueType::Bone:
            config = getPresetBone();
            break;
        case TissueType::SoftTissue:
            config = getPresetSoftTissue();
            break;
        case TissueType::Skin:
            config = getPresetSkin();
            break;
        case TissueType::Custom:
        default:
            config.name = "Custom";
            config.isovalue = 0.0;
            break;
    }
    return addSurface(config);
}

void SurfaceRenderer::removeSurface(size_t index)
{
    if (index >= impl_->surfaces.size()) {
        return;
    }
    impl_->surfaces.erase(impl_->surfaces.begin() + static_cast<ptrdiff_t>(index));
}

void SurfaceRenderer::clearSurfaces()
{
    impl_->surfaces.clear();
}

size_t SurfaceRenderer::getSurfaceCount() const
{
    return impl_->surfaces.size();
}

SurfaceConfig SurfaceRenderer::getSurfaceConfig(size_t index) const
{
    if (index >= impl_->surfaces.size()) {
        throw std::out_of_range("Surface index out of range");
    }
    return impl_->surfaces[index].config;
}

void SurfaceRenderer::updateSurface(size_t index, const SurfaceConfig& config)
{
    if (index >= impl_->surfaces.size()) {
        return;
    }

    auto& entry = impl_->surfaces[index];
    entry.config = config;
    entry.marchingCubes->SetValue(0, config.isovalue);

    auto property = entry.actor->GetProperty();
    property->SetColor(config.color[0], config.color[1], config.color[2]);
    property->SetOpacity(config.opacity);
    entry.actor->SetVisibility(config.visible ? 1 : 0);

    entry.needsUpdate = true;
}

void SurfaceRenderer::setSurfaceVisibility(size_t index, bool visible)
{
    if (index >= impl_->surfaces.size()) {
        return;
    }
    impl_->surfaces[index].config.visible = visible;
    impl_->surfaces[index].actor->SetVisibility(visible ? 1 : 0);
}

void SurfaceRenderer::setSurfaceColor(size_t index, double r, double g, double b)
{
    if (index >= impl_->surfaces.size()) {
        return;
    }
    impl_->surfaces[index].config.color = {r, g, b};
    impl_->surfaces[index].actor->GetProperty()->SetColor(r, g, b);
}

void SurfaceRenderer::setSurfaceOpacity(size_t index, double opacity)
{
    if (index >= impl_->surfaces.size()) {
        return;
    }
    impl_->surfaces[index].config.opacity = opacity;
    impl_->surfaces[index].actor->GetProperty()->SetOpacity(opacity);
}

void SurfaceRenderer::setSurfaceQuality(SurfaceQuality quality)
{
    impl_->quality = quality;
    impl_->applyQualitySettings();
}

vtkSmartPointer<vtkActor> SurfaceRenderer::getActor(size_t index) const
{
    if (index >= impl_->surfaces.size()) {
        return nullptr;
    }
    return impl_->surfaces[index].actor;
}

std::vector<vtkSmartPointer<vtkActor>> SurfaceRenderer::getAllActors() const
{
    std::vector<vtkSmartPointer<vtkActor>> actors;
    actors.reserve(impl_->surfaces.size());
    for (const auto& entry : impl_->surfaces) {
        actors.push_back(entry.actor);
    }
    return actors;
}

void SurfaceRenderer::addToRenderer(vtkSmartPointer<vtkRenderer> renderer)
{
    if (!renderer) {
        return;
    }
    for (const auto& entry : impl_->surfaces) {
        renderer->AddActor(entry.actor);
    }
}

void SurfaceRenderer::removeFromRenderer(vtkSmartPointer<vtkRenderer> renderer)
{
    if (!renderer) {
        return;
    }
    for (const auto& entry : impl_->surfaces) {
        renderer->RemoveActor(entry.actor);
    }
}

SurfaceData SurfaceRenderer::getSurfaceData(size_t index) const
{
    SurfaceData data;
    if (index >= impl_->surfaces.size()) {
        return data;
    }

    const auto& entry = impl_->surfaces[index];
    data.name = entry.config.name;
    data.actor = entry.actor;
    data.triangleCount = entry.triangleCount;
    data.surfaceArea = entry.surfaceArea;
    data.volume = entry.volume;

    return data;
}

void SurfaceRenderer::extractSurfaces()
{
    for (auto& entry : impl_->surfaces) {
        if (entry.needsUpdate) {
            impl_->updatePipeline(entry);
            entry.mapper->Update();
            impl_->computeStatistics(entry);
        }
    }
}

void SurfaceRenderer::update()
{
    extractSurfaces();
    for (auto& entry : impl_->surfaces) {
        entry.actor->Modified();
    }
}

// Preset definitions
SurfaceConfig SurfaceRenderer::getPresetBone()
{
    return SurfaceConfig{
        .name = "Bone",
        .isovalue = 300.0,
        .color = {0.9, 0.85, 0.75},  // Light bone color
        .opacity = 1.0,
        .smoothingEnabled = true,
        .smoothingIterations = 20,
        .smoothingPassBand = 0.01,
        .decimationEnabled = true,
        .decimationReduction = 0.5,
        .visible = true
    };
}

SurfaceConfig SurfaceRenderer::getPresetBoneHighDensity()
{
    return SurfaceConfig{
        .name = "Bone (High Density)",
        .isovalue = 500.0,
        .color = {1.0, 0.95, 0.85},  // Lighter for dense bone
        .opacity = 1.0,
        .smoothingEnabled = true,
        .smoothingIterations = 15,
        .smoothingPassBand = 0.01,
        .decimationEnabled = true,
        .decimationReduction = 0.4,
        .visible = true
    };
}

SurfaceConfig SurfaceRenderer::getPresetSoftTissue()
{
    return SurfaceConfig{
        .name = "Soft Tissue",
        .isovalue = 60.0,
        .color = {0.85, 0.6, 0.5},  // Tissue pink
        .opacity = 0.7,
        .smoothingEnabled = true,
        .smoothingIterations = 25,
        .smoothingPassBand = 0.005,
        .decimationEnabled = true,
        .decimationReduction = 0.6,
        .visible = true
    };
}

SurfaceConfig SurfaceRenderer::getPresetSkin()
{
    return SurfaceConfig{
        .name = "Skin",
        .isovalue = -50.0,
        .color = {0.95, 0.82, 0.72},  // Skin tone
        .opacity = 0.5,
        .smoothingEnabled = true,
        .smoothingIterations = 30,
        .smoothingPassBand = 0.001,
        .decimationEnabled = true,
        .decimationReduction = 0.7,
        .visible = true
    };
}

SurfaceConfig SurfaceRenderer::getPresetLung()
{
    return SurfaceConfig{
        .name = "Lung",
        .isovalue = -500.0,
        .color = {0.7, 0.8, 0.9},  // Light blue
        .opacity = 0.4,
        .smoothingEnabled = true,
        .smoothingIterations = 20,
        .smoothingPassBand = 0.01,
        .decimationEnabled = true,
        .decimationReduction = 0.6,
        .visible = true
    };
}

SurfaceConfig SurfaceRenderer::getPresetBloodVessels()
{
    return SurfaceConfig{
        .name = "Blood Vessels",
        .isovalue = 200.0,
        .color = {0.8, 0.2, 0.2},  // Red
        .opacity = 0.9,
        .smoothingEnabled = true,
        .smoothingIterations = 15,
        .smoothingPassBand = 0.02,
        .decimationEnabled = true,
        .decimationReduction = 0.4,
        .visible = true
    };
}

// =============================================================================
// Per-Vertex Scalar Coloring
// =============================================================================

size_t SurfaceRenderer::addScalarSurface(
    const std::string& name,
    vtkSmartPointer<vtkPolyData> surface,
    const std::string& activeArrayName)
{
    impl_->logger->info("Adding scalar surface '{}' with array '{}'",
                        name, activeArrayName);

    SurfaceEntry entry;
    entry.config.name = name;
    entry.config.visible = true;
    entry.config.opacity = 1.0;
    entry.isScalarSurface = true;
    entry.activeScalarArray = activeArrayName;

    entry.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    entry.mapper->SetInputData(surface);
    entry.mapper->ScalarVisibilityOn();
    entry.mapper->SetScalarModeToUsePointData();
    entry.mapper->SetColorModeToMapScalars();

    if (auto* arr = surface->GetPointData()->GetArray(activeArrayName.c_str())) {
        surface->GetPointData()->SetActiveScalars(activeArrayName.c_str());
        double range[2];
        arr->GetRange(range);
        entry.mapper->SetScalarRange(range[0], range[1]);
    }

    entry.actor = vtkSmartPointer<vtkActor>::New();
    entry.actor->SetMapper(entry.mapper);

    auto* prop = entry.actor->GetProperty();
    prop->SetInterpolationToPhong();
    prop->SetAmbient(0.1);
    prop->SetDiffuse(0.7);
    prop->SetSpecular(0.3);
    prop->SetSpecularPower(20.0);

    entry.triangleCount = surface->GetNumberOfPolys();
    entry.needsUpdate = false;

    impl_->surfaces.push_back(std::move(entry));
    return impl_->surfaces.size() - 1;
}

void SurfaceRenderer::setSurfaceScalarRange(size_t index, double minVal, double maxVal)
{
    if (index >= impl_->surfaces.size()) {
        return;
    }
    impl_->surfaces[index].mapper->SetScalarRange(minVal, maxVal);
}

std::pair<double, double> SurfaceRenderer::surfaceScalarRange(size_t index) const
{
    if (index >= impl_->surfaces.size()) {
        return {0.0, 0.0};
    }
    const double* range = impl_->surfaces[index].mapper->GetScalarRange();
    return {range[0], range[1]};
}

void SurfaceRenderer::setSurfaceLookupTable(size_t index,
                                            vtkSmartPointer<vtkLookupTable> lut)
{
    if (index >= impl_->surfaces.size()) {
        return;
    }
    impl_->surfaces[index].mapper->SetLookupTable(lut);
}

// =============================================================================
// Hemodynamic Colormap Factories
// =============================================================================

vtkSmartPointer<vtkLookupTable> SurfaceRenderer::createWSSLookupTable(double maxWSS)
{
    auto lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetNumberOfTableValues(256);
    lut->SetRange(0.0, maxWSS);
    lut->SetHueRange(0.667, 0.0);   // Blue (0.667) → Red (0.0)
    lut->SetSaturationRange(1.0, 1.0);
    lut->SetValueRange(0.8, 1.0);
    lut->Build();
    return lut;
}

vtkSmartPointer<vtkLookupTable> SurfaceRenderer::createOSILookupTable()
{
    auto lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetNumberOfTableValues(256);
    lut->SetRange(0.0, 0.5);

    lut->SetNumberOfTableValues(256);
    lut->Build();

    // Manually set diverging colormap: blue → white → red
    for (int i = 0; i < 256; ++i) {
        double t = static_cast<double>(i) / 255.0;
        double r, g, b;
        if (t < 0.5) {
            // Blue → White
            double s = t / 0.5;
            r = s;
            g = s;
            b = 1.0;
        } else {
            // White → Red
            double s = (t - 0.5) / 0.5;
            r = 1.0;
            g = 1.0 - s;
            b = 1.0 - s;
        }
        lut->SetTableValue(i, r, g, b, 1.0);
    }

    return lut;
}

vtkSmartPointer<vtkLookupTable> SurfaceRenderer::createRRTLookupTable(double maxRRT)
{
    auto lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetNumberOfTableValues(256);
    lut->SetRange(0.0, maxRRT);

    lut->Build();

    // Sequential colormap: light yellow → orange → dark red
    for (int i = 0; i < 256; ++i) {
        double t = static_cast<double>(i) / 255.0;
        double r = 1.0;
        double g = 1.0 - 0.8 * t;              // 1.0 → 0.2
        double b = 0.6 * (1.0 - t) * (1.0 - t); // 0.6 → 0.0 (quadratic)
        lut->SetTableValue(i, r, g, b, 1.0);
    }

    return lut;
}

} // namespace dicom_viewer::services

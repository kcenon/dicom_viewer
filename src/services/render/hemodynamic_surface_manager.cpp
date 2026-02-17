#include "services/hemodynamic_surface_manager.hpp"

#include "services/surface_renderer.hpp"

#include <vtkFloatArray.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>

#include <optional>

namespace dicom_viewer::services {

class HemodynamicSurfaceManager::Impl {
public:
    std::optional<size_t> wssIdx;
    std::optional<size_t> osiIdx;
    std::optional<size_t> afiIdx;
    std::optional<size_t> rrtIdx;
};

HemodynamicSurfaceManager::HemodynamicSurfaceManager()
    : impl_(std::make_unique<Impl>())
{}

HemodynamicSurfaceManager::~HemodynamicSurfaceManager() = default;
HemodynamicSurfaceManager::HemodynamicSurfaceManager(HemodynamicSurfaceManager&&) noexcept = default;
HemodynamicSurfaceManager& HemodynamicSurfaceManager::operator=(HemodynamicSurfaceManager&&) noexcept = default;

size_t HemodynamicSurfaceManager::showWSS(SurfaceRenderer& renderer,
                                           vtkSmartPointer<vtkPolyData> wallMesh,
                                           double maxWSS)
{
    auto idx = renderer.addScalarSurface("WSS", wallMesh, "WSS");
    renderer.setSurfaceScalarRange(idx, 0.0, maxWSS);
    renderer.setSurfaceLookupTable(idx, SurfaceRenderer::createWSSLookupTable(maxWSS));
    impl_->wssIdx = idx;
    return idx;
}

size_t HemodynamicSurfaceManager::showOSI(SurfaceRenderer& renderer,
                                           vtkSmartPointer<vtkPolyData> wallMesh)
{
    auto idx = renderer.addScalarSurface("OSI", wallMesh, "OSI");
    renderer.setSurfaceScalarRange(idx, 0.0, 0.5);
    renderer.setSurfaceLookupTable(idx, SurfaceRenderer::createOSILookupTable());
    impl_->osiIdx = idx;
    return idx;
}

size_t HemodynamicSurfaceManager::showAFI(SurfaceRenderer& renderer,
                                           vtkSmartPointer<vtkPolyData> tawssSurface)
{
    auto afiSurface = computeAFI(tawssSurface);
    if (!afiSurface) {
        // Fallback: add with raw TAWSS data
        auto idx = renderer.addScalarSurface("AFI", tawssSurface, "TAWSS");
        impl_->afiIdx = idx;
        return idx;
    }

    // Determine max AFI from the computed array
    double maxAFI = 2.0;
    if (auto* arr = afiSurface->GetPointData()->GetArray("AFI")) {
        double range[2];
        arr->GetRange(range);
        maxAFI = range[1];
        if (maxAFI < 2.0) maxAFI = 2.0;
    }

    auto idx = renderer.addScalarSurface("AFI", afiSurface, "AFI");
    renderer.setSurfaceScalarRange(idx, 0.0, maxAFI);
    renderer.setSurfaceLookupTable(idx, SurfaceRenderer::createAFILookupTable(maxAFI));
    impl_->afiIdx = idx;
    return idx;
}

size_t HemodynamicSurfaceManager::showRRT(SurfaceRenderer& renderer,
                                           vtkSmartPointer<vtkPolyData> rrtSurface,
                                           double maxRRT)
{
    auto idx = renderer.addScalarSurface("RRT", rrtSurface, "RRT");
    renderer.setSurfaceScalarRange(idx, 0.0, maxRRT);
    renderer.setSurfaceLookupTable(idx, SurfaceRenderer::createRRTLookupTable(maxRRT));
    impl_->rrtIdx = idx;
    return idx;
}

std::optional<size_t> HemodynamicSurfaceManager::wssIndex() const
{
    return impl_->wssIdx;
}

std::optional<size_t> HemodynamicSurfaceManager::osiIndex() const
{
    return impl_->osiIdx;
}

std::optional<size_t> HemodynamicSurfaceManager::afiIndex() const
{
    return impl_->afiIdx;
}

std::optional<size_t> HemodynamicSurfaceManager::rrtIndex() const
{
    return impl_->rrtIdx;
}

vtkSmartPointer<vtkPolyData>
HemodynamicSurfaceManager::computeAFI(vtkSmartPointer<vtkPolyData> tawssSurface)
{
    if (!tawssSurface) return nullptr;

    auto* tawssArray = tawssSurface->GetPointData()->GetArray("TAWSS");
    if (!tawssArray) return nullptr;

    auto numPoints = tawssArray->GetNumberOfTuples();
    if (numPoints == 0) return nullptr;

    // Compute mean TAWSS
    double sum = 0.0;
    for (vtkIdType i = 0; i < numPoints; ++i) {
        sum += tawssArray->GetComponent(i, 0);
    }
    double meanTAWSS = sum / static_cast<double>(numPoints);

    // Avoid division by zero
    if (meanTAWSS < 1e-12) return nullptr;

    // Create AFI array: AFI = TAWSS_local / mean_TAWSS
    auto afiArray = vtkSmartPointer<vtkFloatArray>::New();
    afiArray->SetName("AFI");
    afiArray->SetNumberOfComponents(1);
    afiArray->SetNumberOfTuples(numPoints);

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double tawss = tawssArray->GetComponent(i, 0);
        auto afi = static_cast<float>(tawss / meanTAWSS);
        afiArray->SetValue(i, afi);
    }

    // Deep copy and add AFI array
    auto output = vtkSmartPointer<vtkPolyData>::New();
    output->DeepCopy(tawssSurface);
    output->GetPointData()->AddArray(afiArray);
    output->GetPointData()->SetActiveScalars("AFI");

    return output;
}

} // namespace dicom_viewer::services

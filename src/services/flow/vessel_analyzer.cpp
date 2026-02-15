#include "services/flow/vessel_analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>

#include "core/logging.hpp"

namespace {

auto& getLogger() {
    static auto logger =
        dicom_viewer::logging::LoggerFactory::create("VesselAnalyzer");
    return logger;
}

}  // anonymous namespace

namespace dicom_viewer::services {

// =============================================================================
// VesselAnalyzer::Impl
// =============================================================================

class VesselAnalyzer::Impl {
public:
    double bloodViscosity = 0.004;   // Pa*s (4 cP)
    double bloodDensity = 1060.0;    // kg/m^3
    double lowWSSThreshold = 0.4;    // Pa
};

// =============================================================================
// Lifecycle
// =============================================================================

VesselAnalyzer::VesselAnalyzer()
    : impl_(std::make_unique<Impl>()) {}

VesselAnalyzer::~VesselAnalyzer() = default;

VesselAnalyzer::VesselAnalyzer(VesselAnalyzer&&) noexcept = default;
VesselAnalyzer& VesselAnalyzer::operator=(VesselAnalyzer&&) noexcept = default;

// =============================================================================
// Configuration
// =============================================================================

void VesselAnalyzer::setBloodViscosity(double mu) {
    impl_->bloodViscosity = mu;
}

void VesselAnalyzer::setBloodDensity(double rho) {
    impl_->bloodDensity = rho;
}

void VesselAnalyzer::setLowWSSThreshold(double threshold) {
    impl_->lowWSSThreshold = threshold;
}

double VesselAnalyzer::bloodViscosity() const {
    return impl_->bloodViscosity;
}

double VesselAnalyzer::bloodDensity() const {
    return impl_->bloodDensity;
}

// =============================================================================
// WSS computation (single phase)
// =============================================================================

std::expected<WSSResult, FlowError>
VesselAnalyzer::computeWSS(const VelocityPhase& phase,
                           vtkSmartPointer<vtkPolyData> wallMesh) const {
    if (!phase.velocityField) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "VelocityPhase has null velocity field"});
    }
    if (!wallMesh || wallMesh->GetNumberOfPoints() == 0) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Wall mesh is null or empty"});
    }

    auto image = phase.velocityField;
    if (image->GetNumberOfComponentsPerPixel() != 3) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Expected 3-component velocity field"});
    }

    // Ensure wall mesh has normals
    vtkSmartPointer<vtkPolyData> meshWithNormals = wallMesh;
    if (!wallMesh->GetPointData()->GetNormals()) {
        auto normalFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
        normalFilter->SetInputData(wallMesh);
        normalFilter->ComputePointNormalsOn();
        normalFilter->SplittingOff();
        normalFilter->Update();
        meshWithNormals = normalFilter->GetOutput();
    }

    auto normals = meshWithNormals->GetPointData()->GetNormals();
    int numVertices = meshWithNormals->GetNumberOfPoints();

    // Create WSS arrays
    auto wssMagnitude = vtkSmartPointer<vtkFloatArray>::New();
    wssMagnitude->SetName("WSS_Magnitude");
    wssMagnitude->SetNumberOfTuples(numVertices);

    auto wssVector = vtkSmartPointer<vtkFloatArray>::New();
    wssVector->SetName("WSS_Vector");
    wssVector->SetNumberOfComponents(3);
    wssVector->SetNumberOfTuples(numVertices);

    auto region = image->GetLargestPossibleRegion();
    double mu = impl_->bloodViscosity;
    double sumWSS = 0.0;
    double maxWSS = 0.0;
    int validCount = 0;

    // Multi-point sampling for velocity gradient estimation
    auto spacing = image->GetSpacing();
    double minSpacing = std::min({spacing[0], spacing[1], spacing[2]});
    constexpr int kNumSamples = 3;

    for (int i = 0; i < numVertices; ++i) {
        double pt[3], normal[3];
        meshWithNormals->GetPoint(i, pt);
        normals->GetTuple(i, normal);

        // Inward normal (flip outward normal)
        double inward[3] = {-normal[0], -normal[1], -normal[2]};

        // Sample tangential velocity at 1×, 2×, 3× spacing from wall
        double sd[kNumSamples], stx[kNumSamples], sty[kNumSamples], stz[kNumSamples];
        int nValid = 0;

        for (int s = 0; s < kNumSamples; ++s) {
            double d = minSpacing * (s + 1);
            VectorImage3D::PointType samplePoint;
            samplePoint[0] = pt[0] + inward[0] * d;
            samplePoint[1] = pt[1] + inward[1] * d;
            samplePoint[2] = pt[2] + inward[2] * d;

            VectorImage3D::IndexType idx;
            if (!image->TransformPhysicalPointToIndex(samplePoint, idx)) continue;
            if (!region.IsInside(idx)) continue;

            auto pixel = image->GetPixel(idx);
            double vx = pixel[0], vy = pixel[1], vz = pixel[2];

            // Tangential velocity: V_t = V - (V·n)*n
            double vDotN = vx * inward[0] + vy * inward[1] + vz * inward[2];
            sd[nValid] = d;
            stx[nValid] = vx - vDotN * inward[0];
            sty[nValid] = vy - vDotN * inward[1];
            stz[nValid] = vz - vDotN * inward[2];
            ++nValid;
        }

        if (nValid == 0) {
            wssMagnitude->SetValue(i, 0.0f);
            wssVector->SetTuple3(i, 0.0, 0.0, 0.0);
            continue;
        }

        // Estimate velocity gradient at wall (d=0) in cm/s per mm
        double gx, gy, gz;

        if (nValid >= 3) {
            // Quadratic interpolation: Lagrange polynomial derivative at d=0
            double d1 = sd[0], d2 = sd[1], d3 = sd[2];
            double c1 = -(d2 + d3) / ((d1 - d2) * (d1 - d3));
            double c2 = -(d1 + d3) / ((d2 - d1) * (d2 - d3));
            double c3 = -(d1 + d2) / ((d3 - d1) * (d3 - d2));
            gx = c1 * stx[0] + c2 * stx[1] + c3 * stx[2];
            gy = c1 * sty[0] + c2 * sty[1] + c3 * sty[2];
            gz = c1 * stz[0] + c2 * stz[1] + c3 * stz[2];
        } else if (nValid == 2) {
            double invDd = 1.0 / (sd[1] - sd[0]);
            gx = (stx[1] - stx[0]) * invDd;
            gy = (sty[1] - sty[0]) * invDd;
            gz = (stz[1] - stz[0]) * invDd;
        } else {
            gx = stx[0] / sd[0];
            gy = sty[0] / sd[0];
            gz = stz[0] / sd[0];
        }

        // Convert gradient: (cm/s)/mm → (m/s)/m = ×10
        // WSS = μ × velocity_gradient (Pa = Pa·s × s⁻¹)
        double wssX = mu * gx * 10.0;
        double wssY = mu * gy * 10.0;
        double wssZ = mu * gz * 10.0;
        double wssMag = std::sqrt(wssX * wssX + wssY * wssY + wssZ * wssZ);

        wssMagnitude->SetValue(i, static_cast<float>(wssMag));
        wssVector->SetTuple3(i, wssX, wssY, wssZ);

        sumWSS += wssMag;
        maxWSS = std::max(maxWSS, wssMag);
        ++validCount;
    }

    // Attach arrays to output mesh
    auto outputMesh = vtkSmartPointer<vtkPolyData>::New();
    outputMesh->DeepCopy(meshWithNormals);
    outputMesh->GetPointData()->AddArray(wssMagnitude);
    outputMesh->GetPointData()->AddArray(wssVector);

    // Compute low WSS area from actual mesh cell areas
    double lowWSSArea = 0.0;
    auto* polys = outputMesh->GetPolys();
    if (polys) {
        polys->InitTraversal();
        vtkIdType npts;
        const vtkIdType* pts;
        while (polys->GetNextCell(npts, pts)) {
            if (npts != 3) continue;
            double w0 = wssMagnitude->GetValue(pts[0]);
            double w1 = wssMagnitude->GetValue(pts[1]);
            double w2 = wssMagnitude->GetValue(pts[2]);
            if ((w0 + w1 + w2) / 3.0 >= impl_->lowWSSThreshold) continue;

            double p0[3], p1[3], p2[3];
            outputMesh->GetPoint(pts[0], p0);
            outputMesh->GetPoint(pts[1], p1);
            outputMesh->GetPoint(pts[2], p2);
            double e1[3] = {p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2]};
            double e2[3] = {p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2]};
            double cx = e1[1]*e2[2] - e1[2]*e2[1];
            double cy = e1[2]*e2[0] - e1[0]*e2[2];
            double cz = e1[0]*e2[1] - e1[1]*e2[0];
            lowWSSArea += 0.5 * std::sqrt(cx*cx + cy*cy + cz*cz);
        }
        lowWSSArea /= 100.0;  // mm² → cm²
    }

    WSSResult result;
    result.wallMesh = outputMesh;
    result.meanWSS = (validCount > 0) ? sumWSS / validCount : 0.0;
    result.maxWSS = maxWSS;
    result.lowWSSArea = lowWSSArea;
    result.wallVertexCount = validCount;

    getLogger()->debug("WSS: mean={:.4f} Pa, max={:.4f} Pa, lowArea={:.2f} cm², vertices={}",
                       result.meanWSS, result.maxWSS, result.lowWSSArea, validCount);

    return result;
}

// =============================================================================
// Time-Averaged WSS (TAWSS)
// =============================================================================

std::expected<WSSResult, FlowError>
VesselAnalyzer::computeTAWSS(const std::vector<VelocityPhase>& phases,
                             vtkSmartPointer<vtkPolyData> wallMesh) const {
    if (phases.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "No phases provided for TAWSS computation"});
    }

    // Compute WSS for each phase and accumulate
    std::vector<WSSResult> phaseResults;
    phaseResults.reserve(phases.size());

    for (const auto& phase : phases) {
        auto wssResult = computeWSS(phase, wallMesh);
        if (!wssResult) {
            return std::unexpected(wssResult.error());
        }
        phaseResults.push_back(std::move(*wssResult));
    }

    int numVertices = phaseResults[0].wallMesh->GetNumberOfPoints();
    int numPhases = static_cast<int>(phases.size());

    // Compute TAWSS = (1/N) * sum(|WSS_i|)
    auto tawssArray = vtkSmartPointer<vtkFloatArray>::New();
    tawssArray->SetName("TAWSS");
    tawssArray->SetNumberOfTuples(numVertices);

    double sumTAWSS = 0.0;
    double maxTAWSS = 0.0;

    for (int v = 0; v < numVertices; ++v) {
        double sum = 0.0;
        for (int p = 0; p < numPhases; ++p) {
            auto wssMag = phaseResults[p].wallMesh->GetPointData()
                ->GetArray("WSS_Magnitude");
            if (wssMag) {
                sum += wssMag->GetTuple1(v);
            }
        }
        double tawss = sum / numPhases;
        tawssArray->SetValue(v, static_cast<float>(tawss));
        sumTAWSS += tawss;
        maxTAWSS = std::max(maxTAWSS, tawss);
    }

    auto outputMesh = vtkSmartPointer<vtkPolyData>::New();
    outputMesh->DeepCopy(phaseResults[0].wallMesh);
    outputMesh->GetPointData()->AddArray(tawssArray);

    WSSResult result;
    result.wallMesh = outputMesh;
    result.meanWSS = (numVertices > 0) ? sumTAWSS / numVertices : 0.0;
    result.maxWSS = maxTAWSS;
    result.wallVertexCount = numVertices;

    getLogger()->info("TAWSS: mean={:.4f} Pa, max={:.4f} Pa, phases={}",
                      result.meanWSS, result.maxWSS, numPhases);

    return result;
}

// =============================================================================
// Oscillatory Shear Index (OSI)
// =============================================================================

std::expected<WSSResult, FlowError>
VesselAnalyzer::computeOSI(const std::vector<VelocityPhase>& phases,
                           vtkSmartPointer<vtkPolyData> wallMesh) const {
    if (phases.size() < 2) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "OSI requires at least 2 cardiac phases"});
    }

    // Compute WSS vectors for each phase
    std::vector<WSSResult> phaseResults;
    for (const auto& phase : phases) {
        auto wss = computeWSS(phase, wallMesh);
        if (!wss) return std::unexpected(wss.error());
        phaseResults.push_back(std::move(*wss));
    }

    int numVertices = phaseResults[0].wallMesh->GetNumberOfPoints();
    int numPhases = static_cast<int>(phases.size());

    auto osiArray = vtkSmartPointer<vtkFloatArray>::New();
    osiArray->SetName("OSI");
    osiArray->SetNumberOfTuples(numVertices);

    auto tawssArray = vtkSmartPointer<vtkFloatArray>::New();
    tawssArray->SetName("TAWSS");
    tawssArray->SetNumberOfTuples(numVertices);

    double sumOSI = 0.0;
    int validCount = 0;

    for (int v = 0; v < numVertices; ++v) {
        // Sum of WSS vectors and sum of magnitudes
        double sumVec[3] = {0, 0, 0};
        double sumMag = 0.0;

        for (int p = 0; p < numPhases; ++p) {
            auto wssVec = phaseResults[p].wallMesh->GetPointData()
                ->GetArray("WSS_Vector");
            auto wssMag = phaseResults[p].wallMesh->GetPointData()
                ->GetArray("WSS_Magnitude");
            if (wssVec && wssMag) {
                double vec[3];
                wssVec->GetTuple(v, vec);
                sumVec[0] += vec[0];
                sumVec[1] += vec[1];
                sumVec[2] += vec[2];
                sumMag += wssMag->GetTuple1(v);
            }
        }

        double magSumVec = std::sqrt(sumVec[0] * sumVec[0] +
                                     sumVec[1] * sumVec[1] +
                                     sumVec[2] * sumVec[2]);

        double osi = 0.0;
        double tawss = sumMag / numPhases;
        if (sumMag > 1e-12) {
            osi = 0.5 * (1.0 - magSumVec / sumMag);
        }

        osiArray->SetValue(v, static_cast<float>(osi));
        tawssArray->SetValue(v, static_cast<float>(tawss));

        sumOSI += osi;
        ++validCount;
    }

    auto outputMesh = vtkSmartPointer<vtkPolyData>::New();
    outputMesh->DeepCopy(phaseResults[0].wallMesh);
    outputMesh->GetPointData()->AddArray(osiArray);
    outputMesh->GetPointData()->AddArray(tawssArray);

    WSSResult result;
    result.wallMesh = outputMesh;
    result.meanOSI = (validCount > 0) ? sumOSI / validCount : 0.0;
    result.wallVertexCount = validCount;

    // Compute TAWSS stats from the array
    double sumTAWSS = 0.0;
    double maxTAWSS = 0.0;
    for (int v = 0; v < numVertices; ++v) {
        double t = tawssArray->GetValue(v);
        sumTAWSS += t;
        maxTAWSS = std::max(maxTAWSS, t);
    }
    result.meanWSS = (numVertices > 0) ? sumTAWSS / numVertices : 0.0;
    result.maxWSS = maxTAWSS;

    getLogger()->info("OSI: mean={:.4f}, TAWSS mean={:.4f} Pa, phases={}",
                      result.meanOSI, result.meanWSS, numPhases);

    return result;
}

// =============================================================================
// Vorticity (curl of velocity)
// =============================================================================

std::expected<VortexResult, FlowError>
VesselAnalyzer::computeVorticity(const VelocityPhase& phase) const {
    if (!phase.velocityField) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "VelocityPhase has null velocity field"});
    }

    auto image = phase.velocityField;
    if (image->GetNumberOfComponentsPerPixel() != 3) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Expected 3-component velocity field"});
    }

    auto region = image->GetLargestPossibleRegion();
    auto size = region.GetSize();
    auto spacing = image->GetSpacing();

    // Create output images matching input geometry
    auto vortField = VectorImage3D::New();
    vortField->SetRegions(region);
    vortField->SetSpacing(image->GetSpacing());
    vortField->SetOrigin(image->GetOrigin());
    vortField->SetDirection(image->GetDirection());
    vortField->SetNumberOfComponentsPerPixel(3);
    vortField->Allocate(true);

    auto vortMag = FloatImage3D::New();
    vortMag->SetRegions(region);
    vortMag->SetSpacing(image->GetSpacing());
    vortMag->SetOrigin(image->GetOrigin());
    vortMag->SetDirection(image->GetDirection());
    vortMag->Allocate(true);

    auto helicity = FloatImage3D::New();
    helicity->SetRegions(region);
    helicity->SetSpacing(image->GetSpacing());
    helicity->SetOrigin(image->GetOrigin());
    helicity->SetDirection(image->GetDirection());
    helicity->Allocate(true);

    // Spacing in meters for SI vorticity (1/s)
    // But velocity is in cm/s and spacing in mm
    // vorticity = dV/dx where V in cm/s and dx in mm
    // → raw vorticity unit = cm/s/mm = 10/s
    // To get 1/s: multiply by 10
    double dxMM = spacing[0];
    double dyMM = spacing[1];
    double dzMM = spacing[2];

    // Central differences: dVi/dj = (V[j+1] - V[j-1]) / (2*spacing_j)
    // Units: (cm/s) / mm → multiply by 10 for 1/s
    auto* vBuf = image->GetBufferPointer();
    auto* vortBuf = vortField->GetBufferPointer();
    auto* magBuf = vortMag->GetBufferPointer();
    auto* helBuf = helicity->GetBufferPointer();

    int nx = static_cast<int>(size[0]);
    int ny = static_cast<int>(size[1]);
    int nz = static_cast<int>(size[2]);

    for (int z = 1; z < nz - 1; ++z) {
        for (int y = 1; y < ny - 1; ++y) {
            for (int x = 1; x < nx - 1; ++x) {
                int idx = z * ny * nx + y * nx + x;

                // Neighbor indices for central differences
                int xp = z * ny * nx + y * nx + (x + 1);
                int xm = z * ny * nx + y * nx + (x - 1);
                int yp = z * ny * nx + (y + 1) * nx + x;
                int ym = z * ny * nx + (y - 1) * nx + x;
                int zp = (z + 1) * ny * nx + y * nx + x;
                int zm = (z - 1) * ny * nx + y * nx + x;

                // Velocity components at neighbors
                // dVx/dy, dVx/dz
                double dVxDy = (vBuf[yp * 3] - vBuf[ym * 3]) / (2.0 * dyMM);
                double dVxDz = (vBuf[zp * 3] - vBuf[zm * 3]) / (2.0 * dzMM);

                // dVy/dx, dVy/dz
                double dVyDx = (vBuf[xp * 3 + 1] - vBuf[xm * 3 + 1]) / (2.0 * dxMM);
                double dVyDz = (vBuf[zp * 3 + 1] - vBuf[zm * 3 + 1]) / (2.0 * dzMM);

                // dVz/dx, dVz/dy
                double dVzDx = (vBuf[xp * 3 + 2] - vBuf[xm * 3 + 2]) / (2.0 * dxMM);
                double dVzDy = (vBuf[yp * 3 + 2] - vBuf[ym * 3 + 2]) / (2.0 * dyMM);

                // curl(V) = (dVz/dy - dVy/dz, dVx/dz - dVz/dx, dVy/dx - dVx/dy)
                // Convert from cm/s/mm to 1/s: multiply by 10
                double wx = (dVzDy - dVyDz) * 10.0;
                double wy = (dVxDz - dVzDx) * 10.0;
                double wz = (dVyDx - dVxDy) * 10.0;

                vortBuf[idx * 3]     = static_cast<float>(wx);
                vortBuf[idx * 3 + 1] = static_cast<float>(wy);
                vortBuf[idx * 3 + 2] = static_cast<float>(wz);

                double mag = std::sqrt(wx * wx + wy * wy + wz * wz);
                magBuf[idx] = static_cast<float>(mag);

                // Helicity density: H = V · omega
                // V in cm/s, omega in 1/s → H in cm/s * 1/s = cm/s^2
                double vx = vBuf[idx * 3];
                double vy = vBuf[idx * 3 + 1];
                double vz = vBuf[idx * 3 + 2];
                double h = vx * wx + vy * wy + vz * wz;
                helBuf[idx] = static_cast<float>(h);
            }
        }
    }

    VortexResult result;
    result.vorticityField = vortField;
    result.vorticityMagnitude = vortMag;
    result.helicityDensity = helicity;

    getLogger()->debug("Vorticity: computed for {}x{}x{} volume",
                       nx, ny, nz);

    return result;
}

// =============================================================================
// Turbulent Kinetic Energy (TKE)
// =============================================================================

std::expected<FloatImage3D::Pointer, FlowError>
VesselAnalyzer::computeTKE(const std::vector<VelocityPhase>& phases) const {
    if (phases.size() < 3) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "TKE requires at least 3 cardiac phases"});
    }

    // Validate all phases have velocity fields
    for (const auto& phase : phases) {
        if (!phase.velocityField) {
            return std::unexpected(FlowError{
                FlowError::Code::InvalidInput,
                "Phase " + std::to_string(phase.phaseIndex) +
                " has null velocity field"});
        }
        if (phase.velocityField->GetNumberOfComponentsPerPixel() != 3) {
            return std::unexpected(FlowError{
                FlowError::Code::InvalidInput,
                "Phase " + std::to_string(phase.phaseIndex) +
                " has wrong component count"});
        }
    }

    auto refImage = phases[0].velocityField;
    auto region = refImage->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int numPixels = static_cast<int>(size[0] * size[1] * size[2]);
    int numPhases = static_cast<int>(phases.size());

    // Create output TKE image
    auto tke = FloatImage3D::New();
    tke->SetRegions(region);
    tke->SetSpacing(refImage->GetSpacing());
    tke->SetOrigin(refImage->GetOrigin());
    tke->SetDirection(refImage->GetDirection());
    tke->Allocate(true);

    auto* tkeBuf = tke->GetBufferPointer();

    // First pass: compute mean velocity at each voxel
    std::vector<float> meanVx(numPixels, 0.0f);
    std::vector<float> meanVy(numPixels, 0.0f);
    std::vector<float> meanVz(numPixels, 0.0f);

    for (const auto& phase : phases) {
        auto* buf = phase.velocityField->GetBufferPointer();
        for (int i = 0; i < numPixels; ++i) {
            meanVx[i] += buf[i * 3];
            meanVy[i] += buf[i * 3 + 1];
            meanVz[i] += buf[i * 3 + 2];
        }
    }

    for (int i = 0; i < numPixels; ++i) {
        meanVx[i] /= numPhases;
        meanVy[i] /= numPhases;
        meanVz[i] /= numPhases;
    }

    // Second pass: compute variance
    for (const auto& phase : phases) {
        auto* buf = phase.velocityField->GetBufferPointer();
        for (int i = 0; i < numPixels; ++i) {
            float dvx = buf[i * 3] - meanVx[i];
            float dvy = buf[i * 3 + 1] - meanVy[i];
            float dvz = buf[i * 3 + 2] - meanVz[i];
            tkeBuf[i] += dvx * dvx + dvy * dvy + dvz * dvz;
        }
    }

    // TKE = 0.5 * (var_Vx + var_Vy + var_Vz)
    // Variance = sum((Vi - mean)^2) / N
    // Convert from (cm/s)^2 to (m/s)^2: divide by 10000
    // Then multiply by density/2 for J/m^3
    double rho = impl_->bloodDensity;
    for (int i = 0; i < numPixels; ++i) {
        double variance_sum = tkeBuf[i] / numPhases;  // (cm/s)^2
        double variance_si = variance_sum * 1e-4;     // (m/s)^2
        tkeBuf[i] = static_cast<float>(0.5 * rho * variance_si);  // J/m^3
    }

    getLogger()->info("TKE: computed from {} phases, density={} kg/m^3",
                      numPhases, rho);

    return tke;
}

// =============================================================================
// Kinetic Energy
// =============================================================================

std::expected<KineticEnergyResult, FlowError>
VesselAnalyzer::computeKineticEnergy(const VelocityPhase& phase,
                                     FloatImage3D::Pointer mask) const {
    if (!phase.velocityField) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "VelocityPhase has null velocity field"});
    }

    auto image = phase.velocityField;
    if (image->GetNumberOfComponentsPerPixel() != 3) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Expected 3-component velocity field"});
    }

    auto region = image->GetLargestPossibleRegion();
    auto size = region.GetSize();
    auto spacing = image->GetSpacing();

    // Create output KE image matching input geometry
    auto keImage = FloatImage3D::New();
    keImage->SetRegions(region);
    keImage->SetSpacing(image->GetSpacing());
    keImage->SetOrigin(image->GetOrigin());
    keImage->SetDirection(image->GetDirection());
    keImage->Allocate(true);

    int numPixels = static_cast<int>(size[0] * size[1] * size[2]);
    auto* vBuf = image->GetBufferPointer();
    auto* keBuf = keImage->GetBufferPointer();

    // Validate mask dimensions if provided
    float* maskBuf = nullptr;
    if (mask) {
        auto maskSize = mask->GetLargestPossibleRegion().GetSize();
        if (maskSize[0] != size[0] || maskSize[1] != size[1] || maskSize[2] != size[2]) {
            return std::unexpected(FlowError{
                FlowError::Code::InvalidInput,
                "Mask dimensions do not match velocity field"});
        }
        maskBuf = mask->GetBufferPointer();
    }

    double rho = impl_->bloodDensity;  // kg/m^3

    // Voxel volume in m^3: spacing is in mm, so mm^3 * 1e-9 = m^3
    double voxelVolM3 = spacing[0] * spacing[1] * spacing[2] * 1e-9;

    double totalKE = 0.0;
    double sumKEDensity = 0.0;
    int voxelCount = 0;

    for (int i = 0; i < numPixels; ++i) {
        // Skip masked-out voxels
        if (maskBuf && maskBuf[i] == 0.0f) {
            continue;
        }

        // Velocity in cm/s → m/s: multiply by 0.01
        double vx = vBuf[i * 3] * 0.01;
        double vy = vBuf[i * 3 + 1] * 0.01;
        double vz = vBuf[i * 3 + 2] * 0.01;
        double u2 = vx * vx + vy * vy + vz * vz;

        // Per-voxel KE density: 0.5 * rho * |u|^2 (J/m^3)
        double keDensity = 0.5 * rho * u2;
        keBuf[i] = static_cast<float>(keDensity);

        sumKEDensity += keDensity;
        totalKE += keDensity * voxelVolM3;
        ++voxelCount;
    }

    KineticEnergyResult result;
    result.keField = keImage;
    result.totalKE = totalKE;
    result.meanKE = (voxelCount > 0) ? sumKEDensity / voxelCount : 0.0;
    result.voxelCount = voxelCount;

    getLogger()->info("KE: total={:.6f} J, mean={:.2f} J/m^3, voxels={}",
                      result.totalKE, result.meanKE, voxelCount);

    return result;
}

// =============================================================================
// Relative Residence Time (RRT)
// =============================================================================

std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
VesselAnalyzer::computeRRT(vtkSmartPointer<vtkPolyData> surface) const {
    if (!surface || surface->GetNumberOfPoints() == 0) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Surface is null or empty"});
    }

    auto* osiArray = surface->GetPointData()->GetArray("OSI");
    auto* tawssArray = surface->GetPointData()->GetArray("TAWSS");

    if (!osiArray || !tawssArray) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Surface must have OSI and TAWSS point data arrays"});
    }

    int numPoints = surface->GetNumberOfPoints();

    auto rrtArray = vtkSmartPointer<vtkFloatArray>::New();
    rrtArray->SetName("RRT");
    rrtArray->SetNumberOfTuples(numPoints);

    for (int i = 0; i < numPoints; ++i) {
        double osi = osiArray->GetTuple1(i);
        double tawss = tawssArray->GetTuple1(i);

        // RRT = 1 / ((1 - 2*OSI) * TAWSS)
        double denominator = (1.0 - 2.0 * osi) * tawss;
        float rrt = 0.0f;
        if (std::abs(denominator) > 1e-12) {
            rrt = static_cast<float>(1.0 / denominator);
        }

        rrtArray->SetValue(i, rrt);
    }

    auto outputSurface = vtkSmartPointer<vtkPolyData>::New();
    outputSurface->DeepCopy(surface);
    outputSurface->GetPointData()->AddArray(rrtArray);

    getLogger()->info("RRT: computed for {} surface points", numPoints);

    return outputSurface;
}

}  // namespace dicom_viewer::services

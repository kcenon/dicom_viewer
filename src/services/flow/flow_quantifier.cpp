#include "services/flow/flow_quantifier.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>

#include "core/logging.hpp"

namespace {

auto& getLogger() {
    static auto logger =
        dicom_viewer::logging::LoggerFactory::create("FlowQuantifier");
    return logger;
}

/// Generate orthogonal basis vectors for a plane given its normal
void computePlaneBasis(const std::array<double, 3>& normal,
                       std::array<double, 3>& u,
                       std::array<double, 3>& v) {
    using dicom_viewer::services::FlowQuantifier;

    // Pick reference vector not parallel to normal
    std::array<double, 3> ref = {1.0, 0.0, 0.0};
    if (std::abs(FlowQuantifier::dotProduct(normal, ref)) > 0.9) {
        ref = {0.0, 1.0, 0.0};
    }

    u = FlowQuantifier::normalize(FlowQuantifier::crossProduct(normal, ref));
    v = FlowQuantifier::normalize(FlowQuantifier::crossProduct(normal, u));
}

}  // anonymous namespace

namespace dicom_viewer::services {

// =============================================================================
// FlowQuantifier::Impl
// =============================================================================

class FlowQuantifier::Impl {
public:
    MeasurementPlane plane;
};

// =============================================================================
// Lifecycle
// =============================================================================

FlowQuantifier::FlowQuantifier()
    : impl_(std::make_unique<Impl>()) {}

FlowQuantifier::~FlowQuantifier() = default;

FlowQuantifier::FlowQuantifier(FlowQuantifier&&) noexcept = default;
FlowQuantifier& FlowQuantifier::operator=(FlowQuantifier&&) noexcept = default;

// =============================================================================
// Measurement plane configuration
// =============================================================================

void FlowQuantifier::setMeasurementPlane(const MeasurementPlane& plane) {
    impl_->plane = plane;
    // Ensure normal is unit vector
    impl_->plane.normal = normalize(plane.normal);
}

void FlowQuantifier::setMeasurementPlaneFrom3Points(
    const std::array<double, 3>& p1,
    const std::array<double, 3>& p2,
    const std::array<double, 3>& p3) {

    // Edge vectors
    std::array<double, 3> e1 = {p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2]};
    std::array<double, 3> e2 = {p3[0] - p1[0], p3[1] - p1[1], p3[2] - p1[2]};

    // Normal = e1 × e2
    auto normal = normalize(crossProduct(e1, e2));

    // Center = centroid
    std::array<double, 3> center = {
        (p1[0] + p2[0] + p3[0]) / 3.0,
        (p1[1] + p2[1] + p3[1]) / 3.0,
        (p1[2] + p2[2] + p3[2]) / 3.0
    };

    impl_->plane.center = center;
    impl_->plane.normal = normal;
}

MeasurementPlane FlowQuantifier::measurementPlane() const {
    return impl_->plane;
}

// =============================================================================
// Core flow measurement
// =============================================================================

std::expected<FlowMeasurement, FlowError>
FlowQuantifier::measureFlow(const VelocityPhase& phase) const {
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

    const auto& center = impl_->plane.center;
    const auto& normal = impl_->plane.normal;
    double radius = impl_->plane.radius;
    double spacing = std::max(0.1, impl_->plane.sampleSpacing);

    // Compute plane basis vectors
    std::array<double, 3> u, v;
    computePlaneBasis(normal, u, v);

    // Sample velocity field at grid points on the plane
    double sumThroughPlane = 0.0;
    double maxThroughPlane = 0.0;
    int sampleCount = 0;

    // Pixel area in cm^2 (spacing in mm → spacing/10 in cm)
    double pixelAreaCm2 = (spacing / 10.0) * (spacing / 10.0);

    auto region = image->GetLargestPossibleRegion();

    for (double si = -radius; si <= radius; si += spacing) {
        for (double sj = -radius; sj <= radius; sj += spacing) {
            // Skip points outside circular sampling region
            if (si * si + sj * sj > radius * radius) continue;

            // World coordinate of sample point
            VectorImage3D::PointType point;
            point[0] = center[0] + si * u[0] + sj * v[0];
            point[1] = center[1] + si * u[1] + sj * v[1];
            point[2] = center[2] + si * u[2] + sj * v[2];

            // Transform to image index (nearest-neighbor)
            VectorImage3D::IndexType index;
            bool inBounds = image->TransformPhysicalPointToIndex(point, index);
            if (!inBounds) continue;
            if (!region.IsInside(index)) continue;

            // Get velocity vector at this point
            auto pixel = image->GetPixel(index);
            std::array<double, 3> velocity = {
                pixel[0], pixel[1], pixel[2]
            };

            // Through-plane velocity component (dot product with normal)
            double vThrough = dotProduct(velocity, normal);

            sumThroughPlane += vThrough;
            maxThroughPlane = std::max(maxThroughPlane, std::abs(vThrough));
            ++sampleCount;
        }
    }

    FlowMeasurement result;
    result.phaseIndex = phase.phaseIndex;
    result.sampleCount = sampleCount;

    if (sampleCount > 0) {
        result.meanVelocity = sumThroughPlane / sampleCount;
        result.maxVelocity = maxThroughPlane;
        result.crossSectionArea = sampleCount * pixelAreaCm2;
        // Flow rate = mean_through_plane_velocity × area
        result.flowRate = sumThroughPlane * pixelAreaCm2;  // mL/s
    }

    getLogger()->debug("Phase {}: flow={:.2f} mL/s, mean_v={:.2f} cm/s, "
                       "max_v={:.2f} cm/s, area={:.2f} cm², samples={}",
                       result.phaseIndex, result.flowRate,
                       result.meanVelocity, result.maxVelocity,
                       result.crossSectionArea, result.sampleCount);

    return result;
}

// =============================================================================
// Time-velocity curve computation
// =============================================================================

std::expected<TimeVelocityCurve, FlowError>
FlowQuantifier::computeTimeVelocityCurve(
    const std::vector<VelocityPhase>& phases,
    double temporalResolution) const {

    if (phases.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "No phases provided for time-velocity curve"});
    }

    if (temporalResolution <= 0.0) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Temporal resolution must be positive"});
    }

    TimeVelocityCurve tvc;
    tvc.timePoints.reserve(phases.size());
    tvc.meanVelocities.reserve(phases.size());
    tvc.maxVelocities.reserve(phases.size());
    tvc.flowRates.reserve(phases.size());

    for (const auto& phase : phases) {
        auto measurement = measureFlow(phase);
        if (!measurement) {
            return std::unexpected(measurement.error());
        }

        tvc.timePoints.push_back(phase.triggerTime);
        tvc.meanVelocities.push_back(measurement->meanVelocity);
        tvc.maxVelocities.push_back(measurement->maxVelocity);
        tvc.flowRates.push_back(measurement->flowRate);
    }

    // Compute stroke volume and regurgitant volume
    // dt in seconds = temporalResolution in ms / 1000
    double dtSeconds = temporalResolution / 1000.0;

    double forwardFlow = 0.0;
    double backwardFlow = 0.0;

    for (double fr : tvc.flowRates) {
        if (fr >= 0.0) {
            forwardFlow += fr * dtSeconds;   // mL
        } else {
            backwardFlow += (-fr) * dtSeconds;  // mL (positive value)
        }
    }

    tvc.strokeVolume = forwardFlow;
    tvc.regurgitantVolume = backwardFlow;

    if (forwardFlow > 0.0) {
        tvc.regurgitantFraction = (backwardFlow / forwardFlow) * 100.0;
    }

    getLogger()->info("TVC: {} phases, SV={:.1f} mL, RV={:.1f} mL, "
                      "RF={:.1f}%",
                      phases.size(), tvc.strokeVolume,
                      tvc.regurgitantVolume, tvc.regurgitantFraction);

    return tvc;
}

// =============================================================================
// Pressure gradient estimation
// =============================================================================

double FlowQuantifier::estimatePressureGradient(double maxVelocityCmPerS) {
    // Simplified Bernoulli: ΔP = 4 × V² (mmHg, V in m/s)
    // V_m_per_s = V_cm_per_s / 100
    // ΔP = 4 × (V/100)² = 4 × V² / 10000 = 0.0004 × V²
    double vMetersPerS = maxVelocityCmPerS / 100.0;
    return 4.0 * vMetersPerS * vMetersPerS;
}

// =============================================================================
// CSV export
// =============================================================================

std::expected<void, FlowError>
FlowQuantifier::exportToCSV(const TimeVelocityCurve& curve,
                            const std::string& filePath) {
    if (filePath.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Empty file path"});
    }

    std::ofstream ofs(filePath);
    if (!ofs.is_open()) {
        return std::unexpected(FlowError{
            FlowError::Code::InternalError,
            "Cannot open file: " + filePath});
    }

    // Header
    ofs << "Time_ms,MeanVelocity_cm_s,MaxVelocity_cm_s,FlowRate_mL_s\n";

    // Data rows
    for (size_t i = 0; i < curve.timePoints.size(); ++i) {
        ofs << curve.timePoints[i] << ","
            << curve.meanVelocities[i] << ","
            << curve.maxVelocities[i] << ","
            << curve.flowRates[i] << "\n";
    }

    // Summary
    ofs << "\n# Summary\n";
    ofs << "# Stroke Volume (mL)," << curve.strokeVolume << "\n";
    ofs << "# Regurgitant Volume (mL)," << curve.regurgitantVolume << "\n";
    ofs << "# Regurgitant Fraction (%)," << curve.regurgitantFraction << "\n";

    return {};
}

// =============================================================================
// Vector math utilities
// =============================================================================

double FlowQuantifier::dotProduct(
    const std::array<double, 3>& a,
    const std::array<double, 3>& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

std::array<double, 3> FlowQuantifier::normalize(
    const std::array<double, 3>& v) {
    double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len < 1e-12) {
        return {0, 0, 0};
    }
    return {v[0] / len, v[1] / len, v[2] / len};
}

std::array<double, 3> FlowQuantifier::crossProduct(
    const std::array<double, 3>& a,
    const std::array<double, 3>& b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

}  // namespace dicom_viewer::services

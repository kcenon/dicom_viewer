#pragma once

/// @file flow_phantom_generator.hpp
/// @brief Synthetic 4D Flow MRI phantom data generator for integration testing
///
/// Generates VelocityPhase objects with analytically known ground truth
/// for validating the complete 4D Flow pipeline.
///
/// Supported phantoms:
/// - Poiseuille pipe flow:  V(r) = Vmax*(1 - r^2/R^2), Q = pi*R^2*Vmax/2
/// - Pulsatile flow:        Sinusoidal variation across cardiac phases
/// - Aliased velocity:      Phase-wrapped field exceeding VENC
/// - Rotating cylinder:     Rigid body rotation with vorticity = 2*omega

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include <itkImage.h>
#include <itkVectorImage.h>

#include "services/flow/velocity_field_assembler.hpp"

namespace dicom_viewer::test_utils {

using services::FloatImage3D;
using services::VectorImage3D;
using services::VelocityPhase;

/// Analytical ground truth for a Poiseuille pipe phantom
struct PoiseuilleGroundTruth {
    double vMax;        ///< Peak centerline velocity (cm/s)
    double radius;      ///< Pipe radius (mm)
    double flowRate;    ///< Analytical flow rate: pi*R_cm^2*Vmax/2 (mL/s)
    double meanVelocity; ///< Mean velocity: Vmax/2 (cm/s)
};

/// Analytical ground truth for a pulsatile flow phantom
struct PulsatileGroundTruth {
    double baseVelocity;    ///< Mean velocity amplitude (cm/s)
    double amplitude;       ///< Sinusoidal amplitude (cm/s)
    double strokeVolume;    ///< Expected stroke volume (mL)
    int phaseCount;
    double temporalResolution;  ///< ms between phases
};

/// Analytical ground truth for a rotating cylinder phantom
struct RotatingGroundTruth {
    double angularVelocity; ///< rad/s
    double radius;          ///< Cylinder radius (mm)
    double vorticity;       ///< Analytical vorticity: 2*omega
};

/// Create a VectorImage3D with specified dimensions, spacing, and origin
inline VectorImage3D::Pointer createVectorImage(
    int dimX, int dimY, int dimZ,
    double spacingMm = 1.0,
    std::array<double, 3> originMm = {0.0, 0.0, 0.0}) {

    auto image = VectorImage3D::New();
    VectorImage3D::SizeType size;
    size[0] = dimX; size[1] = dimY; size[2] = dimZ;
    VectorImage3D::IndexType start;
    start[0] = 0; start[1] = 0; start[2] = 0;
    image->SetRegions(VectorImage3D::RegionType(start, size));
    image->SetNumberOfComponentsPerPixel(3);

    VectorImage3D::SpacingType spacing;
    spacing[0] = spacingMm; spacing[1] = spacingMm; spacing[2] = spacingMm;
    image->SetSpacing(spacing);

    VectorImage3D::PointType origin;
    origin[0] = originMm[0]; origin[1] = originMm[1]; origin[2] = originMm[2];
    image->SetOrigin(origin);

    image->Allocate(true);  // zero-initialize
    return image;
}

/// Create a FloatImage3D with specified dimensions
inline FloatImage3D::Pointer createScalarImage(
    int dimX, int dimY, int dimZ,
    double spacingMm = 1.0,
    std::array<double, 3> originMm = {0.0, 0.0, 0.0}) {

    auto image = FloatImage3D::New();
    FloatImage3D::SizeType size;
    size[0] = dimX; size[1] = dimY; size[2] = dimZ;
    FloatImage3D::IndexType start;
    start[0] = 0; start[1] = 0; start[2] = 0;
    image->SetRegions(FloatImage3D::RegionType(start, size));

    FloatImage3D::SpacingType spacing;
    spacing[0] = spacingMm; spacing[1] = spacingMm; spacing[2] = spacingMm;
    image->SetSpacing(spacing);

    FloatImage3D::PointType origin;
    origin[0] = originMm[0]; origin[1] = originMm[1]; origin[2] = originMm[2];
    image->SetOrigin(origin);

    image->Allocate(true);
    return image;
}

// =============================================================================
// Poiseuille Pipe Flow
// =============================================================================

/// Generate Poiseuille pipe flow along Z axis with parabolic velocity profile
///
/// @param dim       Volume dimension (cubic: dim x dim x dim)
/// @param vMax      Peak centerline velocity (cm/s)
/// @param pipeRadius Pipe radius in voxels (must be < dim/2)
/// @param phaseIndex Cardiac phase index
/// @return VelocityPhase with parabolic Z-velocity, and PoiseuilleGroundTruth
inline std::pair<VelocityPhase, PoiseuilleGroundTruth>
generatePoiseuillePipe(int dim, double vMax, double pipeRadius,
                       int phaseIndex = 0) {
    auto velocity = createVectorImage(dim, dim, dim);
    auto magnitude = createScalarImage(dim, dim, dim);

    auto* vBuf = velocity->GetBufferPointer();
    auto* mBuf = magnitude->GetBufferPointer();

    double centerX = (dim - 1) / 2.0;
    double centerY = (dim - 1) / 2.0;
    double R2 = pipeRadius * pipeRadius;

    for (int z = 0; z < dim; ++z) {
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                int idx = z * dim * dim + y * dim + x;
                double dx = x - centerX;
                double dy = y - centerY;
                double r2 = dx * dx + dy * dy;

                float vz = 0.0f;
                if (r2 < R2) {
                    vz = static_cast<float>(vMax * (1.0 - r2 / R2));
                }

                vBuf[idx * 3]     = 0.0f;  // Vx
                vBuf[idx * 3 + 1] = 0.0f;  // Vy
                vBuf[idx * 3 + 2] = vz;    // Vz
                mBuf[idx] = std::abs(vz);
            }
        }
    }

    VelocityPhase phase;
    phase.velocityField = velocity;
    phase.magnitudeImage = magnitude;
    phase.phaseIndex = phaseIndex;
    phase.triggerTime = phaseIndex * 40.0;

    // Analytical: Q = pi * R_cm^2 * Vmax / 2
    // spacing = 1mm, so R_cm = pipeRadius * 0.1
    double R_cm = pipeRadius * 0.1;  // mm to cm
    PoiseuilleGroundTruth truth;
    truth.vMax = vMax;
    truth.radius = pipeRadius;
    truth.flowRate = std::numbers::pi * R_cm * R_cm * vMax / 2.0;
    truth.meanVelocity = vMax / 2.0;

    return {phase, truth};
}

// =============================================================================
// Pulsatile Flow
// =============================================================================

/// Generate multi-phase pulsatile flow with sinusoidal velocity variation
///
/// V(t) = baseVelocity + amplitude * sin(2*pi*t/T)
/// where T = phaseCount * temporalResolution
///
/// @param dim              Volume dimension
/// @param phaseCount       Number of cardiac phases
/// @param baseVelocity     DC component of velocity (cm/s)
/// @param amplitude        AC amplitude (cm/s)
/// @param temporalResolution Time between phases (ms)
/// @return Vector of VelocityPhase and PulsatileGroundTruth
inline std::pair<std::vector<VelocityPhase>, PulsatileGroundTruth>
generatePulsatileFlow(int dim, int phaseCount, double baseVelocity,
                      double amplitude, double temporalResolution) {
    std::vector<VelocityPhase> phases;
    phases.reserve(phaseCount);

    double T = phaseCount * temporalResolution;  // Full cycle period (ms)

    for (int p = 0; p < phaseCount; ++p) {
        double t = p * temporalResolution;
        double vz = baseVelocity + amplitude * std::sin(2.0 * std::numbers::pi * t / T);

        auto velocity = createVectorImage(dim, dim, dim);
        auto* buf = velocity->GetBufferPointer();
        int numPixels = dim * dim * dim;
        for (int i = 0; i < numPixels; ++i) {
            buf[i * 3]     = 0.0f;
            buf[i * 3 + 1] = 0.0f;
            buf[i * 3 + 2] = static_cast<float>(vz);
        }

        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = p;
        phase.triggerTime = t;
        phases.push_back(std::move(phase));
    }

    // Stroke volume = integral of positive flow over one cycle
    // For V(t) = base + A*sin(wt) with all flow positive (base > A):
    //   SV = cross_section_area * integral(V(t)*dt) over positive part
    // Since this is uniform flow, area = entire volume cross section
    PulsatileGroundTruth truth;
    truth.baseVelocity = baseVelocity;
    truth.amplitude = amplitude;
    truth.phaseCount = phaseCount;
    truth.temporalResolution = temporalResolution;
    // SV computed by integration in test (depends on measurement plane area)
    truth.strokeVolume = 0.0;

    return {std::move(phases), truth};
}

// =============================================================================
// Aliased Velocity Field
// =============================================================================

/// Generate a velocity field with values exceeding VENC, causing aliasing
///
/// Creates a field where the true velocity is `trueVelocity` along Z,
/// but after phase wrapping with the given VENC, the measured velocity
/// wraps around. The test should unwrap it back to the true value.
///
/// @param dim           Volume dimension
/// @param trueVelocity  Actual velocity (cm/s), should be > VENC
/// @param venc          Velocity encoding value (cm/s)
/// @return VelocityPhase with wrapped velocities
inline VelocityPhase generateAliasedField(int dim, double trueVelocity,
                                          double venc) {
    auto velocity = createVectorImage(dim, dim, dim);
    auto magnitude = createScalarImage(dim, dim, dim);

    auto* vBuf = velocity->GetBufferPointer();
    auto* mBuf = magnitude->GetBufferPointer();
    int numPixels = dim * dim * dim;

    // Simulate phase wrapping: if V > VENC, measured = V - 2*VENC
    double wrappedV = trueVelocity;
    while (wrappedV > venc) wrappedV -= 2.0 * venc;
    while (wrappedV < -venc) wrappedV += 2.0 * venc;

    for (int i = 0; i < numPixels; ++i) {
        vBuf[i * 3]     = 0.0f;
        vBuf[i * 3 + 1] = 0.0f;
        vBuf[i * 3 + 2] = static_cast<float>(wrappedV);
        mBuf[i] = static_cast<float>(std::abs(trueVelocity));
    }

    VelocityPhase phase;
    phase.velocityField = velocity;
    phase.magnitudeImage = magnitude;
    phase.phaseIndex = 0;
    phase.triggerTime = 0.0;
    return phase;
}

// =============================================================================
// Rotating Cylinder
// =============================================================================

/// Generate rigid-body rotation in XY plane around Z axis
///
/// V_x(r) = -omega * y, V_y(r) = omega * x (with respect to center)
/// Vorticity = 2 * omega (known analytical solution)
///
/// @param dim             Volume dimension
/// @param angularVelocity Angular velocity omega (rad/s, treated as cm/s scaling)
/// @param cylinderRadius  Radius of rotating region in voxels
/// @return VelocityPhase and RotatingGroundTruth
inline std::pair<VelocityPhase, RotatingGroundTruth>
generateRotatingCylinder(int dim, double angularVelocity,
                         double cylinderRadius) {
    auto velocity = createVectorImage(dim, dim, dim);
    auto magnitude = createScalarImage(dim, dim, dim);

    auto* vBuf = velocity->GetBufferPointer();
    auto* mBuf = magnitude->GetBufferPointer();

    double centerX = (dim - 1) / 2.0;
    double centerY = (dim - 1) / 2.0;
    double R2 = cylinderRadius * cylinderRadius;

    for (int z = 0; z < dim; ++z) {
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                int idx = z * dim * dim + y * dim + x;
                double dx = x - centerX;
                double dy = y - centerY;
                double r2 = dx * dx + dy * dy;

                float vx = 0.0f, vy = 0.0f;
                if (r2 < R2) {
                    // V = omega x r → Vx = -omega*dy, Vy = omega*dx
                    vx = static_cast<float>(-angularVelocity * dy);
                    vy = static_cast<float>(angularVelocity * dx);
                }

                vBuf[idx * 3]     = vx;
                vBuf[idx * 3 + 1] = vy;
                vBuf[idx * 3 + 2] = 0.0f;
                mBuf[idx] = std::sqrt(vx * vx + vy * vy);
            }
        }
    }

    VelocityPhase phase;
    phase.velocityField = velocity;
    phase.magnitudeImage = magnitude;
    phase.phaseIndex = 0;
    phase.triggerTime = 0.0;

    RotatingGroundTruth truth;
    truth.angularVelocity = angularVelocity;
    truth.radius = cylinderRadius;
    truth.vorticity = 2.0 * angularVelocity;

    return {phase, truth};
}

// =============================================================================
// Uniform flow with gradient (for eddy current correction testing)
// =============================================================================

/// Generate a velocity field with a linear background gradient
///
/// V_z(x,y,z) = trueVelocity + gradX*x + gradY*y + gradZ*z
/// After eddy current correction, the gradient should be removed.
///
/// @param dim           Volume dimension
/// @param trueVelocity  True uniform velocity (cm/s)
/// @param gradX/Y/Z     Background gradient coefficients (cm/s per mm)
inline VelocityPhase generateFieldWithBackground(
    int dim, double trueVelocity,
    double gradX, double gradY, double gradZ) {

    auto velocity = createVectorImage(dim, dim, dim);
    auto magnitude = createScalarImage(dim, dim, dim);

    auto* vBuf = velocity->GetBufferPointer();
    auto* mBuf = magnitude->GetBufferPointer();

    double center = (dim - 1) / 2.0;

    for (int z = 0; z < dim; ++z) {
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                int idx = z * dim * dim + y * dim + x;
                double bg = gradX * (x - center) + gradY * (y - center) +
                            gradZ * (z - center);
                float vz = static_cast<float>(trueVelocity + bg);

                vBuf[idx * 3]     = 0.0f;
                vBuf[idx * 3 + 1] = 0.0f;
                vBuf[idx * 3 + 2] = vz;

                // Magnitude represents tissue signal
                mBuf[idx] = 1000.0f;  // Uniform high signal
            }
        }
    }

    VelocityPhase phase;
    phase.velocityField = velocity;
    phase.magnitudeImage = magnitude;
    phase.phaseIndex = 0;
    phase.triggerTime = 0.0;
    return phase;
}

}  // namespace dicom_viewer::test_utils

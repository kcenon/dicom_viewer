#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

#include <itkVectorImage.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkTriangle.h>

#include "services/flow/vessel_analyzer.hpp"

#include "../test_utils/flow_phantom_generator.hpp"

using namespace dicom_viewer::services;
namespace phantom = dicom_viewer::test_utils;

namespace {

/// Create a simple cylindrical wall mesh around a pipe along Z axis
/// @param radius Pipe radius in mm
/// @param length Pipe length in mm
/// @param nCirc Circumferential segments
/// @param nAxial Axial segments
/// @param centerX/Y Center of pipe in mm
vtkSmartPointer<vtkPolyData> createCylindricalWallMesh(
    double radius, double length, int nCirc, int nAxial,
    double centerX, double centerY, double zStart) {

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto triangles = vtkSmartPointer<vtkCellArray>::New();
    auto normals = vtkSmartPointer<vtkFloatArray>::New();
    normals->SetName("Normals");
    normals->SetNumberOfComponents(3);

    // Generate vertices on cylinder surface
    for (int a = 0; a <= nAxial; ++a) {
        double z = zStart + length * a / nAxial;
        for (int c = 0; c < nCirc; ++c) {
            double theta = 2.0 * std::numbers::pi * c / nCirc;
            double x = centerX + radius * std::cos(theta);
            double y = centerY + radius * std::sin(theta);
            points->InsertNextPoint(x, y, z);

            // Outward normal
            float nx = static_cast<float>(std::cos(theta));
            float ny = static_cast<float>(std::sin(theta));
            normals->InsertNextTuple3(nx, ny, 0.0f);
        }
    }

    // Create triangles
    for (int a = 0; a < nAxial; ++a) {
        for (int c = 0; c < nCirc; ++c) {
            int c_next = (c + 1) % nCirc;
            int i00 = a * nCirc + c;
            int i01 = a * nCirc + c_next;
            int i10 = (a + 1) * nCirc + c;
            int i11 = (a + 1) * nCirc + c_next;

            auto tri1 = vtkSmartPointer<vtkTriangle>::New();
            tri1->GetPointIds()->SetId(0, i00);
            tri1->GetPointIds()->SetId(1, i10);
            tri1->GetPointIds()->SetId(2, i01);
            triangles->InsertNextCell(tri1);

            auto tri2 = vtkSmartPointer<vtkTriangle>::New();
            tri2->GetPointIds()->SetId(0, i01);
            tri2->GetPointIds()->SetId(1, i10);
            tri2->GetPointIds()->SetId(2, i11);
            triangles->InsertNextCell(tri2);
        }
    }

    auto mesh = vtkSmartPointer<vtkPolyData>::New();
    mesh->SetPoints(points);
    mesh->SetPolys(triangles);
    mesh->GetPointData()->SetNormals(normals);
    return mesh;
}

}  // anonymous namespace

// =============================================================================
// Configuration and lifecycle
// =============================================================================

TEST(VesselAnalyzerTest, DefaultConstruction) {
    VesselAnalyzer analyzer;
    EXPECT_NEAR(analyzer.bloodViscosity(), 0.004, 1e-6);
    EXPECT_NEAR(analyzer.bloodDensity(), 1060.0, 1e-6);
}

TEST(VesselAnalyzerTest, MoveConstruction) {
    VesselAnalyzer a;
    a.setBloodViscosity(0.005);
    VesselAnalyzer b(std::move(a));
    EXPECT_NEAR(b.bloodViscosity(), 0.005, 1e-6);
}

TEST(VesselAnalyzerTest, SetProperties) {
    VesselAnalyzer analyzer;
    analyzer.setBloodViscosity(0.003);
    analyzer.setBloodDensity(1050.0);
    EXPECT_NEAR(analyzer.bloodViscosity(), 0.003, 1e-6);
    EXPECT_NEAR(analyzer.bloodDensity(), 1050.0, 1e-6);
}

// =============================================================================
// WSS tests
// =============================================================================

TEST(VesselAnalyzerWSS, NullVelocityFieldReturnsError) {
    VesselAnalyzer analyzer;
    VelocityPhase phase;  // null field
    auto mesh = vtkSmartPointer<vtkPolyData>::New();
    auto result = analyzer.computeWSS(phase, mesh);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, FlowError::Code::InvalidInput);
}

TEST(VesselAnalyzerWSS, EmptyWallMeshReturnsError) {
    VesselAnalyzer analyzer;
    auto [phase, truth] = phantom::generatePoiseuillePipe(32, 100.0, 10.0);
    auto result = analyzer.computeWSS(phase, nullptr);
    EXPECT_FALSE(result.has_value());
}

TEST(VesselAnalyzerWSS, PoiseuilleFlowProducesNonZeroWSS) {
    // Poiseuille flow: V(r) = Vmax * (1 - r^2/R^2)
    // Wall shear: tau = mu * 2*Vmax/R (analytical for Poiseuille)
    constexpr int kDim = 64;
    constexpr double kVMax = 100.0;  // cm/s
    constexpr double kRadius = 15.0; // voxels = 15mm at 1mm spacing

    auto [phase, truth] = phantom::generatePoiseuillePipe(
        kDim, kVMax, kRadius);

    double centerXY = (kDim - 1) / 2.0;
    auto wallMesh = createCylindricalWallMesh(
        kRadius, kDim - 2.0, 32, 8, centerXY, centerXY, 1.0);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeWSS(phase, wallMesh);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // WSS should be positive for Poiseuille flow
    EXPECT_GT(result->meanWSS, 0.0);
    EXPECT_GT(result->maxWSS, 0.0);
    EXPECT_GT(result->wallVertexCount, 0);

    // Analytical WSS for Poiseuille: tau = mu * 2*Vmax/R
    // mu = 0.004 Pa*s, Vmax = 100 cm/s = 1 m/s, R = 15mm = 0.015 m
    // tau = 0.004 * 2 * 1.0 / 0.015 = 0.533 Pa
    double analyticalWSS = 0.004 * 2.0 * (kVMax * 0.01) / (kRadius * 0.001);
    // Allow broad tolerance due to discrete sampling and nearest-neighbor
    EXPECT_GT(result->meanWSS, analyticalWSS * 0.1)
        << "WSS should be in the right order of magnitude";
    EXPECT_LT(result->meanWSS, analyticalWSS * 5.0)
        << "WSS should not be wildly off";
}

TEST(VesselAnalyzerWSS, WSSOutputMeshHasDataArrays) {
    constexpr int kDim = 32;
    auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, 50.0, 8.0);

    double center = (kDim - 1) / 2.0;
    auto wallMesh = createCylindricalWallMesh(8.0, 20.0, 16, 4, center, center, 5.0);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeWSS(phase, wallMesh);
    ASSERT_TRUE(result.has_value());

    auto outMesh = result->wallMesh;
    ASSERT_NE(outMesh, nullptr);
    EXPECT_NE(outMesh->GetPointData()->GetArray("WSS_Magnitude"), nullptr);
    EXPECT_NE(outMesh->GetPointData()->GetArray("WSS_Vector"), nullptr);
}

// =============================================================================
// TAWSS tests
// =============================================================================

TEST(VesselAnalyzerTAWSS, EmptyPhasesReturnsError) {
    VesselAnalyzer analyzer;
    std::vector<VelocityPhase> empty;
    auto mesh = vtkSmartPointer<vtkPolyData>::New();
    auto result = analyzer.computeTAWSS(empty, mesh);
    EXPECT_FALSE(result.has_value());
}

TEST(VesselAnalyzerTAWSS, MultiPhaseProducesResult) {
    constexpr int kDim = 32;
    constexpr double kRadius = 8.0;

    // Create 3 phases with different velocities (pulsatile)
    std::vector<VelocityPhase> phases;
    for (int p = 0; p < 3; ++p) {
        double vmax = 50.0 + p * 20.0;
        auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, vmax, kRadius, p);
        phases.push_back(std::move(phase));
    }

    double center = (kDim - 1) / 2.0;
    auto wallMesh = createCylindricalWallMesh(kRadius, 20.0, 16, 4, center, center, 5.0);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeTAWSS(phases, wallMesh);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_GT(result->meanWSS, 0.0);
    EXPECT_GT(result->wallVertexCount, 0);

    // Output mesh should have TAWSS array
    EXPECT_NE(result->wallMesh->GetPointData()->GetArray("TAWSS"), nullptr);
}

// =============================================================================
// OSI tests
// =============================================================================

TEST(VesselAnalyzerOSI, TooFewPhasesReturnsError) {
    VesselAnalyzer analyzer;
    std::vector<VelocityPhase> onePhase(1);
    auto mesh = vtkSmartPointer<vtkPolyData>::New();
    auto result = analyzer.computeOSI(onePhase, mesh);
    EXPECT_FALSE(result.has_value());
}

TEST(VesselAnalyzerOSI, UnidirectionalFlowHasZeroOSI) {
    // All phases have same direction flow → OSI should be ~0
    constexpr int kDim = 32;
    constexpr double kRadius = 8.0;

    std::vector<VelocityPhase> phases;
    for (int p = 0; p < 5; ++p) {
        double vmax = 50.0 + p * 5.0;  // Always positive, same direction
        auto [phase, truth] = phantom::generatePoiseuillePipe(kDim, vmax, kRadius, p);
        phases.push_back(std::move(phase));
    }

    double center = (kDim - 1) / 2.0;
    auto wallMesh = createCylindricalWallMesh(kRadius, 20.0, 16, 4, center, center, 5.0);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeOSI(phases, wallMesh);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // Unidirectional flow → OSI ≈ 0
    EXPECT_LT(result->meanOSI, 0.1)
        << "Unidirectional flow should have low OSI";

    EXPECT_NE(result->wallMesh->GetPointData()->GetArray("OSI"), nullptr);
}

// =============================================================================
// Vorticity tests
// =============================================================================

TEST(VesselAnalyzerVorticity, NullFieldReturnsError) {
    VesselAnalyzer analyzer;
    VelocityPhase phase;
    auto result = analyzer.computeVorticity(phase);
    EXPECT_FALSE(result.has_value());
}

TEST(VesselAnalyzerVorticity, UniformFlowHasZeroVorticity) {
    // Uniform flow → curl = 0
    constexpr int kDim = 32;
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    for (int i = 0; i < kDim * kDim * kDim; ++i) {
        buf[i * 3]     = 0.0f;
        buf[i * 3 + 1] = 0.0f;
        buf[i * 3 + 2] = 50.0f;  // Uniform Z flow
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    VesselAnalyzer analyzer;
    auto result = analyzer.computeVorticity(phase);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // Interior vorticity should be ~0 for uniform flow
    auto* magBuf = result->vorticityMagnitude->GetBufferPointer();
    int center = kDim / 2;
    int idx = center * kDim * kDim + center * kDim + center;
    EXPECT_NEAR(magBuf[idx], 0.0, 0.1);
}

TEST(VesselAnalyzerVorticity, RotatingCylinderMatchesAnalytical) {
    // Rigid body rotation: V = omega × r
    // Analytical vorticity = 2*omega (uniform inside cylinder)
    constexpr int kDim = 64;
    constexpr double kOmega = 5.0;  // cm/s per mm (angular velocity scaling)
    constexpr double kRadius = 20.0;

    auto [phase, truth] = phantom::generateRotatingCylinder(
        kDim, kOmega, kRadius);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeVorticity(phase);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // Check vorticity at center (should be ≈ 2*omega * 10 in 1/s)
    // The phantom has V in cm/s, spacing in mm
    // dVy/dx - dVx/dy = omega - (-omega) = 2*omega (in cm/s/mm)
    // After unit conversion (×10): 2*omega*10 = 100 1/s
    auto* vortBuf = result->vorticityField->GetBufferPointer();
    int center = kDim / 2;
    int idx = center * kDim * kDim + center * kDim + center;

    double wz = vortBuf[idx * 3 + 2];
    double expectedWz = 2.0 * kOmega * 10.0;  // 1/s
    EXPECT_NEAR(wz, expectedWz, expectedWz * 0.05)
        << "Z-vorticity at center should match 2*omega";

    // X and Y components should be ~0 for pure XY rotation
    EXPECT_NEAR(vortBuf[idx * 3], 0.0, 1.0);
    EXPECT_NEAR(vortBuf[idx * 3 + 1], 0.0, 1.0);
}

TEST(VesselAnalyzerVorticity, HelicitySignMatchesRotationDirection) {
    constexpr int kDim = 64;

    // Right-handed rotation with forward flow
    auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
    auto* buf = velocity->GetBufferPointer();
    double center = (kDim - 1) / 2.0;
    double omega = 3.0;
    double R2 = 20.0 * 20.0;

    for (int z = 0; z < kDim; ++z) {
        for (int y = 0; y < kDim; ++y) {
            for (int x = 0; x < kDim; ++x) {
                int idx = z * kDim * kDim + y * kDim + x;
                double dx = x - center;
                double dy = y - center;
                if (dx * dx + dy * dy < R2) {
                    buf[idx * 3]     = static_cast<float>(-omega * dy);
                    buf[idx * 3 + 1] = static_cast<float>(omega * dx);
                    buf[idx * 3 + 2] = 30.0f;  // Forward flow along Z
                } else {
                    buf[idx * 3]     = 0.0f;
                    buf[idx * 3 + 1] = 0.0f;
                    buf[idx * 3 + 2] = 0.0f;
                }
            }
        }
    }

    VelocityPhase phase;
    phase.velocityField = velocity;

    VesselAnalyzer analyzer;
    auto result = analyzer.computeVorticity(phase);
    ASSERT_TRUE(result.has_value());

    // Helicity = V · ω. With positive wz and positive Vz → positive helicity
    auto* helBuf = result->helicityDensity->GetBufferPointer();
    int cIdx = (kDim / 2) * kDim * kDim + (kDim / 2) * kDim + (kDim / 2);
    EXPECT_GT(helBuf[cIdx], 0.0)
        << "Forward flow + positive rotation → positive helicity";
}

TEST(VesselAnalyzerVorticity, OutputImageDimensionsMatch) {
    constexpr int kDim = 16;
    auto [phase, truth] = phantom::generateRotatingCylinder(kDim, 2.0, 6.0);

    VesselAnalyzer analyzer;
    auto result = analyzer.computeVorticity(phase);
    ASSERT_TRUE(result.has_value());

    auto size = result->vorticityMagnitude->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], static_cast<unsigned>(kDim));
    EXPECT_EQ(size[1], static_cast<unsigned>(kDim));
    EXPECT_EQ(size[2], static_cast<unsigned>(kDim));

    EXPECT_EQ(result->vorticityField->GetNumberOfComponentsPerPixel(), 3u);
}

// =============================================================================
// TKE tests
// =============================================================================

TEST(VesselAnalyzerTKE, TooFewPhasesReturnsError) {
    VesselAnalyzer analyzer;
    std::vector<VelocityPhase> twoPhases(2);
    auto result = analyzer.computeTKE(twoPhases);
    EXPECT_FALSE(result.has_value());
}

TEST(VesselAnalyzerTKE, NullPhaseReturnsError) {
    VesselAnalyzer analyzer;
    std::vector<VelocityPhase> phases(4);  // All null velocity fields
    auto result = analyzer.computeTKE(phases);
    EXPECT_FALSE(result.has_value());
}

TEST(VesselAnalyzerTKE, ConstantFlowHasZeroTKE) {
    // Same velocity at all phases → zero variance → zero TKE
    constexpr int kDim = 16;
    std::vector<VelocityPhase> phases;
    for (int p = 0; p < 5; ++p) {
        auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
        auto* buf = velocity->GetBufferPointer();
        for (int i = 0; i < kDim * kDim * kDim; ++i) {
            buf[i * 3]     = 10.0f;
            buf[i * 3 + 1] = 20.0f;
            buf[i * 3 + 2] = 50.0f;
        }
        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = p;
        phases.push_back(std::move(phase));
    }

    VesselAnalyzer analyzer;
    auto result = analyzer.computeTKE(phases);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // TKE should be ~0 everywhere
    auto* buf = result.value()->GetBufferPointer();
    int center = kDim / 2;
    int idx = center * kDim * kDim + center * kDim + center;
    EXPECT_NEAR(buf[idx], 0.0, 0.001);
}

TEST(VesselAnalyzerTKE, VariableFlowHasPositiveTKE) {
    // Different velocities at each phase → positive TKE
    constexpr int kDim = 16;
    std::vector<VelocityPhase> phases;
    for (int p = 0; p < 5; ++p) {
        auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
        auto* buf = velocity->GetBufferPointer();
        float vz = 30.0f + p * 20.0f;  // 30, 50, 70, 90, 110
        for (int i = 0; i < kDim * kDim * kDim; ++i) {
            buf[i * 3]     = 0.0f;
            buf[i * 3 + 1] = 0.0f;
            buf[i * 3 + 2] = vz;
        }
        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = p;
        phases.push_back(std::move(phase));
    }

    VesselAnalyzer analyzer;
    auto result = analyzer.computeTKE(phases);
    ASSERT_TRUE(result.has_value());

    auto* buf = result.value()->GetBufferPointer();
    int center = kDim / 2;
    int idx = center * kDim * kDim + center * kDim + center;

    // TKE should be positive
    EXPECT_GT(buf[idx], 0.0);

    // Verify TKE value analytically:
    // Vz = [30, 50, 70, 90, 110], mean = 70
    // deviations = [-40, -20, 0, 20, 40]
    // var_Vz = (1600+400+0+400+1600)/5 = 800 (cm/s)^2
    // var_Vz_SI = 800 * 1e-4 = 0.08 (m/s)^2
    // TKE = 0.5 * rho * var = 0.5 * 1060 * 0.08 = 42.4 J/m^3
    EXPECT_NEAR(buf[idx], 42.4, 1.0);
}

TEST(VesselAnalyzerTKE, OutputImageDimensionsMatch) {
    constexpr int kDim = 16;
    std::vector<VelocityPhase> phases;
    for (int p = 0; p < 3; ++p) {
        auto velocity = phantom::createVectorImage(kDim, kDim, kDim);
        auto* buf = velocity->GetBufferPointer();
        for (int i = 0; i < kDim * kDim * kDim; ++i) {
            buf[i * 3 + 2] = static_cast<float>(p * 10.0);
        }
        VelocityPhase phase;
        phase.velocityField = velocity;
        phase.phaseIndex = p;
        phases.push_back(std::move(phase));
    }

    VesselAnalyzer analyzer;
    auto result = analyzer.computeTKE(phases);
    ASSERT_TRUE(result.has_value());

    auto size = result.value()->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(size[0], static_cast<unsigned>(kDim));
    EXPECT_EQ(size[1], static_cast<unsigned>(kDim));
    EXPECT_EQ(size[2], static_cast<unsigned>(kDim));
}

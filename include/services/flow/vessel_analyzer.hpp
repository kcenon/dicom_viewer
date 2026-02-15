#pragma once

#include <expected>
#include <memory>
#include <vector>

#include <itkImage.h>
#include <itkVectorImage.h>
#include <vtkSmartPointer.h>

#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/velocity_field_assembler.hpp"

class vtkPolyData;

namespace dicom_viewer::services {

/**
 * @brief Wall Shear Stress analysis result
 *
 * WSS is computed as tau = mu * dV/dn at the vessel wall surface.
 * Contains per-vertex WSS data on the wall mesh.
 *
 * @trace SRS-FR-047
 */
struct WSSResult {
    vtkSmartPointer<vtkPolyData> wallMesh;  ///< Vessel surface with WSS arrays
    double meanWSS = 0.0;           ///< Mean WSS magnitude (Pa)
    double maxWSS = 0.0;            ///< Maximum WSS magnitude (Pa)
    double meanOSI = 0.0;           ///< Mean Oscillatory Shear Index [0, 0.5]
    double lowWSSArea = 0.0;        ///< Area with WSS below threshold (cm^2)
    int wallVertexCount = 0;        ///< Number of wall vertices analyzed
};

/**
 * @brief Vortex and turbulence analysis result
 *
 * @trace SRS-FR-047
 */
struct VortexResult {
    FloatImage3D::Pointer vorticityMagnitude;   ///< |curl(V)| in 1/s
    VectorImage3D::Pointer vorticityField;      ///< curl(V) vector field
    FloatImage3D::Pointer helicityDensity;      ///< V dot curl(V) in m/s^2
};

/**
 * @brief Kinetic Energy analysis result
 *
 * Per-voxel KE = 0.5 * rho * |u|^2 (J/m^3)
 * Total KE = sum(per-voxel KE * voxel_volume) (Joules)
 *
 * @trace SRS-FR-047
 */
struct KineticEnergyResult {
    FloatImage3D::Pointer keField;   ///< Per-voxel KE in J/m^3
    double totalKE = 0.0;           ///< Integrated KE over volume (Joules)
    double meanKE = 0.0;            ///< Mean per-voxel KE (J/m^3)
    int voxelCount = 0;             ///< Number of voxels used in computation
};

/**
 * @brief Advanced hemodynamic analysis for 4D Flow velocity data
 *
 * Computes Wall Shear Stress (WSS), Oscillatory Shear Index (OSI),
 * Turbulent Kinetic Energy (TKE), vorticity, and helicity from
 * velocity fields at vessel wall boundaries.
 *
 * Algorithm Summary:
 * @code
 * WSS:       tau = mu * (dV/dn)|_wall
 * TAWSS:     (1/N) * sum(|tau_i|)
 * OSI:       0.5 * (1 - |sum(tau_i)| / sum(|tau_i|))
 * Vorticity: omega = curl(V) = nabla x V
 * Helicity:  H = V dot omega
 * TKE:       0.5 * (var_Vx + var_Vy + var_Vz)
 * @endcode
 *
 * This is a service-layer class without Qt dependency.
 *
 * @trace SRS-FR-047
 */
class VesselAnalyzer {
public:
    VesselAnalyzer();
    ~VesselAnalyzer();

    // Non-copyable, movable
    VesselAnalyzer(const VesselAnalyzer&) = delete;
    VesselAnalyzer& operator=(const VesselAnalyzer&) = delete;
    VesselAnalyzer(VesselAnalyzer&&) noexcept;
    VesselAnalyzer& operator=(VesselAnalyzer&&) noexcept;

    /**
     * @brief Set blood viscosity for WSS computation
     * @param mu Dynamic viscosity in Pa*s (default: 0.004 = 4 cP)
     */
    void setBloodViscosity(double mu);

    /**
     * @brief Set blood density for energy calculations
     * @param rho Density in kg/m^3 (default: 1060)
     */
    void setBloodDensity(double rho);

    /**
     * @brief Set low WSS threshold for area computation
     * @param threshold WSS threshold in Pa (default: 0.4 Pa)
     */
    void setLowWSSThreshold(double threshold);

    /**
     * @brief Get current blood viscosity
     */
    [[nodiscard]] double bloodViscosity() const;

    /**
     * @brief Get current blood density
     */
    [[nodiscard]] double bloodDensity() const;

    // --- Wall Shear Stress ---

    /**
     * @brief Compute WSS at vessel wall for a single phase
     *
     * For each wall vertex:
     * 1. Get inward normal direction
     * 2. Sample velocity at 1-2 voxels from wall along normal
     * 3. WSS = mu * |V_near| / distance
     *
     * @param phase Velocity field
     * @param wallMesh Vessel wall surface mesh with vertex normals
     * @return WSSResult with per-vertex WSS data
     */
    [[nodiscard]] std::expected<WSSResult, FlowError>
    computeWSS(const VelocityPhase& phase,
               vtkSmartPointer<vtkPolyData> wallMesh) const;

    /**
     * @brief Compute Time-Averaged WSS (TAWSS) across all phases
     *
     * TAWSS = (1/N) * sum(|tau_i|) at each wall vertex
     *
     * @param phases All cardiac phases
     * @param wallMesh Vessel wall surface mesh with vertex normals
     * @return WSSResult with TAWSS in meanWSS field
     */
    [[nodiscard]] std::expected<WSSResult, FlowError>
    computeTAWSS(const std::vector<VelocityPhase>& phases,
                 vtkSmartPointer<vtkPolyData> wallMesh) const;

    /**
     * @brief Compute Oscillatory Shear Index from per-phase WSS vectors
     *
     * OSI = 0.5 * (1 - |sum(tau_i)| / sum(|tau_i|))
     * Range: [0, 0.5], higher = more oscillatory (atherosclerosis risk)
     *
     * @param phases All cardiac phases
     * @param wallMesh Vessel wall surface mesh with vertex normals
     * @return WSSResult with OSI data on wallMesh
     */
    [[nodiscard]] std::expected<WSSResult, FlowError>
    computeOSI(const std::vector<VelocityPhase>& phases,
               vtkSmartPointer<vtkPolyData> wallMesh) const;

    // --- Vortex analysis ---

    /**
     * @brief Compute vorticity field (curl of velocity)
     *
     * omega = nabla x V using central finite differences:
     *   omega_x = dVz/dy - dVy/dz
     *   omega_y = dVx/dz - dVz/dx
     *   omega_z = dVy/dx - dVx/dy
     *
     * @param phase Velocity field
     * @return VortexResult with vorticity field and helicity
     */
    [[nodiscard]] std::expected<VortexResult, FlowError>
    computeVorticity(const VelocityPhase& phase) const;

    // --- Turbulent Kinetic Energy ---

    /**
     * @brief Compute TKE from temporal velocity variance
     *
     * TKE = 0.5 * (sigma^2_Vx + sigma^2_Vy + sigma^2_Vz)
     * where sigma^2 is temporal variance at each voxel
     *
     * @param phases All cardiac phases (minimum 3 required)
     * @return TKE volume in J/m^3
     */
    [[nodiscard]] std::expected<FloatImage3D::Pointer, FlowError>
    computeTKE(const std::vector<VelocityPhase>& phases) const;

    // --- Kinetic Energy ---

    /**
     * @brief Compute instantaneous Kinetic Energy for a single phase
     *
     * KE = 0.5 * rho * |u|^2 per voxel (J/m^3)
     * Total KE = sum(KE_voxel * voxel_volume) in Joules
     *
     * @param phase Velocity field
     * @param mask Optional mask restricting computation to ROI (non-zero voxels)
     * @return KineticEnergyResult on success, FlowError on failure
     */
    [[nodiscard]] std::expected<KineticEnergyResult, FlowError>
    computeKineticEnergy(const VelocityPhase& phase,
                         FloatImage3D::Pointer mask = nullptr) const;

    // --- Relative Residence Time ---

    /**
     * @brief Compute Relative Residence Time from OSI and TAWSS surface data
     *
     * RRT = 1 / ((1 - 2*OSI) * TAWSS) for each surface point.
     * Input surface must have "OSI" and "TAWSS" point data arrays
     * (as produced by computeOSI).
     *
     * @param surface Mesh with OSI and TAWSS arrays
     * @return Surface with added "RRT" array, or error
     */
    [[nodiscard]] std::expected<vtkSmartPointer<vtkPolyData>, FlowError>
    computeRRT(vtkSmartPointer<vtkPolyData> surface) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

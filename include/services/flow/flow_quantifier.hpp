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

#pragma once

#include <array>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/velocity_field_assembler.hpp"

namespace dicom_viewer::services {

/**
 * @brief Flow measurement result at a single cardiac phase
 *
 * @trace SRS-FR-047
 */
struct FlowMeasurement {
    int phaseIndex = 0;
    double flowRate = 0.0;           ///< mL/s (= cm^3/s)
    double meanVelocity = 0.0;       ///< cm/s (through-plane mean)
    double maxVelocity = 0.0;        ///< cm/s (through-plane max)
    double minVelocity = 0.0;        ///< cm/s (through-plane min)
    double stdVelocity = 0.0;        ///< cm/s (through-plane std dev)
    double crossSectionArea = 0.0;   ///< cm^2 (sampled area)
    double roiAreaMm2 = 0.0;         ///< mm^2 (physical ROI area)
    int sampleCount = 0;             ///< Number of in-bounds samples
};

/**
 * @brief Measurement plane definition for flow quantification
 */
struct MeasurementPlane {
    std::array<double, 3> center = {0, 0, 0};     ///< Plane center in mm
    std::array<double, 3> normal = {0, 0, 1};     ///< Plane normal (unit vector)
    double radius = 50.0;                          ///< Sampling radius in mm
    double sampleSpacing = 1.0;                    ///< Grid spacing in mm
};

/**
 * @brief Time-velocity curve across all cardiac phases
 *
 * @trace SRS-FR-047
 */
struct TimeVelocityCurve {
    std::vector<double> timePoints;       ///< ms from R-wave
    std::vector<double> meanVelocities;   ///< cm/s per phase
    std::vector<double> maxVelocities;    ///< cm/s per phase
    std::vector<double> minVelocities;    ///< cm/s per phase
    std::vector<double> stdVelocities;    ///< cm/s per phase
    std::vector<double> flowRates;        ///< mL/s per phase
    std::vector<double> minFlowRates;     ///< mL/s per phase (min per-pixel)
    std::vector<double> stdFlowRates;     ///< mL/s per phase (std dev of per-pixel)

    double strokeVolume = 0.0;            ///< mL (integral of forward flow)
    double regurgitantVolume = 0.0;       ///< mL (integral of backward flow)
    double regurgitantFraction = 0.0;     ///< percentage (0-100)
    double meanRoiArea = 0.0;             ///< mm^2 (mean ROI area across phases)
};

/**
 * @brief Quantitative hemodynamic measurement from 4D Flow velocity data
 *
 * Computes flow rate, stroke volume, time-velocity curves, and pressure
 * gradients from velocity fields at user-defined measurement planes.
 *
 * Flow Rate Algorithm:
 * @code
 * 1. Create grid of sample points on measurement plane
 * 2. For each sample point within vessel boundary:
 *    V_through = dot(V(x,y,z), plane_normal)
 * 3. FlowRate = sum(V_through) × pixel_area  [mL/s]
 * @endcode
 *
 * This is a service-layer class without Qt or VTK dependency.
 *
 * @trace SRS-FR-047
 */
class FlowQuantifier {
public:
    FlowQuantifier();
    ~FlowQuantifier();

    // Non-copyable, movable
    FlowQuantifier(const FlowQuantifier&) = delete;
    FlowQuantifier& operator=(const FlowQuantifier&) = delete;
    FlowQuantifier(FlowQuantifier&&) noexcept;
    FlowQuantifier& operator=(FlowQuantifier&&) noexcept;

    /**
     * @brief Set the measurement plane for flow quantification
     * @param plane Plane center, normal, radius, and sample spacing
     */
    void setMeasurementPlane(const MeasurementPlane& plane);

    /**
     * @brief Define measurement plane from three points
     *
     * Normal is computed as (p2-p1) × (p3-p1), center is centroid.
     */
    void setMeasurementPlaneFrom3Points(
        const std::array<double, 3>& p1,
        const std::array<double, 3>& p2,
        const std::array<double, 3>& p3);

    /**
     * @brief Get current measurement plane
     */
    [[nodiscard]] MeasurementPlane measurementPlane() const;

    // --- Core measurements ---

    /**
     * @brief Measure flow at a single cardiac phase
     *
     * Samples the velocity field at grid points on the measurement plane,
     * computes through-plane velocity component, and integrates to get
     * flow rate in mL/s.
     *
     * @param phase Velocity phase with 3-component vector field
     * @return FlowMeasurement on success, FlowError on failure
     */
    [[nodiscard]] std::expected<FlowMeasurement, FlowError>
    measureFlow(const VelocityPhase& phase) const;

    /**
     * @brief Compute time-velocity curve across all cardiac phases
     *
     * Calls measureFlow for each phase and computes stroke volume,
     * regurgitant volume, and regurgitant fraction.
     *
     * @param phases All cardiac phases in temporal order
     * @param temporalResolution Time between phases in ms
     * @return TimeVelocityCurve on success, FlowError on failure
     */
    [[nodiscard]] std::expected<TimeVelocityCurve, FlowError>
    computeTimeVelocityCurve(
        const std::vector<VelocityPhase>& phases,
        double temporalResolution) const;

    /**
     * @brief Estimate pressure gradient using simplified Bernoulli
     *
     * ΔP = 4 × V²_max (mmHg, when V in m/s)
     *
     * @param maxVelocityCmPerS Maximum velocity in cm/s
     * @return Pressure gradient in mmHg
     */
    [[nodiscard]] static double estimatePressureGradient(
        double maxVelocityCmPerS);

    /**
     * @brief Export time-velocity curve data to CSV file
     * @param curve Time-velocity curve data
     * @param filePath Output file path
     */
    [[nodiscard]] static std::expected<void, FlowError>
    exportToCSV(const TimeVelocityCurve& curve,
                const std::string& filePath);

    // --- Heart Rate ---

    /**
     * @brief Extract heart rate from trigger time data
     *
     * Computes HR = 60000 / RR_interval_ms from the trigger times
     * of the provided velocity phases. Assumes phases span one full
     * cardiac cycle.
     *
     * @param phases Velocity phases with trigger time data
     * @param temporalResolution Time between phases in ms (fallback)
     * @return Heart rate in BPM, or FlowError if computation fails
     */
    [[nodiscard]] static std::expected<double, FlowError>
    extractHeartRate(const std::vector<VelocityPhase>& phases,
                     double temporalResolution = 0.0);

    // --- Utility ---

    [[nodiscard]] static double dotProduct(
        const std::array<double, 3>& a,
        const std::array<double, 3>& b);

    [[nodiscard]] static std::array<double, 3> normalize(
        const std::array<double, 3>& v);

    [[nodiscard]] static std::array<double, 3> crossProduct(
        const std::array<double, 3>& a,
        const std::array<double, 3>& b);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

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
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Target phase for best-phase selection
 *
 * @trace SRS-FR-050
 */
enum class PhaseTarget {
    Diastole,   ///< Best diastolic phase (70-80% R-R)
    Systole,    ///< Best systolic phase (35-45% R-R)
    Custom      ///< Custom target percentage
};

/**
 * @brief Metadata for a single cardiac phase
 *
 * Represents one temporal phase within an ECG-gated cardiac cycle.
 * Each phase corresponds to a specific moment in the R-R interval,
 * identified either by absolute trigger time or nominal percentage.
 *
 * @trace SRS-FR-050, SDS-MOD-009
 */
struct CardiacPhaseInfo {
    int phaseIndex = 0;
    double triggerTime = 0.0;           ///< ms from R-wave
    double nominalPercentage = 0.0;     ///< % of R-R interval (0-100)
    std::string phaseLabel;             ///< e.g. "75% diastole"
    std::vector<int> frameIndices;      ///< Frame indices belonging to this phase

    /// Check if this phase is in the diastolic region (50-100% R-R)
    [[nodiscard]] bool isDiastolic() const noexcept {
        return nominalPercentage >= 50.0;
    }

    /// Check if this phase is in the systolic region (0-50% R-R)
    [[nodiscard]] bool isSystolic() const noexcept {
        return nominalPercentage < 50.0;
    }
};

/**
 * @brief Result of cardiac phase separation
 *
 * Contains all detected phases, best-phase indices, and R-R interval
 * estimation from a cardiac-gated acquisition.
 *
 * @trace SRS-FR-050, SDS-MOD-009
 */
struct CardiacPhaseResult {
    std::vector<CardiacPhaseInfo> phases;
    int bestDiastolicPhase = -1;        ///< Index into phases (70-80% R-R)
    int bestSystolicPhase = -1;         ///< Index into phases (35-45% R-R)
    double rrInterval = 0.0;            ///< Estimated R-R interval in ms
    int slicesPerPhase = 0;             ///< Number of slices per phase

    /// Check if phase separation succeeded
    [[nodiscard]] bool isValid() const noexcept {
        return !phases.empty() && slicesPerPhase > 0;
    }

    /// Get total number of phases
    [[nodiscard]] int phaseCount() const noexcept {
        return static_cast<int>(phases.size());
    }
};

/**
 * @brief Error information for cardiac operations
 *
 * @trace SRS-FR-050
 */
struct CardiacError {
    enum class Code {
        Success,
        NotCardiacGated,
        InsufficientPhases,
        MissingTemporalData,
        InconsistentFrameCount,
        VolumeAssemblyFailed,
        InternalError
    };

    Code code = Code::Success;
    std::string message;

    [[nodiscard]] bool isSuccess() const noexcept {
        return code == Code::Success;
    }

    [[nodiscard]] std::string toString() const {
        switch (code) {
            case Code::Success: return "Success";
            case Code::NotCardiacGated:
                return "Not a cardiac-gated series: " + message;
            case Code::InsufficientPhases:
                return "Insufficient cardiac phases: " + message;
            case Code::MissingTemporalData:
                return "Missing temporal data: " + message;
            case Code::InconsistentFrameCount:
                return "Inconsistent frame count: " + message;
            case Code::VolumeAssemblyFailed:
                return "Volume assembly failed: " + message;
            case Code::InternalError:
                return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/// DICOM tags relevant to cardiac gating
namespace cardiac_tag {
    /// Trigger Time (0018,1060) - ms from R-wave
    inline constexpr uint32_t TriggerTime           = 0x00181060;
    /// Cardiac Synchronization Technique (0018,9037) - PROSPECTIVE, RETROSPECTIVE, etc.
    inline constexpr uint32_t CardiacSyncTechnique   = 0x00189037;
    /// Nominal Percentage of Cardiac Phase (0018,9241)
    inline constexpr uint32_t NominalPercentage      = 0x00189241;
    /// Low R-R Value (0018,1081)
    inline constexpr uint32_t LowRRValue             = 0x00181081;
    /// High R-R Value (0018,1082)
    inline constexpr uint32_t HighRRValue            = 0x00181082;
    /// Intervals Acquired (0018,1083)
    inline constexpr uint32_t IntervalsAcquired      = 0x00181083;
    /// Heart Rate (0018,1088)
    inline constexpr uint32_t HeartRate              = 0x00181088;
}  // namespace cardiac_tag

/// Constants for cardiac phase analysis
namespace cardiac_constants {
    /// Optimal diastolic range: 70-80% R-R
    inline constexpr double kDiastoleRangeMin = 70.0;
    inline constexpr double kDiastoleRangeMax = 80.0;
    inline constexpr double kDiastoleOptimal  = 75.0;

    /// Optimal systolic range: 35-45% R-R
    inline constexpr double kSystoleRangeMin = 35.0;
    inline constexpr double kSystoleRangeMax = 45.0;
    inline constexpr double kSystoleOptimal  = 40.0;

    /// Trigger time clustering tolerance (ms)
    inline constexpr double kTriggerTimeToleranceMs = 10.0;
}  // namespace cardiac_constants

// =============================================================================
// Calcium Scoring Types
// =============================================================================

/**
 * @brief Individual calcified lesion detected in coronary calcium scoring
 *
 * Each lesion is a connected component of voxels >= 130 HU with area >= 1mm².
 * The Agatston score for the lesion is area * density weight factor.
 *
 * @trace SRS-FR-052, SDS-MOD-009
 */
struct CalcifiedLesion {
    int labelId = 0;                          ///< Connected component label
    double areaMM2 = 0.0;                     ///< Total area in mm²
    double peakHU = 0.0;                      ///< Peak Hounsfield Unit in lesion
    int weightFactor = 0;                     ///< Agatston density weight (1-4)
    double agatstonScore = 0.0;               ///< Sum of per-slice (area * weight)
    double volumeMM3 = 0.0;                   ///< Volume in mm³
    std::array<double, 3> centroid = {0.0, 0.0, 0.0};  ///< Center of mass
    std::string assignedArtery;               ///< "LAD", "LCx", "RCA", "LM", or ""
};

/**
 * @brief Complete calcium scoring result
 *
 * Contains total Agatston score, volume score, per-artery breakdown,
 * risk classification, and individual lesion details.
 *
 * @trace SRS-FR-052, SDS-MOD-009
 */
struct CalciumScoreResult {
    double totalAgatston = 0.0;
    double volumeScore = 0.0;                 ///< Total calcified volume in mm³
    double massScore = 0.0;                   ///< Mass score in mg (requires calibration)
    std::map<std::string, double> perArteryScores;  ///< "LAD" → Agatston
    std::string riskCategory;                 ///< "None", "Minimal", "Mild", "Moderate", "Severe"
    std::vector<CalcifiedLesion> lesions;
    int lesionCount = 0;

    /// Check if any calcification was found
    [[nodiscard]] bool hasCalcium() const noexcept {
        return totalAgatston > 0.0;
    }
};

/// Constants for Agatston calcium scoring algorithm
namespace calcium_constants {
    /// Fixed HU threshold for calcified lesions (Agatston standard)
    inline constexpr short kHUThreshold = 130;

    /// Minimum lesion area to qualify (noise filter)
    inline constexpr double kMinLesionAreaMM2 = 1.0;

    /// Density weight factor thresholds
    inline constexpr short kWeightThreshold1 = 130;   ///< 130-199 HU → weight 1
    inline constexpr short kWeightThreshold2 = 200;   ///< 200-299 HU → weight 2
    inline constexpr short kWeightThreshold3 = 300;   ///< 300-399 HU → weight 3
    inline constexpr short kWeightThreshold4 = 400;   ///< >= 400 HU → weight 4

    /// Risk classification thresholds (Agatston score)
    inline constexpr double kRiskNone = 0.0;
    inline constexpr double kRiskMinimal = 10.0;
    inline constexpr double kRiskMild = 100.0;
    inline constexpr double kRiskModerate = 400.0;
}  // namespace calcium_constants

// =============================================================================
// Coronary CTA Types (Centerline & CPR)
// =============================================================================

/**
 * @brief Parameters for Frangi vesselness filter
 *
 * Controls multi-scale Hessian analysis for tubular structure enhancement.
 * Default values are optimized for coronary arteries (0.5-3.0 mm radius).
 *
 * @trace SRS-FR-051, SDS-MOD-009
 */
struct VesselnessParams {
    double sigmaMin = 0.5;     ///< Minimum scale in mm
    double sigmaMax = 3.0;     ///< Maximum scale in mm
    int sigmaSteps = 5;        ///< Number of intermediate scales
    double alpha = 0.5;        ///< Plate-like structure suppression
    double beta = 0.5;         ///< Blob-like structure suppression
    double gamma = 5.0;        ///< Background suppression (Frobenius norm)
};

/**
 * @brief Single point along a vessel centerline
 *
 * Each point stores 3D position, estimated vessel radius,
 * and local Frenet frame (tangent, normal) for CPR generation.
 *
 * @trace SRS-FR-051
 */
struct CenterlinePoint {
    std::array<double, 3> position = {0.0, 0.0, 0.0};
    double radius = 0.0;                ///< Estimated vessel radius at this point (mm)
    std::array<double, 3> tangent = {1.0, 0.0, 0.0};   ///< Tangent direction
    std::array<double, 3> normal = {0.0, 1.0, 0.0};    ///< Normal direction
};

/**
 * @brief Complete centerline extraction result for one vessel
 *
 * @trace SRS-FR-051, SDS-MOD-009
 */
struct CenterlineResult {
    std::string vesselName;                     ///< "LAD", "LCx", "RCA", etc.
    std::vector<CenterlinePoint> points;
    double totalLength = 0.0;                   ///< Total path length in mm
    double minLumenDiameter = 0.0;              ///< Minimum lumen diameter in mm
    double referenceDiameter = 0.0;             ///< Proximal reference diameter in mm
    double stenosisPercent = 0.0;               ///< (1 - min/ref) * 100

    /// Check if the centerline is valid
    [[nodiscard]] bool isValid() const noexcept {
        return points.size() >= 2;
    }

    /// Get number of centerline points
    [[nodiscard]] int pointCount() const noexcept {
        return static_cast<int>(points.size());
    }
};

/**
 * @brief CPR view generation mode
 *
 * @trace SRS-FR-051
 */
enum class CPRType {
    Straightened,       ///< Unfold vessel onto flat 2D plane
    CrossSectional,     ///< Perpendicular slices at intervals
    Stretched           ///< Preserve proportional distances
};

}  // namespace dicom_viewer::services

#pragma once

#include <cmath>
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

}  // namespace dicom_viewer::services

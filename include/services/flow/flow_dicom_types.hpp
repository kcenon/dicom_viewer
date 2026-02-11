#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Error information for flow analysis operations
 *
 * @trace SRS-FR-043
 */
struct FlowError {
    enum class Code {
        Success,
        InvalidInput,
        UnsupportedVendor,
        ParseFailed,
        MissingTag,
        InconsistentData,
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
            case Code::InvalidInput: return "Invalid input: " + message;
            case Code::UnsupportedVendor: return "Unsupported vendor: " + message;
            case Code::ParseFailed: return "Parse failed: " + message;
            case Code::MissingTag: return "Missing DICOM tag: " + message;
            case Code::InconsistentData: return "Inconsistent data: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Scanner vendor identification
 */
enum class FlowVendorType {
    Unknown,
    Siemens,
    Philips,
    GE
};

/**
 * @brief Velocity encoding direction classification
 */
enum class VelocityComponent {
    Magnitude,
    Vx,
    Vy,
    Vz
};

/**
 * @brief Metadata for a single DICOM frame in a 4D Flow series
 */
struct FlowFrame {
    std::string filePath;
    std::string sopInstanceUid;
    int cardiacPhase = 0;
    VelocityComponent component = VelocityComponent::Magnitude;
    double venc = 0.0;
    int sliceIndex = 0;
    double triggerTime = 0.0;
};

/**
 * @brief Complete parsed result for a 4D Flow MRI series
 *
 * @trace SRS-FR-043
 */
struct FlowSeriesInfo {
    FlowVendorType vendor = FlowVendorType::Unknown;
    int phaseCount = 0;
    double temporalResolution = 0.0;
    std::array<double, 3> venc = {0.0, 0.0, 0.0};
    bool isSignedPhase = true;

    /// Frame matrix: [phaseIndex][component] → list of file paths (sorted by slice)
    std::vector<std::map<VelocityComponent, std::vector<std::string>>> frameMatrix;

    std::string patientId;
    std::string studyDate;
    std::string seriesDescription;
    std::string seriesInstanceUid;
};

/**
 * @brief Convert FlowVendorType to string
 */
[[nodiscard]] inline std::string vendorToString(FlowVendorType vendor) {
    switch (vendor) {
        case FlowVendorType::Siemens: return "Siemens";
        case FlowVendorType::Philips: return "Philips";
        case FlowVendorType::GE: return "GE";
        default: return "Unknown";
    }
}

/**
 * @brief Convert VelocityComponent to string
 */
[[nodiscard]] inline std::string componentToString(VelocityComponent comp) {
    switch (comp) {
        case VelocityComponent::Magnitude: return "Magnitude";
        case VelocityComponent::Vx: return "Vx";
        case VelocityComponent::Vy: return "Vy";
        case VelocityComponent::Vz: return "Vz";
    }
    return "Unknown";
}

}  // namespace dicom_viewer::services

#include "services/flow/vendor_parsers/siemens_flow_parser.hpp"

#include <algorithm>
#include <string>

namespace {

std::string getMetaString(const itk::MetaDataDictionary& dict,
                          const std::string& key) {
    std::string value;
    itk::ExposeMetaData<std::string>(dict, key, value);
    // Trim trailing whitespace/null
    while (!value.empty() && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    return value;
}

double parseDouble(const std::string& str) {
    try {
        return std::stod(str);
    } catch (...) {
        return 0.0;
    }
}

int parseInt(const std::string& str) {
    try {
        return std::stoi(str);
    } catch (...) {
        return 0;
    }
}

}  // anonymous namespace

namespace dicom_viewer::services {

FlowVendorType SiemensFlowParser::vendorType() const noexcept {
    return FlowVendorType::Siemens;
}

std::string SiemensFlowParser::expectedIODType() const {
    return "Enhanced MR Image Storage";
}

double SiemensFlowParser::extractVENC(
    const itk::MetaDataDictionary& dictionary) const {
    // (0018,9197) Velocity Encoding Minimum Value
    auto vencStr = getMetaString(dictionary, "0018|9197");
    if (!vencStr.empty()) {
        return std::abs(parseDouble(vencStr));
    }

    // Fallback: Siemens private tag (0051,1014) may contain VENC info
    auto privateStr = getMetaString(dictionary, "0051|1014");
    if (!privateStr.empty()) {
        // Siemens encodes as "v{VENC}cm/s" or similar
        auto pos = privateStr.find('v');
        if (pos != std::string::npos) {
            return parseDouble(privateStr.substr(pos + 1));
        }
    }

    return 0.0;
}

VelocityComponent SiemensFlowParser::classifyComponent(
    const itk::MetaDataDictionary& dictionary) const {
    // (0008,0008) Image Type — check for velocity encoding direction
    auto imageType = getMetaString(dictionary, "0008|0008");
    std::transform(imageType.begin(), imageType.end(), imageType.begin(),
                   ::toupper);

    // Siemens Enhanced MR: Image Type contains direction info
    // e.g., "ORIGINAL\PRIMARY\M\ND" (magnitude) or "ORIGINAL\PRIMARY\P\ND" (phase)
    if (imageType.find("\\M\\") != std::string::npos ||
        imageType.find("\\M_") != std::string::npos ||
        imageType.find("MAG") != std::string::npos) {
        return VelocityComponent::Magnitude;
    }

    // Siemens private tag (0051,1014) contains flow direction
    auto flowDir = getMetaString(dictionary, "0051|1014");
    std::transform(flowDir.begin(), flowDir.end(), flowDir.begin(), ::toupper);

    if (flowDir.find("RL") != std::string::npos ||
        flowDir.find("AP_RL") != std::string::npos) {
        return VelocityComponent::Vx;
    }
    if (flowDir.find("AP") != std::string::npos) {
        return VelocityComponent::Vy;
    }
    if (flowDir.find("FH") != std::string::npos ||
        flowDir.find("SI") != std::string::npos) {
        return VelocityComponent::Vz;
    }

    // Fallback: check phase image
    if (imageType.find("\\P\\") != std::string::npos ||
        imageType.find("VELOCITY") != std::string::npos ||
        imageType.find("PHASE") != std::string::npos) {
        // Cannot determine specific direction — default to Vx
        return VelocityComponent::Vx;
    }

    return VelocityComponent::Magnitude;
}

int SiemensFlowParser::extractPhaseIndex(
    const itk::MetaDataDictionary& dictionary) const {
    // (0020,9057) In-Stack Position Number (Enhanced MR)
    auto stackPos = getMetaString(dictionary, "0020|9057");
    if (!stackPos.empty()) {
        return parseInt(stackPos);
    }

    // Fallback: (0020,0013) Instance Number for temporal ordering
    auto instanceNum = getMetaString(dictionary, "0020|0013");
    if (!instanceNum.empty()) {
        return parseInt(instanceNum);
    }

    return 0;
}

double SiemensFlowParser::extractTriggerTime(
    const itk::MetaDataDictionary& dictionary) const {
    // (0018,1060) Trigger Time
    auto triggerStr = getMetaString(dictionary, "0018|1060");
    if (!triggerStr.empty()) {
        return parseDouble(triggerStr);
    }

    // (0020,9153) Nominal Cardiac Trigger Delay Time (Enhanced MR)
    auto nominalStr = getMetaString(dictionary, "0020|9153");
    if (!nominalStr.empty()) {
        return parseDouble(nominalStr);
    }

    return 0.0;
}

}  // namespace dicom_viewer::services

#include "services/flow/vendor_parsers/ge_flow_parser.hpp"

#include <algorithm>
#include <string>

namespace {

std::string getMetaString(const itk::MetaDataDictionary& dict,
                          const std::string& key) {
    std::string value;
    itk::ExposeMetaData<std::string>(dict, key, value);
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

FlowVendorType GEFlowParser::vendorType() const noexcept {
    return FlowVendorType::GE;
}

std::string GEFlowParser::expectedIODType() const {
    return "MR Image Storage";
}

double GEFlowParser::extractVENC(
    const itk::MetaDataDictionary& dictionary) const {
    // GE private tag (0019,10cc) contains VENC info
    auto geVenc = getMetaString(dictionary, "0019|10cc");
    if (!geVenc.empty()) {
        return std::abs(parseDouble(geVenc));
    }

    // Fallback: standard (0018,9197)
    auto vencStr = getMetaString(dictionary, "0018|9197");
    if (!vencStr.empty()) {
        return std::abs(parseDouble(vencStr));
    }

    return 0.0;
}

VelocityComponent GEFlowParser::classifyComponent(
    const itk::MetaDataDictionary& dictionary) const {
    // (0008,0008) Image Type
    auto imageType = getMetaString(dictionary, "0008|0008");
    std::transform(imageType.begin(), imageType.end(), imageType.begin(),
                   ::toupper);

    // GE magnitude images
    if (imageType.find("\\M\\") != std::string::npos ||
        imageType.find("MAG") != std::string::npos) {
        return VelocityComponent::Magnitude;
    }

    // GE uses series description for velocity direction
    auto seriesDesc = getMetaString(dictionary, "0008|103e");
    std::transform(seriesDesc.begin(), seriesDesc.end(), seriesDesc.begin(),
                   ::toupper);

    if (seriesDesc.find("FLOW_RL") != std::string::npos ||
        seriesDesc.find("_X") != std::string::npos) {
        return VelocityComponent::Vx;
    }
    if (seriesDesc.find("FLOW_AP") != std::string::npos ||
        seriesDesc.find("_Y") != std::string::npos) {
        return VelocityComponent::Vy;
    }
    if (seriesDesc.find("FLOW_SI") != std::string::npos ||
        seriesDesc.find("_Z") != std::string::npos) {
        return VelocityComponent::Vz;
    }

    // GE private (0019,10cc) may encode direction
    auto gePrivate = getMetaString(dictionary, "0019|10cc");
    if (!gePrivate.empty()) {
        // Phase image but direction unknown
        return VelocityComponent::Vx;
    }

    // Fallback: check for phase indicator
    if (imageType.find("\\P\\") != std::string::npos ||
        imageType.find("PHASE") != std::string::npos ||
        imageType.find("VELOCITY") != std::string::npos) {
        return VelocityComponent::Vx;
    }

    return VelocityComponent::Magnitude;
}

int GEFlowParser::extractPhaseIndex(
    const itk::MetaDataDictionary& dictionary) const {
    // GE: (0020,0013) Instance Number — primary ordering
    auto instanceNum = getMetaString(dictionary, "0020|0013");
    if (!instanceNum.empty()) {
        return parseInt(instanceNum);
    }

    return 0;
}

double GEFlowParser::extractTriggerTime(
    const itk::MetaDataDictionary& dictionary) const {
    // (0018,1060) Trigger Time
    auto triggerStr = getMetaString(dictionary, "0018|1060");
    if (!triggerStr.empty()) {
        return parseDouble(triggerStr);
    }

    return 0.0;
}

}  // namespace dicom_viewer::services

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

#include "services/flow/vendor_parsers/philips_flow_parser.hpp"

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

FlowVendorType PhilipsFlowParser::vendorType() const noexcept {
    return FlowVendorType::Philips;
}

std::string PhilipsFlowParser::expectedIODType() const {
    return "MR Image Storage";
}

double PhilipsFlowParser::extractVENC(
    const itk::MetaDataDictionary& dictionary) const {
    // (0018,9197) Velocity Encoding Minimum Value — standard tag
    auto vencStr = getMetaString(dictionary, "0018|9197");
    if (!vencStr.empty()) {
        return std::abs(parseDouble(vencStr));
    }

    // Philips private: (2001,101a) may contain VENC
    auto privateVenc = getMetaString(dictionary, "2001|101a");
    if (!privateVenc.empty()) {
        return std::abs(parseDouble(privateVenc));
    }

    return 0.0;
}

VelocityComponent PhilipsFlowParser::classifyComponent(
    const itk::MetaDataDictionary& dictionary) const {
    // (0008,0008) Image Type
    auto imageType = getMetaString(dictionary, "0008|0008");
    std::transform(imageType.begin(), imageType.end(), imageType.begin(),
                   ::toupper);

    // Philips uses Image Type to distinguish magnitude vs phase
    if (imageType.find("\\M\\") != std::string::npos ||
        imageType.find("\\M_") != std::string::npos ||
        imageType.find("FFE_M") != std::string::npos) {
        return VelocityComponent::Magnitude;
    }

    // Philips private tag (2005,1071) Scale Slope for velocity data
    auto scaleSlopeStr = getMetaString(dictionary, "2005|1071");

    // Philips private tag (2001,100a) Slice Number / orientation info
    auto sliceInfo = getMetaString(dictionary, "2001|100a");
    std::transform(sliceInfo.begin(), sliceInfo.end(), sliceInfo.begin(),
                   ::toupper);

    // Philips encodes direction in series description or private tags
    auto seriesDesc = getMetaString(dictionary, "0008|103e");
    std::transform(seriesDesc.begin(), seriesDesc.end(), seriesDesc.begin(),
                   ::toupper);

    if (seriesDesc.find("_RL") != std::string::npos ||
        seriesDesc.find("_LR") != std::string::npos ||
        seriesDesc.find("VX") != std::string::npos) {
        return VelocityComponent::Vx;
    }
    if (seriesDesc.find("_AP") != std::string::npos ||
        seriesDesc.find("_PA") != std::string::npos ||
        seriesDesc.find("VY") != std::string::npos) {
        return VelocityComponent::Vy;
    }
    if (seriesDesc.find("_FH") != std::string::npos ||
        seriesDesc.find("_HF") != std::string::npos ||
        seriesDesc.find("VZ") != std::string::npos) {
        return VelocityComponent::Vz;
    }

    // Fallback: phase image without direction info
    if (imageType.find("\\P\\") != std::string::npos ||
        imageType.find("PHASE") != std::string::npos) {
        return VelocityComponent::Vx;
    }

    return VelocityComponent::Magnitude;
}

int PhilipsFlowParser::extractPhaseIndex(
    const itk::MetaDataDictionary& dictionary) const {
    // (0020,0013) Instance Number — Philips uses classic ordering
    auto instanceNum = getMetaString(dictionary, "0020|0013");
    if (!instanceNum.empty()) {
        return parseInt(instanceNum);
    }

    return 0;
}

double PhilipsFlowParser::extractTriggerTime(
    const itk::MetaDataDictionary& dictionary) const {
    // (0018,1060) Trigger Time
    auto triggerStr = getMetaString(dictionary, "0018|1060");
    if (!triggerStr.empty()) {
        return parseDouble(triggerStr);
    }

    return 0.0;
}

}  // namespace dicom_viewer::services

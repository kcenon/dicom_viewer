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


/**
 * @file philips_flow_parser.hpp
 * @brief Philips-specific 4D Flow DICOM parser implementing IVendorFlowParser
 * @details Parses Philips Classic MR IOD format with scale slope in DICOM tag
 *          (2005,1071) and phase index in (2001,100a). Implements vendor-
 *          specific VENC, component classification, phase indexing, and
 *          trigger time extraction.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include "services/flow/vendor_parsers/i_vendor_flow_parser.hpp"

namespace dicom_viewer::services {

/**
 * @brief Philips-specific 4D Flow DICOM parser
 *
 * Handles Classic MR IOD with scale slope in (2005,1071)
 * and phase index in (2001,100a).
 *
 * @trace SRS-FR-043
 */
class PhilipsFlowParser : public IVendorFlowParser {
public:
    [[nodiscard]] FlowVendorType vendorType() const noexcept override;
    [[nodiscard]] std::string expectedIODType() const override;
    [[nodiscard]] double extractVENC(
        const itk::MetaDataDictionary& dictionary) const override;
    [[nodiscard]] VelocityComponent classifyComponent(
        const itk::MetaDataDictionary& dictionary) const override;
    [[nodiscard]] int extractPhaseIndex(
        const itk::MetaDataDictionary& dictionary) const override;
    [[nodiscard]] double extractTriggerTime(
        const itk::MetaDataDictionary& dictionary) const override;
};

}  // namespace dicom_viewer::services

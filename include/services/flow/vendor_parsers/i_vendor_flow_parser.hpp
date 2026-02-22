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
 * @file i_vendor_flow_parser.hpp
 * @brief Interface for vendor-specific 4D Flow DICOM parsing
 * @details Strategy pattern interface for vendor-specific parsing logic. Each
 *          vendor (Siemens, Philips, GE) implements methods for velocity
 *          tags extraction, VENC retrieval, velocity component classification,
 *          phase indexing, and trigger time extraction.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <string>
#include <vector>

#include <itkGDCMImageIO.h>
#include <itkMetaDataObject.h>

#include "services/flow/flow_dicom_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Interface for vendor-specific 4D Flow DICOM parsing
 *
 * Strategy pattern interface — each vendor (Siemens, Philips, GE)
 * implements its own parsing logic for velocity tags, VENC extraction,
 * and velocity component classification.
 *
 * @trace SRS-FR-043
 */
class IVendorFlowParser {
public:
    virtual ~IVendorFlowParser() = default;

    /**
     * @brief Get the vendor type this parser handles
     */
    [[nodiscard]] virtual FlowVendorType vendorType() const noexcept = 0;

    /**
     * @brief Get expected IOD type name for this vendor
     */
    [[nodiscard]] virtual std::string expectedIODType() const = 0;

    /**
     * @brief Extract VENC value from DICOM metadata
     * @param dictionary DICOM metadata dictionary
     * @return VENC in cm/s, or 0.0 if not found
     */
    [[nodiscard]] virtual double extractVENC(
        const itk::MetaDataDictionary& dictionary) const = 0;

    /**
     * @brief Classify velocity component from DICOM metadata
     * @param dictionary DICOM metadata dictionary
     * @return Classified velocity component (Magnitude, Vx, Vy, Vz)
     */
    [[nodiscard]] virtual VelocityComponent classifyComponent(
        const itk::MetaDataDictionary& dictionary) const = 0;

    /**
     * @brief Extract cardiac phase index from DICOM metadata
     * @param dictionary DICOM metadata dictionary
     * @return Phase index (0-based)
     */
    [[nodiscard]] virtual int extractPhaseIndex(
        const itk::MetaDataDictionary& dictionary) const = 0;

    /**
     * @brief Extract trigger time from DICOM metadata
     * @param dictionary DICOM metadata dictionary
     * @return Trigger time in ms
     */
    [[nodiscard]] virtual double extractTriggerTime(
        const itk::MetaDataDictionary& dictionary) const = 0;
};

}  // namespace dicom_viewer::services

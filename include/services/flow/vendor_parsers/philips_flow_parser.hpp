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

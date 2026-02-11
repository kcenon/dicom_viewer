#pragma once

#include "services/flow/vendor_parsers/i_vendor_flow_parser.hpp"

namespace dicom_viewer::services {

/**
 * @brief GE-specific 4D Flow DICOM parser
 *
 * Handles Classic MR IOD with velocity/VENC in (0019,10cc)
 * and instance number-based phase ordering.
 *
 * @trace SRS-FR-043
 */
class GEFlowParser : public IVendorFlowParser {
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

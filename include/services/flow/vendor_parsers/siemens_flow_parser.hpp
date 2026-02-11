#pragma once

#include "services/flow/vendor_parsers/i_vendor_flow_parser.hpp"

namespace dicom_viewer::services {

/**
 * @brief Siemens-specific 4D Flow DICOM parser
 *
 * Handles Enhanced MR IOD with velocity info in (0051,1014)
 * and VENC in (0018,9197).
 *
 * @trace SRS-FR-043
 */
class SiemensFlowParser : public IVendorFlowParser {
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

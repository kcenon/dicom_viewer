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

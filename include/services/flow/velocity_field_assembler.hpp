#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkVectorImage.h>

#include "services/flow/flow_dicom_types.hpp"

namespace dicom_viewer::services {

/// Common ITK type aliases for 4D Flow operations
using FloatImage3D = itk::Image<float, 3>;
using VectorImage3D = itk::VectorImage<float, 3>;

/**
 * @brief Assembled velocity field for one cardiac phase
 *
 * Contains a 3-component vector field (Vx, Vy, Vz) and the corresponding
 * magnitude image for a single cardiac phase in the 4D Flow sequence.
 *
 * @trace SRS-FR-044
 */
struct VelocityPhase {
    VectorImage3D::Pointer velocityField;   ///< 3-component (Vx, Vy, Vz)
    FloatImage3D::Pointer magnitudeImage;   ///< Magnitude image
    int phaseIndex = 0;                     ///< 0-based cardiac phase index
    double triggerTime = 0.0;               ///< ms from R-wave
};

/**
 * @brief Assembles 3D velocity vector fields from parsed 4D Flow DICOM frames
 *
 * Takes the frame matrix produced by FlowDicomParser and constructs a temporal
 * sequence of 3D velocity vector fields using ITK image types.
 *
 * Pipeline:
 * @code
 * FlowSeriesInfo (from FlowDicomParser)
 *   → Read scalar DICOM volumes (Magnitude, Vx, Vy, Vz)
 *   → Apply VENC scaling to convert pixel values to velocity (cm/s)
 *   → Compose 3 scalar volumes into VectorImage3D
 *   → Output VelocityPhase per cardiac phase
 * @endcode
 *
 * @trace SRS-FR-044
 */
class VelocityFieldAssembler {
public:
    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    VelocityFieldAssembler();
    ~VelocityFieldAssembler();

    // Non-copyable, movable
    VelocityFieldAssembler(const VelocityFieldAssembler&) = delete;
    VelocityFieldAssembler& operator=(const VelocityFieldAssembler&) = delete;
    VelocityFieldAssembler(VelocityFieldAssembler&&) noexcept;
    VelocityFieldAssembler& operator=(VelocityFieldAssembler&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Assemble all cardiac phases into velocity fields
     *
     * Reads pixel data for each phase and component, applies VENC scaling,
     * and composes vector fields.
     *
     * @param seriesInfo Parsed series from FlowDicomParser
     * @return Vector of VelocityPhase on success, FlowError on failure
     */
    [[nodiscard]] std::expected<std::vector<VelocityPhase>, FlowError>
    assembleAllPhases(const FlowSeriesInfo& seriesInfo) const;

    /**
     * @brief Assemble a single cardiac phase (on-demand loading)
     *
     * @param seriesInfo Parsed series from FlowDicomParser
     * @param phaseIndex 0-based cardiac phase index
     * @return VelocityPhase on success, FlowError on failure
     */
    [[nodiscard]] std::expected<VelocityPhase, FlowError>
    assemblePhase(const FlowSeriesInfo& seriesInfo, int phaseIndex) const;

    /**
     * @brief Apply VENC scaling to convert pixel values to velocity
     *
     * @param pixelValue Raw pixel value from DICOM
     * @param venc Velocity encoding value (cm/s)
     * @param maxPixelValue Maximum possible pixel value (2^bitsStored or 2^(bitsStored-1))
     * @param isSigned Whether the phase data uses signed representation
     * @return Velocity in cm/s
     */
    [[nodiscard]] static float applyVENCScaling(
        float pixelValue, double venc, int maxPixelValue, bool isSigned);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

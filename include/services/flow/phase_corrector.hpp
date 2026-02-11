#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <vector>

#include <itkImage.h>
#include <itkPoint.h>
#include <itkVectorImage.h>

#include "services/flow/flow_dicom_types.hpp"
#include "services/flow/velocity_field_assembler.hpp"

namespace dicom_viewer::services {

/**
 * @brief Configuration for phase correction algorithms
 *
 * @trace SRS-FR-045
 */
struct PhaseCorrectionConfig {
    bool enableAliasingUnwrap = true;
    bool enableEddyCurrentCorrection = true;
    bool enableMaxwellCorrection = true;
    int polynomialOrder = 2;           ///< Order for eddy current polynomial fit
    double aliasingThreshold = 0.8;    ///< Fraction of VENC for jump detection

    [[nodiscard]] bool isValid() const noexcept {
        return polynomialOrder >= 1 && polynomialOrder <= 4 &&
               aliasingThreshold > 0.0 && aliasingThreshold <= 1.0;
    }
};

/// Mask image type for stationary tissue detection
using MaskImage3D = itk::Image<unsigned char, 3>;

/**
 * @brief Applies corrections to raw 4D Flow velocity data
 *
 * Corrects three types of systematic errors in phase-contrast MRI:
 * 1. Velocity aliasing (phase wrapping beyond VENC)
 * 2. Eddy current background phase offsets
 * 3. Maxwell term (concomitant gradient) errors
 *
 * Each correction can be independently enabled/disabled via PhaseCorrectionConfig.
 * Corrections are applied to copies — original data is not modified.
 *
 * @trace SRS-FR-045
 */
class PhaseCorrector {
public:
    /// Progress callback (0.0 to 1.0)
    using ProgressCallback = std::function<void(double progress)>;

    PhaseCorrector();
    ~PhaseCorrector();

    // Non-copyable, movable
    PhaseCorrector(const PhaseCorrector&) = delete;
    PhaseCorrector& operator=(const PhaseCorrector&) = delete;
    PhaseCorrector(PhaseCorrector&&) noexcept;
    PhaseCorrector& operator=(PhaseCorrector&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Apply all enabled corrections to a velocity phase
     *
     * Creates a corrected copy of the input phase. The original is not modified.
     *
     * @param phase Input velocity phase
     * @param venc Velocity encoding value (cm/s), uniform across components
     * @param config Correction options
     * @return Corrected VelocityPhase on success, FlowError on failure
     */
    [[nodiscard]] std::expected<VelocityPhase, FlowError>
    correctPhase(const VelocityPhase& phase, double venc,
                 const PhaseCorrectionConfig& config) const;

    /**
     * @brief Unwrap velocity aliasing artifacts in a vector velocity field
     *
     * Detects and corrects phase wraps where velocity exceeds VENC,
     * using neighbor-based jump detection.
     *
     * @param velocity 3-component velocity field (modified in-place)
     * @param venc Velocity encoding value (cm/s)
     * @param threshold Fraction of VENC for jump detection (0.0-1.0)
     */
    static void unwrapAliasing(VectorImage3D::Pointer velocity,
                               double venc, double threshold);

    /**
     * @brief Correct eddy current background phase from magnitude reference
     *
     * Fits a polynomial surface to velocity values in stationary tissue
     * regions and subtracts the fitted background from the entire volume.
     *
     * @param velocity 3-component velocity field (modified in-place)
     * @param magnitude Magnitude image for tissue detection
     * @param polynomialOrder Polynomial order (1-4)
     */
    static void correctEddyCurrent(VectorImage3D::Pointer velocity,
                                   FloatImage3D::Pointer magnitude,
                                   int polynomialOrder);

    /**
     * @brief Create binary mask of stationary tissue from magnitude image
     *
     * Uses Otsu thresholding to identify low-signal regions (air/background)
     * and returns a mask of stationary tissue.
     *
     * @param magnitude Magnitude image
     * @return Binary mask (255 = stationary tissue, 0 = background/vessel)
     */
    [[nodiscard]] static MaskImage3D::Pointer createStationaryMask(
        FloatImage3D::Pointer magnitude);

    /**
     * @brief Fit polynomial to scalar field within masked region
     *
     * Performs least-squares fitting of a polynomial surface to velocity
     * values at locations identified by the mask.
     *
     * @param scalarField Single velocity component image
     * @param mask Binary mask (non-zero = include in fitting)
     * @param order Polynomial order (1 = linear, 2 = quadratic)
     * @return Polynomial coefficients
     */
    [[nodiscard]] static std::vector<double> fitPolynomialBackground(
        FloatImage3D::Pointer scalarField,
        MaskImage3D::Pointer mask, int order);

    /**
     * @brief Evaluate polynomial at a 3D point
     *
     * @param coeffs Polynomial coefficients from fitPolynomialBackground
     * @param x Normalized x coordinate
     * @param y Normalized y coordinate
     * @param z Normalized z coordinate
     * @param order Polynomial order
     * @return Evaluated polynomial value
     */
    [[nodiscard]] static double evaluatePolynomial(
        const std::vector<double>& coeffs,
        double x, double y, double z, int order);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

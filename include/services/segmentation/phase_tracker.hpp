#pragma once

#include "services/segmentation/threshold_segmenter.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkVector.h>

namespace dicom_viewer::services {

/**
 * @brief Temporal mask propagation across cardiac phases
 *
 * Propagates a segmentation mask from a reference phase to all other
 * phases using deformable image registration. The displacement field
 * between consecutive phases is computed via ITK Demons registration,
 * and the mask is warped accordingly.
 *
 * Propagation is bidirectional:
 *   Reference → phase+1 → phase+2 → ... → last (forward)
 *   Reference → phase-1 → phase-2 → ... → first (backward)
 *
 * @trace SRS-FR-047
 */
class PhaseTracker {
public:
    using FloatImage3D = itk::Image<float, 3>;
    using LabelMapType = itk::Image<uint8_t, 3>;
    using DisplacementFieldType =
        itk::Image<itk::Vector<float, 3>, 3>;

    /// Progress callback: (currentPhase, totalPhases) → void
    using ProgressCallback = std::function<void(int current, int total)>;

    /**
     * @brief Configuration for phase tracking
     */
    struct TrackingConfig {
        int referencePhase = 0;            ///< Index of the reference phase
        double smoothingSigma = 1.0;       ///< Gaussian smoothing sigma (mm)
        int registrationIterations = 50;   ///< Demons registration iterations
        bool applyMorphologicalClosing = true;  ///< Fill small gaps after warping
        int closingRadius = 1;             ///< Structuring element radius (voxels)
        double volumeDeviationThreshold = 0.20; ///< Flag phases with >20% volume change
    };

    /**
     * @brief Per-phase tracking result
     */
    struct PhaseResult {
        LabelMapType::Pointer mask;        ///< Propagated mask for this phase
        double volumeRatio = 1.0;          ///< Volume relative to reference (1.0 = same)
        bool qualityWarning = false;       ///< true if volume deviation > threshold
    };

    /**
     * @brief Complete tracking result across all phases
     */
    struct TrackingResult {
        std::vector<PhaseResult> phases;   ///< One per input phase
        int referencePhase = 0;            ///< Index of the reference phase
        int warningCount = 0;              ///< Number of phases with quality warnings
    };

    PhaseTracker();
    ~PhaseTracker();

    PhaseTracker(const PhaseTracker&) = delete;
    PhaseTracker& operator=(const PhaseTracker&) = delete;
    PhaseTracker(PhaseTracker&&) noexcept;
    PhaseTracker& operator=(PhaseTracker&&) noexcept;

    /**
     * @brief Set progress callback
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Propagate mask from reference phase to all phases
     *
     * @param referenceMask Segmentation mask at the reference phase
     * @param magnitudePhases Magnitude images for all cardiac phases
     * @param config Tracking configuration
     * @return Tracking result with masks for all phases
     */
    [[nodiscard]] std::expected<TrackingResult, SegmentationError>
    propagateMask(
        LabelMapType::Pointer referenceMask,
        const std::vector<FloatImage3D::Pointer>& magnitudePhases,
        const TrackingConfig& config) const;

    // =====================================================================
    // Low-level methods (public for testing)
    // =====================================================================

    /**
     * @brief Compute displacement field between two phases
     *
     * Uses ITK Demons registration to find the deformation that maps
     * fixedImage to movingImage.
     *
     * @param fixedImage Target phase magnitude
     * @param movingImage Source phase magnitude
     * @param iterations Number of registration iterations
     * @param smoothingSigma Gaussian smoothing sigma
     * @return Displacement field or error
     */
    [[nodiscard]] static std::expected<DisplacementFieldType::Pointer,
                                       SegmentationError>
    computeDisplacementField(
        FloatImage3D::Pointer fixedImage,
        FloatImage3D::Pointer movingImage,
        int iterations,
        double smoothingSigma);

    /**
     * @brief Warp a label map using a displacement field
     *
     * Uses nearest-neighbor interpolation to preserve label values.
     *
     * @param mask Input label map
     * @param displacementField Deformation field
     * @return Warped label map or error
     */
    [[nodiscard]] static std::expected<LabelMapType::Pointer,
                                       SegmentationError>
    warpMask(LabelMapType::Pointer mask,
             DisplacementFieldType::Pointer displacementField);

    /**
     * @brief Apply morphological closing to fill small gaps
     *
     * @param mask Input label map
     * @param radius Structuring element radius in voxels
     * @return Closed label map
     */
    [[nodiscard]] static LabelMapType::Pointer
    applyClosing(LabelMapType::Pointer mask, int radius);

    /**
     * @brief Count non-zero voxels in a label map
     */
    [[nodiscard]] static size_t countNonZeroVoxels(
        LabelMapType::Pointer mask);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

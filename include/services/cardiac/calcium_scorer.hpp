#pragma once

#include <expected>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <itkImage.h>

#include "services/cardiac/cardiac_types.hpp"

namespace dicom_viewer::services {

/**
 * @brief Agatston coronary artery calcium scorer
 *
 * Computes Agatston, volume, and mass calcium scores from non-contrast
 * cardiac CT acquisitions. Implements the standard Agatston algorithm:
 * threshold at 130 HU, connected component analysis, density-weighted
 * per-slice area scoring.
 *
 * Supports per-artery scoring (LAD, LCx, RCA, LM) when artery ROI
 * masks are provided, and cardiovascular risk classification following
 * established clinical thresholds.
 *
 * @example
 * @code
 * CalciumScorer scorer;
 * auto result = scorer.computeAgatston(cardiacVolume, 3.0);
 * if (result) {
 *     auto& score = result.value();
 *     std::cout << "Agatston: " << score.totalAgatston
 *               << ", Risk: " << score.riskCategory << std::endl;
 * }
 * @endcode
 *
 * @trace SRS-FR-052, SDS-MOD-009
 */
class CalciumScorer {
public:
    CalciumScorer();
    ~CalciumScorer();

    // Non-copyable, movable
    CalciumScorer(const CalciumScorer&) = delete;
    CalciumScorer& operator=(const CalciumScorer&) = delete;
    CalciumScorer(CalciumScorer&&) noexcept;
    CalciumScorer& operator=(CalciumScorer&&) noexcept;

    /**
     * @brief Compute Agatston calcium score from non-contrast cardiac CT
     *
     * Applies the standard Agatston algorithm:
     * 1. Threshold at >= 130 HU
     * 2. Connected component labeling
     * 3. Filter components < 1mm²
     * 4. Compute per-slice area × density weight per component
     * 5. Sum across all lesions
     *
     * @param image Non-contrast cardiac CT volume
     * @param sliceThickness Slice thickness in mm (for area/volume calculation)
     * @return CalciumScoreResult with total Agatston, lesion details, risk category
     * @trace SRS-FR-052
     */
    [[nodiscard]] std::expected<CalciumScoreResult, CardiacError>
    computeAgatston(itk::Image<short, 3>::Pointer image,
                    double sliceThickness) const;

    /**
     * @brief Compute volume score (sum of calcified voxel volumes)
     *
     * @param image Non-contrast cardiac CT volume
     * @return Volume score in mm³, or error
     */
    [[nodiscard]] std::expected<double, CardiacError>
    computeVolumeScore(itk::Image<short, 3>::Pointer image) const;

    /**
     * @brief Compute mass score with calibration factor
     *
     * Mass = sum over all calcified voxels of (HU * calibrationFactor * voxelVolume)
     *
     * @param image Non-contrast cardiac CT volume
     * @param calibrationFactor Calibration factor from phantom (mg/mL per HU)
     * @return Mass score in mg, or error
     */
    [[nodiscard]] std::expected<double, CardiacError>
    computeMassScore(itk::Image<short, 3>::Pointer image,
                     double calibrationFactor) const;

    /**
     * @brief Classify cardiovascular risk based on Agatston score
     *
     * | Score     | Category    |
     * |-----------|-------------|
     * | 0         | None        |
     * | 1-10      | Minimal     |
     * | 11-100    | Mild        |
     * | 101-400   | Moderate    |
     * | > 400     | Severe      |
     *
     * @param agatstonScore Total Agatston score
     * @return Risk category string
     */
    [[nodiscard]] static std::string classifyRisk(double agatstonScore);

    /**
     * @brief Assign Agatston density weight factor based on peak HU
     *
     * @param peakHU Peak Hounsfield Unit value
     * @return Weight factor (1-4), or 0 if below threshold
     */
    [[nodiscard]] static int densityWeightFactor(short peakHU);

    /**
     * @brief Assign lesions to coronary arteries based on ROI masks
     *
     * Matches lesion centroids to artery ROI masks. Each artery mask
     * defines the spatial region of one vessel.
     *
     * @param lesions Lesions to assign (modified in place)
     * @param arteryROIs Map of artery name to binary ROI mask
     */
    static void assignToArteries(
        std::vector<CalcifiedLesion>& lesions,
        const std::map<std::string, itk::Image<uint8_t, 3>::Pointer>& arteryROIs);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

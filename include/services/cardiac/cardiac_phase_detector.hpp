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
 * @file cardiac_phase_detector.hpp
 * @brief ECG-gated cardiac phase detection and separation
 * @details Detects ECG-gated cardiac CT/MR series and separates multi-phase
 *          data into individual cardiac phase volumes. Supports Enhanced
 *          and Classic DICOM IODs with automatic best diastolic/systolic
 *          phase selection and ejection fraction estimation.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <itkImage.h>

#include "services/cardiac/cardiac_types.hpp"
#include "services/enhanced_dicom/enhanced_dicom_types.hpp"

namespace dicom_viewer::core {
struct DicomMetadata;
struct SliceInfo;
}  // namespace dicom_viewer::core

namespace dicom_viewer::services {

/**
 * @brief ECG-gated cardiac phase detection and separation
 *
 * Detects ECG-gated cardiac CT/MR series and separates multi-phase
 * data into individual cardiac phase volumes. Supports both Enhanced
 * (multi-frame) and Classic (single-frame) DICOM IODs.
 *
 * Key capabilities:
 * - ECG gating detection via Cardiac Sync Technique or Trigger Time tags
 * - Phase separation by trigger time clustering or nominal percentage
 * - Best diastolic (70-80% R-R) and systolic (35-45% R-R) phase selection
 * - Per-phase 3D volume assembly
 * - Ejection fraction estimation from end-diastolic/end-systolic volumes
 *
 * @example
 * @code
 * CardiacPhaseDetector detector;
 * if (detector.detectECGGating(enhancedSeriesInfo)) {
 *     auto result = detector.separatePhases(enhancedSeriesInfo);
 *     if (result) {
 *         int bestPhase = detector.selectBestPhase(result.value());
 *     }
 * }
 * @endcode
 *
 * @trace SRS-FR-050, SDS-MOD-009
 */
class CardiacPhaseDetector {
public:
    CardiacPhaseDetector();
    ~CardiacPhaseDetector();

    // Non-copyable, movable
    CardiacPhaseDetector(const CardiacPhaseDetector&) = delete;
    CardiacPhaseDetector& operator=(const CardiacPhaseDetector&) = delete;
    CardiacPhaseDetector(CardiacPhaseDetector&&) noexcept;
    CardiacPhaseDetector& operator=(CardiacPhaseDetector&&) noexcept;

    /**
     * @brief Detect if an Enhanced DICOM series is ECG-gated
     *
     * Checks for Cardiac Synchronization Technique in functional groups
     * and/or presence of Trigger Time / Nominal Percentage per frame.
     *
     * @param series Parsed Enhanced DICOM series info
     * @return true if ECG gating is detected
     */
    [[nodiscard]] bool detectECGGating(const EnhancedSeriesInfo& series) const;

    /**
     * @brief Detect if a Classic DICOM series is ECG-gated
     *
     * Checks for Trigger Time (0018,1060) tag presence in the metadata.
     *
     * @param classicSeries Classic DICOM metadata for each slice
     * @return true if ECG gating is detected
     */
    [[nodiscard]] bool detectECGGating(
        const std::vector<core::DicomMetadata>& classicSeries) const;

    /**
     * @brief Separate Enhanced DICOM frames into cardiac phases
     *
     * Groups frames by trigger time or nominal percentage, sorts
     * each group spatially, and assigns phase labels.
     *
     * @param series Parsed Enhanced DICOM series info
     * @return Phase separation result on success
     * @trace SRS-FR-050
     */
    [[nodiscard]] std::expected<CardiacPhaseResult, CardiacError>
    separatePhases(const EnhancedSeriesInfo& series) const;

    /**
     * @brief Select best phase for a given clinical target
     *
     * @param result Phase separation result
     * @param target Diastole (70-80%), Systole (35-45%), or Custom
     * @param customPercentage Target % for Custom mode (ignored otherwise)
     * @return Index into result.phases, or -1 if not found
     */
    [[nodiscard]] int selectBestPhase(
        const CardiacPhaseResult& result,
        PhaseTarget target = PhaseTarget::Diastole,
        double customPercentage = 75.0) const;

    /**
     * @brief Build 3D volumes for each cardiac phase
     *
     * Assembles per-phase 3D ITK volumes from the source Enhanced
     * DICOM file using frame indices from each phase.
     *
     * @param result Phase separation result
     * @param seriesInfo Original series info (for pixel data access)
     * @return Vector of (phase info, volume) pairs
     * @trace SRS-FR-050
     */
    [[nodiscard]] std::expected<
        std::vector<std::pair<CardiacPhaseInfo, itk::Image<short, 3>::Pointer>>,
        CardiacError>
    buildPhaseVolumes(
        const CardiacPhaseResult& result,
        const EnhancedSeriesInfo& seriesInfo) const;

    /**
     * @brief Estimate ejection fraction from ED and ES volumes
     *
     * Uses a simple volume-based method: EF = (EDV - ESV) / EDV * 100
     * where volume is estimated by counting voxels above a threshold
     * and multiplying by voxel volume.
     *
     * @param endDiastolic End-diastolic 3D volume
     * @param endSystolic End-systolic 3D volume
     * @param huThreshold HU threshold for blood pool segmentation (default: 200)
     * @return Ejection fraction in percent (0-100), or error
     */
    [[nodiscard]] std::expected<double, CardiacError>
    estimateEjectionFraction(
        itk::Image<short, 3>::Pointer endDiastolic,
        itk::Image<short, 3>::Pointer endSystolic,
        short huThreshold = 200) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Helper methods for phase separation strategies
    [[nodiscard]] std::expected<CardiacPhaseResult, CardiacError>
    buildResultFromNominalGroups(
        const std::vector<std::pair<double, std::vector<int>>>& groups,
        const EnhancedSeriesInfo& series) const;

    [[nodiscard]] std::expected<CardiacPhaseResult, CardiacError>
    buildResultFromTriggerTimeClusters(
        const std::vector<std::vector<int>>& clusters,
        const EnhancedSeriesInfo& series) const;

    [[nodiscard]] std::vector<std::pair<int, std::vector<int>>>
    groupByTemporalIndex(const EnhancedSeriesInfo& series) const;

    [[nodiscard]] std::expected<CardiacPhaseResult, CardiacError>
    buildResultFromTemporalGroups(
        const std::vector<std::pair<int, std::vector<int>>>& groups,
        const EnhancedSeriesInfo& series) const;

    void estimateRRInterval(CardiacPhaseResult& result,
                            const EnhancedSeriesInfo& series) const;
};

}  // namespace dicom_viewer::services

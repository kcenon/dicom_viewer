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

#include "services/cardiac/cardiac_phase_detector.hpp"
#include "services/enhanced_dicom/frame_extractor.hpp"
#include "core/dicom_loader.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>
#include <sstream>

#include <kcenon/common/logging/log_macros.h>

#include <itkImageRegionIterator.h>

namespace dicom_viewer::services {

namespace {

/// Generate phase label from nominal percentage
std::string makePhaseLabel(double nominalPct) {
    std::ostringstream oss;
    oss << static_cast<int>(std::round(nominalPct)) << "% "
        << (nominalPct >= 50.0 ? "diastole" : "systole");
    return oss.str();
}

/// Find the phase index closest to a target percentage
int findClosestPhase(const std::vector<CardiacPhaseInfo>& phases,
                     double targetPct)
{
    if (phases.empty()) {
        return -1;
    }
    int bestIdx = 0;
    double bestDist = std::abs(phases[0].nominalPercentage - targetPct);
    for (int i = 1; i < static_cast<int>(phases.size()); ++i) {
        double dist = std::abs(phases[i].nominalPercentage - targetPct);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

/// Cluster trigger times with a tolerance to group frames into phases
std::vector<std::vector<int>> clusterByTriggerTime(
    const std::vector<EnhancedFrameInfo>& frames,
    double toleranceMs)
{
    // Collect (trigger time, frame index) pairs
    struct TriggerEntry {
        double triggerTime;
        int frameIndex;
    };

    std::vector<TriggerEntry> entries;
    entries.reserve(frames.size());
    for (const auto& frame : frames) {
        if (frame.triggerTime.has_value()) {
            entries.push_back({frame.triggerTime.value(), frame.frameIndex});
        }
    }

    if (entries.empty()) {
        return {};
    }

    // Sort by trigger time
    std::sort(entries.begin(), entries.end(),
              [](const TriggerEntry& a, const TriggerEntry& b) {
                  return a.triggerTime < b.triggerTime;
              });

    // Cluster: group consecutive entries within tolerance
    std::vector<std::vector<int>> clusters;
    double clusterStart = entries[0].triggerTime;
    clusters.push_back({entries[0].frameIndex});

    for (size_t i = 1; i < entries.size(); ++i) {
        if (entries[i].triggerTime - clusterStart <= toleranceMs) {
            clusters.back().push_back(entries[i].frameIndex);
        } else {
            clusterStart = entries[i].triggerTime;
            clusters.push_back({entries[i].frameIndex});
        }
    }

    return clusters;
}

/// Group frames by exact nominal percentage match
std::vector<std::pair<double, std::vector<int>>> groupByNominalPercentage(
    const std::vector<EnhancedFrameInfo>& frames)
{
    // Collect unique percentages preserving order
    std::vector<double> uniquePcts;
    std::vector<std::pair<double, std::vector<int>>> groups;

    for (const auto& frame : frames) {
        auto it = frame.dimensionIndices.find(cardiac_tag::NominalPercentage);
        double pct = -1.0;
        if (it != frame.dimensionIndices.end()) {
            pct = static_cast<double>(it->second);
        } else if (frame.triggerTime.has_value()) {
            // Fall back to trigger time if nominal percentage not available
            continue;
        } else {
            continue;
        }

        bool found = false;
        for (auto& [gPct, gFrames] : groups) {
            if (std::abs(gPct - pct) < 0.5) {
                gFrames.push_back(frame.frameIndex);
                found = true;
                break;
            }
        }
        if (!found) {
            groups.push_back({pct, {frame.frameIndex}});
        }
    }

    // Sort groups by percentage
    std::sort(groups.begin(), groups.end(),
              [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });

    return groups;
}

/// Compute mean trigger time for a set of frame indices
double meanTriggerTime(const std::vector<EnhancedFrameInfo>& frames,
                       const std::vector<int>& indices)
{
    double sum = 0.0;
    int count = 0;
    for (int idx : indices) {
        for (const auto& f : frames) {
            if (f.frameIndex == idx && f.triggerTime.has_value()) {
                sum += f.triggerTime.value();
                ++count;
                break;
            }
        }
    }
    return count > 0 ? sum / count : 0.0;
}

/// Sort frame indices by spatial position (z-axis)
void sortFramesBySpatialPosition(
    std::vector<int>& frameIndices,
    const std::vector<EnhancedFrameInfo>& allFrames)
{
    std::sort(frameIndices.begin(), frameIndices.end(),
              [&allFrames](int a, int b) {
                  const EnhancedFrameInfo* fa = nullptr;
                  const EnhancedFrameInfo* fb = nullptr;
                  for (const auto& f : allFrames) {
                      if (f.frameIndex == a) fa = &f;
                      if (f.frameIndex == b) fb = &f;
                      if (fa && fb) break;
                  }
                  if (fa && fb) {
                      return fa->imagePosition[2] < fb->imagePosition[2];
                  }
                  return a < b;
              });
}

}  // anonymous namespace

class CardiacPhaseDetector::Impl {
public:
    FrameExtractor frameExtractor;
};

CardiacPhaseDetector::CardiacPhaseDetector()
    : impl_(std::make_unique<Impl>()) {}

CardiacPhaseDetector::~CardiacPhaseDetector() = default;

CardiacPhaseDetector::CardiacPhaseDetector(CardiacPhaseDetector&&) noexcept
    = default;
CardiacPhaseDetector& CardiacPhaseDetector::operator=(
    CardiacPhaseDetector&&) noexcept = default;

bool CardiacPhaseDetector::detectECGGating(
    const EnhancedSeriesInfo& series) const
{
    // Check 1: Does any frame have a trigger time?
    for (const auto& frame : series.frames) {
        if (frame.triggerTime.has_value()) {
            LOG_DEBUG(std::format("ECG gating detected: frame {} has trigger "
                                 "time {}ms",
                                 frame.frameIndex,
                                 frame.triggerTime.value()));
            return true;
        }
    }

    // Check 2: Does any frame have a temporal position index?
    for (const auto& frame : series.frames) {
        if (frame.temporalPositionIndex.has_value()) {
            LOG_DEBUG(std::format("ECG gating detected: frame {} has temporal "
                                 "position index {}",
                                 frame.frameIndex,
                                 frame.temporalPositionIndex.value()));
            return true;
        }
    }

    // Check 3: Do frames have NominalPercentage dimension index?
    for (const auto& frame : series.frames) {
        if (frame.dimensionIndices.count(cardiac_tag::NominalPercentage) > 0) {
            LOG_DEBUG(std::format("ECG gating detected: frame {} has nominal "
                                 "percentage dimension index",
                                 frame.frameIndex));
            return true;
        }
    }

    LOG_DEBUG("No ECG gating detected in Enhanced series");
    return false;
}

bool CardiacPhaseDetector::detectECGGating(
    const std::vector<core::DicomMetadata>& classicSeries) const
{
    // Classic IOD: check if the series has cardiac-related modality
    // and sufficient number of slices (multi-phase = N * slicesPerPhase)
    // For now, check if the series is CT with multiple slices
    // that could represent a cardiac-gated acquisition.
    //
    // Note: In Classic IOD, trigger time is typically in the top-level
    // dataset but not exposed in DicomMetadata. This is a simplified check.
    if (classicSeries.empty()) {
        return false;
    }

    // If modality is CT and we have many slices, it could be cardiac
    // A true implementation would read Trigger Time from the DICOM file
    const auto& first = classicSeries.front();
    if (first.modality != "CT" && first.modality != "MR") {
        return false;
    }

    LOG_DEBUG(std::format("Classic IOD cardiac detection: {} slices, "
                         "modality={}",
                         classicSeries.size(), first.modality));

    // With Classic IOD, we can't easily detect ECG gating without
    // reading the raw DICOM tags. Return false for now; callers should
    // prefer Enhanced IOD detection.
    return false;
}

std::expected<CardiacPhaseResult, CardiacError>
CardiacPhaseDetector::separatePhases(const EnhancedSeriesInfo& series) const
{
    LOG_INFO(std::format("Separating cardiac phases for {} frames",
                        series.frames.size()));

    if (series.frames.empty()) {
        return std::unexpected(CardiacError{
            CardiacError::Code::MissingTemporalData,
            "No frames available for phase separation"
        });
    }

    // Strategy 1: Group by nominal percentage (preferred, more precise)
    auto nominalGroups = groupByNominalPercentage(series.frames);
    if (nominalGroups.size() >= 2) {
        return buildResultFromNominalGroups(nominalGroups, series);
    }

    // Strategy 2: Cluster by trigger time
    auto clusters = clusterByTriggerTime(
        series.frames, cardiac_constants::kTriggerTimeToleranceMs);
    if (clusters.size() >= 2) {
        return buildResultFromTriggerTimeClusters(clusters, series);
    }

    // Strategy 3: Use temporal position index from DimensionIndex
    auto temporalGroups = groupByTemporalIndex(series);
    if (temporalGroups.size() >= 2) {
        return buildResultFromTemporalGroups(temporalGroups, series);
    }

    return std::unexpected(CardiacError{
        CardiacError::Code::NotCardiacGated,
        "Could not detect multiple cardiac phases (found " +
            std::to_string(std::max({nominalGroups.size(), clusters.size(),
                                     temporalGroups.size()})) +
            " groups)"
    });
}

int CardiacPhaseDetector::selectBestPhase(
    const CardiacPhaseResult& result,
    PhaseTarget target,
    double customPercentage) const
{
    if (result.phases.empty()) {
        return -1;
    }

    double targetPct = customPercentage;
    switch (target) {
        case PhaseTarget::Diastole:
            targetPct = cardiac_constants::kDiastoleOptimal;
            break;
        case PhaseTarget::Systole:
            targetPct = cardiac_constants::kSystoleOptimal;
            break;
        case PhaseTarget::Custom:
            break;
    }

    int idx = findClosestPhase(result.phases, targetPct);
    LOG_INFO(std::format("Best phase for target {}%: phase {} ({})",
                        targetPct, idx,
                        idx >= 0 ? result.phases[idx].phaseLabel : "none"));
    return idx;
}

std::expected<
    std::vector<std::pair<CardiacPhaseInfo, itk::Image<short, 3>::Pointer>>,
    CardiacError>
CardiacPhaseDetector::buildPhaseVolumes(
    const CardiacPhaseResult& result,
    const EnhancedSeriesInfo& seriesInfo) const
{
    LOG_INFO(std::format("Building {} phase volumes from {}",
                        result.phases.size(), seriesInfo.filePath));

    std::vector<std::pair<CardiacPhaseInfo, itk::Image<short, 3>::Pointer>>
        volumes;
    volumes.reserve(result.phases.size());

    for (const auto& phase : result.phases) {
        if (phase.frameIndices.empty()) {
            LOG_WARNING(std::format("Phase {} has no frames, skipping",
                                phase.phaseIndex));
            continue;
        }

        auto volumeResult = impl_->frameExtractor.assembleVolumeFromFrames(
            seriesInfo.filePath, seriesInfo, phase.frameIndices);

        if (!volumeResult) {
            return std::unexpected(CardiacError{
                CardiacError::Code::VolumeAssemblyFailed,
                "Failed to assemble volume for phase " +
                    std::to_string(phase.phaseIndex) + ": " +
                    volumeResult.error().toString()
            });
        }

        volumes.emplace_back(phase, volumeResult.value());
    }

    LOG_INFO(std::format("Built {} phase volumes successfully", volumes.size()));
    return volumes;
}

std::expected<double, CardiacError>
CardiacPhaseDetector::estimateEjectionFraction(
    itk::Image<short, 3>::Pointer endDiastolic,
    itk::Image<short, 3>::Pointer endSystolic,
    short huThreshold) const
{
    if (!endDiastolic || !endSystolic) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null volume pointer for EF estimation"
        });
    }

    // Compute voxel volume in mL (mm^3 -> mL)
    auto edSpacing = endDiastolic->GetSpacing();
    double edVoxelVol = edSpacing[0] * edSpacing[1] * edSpacing[2] / 1000.0;

    auto esSpacing = endSystolic->GetSpacing();
    double esVoxelVol = esSpacing[0] * esSpacing[1] * esSpacing[2] / 1000.0;

    // Count voxels above threshold (blood pool)
    long edCount = 0;
    {
        itk::ImageRegionIterator<itk::Image<short, 3>> it(
            endDiastolic, endDiastolic->GetLargestPossibleRegion());
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() > huThreshold) {
                ++edCount;
            }
        }
    }

    long esCount = 0;
    {
        itk::ImageRegionIterator<itk::Image<short, 3>> it(
            endSystolic, endSystolic->GetLargestPossibleRegion());
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() > huThreshold) {
                ++esCount;
            }
        }
    }

    double edVolume = edCount * edVoxelVol;  // mL
    double esVolume = esCount * esVoxelVol;  // mL

    LOG_INFO(std::format("EF estimation: EDV={:.1f}mL, ESV={:.1f}mL",
                        edVolume, esVolume));

    if (edVolume <= 0.0) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "End-diastolic volume is zero or negative"
        });
    }

    double ef = (edVolume - esVolume) / edVolume * 100.0;
    LOG_INFO(std::format("Estimated EF: {:.1f}%", ef));
    return ef;
}

// --- Private helper methods (accessed via friendship or inline) ---

std::expected<CardiacPhaseResult, CardiacError>
CardiacPhaseDetector::buildResultFromNominalGroups(
    const std::vector<std::pair<double, std::vector<int>>>& groups,
    const EnhancedSeriesInfo& series) const
{
    CardiacPhaseResult result;
    result.slicesPerPhase = static_cast<int>(groups[0].second.size());

    for (int i = 0; i < static_cast<int>(groups.size()); ++i) {
        CardiacPhaseInfo phase;
        phase.phaseIndex = i;
        phase.nominalPercentage = groups[i].first;
        phase.phaseLabel = makePhaseLabel(groups[i].first);
        phase.frameIndices = groups[i].second;

        // Compute mean trigger time if available
        phase.triggerTime = meanTriggerTime(series.frames, phase.frameIndices);

        // Sort frames spatially within each phase
        sortFramesBySpatialPosition(phase.frameIndices, series.frames);

        result.phases.push_back(std::move(phase));
    }

    // Estimate R-R interval from max trigger time
    estimateRRInterval(result, series);

    // Select best phases
    result.bestDiastolicPhase = findClosestPhase(
        result.phases, cardiac_constants::kDiastoleOptimal);
    result.bestSystolicPhase = findClosestPhase(
        result.phases, cardiac_constants::kSystoleOptimal);

    LOG_INFO(std::format("Phase separation by nominal %: {} phases, "
                        "{} slices/phase, R-R={:.0f}ms",
                        result.phases.size(), result.slicesPerPhase,
                        result.rrInterval));

    return result;
}

std::expected<CardiacPhaseResult, CardiacError>
CardiacPhaseDetector::buildResultFromTriggerTimeClusters(
    const std::vector<std::vector<int>>& clusters,
    const EnhancedSeriesInfo& series) const
{
    CardiacPhaseResult result;
    result.slicesPerPhase = static_cast<int>(clusters[0].size());

    // First pass: compute mean trigger times
    std::vector<double> meanTriggers;
    for (const auto& cluster : clusters) {
        meanTriggers.push_back(meanTriggerTime(series.frames, cluster));
    }

    // Estimate R-R interval from the range of trigger times
    double minTrigger = *std::min_element(meanTriggers.begin(),
                                          meanTriggers.end());
    double maxTrigger = *std::max_element(meanTriggers.begin(),
                                          meanTriggers.end());
    // R-R interval is approximately the full cycle
    // If triggers span from 0 to T, R-R ≈ T * N/(N-1) for N phases
    int numPhases = static_cast<int>(clusters.size());
    if (numPhases > 1 && maxTrigger > minTrigger) {
        result.rrInterval = (maxTrigger - minTrigger) *
                            numPhases / (numPhases - 1);
    }

    for (int i = 0; i < numPhases; ++i) {
        CardiacPhaseInfo phase;
        phase.phaseIndex = i;
        phase.triggerTime = meanTriggers[i];
        phase.frameIndices = clusters[i];

        // Convert trigger time to nominal percentage
        if (result.rrInterval > 0.0) {
            phase.nominalPercentage =
                (phase.triggerTime - minTrigger) / result.rrInterval * 100.0;
        } else {
            phase.nominalPercentage =
                static_cast<double>(i) / numPhases * 100.0;
        }

        phase.phaseLabel = makePhaseLabel(phase.nominalPercentage);
        sortFramesBySpatialPosition(phase.frameIndices, series.frames);
        result.phases.push_back(std::move(phase));
    }

    result.bestDiastolicPhase = findClosestPhase(
        result.phases, cardiac_constants::kDiastoleOptimal);
    result.bestSystolicPhase = findClosestPhase(
        result.phases, cardiac_constants::kSystoleOptimal);

    LOG_INFO(std::format("Phase separation by trigger time: {} phases, "
                        "{} slices/phase, R-R={:.0f}ms",
                        result.phases.size(), result.slicesPerPhase,
                        result.rrInterval));

    return result;
}

std::vector<std::pair<int, std::vector<int>>>
CardiacPhaseDetector::groupByTemporalIndex(
    const EnhancedSeriesInfo& series) const
{
    std::vector<std::pair<int, std::vector<int>>> groups;

    for (const auto& frame : series.frames) {
        int tempIdx = -1;
        if (frame.temporalPositionIndex.has_value()) {
            tempIdx = frame.temporalPositionIndex.value();
        } else {
            auto it = frame.dimensionIndices.find(
                dimension_tag::TemporalPositionIndex);
            if (it != frame.dimensionIndices.end()) {
                tempIdx = it->second;
            }
        }

        if (tempIdx < 0) continue;

        bool found = false;
        for (auto& [gIdx, gFrames] : groups) {
            if (gIdx == tempIdx) {
                gFrames.push_back(frame.frameIndex);
                found = true;
                break;
            }
        }
        if (!found) {
            groups.push_back({tempIdx, {frame.frameIndex}});
        }
    }

    // Sort by temporal index
    std::sort(groups.begin(), groups.end(),
              [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });

    return groups;
}

std::expected<CardiacPhaseResult, CardiacError>
CardiacPhaseDetector::buildResultFromTemporalGroups(
    const std::vector<std::pair<int, std::vector<int>>>& groups,
    const EnhancedSeriesInfo& series) const
{
    CardiacPhaseResult result;
    result.slicesPerPhase = static_cast<int>(groups[0].second.size());
    int numPhases = static_cast<int>(groups.size());

    for (int i = 0; i < numPhases; ++i) {
        CardiacPhaseInfo phase;
        phase.phaseIndex = i;
        phase.frameIndices = groups[i].second;

        // Distribute evenly across R-R interval
        phase.nominalPercentage =
            static_cast<double>(i) / numPhases * 100.0;
        phase.triggerTime = meanTriggerTime(series.frames, phase.frameIndices);
        phase.phaseLabel = makePhaseLabel(phase.nominalPercentage);

        sortFramesBySpatialPosition(phase.frameIndices, series.frames);
        result.phases.push_back(std::move(phase));
    }

    estimateRRInterval(result, series);

    result.bestDiastolicPhase = findClosestPhase(
        result.phases, cardiac_constants::kDiastoleOptimal);
    result.bestSystolicPhase = findClosestPhase(
        result.phases, cardiac_constants::kSystoleOptimal);

    LOG_INFO(std::format("Phase separation by temporal index: {} phases, "
                        "{} slices/phase",
                        result.phases.size(), result.slicesPerPhase));

    return result;
}

void CardiacPhaseDetector::estimateRRInterval(
    CardiacPhaseResult& result,
    const EnhancedSeriesInfo& series) const
{
    if (result.rrInterval > 0.0) return;

    // Try to estimate from max trigger time
    double maxTrigger = 0.0;
    for (const auto& frame : series.frames) {
        if (frame.triggerTime.has_value()) {
            maxTrigger = std::max(maxTrigger, frame.triggerTime.value());
        }
    }

    int numPhases = static_cast<int>(result.phases.size());
    if (maxTrigger > 0.0 && numPhases > 1) {
        // Assume trigger times span most of the R-R interval
        result.rrInterval = maxTrigger * numPhases / (numPhases - 1);
    } else if (numPhases > 0) {
        // Default assumption: 75 bpm → R-R = 800ms
        result.rrInterval = 800.0;
    }
}

}  // namespace dicom_viewer::services

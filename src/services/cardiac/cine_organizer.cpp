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

#include "services/cardiac/cine_organizer.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <utility>

#include <itkCastImageFilter.h>
#include <itkImage.h>
#include <itkImageRegionIterator.h>

#include "core/dicom_loader.hpp"
#include "services/enhanced_dicom/enhanced_dicom_types.hpp"
#include "services/enhanced_dicom/frame_extractor.hpp"
#include "services/flow/temporal_navigator.hpp"
#include "services/flow/velocity_field_assembler.hpp"

namespace dicom_viewer::services {

namespace {

/// Tolerance for grouping trigger times into the same phase (ms)
constexpr double kTriggerTimeGroupTolerance = 5.0;

/// Tolerance for grouping slice locations (mm)
constexpr double kSliceLocationTolerance = 0.5;

/// Minimum number of temporal phases to qualify as cine
constexpr int kMinCinePhases = 2;

/// Compute slice normal from direction cosines [rowX,rowY,rowZ,colX,colY,colZ]
std::array<double, 3> computeSliceNormal(const std::array<double, 6>& orient) {
    return {
        orient[1] * orient[5] - orient[2] * orient[4],
        orient[2] * orient[3] - orient[0] * orient[5],
        orient[0] * orient[4] - orient[1] * orient[3]
    };
}

/// Compute the dot product along the slice normal for a given position
double projectAlongNormal(const std::array<double, 3>& position,
                          const std::array<double, 3>& normal) {
    return position[0] * normal[0]
         + position[1] * normal[1]
         + position[2] * normal[2];
}

/// Group trigger times into clusters within tolerance
std::vector<double> clusterTriggerTimes(
    const std::vector<double>& triggerTimes, double tolerance)
{
    if (triggerTimes.empty()) return {};

    auto sorted = triggerTimes;
    std::sort(sorted.begin(), sorted.end());

    // Remove duplicates within tolerance
    std::vector<double> unique;
    unique.push_back(sorted.front());
    for (size_t i = 1; i < sorted.size(); ++i) {
        if (sorted[i] - unique.back() > tolerance) {
            unique.push_back(sorted[i]);
        }
    }
    return unique;
}

/// Find which cluster a trigger time belongs to
int findClusterIndex(double triggerTime,
                     const std::vector<double>& clusters,
                     double tolerance)
{
    for (size_t i = 0; i < clusters.size(); ++i) {
        if (std::abs(triggerTime - clusters[i]) <= tolerance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/// Group frames by temporal position index
std::map<int, std::vector<int>> groupFramesByTemporalIndex(
    const EnhancedSeriesInfo& series)
{
    std::map<int, std::vector<int>> groups;
    for (const auto& frame : series.frames) {
        int temporalIdx = 0;
        if (frame.temporalPositionIndex.has_value()) {
            temporalIdx = frame.temporalPositionIndex.value();
        } else if (frame.triggerTime.has_value()) {
            // Use trigger time as a fallback grouping key
            // Round to nearest ms for grouping
            temporalIdx = static_cast<int>(
                std::round(frame.triggerTime.value()));
        }
        groups[temporalIdx].push_back(frame.frameIndex);
    }
    return groups;
}

/// Sort frame indices by spatial position along slice normal
void sortFramesBySpatialPosition(std::vector<int>& frameIndices,
                                  const EnhancedSeriesInfo& series)
{
    if (frameIndices.empty()) return;

    // Compute slice normal from first frame
    const auto& firstFrame = series.frames[frameIndices[0]];
    auto normal = computeSliceNormal(firstFrame.imageOrientation);

    std::sort(frameIndices.begin(), frameIndices.end(),
        [&](int a, int b) {
            double posA = projectAlongNormal(
                series.frames[a].imagePosition, normal);
            double posB = projectAlongNormal(
                series.frames[b].imagePosition, normal);
            return posA < posB;
        });
}

/// Count unique slice locations from frame positions
int countUniqueSliceLocations(const std::vector<int>& frameIndices,
                               const EnhancedSeriesInfo& series)
{
    if (frameIndices.empty()) return 0;

    const auto& firstFrame = series.frames[frameIndices[0]];
    auto normal = computeSliceNormal(firstFrame.imageOrientation);

    std::vector<double> positions;
    for (int idx : frameIndices) {
        positions.push_back(
            projectAlongNormal(series.frames[idx].imagePosition, normal));
    }
    std::sort(positions.begin(), positions.end());

    int uniqueCount = 1;
    for (size_t i = 1; i < positions.size(); ++i) {
        if (positions[i] - positions[i - 1] > kSliceLocationTolerance) {
            ++uniqueCount;
        }
    }
    return uniqueCount;
}

/// Convert short 3D image to float 3D image
FloatImage3D::Pointer shortToFloat(itk::Image<short, 3>::Pointer shortImage) {
    using CastFilter = itk::CastImageFilter<itk::Image<short, 3>, FloatImage3D>;
    auto caster = CastFilter::New();
    caster->SetInput(shortImage);
    caster->Update();
    return caster->GetOutput();
}

/// Check for cine-related keywords in series description
bool hasCineKeywords(const std::string& desc) {
    if (desc.empty()) return false;
    // Convert to lowercase for case-insensitive matching
    std::string lower = desc;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower.find("cine") != std::string::npos
        || lower.find("retro") != std::string::npos
        || lower.find("realtime") != std::string::npos;
}

}  // namespace

// =============================================================================
// CineOrganizer::Impl
// =============================================================================

class CineOrganizer::Impl {
public:
    Impl() = default;
};

// =============================================================================
// CineOrganizer Public Interface
// =============================================================================

CineOrganizer::CineOrganizer()
    : impl_(std::make_unique<Impl>()) {}

CineOrganizer::~CineOrganizer() = default;

CineOrganizer::CineOrganizer(CineOrganizer&&) noexcept = default;
CineOrganizer& CineOrganizer::operator=(CineOrganizer&&) noexcept = default;

// --- Cine Detection ---

bool CineOrganizer::detectCineSeries(const EnhancedSeriesInfo& series) const {
    // Rule 1: Modality must be MR
    if (series.modality != "MR") {
        return false;
    }

    // Rule 2: Must have multiple frames
    if (series.numberOfFrames < kMinCinePhases) {
        return false;
    }

    // Rule 3: Check for temporal dimension presence
    bool hasTemporalIndex = false;
    bool hasTriggerTime = false;
    std::set<int> uniqueTemporalIndices;
    std::set<int> uniqueTriggerTimeKeys;

    for (const auto& frame : series.frames) {
        if (frame.temporalPositionIndex.has_value()) {
            hasTemporalIndex = true;
            uniqueTemporalIndices.insert(
                frame.temporalPositionIndex.value());
        }
        if (frame.triggerTime.has_value()) {
            hasTriggerTime = true;
            // Round to nearest ms for uniqueness check
            uniqueTriggerTimeKeys.insert(
                static_cast<int>(std::round(frame.triggerTime.value())));
        }
    }

    if (!hasTemporalIndex && !hasTriggerTime) {
        return false;
    }

    // Rule 4: Must have multiple unique temporal positions
    int temporalPositions = 0;
    if (hasTemporalIndex) {
        temporalPositions = static_cast<int>(uniqueTemporalIndices.size());
    } else {
        // Cluster trigger times to count distinct phases
        std::vector<double> allTriggerTimes;
        for (const auto& frame : series.frames) {
            if (frame.triggerTime.has_value()) {
                allTriggerTimes.push_back(frame.triggerTime.value());
            }
        }
        auto clusters = clusterTriggerTimes(
            allTriggerTimes, kTriggerTimeGroupTolerance);
        temporalPositions = static_cast<int>(clusters.size());
    }

    if (temporalPositions < kMinCinePhases) {
        return false;
    }

    // Rule 5: Frame count should be consistent with temporal x spatial
    // (This is a soft check — we allow some tolerance)
    if (hasTemporalIndex) {
        int expectedSpatialCount =
            series.numberOfFrames / temporalPositions;
        if (expectedSpatialCount < 1) {
            return false;
        }
        // Frames should divide evenly
        if (series.numberOfFrames % temporalPositions != 0) {
            return false;
        }
    }

    return true;
}

bool CineOrganizer::detectCineSeries(
    const std::vector<core::DicomMetadata>& metadata,
    const std::vector<core::SliceInfo>& slices) const
{
    if (metadata.empty() || slices.empty()) {
        return false;
    }

    // Rule 1: Modality must be MR
    if (metadata[0].modality != "MR") {
        return false;
    }

    // Rule 2: All files must belong to the same series
    const auto& seriesUid = metadata[0].seriesInstanceUid;
    for (const auto& m : metadata) {
        if (m.seriesInstanceUid != seriesUid) {
            return false;
        }
    }

    // Rule 3: Must have multiple files (at least 2 phases × 1 slice)
    if (metadata.size() < static_cast<size_t>(kMinCinePhases)) {
        return false;
    }

    // For Classic DICOM, we detect cine by checking if there are multiple
    // files at the same slice location (different trigger times).
    // We use instance number grouping as a heuristic: if instance numbers
    // repeat pattern (1,2,...,N,1,2,...,N) it indicates multi-phase.
    //
    // A more robust check requires Trigger Time tag which is not in the
    // basic DicomMetadata struct. Use hasCineKeywords as fallback.
    if (hasCineKeywords(metadata[0].seriesDescription)) {
        return true;
    }

    // Group by spatial location to detect temporal repetition
    std::map<int, int> locationCounts;  // rounded Z → count
    for (const auto& slice : slices) {
        int zKey = static_cast<int>(
            std::round(slice.sliceLocation / kSliceLocationTolerance));
        locationCounts[zKey]++;
    }

    // If any location has multiple files, it indicates multi-phase
    for (const auto& [loc, count] : locationCounts) {
        if (count >= kMinCinePhases) {
            return true;
        }
    }

    return false;
}

// --- Phase Organization (Enhanced DICOM) ---

std::expected<CineVolumeSeries, CardiacError>
CineOrganizer::organizePhases(const EnhancedSeriesInfo& series) const {
    if (!detectCineSeries(series)) {
        return std::unexpected(CardiacError{
            CardiacError::Code::NotCardiacGated,
            "Series is not a cine MRI acquisition"});
    }

    // Group frames by temporal position
    auto temporalGroups = groupFramesByTemporalIndex(series);

    if (temporalGroups.empty()) {
        return std::unexpected(CardiacError{
            CardiacError::Code::MissingTemporalData,
            "No temporal groups found in cine series"});
    }

    // Build CineSeriesInfo
    CineSeriesInfo info;
    info.phaseCount = static_cast<int>(temporalGroups.size());
    info.seriesDescription = series.seriesDescription;

    // Detect orientation from first frame
    if (!series.frames.empty()) {
        info.orientation = detectOrientation(
            series.frames[0].imageOrientation,
            series.seriesDescription);
    }

    // Sort each temporal group spatially and count slice locations
    std::vector<std::vector<int>> sortedGroups;
    for (auto& [temporalKey, frameIndices] : temporalGroups) {
        sortFramesBySpatialPosition(frameIndices, series);
        sortedGroups.push_back(std::move(frameIndices));
    }

    // Count unique slices from first group
    if (!sortedGroups.empty()) {
        info.sliceCount = countUniqueSliceLocations(
            sortedGroups[0], series);
    }

    // Verify consistent frame count across phases
    for (size_t i = 1; i < sortedGroups.size(); ++i) {
        if (sortedGroups[i].size() != sortedGroups[0].size()) {
            return std::unexpected(CardiacError{
                CardiacError::Code::InconsistentFrameCount,
                "Phase " + std::to_string(i) + " has "
                + std::to_string(sortedGroups[i].size())
                + " frames, expected "
                + std::to_string(sortedGroups[0].size())});
        }
    }

    // Collect trigger times for each phase
    for (const auto& group : sortedGroups) {
        if (!group.empty()) {
            const auto& frame = series.frames[group[0]];
            double tt = frame.triggerTime.value_or(0.0);
            info.triggerTimes.push_back(tt);
        }
    }

    // Compute temporal resolution
    if (info.triggerTimes.size() >= 2) {
        // Average difference between consecutive trigger times
        double totalDiff = 0.0;
        for (size_t i = 1; i < info.triggerTimes.size(); ++i) {
            totalDiff += info.triggerTimes[i] - info.triggerTimes[i - 1];
        }
        info.temporalResolution =
            totalDiff / static_cast<double>(info.triggerTimes.size() - 1);
    }

    // Assemble 3D volumes per phase using FrameExtractor
    FrameExtractor extractor;
    CineVolumeSeries result;
    result.info = info;

    for (const auto& group : sortedGroups) {
        auto volume = extractor.assembleVolumeFromFrames(
            series.filePath, series, group);
        if (!volume) {
            return std::unexpected(CardiacError{
                CardiacError::Code::VolumeAssemblyFailed,
                "Failed to assemble volume: "
                + volume.error().toString()});
        }
        result.phaseVolumes.push_back(std::move(volume.value()));
    }

    return result;
}

// --- Phase Organization (Classic DICOM) ---

std::expected<CineVolumeSeries, CardiacError>
CineOrganizer::organizePhases(
    const std::vector<std::string>& dicomFiles,
    const std::vector<core::DicomMetadata>& metadata,
    const std::vector<core::SliceInfo>& slices) const
{
    if (dicomFiles.size() != metadata.size()
        || dicomFiles.size() != slices.size()
        || dicomFiles.empty())
    {
        return std::unexpected(CardiacError{
            CardiacError::Code::MissingTemporalData,
            "Invalid input: file/metadata/slice arrays must be "
            "non-empty and equal length"});
    }

    if (!detectCineSeries(metadata, slices)) {
        return std::unexpected(CardiacError{
            CardiacError::Code::NotCardiacGated,
            "Series is not a cine MRI acquisition"});
    }

    // Group by spatial location
    struct FileEntry {
        size_t index;
        double slicePosition;
        int instanceNumber;
    };

    // Compute slice normal from first slice
    auto normal = computeSliceNormal(slices[0].imageOrientation);

    std::vector<FileEntry> entries;
    for (size_t i = 0; i < dicomFiles.size(); ++i) {
        entries.push_back({
            i,
            projectAlongNormal(slices[i].imagePosition, normal),
            slices[i].instanceNumber
        });
    }

    // Group by slice position
    std::sort(entries.begin(), entries.end(),
        [](const FileEntry& a, const FileEntry& b) {
            return a.slicePosition < b.slicePosition;
        });

    // Cluster by location
    std::vector<std::vector<size_t>> locationGroups;
    std::vector<size_t> currentGroup;
    currentGroup.push_back(entries[0].index);
    double currentPos = entries[0].slicePosition;

    for (size_t i = 1; i < entries.size(); ++i) {
        if (entries[i].slicePosition - currentPos > kSliceLocationTolerance) {
            locationGroups.push_back(std::move(currentGroup));
            currentGroup.clear();
            currentPos = entries[i].slicePosition;
        }
        currentGroup.push_back(entries[i].index);
    }
    if (!currentGroup.empty()) {
        locationGroups.push_back(std::move(currentGroup));
    }

    // Each location group has files from different phases
    // Sort by instance number within each location to get phase ordering
    int phaseCount = 0;
    for (auto& group : locationGroups) {
        std::sort(group.begin(), group.end(),
            [&slices](size_t a, size_t b) {
                return slices[a].instanceNumber < slices[b].instanceNumber;
            });
        phaseCount = std::max(phaseCount, static_cast<int>(group.size()));
    }

    if (phaseCount < kMinCinePhases) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InsufficientPhases,
            "Only " + std::to_string(phaseCount)
            + " temporal phases found, need at least "
            + std::to_string(kMinCinePhases)});
    }

    // Build phase-indexed structure: phases[phaseIdx] = list of file indices
    std::vector<std::vector<size_t>> phaseFileIndices(phaseCount);
    for (const auto& locGroup : locationGroups) {
        for (size_t p = 0; p < locGroup.size() && p < static_cast<size_t>(phaseCount); ++p) {
            phaseFileIndices[p].push_back(locGroup[p]);
        }
    }

    // Build CineSeriesInfo
    CineSeriesInfo info;
    info.phaseCount = phaseCount;
    info.sliceCount = static_cast<int>(locationGroups.size());
    info.seriesDescription = metadata[0].seriesDescription;
    info.orientation = detectOrientation(
        slices[0].imageOrientation, metadata[0].seriesDescription);

    // Assemble volumes for each phase using DicomLoader
    core::DicomLoader loader;
    CineVolumeSeries result;
    result.info = info;

    for (int phase = 0; phase < phaseCount; ++phase) {
        // Build SliceInfo vector for this phase, sorted by position
        std::vector<core::SliceInfo> phaseSlices;
        for (size_t fileIdx : phaseFileIndices[phase]) {
            phaseSlices.push_back(slices[fileIdx]);
        }

        // Sort by spatial position
        std::sort(phaseSlices.begin(), phaseSlices.end(),
            [&normal](const core::SliceInfo& a, const core::SliceInfo& b) {
                return projectAlongNormal(a.imagePosition, normal)
                     < projectAlongNormal(b.imagePosition, normal);
            });

        auto volume = loader.loadCTSeries(phaseSlices);
        if (!volume) {
            return std::unexpected(CardiacError{
                CardiacError::Code::VolumeAssemblyFailed,
                "Failed to assemble Classic DICOM volume for phase "
                + std::to_string(phase)});
        }
        result.phaseVolumes.push_back(std::move(volume.value()));
    }

    return result;
}

// --- Orientation Detection ---

CineOrientation CineOrganizer::detectOrientation(
    const std::array<double, 6>& orientation,
    const std::string& seriesDescription) const
{
    auto normal = computeSliceNormal(orientation);

    // Normalize
    double mag = std::sqrt(
        normal[0] * normal[0]
      + normal[1] * normal[1]
      + normal[2] * normal[2]);
    if (mag < 1e-10) {
        return CineOrientation::Unknown;
    }
    normal[0] /= mag;
    normal[1] /= mag;
    normal[2] /= mag;

    // Compute absolute components of the normal
    double absX = std::abs(normal[0]);
    double absY = std::abs(normal[1]);
    double absZ = std::abs(normal[2]);

    // Short axis: normal is approximately along the long axis of LV,
    // which typically has a significant component along the body's
    // superior-inferior axis. In patient coordinate system, this is
    // often a mix of Y and Z.
    // A purely transverse slice has normal ≈ (0, 0, ±1)
    // Short axis is typically oblique but close to transverse
    constexpr double kAxialThreshold = 0.7;

    if (absZ > kAxialThreshold) {
        // Nearly transverse — likely short axis
        // Check series description for confirmation
        std::string lower = seriesDescription;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower.find("4ch") != std::string::npos
            || lower.find("4 ch") != std::string::npos
            || lower.find("four") != std::string::npos)
        {
            return CineOrientation::FourChamber;
        }

        return CineOrientation::ShortAxis;
    }

    // Long-axis views have significant oblique components
    // Differentiate using series description keywords
    std::string lower = seriesDescription;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower.find("2ch") != std::string::npos
        || lower.find("2 ch") != std::string::npos
        || lower.find("two") != std::string::npos)
    {
        return CineOrientation::TwoChamber;
    }
    if (lower.find("3ch") != std::string::npos
        || lower.find("3 ch") != std::string::npos
        || lower.find("three") != std::string::npos
        || lower.find("lvot") != std::string::npos)
    {
        return CineOrientation::ThreeChamber;
    }
    if (lower.find("4ch") != std::string::npos
        || lower.find("4 ch") != std::string::npos
        || lower.find("four") != std::string::npos)
    {
        return CineOrientation::FourChamber;
    }

    // Sagittal-like normal could be 2CH
    if (absX > kAxialThreshold) {
        return CineOrientation::TwoChamber;
    }

    // Default: unknown if purely oblique without descriptor keywords
    return CineOrientation::Unknown;
}

// --- Short-Axis Stack Reconstruction ---

std::expected<CineVolumeSeries, CardiacError>
CineOrganizer::reconstructShortAxisStack(
    const EnhancedSeriesInfo& series) const
{
    // This is essentially the same as organizePhases for Enhanced DICOM,
    // but explicitly validates short-axis orientation
    auto result = organizePhases(series);
    if (!result) {
        return result;
    }

    // Verify orientation is short axis
    if (result->info.orientation != CineOrientation::ShortAxis
        && result->info.orientation != CineOrientation::Unknown)
    {
        return std::unexpected(CardiacError{
            CardiacError::Code::InconsistentFrameCount,
            "Series orientation is "
            + cineOrientationToString(result->info.orientation)
            + ", expected Short Axis for SA stack reconstruction"});
    }

    // Override orientation to SA
    result->info.orientation = CineOrientation::ShortAxis;
    return result;
}

// --- TemporalNavigator Integration ---

std::unique_ptr<TemporalNavigator>
CineOrganizer::createCineNavigator(const CineVolumeSeries& cineSeries) const {
    auto navigator = std::make_unique<TemporalNavigator>();

    navigator->initialize(
        cineSeries.info.phaseCount,
        cineSeries.info.temporalResolution,
        /* cacheWindowSize = */ std::min(cineSeries.info.phaseCount, 10));

    // Capture phase volumes for the loader callback
    // The loader converts short→float for TemporalNavigator compatibility
    auto phaseVolumes = std::make_shared<
        std::vector<itk::Image<short, 3>::Pointer>>(
            cineSeries.phaseVolumes);
    auto triggerTimes = std::make_shared<std::vector<double>>(
        cineSeries.info.triggerTimes);

    navigator->setPhaseLoader(
        [phaseVolumes, triggerTimes](int phaseIndex)
            -> std::expected<VelocityPhase, FlowError>
        {
            if (phaseIndex < 0
                || phaseIndex >= static_cast<int>(phaseVolumes->size()))
            {
                return std::unexpected(FlowError{
                    FlowError::Code::InvalidInput,
                    "Phase index " + std::to_string(phaseIndex)
                    + " out of range [0, "
                    + std::to_string(phaseVolumes->size()) + ")"});
            }

            auto shortImage = (*phaseVolumes)[phaseIndex];
            if (!shortImage) {
                return std::unexpected(FlowError{
                    FlowError::Code::InternalError,
                    "Phase volume is null for phase "
                    + std::to_string(phaseIndex)});
            }

            // Convert short image to float for magnitude display
            VelocityPhase phase;
            phase.magnitudeImage = shortToFloat(shortImage);
            phase.velocityField = nullptr;  // Cine MRI has no velocity data
            phase.phaseIndex = phaseIndex;
            phase.triggerTime =
                (phaseIndex < static_cast<int>(triggerTimes->size()))
                    ? (*triggerTimes)[phaseIndex]
                    : 0.0;

            return phase;
        });

    return navigator;
}

}  // namespace dicom_viewer::services

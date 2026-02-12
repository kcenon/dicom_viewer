#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <itkImage.h>

#include "services/cardiac/cardiac_types.hpp"
#include "services/enhanced_dicom/enhanced_dicom_types.hpp"

namespace dicom_viewer::core {
struct DicomMetadata;
struct SliceInfo;
}  // namespace dicom_viewer::core

namespace dicom_viewer::services {

class TemporalNavigator;

// =============================================================================
// Cine MRI Data Structures
// =============================================================================

/**
 * @brief Cine MRI acquisition orientation
 *
 * Classified from Image Orientation Patient (0020,0037) cosines.
 *
 * @trace SRS-FR-053
 */
enum class CineOrientation {
    ShortAxis,    ///< SA: slice normal approximately along long axis of LV
    TwoChamber,   ///< 2CH: oblique sagittal through LV and LA
    ThreeChamber, ///< 3CH: oblique through LVOT
    FourChamber,  ///< 4CH: oblique through all 4 chambers
    Unknown       ///< Could not classify orientation
};

/**
 * @brief Convert CineOrientation to human-readable string
 */
[[nodiscard]] inline std::string cineOrientationToString(CineOrientation o) {
    switch (o) {
        case CineOrientation::ShortAxis:    return "SA";
        case CineOrientation::TwoChamber:   return "2CH";
        case CineOrientation::ThreeChamber: return "3CH";
        case CineOrientation::FourChamber:  return "4CH";
        case CineOrientation::Unknown:      return "Unknown";
    }
    return "Unknown";
}

/**
 * @brief Metadata describing a detected cine MRI series
 *
 * @trace SRS-FR-053, SDS-MOD-009
 */
struct CineSeriesInfo {
    int phaseCount = 0;                  ///< Number of temporal phases
    int sliceCount = 0;                  ///< Number of unique slice locations
    double temporalResolution = 0.0;     ///< Time between phases (ms)
    CineOrientation orientation = CineOrientation::Unknown;
    std::vector<double> triggerTimes;    ///< Sorted trigger times per phase (ms)
    std::string seriesDescription;       ///< DICOM Series Description

    /// Check if the cine series info is valid
    [[nodiscard]] bool isValid() const noexcept {
        return phaseCount >= 2 && sliceCount >= 1;
    }
};

/**
 * @brief Organized cine data ready for TemporalNavigator consumption
 *
 * Each phase volume is a 3D ITK image (short pixel type) representing
 * the cardiac anatomy at a specific moment in the cardiac cycle.
 *
 * @trace SRS-FR-053, SDS-MOD-009
 */
struct CineVolumeSeries {
    CineSeriesInfo info;
    /// phases[phaseIdx] = 3D volume for that cardiac phase
    std::vector<itk::Image<short, 3>::Pointer> phaseVolumes;

    /// Check if the series has valid phase volumes
    [[nodiscard]] bool isValid() const noexcept {
        return info.isValid()
            && static_cast<int>(phaseVolumes.size()) == info.phaseCount;
    }
};

// =============================================================================
// CineOrganizer
// =============================================================================

/**
 * @brief Cine MRI series detection, organization, and temporal display adapter
 *
 * Detects cine MRI acquisitions (both Classic and Enhanced DICOM IODs),
 * organizes multi-phase frames into 3D volumes per cardiac phase, and
 * bridges to TemporalNavigator for playback functionality.
 *
 * Key capabilities:
 * - Cine series detection via Trigger Time and Temporal Position tags
 * - Orientation classification (SA, 2CH, 3CH, 4CH) from image cosines
 * - Multi-slice short-axis stack reconstruction
 * - TemporalNavigator adapter for cine loop playback
 *
 * @example
 * @code
 * CineOrganizer organizer;
 * if (organizer.detectCineSeries(enhancedSeries)) {
 *     auto cineSeries = organizer.organizePhases(files);
 *     if (cineSeries) {
 *         auto navigator = organizer.createCineNavigator(cineSeries.value());
 *         navigator->play(25.0);
 *     }
 * }
 * @endcode
 *
 * @trace SRS-FR-053, SDS-MOD-009
 */
class CineOrganizer {
public:
    CineOrganizer();
    ~CineOrganizer();

    // Non-copyable, movable
    CineOrganizer(const CineOrganizer&) = delete;
    CineOrganizer& operator=(const CineOrganizer&) = delete;
    CineOrganizer(CineOrganizer&&) noexcept;
    CineOrganizer& operator=(CineOrganizer&&) noexcept;

    // --- Cine Detection ---

    /**
     * @brief Detect if an Enhanced DICOM series is a cine MRI acquisition
     *
     * Checks modality is MR, presence of temporal dimensions
     * (Trigger Time or Temporal Position Index), and verifies
     * frame count consistency with temporal positions.
     *
     * @param series Parsed Enhanced DICOM series info
     * @return true if the series is a cine MRI acquisition
     */
    [[nodiscard]] bool detectCineSeries(const EnhancedSeriesInfo& series) const;

    /**
     * @brief Detect if Classic DICOM files form a cine MRI series
     *
     * Checks modality is MR and scans for multiple unique Trigger Time
     * values within the same Series Instance UID.
     *
     * @param metadata Classic DICOM metadata for each file
     * @param slices Corresponding slice info for spatial data
     * @return true if the files form a cine MRI series
     */
    [[nodiscard]] bool detectCineSeries(
        const std::vector<core::DicomMetadata>& metadata,
        const std::vector<core::SliceInfo>& slices) const;

    // --- Phase Organization ---

    /**
     * @brief Organize Enhanced DICOM frames into cine volume series
     *
     * Groups frames by temporal position, sorts each group spatially,
     * and assembles 3D volumes per cardiac phase.
     *
     * @param series Enhanced DICOM series info
     * @return Organized cine volume series, or error
     * @trace SRS-FR-053
     */
    [[nodiscard]] std::expected<CineVolumeSeries, CardiacError>
    organizePhases(const EnhancedSeriesInfo& series) const;

    /**
     * @brief Organize Classic DICOM files into cine volume series
     *
     * Groups files by trigger time, sorts each group by slice location,
     * and assembles 3D volumes per cardiac phase.
     *
     * @param dicomFiles Paths to Classic DICOM files
     * @param metadata DICOM metadata for each file (parallel to dicomFiles)
     * @param slices Slice info for each file (parallel to dicomFiles)
     * @return Organized cine volume series, or error
     */
    [[nodiscard]] std::expected<CineVolumeSeries, CardiacError>
    organizePhases(
        const std::vector<std::string>& dicomFiles,
        const std::vector<core::DicomMetadata>& metadata,
        const std::vector<core::SliceInfo>& slices) const;

    // --- Orientation Detection ---

    /**
     * @brief Detect cine MRI acquisition orientation
     *
     * Classifies the acquisition plane from Image Orientation Patient
     * (0020,0037) direction cosines and optional Series Description.
     *
     * @param orientation 6-element direction cosines [rowX,rowY,rowZ,colX,colY,colZ]
     * @param seriesDescription Optional series description for keyword-based hints
     * @return Detected orientation
     */
    [[nodiscard]] CineOrientation detectOrientation(
        const std::array<double, 6>& orientation,
        const std::string& seriesDescription = "") const;

    // --- Short-Axis Stack Reconstruction ---

    /**
     * @brief Reconstruct short-axis stack from multi-slice cine data
     *
     * Each slice location has N temporal phases. This method reorganizes
     * the data into N 3D volumes, one per phase, where each volume
     * contains all slice locations stacked spatially.
     *
     * @param series Enhanced DICOM series info
     * @return Cine volume series with reconstructed SA stack
     */
    [[nodiscard]] std::expected<CineVolumeSeries, CardiacError>
    reconstructShortAxisStack(const EnhancedSeriesInfo& series) const;

    // --- TemporalNavigator Integration ---

    /**
     * @brief Create a TemporalNavigator configured for cine playback
     *
     * Converts short-pixel phase volumes to float magnitude images
     * and sets up the phase loader for on-demand access.
     *
     * @param cineSeries Organized cine volume series
     * @return Configured TemporalNavigator ready for playback
     * @trace SRS-FR-053
     */
    [[nodiscard]] std::unique_ptr<TemporalNavigator>
    createCineNavigator(const CineVolumeSeries& cineSeries) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

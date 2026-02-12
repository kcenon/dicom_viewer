#pragma once

/// @file cardiac_phantom_generator.hpp
/// @brief Synthetic phantom generators for cardiac CT integration testing
///
/// Generates ITK images with analytically known properties for validating:
/// - Calcium scoring accuracy (CalciumPhantom)
/// - Coronary centerline extraction accuracy (VesselPhantom)
/// - Cardiac phase separation (CardiacPhasePhantom)
///
/// All phantoms are deterministic and platform-independent.

#include <array>
#include <cmath>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkImageRegionIterator.h>

#include "services/cardiac/cardiac_types.hpp"

namespace dicom_viewer::test_utils {

using services::CalcifiedLesion;
using services::CenterlinePoint;

// =============================================================================
// Calcium Scoring Phantom
// =============================================================================

/// Definition of a single calcified lesion to embed in the phantom
struct LesionDefinition {
    std::array<double, 3> center;    ///< Physical center (mm)
    double radius;                   ///< Lesion radius (mm)
    double peakHU;                   ///< Peak Hounsfield Units (must be >130)
    std::string artery;              ///< "LAD", "LCx", "RCA", "LM"
};

/// Analytical ground truth for calcium scoring validation
struct CalciumGroundTruth {
    double expectedAgatston;         ///< Expected total Agatston score
    double expectedVolumeMM3;        ///< Expected total calcified volume
    int expectedLesionCount;         ///< Number of lesions above threshold
    std::vector<LesionDefinition> lesions;
};

/// Create a volume with embedded calcified lesions of known properties
/// @param sizeX, sizeY, sizeZ Volume dimensions in voxels
/// @param spacing Isotropic voxel spacing in mm
/// @param lesions Lesion definitions to embed
/// @param backgroundHU Background HU value (default: 30, soft tissue)
inline itk::Image<short, 3>::Pointer createCalciumPhantom(
    int sizeX, int sizeY, int sizeZ,
    double spacing,
    const std::vector<LesionDefinition>& lesions,
    short backgroundHU = 30)
{
    using ImageType = itk::Image<short, 3>;

    auto image = ImageType::New();
    ImageType::SizeType size;
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;

    ImageType::RegionType region;
    region.SetSize(size);

    image->SetRegions(region);
    const double sp[3] = {spacing, spacing, spacing};
    image->SetSpacing(sp);
    const double origin[3] = {0.0, 0.0, 0.0};
    image->SetOrigin(origin);
    image->Allocate();
    image->FillBuffer(backgroundHU);

    // Embed each lesion as a sphere with uniform HU
    itk::ImageRegionIterator<ImageType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        ImageType::PointType point;
        image->TransformIndexToPhysicalPoint(idx, point);

        for (const auto& lesion : lesions) {
            double dx = point[0] - lesion.center[0];
            double dy = point[1] - lesion.center[1];
            double dz = point[2] - lesion.center[2];
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

            if (dist <= lesion.radius) {
                // Smooth falloff at boundary for realism
                double t = dist / lesion.radius;
                short hu = static_cast<short>(
                    lesion.peakHU * (1.0 - 0.3 * t * t));
                if (hu > it.Get()) {
                    it.Set(hu);
                }
            }
        }
    }

    return image;
}

/// Compute expected Agatston score for a calcium phantom
/// Uses the same algorithm as the CalciumScorer for validation
inline CalciumGroundTruth computeCalciumGroundTruth(
    const std::vector<LesionDefinition>& lesions,
    double spacing,
    double sliceThickness)
{
    CalciumGroundTruth truth;
    truth.lesions = lesions;
    truth.expectedAgatston = 0.0;
    truth.expectedVolumeMM3 = 0.0;
    truth.expectedLesionCount = 0;

    double pixelArea = spacing * spacing;

    for (const auto& lesion : lesions) {
        if (lesion.peakHU < 130.0) continue;

        // Approximate: volume of sphere, number of voxels
        double volumeMM3 = (4.0 / 3.0) * M_PI
                         * lesion.radius * lesion.radius * lesion.radius;

        // Density weight factor
        int weight = 0;
        if (lesion.peakHU >= 400) weight = 4;
        else if (lesion.peakHU >= 300) weight = 3;
        else if (lesion.peakHU >= 200) weight = 2;
        else if (lesion.peakHU >= 130) weight = 1;

        // Approximate Agatston: total area across slices * weight
        // For a sphere, the cross-sectional area varies per slice
        // We approximate the number of slices the lesion spans
        double height = 2.0 * lesion.radius;
        int numSlices = std::max(1, static_cast<int>(height / sliceThickness));

        // Average slice area ≈ pi * r^2 * 2/3 (average of all cross-sections)
        double avgAreaMM2 = M_PI * lesion.radius * lesion.radius * 2.0 / 3.0;
        double lesionAgatston = avgAreaMM2 * weight * numSlices;

        truth.expectedAgatston += lesionAgatston;
        truth.expectedVolumeMM3 += volumeMM3;
        ++truth.expectedLesionCount;
    }

    return truth;
}

// =============================================================================
// Vessel Phantom
// =============================================================================

/// Ground truth for vessel centerline validation
struct VesselGroundTruth {
    std::vector<CenterlinePoint> centerline;
    double totalLength;              ///< Analytical path length (mm)
    double vesselRadius;             ///< Uniform vessel radius (mm)
};

/// Create a volume with a synthetic vessel following a known centerline
/// @param sizeX, sizeY, sizeZ Volume dimensions in voxels
/// @param spacing Isotropic voxel spacing in mm
/// @param centerline Known centerline path (physical coordinates)
/// @param vesselRadius Vessel radius in mm
/// @param vesselHU Vessel HU value
/// @param backgroundHU Background HU value
inline itk::Image<short, 3>::Pointer createVesselPhantom(
    int sizeX, int sizeY, int sizeZ,
    double spacing,
    const std::vector<CenterlinePoint>& centerline,
    double vesselRadius = 2.0,
    short vesselHU = 300,
    short backgroundHU = 0)
{
    using ImageType = itk::Image<short, 3>;

    auto image = ImageType::New();
    ImageType::SizeType size;
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;

    ImageType::RegionType region;
    region.SetSize(size);

    image->SetRegions(region);
    const double sp[3] = {spacing, spacing, spacing};
    image->SetSpacing(sp);
    const double origin[3] = {0.0, 0.0, 0.0};
    image->SetOrigin(origin);
    image->Allocate();
    image->FillBuffer(backgroundHU);

    // For each voxel, compute distance to nearest centerline segment
    itk::ImageRegionIterator<ImageType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        ImageType::PointType point;
        image->TransformIndexToPhysicalPoint(idx, point);

        double minDist = std::numeric_limits<double>::max();

        for (size_t i = 0; i + 1 < centerline.size(); ++i) {
            // Distance from point to line segment
            const auto& a = centerline[i].position;
            const auto& b = centerline[i + 1].position;

            double abx = b[0] - a[0], aby = b[1] - a[1], abz = b[2] - a[2];
            double apx = point[0] - a[0], apy = point[1] - a[1], apz = point[2] - a[2];

            double ab2 = abx*abx + aby*aby + abz*abz;
            if (ab2 < 1e-10) continue;

            double t = (apx*abx + apy*aby + apz*abz) / ab2;
            t = std::clamp(t, 0.0, 1.0);

            double cx = a[0] + t*abx - point[0];
            double cy = a[1] + t*aby - point[1];
            double cz = a[2] + t*abz - point[2];
            double dist = std::sqrt(cx*cx + cy*cy + cz*cz);

            if (dist < minDist) {
                minDist = dist;
            }
        }

        if (minDist <= vesselRadius) {
            it.Set(vesselHU);
        }
    }

    return image;
}

/// Generate a straight vessel centerline along Y-axis
inline VesselGroundTruth generateStraightVessel(
    double centerX, double centerZ,
    double startY, double endY,
    double vesselRadius = 2.0,
    int numPoints = 50)
{
    VesselGroundTruth truth;
    truth.vesselRadius = vesselRadius;
    truth.totalLength = endY - startY;

    for (int i = 0; i < numPoints; ++i) {
        double t = static_cast<double>(i) / (numPoints - 1);
        CenterlinePoint pt;
        pt.position = {centerX, startY + t * (endY - startY), centerZ};
        pt.tangent = {0.0, 1.0, 0.0};
        pt.normal = {1.0, 0.0, 0.0};
        pt.radius = vesselRadius;
        truth.centerline.push_back(pt);
    }

    return truth;
}

/// Generate an S-curved vessel centerline
inline VesselGroundTruth generateCurvedVessel(
    double centerX, double centerZ,
    double startY, double endY,
    double amplitude = 5.0,
    double vesselRadius = 2.0,
    int numPoints = 100)
{
    VesselGroundTruth truth;
    truth.vesselRadius = vesselRadius;
    truth.totalLength = 0.0;

    for (int i = 0; i < numPoints; ++i) {
        double t = static_cast<double>(i) / (numPoints - 1);
        double y = startY + t * (endY - startY);

        // S-curve: sinusoidal displacement in X
        double x = centerX + amplitude * std::sin(2.0 * M_PI * t);
        double z = centerZ + amplitude * 0.5 * std::cos(2.0 * M_PI * t);

        CenterlinePoint pt;
        pt.position = {x, y, z};
        pt.radius = vesselRadius;
        truth.centerline.push_back(pt);
    }

    // Compute arc length
    for (size_t i = 1; i < truth.centerline.size(); ++i) {
        double dx = truth.centerline[i].position[0] - truth.centerline[i-1].position[0];
        double dy = truth.centerline[i].position[1] - truth.centerline[i-1].position[1];
        double dz = truth.centerline[i].position[2] - truth.centerline[i-1].position[2];
        truth.totalLength += std::sqrt(dx*dx + dy*dy + dz*dz);
    }

    // Compute tangent/normal
    for (int i = 0; i < numPoints; ++i) {
        std::array<double, 3> tangent = {0.0, 0.0, 0.0};
        if (i == 0) {
            for (int d = 0; d < 3; ++d)
                tangent[d] = truth.centerline[1].position[d] - truth.centerline[0].position[d];
        } else if (i == numPoints - 1) {
            for (int d = 0; d < 3; ++d)
                tangent[d] = truth.centerline[i].position[d] - truth.centerline[i-1].position[d];
        } else {
            for (int d = 0; d < 3; ++d)
                tangent[d] = truth.centerline[i+1].position[d] - truth.centerline[i-1].position[d];
        }
        double mag = std::sqrt(tangent[0]*tangent[0] + tangent[1]*tangent[1] + tangent[2]*tangent[2]);
        if (mag > 1e-10) {
            for (auto& v : tangent) v /= mag;
        }
        truth.centerline[i].tangent = tangent;

        // Normal via minimum component trick
        std::array<double, 3> ref = {0.0, 0.0, 0.0};
        int minIdx = 0;
        double minVal = std::abs(tangent[0]);
        for (int d = 1; d < 3; ++d) {
            if (std::abs(tangent[d]) < minVal) {
                minVal = std::abs(tangent[d]);
                minIdx = d;
            }
        }
        ref[minIdx] = 1.0;
        double dot = ref[0]*tangent[0] + ref[1]*tangent[1] + ref[2]*tangent[2];
        std::array<double, 3> normal;
        for (int d = 0; d < 3; ++d) normal[d] = ref[d] - dot * tangent[d];
        double nmag = std::sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
        if (nmag > 1e-10) for (auto& n : normal) n /= nmag;
        truth.centerline[i].normal = normal;
    }

    return truth;
}

// =============================================================================
// Cardiac Phase Phantom
// =============================================================================

/// Ground truth for cardiac phase separation
struct CardiacPhaseGroundTruth {
    int phaseCount;
    int slicesPerPhase;
    double rrInterval;               ///< R-R interval in ms
    std::vector<double> nominalPercentages;
    int bestDiastolePhaseIndex;      ///< Index of 75% R-R phase
    int bestSystolePhaseIndex;       ///< Index of 40% R-R phase
};

/// Generate EnhancedFrameInfo array simulating a multi-phase cardiac CT
/// @param phaseCount Number of cardiac phases
/// @param slicesPerPhase Number of slices in each phase
/// @param rrInterval R-R interval in milliseconds
/// @param startZ Starting Z position (mm)
/// @param sliceSpacing Z spacing between slices (mm)
inline std::pair<std::vector<services::EnhancedFrameInfo>, CardiacPhaseGroundTruth>
generateCardiacPhaseFrames(
    int phaseCount = 10,
    int slicesPerPhase = 20,
    double rrInterval = 800.0,
    double startZ = 0.0,
    double sliceSpacing = 2.5)
{
    std::vector<services::EnhancedFrameInfo> frames;
    CardiacPhaseGroundTruth truth;
    truth.phaseCount = phaseCount;
    truth.slicesPerPhase = slicesPerPhase;
    truth.rrInterval = rrInterval;
    truth.bestDiastolePhaseIndex = -1;
    truth.bestSystolePhaseIndex = -1;

    int frameIdx = 0;
    for (int phase = 0; phase < phaseCount; ++phase) {
        double nominal = (static_cast<double>(phase) / phaseCount) * 100.0;
        double triggerTime = (nominal / 100.0) * rrInterval;
        truth.nominalPercentages.push_back(nominal);

        // Track best diastole (closest to 75%) and systole (closest to 40%)
        if (truth.bestDiastolePhaseIndex < 0 ||
            std::abs(nominal - 75.0) < std::abs(truth.nominalPercentages[truth.bestDiastolePhaseIndex] - 75.0)) {
            truth.bestDiastolePhaseIndex = phase;
        }
        if (truth.bestSystolePhaseIndex < 0 ||
            std::abs(nominal - 40.0) < std::abs(truth.nominalPercentages[truth.bestSystolePhaseIndex] - 40.0)) {
            truth.bestSystolePhaseIndex = phase;
        }

        for (int slice = 0; slice < slicesPerPhase; ++slice) {
            services::EnhancedFrameInfo frame;
            frame.frameIndex = frameIdx++;
            frame.imagePosition = {0.0, 0.0, startZ + slice * sliceSpacing};
            frame.triggerTime = triggerTime;
            frame.temporalPositionIndex = phase + 1;
            frame.sliceThickness = sliceSpacing;

            // Add DimensionIndex entries
            frame.dimensionIndices[0x00200032] = slice;      // ImagePositionPatient
            frame.dimensionIndices[0x00189241] = static_cast<int>(nominal);  // NominalPercentage

            frames.push_back(frame);
        }
    }

    return {frames, truth};
}

}  // namespace dicom_viewer::test_utils

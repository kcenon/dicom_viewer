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

#include "services/cardiac/calcium_scorer.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>

#include <kcenon/common/logging/log_macros.h>

#include <itkBinaryThresholdImageFilter.h>
#include <itkConnectedComponentImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkLabelStatisticsImageFilter.h>

namespace dicom_viewer::services {

using ImageType = itk::Image<short, 3>;
using LabelImageType = itk::Image<unsigned int, 3>;
using MaskImageType = itk::Image<uint8_t, 3>;

class CalciumScorer::Impl {
public:
};

CalciumScorer::CalciumScorer()
    : impl_(std::make_unique<Impl>()) {}

CalciumScorer::~CalciumScorer() = default;

CalciumScorer::CalciumScorer(CalciumScorer&&) noexcept = default;
CalciumScorer& CalciumScorer::operator=(CalciumScorer&&) noexcept = default;

int CalciumScorer::densityWeightFactor(short peakHU)
{
    if (peakHU >= calcium_constants::kWeightThreshold4) return 4;
    if (peakHU >= calcium_constants::kWeightThreshold3) return 3;
    if (peakHU >= calcium_constants::kWeightThreshold2) return 2;
    if (peakHU >= calcium_constants::kWeightThreshold1) return 1;
    return 0;
}

std::string CalciumScorer::classifyRisk(double agatstonScore)
{
    if (agatstonScore <= calcium_constants::kRiskNone) return "None";
    if (agatstonScore <= calcium_constants::kRiskMinimal) return "Minimal";
    if (agatstonScore <= calcium_constants::kRiskMild) return "Mild";
    if (agatstonScore <= calcium_constants::kRiskModerate) return "Moderate";
    return "Severe";
}

std::expected<CalciumScoreResult, CardiacError>
CalciumScorer::computeAgatston(ImageType::Pointer image,
                                double sliceThickness) const
{
    if (!image) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null image pointer for calcium scoring"
        });
    }

    if (sliceThickness <= 0.0) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Invalid slice thickness: " + std::to_string(sliceThickness)
        });
    }

    LOG_INFO("Computing Agatston calcium score...");

    auto spacing = image->GetSpacing();
    double pixelAreaMM2 = spacing[0] * spacing[1];
    auto region = image->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int numSlices = static_cast<int>(size[2]);

    // Step 1: Binary threshold at >= 130 HU
    using ThresholdFilter = itk::BinaryThresholdImageFilter<ImageType, MaskImageType>;
    auto threshold = ThresholdFilter::New();
    threshold->SetInput(image);
    threshold->SetLowerThreshold(calcium_constants::kHUThreshold);
    threshold->SetUpperThreshold(std::numeric_limits<short>::max());
    threshold->SetInsideValue(1);
    threshold->SetOutsideValue(0);
    threshold->Update();

    // Step 2: Connected component labeling
    using ConnectedFilter = itk::ConnectedComponentImageFilter<MaskImageType, LabelImageType>;
    auto connected = ConnectedFilter::New();
    connected->SetInput(threshold->GetOutput());
    connected->SetFullyConnected(false);  // 6-connectivity in 3D
    connected->Update();

    auto labelImage = connected->GetOutput();
    unsigned int numComponents = connected->GetObjectCount();

    LOG_DEBUG(std::format("Found {} connected components above 130 HU",
                         numComponents));

    if (numComponents == 0) {
        CalciumScoreResult result;
        result.totalAgatston = 0.0;
        result.volumeScore = 0.0;
        result.massScore = 0.0;
        result.riskCategory = classifyRisk(0.0);
        result.lesionCount = 0;
        LOG_INFO("No calcification detected");
        return result;
    }

    // Step 3: Compute statistics per label using LabelStatisticsImageFilter
    using StatsFilter = itk::LabelStatisticsImageFilter<ImageType, LabelImageType>;
    auto stats = StatsFilter::New();
    stats->SetInput(image);
    stats->SetLabelInput(labelImage);
    stats->Update();

    // Step 4: Process each component
    CalciumScoreResult result;
    result.totalAgatston = 0.0;
    result.volumeScore = 0.0;

    for (unsigned int label = 1; label <= numComponents; ++label) {
        if (!stats->HasLabel(label)) continue;

        double count = static_cast<double>(stats->GetCount(label));
        double totalAreaMM2 = count * pixelAreaMM2;

        // Filter: discard components smaller than minimum area
        if (totalAreaMM2 < calcium_constants::kMinLesionAreaMM2) {
            continue;
        }

        short peakHU = static_cast<short>(stats->GetMaximum(label));
        int weight = densityWeightFactor(peakHU);
        if (weight == 0) continue;

        // Compute per-slice Agatston score for this lesion
        // We need to iterate over slices and compute area per slice
        double lesionAgatston = 0.0;
        double lesionVolumeMM3 = 0.0;
        std::array<double, 3> centroidSum = {0.0, 0.0, 0.0};
        int voxelCount = 0;

        for (int z = 0; z < numSlices; ++z) {
            int sliceVoxelCount = 0;
            short slicePeakHU = 0;

            // Count voxels for this label in this slice
            ImageType::IndexType idx;
            idx[2] = z;
            for (int y = 0; y < static_cast<int>(size[1]); ++y) {
                idx[1] = y;
                for (int x = 0; x < static_cast<int>(size[0]); ++x) {
                    idx[0] = x;
                    if (labelImage->GetPixel(idx) == label) {
                        ++sliceVoxelCount;
                        short hu = image->GetPixel(idx);
                        if (hu > slicePeakHU) {
                            slicePeakHU = hu;
                        }
                        // Accumulate centroid
                        ImageType::PointType point;
                        image->TransformIndexToPhysicalPoint(idx, point);
                        centroidSum[0] += point[0];
                        centroidSum[1] += point[1];
                        centroidSum[2] += point[2];
                        ++voxelCount;
                    }
                }
            }

            if (sliceVoxelCount > 0) {
                double sliceAreaMM2 = sliceVoxelCount * pixelAreaMM2;
                int sliceWeight = densityWeightFactor(slicePeakHU);
                lesionAgatston += sliceAreaMM2 * sliceWeight;
                lesionVolumeMM3 += sliceVoxelCount * pixelAreaMM2 * sliceThickness;
            }
        }

        if (voxelCount == 0) continue;

        CalcifiedLesion lesion;
        lesion.labelId = static_cast<int>(label);
        lesion.areaMM2 = totalAreaMM2;
        lesion.peakHU = peakHU;
        lesion.weightFactor = weight;
        lesion.agatstonScore = lesionAgatston;
        lesion.volumeMM3 = lesionVolumeMM3;
        lesion.centroid = {
            centroidSum[0] / voxelCount,
            centroidSum[1] / voxelCount,
            centroidSum[2] / voxelCount
        };

        result.totalAgatston += lesionAgatston;
        result.volumeScore += lesionVolumeMM3;
        result.lesions.push_back(std::move(lesion));
    }

    result.lesionCount = static_cast<int>(result.lesions.size());
    result.riskCategory = classifyRisk(result.totalAgatston);

    LOG_INFO(std::format("Calcium scoring complete: Agatston={:.1f}, "
                        "{} lesions, risk={}",
                        result.totalAgatston, result.lesionCount,
                        result.riskCategory));

    return result;
}

std::expected<double, CardiacError>
CalciumScorer::computeVolumeScore(ImageType::Pointer image) const
{
    if (!image) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null image pointer for volume score"
        });
    }

    auto spacing = image->GetSpacing();
    double voxelVolMM3 = spacing[0] * spacing[1] * spacing[2];
    double totalVolume = 0.0;

    itk::ImageRegionConstIterator<ImageType> it(
        image, image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() >= calcium_constants::kHUThreshold) {
            totalVolume += voxelVolMM3;
        }
    }

    LOG_DEBUG(std::format("Volume score: {:.2f} mm³", totalVolume));
    return totalVolume;
}

std::expected<double, CardiacError>
CalciumScorer::computeMassScore(ImageType::Pointer image,
                                 double calibrationFactor) const
{
    if (!image) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null image pointer for mass score"
        });
    }

    if (calibrationFactor <= 0.0) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Invalid calibration factor: " + std::to_string(calibrationFactor)
        });
    }

    auto spacing = image->GetSpacing();
    double voxelVolMM3 = spacing[0] * spacing[1] * spacing[2];
    // Convert mm³ to mL: 1 mL = 1000 mm³
    double voxelVolML = voxelVolMM3 / 1000.0;
    double totalMass = 0.0;

    itk::ImageRegionConstIterator<ImageType> it(
        image, image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        short hu = it.Get();
        if (hu >= calcium_constants::kHUThreshold) {
            // mass = HU * calibrationFactor * voxelVolume
            totalMass += static_cast<double>(hu) * calibrationFactor * voxelVolML;
        }
    }

    LOG_DEBUG(std::format("Mass score: {:.2f} mg", totalMass));
    return totalMass;
}

void CalciumScorer::assignToArteries(
    std::vector<CalcifiedLesion>& lesions,
    const std::map<std::string, itk::Image<uint8_t, 3>::Pointer>& arteryROIs)
{
    if (arteryROIs.empty()) return;

    for (auto& lesion : lesions) {
        // Check if the lesion centroid falls within any artery ROI
        for (const auto& [arteryName, roiImage] : arteryROIs) {
            if (!roiImage) continue;

            ImageType::PointType point;
            point[0] = lesion.centroid[0];
            point[1] = lesion.centroid[1];
            point[2] = lesion.centroid[2];

            MaskImageType::IndexType idx;
            if (roiImage->TransformPhysicalPointToIndex(point, idx)) {
                auto roiRegion = roiImage->GetLargestPossibleRegion();
                if (roiRegion.IsInside(idx) && roiImage->GetPixel(idx) > 0) {
                    lesion.assignedArtery = arteryName;
                    break;
                }
            }
        }
    }
}

}  // namespace dicom_viewer::services

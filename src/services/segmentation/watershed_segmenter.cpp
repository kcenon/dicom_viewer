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

#include "services/segmentation/watershed_segmenter.hpp"
#include "services/segmentation/threshold_segmenter.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <format>

#include <itkWatershedImageFilter.h>
#include <itkMorphologicalWatershedImageFilter.h>
#include <itkMorphologicalWatershedFromMarkersImageFilter.h>
#include <itkGradientMagnitudeRecursiveGaussianImageFilter.h>
#include <itkRelabelComponentImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkImageRegionIterator.h>
#include <itkImageRegionConstIterator.h>
#include <itkCommand.h>

#include <unordered_map>

namespace dicom_viewer::services {

namespace {

/**
 * @brief ITK progress observer for callback integration
 */
class ProgressObserver : public itk::Command {
public:
    using Self = ProgressObserver;
    using Superclass = itk::Command;
    using Pointer = itk::SmartPointer<Self>;

    itkNewMacro(Self);

    void setCallback(WatershedSegmenter::ProgressCallback callback) {
        callback_ = std::move(callback);
    }

    void Execute(itk::Object* caller, const itk::EventObject& event) override {
        Execute(static_cast<const itk::Object*>(caller), event);
    }

    void Execute(const itk::Object* caller, const itk::EventObject& event) override {
        if (!callback_) return;

        if (itk::ProgressEvent().CheckEvent(&event)) {
            const auto* process = dynamic_cast<const itk::ProcessObject*>(caller);
            if (process) {
                callback_(process->GetProgress());
            }
        }
    }

private:
    WatershedSegmenter::ProgressCallback callback_;
};

}  // anonymous namespace

std::expected<WatershedResult, SegmentationError>
WatershedSegmenter::segment(
    ImageType::Pointer input,
    const WatershedParameters& params
) const {
    LOG_INFO(std::format("Watershed segmentation: level={:.3f}, threshold={:.3f}, sigma={:.2f}",
                      params.level, params.threshold, params.gradientSigma));

    if (!input) {
        LOG_ERROR("Input image is null");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        LOG_ERROR("Invalid parameters");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid watershed parameters"
        });
    }

    try {
        // Compute gradient magnitude
        auto gradient = computeGradientMagnitude(input, params.gradientSigma);

        // Apply watershed transform
        auto labelMap = applyWatershed(gradient, params.level, params.threshold);

        // Remove small regions if requested
        if (params.mergeSmallRegions && params.minimumRegionSize > 0) {
            labelMap = removeSmallRegions(labelMap, params.minimumRegionSize);
        }

        // Compute region statistics
        auto regions = computeRegionStatistics(labelMap);

        WatershedResult result;
        result.labelMap = labelMap;
        result.regionCount = regions.size();
        result.regions = std::move(regions);

        LOG_INFO(std::format("Watershed complete: {} regions found", result.regionCount));
        return result;
    }
    catch (const itk::ExceptionObject& e) {
        LOG_ERROR(std::format("ITK exception: {}", e.GetDescription()));
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::format("Standard exception: {}", e.what()));
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

std::expected<WatershedResult, SegmentationError>
WatershedSegmenter::segmentWithMarkers(
    ImageType::Pointer input,
    LabelMapType::Pointer markers,
    const WatershedParameters& params
) const {
    LOG_INFO(std::format("Marker-based watershed segmentation: sigma={:.2f}",
                      params.gradientSigma));

    if (!input) {
        LOG_ERROR("Input image is null");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!markers) {
        LOG_ERROR("Marker image is null");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Marker image is null"
        });
    }

    // Verify dimensions match
    auto inputSize = input->GetLargestPossibleRegion().GetSize();
    auto markerSize = markers->GetLargestPossibleRegion().GetSize();
    if (inputSize != markerSize) {
        LOG_ERROR("Input and marker dimensions do not match");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input and marker image dimensions must match"
        });
    }

    if (!params.isValid()) {
        LOG_ERROR("Invalid parameters");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid watershed parameters"
        });
    }

    try {
        // Compute gradient magnitude
        auto gradient = computeGradientMagnitude(input, params.gradientSigma);

        // Apply marker-based watershed
        auto labelMap = applyMarkerWatershed(gradient, markers);

        // Remove small regions if requested
        if (params.mergeSmallRegions && params.minimumRegionSize > 0) {
            labelMap = removeSmallRegions(labelMap, params.minimumRegionSize);
        }

        // Compute region statistics
        auto regions = computeRegionStatistics(labelMap);

        WatershedResult result;
        result.labelMap = labelMap;
        result.regionCount = regions.size();
        result.regions = std::move(regions);

        LOG_INFO(std::format("Marker watershed complete: {} regions found", result.regionCount));
        return result;
    }
    catch (const itk::ExceptionObject& e) {
        LOG_ERROR(std::format("ITK exception: {}", e.GetDescription()));
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::format("Standard exception: {}", e.what()));
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

std::expected<WatershedSegmenter::BinaryMaskType::Pointer, SegmentationError>
WatershedSegmenter::extractRegion(
    LabelMapType::Pointer labelMap,
    unsigned long regionLabel
) const {
    if (!labelMap) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Label map is null"
        });
    }

    try {
        using ThresholdFilterType = itk::BinaryThresholdImageFilter<LabelMapType, BinaryMaskType>;
        auto filter = ThresholdFilterType::New();

        filter->SetInput(labelMap);
        filter->SetLowerThreshold(regionLabel);
        filter->SetUpperThreshold(regionLabel);
        filter->SetInsideValue(1);
        filter->SetOutsideValue(0);

        filter->Update();

        return filter->GetOutput();
    }
    catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
}

void WatershedSegmenter::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

WatershedSegmenter::FloatImageType::Pointer
WatershedSegmenter::computeGradientMagnitude(
    ImageType::Pointer input,
    double sigma
) const {
    // Cast to float for gradient computation
    using CastFilterType = itk::CastImageFilter<ImageType, FloatImageType>;
    auto castFilter = CastFilterType::New();
    castFilter->SetInput(input);

    // Apply gradient magnitude filter with Gaussian smoothing
    using GradientFilterType = itk::GradientMagnitudeRecursiveGaussianImageFilter<
        FloatImageType, FloatImageType>;
    auto gradientFilter = GradientFilterType::New();

    gradientFilter->SetInput(castFilter->GetOutput());
    gradientFilter->SetSigma(sigma);

    if (progressCallback_) {
        auto observer = ProgressObserver::New();
        observer->setCallback([this](double progress) {
            progressCallback_(progress * 0.3);  // Gradient is ~30% of total work
        });
        gradientFilter->AddObserver(itk::ProgressEvent(), observer);
    }

    gradientFilter->Update();

    return gradientFilter->GetOutput();
}

WatershedSegmenter::LabelMapType::Pointer
WatershedSegmenter::applyWatershed(
    FloatImageType::Pointer gradient,
    double level,
    double threshold
) const {
    using WatershedFilterType = itk::WatershedImageFilter<FloatImageType>;
    auto watershedFilter = WatershedFilterType::New();

    watershedFilter->SetInput(gradient);
    watershedFilter->SetLevel(level);
    watershedFilter->SetThreshold(threshold);

    if (progressCallback_) {
        auto observer = ProgressObserver::New();
        observer->setCallback([this](double progress) {
            progressCallback_(0.3 + progress * 0.6);  // Watershed is ~60% of work
        });
        watershedFilter->AddObserver(itk::ProgressEvent(), observer);
    }

    watershedFilter->Update();

    return watershedFilter->GetOutput();
}

WatershedSegmenter::LabelMapType::Pointer
WatershedSegmenter::applyMarkerWatershed(
    FloatImageType::Pointer gradient,
    LabelMapType::Pointer markers
) const {
    using MorphoWatershedFilterType = itk::MorphologicalWatershedFromMarkersImageFilter<
        FloatImageType, LabelMapType>;
    auto watershedFilter = MorphoWatershedFilterType::New();

    watershedFilter->SetInput(gradient);
    watershedFilter->SetMarkerImage(markers);
    watershedFilter->SetMarkWatershedLine(false);

    if (progressCallback_) {
        auto observer = ProgressObserver::New();
        observer->setCallback([this](double progress) {
            progressCallback_(0.3 + progress * 0.6);
        });
        watershedFilter->AddObserver(itk::ProgressEvent(), observer);
    }

    watershedFilter->Update();

    return watershedFilter->GetOutput();
}

WatershedSegmenter::LabelMapType::Pointer
WatershedSegmenter::removeSmallRegions(
    LabelMapType::Pointer labelMap,
    int minimumSize
) const {
    using RelabelFilterType = itk::RelabelComponentImageFilter<LabelMapType, LabelMapType>;
    auto relabelFilter = RelabelFilterType::New();

    relabelFilter->SetInput(labelMap);
    relabelFilter->SetMinimumObjectSize(static_cast<unsigned long>(minimumSize));

    if (progressCallback_) {
        auto observer = ProgressObserver::New();
        observer->setCallback([this](double progress) {
            progressCallback_(0.9 + progress * 0.1);  // Relabel is ~10% of work
        });
        relabelFilter->AddObserver(itk::ProgressEvent(), observer);
    }

    relabelFilter->Update();

    return relabelFilter->GetOutput();
}

std::vector<RegionInfo>
WatershedSegmenter::computeRegionStatistics(
    LabelMapType::Pointer labelMap
) const {
    std::unordered_map<unsigned long, RegionInfo> regionMap;

    auto region = labelMap->GetLargestPossibleRegion();
    itk::ImageRegionConstIterator<LabelMapType> it(labelMap, region);

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        unsigned long label = it.Get();
        if (label == 0) continue;  // Skip background

        auto& info = regionMap[label];
        info.label = label;
        info.voxelCount++;

        auto idx = it.GetIndex();
        info.centroid[0] += static_cast<double>(idx[0]);
        info.centroid[1] += static_cast<double>(idx[1]);
        info.centroid[2] += static_cast<double>(idx[2]);
    }

    // Finalize centroids
    std::vector<RegionInfo> regions;
    regions.reserve(regionMap.size());

    for (auto& [label, info] : regionMap) {
        if (info.voxelCount > 0) {
            info.centroid[0] /= static_cast<double>(info.voxelCount);
            info.centroid[1] /= static_cast<double>(info.voxelCount);
            info.centroid[2] /= static_cast<double>(info.voxelCount);
        }
        regions.push_back(info);
    }

    // Sort by label
    std::sort(regions.begin(), regions.end(),
              [](const RegionInfo& a, const RegionInfo& b) {
                  return a.label < b.label;
              });

    return regions;
}

}  // namespace dicom_viewer::services

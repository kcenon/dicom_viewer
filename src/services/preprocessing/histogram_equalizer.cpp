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

#include "services/preprocessing/histogram_equalizer.hpp"

#include <algorithm>
#include <cmath>

#include <itkAdaptiveHistogramEqualizationImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkCommand.h>
#include <itkExtractImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkRescaleIntensityImageFilter.h>
#include <itkStatisticsImageFilter.h>

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

    void setCallback(HistogramEqualizer::ProgressCallback callback) {
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
    HistogramEqualizer::ProgressCallback callback_;
};

}  // anonymous namespace

/**
 * @brief PIMPL implementation for HistogramEqualizer
 */
class HistogramEqualizer::Impl {
public:
    ProgressCallback progressCallback;
};

HistogramEqualizer::HistogramEqualizer() : impl_(std::make_unique<Impl>()) {}

HistogramEqualizer::~HistogramEqualizer() = default;

HistogramEqualizer::HistogramEqualizer(HistogramEqualizer&&) noexcept = default;

HistogramEqualizer& HistogramEqualizer::operator=(HistogramEqualizer&&) noexcept = default;

void HistogramEqualizer::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<HistogramEqualizer::ImageType::Pointer, PreprocessingError>
HistogramEqualizer::equalize(ImageType::Pointer input) const {
    return equalize(input, Parameters{});
}

std::expected<HistogramEqualizer::ImageType::Pointer, PreprocessingError>
HistogramEqualizer::equalize(ImageType::Pointer input, const Parameters& params) const {
    // Validate input
    if (!input) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InvalidInput,
            "Input image is null"
        });
    }

    // Validate parameters
    if (!params.isValid()) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InvalidParameters,
            "Invalid parameters: check clipLimit (0.1-10.0), tileSize (1-64), "
            "numberOfBins (16-4096)"
        });
    }

    try {
        // Use float for internal processing (required by ITK's adaptive filter)
        using FloatImageType = itk::Image<float, 3>;

        // Cast input to float
        using CastToFloatType = itk::CastImageFilter<ImageType, FloatImageType>;
        auto castToFloat = CastToFloatType::New();
        castToFloat->SetInput(input);

        // Get original intensity range for output scaling
        using StatsFilterType = itk::StatisticsImageFilter<ImageType>;
        auto statsFilter = StatsFilterType::New();
        statsFilter->SetInput(input);
        statsFilter->Update();
        double originalMin = statsFilter->GetMinimum();
        double originalMax = statsFilter->GetMaximum();

        FloatImageType::Pointer processedFloat;

        if (params.method == EqualizationMethod::CLAHE ||
            params.method == EqualizationMethod::Adaptive) {
            // Use AdaptiveHistogramEqualizationImageFilter
            using AdaptiveFilterType =
                itk::AdaptiveHistogramEqualizationImageFilter<FloatImageType>;
            auto adaptiveFilter = AdaptiveFilterType::New();

            adaptiveFilter->SetInput(castToFloat->GetOutput());

            // Set radius (tile size / 2)
            AdaptiveFilterType::RadiusType radius;
            radius[0] = params.tileSize[0] / 2;
            radius[1] = params.tileSize[1] / 2;
            radius[2] = params.tileSize[2] / 2;
            adaptiveFilter->SetRadius(radius);

            // Configure for CLAHE vs standard AHE
            if (params.method == EqualizationMethod::CLAHE) {
                // Alpha controls blending (0 = classic AHE, 1 = unfiltered)
                // For CLAHE-like behavior, use moderate alpha
                adaptiveFilter->SetAlpha(0.5);
                // Beta controls clip limit effect (higher = more limiting)
                // Map clipLimit [0.1, 10.0] to beta [0.1, 0.9]
                double beta = 1.0 - (params.clipLimit / 10.0) * 0.8;
                adaptiveFilter->SetBeta(std::max(0.1, std::min(0.9, beta)));
            } else {
                // Standard adaptive histogram equalization
                adaptiveFilter->SetAlpha(0.3);
                adaptiveFilter->SetBeta(0.3);
            }

            // Attach progress observer if callback is set
            if (impl_->progressCallback) {
                auto observer = ProgressObserver::New();
                observer->setCallback(impl_->progressCallback);
                adaptiveFilter->AddObserver(itk::ProgressEvent(), observer);
            }

            adaptiveFilter->Update();
            processedFloat = adaptiveFilter->GetOutput();
        } else {
            // Standard global histogram equalization
            // ITK doesn't have a direct global histogram equalization filter,
            // so we use AdaptiveHistogramEqualizationImageFilter with large radius
            using AdaptiveFilterType =
                itk::AdaptiveHistogramEqualizationImageFilter<FloatImageType>;
            auto adaptiveFilter = AdaptiveFilterType::New();

            adaptiveFilter->SetInput(castToFloat->GetOutput());

            // Use large radius for global-like behavior
            auto region = input->GetLargestPossibleRegion();
            auto size = region.GetSize();
            AdaptiveFilterType::RadiusType radius;
            radius[0] = size[0] / 2;
            radius[1] = size[1] / 2;
            radius[2] = size[2] / 2;
            adaptiveFilter->SetRadius(radius);

            adaptiveFilter->SetAlpha(1.0);  // Pure histogram equalization
            adaptiveFilter->SetBeta(0.0);   // No clip limiting

            if (impl_->progressCallback) {
                auto observer = ProgressObserver::New();
                observer->setCallback(impl_->progressCallback);
                adaptiveFilter->AddObserver(itk::ProgressEvent(), observer);
            }

            adaptiveFilter->Update();
            processedFloat = adaptiveFilter->GetOutput();
        }

        // Rescale output to desired range
        using RescaleFilterType = itk::RescaleIntensityImageFilter<FloatImageType, ImageType>;
        auto rescaleFilter = RescaleFilterType::New();
        rescaleFilter->SetInput(processedFloat);

        if (params.preserveRange) {
            rescaleFilter->SetOutputMinimum(static_cast<short>(originalMin));
            rescaleFilter->SetOutputMaximum(static_cast<short>(originalMax));
        } else {
            rescaleFilter->SetOutputMinimum(static_cast<short>(params.outputMinimum));
            rescaleFilter->SetOutputMaximum(static_cast<short>(params.outputMaximum));
        }

        rescaleFilter->Update();

        return rescaleFilter->GetOutput();
    }
    catch (const itk::ExceptionObject& e) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

std::expected<HistogramEqualizer::ImageType::Pointer, PreprocessingError>
HistogramEqualizer::applyCLAHE(
    ImageType::Pointer input,
    double clipLimit,
    const std::array<unsigned int, 3>& tileSize
) const {
    Parameters params;
    params.method = EqualizationMethod::CLAHE;
    params.clipLimit = clipLimit;
    params.tileSize = tileSize;

    return equalize(input, params);
}

std::expected<HistogramEqualizer::Image2DType::Pointer, PreprocessingError>
HistogramEqualizer::equalizeSlice(
    ImageType::Pointer input,
    unsigned int sliceIndex
) const {
    return equalizeSlice(input, sliceIndex, Parameters{});
}

std::expected<HistogramEqualizer::Image2DType::Pointer, PreprocessingError>
HistogramEqualizer::equalizeSlice(
    ImageType::Pointer input,
    unsigned int sliceIndex,
    const Parameters& params
) const {
    // Validate input
    if (!input) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InvalidInput,
            "Input image is null"
        });
    }

    // Validate parameters
    if (!params.isValid()) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InvalidParameters,
            "Invalid parameters"
        });
    }

    // Validate slice index
    auto region = input->GetLargestPossibleRegion();
    auto size = region.GetSize();
    if (sliceIndex >= size[2]) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InvalidParameters,
            "Slice index out of range"
        });
    }

    try {
        // Extract 2D slice from 3D volume
        using ExtractFilterType = itk::ExtractImageFilter<ImageType, Image2DType>;
        auto extractFilter = ExtractFilterType::New();
        extractFilter->SetDirectionCollapseToSubmatrix();

        ImageType::RegionType extractRegion = region;
        extractRegion.SetSize(2, 0);  // Collapse Z dimension
        extractRegion.SetIndex(2, sliceIndex);

        extractFilter->SetExtractionRegion(extractRegion);
        extractFilter->SetInput(input);
        extractFilter->Update();

        // Process the 2D slice
        using FloatImage2DType = itk::Image<float, 2>;

        // Cast to float
        using CastToFloatType = itk::CastImageFilter<Image2DType, FloatImage2DType>;
        auto castToFloat = CastToFloatType::New();
        castToFloat->SetInput(extractFilter->GetOutput());

        // Get original intensity range
        using StatsFilterType = itk::StatisticsImageFilter<Image2DType>;
        auto statsFilter = StatsFilterType::New();
        statsFilter->SetInput(extractFilter->GetOutput());
        statsFilter->Update();
        double originalMin = statsFilter->GetMinimum();
        double originalMax = statsFilter->GetMaximum();

        // Apply adaptive histogram equalization
        using AdaptiveFilterType =
            itk::AdaptiveHistogramEqualizationImageFilter<FloatImage2DType>;
        auto adaptiveFilter = AdaptiveFilterType::New();
        adaptiveFilter->SetInput(castToFloat->GetOutput());

        AdaptiveFilterType::RadiusType radius;
        radius[0] = params.tileSize[0] / 2;
        radius[1] = params.tileSize[1] / 2;
        adaptiveFilter->SetRadius(radius);

        if (params.method == EqualizationMethod::CLAHE) {
            adaptiveFilter->SetAlpha(0.5);
            double beta = 1.0 - (params.clipLimit / 10.0) * 0.8;
            adaptiveFilter->SetBeta(std::max(0.1, std::min(0.9, beta)));
        } else if (params.method == EqualizationMethod::Adaptive) {
            adaptiveFilter->SetAlpha(0.3);
            adaptiveFilter->SetBeta(0.3);
        } else {
            // Standard: use large radius
            auto sliceSize = extractFilter->GetOutput()->GetLargestPossibleRegion().GetSize();
            radius[0] = sliceSize[0] / 2;
            radius[1] = sliceSize[1] / 2;
            adaptiveFilter->SetRadius(radius);
            adaptiveFilter->SetAlpha(1.0);
            adaptiveFilter->SetBeta(0.0);
        }

        adaptiveFilter->Update();

        // Rescale to original range
        using RescaleFilterType = itk::RescaleIntensityImageFilter<FloatImage2DType, Image2DType>;
        auto rescaleFilter = RescaleFilterType::New();
        rescaleFilter->SetInput(adaptiveFilter->GetOutput());

        if (params.preserveRange) {
            rescaleFilter->SetOutputMinimum(static_cast<short>(originalMin));
            rescaleFilter->SetOutputMaximum(static_cast<short>(originalMax));
        } else {
            rescaleFilter->SetOutputMinimum(static_cast<short>(params.outputMinimum));
            rescaleFilter->SetOutputMaximum(static_cast<short>(params.outputMaximum));
        }

        rescaleFilter->Update();

        return rescaleFilter->GetOutput();
    }
    catch (const itk::ExceptionObject& e) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

std::expected<HistogramEqualizer::Image2DType::Pointer, PreprocessingError>
HistogramEqualizer::preview(
    ImageType::Pointer input,
    unsigned int previewSlice
) const {
    return equalizeSlice(input, previewSlice, Parameters{});
}

std::expected<HistogramEqualizer::Image2DType::Pointer, PreprocessingError>
HistogramEqualizer::preview(
    ImageType::Pointer input,
    unsigned int previewSlice,
    const Parameters& params
) const {
    return equalizeSlice(input, previewSlice, params);
}

HistogramData HistogramEqualizer::computeHistogram(
    ImageType::Pointer input,
    int numBins
) const {
    HistogramData result;

    if (!input) {
        return result;
    }

    // Get min/max values
    using StatsFilterType = itk::StatisticsImageFilter<ImageType>;
    auto statsFilter = StatsFilterType::New();
    statsFilter->SetInput(input);
    statsFilter->Update();

    result.minValue = statsFilter->GetMinimum();
    result.maxValue = statsFilter->GetMaximum();

    // Compute bin width
    double range = result.maxValue - result.minValue;
    if (range <= 0) {
        // Uniform image
        result.bins.push_back(result.minValue);
        result.counts.push_back(input->GetLargestPossibleRegion().GetNumberOfPixels());
        return result;
    }

    double binWidth = range / numBins;

    // Initialize bins
    result.bins.resize(numBins);
    result.counts.resize(numBins, 0);

    for (int i = 0; i < numBins; ++i) {
        result.bins[i] = result.minValue + (i + 0.5) * binWidth;
    }

    // Count pixels in each bin
    itk::ImageRegionConstIterator<ImageType> it(
        input, input->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        double value = it.Get();
        int binIndex = static_cast<int>((value - result.minValue) / binWidth);
        binIndex = std::max(0, std::min(numBins - 1, binIndex));
        result.counts[binIndex]++;
    }

    return result;
}

}  // namespace dicom_viewer::services

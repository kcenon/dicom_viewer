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

#include "services/preprocessing/isotropic_resampler.hpp"

#include <cmath>

#include <itkBSplineInterpolateImageFunction.h>
#include <itkCastImageFilter.h>
#include <itkCommand.h>
#include <itkIdentityTransform.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkWindowedSincInterpolateImageFunction.h>

namespace dicom_viewer::services {

namespace {

/**
 * @brief ITK progress observer for resampling callback integration
 */
class ResampleProgressObserver : public itk::Command {
public:
    using Self = ResampleProgressObserver;
    using Superclass = itk::Command;
    using Pointer = itk::SmartPointer<Self>;

    itkNewMacro(Self);

    void setCallback(IsotropicResampler::ProgressCallback callback) {
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
    IsotropicResampler::ProgressCallback callback_;
};

/**
 * @brief Calculate output size for isotropic resampling
 */
template <typename ImageType>
typename ImageType::SizeType calculateOutputSize(
    typename ImageType::Pointer input,
    double targetSpacing
) {
    auto inputSize = input->GetLargestPossibleRegion().GetSize();
    auto inputSpacing = input->GetSpacing();

    typename ImageType::SizeType outputSize;
    for (unsigned int i = 0; i < 3; ++i) {
        outputSize[i] = static_cast<typename ImageType::SizeType::SizeValueType>(
            std::ceil(inputSize[i] * inputSpacing[i] / targetSpacing)
        );
        if (outputSize[i] < 1) {
            outputSize[i] = 1;
        }
    }
    return outputSize;
}

}  // anonymous namespace

/**
 * @brief PIMPL implementation for IsotropicResampler
 */
class IsotropicResampler::Impl {
public:
    ProgressCallback progressCallback;
};

IsotropicResampler::IsotropicResampler()
    : impl_(std::make_unique<Impl>()) {}

IsotropicResampler::~IsotropicResampler() = default;

IsotropicResampler::IsotropicResampler(IsotropicResampler&&) noexcept = default;

IsotropicResampler& IsotropicResampler::operator=(IsotropicResampler&&) noexcept = default;

void IsotropicResampler::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<IsotropicResampler::ImageType::Pointer, PreprocessingError>
IsotropicResampler::resample(ImageType::Pointer input) const {
    return resample(input, Parameters{});
}

std::expected<IsotropicResampler::ImageType::Pointer, PreprocessingError>
IsotropicResampler::resample(
    ImageType::Pointer input,
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
            "Invalid resampling parameters: check targetSpacing (0.1-10.0mm) "
            "and splineOrder (2-5)"
        });
    }

    try {
        // Store original image information
        auto originalOrigin = input->GetOrigin();
        auto originalDirection = input->GetDirection();

        // Calculate output size and spacing
        auto outputSize = calculateOutputSize<ImageType>(input, params.targetSpacing);

        ImageType::SpacingType outputSpacing;
        outputSpacing.Fill(params.targetSpacing);

        // Create resample filter
        using TransformType = itk::IdentityTransform<double, 3>;
        using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;

        auto resampleFilter = ResampleFilterType::New();
        resampleFilter->SetInput(input);
        resampleFilter->SetSize(outputSize);
        resampleFilter->SetOutputSpacing(outputSpacing);
        resampleFilter->SetOutputOrigin(originalOrigin);
        resampleFilter->SetOutputDirection(originalDirection);
        resampleFilter->SetTransform(TransformType::New());
        resampleFilter->SetDefaultPixelValue(static_cast<short>(params.defaultValue));

        // Set interpolator based on parameters
        switch (params.interpolation) {
            case Interpolation::NearestNeighbor: {
                using InterpolatorType =
                    itk::NearestNeighborInterpolateImageFunction<ImageType, double>;
                resampleFilter->SetInterpolator(InterpolatorType::New());
                break;
            }
            case Interpolation::Linear: {
                using InterpolatorType =
                    itk::LinearInterpolateImageFunction<ImageType, double>;
                resampleFilter->SetInterpolator(InterpolatorType::New());
                break;
            }
            case Interpolation::BSpline: {
                using InterpolatorType =
                    itk::BSplineInterpolateImageFunction<ImageType, double>;
                auto interpolator = InterpolatorType::New();
                interpolator->SetSplineOrder(params.splineOrder);
                resampleFilter->SetInterpolator(interpolator);
                break;
            }
            case Interpolation::WindowedSinc: {
                // Using Hamming window with radius 4
                constexpr unsigned int WindowRadius = 4;
                using WindowFunctionType = itk::Function::HammingWindowFunction<WindowRadius>;
                using InterpolatorType = itk::WindowedSincInterpolateImageFunction<
                    ImageType, WindowRadius, WindowFunctionType>;
                resampleFilter->SetInterpolator(InterpolatorType::New());
                break;
            }
        }

        // Attach progress observer if callback is set
        if (impl_->progressCallback) {
            auto observer = ResampleProgressObserver::New();
            observer->setCallback(impl_->progressCallback);
            resampleFilter->AddObserver(itk::ProgressEvent(), observer);
        }

        resampleFilter->Update();

        return resampleFilter->GetOutput();
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

std::expected<IsotropicResampler::LabelMapType::Pointer, PreprocessingError>
IsotropicResampler::resampleLabels(
    LabelMapType::Pointer input,
    double targetSpacing
) const {
    // Validate input
    if (!input) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InvalidInput,
            "Input label map is null"
        });
    }

    // Validate target spacing
    if (targetSpacing < 0.1 || targetSpacing > 10.0) {
        return std::unexpected(PreprocessingError{
            PreprocessingError::Code::InvalidParameters,
            "Invalid target spacing: must be between 0.1 and 10.0 mm"
        });
    }

    try {
        // Store original image information
        auto originalOrigin = input->GetOrigin();
        auto originalDirection = input->GetDirection();

        // Calculate output size
        auto outputSize = calculateOutputSize<LabelMapType>(input, targetSpacing);

        LabelMapType::SpacingType outputSpacing;
        outputSpacing.Fill(targetSpacing);

        // Create resample filter with nearest neighbor interpolation
        using TransformType = itk::IdentityTransform<double, 3>;
        using InterpolatorType =
            itk::NearestNeighborInterpolateImageFunction<LabelMapType, double>;
        using ResampleFilterType =
            itk::ResampleImageFilter<LabelMapType, LabelMapType>;

        auto resampleFilter = ResampleFilterType::New();
        resampleFilter->SetInput(input);
        resampleFilter->SetSize(outputSize);
        resampleFilter->SetOutputSpacing(outputSpacing);
        resampleFilter->SetOutputOrigin(originalOrigin);
        resampleFilter->SetOutputDirection(originalDirection);
        resampleFilter->SetTransform(TransformType::New());
        resampleFilter->SetInterpolator(InterpolatorType::New());
        resampleFilter->SetDefaultPixelValue(0);  // Background label

        // Attach progress observer if callback is set
        if (impl_->progressCallback) {
            auto observer = ResampleProgressObserver::New();
            observer->setCallback(impl_->progressCallback);
            resampleFilter->AddObserver(itk::ProgressEvent(), observer);
        }

        resampleFilter->Update();

        return resampleFilter->GetOutput();
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

std::expected<IsotropicResampler::ResampledInfo, PreprocessingError>
IsotropicResampler::previewDimensions(
    ImageType::Pointer input,
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
            "Invalid resampling parameters"
        });
    }

    auto inputSize = input->GetLargestPossibleRegion().GetSize();
    auto inputSpacing = input->GetSpacing();
    auto outputSize = calculateOutputSize<ImageType>(input, params.targetSpacing);

    ResampledInfo info;

    // Original dimensions
    for (unsigned int i = 0; i < 3; ++i) {
        info.originalSize[i] = static_cast<unsigned int>(inputSize[i]);
        info.originalSpacing[i] = inputSpacing[i];
        info.resampledSize[i] = static_cast<unsigned int>(outputSize[i]);
    }

    info.resampledSpacing = params.targetSpacing;

    // Estimate memory (short = 2 bytes per voxel)
    info.estimatedMemoryBytes = static_cast<size_t>(outputSize[0]) *
                                 static_cast<size_t>(outputSize[1]) *
                                 static_cast<size_t>(outputSize[2]) *
                                 sizeof(short);

    return info;
}

bool IsotropicResampler::needsResampling(ImageType::Pointer input) {
    if (!input) {
        return false;
    }

    auto spacing = input->GetSpacing();

    // Check if spacing differs by more than 1% between any dimensions
    constexpr double threshold = 0.01;

    double avgSpacing = (spacing[0] + spacing[1] + spacing[2]) / 3.0;

    for (unsigned int i = 0; i < 3; ++i) {
        double diff = std::abs(spacing[i] - avgSpacing) / avgSpacing;
        if (diff > threshold) {
            return true;
        }
    }

    return false;
}

std::string IsotropicResampler::interpolationToString(Interpolation interp) {
    switch (interp) {
        case Interpolation::NearestNeighbor:
            return "Nearest Neighbor";
        case Interpolation::Linear:
            return "Linear";
        case Interpolation::BSpline:
            return "B-Spline";
        case Interpolation::WindowedSinc:
            return "Windowed Sinc";
    }
    return "Unknown";
}

}  // namespace dicom_viewer::services

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

#include "services/preprocessing/gaussian_smoother.hpp"

#include <cmath>

#include <itkCommand.h>
#include <itkDiscreteGaussianImageFilter.h>
#include <itkExtractImageFilter.h>

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

    void setCallback(GaussianSmoother::ProgressCallback callback) {
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
    GaussianSmoother::ProgressCallback callback_;
};

}  // anonymous namespace

/**
 * @brief PIMPL implementation for GaussianSmoother
 */
class GaussianSmoother::Impl {
public:
    ProgressCallback progressCallback;
};

GaussianSmoother::GaussianSmoother() : impl_(std::make_unique<Impl>()) {}

GaussianSmoother::~GaussianSmoother() = default;

GaussianSmoother::GaussianSmoother(GaussianSmoother&&) noexcept = default;

GaussianSmoother& GaussianSmoother::operator=(GaussianSmoother&&) noexcept = default;

void GaussianSmoother::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<GaussianSmoother::ImageType::Pointer, PreprocessingError>
GaussianSmoother::apply(ImageType::Pointer input) const {
    return apply(input, Parameters{});
}

std::expected<GaussianSmoother::ImageType::Pointer, PreprocessingError>
GaussianSmoother::apply(ImageType::Pointer input, const Parameters& params) const {
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
            "Variance must be in range [0.1, 10.0] and maxKernelWidth in [0, 3-32]"
        });
    }

    try {
        using FilterType = itk::DiscreteGaussianImageFilter<ImageType, ImageType>;
        auto filter = FilterType::New();

        filter->SetInput(input);
        filter->SetVariance(params.variance);
        filter->SetUseImageSpacing(params.useImageSpacing);

        if (params.maxKernelWidth > 0) {
            filter->SetMaximumKernelWidth(params.maxKernelWidth);
        }

        // Attach progress observer if callback is set
        if (impl_->progressCallback) {
            auto observer = ProgressObserver::New();
            observer->setCallback(impl_->progressCallback);
            filter->AddObserver(itk::ProgressEvent(), observer);
        }

        filter->Update();

        return filter->GetOutput();
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

std::expected<GaussianSmoother::Image2DType::Pointer, PreprocessingError>
GaussianSmoother::applyToSlice(ImageType::Pointer input, unsigned int sliceIndex) const {
    return applyToSlice(input, sliceIndex, Parameters{});
}

std::expected<GaussianSmoother::Image2DType::Pointer, PreprocessingError>
GaussianSmoother::applyToSlice(
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
            "Variance must be in range [0.1, 10.0] and maxKernelWidth in [0, 3-32]"
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

        // Apply Gaussian smoothing to 2D slice
        using GaussianFilter2DType = itk::DiscreteGaussianImageFilter<Image2DType, Image2DType>;
        auto gaussianFilter = GaussianFilter2DType::New();

        gaussianFilter->SetInput(extractFilter->GetOutput());
        gaussianFilter->SetVariance(params.variance);
        gaussianFilter->SetUseImageSpacing(params.useImageSpacing);

        if (params.maxKernelWidth > 0) {
            gaussianFilter->SetMaximumKernelWidth(params.maxKernelWidth);
        }

        gaussianFilter->Update();

        return gaussianFilter->GetOutput();
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

std::array<unsigned int, 3> GaussianSmoother::getKernelRadius(
    const Parameters& params,
    const std::array<double, 3>& spacing
) {
    std::array<unsigned int, 3> radius{};

    // Calculate standard deviation from variance
    const double sigma = std::sqrt(params.variance);

    // Kernel radius is typically 3*sigma to capture 99.7% of Gaussian
    constexpr double sigmaFactor = 3.0;

    for (size_t i = 0; i < 3; ++i) {
        double effectiveSpacing = params.useImageSpacing ? spacing[i] : 1.0;
        double radiusInVoxels = (sigma * sigmaFactor) / effectiveSpacing;

        // Apply maxKernelWidth limit if specified
        if (params.maxKernelWidth > 0) {
            radiusInVoxels = std::min(radiusInVoxels,
                                       static_cast<double>(params.maxKernelWidth / 2));
        }

        radius[i] = static_cast<unsigned int>(std::ceil(radiusInVoxels));
    }

    return radius;
}

}  // namespace dicom_viewer::services

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

#include "services/preprocessing/anisotropic_diffusion_filter.hpp"

#include <cmath>

#include <itkCastImageFilter.h>
#include <itkCommand.h>
#include <itkCurvatureAnisotropicDiffusionImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkGradientAnisotropicDiffusionImageFilter.h>

namespace dicom_viewer::services {

namespace {

/**
 * @brief ITK progress observer for callback integration
 */
class DiffusionProgressObserver : public itk::Command {
public:
    using Self = DiffusionProgressObserver;
    using Superclass = itk::Command;
    using Pointer = itk::SmartPointer<Self>;

    itkNewMacro(Self);

    void setCallback(AnisotropicDiffusionFilter::ProgressCallback callback) {
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
    AnisotropicDiffusionFilter::ProgressCallback callback_;
};

}  // anonymous namespace

/**
 * @brief PIMPL implementation for AnisotropicDiffusionFilter
 */
class AnisotropicDiffusionFilter::Impl {
public:
    ProgressCallback progressCallback;
};

AnisotropicDiffusionFilter::AnisotropicDiffusionFilter()
    : impl_(std::make_unique<Impl>()) {}

AnisotropicDiffusionFilter::~AnisotropicDiffusionFilter() = default;

AnisotropicDiffusionFilter::AnisotropicDiffusionFilter(
    AnisotropicDiffusionFilter&&) noexcept = default;

AnisotropicDiffusionFilter& AnisotropicDiffusionFilter::operator=(
    AnisotropicDiffusionFilter&&) noexcept = default;

void AnisotropicDiffusionFilter::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<AnisotropicDiffusionFilter::InputImageType::Pointer, PreprocessingError>
AnisotropicDiffusionFilter::apply(InputImageType::Pointer input) const {
    return apply(input, Parameters{});
}

std::expected<AnisotropicDiffusionFilter::InputImageType::Pointer, PreprocessingError>
AnisotropicDiffusionFilter::apply(
    InputImageType::Pointer input,
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
            "numberOfIterations must be in [1, 50], conductance in [0.5, 10.0], "
            "timeStep in [0.0, 0.125]"
        });
    }

    try {
        // Cast input to float for diffusion filter (required by ITK)
        using CastToFloatType = itk::CastImageFilter<InputImageType, InternalImageType>;
        auto castToFloat = CastToFloatType::New();
        castToFloat->SetInput(input);

        // Apply curvature anisotropic diffusion
        using DiffusionFilterType = itk::CurvatureAnisotropicDiffusionImageFilter<
            InternalImageType, InternalImageType>;
        auto diffusionFilter = DiffusionFilterType::New();

        diffusionFilter->SetInput(castToFloat->GetOutput());
        diffusionFilter->SetNumberOfIterations(
            static_cast<unsigned int>(params.numberOfIterations));
        diffusionFilter->SetConductanceParameter(params.conductance);
        diffusionFilter->SetUseImageSpacing(params.useImageSpacing);

        // Set time step (automatic if 0)
        double effectiveTimeStep = params.timeStep;
        if (effectiveTimeStep <= 0.0) {
            effectiveTimeStep = Parameters::getDefaultTimeStep();
        }
        diffusionFilter->SetTimeStep(effectiveTimeStep);

        // Attach progress observer if callback is set
        if (impl_->progressCallback) {
            auto observer = DiffusionProgressObserver::New();
            observer->setCallback(impl_->progressCallback);
            diffusionFilter->AddObserver(itk::ProgressEvent(), observer);
        }

        // Cast back to short for output
        using CastToShortType = itk::CastImageFilter<InternalImageType, InputImageType>;
        auto castToShort = CastToShortType::New();
        castToShort->SetInput(diffusionFilter->GetOutput());

        castToShort->Update();

        return castToShort->GetOutput();
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

std::expected<AnisotropicDiffusionFilter::Input2DImageType::Pointer, PreprocessingError>
AnisotropicDiffusionFilter::applyToSlice(
    InputImageType::Pointer input,
    unsigned int sliceIndex
) const {
    return applyToSlice(input, sliceIndex, Parameters{});
}

std::expected<AnisotropicDiffusionFilter::Input2DImageType::Pointer, PreprocessingError>
AnisotropicDiffusionFilter::applyToSlice(
    InputImageType::Pointer input,
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
            "numberOfIterations must be in [1, 50], conductance in [0.5, 10.0], "
            "timeStep in [0.0, 0.125]"
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
        using ExtractFilterType = itk::ExtractImageFilter<InputImageType, Input2DImageType>;
        auto extractFilter = ExtractFilterType::New();
        extractFilter->SetDirectionCollapseToSubmatrix();

        InputImageType::RegionType extractRegion = region;
        extractRegion.SetSize(2, 0);  // Collapse Z dimension
        extractRegion.SetIndex(2, sliceIndex);

        extractFilter->SetExtractionRegion(extractRegion);
        extractFilter->SetInput(input);

        // Cast to float for diffusion
        using Float2DType = itk::Image<float, 2>;
        using CastToFloat2DType = itk::CastImageFilter<Input2DImageType, Float2DType>;
        auto castToFloat = CastToFloat2DType::New();
        castToFloat->SetInput(extractFilter->GetOutput());

        // Apply 2D diffusion filter
        using DiffusionFilter2DType = itk::CurvatureAnisotropicDiffusionImageFilter<
            Float2DType, Float2DType>;
        auto diffusionFilter = DiffusionFilter2DType::New();

        diffusionFilter->SetInput(castToFloat->GetOutput());
        diffusionFilter->SetNumberOfIterations(
            static_cast<unsigned int>(params.numberOfIterations));
        diffusionFilter->SetConductanceParameter(params.conductance);
        diffusionFilter->SetUseImageSpacing(params.useImageSpacing);

        // 2D time step limit is different: 1/4 = 0.25
        double effectiveTimeStep = params.timeStep;
        if (effectiveTimeStep <= 0.0) {
            effectiveTimeStep = 0.125;  // Safe default for 2D
        }
        diffusionFilter->SetTimeStep(effectiveTimeStep);

        // Cast back to short
        using CastToShort2DType = itk::CastImageFilter<Float2DType, Input2DImageType>;
        auto castToShort = CastToShort2DType::New();
        castToShort->SetInput(diffusionFilter->GetOutput());

        castToShort->Update();

        return castToShort->GetOutput();
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

double AnisotropicDiffusionFilter::estimateProcessingTime(
    const std::array<unsigned int, 3>& imageSize,
    const Parameters& params
) {
    // Empirical estimation based on image size and iterations
    // Time complexity: O(N * iterations) where N = total voxels
    constexpr double voxelsPerSecond = 1e7;  // Approximate processing rate

    const double totalVoxels = static_cast<double>(imageSize[0]) *
                               static_cast<double>(imageSize[1]) *
                               static_cast<double>(imageSize[2]);

    const double estimatedSeconds = (totalVoxels * params.numberOfIterations) /
                                    voxelsPerSecond;

    return estimatedSeconds;
}

}  // namespace dicom_viewer::services

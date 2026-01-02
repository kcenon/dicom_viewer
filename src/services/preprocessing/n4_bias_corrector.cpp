#include "services/preprocessing/n4_bias_corrector.hpp"

#include <cmath>

#include <itkCastImageFilter.h>
#include <itkCommand.h>
#include <itkDivideImageFilter.h>
#include <itkIdentityTransform.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkN4BiasFieldCorrectionImageFilter.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkOtsuThresholdImageFilter.h>
#include <itkResampleImageFilter.h>

namespace dicom_viewer::services {

namespace {

/**
 * @brief ITK progress observer for N4 bias correction callback integration
 */
class N4ProgressObserver : public itk::Command {
public:
    using Self = N4ProgressObserver;
    using Superclass = itk::Command;
    using Pointer = itk::SmartPointer<Self>;

    itkNewMacro(Self);

    void setCallback(N4BiasCorrector::ProgressCallback callback) {
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
    N4BiasCorrector::ProgressCallback callback_;
};

}  // anonymous namespace

/**
 * @brief PIMPL implementation for N4BiasCorrector
 */
class N4BiasCorrector::Impl {
public:
    ProgressCallback progressCallback;
};

N4BiasCorrector::N4BiasCorrector()
    : impl_(std::make_unique<Impl>()) {}

N4BiasCorrector::~N4BiasCorrector() = default;

N4BiasCorrector::N4BiasCorrector(N4BiasCorrector&&) noexcept = default;

N4BiasCorrector& N4BiasCorrector::operator=(N4BiasCorrector&&) noexcept = default;

void N4BiasCorrector::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<N4BiasCorrector::Result, PreprocessingError>
N4BiasCorrector::apply(InputImageType::Pointer input) const {
    return apply(input, Parameters{}, nullptr);
}

std::expected<N4BiasCorrector::Result, PreprocessingError>
N4BiasCorrector::apply(
    InputImageType::Pointer input,
    const Parameters& params
) const {
    return apply(input, params, nullptr);
}

std::expected<N4BiasCorrector::Result, PreprocessingError>
N4BiasCorrector::apply(
    InputImageType::Pointer input,
    const Parameters& params,
    MaskImageType::Pointer mask
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
            "Invalid N4 correction parameters: check shrinkFactor, "
            "numberOfFittingLevels, maxIterationsPerLevel, and convergenceThreshold"
        });
    }

    try {
        // Cast input to float for N4 filter (required by ITK)
        using CastToFloatType = itk::CastImageFilter<InputImageType, FloatImageType>;
        auto castToFloat = CastToFloatType::New();
        castToFloat->SetInput(input);
        castToFloat->Update();

        FloatImageType::Pointer floatImage = castToFloat->GetOutput();

        // Store original image information for resampling
        auto originalSize = floatImage->GetLargestPossibleRegion().GetSize();
        auto originalSpacing = floatImage->GetSpacing();
        auto originalOrigin = floatImage->GetOrigin();
        auto originalDirection = floatImage->GetDirection();

        // Calculate shrunk image properties
        FloatImageType::SizeType shrunkSize;
        FloatImageType::SpacingType shrunkSpacing;
        for (unsigned int d = 0; d < 3; ++d) {
            shrunkSize[d] = originalSize[d] / params.shrinkFactor;
            if (shrunkSize[d] < 1) shrunkSize[d] = 1;
            shrunkSpacing[d] = originalSpacing[d] * params.shrinkFactor;
        }

        // Resample input image to shrunk size
        using TransformType = itk::IdentityTransform<double, 3>;
        using InterpolatorType = itk::LinearInterpolateImageFunction<FloatImageType, double>;
        using ResampleFilterType = itk::ResampleImageFilter<FloatImageType, FloatImageType>;

        auto resampleShrink = ResampleFilterType::New();
        resampleShrink->SetInput(floatImage);
        resampleShrink->SetSize(shrunkSize);
        resampleShrink->SetOutputSpacing(shrunkSpacing);
        resampleShrink->SetOutputOrigin(originalOrigin);
        resampleShrink->SetOutputDirection(originalDirection);
        resampleShrink->SetTransform(TransformType::New());
        resampleShrink->SetInterpolator(InterpolatorType::New());
        resampleShrink->SetDefaultPixelValue(0);
        resampleShrink->Update();

        FloatImageType::Pointer shrunkImage = resampleShrink->GetOutput();

        // Create or resample mask
        MaskImageType::Pointer shrunkMask;
        if (mask) {
            // Resample the provided mask
            using MaskInterpolatorType = itk::NearestNeighborInterpolateImageFunction<
                MaskImageType, double>;
            using ResampleMaskType = itk::ResampleImageFilter<MaskImageType, MaskImageType>;

            auto resampleMask = ResampleMaskType::New();
            resampleMask->SetInput(mask);
            resampleMask->SetSize(shrunkSize);
            resampleMask->SetOutputSpacing(shrunkSpacing);
            resampleMask->SetOutputOrigin(originalOrigin);
            resampleMask->SetOutputDirection(originalDirection);
            resampleMask->SetTransform(TransformType::New());
            resampleMask->SetInterpolator(MaskInterpolatorType::New());
            resampleMask->SetDefaultPixelValue(0);
            resampleMask->Update();
            shrunkMask = resampleMask->GetOutput();
        } else {
            // Generate mask using Otsu thresholding on the shrunk image
            using OtsuFilterType = itk::OtsuThresholdImageFilter<
                FloatImageType, MaskImageType>;
            auto otsuFilter = OtsuFilterType::New();
            otsuFilter->SetInput(shrunkImage);
            otsuFilter->SetInsideValue(0);
            otsuFilter->SetOutsideValue(1);
            otsuFilter->Update();
            shrunkMask = otsuFilter->GetOutput();
        }

        // Configure N4 bias field correction filter
        using N4FilterType = itk::N4BiasFieldCorrectionImageFilter<
            FloatImageType, MaskImageType, FloatImageType>;
        auto n4Filter = N4FilterType::New();

        n4Filter->SetInput(shrunkImage);
        n4Filter->SetMaskImage(shrunkMask);

        // Set fitting levels and iterations
        N4FilterType::VariableSizeArrayType maxIterations(
            params.numberOfFittingLevels);
        for (int i = 0; i < params.numberOfFittingLevels; ++i) {
            maxIterations[i] = static_cast<unsigned int>(
                params.maxIterationsPerLevel[i]);
        }
        n4Filter->SetMaximumNumberOfIterations(maxIterations);

        n4Filter->SetNumberOfFittingLevels(
            static_cast<unsigned int>(params.numberOfFittingLevels));
        n4Filter->SetConvergenceThreshold(params.convergenceThreshold);
        n4Filter->SetSplineOrder(static_cast<unsigned int>(params.splineOrder));

        // Set B-spline control points
        N4FilterType::ArrayType numberOfControlPoints;
        numberOfControlPoints.Fill(
            static_cast<unsigned int>(params.numberOfControlPoints));
        n4Filter->SetNumberOfControlPoints(numberOfControlPoints);

        // Set Wiener filter noise if specified
        if (params.wienerFilterNoise > 0.0) {
            n4Filter->SetWienerFilterNoise(params.wienerFilterNoise);
        }

        n4Filter->SetBiasFieldFullWidthAtHalfMaximum(
            params.biasFieldFullWidthAtHalfMaximum);

        // Attach progress observer if callback is set
        if (impl_->progressCallback) {
            auto observer = N4ProgressObserver::New();
            observer->setCallback(impl_->progressCallback);
            n4Filter->AddObserver(itk::ProgressEvent(), observer);
        }

        n4Filter->Update();

        // Get corrected shrunk image
        FloatImageType::Pointer correctedShrunkImage = n4Filter->GetOutput();

        // Calculate bias field on shrunk image: biasField = original / corrected
        using DivideFilterType = itk::DivideImageFilter<
            FloatImageType, FloatImageType, FloatImageType>;
        auto divideForBias = DivideFilterType::New();
        divideForBias->SetInput1(shrunkImage);
        divideForBias->SetInput2(correctedShrunkImage);
        divideForBias->Update();

        FloatImageType::Pointer shrunkBiasField = divideForBias->GetOutput();

        // Resample bias field to full resolution
        auto resampleBias = ResampleFilterType::New();
        resampleBias->SetInput(shrunkBiasField);
        resampleBias->SetSize(originalSize);
        resampleBias->SetOutputSpacing(originalSpacing);
        resampleBias->SetOutputOrigin(originalOrigin);
        resampleBias->SetOutputDirection(originalDirection);
        resampleBias->SetTransform(TransformType::New());
        resampleBias->SetInterpolator(InterpolatorType::New());
        resampleBias->SetDefaultPixelValue(1.0);  // Default to no correction
        resampleBias->Update();

        FloatImageType::Pointer biasField = resampleBias->GetOutput();

        // Apply bias correction to full resolution image
        auto divideFilter = DivideFilterType::New();
        divideFilter->SetInput1(floatImage);
        divideFilter->SetInput2(biasField);
        divideFilter->Update();

        // Cast back to short for output
        using CastToShortType = itk::CastImageFilter<FloatImageType, InputImageType>;
        auto castToShort = CastToShortType::New();
        castToShort->SetInput(divideFilter->GetOutput());
        castToShort->Update();

        Result result;
        result.correctedImage = castToShort->GetOutput();
        result.biasField = biasField;

        return result;
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

double N4BiasCorrector::estimateProcessingTime(
    const std::array<unsigned int, 3>& imageSize,
    const Parameters& params
) {
    // Empirical estimation based on image size, shrink factor, and iterations
    // N4 is computationally intensive due to B-spline fitting
    constexpr double voxelsPerSecond = 5e5;  // Lower than diffusion due to complexity

    const double shrunkVoxels =
        (static_cast<double>(imageSize[0]) / params.shrinkFactor) *
        (static_cast<double>(imageSize[1]) / params.shrinkFactor) *
        (static_cast<double>(imageSize[2]) / params.shrinkFactor);

    // Total iterations across all levels
    int totalIterations = 0;
    for (int iter : params.maxIterationsPerLevel) {
        totalIterations += iter;
    }

    const double estimatedSeconds = (shrunkVoxels * totalIterations) / voxelsPerSecond;

    return estimatedSeconds;
}

}  // namespace dicom_viewer::services

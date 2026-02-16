#include "services/segmentation/mask_smoother.hpp"

#include <cmath>

#include <itkBinaryThresholdImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkDiscreteGaussianImageFilter.h>
#include <itkImageRegionConstIterator.h>

namespace dicom_viewer::services {

size_t MaskSmoother::countForeground(const BinaryMaskType* mask,
                                      uint8_t foregroundValue) {
    if (!mask) {
        return 0;
    }

    size_t count = 0;
    itk::ImageRegionConstIterator<BinaryMaskType> it(
        mask, mask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() == foregroundValue) {
            ++count;
        }
    }
    return count;
}

size_t MaskSmoother::countAboveThreshold(const FloatImageType* image,
                                          float threshold) {
    if (!image) {
        return 0;
    }

    size_t count = 0;
    itk::ImageRegionConstIterator<FloatImageType> it(
        image, image->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() >= threshold) {
            ++count;
        }
    }
    return count;
}

std::expected<MaskSmoother::BinaryMaskType::Pointer, SegmentationError>
MaskSmoother::smooth(BinaryMaskType::Pointer input, const Config& config) {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input mask is null"});
    }

    if (config.sigmaMm <= 0.0) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Sigma must be positive"});
    }

    // Count original foreground volume
    size_t originalVolume = countForeground(input.GetPointer(),
                                             config.foregroundValue);
    if (originalVolume == 0) {
        // Nothing to smooth — return copy of empty mask
        using DuplicatorType =
            itk::CastImageFilter<BinaryMaskType, BinaryMaskType>;
        auto dup = DuplicatorType::New();
        dup->SetInput(input);
        dup->Update();
        return dup->GetOutput();
    }

    // Step 1: Cast uint8 mask to float [0, 1]
    using CastToFloatType =
        itk::CastImageFilter<BinaryMaskType, FloatImageType>;
    auto castToFloat = CastToFloatType::New();
    castToFloat->SetInput(input);
    castToFloat->Update();

    // Normalize: foreground value → 1.0, background → 0.0
    auto floatImage = castToFloat->GetOutput();
    if (config.foregroundValue != 1) {
        float scale = 1.0f / static_cast<float>(config.foregroundValue);
        itk::ImageRegionIterator<FloatImageType> it(
            floatImage, floatImage->GetLargestPossibleRegion());
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            it.Set(it.Get() * scale);
        }
    }

    // Step 2: Apply Gaussian smoothing
    using GaussianFilterType =
        itk::DiscreteGaussianImageFilter<FloatImageType, FloatImageType>;
    auto gaussian = GaussianFilterType::New();
    gaussian->SetInput(floatImage);
    gaussian->SetVariance(config.sigmaMm * config.sigmaMm);
    gaussian->SetUseImageSpacingOn();
    gaussian->Update();
    auto smoothed = gaussian->GetOutput();

    // Step 3: Binary search for threshold that preserves volume
    float lo = 0.0f;
    float hi = 1.0f;
    float bestThreshold = 0.5f;
    size_t bestDiff = originalVolume;

    for (int i = 0; i < config.maxBinarySearchIter; ++i) {
        float mid = (lo + hi) / 2.0f;
        size_t midVolume = countAboveThreshold(smoothed, mid);

        size_t diff = (midVolume > originalVolume)
                          ? (midVolume - originalVolume)
                          : (originalVolume - midVolume);

        if (diff < bestDiff) {
            bestDiff = diff;
            bestThreshold = mid;
        }

        // Check tolerance
        double ratio = static_cast<double>(diff) /
                       static_cast<double>(originalVolume);
        if (ratio <= config.volumeTolerance) {
            break;
        }

        if (midVolume > originalVolume) {
            // Too many voxels → raise threshold
            lo = mid;
        } else {
            // Too few voxels → lower threshold
            hi = mid;
        }
    }

    // Step 4: Threshold at optimal level
    using ThresholdFilterType =
        itk::BinaryThresholdImageFilter<FloatImageType, BinaryMaskType>;
    auto threshold = ThresholdFilterType::New();
    threshold->SetInput(smoothed);
    threshold->SetLowerThreshold(bestThreshold);
    threshold->SetUpperThreshold(1.0f);
    threshold->SetInsideValue(config.foregroundValue);
    threshold->SetOutsideValue(0);
    threshold->Update();

    return threshold->GetOutput();
}

std::expected<MaskSmoother::BinaryMaskType::Pointer, SegmentationError>
MaskSmoother::smooth(BinaryMaskType::Pointer input, double sigmaMm) {
    Config config;
    config.sigmaMm = sigmaMm;
    return smooth(std::move(input), config);
}

}  // namespace dicom_viewer::services

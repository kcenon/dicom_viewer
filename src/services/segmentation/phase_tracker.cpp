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

#include "services/segmentation/phase_tracker.hpp"

#include <cmath>
#include <format>

#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkDemonsRegistrationFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>
#include <itkWarpImageFilter.h>

namespace dicom_viewer::services {

// =========================================================================
// Pimpl
// =========================================================================

class PhaseTracker::Impl {
public:
    PhaseTracker::ProgressCallback progressCallback;

    void reportProgress(int current, int total) const {
        if (progressCallback) {
            progressCallback(current, total);
        }
    }
};

PhaseTracker::PhaseTracker()
    : impl_(std::make_unique<Impl>()) {}

PhaseTracker::~PhaseTracker() = default;

PhaseTracker::PhaseTracker(PhaseTracker&&) noexcept = default;
PhaseTracker& PhaseTracker::operator=(PhaseTracker&&) noexcept = default;

void PhaseTracker::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

// =========================================================================
// Static: Displacement field computation
// =========================================================================

std::expected<PhaseTracker::DisplacementFieldType::Pointer, SegmentationError>
PhaseTracker::computeDisplacementField(
    FloatImage3D::Pointer fixedImage,
    FloatImage3D::Pointer movingImage,
    int iterations,
    double smoothingSigma) {

    if (!fixedImage || !movingImage) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Null image passed to computeDisplacementField"});
    }

    try {
        using DemonsFilterType =
            itk::DemonsRegistrationFilter<FloatImage3D, FloatImage3D,
                                          DisplacementFieldType>;

        auto demons = DemonsFilterType::New();
        demons->SetFixedImage(fixedImage);
        demons->SetMovingImage(movingImage);
        demons->SetNumberOfIterations(iterations);
        demons->SetStandardDeviations(smoothingSigma);
        demons->Update();

        return demons->GetOutput();
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::format("Demons registration failed: {}", e.GetDescription())});
    }
}

// =========================================================================
// Static: Mask warping
// =========================================================================

std::expected<PhaseTracker::LabelMapType::Pointer, SegmentationError>
PhaseTracker::warpMask(
    LabelMapType::Pointer mask,
    DisplacementFieldType::Pointer displacementField) {

    if (!mask || !displacementField) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Null input passed to warpMask"});
    }

    try {
        // Cast uint8 → float for warping (WarpImageFilter requires matching types)
        using CastToFloat =
            itk::CastImageFilter<LabelMapType, FloatImage3D>;
        auto castIn = CastToFloat::New();
        castIn->SetInput(mask);

        // Warp with nearest-neighbor to preserve label values
        using WarpFilterType =
            itk::WarpImageFilter<FloatImage3D, FloatImage3D,
                                 DisplacementFieldType>;
        auto warp = WarpFilterType::New();
        warp->SetInput(castIn->GetOutput());
        warp->SetDisplacementField(displacementField);
        warp->SetOutputSpacing(mask->GetSpacing());
        warp->SetOutputOrigin(mask->GetOrigin());
        warp->SetOutputDirection(mask->GetDirection());
        warp->SetOutputSize(mask->GetLargestPossibleRegion().GetSize());

        // Use nearest-neighbor interpolation for label data
        using InterpolatorType =
            itk::NearestNeighborInterpolateImageFunction<FloatImage3D, double>;
        auto interpolator = InterpolatorType::New();
        warp->SetInterpolator(interpolator);
        warp->Update();

        // Cast back float → uint8
        using CastToLabel =
            itk::CastImageFilter<FloatImage3D, LabelMapType>;
        auto castOut = CastToLabel::New();
        castOut->SetInput(warp->GetOutput());
        castOut->Update();

        return castOut->GetOutput();
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::format("Mask warping failed: {}", e.GetDescription())});
    }
}

// =========================================================================
// Static: Morphological closing
// =========================================================================

PhaseTracker::LabelMapType::Pointer
PhaseTracker::applyClosing(LabelMapType::Pointer mask, int radius) {
    if (!mask || radius <= 0) {
        return mask;
    }

    using StructuringElementType =
        itk::BinaryBallStructuringElement<uint8_t, 3>;
    StructuringElementType se;
    se.SetRadius(radius);
    se.CreateStructuringElement();

    using ClosingFilterType =
        itk::BinaryMorphologicalClosingImageFilter<LabelMapType,
                                                    LabelMapType,
                                                    StructuringElementType>;
    auto closing = ClosingFilterType::New();
    closing->SetInput(mask);
    closing->SetKernel(se);
    closing->SetForegroundValue(1);
    closing->Update();

    return closing->GetOutput();
}

// =========================================================================
// Static: Voxel counting
// =========================================================================

size_t PhaseTracker::countNonZeroVoxels(LabelMapType::Pointer mask) {
    if (!mask) {
        return 0;
    }

    size_t count = 0;
    itk::ImageRegionConstIterator<LabelMapType> it(
        mask, mask->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() != 0) {
            ++count;
        }
    }
    return count;
}

// =========================================================================
// Main propagation
// =========================================================================

std::expected<PhaseTracker::TrackingResult, SegmentationError>
PhaseTracker::propagateMask(
    LabelMapType::Pointer referenceMask,
    const std::vector<FloatImage3D::Pointer>& magnitudePhases,
    const TrackingConfig& config) const {

    if (!referenceMask) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Reference mask is null"});
    }

    const int numPhases = static_cast<int>(magnitudePhases.size());

    if (numPhases < 2) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "At least 2 magnitude phases are required"});
    }

    if (config.referencePhase < 0 || config.referencePhase >= numPhases) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            std::format("Reference phase {} out of range [0, {})",
                        config.referencePhase, numPhases)});
    }

    for (int i = 0; i < numPhases; ++i) {
        if (!magnitudePhases[i]) {
            return std::unexpected(SegmentationError{
                SegmentationError::Code::InvalidInput,
                std::format("Magnitude phase {} is null", i)});
        }
    }

    // Initialize result
    TrackingResult result;
    result.referencePhase = config.referencePhase;
    result.phases.resize(numPhases);

    // Reference phase: copy mask directly
    result.phases[config.referencePhase].mask = referenceMask;
    result.phases[config.referencePhase].volumeRatio = 1.0;

    const size_t refVolume = countNonZeroVoxels(referenceMask);
    int progressCount = 0;
    const int totalSteps = numPhases - 1;

    // Forward propagation: reference → reference+1 → ... → last
    auto currentMask = referenceMask;
    for (int i = config.referencePhase + 1; i < numPhases; ++i) {
        // Compute displacement: phase[i-1] → phase[i]
        auto fieldResult = computeDisplacementField(
            magnitudePhases[i], magnitudePhases[i - 1],
            config.registrationIterations, config.smoothingSigma);

        if (!fieldResult) {
            return std::unexpected(fieldResult.error());
        }

        // Warp current mask with displacement field
        auto warpResult = warpMask(currentMask, *fieldResult);
        if (!warpResult) {
            return std::unexpected(warpResult.error());
        }

        currentMask = *warpResult;

        // Optional morphological closing
        if (config.applyMorphologicalClosing) {
            currentMask = applyClosing(currentMask, config.closingRadius);
        }

        // Volume consistency check
        const size_t phaseVolume = countNonZeroVoxels(currentMask);
        double ratio = (refVolume > 0)
            ? static_cast<double>(phaseVolume) / static_cast<double>(refVolume)
            : 0.0;

        result.phases[i].mask = currentMask;
        result.phases[i].volumeRatio = ratio;
        result.phases[i].qualityWarning =
            std::abs(ratio - 1.0) > config.volumeDeviationThreshold;
        if (result.phases[i].qualityWarning) {
            ++result.warningCount;
        }

        ++progressCount;
        impl_->reportProgress(progressCount, totalSteps);
    }

    // Backward propagation: reference → reference-1 → ... → first
    currentMask = referenceMask;
    for (int i = config.referencePhase - 1; i >= 0; --i) {
        // Compute displacement: phase[i+1] → phase[i]
        auto fieldResult = computeDisplacementField(
            magnitudePhases[i], magnitudePhases[i + 1],
            config.registrationIterations, config.smoothingSigma);

        if (!fieldResult) {
            return std::unexpected(fieldResult.error());
        }

        // Warp current mask with displacement field
        auto warpResult = warpMask(currentMask, *fieldResult);
        if (!warpResult) {
            return std::unexpected(warpResult.error());
        }

        currentMask = *warpResult;

        // Optional morphological closing
        if (config.applyMorphologicalClosing) {
            currentMask = applyClosing(currentMask, config.closingRadius);
        }

        // Volume consistency check
        const size_t phaseVolume = countNonZeroVoxels(currentMask);
        double ratio = (refVolume > 0)
            ? static_cast<double>(phaseVolume) / static_cast<double>(refVolume)
            : 0.0;

        result.phases[i].mask = currentMask;
        result.phases[i].volumeRatio = ratio;
        result.phases[i].qualityWarning =
            std::abs(ratio - 1.0) > config.volumeDeviationThreshold;
        if (result.phases[i].qualityWarning) {
            ++result.warningCount;
        }

        ++progressCount;
        impl_->reportProgress(progressCount, totalSteps);
    }

    return result;
}

}  // namespace dicom_viewer::services

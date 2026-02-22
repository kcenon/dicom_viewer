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

#include "services/segmentation/hollow_tool.hpp"

#include <algorithm>
#include <cmath>

#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryDilateImageFilter.h>
#include <itkBinaryErodeImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkSubtractImageFilter.h>

namespace dicom_viewer::services {

int HollowTool::mmToVoxelRadius(const BinaryMaskType* image,
                                 double thicknessMm) {
    if (!image || thicknessMm <= 0.0) {
        return 1;
    }

    auto spacing = image->GetSpacing();
    double minSpacing = std::min({spacing[0], spacing[1], spacing[2]});
    if (minSpacing <= 0.0) {
        return 1;
    }

    int radius = static_cast<int>(std::round(thicknessMm / minSpacing));
    return std::max(radius, 1);
}

namespace {

using BinaryMaskType = HollowTool::BinaryMaskType;
using StructuringElementType =
    itk::BinaryBallStructuringElement<uint8_t, 3>;

/// Create a ball structuring element with the given radius
StructuringElementType createBall(int radius) {
    StructuringElementType element;
    element.SetRadius(radius);
    element.CreateStructuringElement();
    return element;
}

/// Erode a binary mask
BinaryMaskType::Pointer erodeMask(BinaryMaskType::Pointer input,
                                   int radius, uint8_t fg) {
    auto element = createBall(radius);

    using FilterType =
        itk::BinaryErodeImageFilter<BinaryMaskType, BinaryMaskType,
                                     StructuringElementType>;
    auto filter = FilterType::New();
    filter->SetInput(input);
    filter->SetKernel(element);
    filter->SetForegroundValue(fg);
    filter->SetBackgroundValue(0);
    filter->Update();
    return filter->GetOutput();
}

/// Dilate a binary mask
BinaryMaskType::Pointer dilateMask(BinaryMaskType::Pointer input,
                                    int radius, uint8_t fg) {
    auto element = createBall(radius);

    using FilterType =
        itk::BinaryDilateImageFilter<BinaryMaskType, BinaryMaskType,
                                      StructuringElementType>;
    auto filter = FilterType::New();
    filter->SetInput(input);
    filter->SetKernel(element);
    filter->SetForegroundValue(fg);
    filter->SetBackgroundValue(0);
    filter->Update();
    return filter->GetOutput();
}

/// Subtract B from A: result = A AND NOT B
BinaryMaskType::Pointer subtractMask(BinaryMaskType::Pointer a,
                                      BinaryMaskType::Pointer b,
                                      uint8_t fg) {
    auto result = BinaryMaskType::New();
    result->SetRegions(a->GetLargestPossibleRegion());
    result->SetSpacing(a->GetSpacing());
    result->SetOrigin(a->GetOrigin());
    result->SetDirection(a->GetDirection());
    result->Allocate();
    result->FillBuffer(0);

    itk::ImageRegionConstIterator<BinaryMaskType> itA(
        a, a->GetLargestPossibleRegion());
    itk::ImageRegionConstIterator<BinaryMaskType> itB(
        b, b->GetLargestPossibleRegion());
    itk::ImageRegionIterator<BinaryMaskType> itOut(
        result, result->GetLargestPossibleRegion());

    for (itA.GoToBegin(), itB.GoToBegin(), itOut.GoToBegin();
         !itA.IsAtEnd(); ++itA, ++itB, ++itOut) {
        if (itA.Get() == fg && itB.Get() != fg) {
            itOut.Set(fg);
        }
    }

    return result;
}

/// OR two masks: result = A OR B
BinaryMaskType::Pointer orMask(BinaryMaskType::Pointer a,
                                BinaryMaskType::Pointer b,
                                uint8_t fg) {
    auto result = BinaryMaskType::New();
    result->SetRegions(a->GetLargestPossibleRegion());
    result->SetSpacing(a->GetSpacing());
    result->SetOrigin(a->GetOrigin());
    result->SetDirection(a->GetDirection());
    result->Allocate();
    result->FillBuffer(0);

    itk::ImageRegionConstIterator<BinaryMaskType> itA(
        a, a->GetLargestPossibleRegion());
    itk::ImageRegionConstIterator<BinaryMaskType> itB(
        b, b->GetLargestPossibleRegion());
    itk::ImageRegionIterator<BinaryMaskType> itOut(
        result, result->GetLargestPossibleRegion());

    for (itA.GoToBegin(), itB.GoToBegin(), itOut.GoToBegin();
         !itA.IsAtEnd(); ++itA, ++itB, ++itOut) {
        if (itA.Get() == fg || itB.Get() == fg) {
            itOut.Set(fg);
        }
    }

    return result;
}

}  // anonymous namespace

std::expected<BinaryMaskType::Pointer, SegmentationError>
HollowTool::makeHollow(BinaryMaskType::Pointer input, const Config& config) {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input mask is null"});
    }

    if (config.thicknessMm <= 0.0) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Shell thickness must be positive"});
    }

    int radius = mmToVoxelRadius(input.GetPointer(), config.thicknessMm);
    uint8_t fg = config.foregroundValue;

    switch (config.direction) {
        case HollowDirection::Inside: {
            // Shell = original - eroded
            auto eroded = erodeMask(input, radius, fg);
            return subtractMask(input, eroded, fg);
        }

        case HollowDirection::Outside: {
            // Shell = dilated - original
            auto dilated = dilateMask(input, radius, fg);
            return subtractMask(dilated, input, fg);
        }

        case HollowDirection::Both: {
            // Shell = (dilated - eroded) where overlap with original boundary
            int halfRadius = std::max(radius / 2, 1);
            auto eroded = erodeMask(input, halfRadius, fg);
            auto dilated = dilateMask(input, halfRadius, fg);
            return subtractMask(dilated, eroded, fg);
        }
    }

    return std::unexpected(SegmentationError{
        SegmentationError::Code::InternalError,
        "Unknown hollow direction"});
}

std::expected<BinaryMaskType::Pointer, SegmentationError>
HollowTool::makeHollow(BinaryMaskType::Pointer input, double thicknessMm) {
    Config config;
    config.thicknessMm = thicknessMm;
    return makeHollow(std::move(input), config);
}

}  // namespace dicom_viewer::services

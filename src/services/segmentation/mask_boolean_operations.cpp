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

#include "services/segmentation/mask_boolean_operations.hpp"

#include <sstream>

namespace dicom_viewer::services {

std::expected<void, SegmentationError>
MaskBooleanOperations::validateCompatibility(
    LabelMapType::Pointer maskA,
    LabelMapType::Pointer maskB)
{
    if (!maskA || !maskB) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Null label map pointer"});
    }

    auto sizeA = maskA->GetLargestPossibleRegion().GetSize();
    auto sizeB = maskB->GetLargestPossibleRegion().GetSize();

    if (sizeA != sizeB) {
        std::ostringstream oss;
        oss << "Dimension mismatch: A=" << sizeA[0] << "x" << sizeA[1] << "x" << sizeA[2]
            << " vs B=" << sizeB[0] << "x" << sizeB[1] << "x" << sizeB[2];
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            oss.str()});
    }

    auto spacingA = maskA->GetSpacing();
    auto spacingB = maskB->GetSpacing();
    constexpr double kTolerance = 1e-6;
    for (unsigned int d = 0; d < 3; ++d) {
        if (std::abs(spacingA[d] - spacingB[d]) > kTolerance) {
            return std::unexpected(SegmentationError{
                SegmentationError::Code::InvalidInput,
                "Spacing mismatch between masks"});
        }
    }

    return {};
}

MaskBooleanOperations::LabelMapType::Pointer
MaskBooleanOperations::createOutputMap(LabelMapType::Pointer source)
{
    auto output = LabelMapType::New();
    output->SetRegions(source->GetLargestPossibleRegion());
    output->SetSpacing(source->GetSpacing());
    output->SetOrigin(source->GetOrigin());
    output->SetDirection(source->GetDirection());
    output->Allocate(true);
    return output;
}

std::expected<MaskBooleanOperations::LabelMapType::Pointer, SegmentationError>
MaskBooleanOperations::computeUnion(
    LabelMapType::Pointer maskA,
    LabelMapType::Pointer maskB)
{
    auto validation = validateCompatibility(maskA, maskB);
    if (!validation) {
        return std::unexpected(validation.error());
    }

    auto output = createOutputMap(maskA);
    auto* bufA = maskA->GetBufferPointer();
    auto* bufB = maskB->GetBufferPointer();
    auto* bufOut = output->GetBufferPointer();

    auto size = maskA->GetLargestPossibleRegion().GetSize();
    size_t totalVoxels = size[0] * size[1] * size[2];

    for (size_t i = 0; i < totalVoxels; ++i) {
        bufOut[i] = (bufA[i] != 0) ? bufA[i] : bufB[i];
    }

    return output;
}

std::expected<MaskBooleanOperations::LabelMapType::Pointer, SegmentationError>
MaskBooleanOperations::computeDifference(
    LabelMapType::Pointer maskA,
    LabelMapType::Pointer maskB)
{
    auto validation = validateCompatibility(maskA, maskB);
    if (!validation) {
        return std::unexpected(validation.error());
    }

    auto output = createOutputMap(maskA);
    auto* bufA = maskA->GetBufferPointer();
    auto* bufB = maskB->GetBufferPointer();
    auto* bufOut = output->GetBufferPointer();

    auto size = maskA->GetLargestPossibleRegion().GetSize();
    size_t totalVoxels = size[0] * size[1] * size[2];

    for (size_t i = 0; i < totalVoxels; ++i) {
        bufOut[i] = (bufA[i] != 0 && bufB[i] == 0) ? bufA[i] : 0;
    }

    return output;
}

std::expected<MaskBooleanOperations::LabelMapType::Pointer, SegmentationError>
MaskBooleanOperations::computeIntersection(
    LabelMapType::Pointer maskA,
    LabelMapType::Pointer maskB)
{
    auto validation = validateCompatibility(maskA, maskB);
    if (!validation) {
        return std::unexpected(validation.error());
    }

    auto output = createOutputMap(maskA);
    auto* bufA = maskA->GetBufferPointer();
    auto* bufB = maskB->GetBufferPointer();
    auto* bufOut = output->GetBufferPointer();

    auto size = maskA->GetLargestPossibleRegion().GetSize();
    size_t totalVoxels = size[0] * size[1] * size[2];

    for (size_t i = 0; i < totalVoxels; ++i) {
        bufOut[i] = (bufA[i] != 0 && bufB[i] != 0) ? bufA[i] : 0;
    }

    return output;
}

std::expected<MaskBooleanOperations::LabelMapType::Pointer, SegmentationError>
MaskBooleanOperations::computeUnionMultiple(
    const std::vector<LabelMapType::Pointer>& masks)
{
    if (masks.size() < 2) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "At least 2 masks required for multi-mask union"});
    }

    auto result = computeUnion(masks[0], masks[1]);
    if (!result) {
        return result;
    }

    for (size_t i = 2; i < masks.size(); ++i) {
        result = computeUnion(*result, masks[i]);
        if (!result) {
            return result;
        }
    }

    return result;
}

}  // namespace dicom_viewer::services

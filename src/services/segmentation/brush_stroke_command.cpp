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

#include "services/segmentation/brush_stroke_command.hpp"

#include <utility>

namespace dicom_viewer::services {

BrushStrokeCommand::BrushStrokeCommand(
    LabelMapType::Pointer labelMap,
    std::string operationDescription)
    : labelMap_(std::move(labelMap))
    , description_(std::move(operationDescription))
{
}

void BrushStrokeCommand::recordChange(
    size_t linearIndex, uint8_t oldLabel, uint8_t newLabel)
{
    if (oldLabel != newLabel) {
        changes_.push_back({linearIndex, oldLabel, newLabel});
    }
}

size_t BrushStrokeCommand::changeCount() const noexcept
{
    return changes_.size();
}

bool BrushStrokeCommand::hasChanges() const noexcept
{
    return !changes_.empty();
}

void BrushStrokeCommand::execute()
{
    if (!labelMap_) return;

    auto* buffer = labelMap_->GetBufferPointer();
    for (const auto& change : changes_) {
        buffer[change.linearIndex] = change.newLabel;
    }
}

void BrushStrokeCommand::undo()
{
    if (!labelMap_) return;

    auto* buffer = labelMap_->GetBufferPointer();
    for (const auto& change : changes_) {
        buffer[change.linearIndex] = change.oldLabel;
    }
}

std::string BrushStrokeCommand::description() const
{
    return description_;
}

size_t BrushStrokeCommand::memoryUsage() const
{
    return changes_.size() * sizeof(VoxelChange) + description_.size();
}

} // namespace dicom_viewer::services

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

#include "services/segmentation/snapshot_command.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace dicom_viewer::services {

SnapshotCommand::SnapshotCommand(LabelMapType::Pointer labelMap,
                                 std::string operationDescription)
    : labelMap_(std::move(labelMap))
    , description_(std::move(operationDescription))
{
    if (!labelMap_) {
        throw std::invalid_argument("SnapshotCommand: null label map");
    }

    auto size = labelMap_->GetLargestPossibleRegion().GetSize();
    totalVoxels_ = size[0] * size[1] * size[2];

    // Capture "before" state immediately
    beforeState_ = compressRLE(labelMap_->GetBufferPointer(), totalVoxels_);
}

void SnapshotCommand::captureAfterState() {
    afterState_ = compressRLE(labelMap_->GetBufferPointer(), totalVoxels_);
}

bool SnapshotCommand::isComplete() const noexcept {
    return !afterState_.empty();
}

void SnapshotCommand::execute() {
    // Redo: restore the "after" state
    if (!afterState_.empty()) {
        restoreState(afterState_);
    }
}

void SnapshotCommand::undo() {
    restoreState(beforeState_);
}

std::string SnapshotCommand::description() const {
    return description_;
}

size_t SnapshotCommand::memoryUsage() const {
    return beforeState_.capacity() + afterState_.capacity()
           + description_.capacity()
           + sizeof(SnapshotCommand);
}

void SnapshotCommand::restoreState(const std::vector<uint8_t>& compressed) {
    decompressRLE(compressed, labelMap_->GetBufferPointer(), totalVoxels_);
    labelMap_->Modified();
}

// =============================================================================
// RLE compression
// =============================================================================

std::vector<uint8_t>
SnapshotCommand::compressRLE(const uint8_t* data, size_t numElements) {
    std::vector<uint8_t> result;
    // Reserve a reasonable estimate (worst case: all different = 5 bytes each)
    // Typical case: large runs → much smaller
    result.reserve(std::min(numElements * 5, size_t{4096}));

    size_t i = 0;
    while (i < numElements) {
        uint8_t value = data[i];
        uint32_t count = 1;

        // Count consecutive identical values
        while (i + count < numElements
               && data[i + count] == value
               && count < 0xFFFFFFFFu) {
            ++count;
        }

        // Write run: 1 byte value + 4 bytes count (little-endian)
        result.push_back(value);
        result.push_back(static_cast<uint8_t>(count & 0xFF));
        result.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>((count >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((count >> 24) & 0xFF));

        i += count;
    }

    return result;
}

void SnapshotCommand::decompressRLE(const std::vector<uint8_t>& compressed,
                                     uint8_t* output,
                                     size_t numElements) {
    size_t pos = 0;
    size_t outIdx = 0;

    while (pos + 4 < compressed.size() && outIdx < numElements) {
        uint8_t value = compressed[pos];
        uint32_t count =
            static_cast<uint32_t>(compressed[pos + 1])
            | (static_cast<uint32_t>(compressed[pos + 2]) << 8)
            | (static_cast<uint32_t>(compressed[pos + 3]) << 16)
            | (static_cast<uint32_t>(compressed[pos + 4]) << 24);
        pos += 5;

        // Clamp to remaining output space
        size_t writeCount = std::min(static_cast<size_t>(count),
                                     numElements - outIdx);
        std::memset(output + outIdx, value, writeCount);
        outIdx += writeCount;
    }
}

}  // namespace dicom_viewer::services

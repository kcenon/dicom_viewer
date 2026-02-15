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

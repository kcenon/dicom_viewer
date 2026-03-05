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

#include "services/render/dirty_region_tracker.hpp"

#include <algorithm>
#include <cstring>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class DirtyRegionTracker::Impl {
public:
    struct RowSpan {
        uint32_t minCol = UINT32_MAX;
        uint32_t maxCol = 0;
        bool dirty = false;
    };

    explicit Impl(const DirtyRegionConfig& config) : config_(config) {}

    DirtyRegionResult detect(
        const uint8_t* current, const uint8_t* previous,
        uint32_t width, uint32_t height) const
    {
        DirtyRegionResult result;

        if (!current || !previous || width == 0 || height == 0) {
            return result;
        }

        // Build a per-row dirty column mask using XOR comparison.
        // For each row, find the leftmost and rightmost dirty pixel.
        std::vector<RowSpan> rowSpans(height);
        size_t dirtyPixelCount = 0;
        const size_t stride = static_cast<size_t>(width) * 4;

        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t* curRow = current + y * stride;
            const uint8_t* prevRow = previous + y * stride;

            for (uint32_t x = 0; x < width; ++x) {
                size_t off = static_cast<size_t>(x) * 4;
                // Compare RGB channels (ignore alpha for change detection)
                if (curRow[off] != prevRow[off]
                    || curRow[off + 1] != prevRow[off + 1]
                    || curRow[off + 2] != prevRow[off + 2]) {
                    rowSpans[y].dirty = true;
                    rowSpans[y].minCol = std::min(rowSpans[y].minCol, x);
                    rowSpans[y].maxCol = std::max(rowSpans[y].maxCol, x);
                    ++dirtyPixelCount;
                }
            }
        }

        size_t totalPixels = static_cast<size_t>(width) * height;
        result.dirtyRatio = static_cast<double>(dirtyPixelCount)
                            / static_cast<double>(totalPixels);

        if (dirtyPixelCount == 0) {
            return result;
        }

        if (result.dirtyRatio > config_.fullFrameThreshold) {
            result.fullFrameRequired = true;
            DirtyRect fullRect;
            fullRect.x = 0;
            fullRect.y = 0;
            fullRect.width = width;
            fullRect.height = height;
            result.regions.push_back(fullRect);
            return result;
        }

        // Extract contiguous vertical runs of dirty rows into bounding boxes
        auto boxes = extractBoundingBoxes(rowSpans, width, height);

        // Merge nearby boxes
        result.regions = mergeBoxes(boxes, config_.mergeGap);

        return result;
    }

    DirtyRegionConfig config_;

private:
    static std::vector<DirtyRect> extractBoundingBoxes(
        const std::vector<RowSpan>& rowSpans,
        uint32_t /*width*/, uint32_t height)
    {
        std::vector<DirtyRect> boxes;
        uint32_t y = 0;

        while (y < height) {
            // Skip clean rows
            while (y < height && !rowSpans[y].dirty) {
                ++y;
            }
            if (y >= height) {
                break;
            }

            // Start a new dirty run
            uint32_t startY = y;
            uint32_t minCol = rowSpans[y].minCol;
            uint32_t maxCol = rowSpans[y].maxCol;

            while (y < height && rowSpans[y].dirty) {
                minCol = std::min(minCol, rowSpans[y].minCol);
                maxCol = std::max(maxCol, rowSpans[y].maxCol);
                ++y;
            }

            DirtyRect box;
            box.x = minCol;
            box.y = startY;
            box.width = maxCol - minCol + 1;
            box.height = y - startY;
            boxes.push_back(box);
        }

        return boxes;
    }

    static std::vector<DirtyRect> mergeBoxes(
        const std::vector<DirtyRect>& boxes, uint32_t gap)
    {
        if (boxes.size() <= 1) {
            return boxes;
        }

        // Greedy merge: repeatedly merge overlapping/nearby boxes
        std::vector<DirtyRect> merged = boxes;
        bool changed = true;

        while (changed) {
            changed = false;
            std::vector<DirtyRect> next;

            std::vector<bool> consumed(merged.size(), false);

            for (size_t i = 0; i < merged.size(); ++i) {
                if (consumed[i]) {
                    continue;
                }

                DirtyRect current = merged[i];

                for (size_t j = i + 1; j < merged.size(); ++j) {
                    if (consumed[j]) {
                        continue;
                    }

                    if (shouldMerge(current, merged[j], gap)) {
                        current = unionRect(current, merged[j]);
                        consumed[j] = true;
                        changed = true;
                    }
                }

                next.push_back(current);
            }

            merged = std::move(next);
        }

        return merged;
    }

    static bool shouldMerge(const DirtyRect& a, const DirtyRect& b,
                            uint32_t gap)
    {
        // Check if boxes overlap or are within gap pixels of each other
        uint32_t aRight = a.x + a.width;
        uint32_t aBottom = a.y + a.height;
        uint32_t bRight = b.x + b.width;
        uint32_t bBottom = b.y + b.height;

        bool xOverlap = (a.x <= bRight + gap) && (b.x <= aRight + gap);
        bool yOverlap = (a.y <= bBottom + gap) && (b.y <= aBottom + gap);

        return xOverlap && yOverlap;
    }

    static DirtyRect unionRect(const DirtyRect& a, const DirtyRect& b)
    {
        uint32_t minX = std::min(a.x, b.x);
        uint32_t minY = std::min(a.y, b.y);
        uint32_t maxX = std::max(a.x + a.width, b.x + b.width);
        uint32_t maxY = std::max(a.y + a.height, b.y + b.height);

        DirtyRect r;
        r.x = minX;
        r.y = minY;
        r.width = maxX - minX;
        r.height = maxY - minY;
        return r;
    }
};

// ---------------------------------------------------------------------------
// DirtyRegionTracker lifecycle
// ---------------------------------------------------------------------------
DirtyRegionTracker::DirtyRegionTracker(const DirtyRegionConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

DirtyRegionTracker::~DirtyRegionTracker() = default;

DirtyRegionTracker::DirtyRegionTracker(DirtyRegionTracker&&) noexcept = default;
DirtyRegionTracker& DirtyRegionTracker::operator=(DirtyRegionTracker&&) noexcept
    = default;

DirtyRegionResult DirtyRegionTracker::detect(
    const uint8_t* current, const uint8_t* previous,
    uint32_t width, uint32_t height) const
{
    return impl_->detect(current, previous, width, height);
}

std::vector<uint8_t> DirtyRegionTracker::extractRegion(
    const uint8_t* rgba, uint32_t frameWidth, uint32_t frameHeight,
    const DirtyRect& rect)
{
    if (!rgba || frameWidth == 0 || frameHeight == 0
        || rect.width == 0 || rect.height == 0) {
        return {};
    }

    // Clamp rect to frame bounds
    uint32_t x = std::min(rect.x, frameWidth);
    uint32_t y = std::min(rect.y, frameHeight);
    uint32_t w = std::min(rect.width, frameWidth - x);
    uint32_t h = std::min(rect.height, frameHeight - y);

    if (w == 0 || h == 0) {
        return {};
    }

    std::vector<uint8_t> tile(static_cast<size_t>(w) * h * 4);
    const size_t frameStride = static_cast<size_t>(frameWidth) * 4;
    const size_t tileStride = static_cast<size_t>(w) * 4;

    for (uint32_t row = 0; row < h; ++row) {
        const uint8_t* src = rgba + (y + row) * frameStride + x * 4;
        uint8_t* dst = tile.data() + row * tileStride;
        std::memcpy(dst, src, tileStride);
    }

    return tile;
}

void DirtyRegionTracker::applyRegion(
    uint8_t* base, uint32_t frameWidth, uint32_t frameHeight,
    const uint8_t* tile, const DirtyRect& rect)
{
    if (!base || !tile || frameWidth == 0 || frameHeight == 0
        || rect.width == 0 || rect.height == 0) {
        return;
    }

    uint32_t x = std::min(rect.x, frameWidth);
    uint32_t y = std::min(rect.y, frameHeight);
    uint32_t w = std::min(rect.width, frameWidth - x);
    uint32_t h = std::min(rect.height, frameHeight - y);

    if (w == 0 || h == 0) {
        return;
    }

    const size_t frameStride = static_cast<size_t>(frameWidth) * 4;
    const size_t tileStride = static_cast<size_t>(w) * 4;

    for (uint32_t row = 0; row < h; ++row) {
        uint8_t* dst = base + (y + row) * frameStride + x * 4;
        const uint8_t* src = tile + row * tileStride;
        std::memcpy(dst, src, tileStride);
    }
}

const DirtyRegionConfig& DirtyRegionTracker::config() const
{
    return impl_->config_;
}

void DirtyRegionTracker::setConfig(const DirtyRegionConfig& config)
{
    impl_->config_ = config;
}

} // namespace dicom_viewer::services

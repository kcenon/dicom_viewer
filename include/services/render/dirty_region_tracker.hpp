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

/**
 * @file dirty_region_tracker.hpp
 * @brief Tracks changed pixel regions between consecutive rendered frames
 * @details Computes XOR between two RGBA frames to identify bounding boxes
 *          of changed regions. Adjacent dirty regions within a configurable
 *          gap threshold are merged to reduce tile count.
 *
 * ## Algorithm
 * ```
 * frame_N (RGBA)
 *   -> XOR with frame_{N-1}
 *   -> scan for non-zero rows/columns
 *   -> compute bounding boxes of contiguous dirty regions
 *   -> merge nearby boxes (gap < mergeGap pixels)
 *   -> return vector of DirtyRect + dirty ratio
 * ```
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Axis-aligned bounding box of a changed region
 */
struct DirtyRect {
    uint32_t x = 0;      ///< Left edge (pixels from frame left)
    uint32_t y = 0;      ///< Top edge (pixels from frame top)
    uint32_t width = 0;   ///< Width in pixels
    uint32_t height = 0;  ///< Height in pixels
};

/**
 * @brief Result of dirty region detection between two frames
 */
struct DirtyRegionResult {
    std::vector<DirtyRect> regions;  ///< Bounding boxes of changed areas
    double dirtyRatio = 0.0;         ///< Fraction of frame area that changed (0.0-1.0)
    bool fullFrameRequired = false;  ///< True if dirty area exceeds threshold
};

/**
 * @brief Configuration for dirty region tracking
 */
struct DirtyRegionConfig {
    /// Gap threshold for merging nearby dirty regions (pixels)
    uint32_t mergeGap = 16;

    /// Dirty area ratio above which a full frame is required (0.0-1.0)
    double fullFrameThreshold = 0.60;
};

/**
 * @brief Detects changed pixel regions between consecutive RGBA frames
 *
 * Uses XOR-based pixel comparison to find bounding boxes of changed areas.
 * Merges nearby regions to reduce tile count. Returns full-frame flag when
 * the dirty area exceeds a configurable threshold.
 *
 * @trace SRS-FR-REMOTE-007
 */
class DirtyRegionTracker {
public:
    explicit DirtyRegionTracker(const DirtyRegionConfig& config = {});
    ~DirtyRegionTracker();

    // Non-copyable, movable
    DirtyRegionTracker(const DirtyRegionTracker&) = delete;
    DirtyRegionTracker& operator=(const DirtyRegionTracker&) = delete;
    DirtyRegionTracker(DirtyRegionTracker&&) noexcept;
    DirtyRegionTracker& operator=(DirtyRegionTracker&&) noexcept;

    /**
     * @brief Detect dirty regions between two RGBA frames
     * @param current Current frame RGBA data (width * height * 4 bytes)
     * @param previous Previous frame RGBA data (same dimensions)
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @return Dirty region detection result
     */
    [[nodiscard]] DirtyRegionResult detect(
        const uint8_t* current, const uint8_t* previous,
        uint32_t width, uint32_t height) const;

    /**
     * @brief Extract a rectangular sub-region from an RGBA frame
     * @param rgba Source frame RGBA data
     * @param frameWidth Full frame width
     * @param frameHeight Full frame height
     * @param rect Region to extract
     * @return RGBA pixel data for the sub-region
     */
    [[nodiscard]] static std::vector<uint8_t> extractRegion(
        const uint8_t* rgba, uint32_t frameWidth, uint32_t frameHeight,
        const DirtyRect& rect);

    /**
     * @brief Apply a tile's RGBA data onto a base frame
     * @param base Base frame RGBA data (modified in place)
     * @param frameWidth Full frame width
     * @param frameHeight Full frame height
     * @param tile RGBA pixel data for the tile
     * @param rect Position and size of the tile
     */
    static void applyRegion(
        uint8_t* base, uint32_t frameWidth, uint32_t frameHeight,
        const uint8_t* tile, const DirtyRect& rect);

    /**
     * @brief Get the current configuration
     */
    [[nodiscard]] const DirtyRegionConfig& config() const;

    /**
     * @brief Update configuration
     */
    void setConfig(const DirtyRegionConfig& config);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

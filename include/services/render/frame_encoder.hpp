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
 * @file frame_encoder.hpp
 * @brief Compressed frame encoder for render streaming
 * @details Converts raw RGBA pixel buffers from RenderSession::captureFrame()
 *          into compressed formats suitable for network transmission.
 *
 * Supported formats:
 * - JPEG: Lossy compression for interactive streaming (quality-adjustable)
 * - PNG: Lossless compression for annotations and small viewports
 * - H264Stream: Temporal sequence compression (requires ffmpeg, not yet available)
 *
 * ## Performance Targets
 * - JPEG encode of 512x512 at q=85: < 5ms
 * - JPEG output for 1024x768 at q=85: < 200KB
 *
 * ## Input Format
 * Accepts raw RGBA pixel buffers in VTK image order (bottom-to-top scanlines),
 * as produced by OffscreenRenderContext::captureFrame().
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
 * @brief Encoding format for frame compression
 */
enum class EncodeFormat {
    Jpeg,       ///< Lossy JPEG compression (quality-adjustable)
    Png,        ///< Lossless PNG compression
    H264Stream  ///< H.264 temporal compression (requires ffmpeg)
};

/**
 * @brief Frame encoder for converting RGBA buffers to compressed formats
 *
 * Uses VTK's IOImage writers (vtkJPEGWriter, vtkPNGWriter) with in-memory
 * output for JPEG and PNG encoding. H.264 requires ffmpeg integration
 * (not yet available).
 *
 * @trace SRS-FR-REMOTE-002
 */
class FrameEncoder {
public:
    FrameEncoder();
    ~FrameEncoder();

    // Non-copyable, movable
    FrameEncoder(const FrameEncoder&) = delete;
    FrameEncoder& operator=(const FrameEncoder&) = delete;
    FrameEncoder(FrameEncoder&&) noexcept;
    FrameEncoder& operator=(FrameEncoder&&) noexcept;

    /**
     * @brief Encode RGBA buffer to JPEG format
     * @param rgba Raw RGBA pixel data (width * height * 4 bytes)
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @param quality JPEG quality 1-100 (default 85)
     * @return Compressed JPEG data, empty on failure
     */
    [[nodiscard]] std::vector<uint8_t> encodeJpeg(
        const uint8_t* rgba, uint32_t width, uint32_t height,
        int quality = 85);

    /**
     * @brief Encode RGBA buffer to PNG format (lossless)
     * @param rgba Raw RGBA pixel data (width * height * 4 bytes)
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @return Compressed PNG data, empty on failure
     */
    [[nodiscard]] std::vector<uint8_t> encodePng(
        const uint8_t* rgba, uint32_t width, uint32_t height);

    /**
     * @brief Convenience method to encode with specified format
     * @param rgba Raw RGBA pixel data (width * height * 4 bytes)
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @param format Target encoding format
     * @param quality Quality parameter (used for JPEG only, 1-100)
     * @return Compressed data, empty on failure or unsupported format
     */
    [[nodiscard]] std::vector<uint8_t> encode(
        const uint8_t* rgba, uint32_t width, uint32_t height,
        EncodeFormat format, int quality = 85);

    /**
     * @brief Check if H.264 encoding is available
     * @return True if ffmpeg/libav is linked and H.264 codec is available
     */
    [[nodiscard]] static bool isH264Available();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

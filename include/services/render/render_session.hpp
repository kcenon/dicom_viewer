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
 * @file render_session.hpp
 * @brief Aggregate headless rendering session for remote rendering
 * @details Bundles VolumeRenderer and MPRRenderer in off-screen mode,
 *          providing thread-safe frame capture for server-side rendering.
 *
 * ## Thread Safety
 * - Frame capture methods are mutex-protected for concurrent access.
 * - Input data and renderer configuration should be set before
 *   capturing frames from multiple threads.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>

namespace dicom_viewer::services {

class VolumeRenderer;
class MPRRenderer;
enum class MPRPlane;

/**
 * @brief Aggregate headless rendering session
 *
 * Owns a VolumeRenderer and MPRRenderer, both initialized in off-screen mode.
 * Provides thread-safe frame capture for remote rendering.
 *
 * @trace SRS-FR-REMOTE-002
 */
class RenderSession {
public:
    /**
     * @brief Create a render session with the specified frame size
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     */
    explicit RenderSession(uint32_t width, uint32_t height);
    ~RenderSession();

    // Non-copyable, movable
    RenderSession(const RenderSession&) = delete;
    RenderSession& operator=(const RenderSession&) = delete;
    RenderSession(RenderSession&&) noexcept;
    RenderSession& operator=(RenderSession&&) noexcept;

    /**
     * @brief Get the volume renderer
     * @return Reference to the volume renderer
     */
    VolumeRenderer& volumeRenderer();

    /**
     * @brief Get the MPR renderer
     * @return Reference to the MPR renderer
     */
    MPRRenderer& mprRenderer();

    /**
     * @brief Set input volume data for all renderers
     * @param imageData VTK image data (3D volume)
     */
    void setInputData(vtkSmartPointer<vtkImageData> imageData);

    /**
     * @brief Capture a volume rendering frame (thread-safe)
     * @return RGBA pixel data (width * height * 4 bytes)
     */
    [[nodiscard]] std::vector<uint8_t> captureVolumeFrame();

    /**
     * @brief Capture an MPR frame for a specific plane (thread-safe)
     * @param plane MPR plane to capture
     * @return RGBA pixel data (width * height * 4 bytes)
     */
    [[nodiscard]] std::vector<uint8_t> captureMPRFrame(MPRPlane plane);

    /**
     * @brief Resize all off-screen render targets
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void resize(uint32_t width, uint32_t height);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

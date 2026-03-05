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
 * @file offscreen_render_context.hpp
 * @brief Headless off-screen VTK rendering context
 * @details Encapsulates a VTK render window configured for off-screen
 *          rendering and RGBA frame capture. Provides the foundation for
 *          server-side rendering without a display.
 *
 * VTK 9.x SetOffScreenRendering(true) auto-selects the best backend:
 * Metal on macOS, EGL/OSMesa on Linux.
 *
 * ## Thread Safety
 * - Not thread-safe. External synchronization required for concurrent access.
 * - Each context should be used from a single thread at a time.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkRenderWindow;

namespace dicom_viewer::services {

/**
 * @brief Off-screen VTK rendering context for headless frame capture
 *
 * Creates a VTK render window with off-screen rendering enabled,
 * paired with a vtkWindowToImageFilter for RGBA pixel extraction.
 *
 * @trace SRS-FR-REMOTE-001
 */
class OffscreenRenderContext {
public:
    OffscreenRenderContext();
    ~OffscreenRenderContext();

    // Non-copyable, movable
    OffscreenRenderContext(const OffscreenRenderContext&) = delete;
    OffscreenRenderContext& operator=(const OffscreenRenderContext&) = delete;
    OffscreenRenderContext(OffscreenRenderContext&&) noexcept;
    OffscreenRenderContext& operator=(OffscreenRenderContext&&) noexcept;

    /**
     * @brief Initialize the off-screen render window
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     */
    void initialize(uint32_t width, uint32_t height);

    /**
     * @brief Check if the context has been initialized
     * @return True if initialized
     */
    [[nodiscard]] bool isInitialized() const;

    /**
     * @brief Get the underlying VTK render window
     * @return Render window pointer, or nullptr if not initialized
     */
    [[nodiscard]] vtkRenderWindow* getRenderWindow() const;

    /**
     * @brief Get the default renderer attached to this context
     * @return Renderer pointer, or nullptr if not initialized
     */
    [[nodiscard]] vtkRenderer* getRenderer() const;

    /**
     * @brief Resize the render window
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void resize(uint32_t width, uint32_t height);

    /**
     * @brief Get the current frame size
     * @return {width, height} pair
     */
    [[nodiscard]] std::pair<uint32_t, uint32_t> getSize() const;

    /**
     * @brief Check if the render window supports OpenGL rendering
     * @return True if OpenGL is available, false on headless systems without GPU
     */
    [[nodiscard]] bool supportsOpenGL() const;

    /**
     * @brief Render the scene and capture the frame as RGBA pixels
     * @return RGBA pixel data (width * height * 4 bytes), empty if OpenGL unavailable
     */
    [[nodiscard]] std::vector<uint8_t> captureFrame();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

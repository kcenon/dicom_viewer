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
 * @file render_session_manager.hpp
 * @brief Manages lifecycle of per-client headless render sessions
 * @details Creates, tracks, and destroys RenderSession objects for remote
 *          rendering clients. Provides idle timeout cleanup, max session
 *          enforcement, and a background render loop that captures frames
 *          at a configurable target FPS.
 *
 * ## Architecture
 * ```
 * RenderSessionManager
 *   +-- session_map: {session_id -> RenderSession + metadata}
 *   +-- render_thread: fires at target FPS, captures frames
 *   +-- idle_timeout: destroys zombie sessions
 *   +-- frame_callback: delivers rendered frames to caller
 * ```
 *
 * ## Thread Safety
 * - All public methods are thread-safe (internal mutex)
 * - Frame callback is invoked from the render loop thread
 * - Idle cleanup can be called from any thread
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dicom_viewer::services {

class AdaptiveQualityController;
class RenderSession;

/**
 * @brief Configuration for the render session manager
 */
struct RenderSessionManagerConfig {
    /// Maximum concurrent render sessions (0 = unlimited)
    uint32_t maxSessions = 8;

    /// Target frames per second for the render loop
    uint32_t targetFps = 30;

    /// Idle timeout in seconds (0 = no timeout)
    uint32_t idleTimeoutSeconds = 300;

    /// Default frame width when not specified per session
    uint32_t defaultWidth = 512;

    /// Default frame height when not specified per session
    uint32_t defaultHeight = 512;
};

/**
 * @brief Callback invoked when a frame is ready for delivery
 * @param sessionId Session that produced the frame
 * @param rgbaFrame RGBA pixel data (width * height * 4 bytes)
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 */
using FrameReadyCallback = std::function<void(
    const std::string& sessionId,
    const std::vector<uint8_t>& rgbaFrame,
    uint32_t width, uint32_t height)>;

/**
 * @brief Manages per-client headless render session lifecycles
 *
 * Handles creation, destruction, idle timeout, and background
 * render loop for remote rendering sessions.
 *
 * @trace SRS-FR-REMOTE-005
 */
class RenderSessionManager {
public:
    explicit RenderSessionManager(
        const RenderSessionManagerConfig& config = {});
    ~RenderSessionManager();

    // Non-copyable, non-movable (owns background thread)
    RenderSessionManager(const RenderSessionManager&) = delete;
    RenderSessionManager& operator=(const RenderSessionManager&) = delete;
    RenderSessionManager(RenderSessionManager&&) = delete;
    RenderSessionManager& operator=(RenderSessionManager&&) = delete;

    /**
     * @brief Create a new render session
     * @param sessionId Unique session identifier
     * @param width Frame width (0 = use default from config)
     * @param height Frame height (0 = use default from config)
     * @return true if created, false if session exists or limit reached
     */
    bool createSession(const std::string& sessionId,
                       uint32_t width = 0, uint32_t height = 0);

    /**
     * @brief Destroy a render session and free its resources
     * @param sessionId Session to destroy
     * @return true if destroyed, false if session not found
     */
    bool destroySession(const std::string& sessionId);

    /**
     * @brief Check if a session exists
     */
    [[nodiscard]] bool hasSession(const std::string& sessionId) const;

    /**
     * @brief Get a session by ID
     * @return Pointer to session, or nullptr if not found
     */
    [[nodiscard]] RenderSession* getSession(const std::string& sessionId);

    /**
     * @brief Reset the idle timer for a session (e.g., on input event)
     */
    void touchSession(const std::string& sessionId);

    /**
     * @brief Notify that user interaction started on a session
     * @details Transitions quality controller to low-quality high-FPS mode
     * @param sessionId Session receiving interaction
     */
    void notifyInteractionStart(const std::string& sessionId);

    /**
     * @brief Notify that user interaction ended on a session
     * @details Starts debounce timer for high-quality frame emission
     * @param sessionId Session where interaction ended
     */
    void notifyInteractionEnd(const std::string& sessionId);

    /**
     * @brief Get the quality controller for a session
     * @return Pointer to controller, or nullptr if session not found
     */
    [[nodiscard]] AdaptiveQualityController* getQualityController(
        const std::string& sessionId);

    /**
     * @brief Set callback for rendered frame delivery
     * @details Called from the render loop thread at target FPS
     */
    void setFrameReadyCallback(FrameReadyCallback callback);

    /**
     * @brief Start the background render loop
     */
    void startRenderLoop();

    /**
     * @brief Stop the background render loop
     */
    void stopRenderLoop();

    /**
     * @brief Check if the render loop is active
     */
    [[nodiscard]] bool isRenderLoopRunning() const;

    /**
     * @brief Destroy sessions that have been idle beyond the timeout
     * @return Number of sessions destroyed
     */
    size_t cleanupIdleSessions();

    /**
     * @brief Get the number of active sessions
     */
    [[nodiscard]] size_t activeSessionCount() const;

    /**
     * @brief Get all active session IDs
     */
    [[nodiscard]] std::vector<std::string> activeSessionIds() const;

    /**
     * @brief Get the manager configuration
     */
    [[nodiscard]] const RenderSessionManagerConfig& config() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

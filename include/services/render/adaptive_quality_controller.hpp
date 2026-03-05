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
 * @file adaptive_quality_controller.hpp
 * @brief Adaptive JPEG quality controller for render streaming
 * @details Manages quality transitions between interaction and idle states
 *          to balance latency during user interaction with image quality at rest.
 *
 * ## State Machine
 * ```
 * Idle ──(onInteractionStart)──> Interacting
 * Interacting ──(onInteractionEnd)──> PostInteraction
 * PostInteraction ──(debounce expires)──> Idle
 * PostInteraction ──(onInteractionStart)──> Interacting
 * ```
 *
 * ## Thread Safety
 * - All methods are thread-safe (internal mutex)
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>

namespace dicom_viewer::services {

/**
 * @brief Quality state for the adaptive controller
 */
enum class QualityState {
    Idle,              ///< No interaction — high quality, no frame emission
    Interacting,       ///< Active interaction — low quality, high FPS
    PostInteraction    ///< Debounce period — emit one high-quality frame
};

/**
 * @brief Configuration for adaptive quality control
 */
struct AdaptiveQualityConfig {
    /// JPEG quality during interaction (1-100)
    int interactionQuality = 40;

    /// JPEG quality at rest / post-interaction (1-100)
    int idleQuality = 85;

    /// Target FPS during interaction
    uint32_t interactionFps = 30;

    /// Target FPS at rest (on-change only)
    uint32_t idleFps = 1;

    /// Debounce timeout in milliseconds after interaction ends
    uint32_t debounceMs = 100;
};

/**
 * @brief Adaptive quality controller for render streaming
 *
 * Tracks interaction state and provides the appropriate JPEG quality
 * and FPS target for the current moment. During interaction, quality
 * drops and FPS rises; after interaction stops, one high-quality
 * frame is emitted before entering idle mode.
 *
 * @trace SRS-FR-REMOTE-007
 */
class AdaptiveQualityController {
public:
    explicit AdaptiveQualityController(
        const AdaptiveQualityConfig& config = {});
    ~AdaptiveQualityController();

    // Non-copyable, movable
    AdaptiveQualityController(const AdaptiveQualityController&) = delete;
    AdaptiveQualityController& operator=(const AdaptiveQualityController&)
        = delete;
    AdaptiveQualityController(AdaptiveQualityController&&) noexcept;
    AdaptiveQualityController& operator=(
        AdaptiveQualityController&&) noexcept;

    /**
     * @brief Signal that user interaction has started (e.g., mouse_down)
     */
    void onInteractionStart();

    /**
     * @brief Signal that user interaction has ended (e.g., mouse_up)
     */
    void onInteractionEnd();

    /**
     * @brief Get current quality state
     */
    [[nodiscard]] QualityState state() const;

    /**
     * @brief Get the JPEG quality to use for the current state
     * @return Quality value 1-100
     */
    [[nodiscard]] int currentQuality() const;

    /**
     * @brief Get the target FPS for the current state
     */
    [[nodiscard]] uint32_t currentTargetFps() const;

    /**
     * @brief Check whether a frame should be emitted in the current state
     * @details In Idle state, returns false (no frames unless data changes).
     *          In Interacting state, always returns true.
     *          In PostInteraction state, returns true once (for the final
     *          high-quality frame), then transitions to Idle.
     */
    [[nodiscard]] bool shouldEmitFrame();

    /**
     * @brief Update the configuration
     */
    void setConfig(const AdaptiveQualityConfig& config);

    /**
     * @brief Get the current configuration
     */
    [[nodiscard]] const AdaptiveQualityConfig& config() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

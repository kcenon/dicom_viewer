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
 * @file input_event_dispatcher.hpp
 * @brief Dispatches serialized input events into VTK render window interactors
 * @details Receives InputEvent structs (from WebSocket JSON) and injects them
 *          as synthetic VTK interaction events into a vtkRenderWindowInteractor.
 *          Handles coordinate remapping between client canvas and server render
 *          window, and provides burst protection via a bounded event queue.
 *
 * ## Coordinate System
 * - Client: origin at top-left, coordinates in client canvas pixels
 * - VTK: origin at bottom-left, coordinates in render window pixels
 * - Remapping: normalize to [0,1] then scale to server size, flip Y
 *
 * ## Event Mapping
 * | InputEvent.type | VTK Event(s)                              |
 * |-----------------|-------------------------------------------|
 * | mouse_move      | MouseMoveEvent                            |
 * | mouse_down      | Left/Right/MiddleButtonPressEvent          |
 * | mouse_up        | Left/Right/MiddleButtonReleaseEvent        |
 * | scroll          | MouseWheelForward/BackwardEvent            |
 * | key_down        | KeyPressEvent + CharEvent                 |
 * | key_up          | KeyReleaseEvent                           |
 *
 * ## Thread Safety
 * - enqueue() is thread-safe (internal mutex)
 * - processAll() and dispatch() must be called from a single thread
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>
#include <utility>

class vtkRenderWindowInteractor;

namespace dicom_viewer::services {

struct InputEvent;

/**
 * @brief Dispatches input events into VTK interactors for remote rendering
 *
 * Translates client input events into VTK interaction events with coordinate
 * remapping and burst protection.
 *
 * @trace SRS-FR-REMOTE-004
 */
class InputEventDispatcher {
public:
    /**
     * @brief Construct a dispatcher with configurable queue depth
     * @param maxQueueDepth Maximum events in the burst queue (0 = unlimited)
     */
    explicit InputEventDispatcher(uint32_t maxQueueDepth = 64);
    ~InputEventDispatcher();

    // Non-copyable, movable
    InputEventDispatcher(const InputEventDispatcher&) = delete;
    InputEventDispatcher& operator=(const InputEventDispatcher&) = delete;
    InputEventDispatcher(InputEventDispatcher&&) noexcept;
    InputEventDispatcher& operator=(InputEventDispatcher&&) noexcept;

    /**
     * @brief Dispatch a single event immediately to the interactor
     * @param interactor Target VTK interactor (must not be null)
     * @param event Input event to dispatch
     * @param clientWidth Client canvas width in pixels
     * @param clientHeight Client canvas height in pixels
     * @return true if dispatched, false if event type is unknown or interactor is null
     */
    bool dispatch(vtkRenderWindowInteractor* interactor,
                  const InputEvent& event,
                  uint32_t clientWidth, uint32_t clientHeight);

    /**
     * @brief Enqueue an event for batch processing (thread-safe)
     *
     * If the queue exceeds maxQueueDepth, the oldest event is dropped.
     *
     * @param event Input event to enqueue
     */
    void enqueue(const InputEvent& event);

    /**
     * @brief Process all queued events on the interactor
     * @param interactor Target VTK interactor
     * @param clientWidth Client canvas width in pixels
     * @param clientHeight Client canvas height in pixels
     * @return Number of events processed
     */
    size_t processAll(vtkRenderWindowInteractor* interactor,
                      uint32_t clientWidth, uint32_t clientHeight);

    /**
     * @brief Remap coordinates from client canvas to server render window
     * @param clientX X coordinate in client canvas
     * @param clientY Y coordinate in client canvas (top-left origin)
     * @param clientWidth Client canvas width
     * @param clientHeight Client canvas height
     * @param serverWidth Server render window width
     * @param serverHeight Server render window height
     * @return {x, y} in server render window coordinates (bottom-left origin)
     */
    [[nodiscard]] static std::pair<int, int> remapCoordinates(
        double clientX, double clientY,
        uint32_t clientWidth, uint32_t clientHeight,
        uint32_t serverWidth, uint32_t serverHeight);

    /**
     * @brief Get the number of events currently in the queue
     */
    [[nodiscard]] size_t queueSize() const;

    /**
     * @brief Get the total number of successfully dispatched events
     */
    [[nodiscard]] size_t dispatchedCount() const;

    /**
     * @brief Get the total number of events dropped due to queue overflow
     */
    [[nodiscard]] size_t droppedCount() const;

    /**
     * @brief Set the maximum queue depth
     * @param depth New maximum depth (0 = unlimited)
     */
    void setMaxQueueDepth(uint32_t depth);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

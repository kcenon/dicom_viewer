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

#include "services/render/input_event_dispatcher.hpp"
#include "services/render/websocket_frame_streamer.hpp"

#include <vtkCommand.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Key symbol mapping: web key names -> VTK key symbols
// ---------------------------------------------------------------------------
static const char* mapKeySym(const std::string& webKey)
{
    static const std::unordered_map<std::string, const char*> keyMap = {
        {"ArrowUp", "Up"},
        {"ArrowDown", "Down"},
        {"ArrowLeft", "Left"},
        {"ArrowRight", "Right"},
        {"Escape", "Escape"},
        {"Enter", "Return"},
        {"Backspace", "BackSpace"},
        {"Tab", "Tab"},
        {"Delete", "Delete"},
        {"Home", "Home"},
        {"End", "End"},
        {"PageUp", "Prior"},
        {"PageDown", "Next"},
        {"Space", "space"},
        {" ", "space"},
        {"Shift", "Shift_L"},
        {"Control", "Control_L"},
        {"Alt", "Alt_L"},
    };
    auto it = keyMap.find(webKey);
    if (it != keyMap.end()) {
        return it->second;
    }
    // Single character keys: return as-is (VTK uses the character)
    return nullptr;
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class InputEventDispatcher::Impl {
public:
    explicit Impl(uint32_t maxDepth) : maxQueueDepth(maxDepth) {}

    bool dispatchSingle(vtkRenderWindowInteractor* interactor,
                        const InputEvent& event,
                        uint32_t clientWidth, uint32_t clientHeight)
    {
        if (!interactor) {
            return false;
        }

        auto* renderWindow = interactor->GetRenderWindow();
        if (!renderWindow) {
            return false;
        }

        const int* windowSize = renderWindow->GetSize();
        auto serverWidth = static_cast<uint32_t>(windowSize[0]);
        auto serverHeight = static_cast<uint32_t>(windowSize[1]);

        auto [sx, sy] = InputEventDispatcher::remapCoordinates(
            event.x, event.y,
            clientWidth, clientHeight,
            serverWidth, serverHeight);

        int ctrl = event.ctrlKey ? 1 : 0;
        int shift = event.shiftKey ? 1 : 0;

        if (event.type == "mouse_move") {
            interactor->SetEventInformation(sx, sy, ctrl, shift);
            interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
        } else if (event.type == "mouse_down") {
            interactor->SetEventInformation(sx, sy, ctrl, shift);
            dispatchButtonPress(interactor, event.buttons);
        } else if (event.type == "mouse_up") {
            interactor->SetEventInformation(sx, sy, ctrl, shift);
            dispatchButtonRelease(interactor, event.buttons);
        } else if (event.type == "scroll") {
            interactor->SetEventInformation(sx, sy, ctrl, shift);
            if (event.delta > 0) {
                interactor->InvokeEvent(vtkCommand::MouseWheelForwardEvent);
            } else if (event.delta < 0) {
                interactor->InvokeEvent(vtkCommand::MouseWheelBackwardEvent);
            }
        } else if (event.type == "key_down") {
            dispatchKeyPress(interactor, event, ctrl, shift);
        } else if (event.type == "key_up") {
            dispatchKeyRelease(interactor, event, ctrl, shift);
        } else {
            return false; // Unknown event type
        }

        ++dispatched;
        return true;
    }

    // Thread-safe queue operations
    void enqueueEvent(const InputEvent& event)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (maxQueueDepth > 0 && queue.size() >= maxQueueDepth) {
            queue.pop_front();
            ++dropped;
        }
        queue.push_back(event);
    }

    std::deque<InputEvent> drainQueue()
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        std::deque<InputEvent> result;
        result.swap(queue);
        return result;
    }

    size_t currentQueueSize() const
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        return queue.size();
    }

    void registerChannel(uint8_t channelId, vtkRenderWindowInteractor* interactor)
    {
        std::lock_guard<std::mutex> lock(channelMutex);
        if (interactor) {
            channelInteractors[channelId] = interactor;
        } else {
            channelInteractors.erase(channelId);
        }
    }

    size_t processAllByChannel(uint32_t clientWidth, uint32_t clientHeight)
    {
        auto events = drainQueue();
        size_t processed = 0;
        for (const auto& event : events) {
            vtkRenderWindowInteractor* interactor = nullptr;
            {
                std::lock_guard<std::mutex> lock(channelMutex);
                auto it = channelInteractors.find(event.channelId);
                if (it != channelInteractors.end()) {
                    interactor = it->second;
                }
            }
            if (interactor
                && dispatchSingle(interactor, event, clientWidth, clientHeight)) {
                ++processed;
            }
        }
        return processed;
    }

    mutable std::mutex channelMutex;
    std::unordered_map<uint8_t, vtkRenderWindowInteractor*> channelInteractors;

    uint32_t maxQueueDepth;
    size_t dispatched = 0;
    size_t dropped = 0;

private:
    static void dispatchButtonPress(vtkRenderWindowInteractor* interactor, int buttons)
    {
        if (buttons & 1) {
            interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
        }
        if (buttons & 2) {
            interactor->InvokeEvent(vtkCommand::RightButtonPressEvent);
        }
        if (buttons & 4) {
            interactor->InvokeEvent(vtkCommand::MiddleButtonPressEvent);
        }
        // Default to left if no button specified
        if (buttons == 0) {
            interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
        }
    }

    static void dispatchButtonRelease(vtkRenderWindowInteractor* interactor, int buttons)
    {
        if (buttons & 1) {
            interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
        }
        if (buttons & 2) {
            interactor->InvokeEvent(vtkCommand::RightButtonReleaseEvent);
        }
        if (buttons & 4) {
            interactor->InvokeEvent(vtkCommand::MiddleButtonReleaseEvent);
        }
        if (buttons == 0) {
            interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
        }
    }

    static void dispatchKeyPress(vtkRenderWindowInteractor* interactor,
                                 const InputEvent& event,
                                 int ctrl, int shift)
    {
        const char* vtkKeySym = mapKeySym(event.keySym);
        char keyChar = 0;
        if (event.keyCode > 0 && event.keyCode < 128) {
            keyChar = static_cast<char>(event.keyCode);
        } else if (event.keySym.size() == 1) {
            keyChar = event.keySym[0];
        }

        if (vtkKeySym) {
            interactor->SetKeyEventInformation(ctrl, shift, keyChar, 0, vtkKeySym);
        } else if (keyChar != 0) {
            // Single printable character
            char keySym[2] = {keyChar, '\0'};
            interactor->SetKeyEventInformation(ctrl, shift, keyChar, 0, keySym);
        } else {
            interactor->SetKeyEventInformation(ctrl, shift, 0, 0, nullptr);
        }
        interactor->InvokeEvent(vtkCommand::KeyPressEvent);
        if (keyChar != 0) {
            interactor->InvokeEvent(vtkCommand::CharEvent);
        }
    }

    static void dispatchKeyRelease(vtkRenderWindowInteractor* interactor,
                                   const InputEvent& event,
                                   int ctrl, int shift)
    {
        const char* vtkKeySym = mapKeySym(event.keySym);
        char keyChar = 0;
        if (event.keyCode > 0 && event.keyCode < 128) {
            keyChar = static_cast<char>(event.keyCode);
        } else if (event.keySym.size() == 1) {
            keyChar = event.keySym[0];
        }

        if (vtkKeySym) {
            interactor->SetKeyEventInformation(ctrl, shift, keyChar, 0, vtkKeySym);
        } else if (keyChar != 0) {
            char keySym[2] = {keyChar, '\0'};
            interactor->SetKeyEventInformation(ctrl, shift, keyChar, 0, keySym);
        } else {
            interactor->SetKeyEventInformation(ctrl, shift, 0, 0, nullptr);
        }
        interactor->InvokeEvent(vtkCommand::KeyReleaseEvent);
    }

    mutable std::mutex queueMutex;
    std::deque<InputEvent> queue;
};

// ---------------------------------------------------------------------------
// Coordinate remapping
// ---------------------------------------------------------------------------
std::pair<int, int> InputEventDispatcher::remapCoordinates(
    double clientX, double clientY,
    uint32_t clientWidth, uint32_t clientHeight,
    uint32_t serverWidth, uint32_t serverHeight)
{
    if (clientWidth == 0 || clientHeight == 0) {
        return {0, 0};
    }

    // Normalize to [0, 1]
    double normX = clientX / static_cast<double>(clientWidth);
    double normY = clientY / static_cast<double>(clientHeight);

    // Clamp to valid range
    normX = std::clamp(normX, 0.0, 1.0);
    normY = std::clamp(normY, 0.0, 1.0);

    // Scale to server coordinates
    int sx = static_cast<int>(std::round(normX * (serverWidth - 1)));

    // Flip Y: client top-left origin -> VTK bottom-left origin
    int sy = static_cast<int>(std::round((1.0 - normY) * (serverHeight - 1)));

    return {sx, sy};
}

// ---------------------------------------------------------------------------
// InputEventDispatcher lifecycle
// ---------------------------------------------------------------------------
InputEventDispatcher::InputEventDispatcher(uint32_t maxQueueDepth)
    : impl_(std::make_unique<Impl>(maxQueueDepth))
{
}

InputEventDispatcher::~InputEventDispatcher() = default;

InputEventDispatcher::InputEventDispatcher(InputEventDispatcher&&) noexcept = default;
InputEventDispatcher& InputEventDispatcher::operator=(InputEventDispatcher&&) noexcept = default;

bool InputEventDispatcher::dispatch(
    vtkRenderWindowInteractor* interactor,
    const InputEvent& event,
    uint32_t clientWidth, uint32_t clientHeight)
{
    if (!impl_) return false;
    return impl_->dispatchSingle(interactor, event, clientWidth, clientHeight);
}

void InputEventDispatcher::enqueue(const InputEvent& event)
{
    if (!impl_) return;
    impl_->enqueueEvent(event);
}

size_t InputEventDispatcher::processAll(
    vtkRenderWindowInteractor* interactor,
    uint32_t clientWidth, uint32_t clientHeight)
{
    if (!impl_) return 0;

    auto events = impl_->drainQueue();
    size_t processed = 0;
    for (const auto& event : events) {
        if (impl_->dispatchSingle(interactor, event, clientWidth, clientHeight)) {
            ++processed;
        }
    }
    return processed;
}

size_t InputEventDispatcher::queueSize() const
{
    if (!impl_) return 0;
    return impl_->currentQueueSize();
}

size_t InputEventDispatcher::dispatchedCount() const
{
    if (!impl_) return 0;
    return impl_->dispatched;
}

size_t InputEventDispatcher::droppedCount() const
{
    if (!impl_) return 0;
    return impl_->dropped;
}

void InputEventDispatcher::setMaxQueueDepth(uint32_t depth)
{
    if (!impl_) return;
    impl_->maxQueueDepth = depth;
}

void InputEventDispatcher::registerInteractor(
    uint8_t channelId, vtkRenderWindowInteractor* interactor)
{
    if (!impl_) return;
    impl_->registerChannel(channelId, interactor);
}

size_t InputEventDispatcher::processAll(
    uint32_t clientWidth, uint32_t clientHeight)
{
    if (!impl_) return 0;
    return impl_->processAllByChannel(clientWidth, clientHeight);
}

} // namespace dicom_viewer::services

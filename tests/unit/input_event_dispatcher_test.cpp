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

#include <gtest/gtest.h>

#include "services/render/input_event_dispatcher.hpp"
#include "services/render/websocket_frame_streamer.hpp"

#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>

#include <thread>
#include <vector>

using namespace dicom_viewer::services;

// =============================================================================
// Test fixture with off-screen VTK interactor
// =============================================================================

class InputEventDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->SetOffScreenRendering(1);
        renderWindow->SetSize(640, 480);

        interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
        interactor->SetRenderWindow(renderWindow);
        // Do not call Initialize() to avoid display requirements
    }

    InputEventDispatcher dispatcher;
    vtkSmartPointer<vtkRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> interactor;

    static InputEvent makeMouseEvent(const std::string& type,
                                     double x, double y,
                                     int buttons = 0)
    {
        InputEvent event;
        event.type = type;
        event.x = x;
        event.y = y;
        event.buttons = buttons;
        return event;
    }

    static InputEvent makeScrollEvent(double x, double y, double delta)
    {
        InputEvent event;
        event.type = "scroll";
        event.x = x;
        event.y = y;
        event.delta = delta;
        return event;
    }

    static InputEvent makeKeyEvent(const std::string& type,
                                   const std::string& keySym,
                                   int keyCode = 0)
    {
        InputEvent event;
        event.type = type;
        event.x = 320;
        event.y = 240;
        event.keySym = keySym;
        event.keyCode = keyCode;
        return event;
    }
};

// =============================================================================
// Construction and lifecycle
// =============================================================================

TEST_F(InputEventDispatcherTest, DefaultConstruction) {
    EXPECT_EQ(dispatcher.dispatchedCount(), 0u);
    EXPECT_EQ(dispatcher.droppedCount(), 0u);
    EXPECT_EQ(dispatcher.queueSize(), 0u);
}

TEST_F(InputEventDispatcherTest, MoveConstructor) {
    InputEventDispatcher moved(std::move(dispatcher));
    EXPECT_EQ(moved.dispatchedCount(), 0u);
}

TEST_F(InputEventDispatcherTest, MoveAssignment) {
    InputEventDispatcher other;
    other = std::move(dispatcher);
    EXPECT_EQ(other.dispatchedCount(), 0u);
}

// =============================================================================
// Coordinate remapping
// =============================================================================

TEST_F(InputEventDispatcherTest, RemapCoordinatesSameSize) {
    // 1:1 mapping, top-left (0,0) -> bottom-left (0, height-1)
    auto [x, y] = InputEventDispatcher::remapCoordinates(
        0, 0, 640, 480, 640, 480);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 479);
}

TEST_F(InputEventDispatcherTest, RemapCoordinatesBottomRight) {
    // Bottom-right in client -> top-right in VTK is (width-1, 0)
    auto [x, y] = InputEventDispatcher::remapCoordinates(
        640, 480, 640, 480, 640, 480);
    EXPECT_EQ(x, 639);
    EXPECT_EQ(y, 0);
}

TEST_F(InputEventDispatcherTest, RemapCoordinatesCenter) {
    auto [x, y] = InputEventDispatcher::remapCoordinates(
        320, 240, 640, 480, 640, 480);
    EXPECT_EQ(x, 320);
    EXPECT_EQ(y, 240);
}

TEST_F(InputEventDispatcherTest, RemapCoordinatesScale2x) {
    // Client 800x600 -> Server 400x300
    auto [x, y] = InputEventDispatcher::remapCoordinates(
        400, 300, 800, 600, 400, 300);
    EXPECT_EQ(x, 200);
    EXPECT_EQ(y, 150);
}

TEST_F(InputEventDispatcherTest, RemapCoordinatesZeroSize) {
    auto [x, y] = InputEventDispatcher::remapCoordinates(
        100, 100, 0, 0, 640, 480);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

TEST_F(InputEventDispatcherTest, RemapCoordinatesClampNegative) {
    auto [x, y] = InputEventDispatcher::remapCoordinates(
        -10, -10, 640, 480, 640, 480);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 479);
}

TEST_F(InputEventDispatcherTest, RemapCoordinatesClampOverflow) {
    auto [x, y] = InputEventDispatcher::remapCoordinates(
        1000, 800, 640, 480, 640, 480);
    EXPECT_EQ(x, 639);
    EXPECT_EQ(y, 0);
}

// =============================================================================
// Mouse event dispatch
// =============================================================================

TEST_F(InputEventDispatcherTest, DispatchMouseMove) {
    auto event = makeMouseEvent("mouse_move", 320, 240);
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchMouseDownLeft) {
    auto event = makeMouseEvent("mouse_down", 100, 200, 1);
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchMouseDownRight) {
    auto event = makeMouseEvent("mouse_down", 100, 200, 2);
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchMouseDownMiddle) {
    auto event = makeMouseEvent("mouse_down", 100, 200, 4);
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchMouseUp) {
    auto event = makeMouseEvent("mouse_up", 100, 200, 1);
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchMouseMoveWithModifiers) {
    InputEvent event;
    event.type = "mouse_move";
    event.x = 320;
    event.y = 240;
    event.shiftKey = true;
    event.ctrlKey = true;
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

// =============================================================================
// Scroll event dispatch
// =============================================================================

TEST_F(InputEventDispatcherTest, DispatchScrollForward) {
    auto event = makeScrollEvent(320, 240, 3.0);
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchScrollBackward) {
    auto event = makeScrollEvent(320, 240, -3.0);
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

// =============================================================================
// Keyboard event dispatch
// =============================================================================

TEST_F(InputEventDispatcherTest, DispatchKeyDown) {
    auto event = makeKeyEvent("key_down", "ArrowUp");
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchKeyUp) {
    auto event = makeKeyEvent("key_up", "Escape");
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, DispatchPrintableKey) {
    auto event = makeKeyEvent("key_down", "a", 'a');
    EXPECT_TRUE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

// =============================================================================
// Invalid event handling
// =============================================================================

TEST_F(InputEventDispatcherTest, DispatchUnknownType) {
    InputEvent event;
    event.type = "unknown_event";
    EXPECT_FALSE(dispatcher.dispatch(interactor, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 0u);
}

TEST_F(InputEventDispatcherTest, DispatchNullInteractor) {
    auto event = makeMouseEvent("mouse_move", 320, 240);
    EXPECT_FALSE(dispatcher.dispatch(nullptr, event, 640, 480));
    EXPECT_EQ(dispatcher.dispatchedCount(), 0u);
}

// =============================================================================
// Event queue and burst protection
// =============================================================================

TEST_F(InputEventDispatcherTest, EnqueueAndProcess) {
    dispatcher.enqueue(makeMouseEvent("mouse_move", 100, 100));
    dispatcher.enqueue(makeMouseEvent("mouse_move", 200, 200));
    dispatcher.enqueue(makeMouseEvent("mouse_move", 300, 300));

    EXPECT_EQ(dispatcher.queueSize(), 3u);

    size_t processed = dispatcher.processAll(interactor, 640, 480);
    EXPECT_EQ(processed, 3u);
    EXPECT_EQ(dispatcher.queueSize(), 0u);
    EXPECT_EQ(dispatcher.dispatchedCount(), 3u);
}

TEST_F(InputEventDispatcherTest, QueueOverflowDropsOldest) {
    InputEventDispatcher smallQueue(4);

    for (int i = 0; i < 10; ++i) {
        smallQueue.enqueue(makeMouseEvent("mouse_move",
                                          static_cast<double>(i * 10),
                                          static_cast<double>(i * 10)));
    }

    EXPECT_EQ(smallQueue.queueSize(), 4u);
    EXPECT_EQ(smallQueue.droppedCount(), 6u);

    size_t processed = smallQueue.processAll(interactor, 640, 480);
    EXPECT_EQ(processed, 4u);
}

TEST_F(InputEventDispatcherTest, ProcessEmptyQueue) {
    size_t processed = dispatcher.processAll(interactor, 640, 480);
    EXPECT_EQ(processed, 0u);
}

TEST_F(InputEventDispatcherTest, SetMaxQueueDepth) {
    dispatcher.setMaxQueueDepth(2);

    dispatcher.enqueue(makeMouseEvent("mouse_move", 10, 10));
    dispatcher.enqueue(makeMouseEvent("mouse_move", 20, 20));
    dispatcher.enqueue(makeMouseEvent("mouse_move", 30, 30));

    EXPECT_EQ(dispatcher.queueSize(), 2u);
    EXPECT_EQ(dispatcher.droppedCount(), 1u);
}

// =============================================================================
// Thread-safe enqueue
// =============================================================================

TEST_F(InputEventDispatcherTest, ConcurrentEnqueue) {
    InputEventDispatcher largeQueue(1000);

    auto enqueueN = [&](int start, int count) {
        for (int i = start; i < start + count; ++i) {
            largeQueue.enqueue(makeMouseEvent("mouse_move",
                                              static_cast<double>(i),
                                              static_cast<double>(i)));
        }
    };

    std::thread t1(enqueueN, 0, 100);
    std::thread t2(enqueueN, 100, 100);
    std::thread t3(enqueueN, 200, 100);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(largeQueue.queueSize(), 300u);
    EXPECT_EQ(largeQueue.droppedCount(), 0u);
}

// =============================================================================
// Event position verification
// =============================================================================

TEST_F(InputEventDispatcherTest, EventPositionSetCorrectly) {
    // Dispatch mouse event at center of client canvas
    auto event = makeMouseEvent("mouse_move", 320, 240);
    dispatcher.dispatch(interactor, event, 640, 480);

    int* pos = interactor->GetEventPosition();
    // Center should map to (320, 240) in VTK coords for same-size windows
    EXPECT_EQ(pos[0], 320);
    EXPECT_EQ(pos[1], 240);
}

TEST_F(InputEventDispatcherTest, EventPositionTopLeftFlipsY) {
    // Client top-left (0,0) -> VTK bottom-left (0, 479)
    auto event = makeMouseEvent("mouse_move", 0, 0);
    dispatcher.dispatch(interactor, event, 640, 480);

    int* pos = interactor->GetEventPosition();
    EXPECT_EQ(pos[0], 0);
    EXPECT_EQ(pos[1], 479);
}

// =============================================================================
// Full interaction sequence
// =============================================================================

TEST_F(InputEventDispatcherTest, FullDragSequence) {
    // Simulate left-button drag: press -> move -> move -> release
    EXPECT_TRUE(dispatcher.dispatch(
        interactor, makeMouseEvent("mouse_down", 100, 100, 1), 640, 480));
    EXPECT_TRUE(dispatcher.dispatch(
        interactor, makeMouseEvent("mouse_move", 200, 150), 640, 480));
    EXPECT_TRUE(dispatcher.dispatch(
        interactor, makeMouseEvent("mouse_move", 300, 200), 640, 480));
    EXPECT_TRUE(dispatcher.dispatch(
        interactor, makeMouseEvent("mouse_up", 300, 200, 1), 640, 480));

    EXPECT_EQ(dispatcher.dispatchedCount(), 4u);
}

TEST_F(InputEventDispatcherTest, ScrollSequence) {
    // Simulate multiple scroll events (e.g., MPR slice navigation)
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(dispatcher.dispatch(
            interactor, makeScrollEvent(320, 240, 1.0), 640, 480));
    }
    EXPECT_EQ(dispatcher.dispatchedCount(), 5u);
}

// =============================================================================
// InputEvent extended fields
// =============================================================================

TEST_F(InputEventDispatcherTest, InputEventExtendedDefaults) {
    InputEvent event;
    EXPECT_DOUBLE_EQ(event.delta, 0.0);
    EXPECT_FALSE(event.shiftKey);
    EXPECT_FALSE(event.ctrlKey);
    EXPECT_FALSE(event.altKey);
    EXPECT_TRUE(event.keySym.empty());
    EXPECT_EQ(event.channelId, 0u);
}

// =============================================================================
// Channel-based interactor routing
// =============================================================================

TEST_F(InputEventDispatcherTest, RegisterInteractorAndProcessByChannel) {
    dispatcher.registerInteractor(0, interactor);

    InputEvent event = makeMouseEvent("mouse_move", 320, 240);
    event.channelId = 0;
    dispatcher.enqueue(event);

    size_t processed = dispatcher.processAll(640, 480);
    EXPECT_EQ(processed, 1u);
    EXPECT_EQ(dispatcher.dispatchedCount(), 1u);
}

TEST_F(InputEventDispatcherTest, UnregisteredChannelDropsEvent) {
    // Channel 1 has no interactor — event should be skipped
    InputEvent event = makeMouseEvent("mouse_move", 320, 240);
    event.channelId = 1;
    dispatcher.enqueue(event);

    size_t processed = dispatcher.processAll(640, 480);
    EXPECT_EQ(processed, 0u);
}

TEST_F(InputEventDispatcherTest, MultiChannelRoutesCorrectly) {
    // Create a second VTK render window/interactor for channel 1
    auto renderWindow2 = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow2->SetOffScreenRendering(1);
    renderWindow2->SetSize(512, 512);
    auto interactor2 = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor2->SetRenderWindow(renderWindow2);

    dispatcher.registerInteractor(0, interactor);
    dispatcher.registerInteractor(1, interactor2);

    InputEvent e0 = makeMouseEvent("mouse_move", 100, 100);
    e0.channelId = 0;
    InputEvent e1 = makeMouseEvent("mouse_move", 200, 200);
    e1.channelId = 1;
    InputEvent e2 = makeMouseEvent("mouse_move", 300, 300);
    e2.channelId = 0;

    dispatcher.enqueue(e0);
    dispatcher.enqueue(e1);
    dispatcher.enqueue(e2);

    size_t processed = dispatcher.processAll(640, 480);
    EXPECT_EQ(processed, 3u);
    EXPECT_EQ(dispatcher.dispatchedCount(), 3u);
}

TEST_F(InputEventDispatcherTest, RemoveInteractorByRegisteringNull) {
    dispatcher.registerInteractor(0, interactor);

    // Remove channel 0
    dispatcher.registerInteractor(0, nullptr);

    InputEvent event = makeMouseEvent("mouse_move", 320, 240);
    event.channelId = 0;
    dispatcher.enqueue(event);

    size_t processed = dispatcher.processAll(640, 480);
    EXPECT_EQ(processed, 0u);
}

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
 * @file websocket_multiplex_test.cpp
 * @brief Integration tests for WebSocket v2 multiplex frame streaming
 *
 * Tests v2 protocol correctness, channel demultiplexing, and Origin validation
 * without requiring actual WebSocket client connections.
 */

#include <gtest/gtest.h>

#include "services/render/input_event_dispatcher.hpp"
#include "services/render/websocket_frame_streamer.hpp"

#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace dicom_viewer::services;

// =============================================================================
// Helper: build a v2 binary frame manually for parsing verification
// =============================================================================
static std::vector<uint8_t> buildV2FrameBytes(
    const std::string& sessionId,
    uint8_t channelId,
    uint32_t frameSeq,
    uint8_t frameType,
    uint32_t width,
    uint32_t height,
    const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> frame;
    auto appendU8 = [&](uint8_t v) { frame.push_back(v); };
    auto appendU32LE = [&](uint32_t v) {
        frame.push_back(v & 0xFF);
        frame.push_back((v >> 8) & 0xFF);
        frame.push_back((v >> 16) & 0xFF);
        frame.push_back((v >> 24) & 0xFF);
    };

    appendU8(0x02); // version
    uint32_t sidLen = static_cast<uint32_t>(sessionId.size());
    appendU32LE(sidLen);
    for (char c : sessionId) { appendU8(static_cast<uint8_t>(c)); }
    appendU8(channelId);
    appendU32LE(frameSeq);
    appendU8(frameType);
    appendU32LE(width);
    appendU32LE(height);
    for (uint8_t b : payload) { appendU8(b); }

    return frame;
}

// =============================================================================
// v2 protocol frame structure tests
// =============================================================================

class WebSocketV2ProtocolTest : public ::testing::Test {};

TEST_F(WebSocketV2ProtocolTest, V2FrameStartsWithVersionByte) {
    auto frame = buildV2FrameBytes("sess-1", 0, 1, 0x00, 64, 48, {0xAB, 0xCD});
    ASSERT_FALSE(frame.empty());
    EXPECT_EQ(frame[0], 0x02u); // version byte
}

TEST_F(WebSocketV2ProtocolTest, V2FrameChannelIdPosition) {
    const std::string sessionId = "abc";
    uint8_t channelId = 2; // SagittalMPR
    auto frame = buildV2FrameBytes(sessionId, channelId, 1, 0x00, 64, 48, {});

    // Layout: [1B version][4B sidLen][3B "abc"][1B channel_id]...
    // channel_id offset = 1 + 4 + 3 = 8
    ASSERT_GE(frame.size(), 9u);
    EXPECT_EQ(frame[8], channelId);
}

TEST_F(WebSocketV2ProtocolTest, V2FrameFrameTypePosition) {
    const std::string sessionId = "abc"; // 3 bytes
    uint8_t frameType = 0x01; // DeltaFrame
    auto frame = buildV2FrameBytes(sessionId, 0, 42, frameType, 64, 48, {});

    // Layout: [1B version][4B sidLen][3B "abc"][1B channel_id][4B frame_seq][1B frame_type]
    // frame_type offset = 1 + 4 + 3 + 1 + 4 = 13
    ASSERT_GE(frame.size(), 14u);
    EXPECT_EQ(frame[13], frameType);
}

TEST_F(WebSocketV2ProtocolTest, V2FrameTotalSizeCalculation) {
    const std::string sessionId = "session-xyz"; // 11 bytes
    std::vector<uint8_t> payload(1024, 0xAA);
    auto frame = buildV2FrameBytes(sessionId, 1, 100, 0x00, 1920, 1080, payload);

    // Expected: 1 + 4 + 11 + 1 + 4 + 1 + 4 + 4 + 1024 = 1054
    size_t expected = 1 + 4 + sessionId.size() + 1 + 4 + 1 + 4 + 4 + payload.size();
    EXPECT_EQ(frame.size(), expected);
}

TEST_F(WebSocketV2ProtocolTest, AllFourChannelsDistinct) {
    // Each channel produces a uniquely identifiable frame
    for (uint8_t ch = 0; ch < 4; ++ch) {
        auto frame = buildV2FrameBytes("s", ch, 1, 0x00, 64, 48, {0xFF});
        // channel_id at offset: 1 + 4 + 1("s") = 6
        ASSERT_GE(frame.size(), 7u);
        EXPECT_EQ(frame[6], ch) << "Channel " << static_cast<int>(ch);
    }
}

// =============================================================================
// WebSocketFrameStreamer v2 API tests
// =============================================================================

class WebSocketStreamerV2Test : public ::testing::Test {
protected:
    void TearDown() override { streamer.stop(); }
    WebSocketFrameStreamer streamer;
};

TEST_F(WebSocketStreamerV2Test, PushFrameDefaultChannelBackwardCompat) {
    WebSocketStreamConfig cfg;
    cfg.port = 20001;
    cfg.concurrency = 1;
    ASSERT_TRUE(streamer.start(cfg));

    std::vector<uint8_t> data = {0xFF, 0xD8, 0xFF, 0xD9};
    // Default args: channelId=0, frameType=0x00 (Full)
    EXPECT_EQ(streamer.pushFrame("sess", data, 64, 48, 1), 0u);
}

TEST_F(WebSocketStreamerV2Test, PushFrameAllChannels) {
    WebSocketStreamConfig cfg;
    cfg.port = 20002;
    cfg.concurrency = 1;
    ASSERT_TRUE(streamer.start(cfg));

    std::vector<uint8_t> data = {0xAB, 0xCD};
    for (uint8_t ch = 0; ch < 4; ++ch) {
        EXPECT_EQ(
            streamer.pushFrame("sess", data, 64, 48, ch, ch, 0x00), 0u)
            << "Channel " << static_cast<int>(ch);
    }
}

TEST_F(WebSocketStreamerV2Test, PushDeltaFrameType) {
    WebSocketStreamConfig cfg;
    cfg.port = 20003;
    cfg.concurrency = 1;
    ASSERT_TRUE(streamer.start(cfg));

    std::vector<uint8_t> data = {0x01, 0x02};
    EXPECT_EQ(
        streamer.pushFrame("sess", data, 32, 32, 10,
                           static_cast<uint8_t>(ViewportChannel::Volume3D),
                           static_cast<uint8_t>(FrameType::Delta)),
        0u);
}

// =============================================================================
// Origin header validation tests (config-level)
// =============================================================================

TEST_F(WebSocketStreamerV2Test, AllowedOriginsDefaultEmpty) {
    WebSocketStreamConfig cfg;
    EXPECT_TRUE(cfg.allowedOrigins.empty());
}

TEST_F(WebSocketStreamerV2Test, AllowedOriginsConfigured) {
    WebSocketStreamConfig cfg;
    cfg.allowedOrigins = {
        "https://dicom-viewer.hospital.org",
        "https://localhost:3000",
    };
    cfg.port = 20004;
    cfg.concurrency = 1;
    EXPECT_TRUE(streamer.start(cfg));
    EXPECT_TRUE(streamer.isRunning());
}

// =============================================================================
// InputEvent channel routing integration
// =============================================================================

class MultiplexChannelRoutingTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        for (int i = 0; i < 4; ++i) {
            auto rw = vtkSmartPointer<vtkRenderWindow>::New();
            rw->SetOffScreenRendering(1);
            rw->SetSize(512, 512);
            auto ia = vtkSmartPointer<vtkRenderWindowInteractor>::New();
            ia->SetRenderWindow(rw);
            renderWindows[i] = rw;
            interactors[i] = ia;
        }
    }

    InputEventDispatcher dispatcher;
    vtkSmartPointer<vtkRenderWindow> renderWindows[4];
    vtkSmartPointer<vtkRenderWindowInteractor> interactors[4];
};

TEST_F(MultiplexChannelRoutingTest, FourChannelDemux) {
    // Register all 4 viewport interactors
    for (uint8_t ch = 0; ch < 4; ++ch) {
        dispatcher.registerInteractor(ch, interactors[ch]);
    }

    // Enqueue one event per channel
    for (uint8_t ch = 0; ch < 4; ++ch) {
        InputEvent event;
        event.type = "mouse_move";
        event.x = 100;
        event.y = 100;
        event.channelId = ch;
        dispatcher.enqueue(event);
    }

    EXPECT_EQ(dispatcher.queueSize(), 4u);
    size_t processed = dispatcher.processAll(512, 512);
    EXPECT_EQ(processed, 4u);
    EXPECT_EQ(dispatcher.dispatchedCount(), 4u);
}

TEST_F(MultiplexChannelRoutingTest, MixedChannelsOnlyRegisteredProcessed) {
    // Register only channels 0 and 2
    dispatcher.registerInteractor(0, interactors[0]);
    dispatcher.registerInteractor(2, interactors[2]);

    for (uint8_t ch = 0; ch < 4; ++ch) {
        InputEvent event;
        event.type = "scroll";
        event.delta = 1.0;
        event.channelId = ch;
        dispatcher.enqueue(event);
    }

    size_t processed = dispatcher.processAll(512, 512);
    // Only channels 0 and 2 have interactors
    EXPECT_EQ(processed, 2u);
}

TEST_F(MultiplexChannelRoutingTest, BurstEventsThroughMultipleChannels) {
    dispatcher.registerInteractor(0, interactors[0]);
    dispatcher.registerInteractor(1, interactors[1]);

    // Simulate burst of 50 events alternating between channels 0 and 1
    for (int i = 0; i < 50; ++i) {
        InputEvent event;
        event.type = "mouse_move";
        event.x = static_cast<double>(i * 10);
        event.y = static_cast<double>(i * 5);
        event.channelId = static_cast<uint8_t>(i % 2);
        dispatcher.enqueue(event);
    }

    EXPECT_EQ(dispatcher.queueSize(), 50u);
    size_t processed = dispatcher.processAll(512, 512);
    EXPECT_EQ(processed, 50u);
    EXPECT_EQ(dispatcher.queueSize(), 0u);
}

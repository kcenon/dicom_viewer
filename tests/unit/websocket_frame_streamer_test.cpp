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

#include "services/render/websocket_frame_streamer.hpp"

#include <chrono>
#include <thread>
#include <vector>

using namespace dicom_viewer::services;

class WebSocketFrameStreamerTest : public ::testing::Test {
protected:
    void TearDown() override {
        streamer.stop();
    }

    WebSocketFrameStreamer streamer;
};

// =============================================================================
// Construction and lifecycle
// =============================================================================

TEST_F(WebSocketFrameStreamerTest, DefaultConstruction) {
    EXPECT_FALSE(streamer.isRunning());
    EXPECT_EQ(streamer.port(), 0);
    EXPECT_EQ(streamer.connectionCount(), 0);
}

TEST_F(WebSocketFrameStreamerTest, MoveConstructor) {
    WebSocketFrameStreamer moved(std::move(streamer));
    EXPECT_FALSE(moved.isRunning());
}

TEST_F(WebSocketFrameStreamerTest, MoveAssignment) {
    WebSocketFrameStreamer other;
    other = std::move(streamer);
    EXPECT_FALSE(other.isRunning());
}

// =============================================================================
// Server lifecycle
// =============================================================================

TEST_F(WebSocketFrameStreamerTest, StartAndStop) {
    WebSocketStreamConfig config;
    config.port = 19876;  // Use uncommon port for testing
    config.concurrency = 1;

    EXPECT_TRUE(streamer.start(config));
    EXPECT_TRUE(streamer.isRunning());
    EXPECT_EQ(streamer.port(), 19876);

    streamer.stop();
    EXPECT_FALSE(streamer.isRunning());
    EXPECT_EQ(streamer.port(), 0);
}

TEST_F(WebSocketFrameStreamerTest, DoubleStartReturnsFalse) {
    WebSocketStreamConfig config;
    config.port = 19877;
    config.concurrency = 1;

    EXPECT_TRUE(streamer.start(config));
    EXPECT_FALSE(streamer.start(config)); // Already running
}

TEST_F(WebSocketFrameStreamerTest, StopWhenNotRunning) {
    EXPECT_NO_THROW(streamer.stop());
}

TEST_F(WebSocketFrameStreamerTest, DoubleStop) {
    WebSocketStreamConfig config;
    config.port = 19878;
    config.concurrency = 1;

    streamer.start(config);
    streamer.stop();
    EXPECT_NO_THROW(streamer.stop()); // Safe to call twice
}

// =============================================================================
// Connection state (without actual WebSocket clients)
// =============================================================================

TEST_F(WebSocketFrameStreamerTest, NoClientsInitially) {
    EXPECT_FALSE(streamer.hasClients("session-1"));
    EXPECT_EQ(streamer.connectionCount(), 0);
}

TEST_F(WebSocketFrameStreamerTest, PushFrameWithNoClients) {
    WebSocketStreamConfig config;
    config.port = 19879;
    config.concurrency = 1;
    streamer.start(config);

    std::vector<uint8_t> frameData = {0xFF, 0xD8, 0xFF, 0xD9}; // Fake JPEG
    size_t sent = streamer.pushFrame("session-1", frameData, 64, 48, 1);
    EXPECT_EQ(sent, 0u); // No clients connected
}

TEST_F(WebSocketFrameStreamerTest, PushFrameV2WithChannelAndType) {
    WebSocketStreamConfig config;
    config.port = 19880;
    config.concurrency = 1;
    streamer.start(config);

    std::vector<uint8_t> frameData = {0xFF, 0xD8, 0xFF, 0xD9};
    // v2: push with explicit channelId and frameType
    size_t sent = streamer.pushFrame("session-1", frameData, 64, 48, 1, 1, 0x00);
    EXPECT_EQ(sent, 0u); // No clients, but call should not crash
}

// =============================================================================
// Input event callback
// =============================================================================

TEST_F(WebSocketFrameStreamerTest, SetInputEventCallback) {
    bool called = false;
    streamer.setInputEventCallback([&](const InputEvent& event) {
        called = true;
        (void)event;
    });
    // Callback is set but won't be invoked without actual WebSocket connections
    EXPECT_FALSE(called);
}

// =============================================================================
// Configuration
// =============================================================================

TEST_F(WebSocketFrameStreamerTest, DefaultConfig) {
    WebSocketStreamConfig config;
    EXPECT_EQ(config.port, 8081);
    EXPECT_EQ(config.concurrency, 2);
    EXPECT_EQ(config.maxConnections, 16u);
    EXPECT_EQ(config.pingIntervalSeconds, 30u);
    EXPECT_EQ(config.connectionTimeoutSeconds, 90u);
    EXPECT_TRUE(config.allowedOrigins.empty());
    EXPECT_EQ(config.maxMessageSizeBytes, 65536u);
}

TEST_F(WebSocketFrameStreamerTest, AllowedOriginsConfigured) {
    WebSocketStreamConfig config;
    config.allowedOrigins = {"https://example.com", "https://viewer.hospital.org"};
    EXPECT_EQ(config.allowedOrigins.size(), 2u);
    EXPECT_EQ(config.allowedOrigins[0], "https://example.com");
}

TEST_F(WebSocketFrameStreamerTest, MessageSizeLimitConfigured) {
    WebSocketStreamConfig config;
    config.maxMessageSizeBytes = 1024;
    EXPECT_EQ(config.maxMessageSizeBytes, 1024u);
}

// =============================================================================
// Enums
// =============================================================================

TEST_F(WebSocketFrameStreamerTest, ViewportChannelValues) {
    EXPECT_EQ(static_cast<uint8_t>(ViewportChannel::Volume3D), 0u);
    EXPECT_EQ(static_cast<uint8_t>(ViewportChannel::AxialMPR), 1u);
    EXPECT_EQ(static_cast<uint8_t>(ViewportChannel::SagittalMPR), 2u);
    EXPECT_EQ(static_cast<uint8_t>(ViewportChannel::CoronalMPR), 3u);
}

TEST_F(WebSocketFrameStreamerTest, FrameTypeValues) {
    EXPECT_EQ(static_cast<uint8_t>(FrameType::Full), 0x00u);
    EXPECT_EQ(static_cast<uint8_t>(FrameType::Delta), 0x01u);
}

// =============================================================================
// InputEvent struct
// =============================================================================

TEST_F(WebSocketFrameStreamerTest, InputEventDefaults) {
    InputEvent event;
    EXPECT_TRUE(event.sessionId.empty());
    EXPECT_TRUE(event.type.empty());
    EXPECT_DOUBLE_EQ(event.x, 0.0);
    EXPECT_DOUBLE_EQ(event.y, 0.0);
    EXPECT_EQ(event.buttons, 0);
    EXPECT_EQ(event.keyCode, 0);
    EXPECT_EQ(event.timestamp, 0u);
    EXPECT_EQ(event.channelId, 0u);
}

TEST_F(WebSocketFrameStreamerTest, InputEventChannelIdField) {
    InputEvent event;
    event.channelId = static_cast<uint8_t>(ViewportChannel::AxialMPR);
    EXPECT_EQ(event.channelId, 1u);

    event.channelId = static_cast<uint8_t>(ViewportChannel::CoronalMPR);
    EXPECT_EQ(event.channelId, 3u);
}

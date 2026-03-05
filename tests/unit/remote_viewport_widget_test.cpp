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

#include "ui/widgets/remote_viewport_widget.hpp"

#include <QApplication>

#include <cstring>
#include <string>
#include <vector>

using namespace dicom_viewer::ui;

// =============================================================================
// QApplication fixture (required for QWidget tests)
// =============================================================================

class RemoteViewportWidgetTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test";
            static char* argv[] = {arg0, nullptr};
            app_ = std::make_unique<QApplication>(argc, argv);
        }
    }

    static std::unique_ptr<QApplication> app_;

    // Helper: build a binary frame matching the server protocol
    static std::vector<char> buildFrame(
        const std::string& sessionId,
        uint32_t frameSeq, uint32_t width, uint32_t height,
        const std::vector<uint8_t>& imageData)
    {
        uint32_t sidLen = static_cast<uint32_t>(sessionId.size());
        size_t totalSize = 4 + sidLen + 4 + 4 + 4 + imageData.size();

        std::vector<char> frame(totalSize, '\0');
        char* ptr = frame.data();

        std::memcpy(ptr, &sidLen, 4);         ptr += 4;
        std::memcpy(ptr, sessionId.data(), sidLen); ptr += sidLen;
        std::memcpy(ptr, &frameSeq, 4);       ptr += 4;
        std::memcpy(ptr, &width, 4);          ptr += 4;
        std::memcpy(ptr, &height, 4);         ptr += 4;
        std::memcpy(ptr, imageData.data(), imageData.size());

        return frame;
    }
};

std::unique_ptr<QApplication> RemoteViewportWidgetTest::app_;

// =============================================================================
// Frame header parsing
// =============================================================================

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderValid) {
    std::string sessionId = "test-session-123";
    uint32_t frameSeq = 42;
    uint32_t width = 640;
    uint32_t height = 480;
    std::vector<uint8_t> imageData = {0xFF, 0xD8, 0xFF, 0xE0}; // JPEG magic

    auto frame = buildFrame(sessionId, frameSeq, width, height, imageData);

    FrameHeader header;
    EXPECT_TRUE(RemoteViewportWidget::parseFrameHeader(
        frame.data(), frame.size(), header));

    EXPECT_EQ(header.sessionId, "test-session-123");
    EXPECT_EQ(header.frameSeq, 42u);
    EXPECT_EQ(header.width, 640u);
    EXPECT_EQ(header.height, 480u);
    EXPECT_EQ(header.imageDataSize, 4u);
    EXPECT_NE(header.imageData, nullptr);
    EXPECT_EQ(header.imageData[0], 0xFF);
    EXPECT_EQ(header.imageData[1], 0xD8);
}

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderEmptySessionId) {
    std::string sessionId;
    auto frame = buildFrame(sessionId, 1, 512, 512, {0xAB, 0xCD});

    FrameHeader header;
    EXPECT_TRUE(RemoteViewportWidget::parseFrameHeader(
        frame.data(), frame.size(), header));

    EXPECT_TRUE(header.sessionId.empty());
    EXPECT_EQ(header.frameSeq, 1u);
    EXPECT_EQ(header.width, 512u);
    EXPECT_EQ(header.height, 512u);
    EXPECT_EQ(header.imageDataSize, 2u);
}

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderLargeSessionId) {
    std::string sessionId(255, 'x');
    auto frame = buildFrame(sessionId, 99, 1920, 1080, {0x01});

    FrameHeader header;
    EXPECT_TRUE(RemoteViewportWidget::parseFrameHeader(
        frame.data(), frame.size(), header));

    EXPECT_EQ(header.sessionId, sessionId);
    EXPECT_EQ(header.frameSeq, 99u);
    EXPECT_EQ(header.width, 1920u);
    EXPECT_EQ(header.height, 1080u);
}

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderNoImageData) {
    std::string sessionId = "s1";
    auto frame = buildFrame(sessionId, 0, 64, 64, {});

    FrameHeader header;
    EXPECT_TRUE(RemoteViewportWidget::parseFrameHeader(
        frame.data(), frame.size(), header));

    EXPECT_EQ(header.sessionId, "s1");
    EXPECT_EQ(header.imageDataSize, 0u);
}

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderNullData) {
    FrameHeader header;
    EXPECT_FALSE(RemoteViewportWidget::parseFrameHeader(nullptr, 0, header));
}

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderTooSmall) {
    char data[10] = {};
    FrameHeader header;
    EXPECT_FALSE(RemoteViewportWidget::parseFrameHeader(data, 10, header));
}

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderSessionIdOverflow) {
    // session_id_len says 1000 but total data is only 20 bytes
    char data[20] = {};
    uint32_t sidLen = 1000;
    std::memcpy(data, &sidLen, 4);

    FrameHeader header;
    EXPECT_FALSE(RemoteViewportWidget::parseFrameHeader(data, 20, header));
}

TEST_F(RemoteViewportWidgetTest, ParseFrameHeaderExactMinimumSize) {
    // Minimum valid: sidLen=0, seq, width, height, no image data
    std::vector<char> frame(16, '\0');
    uint32_t sidLen = 0;
    uint32_t seq = 1;
    uint32_t w = 10;
    uint32_t h = 10;
    char* ptr = frame.data();
    std::memcpy(ptr, &sidLen, 4); ptr += 4;
    std::memcpy(ptr, &seq, 4);   ptr += 4;
    std::memcpy(ptr, &w, 4);     ptr += 4;
    std::memcpy(ptr, &h, 4);

    FrameHeader header;
    EXPECT_TRUE(RemoteViewportWidget::parseFrameHeader(
        frame.data(), frame.size(), header));
    EXPECT_EQ(header.frameSeq, 1u);
    EXPECT_EQ(header.width, 10u);
    EXPECT_EQ(header.height, 10u);
    EXPECT_EQ(header.imageDataSize, 0u);
}

// =============================================================================
// Widget construction and initial state
// =============================================================================

TEST_F(RemoteViewportWidgetTest, DefaultConstruction) {
    RemoteViewportWidget widget;

    EXPECT_EQ(widget.connectionState(), RemoteConnectionState::Disconnected);
    EXPECT_TRUE(widget.sessionId().isEmpty());
    EXPECT_EQ(widget.framesReceived(), 0u);
    EXPECT_EQ(widget.lastFrameSeq(), 0u);
}

TEST_F(RemoteViewportWidgetTest, MinimumSize) {
    RemoteViewportWidget widget;

    EXPECT_GE(widget.minimumWidth(), 320);
    EXPECT_GE(widget.minimumHeight(), 240);
}

TEST_F(RemoteViewportWidgetTest, FocusPolicy) {
    RemoteViewportWidget widget;

    EXPECT_EQ(widget.focusPolicy(), Qt::StrongFocus);
}

TEST_F(RemoteViewportWidgetTest, MouseTracking) {
    RemoteViewportWidget widget;

    EXPECT_TRUE(widget.hasMouseTracking());
}

// =============================================================================
// Connection state transitions
// =============================================================================

TEST_F(RemoteViewportWidgetTest, ConnectSetsConnectingState) {
    RemoteViewportWidget widget;

    // After calling connectToServer, state should transition to Connecting
    widget.connectToServer("localhost", 8081, "session-1");
    EXPECT_EQ(widget.connectionState(), RemoteConnectionState::Connecting);
    EXPECT_EQ(widget.sessionId(), "session-1");

    // Clean up
    widget.disconnectFromServer();
}

TEST_F(RemoteViewportWidgetTest, DisconnectSetsDisconnectedState) {
    RemoteViewportWidget widget;

    widget.connectToServer("localhost", 8081, "session-1");
    widget.disconnectFromServer();

    EXPECT_EQ(widget.connectionState(), RemoteConnectionState::Disconnected);
}

TEST_F(RemoteViewportWidgetTest, DisconnectWithoutConnectIsNoop) {
    RemoteViewportWidget widget;

    widget.disconnectFromServer();
    EXPECT_EQ(widget.connectionState(), RemoteConnectionState::Disconnected);
}

// =============================================================================
// Connection state enum values
// =============================================================================

TEST_F(RemoteViewportWidgetTest, ConnectionStateEnumValues) {
    // Verify distinct enum values
    EXPECT_NE(static_cast<int>(RemoteConnectionState::Disconnected),
              static_cast<int>(RemoteConnectionState::Connecting));
    EXPECT_NE(static_cast<int>(RemoteConnectionState::Connected),
              static_cast<int>(RemoteConnectionState::Reconnecting));
}

// =============================================================================
// FrameHeader struct defaults
// =============================================================================

TEST_F(RemoteViewportWidgetTest, FrameHeaderDefaults) {
    FrameHeader header;

    EXPECT_TRUE(header.sessionId.empty());
    EXPECT_EQ(header.frameSeq, 0u);
    EXPECT_EQ(header.width, 0u);
    EXPECT_EQ(header.height, 0u);
    EXPECT_EQ(header.imageData, nullptr);
    EXPECT_EQ(header.imageDataSize, 0u);
}

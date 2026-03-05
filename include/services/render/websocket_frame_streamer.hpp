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
 * @file websocket_frame_streamer.hpp
 * @brief WebSocket server for bidirectional render frame streaming
 * @details Runs a Crow-based WebSocket server alongside the DICOMweb REST API
 *          server. Pushes encoded render frames (JPEG/PNG) to connected clients
 *          and receives input events (mouse, keyboard) as JSON text frames.
 *
 * ## Architecture
 * - Standalone Crow WebSocket server on a configurable port
 * - Endpoint: ws://host:port/render/{session_id}
 * - Binary frames: server → client (encoded image data with header)
 * - Text frames: client → server (JSON input events)
 *
 * ## Wire Protocol
 *
 * Server → Client (binary frame):
 * ```
 * [4 bytes: session_id_len][N bytes: session_id][4 bytes: frame_seq]
 * [4 bytes: width][4 bytes: height][M bytes: encoded image data]
 * ```
 *
 * Client → Server (text frame, JSON):
 * ```json
 * {"session_id":"abc","type":"mouse_move","x":512,"y":384,
 *  "buttons":1,"modifiers":[],"ts":1709600000123}
 * ```
 *
 * ## Thread Safety
 * - start()/stop() must be called from one thread
 * - pushFrame() is thread-safe (uses internal locking)
 * - Input event callback is invoked from Crow's IO thread
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

/**
 * @brief Configuration for the WebSocket frame streaming server
 */
struct WebSocketStreamConfig {
    /// Port to listen on (default: 8081, separate from REST API port)
    uint16_t port = 8081;

    /// Number of Crow worker threads
    uint16_t concurrency = 2;

    /// Maximum concurrent WebSocket connections
    uint32_t maxConnections = 16;

    /// Ping interval in seconds (0 = disabled)
    uint32_t pingIntervalSeconds = 30;

    /// Connection timeout in seconds (no pong received)
    uint32_t connectionTimeoutSeconds = 90;
};

/**
 * @brief Parsed input event received from a WebSocket client
 */
struct InputEvent {
    std::string sessionId;
    std::string type;       ///< "mouse_move", "mouse_down", "mouse_up", "key_down", "key_up", "scroll"
    double x = 0;
    double y = 0;
    int buttons = 0;        ///< Mouse button bitmask (1=left, 2=right, 4=middle)
    int keyCode = 0;
    uint64_t timestamp = 0;
    double delta = 0;       ///< Scroll wheel delta (positive=forward, negative=backward)
    bool shiftKey = false;
    bool ctrlKey = false;
    bool altKey = false;
    std::string keySym;     ///< Key symbol string (e.g., "ArrowUp", "Escape")
};

/**
 * @brief Callback type for received input events
 */
using InputEventCallback = std::function<void(const InputEvent& event)>;

/**
 * @brief WebSocket server for streaming render frames to clients
 *
 * Manages a standalone Crow-based WebSocket server that streams
 * encoded render frames and receives client input events.
 *
 * @trace SRS-FR-REMOTE-003
 */
class WebSocketFrameStreamer {
public:
    WebSocketFrameStreamer();
    ~WebSocketFrameStreamer();

    // Non-copyable, movable
    WebSocketFrameStreamer(const WebSocketFrameStreamer&) = delete;
    WebSocketFrameStreamer& operator=(const WebSocketFrameStreamer&) = delete;
    WebSocketFrameStreamer(WebSocketFrameStreamer&&) noexcept;
    WebSocketFrameStreamer& operator=(WebSocketFrameStreamer&&) noexcept;

    /**
     * @brief Start the WebSocket server with given configuration
     * @param config Server configuration
     * @return true if server started successfully
     */
    bool start(const WebSocketStreamConfig& config = {});

    /**
     * @brief Stop the WebSocket server
     */
    void stop();

    /**
     * @brief Check if the server is running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get the actual port the server is listening on
     */
    [[nodiscard]] uint16_t port() const;

    /**
     * @brief Get the number of active WebSocket connections
     */
    [[nodiscard]] size_t connectionCount() const;

    /**
     * @brief Push an encoded frame to all clients connected to a session
     * @param sessionId Target render session ID
     * @param frameData Encoded image data (JPEG/PNG bytes)
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @param frameSeq Monotonically increasing frame sequence number
     * @return Number of clients the frame was sent to
     */
    size_t pushFrame(const std::string& sessionId,
                     const std::vector<uint8_t>& frameData,
                     uint32_t width, uint32_t height,
                     uint32_t frameSeq);

    /**
     * @brief Set callback for received input events
     * @param callback Function to call when an input event arrives
     */
    void setInputEventCallback(InputEventCallback callback);

    /**
     * @brief Check if a session has any connected clients
     * @param sessionId Session to check
     */
    [[nodiscard]] bool hasClients(const std::string& sessionId) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

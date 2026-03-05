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
 * @file remote_viewport_widget.hpp
 * @brief Thin-client viewport widget for displaying server-rendered frames
 * @details Connects to a WebSocket render server, receives binary JPEG frames,
 *          and displays them via QPainter. Captures user input events and
 *          serializes them as JSON to send back to the server.
 *
 * ## Architecture
 * ```
 * RemoteViewportWidget (QWidget)
 *   +-- QWebSocket: connects to ws://host:port/render/<session_id>
 *   +-- QImage: latest decoded frame for display
 *   +-- QPainter: draws scaled frame in paintEvent()
 *   +-- Connection overlay: shows status text
 * ```
 *
 * ## Binary Frame Protocol (Server -> Client)
 * ```
 * [4B session_id_len][NB session_id][4B frame_seq]
 * [4B width][4B height][MB encoded_image_data]
 * ```
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QWidget-derived)
 * - QWebSocket callbacks are dispatched to the UI thread by Qt
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Connection state for the remote viewport
 */
enum class RemoteConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting
};

/**
 * @brief Parsed binary frame header from the render server
 */
struct FrameHeader {
    std::string sessionId;
    uint32_t frameSeq = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    const uint8_t* imageData = nullptr;
    size_t imageDataSize = 0;
};

/**
 * @brief Thin-client viewport widget displaying server-rendered frames
 *
 * Replaces ViewportWidget on thin-client deployments. Uses QWebSocket
 * to receive JPEG frames from the render server and QPainter to display
 * them. No VTK dependency required on the client machine.
 *
 * @trace SRS-FR-REMOTE-006
 */
class RemoteViewportWidget : public QWidget {
    Q_OBJECT

public:
    explicit RemoteViewportWidget(QWidget* parent = nullptr);
    ~RemoteViewportWidget() override;

    // Non-copyable
    RemoteViewportWidget(const RemoteViewportWidget&) = delete;
    RemoteViewportWidget& operator=(const RemoteViewportWidget&) = delete;

    /**
     * @brief Connect to a render server
     * @param host Server hostname or IP
     * @param port Server port
     * @param sessionId Render session identifier
     */
    void connectToServer(const QString& host, uint16_t port,
                         const QString& sessionId);

    /**
     * @brief Disconnect from the render server
     */
    void disconnectFromServer();

    /**
     * @brief Get current connection state
     */
    [[nodiscard]] RemoteConnectionState connectionState() const;

    /**
     * @brief Get the session ID
     */
    [[nodiscard]] QString sessionId() const;

    /**
     * @brief Get the number of frames received
     */
    [[nodiscard]] uint64_t framesReceived() const;

    /**
     * @brief Get the last frame sequence number
     */
    [[nodiscard]] uint32_t lastFrameSeq() const;

    /**
     * @brief Parse a binary frame from the render server
     * @param data Raw binary data
     * @param size Data size in bytes
     * @param[out] header Parsed header fields
     * @return true if header was parsed successfully
     */
    static bool parseFrameHeader(const char* data, size_t size,
                                 FrameHeader& header);

signals:
    /**
     * @brief Connection state changed
     */
    void connectionStateChanged(RemoteConnectionState state);

    /**
     * @brief A frame was received and displayed
     * @param frameSeq Frame sequence number
     */
    void frameDisplayed(uint32_t frameSeq);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

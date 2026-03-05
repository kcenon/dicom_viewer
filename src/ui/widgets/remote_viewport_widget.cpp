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

#include "ui/widgets/remote_viewport_widget.hpp"

#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <cstring>

namespace dicom_viewer::ui {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class RemoteViewportWidget::Impl {
public:
    explicit Impl(RemoteViewportWidget* owner) : owner_(owner) {}

    void connectToServer(const QString& host, uint16_t port,
                         const QString& sessionId)
    {
        sessionId_ = sessionId;
        serverHost_ = host;
        serverPort_ = port;

        if (!socket_) {
            socket_ = std::make_unique<QWebSocket>();
            setupConnections();
        }

        setState(RemoteConnectionState::Connecting);
        openConnection();
    }

    void disconnectFromServer()
    {
        reconnectTimer_.stop();
        if (socket_) {
            socket_->close();
        }
        setState(RemoteConnectionState::Disconnected);
    }

    RemoteConnectionState state() const { return state_; }
    QString sessionId() const { return sessionId_; }
    uint64_t framesReceived() const { return framesReceived_; }
    uint32_t lastFrameSeq() const { return lastFrameSeq_; }

    QImage currentFrame_;

private:
    void setupConnections()
    {
        QObject::connect(socket_.get(), &QWebSocket::connected,
                         owner_, [this]() { onConnected(); });

        QObject::connect(socket_.get(), &QWebSocket::disconnected,
                         owner_, [this]() { onDisconnected(); });

        QObject::connect(
            socket_.get(), &QWebSocket::binaryMessageReceived,
            owner_, [this](const QByteArray& message) {
                onBinaryMessage(message);
            });

        reconnectTimer_.setInterval(3000);
        reconnectTimer_.setSingleShot(true);
        QObject::connect(&reconnectTimer_, &QTimer::timeout,
                         owner_, [this]() { attemptReconnect(); });
    }

    void openConnection()
    {
        QString url = QString("ws://%1:%2/render/%3")
                          .arg(serverHost_)
                          .arg(serverPort_)
                          .arg(sessionId_);
        socket_->open(QUrl(url));
    }

    void onConnected()
    {
        reconnectAttempts_ = 0;
        setState(RemoteConnectionState::Connected);
    }

    void onDisconnected()
    {
        if (state_ == RemoteConnectionState::Disconnected) {
            return; // Intentional disconnect
        }

        setState(RemoteConnectionState::Reconnecting);
        scheduleReconnect();
    }

    void scheduleReconnect()
    {
        if (reconnectAttempts_ >= maxReconnectAttempts_) {
            setState(RemoteConnectionState::Disconnected);
            return;
        }

        // Exponential backoff: 3s, 6s, 12s, 24s, 48s (capped)
        int delay = std::min(3000 * (1 << reconnectAttempts_), 48000);
        reconnectTimer_.setInterval(delay);
        reconnectTimer_.start();
    }

    void attemptReconnect()
    {
        ++reconnectAttempts_;
        setState(RemoteConnectionState::Reconnecting);
        openConnection();
    }

    void onBinaryMessage(const QByteArray& message)
    {
        FrameHeader header;
        if (!RemoteViewportWidget::parseFrameHeader(
                message.constData(),
                static_cast<size_t>(message.size()), header)) {
            return;
        }

        QImage frame = QImage::fromData(
            header.imageData,
            static_cast<int>(header.imageDataSize), "JPEG");
        if (frame.isNull()) {
            return;
        }

        currentFrame_ = std::move(frame);
        lastFrameSeq_ = header.frameSeq;
        ++framesReceived_;

        owner_->update();
        emit owner_->frameDisplayed(header.frameSeq);
    }

    void setState(RemoteConnectionState newState)
    {
        if (state_ == newState) {
            return;
        }
        state_ = newState;
        owner_->update(); // Repaint to show status overlay
        emit owner_->connectionStateChanged(newState);
    }

    RemoteViewportWidget* owner_;
    std::unique_ptr<QWebSocket> socket_;
    QTimer reconnectTimer_;

    RemoteConnectionState state_ = RemoteConnectionState::Disconnected;
    QString sessionId_;
    QString serverHost_;
    uint16_t serverPort_ = 0;

    uint64_t framesReceived_ = 0;
    uint32_t lastFrameSeq_ = 0;

    int reconnectAttempts_ = 0;
    static constexpr int maxReconnectAttempts_ = 5;
};

// ---------------------------------------------------------------------------
// Frame header parsing
// ---------------------------------------------------------------------------
bool RemoteViewportWidget::parseFrameHeader(
    const char* data, size_t size, FrameHeader& header)
{
    // Minimum header: session_id_len(4) + frame_seq(4) + width(4) + height(4)
    constexpr size_t minHeaderSize = 4 + 4 + 4 + 4;
    if (!data || size < minHeaderSize) {
        return false;
    }

    const auto* ptr = reinterpret_cast<const uint8_t*>(data);
    size_t offset = 0;

    // session_id_len (4 bytes, little-endian)
    uint32_t sidLen = 0;
    std::memcpy(&sidLen, ptr + offset, 4);
    offset += 4;

    // Validate session_id fits
    if (offset + sidLen + 12 > size) { // +12 for seq+width+height
        return false;
    }

    // session_id (N bytes)
    header.sessionId = std::string(
        reinterpret_cast<const char*>(ptr + offset), sidLen);
    offset += sidLen;

    // frame_seq (4 bytes)
    std::memcpy(&header.frameSeq, ptr + offset, 4);
    offset += 4;

    // width (4 bytes)
    std::memcpy(&header.width, ptr + offset, 4);
    offset += 4;

    // height (4 bytes)
    std::memcpy(&header.height, ptr + offset, 4);
    offset += 4;

    // Remaining bytes are image data
    header.imageData = ptr + offset;
    header.imageDataSize = size - offset;

    return true;
}

// ---------------------------------------------------------------------------
// RemoteViewportWidget lifecycle
// ---------------------------------------------------------------------------
RemoteViewportWidget::RemoteViewportWidget(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>(this))
{
    setMinimumSize(320, 240);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

RemoteViewportWidget::~RemoteViewportWidget() = default;

void RemoteViewportWidget::connectToServer(
    const QString& host, uint16_t port, const QString& sessionId)
{
    impl_->connectToServer(host, port, sessionId);
}

void RemoteViewportWidget::disconnectFromServer()
{
    impl_->disconnectFromServer();
}

RemoteConnectionState RemoteViewportWidget::connectionState() const
{
    return impl_->state();
}

QString RemoteViewportWidget::sessionId() const
{
    return impl_->sessionId();
}

uint64_t RemoteViewportWidget::framesReceived() const
{
    return impl_->framesReceived();
}

uint32_t RemoteViewportWidget::lastFrameSeq() const
{
    return impl_->lastFrameSeq();
}

void RemoteViewportWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    auto widgetRect = rect();

    // Fill background
    painter.fillRect(widgetRect, Qt::black);

    // Draw the current frame scaled to widget size
    if (!impl_->currentFrame_.isNull()) {
        QImage scaled = impl_->currentFrame_.scaled(
            widgetRect.size(), Qt::KeepAspectRatio,
            Qt::SmoothTransformation);

        // Center the image
        int x = (widgetRect.width() - scaled.width()) / 2;
        int y = (widgetRect.height() - scaled.height()) / 2;
        painter.drawImage(x, y, scaled);
    }

    // Connection status overlay
    auto state = impl_->state();
    if (state != RemoteConnectionState::Connected
        || impl_->currentFrame_.isNull()) {
        QString statusText;
        switch (state) {
        case RemoteConnectionState::Disconnected:
            statusText = tr("Disconnected");
            break;
        case RemoteConnectionState::Connecting:
            statusText = tr("Connecting...");
            break;
        case RemoteConnectionState::Reconnecting:
            statusText = tr("Reconnecting...");
            break;
        case RemoteConnectionState::Connected:
            if (impl_->currentFrame_.isNull()) {
                statusText = tr("Waiting for frames...");
            }
            break;
        }

        if (!statusText.isEmpty()) {
            // Semi-transparent background for readability
            QFont font = painter.font();
            font.setPointSize(14);
            painter.setFont(font);

            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(statusText);
            textRect.moveCenter(widgetRect.center());
            textRect.adjust(-12, -6, 12, 6);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 160));
            painter.drawRoundedRect(textRect, 6, 6);

            painter.setPen(Qt::white);
            painter.drawText(widgetRect, Qt::AlignCenter, statusText);
        }
    }
}

} // namespace dicom_viewer::ui

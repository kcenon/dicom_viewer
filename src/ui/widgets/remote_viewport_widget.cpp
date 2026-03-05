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
#include "services/render/frame_encoder.hpp"

#include <QAction>
#include <QContextMenuEvent>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <QWheelEvent>

#include <chrono>
#include <cstring>
#include <deque>

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
    std::string sessionIdStd() const { return sessionId_.toStdString(); }
    uint64_t framesReceived() const { return framesReceived_; }
    uint32_t lastFrameSeq() const { return lastFrameSeq_; }

    void sendInputEvent(const QByteArray& json)
    {
        if (state_ != RemoteConnectionState::Connected || !socket_) {
            return;
        }
        socket_->sendTextMessage(QString::fromUtf8(json));
    }

    QImage currentFrame_;
    QImage baseFrame_;  // Stored base frame for delta tile compositing
    bool showStatistics_ = false;
    std::deque<std::chrono::steady_clock::time_point> frameTimestamps_;
    uint32_t deltaFramesReceived_ = 0;
    uint32_t fullFramesReceived_ = 0;

    double currentFps() const
    {
        if (frameTimestamps_.size() < 2) {
            return 0.0;
        }
        auto elapsed = std::chrono::duration<double>(
            frameTimestamps_.back() - frameTimestamps_.front());
        if (elapsed.count() <= 0.0) {
            return 0.0;
        }
        return static_cast<double>(frameTimestamps_.size() - 1)
               / elapsed.count();
    }

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

        if (header.frameType == FrameType::DeltaFrame) {
            applyDeltaFrame(header);
        } else {
            applyFullFrame(header);
        }

        lastFrameSeq_ = header.frameSeq;
        ++framesReceived_;

        // Track frame timestamps for FPS calculation (keep last 60 samples)
        frameTimestamps_.push_back(std::chrono::steady_clock::now());
        while (frameTimestamps_.size() > 60) {
            frameTimestamps_.pop_front();
        }

        owner_->update();
        emit owner_->frameDisplayed(header.frameSeq);
    }

    void applyFullFrame(const FrameHeader& header)
    {
        QImage frame = QImage::fromData(
            header.imageData,
            static_cast<int>(header.imageDataSize), "JPEG");
        if (frame.isNull()) {
            return;
        }

        currentFrame_ = frame;
        baseFrame_ = frame.convertToFormat(QImage::Format_RGBA8888);
        ++fullFramesReceived_;
    }

    void applyDeltaFrame(const FrameHeader& header)
    {
        std::vector<uint8_t> deltaData(
            header.imageData,
            header.imageData + header.imageDataSize);
        auto delta = dicom_viewer::services::FrameEncoder::deserializeDelta(
            deltaData);

        if (delta.fullFrame || baseFrame_.isNull()) {
            // Full frame fallback or no base frame yet
            if (!delta.tiles.empty()) {
                QImage frame = QImage::fromData(
                    delta.tiles[0].jpegData.data(),
                    static_cast<int>(delta.tiles[0].jpegData.size()),
                    "JPEG");
                if (!frame.isNull()) {
                    currentFrame_ = frame;
                    baseFrame_ = frame.convertToFormat(
                        QImage::Format_RGBA8888);
                }
            }
            ++fullFramesReceived_;
            return;
        }

        // Apply tiles onto base frame
        QImage composited = baseFrame_.copy();

        for (const auto& tile : delta.tiles) {
            QImage tileImage = QImage::fromData(
                tile.jpegData.data(),
                static_cast<int>(tile.jpegData.size()), "JPEG");
            if (tileImage.isNull()) {
                continue;
            }

            QPainter tilePainter(&composited);
            tilePainter.drawImage(tile.x, tile.y, tileImage);
        }

        baseFrame_ = composited;
        currentFrame_ = composited;
        ++deltaFramesReceived_;
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

    // Frame statistics overlay (bottom-left corner)
    if (impl_->showStatistics_
        && impl_->state() == RemoteConnectionState::Connected) {
        double fps = impl_->currentFps();
        QString statsText = QString("FPS: %1 | Frames: %2 | Seq: %3 | Delta: %4 | Full: %5")
                                .arg(fps, 0, 'f', 1)
                                .arg(impl_->framesReceived())
                                .arg(impl_->lastFrameSeq())
                                .arg(impl_->deltaFramesReceived_)
                                .arg(impl_->fullFramesReceived_);

        QFont statsFont = painter.font();
        statsFont.setPointSize(10);
        painter.setFont(statsFont);

        QFontMetrics sfm(statsFont);
        QRect statsRect = sfm.boundingRect(statsText);
        statsRect.moveBottomLeft(
            QPoint(8, widgetRect.height() - 8));
        statsRect.adjust(-4, -2, 4, 2);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 180));
        painter.drawRoundedRect(statsRect, 4, 4);

        painter.setPen(QColor(0, 255, 0));
        painter.drawText(statsRect, Qt::AlignCenter, statsText);
    }
}

// ---------------------------------------------------------------------------
// Input event serialization
// ---------------------------------------------------------------------------

namespace {

std::string qtKeyToKeySym(int key)
{
    switch (key) {
    case Qt::Key_Left:      return "ArrowLeft";
    case Qt::Key_Right:     return "ArrowRight";
    case Qt::Key_Up:        return "ArrowUp";
    case Qt::Key_Down:      return "ArrowDown";
    case Qt::Key_Return:
    case Qt::Key_Enter:     return "Enter";
    case Qt::Key_Escape:    return "Escape";
    case Qt::Key_Space:     return "Space";
    case Qt::Key_Backspace: return "Backspace";
    case Qt::Key_Tab:       return "Tab";
    case Qt::Key_Delete:    return "Delete";
    case Qt::Key_Home:      return "Home";
    case Qt::Key_End:       return "End";
    case Qt::Key_PageUp:    return "PageUp";
    case Qt::Key_PageDown:  return "PageDown";
    case Qt::Key_Shift:     return "Shift";
    case Qt::Key_Control:   return "Control";
    case Qt::Key_Alt:       return "Alt";
    default: {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            return std::string(1, static_cast<char>('a' + (key - Qt::Key_A)));
        }
        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            return std::string(1, static_cast<char>('0' + (key - Qt::Key_0)));
        }
        if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
            return "F" + std::to_string(key - Qt::Key_F1 + 1);
        }
        return {};
    }
    }
}

int qtButtonsToMask(Qt::MouseButtons buttons)
{
    int mask = 0;
    if (buttons & Qt::LeftButton)   mask |= 1;
    if (buttons & Qt::RightButton)  mask |= 2;
    if (buttons & Qt::MiddleButton) mask |= 4;
    return mask;
}

uint64_t currentTimestampMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // anonymous namespace

QByteArray RemoteViewportWidget::serializeInputEvent(
    const std::string& type,
    const std::string& sessionId,
    double x, double y,
    int buttons,
    int keyCode,
    const std::string& keySym,
    double delta,
    bool shiftKey,
    bool ctrlKey,
    bool altKey)
{
    QJsonObject obj;
    obj["session_id"] = QString::fromStdString(sessionId);
    obj["type"] = QString::fromStdString(type);
    obj["x"] = x;
    obj["y"] = y;
    obj["buttons"] = buttons;
    obj["key_code"] = keyCode;
    obj["key"] = QString::fromStdString(keySym);
    obj["ts"] = static_cast<qint64>(currentTimestampMs());
    obj["delta"] = delta;
    obj["shift"] = shiftKey;
    obj["ctrl"] = ctrlKey;
    obj["alt"] = altKey;

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void RemoteViewportWidget::mouseMoveEvent(QMouseEvent* event)
{
    double nx = static_cast<double>(event->position().x()) / width();
    double ny = static_cast<double>(event->position().y()) / height();
    auto mods = event->modifiers();

    auto json = serializeInputEvent(
        "mouse_move", impl_->sessionIdStd(),
        nx, ny,
        qtButtonsToMask(event->buttons()),
        0, {},
        0.0,
        mods & Qt::ShiftModifier,
        mods & Qt::ControlModifier,
        mods & Qt::AltModifier);

    impl_->sendInputEvent(json);
    emit inputEventSent("mouse_move");
    event->accept();
}

void RemoteViewportWidget::mousePressEvent(QMouseEvent* event)
{
    double nx = static_cast<double>(event->position().x()) / width();
    double ny = static_cast<double>(event->position().y()) / height();
    auto mods = event->modifiers();

    auto json = serializeInputEvent(
        "mouse_down", impl_->sessionIdStd(),
        nx, ny,
        qtButtonsToMask(event->buttons()),
        0, {},
        0.0,
        mods & Qt::ShiftModifier,
        mods & Qt::ControlModifier,
        mods & Qt::AltModifier);

    impl_->sendInputEvent(json);
    emit inputEventSent("mouse_down");
    event->accept();
}

void RemoteViewportWidget::mouseReleaseEvent(QMouseEvent* event)
{
    double nx = static_cast<double>(event->position().x()) / width();
    double ny = static_cast<double>(event->position().y()) / height();
    auto mods = event->modifiers();

    // buttons() returns buttons still held; add the released button
    int mask = qtButtonsToMask(event->buttons())
             | qtButtonsToMask(event->button());

    auto json = serializeInputEvent(
        "mouse_up", impl_->sessionIdStd(),
        nx, ny, mask,
        0, {},
        0.0,
        mods & Qt::ShiftModifier,
        mods & Qt::ControlModifier,
        mods & Qt::AltModifier);

    impl_->sendInputEvent(json);
    emit inputEventSent("mouse_up");
    event->accept();
}

void RemoteViewportWidget::wheelEvent(QWheelEvent* event)
{
    double nx = static_cast<double>(event->position().x()) / width();
    double ny = static_cast<double>(event->position().y()) / height();
    auto mods = event->modifiers();

    // angleDelta().y() is in 1/8 degree increments; normalize to whole steps
    double delta = event->angleDelta().y() / 120.0;

    auto json = serializeInputEvent(
        "scroll", impl_->sessionIdStd(),
        nx, ny,
        qtButtonsToMask(event->buttons()),
        0, {},
        delta,
        mods & Qt::ShiftModifier,
        mods & Qt::ControlModifier,
        mods & Qt::AltModifier);

    impl_->sendInputEvent(json);
    emit inputEventSent("scroll");
    event->accept();
}

void RemoteViewportWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }

    auto mods = event->modifiers();
    auto json = serializeInputEvent(
        "key_down", impl_->sessionIdStd(),
        0.0, 0.0, 0,
        event->key(),
        qtKeyToKeySym(event->key()),
        0.0,
        mods & Qt::ShiftModifier,
        mods & Qt::ControlModifier,
        mods & Qt::AltModifier);

    impl_->sendInputEvent(json);
    emit inputEventSent("key_down");
    event->accept();
}

void RemoteViewportWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }

    auto mods = event->modifiers();
    auto json = serializeInputEvent(
        "key_up", impl_->sessionIdStd(),
        0.0, 0.0, 0,
        event->key(),
        qtKeyToKeySym(event->key()),
        0.0,
        mods & Qt::ShiftModifier,
        mods & Qt::ControlModifier,
        mods & Qt::AltModifier);

    impl_->sendInputEvent(json);
    emit inputEventSent("key_up");
    event->accept();
}

void RemoteViewportWidget::setShowStatistics(bool show)
{
    impl_->showStatistics_ = show;
    update();
}

void RemoteViewportWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    auto* statsAction = menu.addAction(tr("Show Statistics"));
    statsAction->setCheckable(true);
    statsAction->setChecked(impl_->showStatistics_);

    connect(statsAction, &QAction::toggled,
            this, &RemoteViewportWidget::setShowStatistics);

    menu.exec(event->globalPos());
    event->accept();
}

} // namespace dicom_viewer::ui

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

#include "services/render/websocket_frame_streamer.hpp"

#include <crow.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstring>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class WebSocketFrameStreamer::Impl {
public:
    Impl() = default;

    ~Impl() { stopServer(); }

    bool startServer(const WebSocketStreamConfig& config)
    {
        if (running_.load()) {
            return false;
        }

        config_ = config;

        app_ = std::make_unique<crow::SimpleApp>();
        app_->port(config.port).concurrency(config.concurrency);

        // WebSocket route: /render/<session_id>
        // Use onaccept to extract session_id from URL path and store via userdata
        CROW_WEBSOCKET_ROUTE((*app_), "/render/<string>")
            .onaccept([this](const crow::request& req, void** userdata) -> bool {
                // Extract session_id from URL: /render/{session_id}
                auto pos = req.url.rfind('/');
                if (pos == std::string::npos || pos + 1 >= req.url.size()) {
                    return false;
                }
                auto* sid = new std::string(req.url.substr(pos + 1));

                // Check max connections
                std::lock_guard lock(mutex_);
                if (connectionSessions_.size() >= config_.maxConnections) {
                    delete sid;
                    return false;
                }

                *userdata = sid;
                return true;
            })
            .onopen([this](crow::websocket::connection& conn) {
                auto* sid = static_cast<std::string*>(conn.userdata());
                if (sid) {
                    onOpen(conn, *sid);
                }
            })
            .onmessage([this](crow::websocket::connection& conn,
                              const std::string& message, bool isBinary) {
                onMessage(conn, message, isBinary);
            })
            .onclose([this](crow::websocket::connection& conn,
                            const std::string& reason, uint16_t statusCode) {
                onClose(conn, reason, statusCode);
                // Clean up userdata allocated in onaccept
                auto* sid = static_cast<std::string*>(conn.userdata());
                delete sid;
                conn.userdata(nullptr);
            });

        // Start in background thread
        serverThread_ = std::thread([this]() {
            app_->run();
        });

        // Wait briefly for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        running_.store(true);
        port_.store(config.port);

        return true;
    }

    void stopServer()
    {
        if (!running_.load()) {
            return;
        }

        if (app_) {
            app_->stop();
        }

        if (serverThread_.joinable()) {
            serverThread_.join();
        }

        running_.store(false);
        port_.store(0);

        std::lock_guard lock(mutex_);
        sessions_.clear();
        connectionSessions_.clear();
    }

    size_t pushFrame(const std::string& sessionId,
                     const std::vector<uint8_t>& frameData,
                     uint32_t width, uint32_t height,
                     uint32_t frameSeq)
    {
        // Build binary frame header
        std::string binaryFrame = buildBinaryFrame(
            sessionId, frameData, width, height, frameSeq);

        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return 0;
        }

        size_t sent = 0;
        // Copy the set to avoid issues if send triggers close
        auto connections = it->second;
        for (auto* conn : connections) {
            try {
                conn->send_binary(binaryFrame);
                ++sent;
            } catch (...) {
                // Connection may have been closed
            }
        }
        return sent;
    }

    void setInputEventCallback(InputEventCallback callback)
    {
        std::lock_guard lock(mutex_);
        inputCallback_ = std::move(callback);
    }

    [[nodiscard]] bool isRunning() const { return running_.load(); }
    [[nodiscard]] uint16_t port() const { return port_.load(); }

    [[nodiscard]] size_t connectionCount() const
    {
        std::lock_guard lock(mutex_);
        return connectionSessions_.size();
    }

    [[nodiscard]] bool hasClients(const std::string& sessionId) const
    {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        return it != sessions_.end() && !it->second.empty();
    }

private:
    void onOpen(crow::websocket::connection& conn,
                const std::string& sessionId)
    {
        std::lock_guard lock(mutex_);
        sessions_[sessionId].insert(&conn);
        connectionSessions_[&conn] = sessionId;
    }

    void onMessage(crow::websocket::connection& /*conn*/,
                   const std::string& message, bool isBinary)
    {
        if (isBinary) {
            return; // Server only accepts text (JSON) from clients
        }

        InputEventCallback cb;
        {
            std::lock_guard lock(mutex_);
            cb = inputCallback_;
        }

        if (!cb) {
            return;
        }

        try {
            auto json = nlohmann::json::parse(message);
            InputEvent event;
            event.sessionId = json.value("session_id", "");
            event.type = json.value("type", "");
            event.x = json.value("x", 0.0);
            event.y = json.value("y", 0.0);
            event.buttons = json.value("buttons", 0);
            event.keyCode = json.value("key_code", 0);
            event.timestamp = json.value("ts", uint64_t{0});
            event.delta = json.value("delta", 0.0);
            event.shiftKey = json.value("shift", false);
            event.ctrlKey = json.value("ctrl", false);
            event.altKey = json.value("alt", false);
            event.keySym = json.value("key", std::string{});

            cb(event);
        } catch (const nlohmann::json::exception&) {
            // Ignore malformed JSON
        }
    }

    void onClose(crow::websocket::connection& conn,
                 const std::string& /*reason*/, uint16_t /*statusCode*/)
    {
        std::lock_guard lock(mutex_);

        auto it = connectionSessions_.find(&conn);
        if (it != connectionSessions_.end()) {
            auto& sessionId = it->second;
            auto sessIt = sessions_.find(sessionId);
            if (sessIt != sessions_.end()) {
                sessIt->second.erase(&conn);
                if (sessIt->second.empty()) {
                    sessions_.erase(sessIt);
                }
            }
            connectionSessions_.erase(it);
        }
    }

    static std::string buildBinaryFrame(
        const std::string& sessionId,
        const std::vector<uint8_t>& frameData,
        uint32_t width, uint32_t height, uint32_t frameSeq)
    {
        // Header: session_id_len(4) + session_id(N) + frame_seq(4)
        //         + width(4) + height(4) + data(M)
        uint32_t sidLen = static_cast<uint32_t>(sessionId.size());
        size_t totalSize = 4 + sidLen + 4 + 4 + 4 + frameData.size();

        std::string frame(totalSize, '\0');
        char* ptr = frame.data();

        std::memcpy(ptr, &sidLen, 4);       ptr += 4;
        std::memcpy(ptr, sessionId.data(), sidLen); ptr += sidLen;
        std::memcpy(ptr, &frameSeq, 4);     ptr += 4;
        std::memcpy(ptr, &width, 4);        ptr += 4;
        std::memcpy(ptr, &height, 4);       ptr += 4;
        std::memcpy(ptr, frameData.data(), frameData.size());

        return frame;
    }

    WebSocketStreamConfig config_;
    std::unique_ptr<crow::SimpleApp> app_;
    std::thread serverThread_;

    std::atomic<bool> running_{false};
    std::atomic<uint16_t> port_{0};

    mutable std::mutex mutex_;

    // session_id -> set of active connections
    std::unordered_map<std::string,
                       std::set<crow::websocket::connection*>> sessions_;

    // connection -> session_id (reverse map for cleanup)
    std::unordered_map<crow::websocket::connection*,
                       std::string> connectionSessions_;

    InputEventCallback inputCallback_;
};

// ---------------------------------------------------------------------------
// WebSocketFrameStreamer lifecycle
// ---------------------------------------------------------------------------
WebSocketFrameStreamer::WebSocketFrameStreamer()
    : impl_(std::make_unique<Impl>())
{
}

WebSocketFrameStreamer::~WebSocketFrameStreamer() = default;

WebSocketFrameStreamer::WebSocketFrameStreamer(
    WebSocketFrameStreamer&&) noexcept = default;
WebSocketFrameStreamer& WebSocketFrameStreamer::operator=(
    WebSocketFrameStreamer&&) noexcept = default;

bool WebSocketFrameStreamer::start(const WebSocketStreamConfig& config)
{
    if (!impl_) return false;
    return impl_->startServer(config);
}

void WebSocketFrameStreamer::stop()
{
    if (!impl_) return;
    impl_->stopServer();
}

bool WebSocketFrameStreamer::isRunning() const
{
    if (!impl_) return false;
    return impl_->isRunning();
}

uint16_t WebSocketFrameStreamer::port() const
{
    if (!impl_) return 0;
    return impl_->port();
}

size_t WebSocketFrameStreamer::connectionCount() const
{
    if (!impl_) return 0;
    return impl_->connectionCount();
}

size_t WebSocketFrameStreamer::pushFrame(
    const std::string& sessionId,
    const std::vector<uint8_t>& frameData,
    uint32_t width, uint32_t height, uint32_t frameSeq)
{
    if (!impl_) return 0;
    return impl_->pushFrame(sessionId, frameData, width, height, frameSeq);
}

void WebSocketFrameStreamer::setInputEventCallback(InputEventCallback callback)
{
    if (!impl_) return;
    impl_->setInputEventCallback(std::move(callback));
}

bool WebSocketFrameStreamer::hasClients(const std::string& sessionId) const
{
    if (!impl_) return false;
    return impl_->hasClients(sessionId);
}

} // namespace dicom_viewer::services

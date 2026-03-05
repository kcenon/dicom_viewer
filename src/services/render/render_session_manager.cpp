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

#include "services/render/render_session_manager.hpp"
#include "services/render/adaptive_quality_controller.hpp"
#include "services/render/render_session.hpp"
#include "services/render/session_token_validator.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class RenderSessionManager::Impl {
public:
    struct SessionEntry {
        std::unique_ptr<RenderSession> session;
        std::chrono::steady_clock::time_point lastActive;
        uint32_t width;
        uint32_t height;
        uint32_t frameSeq = 0;
        AdaptiveQualityController qualityController;
    };

    explicit Impl(const RenderSessionManagerConfig& config) : config_(config) {}

    ~Impl() { stopLoop(); }

    bool createSession(const std::string& sessionId,
                       uint32_t width, uint32_t height)
    {
        std::lock_guard lock(mutex_);

        if (sessions_.count(sessionId) > 0) {
            return false;
        }

        if (config_.maxSessions > 0
            && sessions_.size() >= config_.maxSessions) {
            return false;
        }

        uint32_t w = (width > 0) ? width : config_.defaultWidth;
        uint32_t h = (height > 0) ? height : config_.defaultHeight;

        SessionEntry entry;
        entry.session = std::make_unique<RenderSession>(w, h);
        entry.lastActive = std::chrono::steady_clock::now();
        entry.width = w;
        entry.height = h;

        sessions_.emplace(sessionId, std::move(entry));
        return true;
    }

    bool destroySession(const std::string& sessionId)
    {
        std::lock_guard lock(mutex_);
        return sessions_.erase(sessionId) > 0;
    }

    bool hasSession(const std::string& sessionId) const
    {
        std::lock_guard lock(mutex_);
        return sessions_.count(sessionId) > 0;
    }

    RenderSession* getSession(const std::string& sessionId)
    {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return nullptr;
        }
        return it->second.session.get();
    }

    void touchSession(const std::string& sessionId)
    {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.lastActive = std::chrono::steady_clock::now();
        }
    }

    void notifyInteractionStart(const std::string& sessionId)
    {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.qualityController.onInteractionStart();
            it->second.lastActive = std::chrono::steady_clock::now();
        }
    }

    void notifyInteractionEnd(const std::string& sessionId)
    {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.qualityController.onInteractionEnd();
        }
    }

    AdaptiveQualityController* getQualityController(
        const std::string& sessionId)
    {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return nullptr;
        }
        return &it->second.qualityController;
    }

    void setFrameReadyCallback(FrameReadyCallback callback)
    {
        std::lock_guard lock(mutex_);
        frameCallback_ = std::move(callback);
    }

    void startLoop()
    {
        if (running_.load()) {
            return;
        }

        running_.store(true);
        renderThread_ = std::thread([this]() { renderLoop(); });
    }

    void stopLoop()
    {
        if (!running_.load()) {
            return;
        }

        {
            std::lock_guard lock(cvMutex_);
            running_.store(false);
        }
        cv_.notify_one();

        if (renderThread_.joinable()) {
            renderThread_.join();
        }
    }

    bool isRenderLoopRunning() const { return running_.load(); }

    size_t cleanupIdleSessions()
    {
        if (config_.idleTimeoutSeconds == 0) {
            return 0;
        }

        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(config_.idleTimeoutSeconds);

        std::lock_guard lock(mutex_);
        size_t removed = 0;

        for (auto it = sessions_.begin(); it != sessions_.end(); ) {
            if ((now - it->second.lastActive) > timeout) {
                it = sessions_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }

        return removed;
    }

    size_t activeSessionCount() const
    {
        std::lock_guard lock(mutex_);
        return sessions_.size();
    }

    std::vector<std::string> activeSessionIds() const
    {
        std::lock_guard lock(mutex_);
        std::vector<std::string> ids;
        ids.reserve(sessions_.size());
        for (const auto& [id, _] : sessions_) {
            ids.push_back(id);
        }
        return ids;
    }

    const RenderSessionManagerConfig& config() const { return config_; }

    SessionTokenValidator& tokenValidator() { return tokenValidator_; }

private:
    void renderLoop()
    {
        using clock = std::chrono::steady_clock;
        auto frameDuration = std::chrono::microseconds(
            config_.targetFps > 0 ? 1'000'000 / config_.targetFps : 33'333);

        while (running_.load()) {
            auto frameStart = clock::now();

            renderAllSessions();

            // Sleep until next frame, waking early if stopped
            auto elapsed = clock::now() - frameStart;
            auto sleepTime = frameDuration - elapsed;
            if (sleepTime > std::chrono::microseconds::zero()) {
                std::unique_lock lock(cvMutex_);
                cv_.wait_for(lock, sleepTime, [this]() {
                    return !running_.load();
                });
            }
        }
    }

    void renderAllSessions()
    {
        // Snapshot session IDs and callback under lock
        std::vector<std::string> ids;
        FrameReadyCallback cb;

        {
            std::lock_guard lock(mutex_);
            cb = frameCallback_;
            if (!cb || sessions_.empty()) {
                return;
            }
            ids.reserve(sessions_.size());
            for (const auto& [id, _] : sessions_) {
                ids.push_back(id);
            }
        }

        // Render each session (lock per session to avoid holding global lock)
        for (const auto& id : ids) {
            std::lock_guard lock(mutex_);
            auto it = sessions_.find(id);
            if (it == sessions_.end()) {
                continue;
            }

            auto& entry = it->second;

            // Check adaptive quality controller
            if (!entry.qualityController.shouldEmitFrame()) {
                continue;
            }

            auto frame = entry.session->captureVolumeFrame();
            if (!frame.empty()) {
                cb(id, frame, entry.width, entry.height);
                ++entry.frameSeq;
            }
        }
    }

    RenderSessionManagerConfig config_;
    SessionTokenValidator tokenValidator_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SessionEntry> sessions_;
    FrameReadyCallback frameCallback_;

    std::atomic<bool> running_{false};
    std::thread renderThread_;
    std::mutex cvMutex_;
    std::condition_variable cv_;
};

// ---------------------------------------------------------------------------
// RenderSessionManager lifecycle
// ---------------------------------------------------------------------------
RenderSessionManager::RenderSessionManager(
    const RenderSessionManagerConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

RenderSessionManager::~RenderSessionManager() = default;

bool RenderSessionManager::createSession(const std::string& sessionId,
                                         uint32_t width, uint32_t height)
{
    return impl_->createSession(sessionId, width, height);
}

bool RenderSessionManager::destroySession(const std::string& sessionId)
{
    return impl_->destroySession(sessionId);
}

bool RenderSessionManager::hasSession(const std::string& sessionId) const
{
    return impl_->hasSession(sessionId);
}

RenderSession* RenderSessionManager::getSession(const std::string& sessionId)
{
    return impl_->getSession(sessionId);
}

void RenderSessionManager::touchSession(const std::string& sessionId)
{
    impl_->touchSession(sessionId);
}

void RenderSessionManager::notifyInteractionStart(
    const std::string& sessionId)
{
    impl_->notifyInteractionStart(sessionId);
}

void RenderSessionManager::notifyInteractionEnd(
    const std::string& sessionId)
{
    impl_->notifyInteractionEnd(sessionId);
}

AdaptiveQualityController* RenderSessionManager::getQualityController(
    const std::string& sessionId)
{
    return impl_->getQualityController(sessionId);
}

void RenderSessionManager::setFrameReadyCallback(FrameReadyCallback callback)
{
    impl_->setFrameReadyCallback(std::move(callback));
}

void RenderSessionManager::startRenderLoop()
{
    impl_->startLoop();
}

void RenderSessionManager::stopRenderLoop()
{
    impl_->stopLoop();
}

bool RenderSessionManager::isRenderLoopRunning() const
{
    return impl_->isRenderLoopRunning();
}

size_t RenderSessionManager::cleanupIdleSessions()
{
    return impl_->cleanupIdleSessions();
}

size_t RenderSessionManager::activeSessionCount() const
{
    return impl_->activeSessionCount();
}

std::vector<std::string> RenderSessionManager::activeSessionIds() const
{
    return impl_->activeSessionIds();
}

const RenderSessionManagerConfig& RenderSessionManager::config() const
{
    return impl_->config();
}

SessionTokenValidator* RenderSessionManager::tokenValidator()
{
    return &impl_->tokenValidator();
}

TokenValidationResult RenderSessionManager::validateSessionToken(
    const std::string& token,
    const std::string& requiredStudyUid)
{
    return impl_->tokenValidator().validateToken(token, requiredStudyUid);
}

std::string RenderSessionManager::generateSessionToken(
    const std::string& userId,
    const std::string& studyUid)
{
    return impl_->tokenValidator().generateToken(userId, studyUid);
}

} // namespace dicom_viewer::services

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

#include "services/render/adaptive_quality_controller.hpp"

#include <chrono>
#include <mutex>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class AdaptiveQualityController::Impl {
public:
    explicit Impl(const AdaptiveQualityConfig& config)
        : config_(config)
    {
    }

    void onInteractionStart()
    {
        std::lock_guard lock(mutex_);
        state_ = QualityState::Interacting;
        postInteractionFrameEmitted_ = false;
    }

    void onInteractionEnd()
    {
        std::lock_guard lock(mutex_);
        if (state_ != QualityState::Interacting) {
            return;
        }
        state_ = QualityState::PostInteraction;
        interactionEndTime_ = std::chrono::steady_clock::now();
        postInteractionFrameEmitted_ = false;
    }

    QualityState state() const
    {
        std::lock_guard lock(mutex_);
        return state_;
    }

    int currentQuality() const
    {
        std::lock_guard lock(mutex_);
        switch (state_) {
        case QualityState::Interacting:
            return config_.interactionQuality;
        case QualityState::PostInteraction:
        case QualityState::Idle:
            return config_.idleQuality;
        }
        return config_.idleQuality;
    }

    uint32_t currentTargetFps() const
    {
        std::lock_guard lock(mutex_);
        switch (state_) {
        case QualityState::Interacting:
            return config_.interactionFps;
        case QualityState::PostInteraction:
        case QualityState::Idle:
            return config_.idleFps;
        }
        return config_.idleFps;
    }

    bool shouldEmitFrame()
    {
        std::lock_guard lock(mutex_);
        switch (state_) {
        case QualityState::Idle:
            return false;

        case QualityState::Interacting:
            return true;

        case QualityState::PostInteraction: {
            auto elapsed = std::chrono::steady_clock::now() - interactionEndTime_;
            auto debounce = std::chrono::milliseconds(config_.debounceMs);

            if (elapsed < debounce) {
                // Still within debounce window — no frame yet
                return false;
            }

            if (!postInteractionFrameEmitted_) {
                // Emit one high-quality frame
                postInteractionFrameEmitted_ = true;
                return true;
            }

            // High-quality frame already emitted — transition to Idle
            state_ = QualityState::Idle;
            return false;
        }
        }
        return false;
    }

    void setConfig(const AdaptiveQualityConfig& config)
    {
        std::lock_guard lock(mutex_);
        config_ = config;
    }

    const AdaptiveQualityConfig& config() const
    {
        std::lock_guard lock(mutex_);
        return config_;
    }

private:
    mutable std::mutex mutex_;
    AdaptiveQualityConfig config_;
    QualityState state_ = QualityState::Idle;
    std::chrono::steady_clock::time_point interactionEndTime_;
    bool postInteractionFrameEmitted_ = false;
};

// ---------------------------------------------------------------------------
// AdaptiveQualityController lifecycle
// ---------------------------------------------------------------------------
AdaptiveQualityController::AdaptiveQualityController(
    const AdaptiveQualityConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

AdaptiveQualityController::~AdaptiveQualityController() = default;

AdaptiveQualityController::AdaptiveQualityController(
    AdaptiveQualityController&&) noexcept = default;

AdaptiveQualityController& AdaptiveQualityController::operator=(
    AdaptiveQualityController&&) noexcept = default;

void AdaptiveQualityController::onInteractionStart()
{
    impl_->onInteractionStart();
}

void AdaptiveQualityController::onInteractionEnd()
{
    impl_->onInteractionEnd();
}

QualityState AdaptiveQualityController::state() const
{
    return impl_->state();
}

int AdaptiveQualityController::currentQuality() const
{
    return impl_->currentQuality();
}

uint32_t AdaptiveQualityController::currentTargetFps() const
{
    return impl_->currentTargetFps();
}

bool AdaptiveQualityController::shouldEmitFrame()
{
    return impl_->shouldEmitFrame();
}

void AdaptiveQualityController::setConfig(const AdaptiveQualityConfig& config)
{
    impl_->setConfig(config);
}

const AdaptiveQualityConfig& AdaptiveQualityController::config() const
{
    return impl_->config();
}

} // namespace dicom_viewer::services

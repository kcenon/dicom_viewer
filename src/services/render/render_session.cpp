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

#include "services/render/render_session.hpp"
#include "services/volume_renderer.hpp"
#include "services/mpr_renderer.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <format>
#include <mutex>

namespace dicom_viewer::services {

class RenderSession::Impl {
public:
    std::unique_ptr<VolumeRenderer> volume;
    std::unique_ptr<MPRRenderer> mpr;
    std::mutex renderMutex;

    Impl(uint32_t width, uint32_t height)
        : volume(std::make_unique<VolumeRenderer>())
        , mpr(std::make_unique<MPRRenderer>())
    {
        volume->enableOffscreenMode(width, height);
        mpr->enableOffscreenMode(width, height);
        LOG_INFO(std::format("Render session created: {}x{}", width, height));
    }
};

RenderSession::RenderSession(uint32_t width, uint32_t height)
    : impl_(std::make_unique<Impl>(width, height))
{
}

RenderSession::~RenderSession() = default;
RenderSession::RenderSession(RenderSession&&) noexcept = default;
RenderSession& RenderSession::operator=(RenderSession&&) noexcept = default;

VolumeRenderer& RenderSession::volumeRenderer()
{
    return *impl_->volume;
}

MPRRenderer& RenderSession::mprRenderer()
{
    return *impl_->mpr;
}

void RenderSession::setInputData(vtkSmartPointer<vtkImageData> imageData)
{
    impl_->volume->setInputData(imageData);
    impl_->mpr->setInputData(imageData);
}

std::vector<uint8_t> RenderSession::captureVolumeFrame()
{
    std::lock_guard<std::mutex> lock(impl_->renderMutex);
    return impl_->volume->captureFrame();
}

std::vector<uint8_t> RenderSession::captureMPRFrame(MPRPlane plane)
{
    std::lock_guard<std::mutex> lock(impl_->renderMutex);
    return impl_->mpr->captureFrame(plane);
}

void RenderSession::resize(uint32_t width, uint32_t height)
{
    std::lock_guard<std::mutex> lock(impl_->renderMutex);
    impl_->volume->resizeOffscreen(width, height);
    impl_->mpr->resizeOffscreen(width, height);
}

} // namespace dicom_viewer::services

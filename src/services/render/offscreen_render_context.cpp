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

#include "services/render/offscreen_render_context.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkWindowToImageFilter.h>
#include <vtkImageData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPointData.h>

#include <format>

namespace dicom_viewer::services {

class OffscreenRenderContext::Impl {
public:
    vtkSmartPointer<vtkRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkWindowToImageFilter> windowToImage;
    uint32_t width = 0;
    uint32_t height = 0;
    bool initialized = false;
};

OffscreenRenderContext::OffscreenRenderContext() : impl_(std::make_unique<Impl>()) {}
OffscreenRenderContext::~OffscreenRenderContext() = default;
OffscreenRenderContext::OffscreenRenderContext(OffscreenRenderContext&&) noexcept = default;
OffscreenRenderContext& OffscreenRenderContext::operator=(OffscreenRenderContext&&) noexcept = default;

void OffscreenRenderContext::initialize(uint32_t width, uint32_t height)
{
    impl_->renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    impl_->renderWindow->SetOffScreenRendering(1);
    impl_->renderWindow->SetSize(static_cast<int>(width), static_cast<int>(height));

    impl_->renderer = vtkSmartPointer<vtkRenderer>::New();
    impl_->renderer->SetBackground(0.0, 0.0, 0.0);
    impl_->renderWindow->AddRenderer(impl_->renderer);

    impl_->windowToImage = vtkSmartPointer<vtkWindowToImageFilter>::New();
    impl_->windowToImage->SetInput(impl_->renderWindow);
    impl_->windowToImage->SetInputBufferTypeToRGBA();
    impl_->windowToImage->ReadFrontBufferOff();

    impl_->width = width;
    impl_->height = height;
    impl_->initialized = true;

    LOG_INFO(std::format("Off-screen render context initialized: {}x{}", width, height));
}

bool OffscreenRenderContext::isInitialized() const
{
    return impl_->initialized;
}

vtkRenderWindow* OffscreenRenderContext::getRenderWindow() const
{
    return impl_->renderWindow;
}

vtkRenderer* OffscreenRenderContext::getRenderer() const
{
    return impl_->renderer;
}

void OffscreenRenderContext::resize(uint32_t width, uint32_t height)
{
    if (!impl_->initialized) {
        return;
    }

    impl_->renderWindow->SetSize(static_cast<int>(width), static_cast<int>(height));
    impl_->width = width;
    impl_->height = height;

    // Reset the filter so it picks up the new window size
    impl_->windowToImage->Modified();

    LOG_INFO(std::format("Off-screen render context resized: {}x{}", width, height));
}

std::pair<uint32_t, uint32_t> OffscreenRenderContext::getSize() const
{
    return {impl_->width, impl_->height};
}

bool OffscreenRenderContext::supportsOpenGL() const
{
    if (!impl_->initialized || !impl_->renderWindow) {
        return false;
    }
    return impl_->renderWindow->SupportsOpenGL();
}

std::vector<uint8_t> OffscreenRenderContext::captureFrame()
{
    if (!impl_->initialized) {
        return {};
    }

    // Check OpenGL support before attempting to render.
    // On headless macOS (no display), OpenGL context creation fails and
    // calling Render() would crash. Return empty in that case.
    if (!impl_->renderWindow->SupportsOpenGL()) {
        LOG_WARNING("Off-screen render window does not support OpenGL");
        return {};
    }

    impl_->renderWindow->Render();
    impl_->windowToImage->Modified();
    impl_->windowToImage->Update();

    vtkImageData* image = impl_->windowToImage->GetOutput();
    if (!image) {
        LOG_WARNING("Off-screen capture produced null image");
        return {};
    }

    int* dims = image->GetDimensions();
    size_t totalBytes = static_cast<size_t>(dims[0]) * dims[1] * 4;

    auto* scalars = image->GetPointData()->GetScalars();
    if (!scalars) {
        return {};
    }

    auto* rawPtr = static_cast<uint8_t*>(scalars->GetVoidPointer(0));
    return {rawPtr, rawPtr + totalBytes};
}

} // namespace dicom_viewer::services

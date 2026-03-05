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

#include "services/render/offscreen_render_context.hpp"

#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

using namespace dicom_viewer::services;

class OffscreenRenderContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        context = std::make_unique<OffscreenRenderContext>();
    }

    void TearDown() override {
        context.reset();
    }

    std::unique_ptr<OffscreenRenderContext> context;
};

// Test construction
TEST_F(OffscreenRenderContextTest, DefaultConstruction) {
    EXPECT_NE(context, nullptr);
    EXPECT_FALSE(context->isInitialized());
}

// Test initialization
TEST_F(OffscreenRenderContextTest, Initialize) {
    context->initialize(800, 600);
    EXPECT_TRUE(context->isInitialized());
}

TEST_F(OffscreenRenderContextTest, GetRenderWindowAfterInit) {
    context->initialize(800, 600);
    EXPECT_NE(context->getRenderWindow(), nullptr);
}

TEST_F(OffscreenRenderContextTest, GetRendererAfterInit) {
    context->initialize(800, 600);
    EXPECT_NE(context->getRenderer(), nullptr);
}

TEST_F(OffscreenRenderContextTest, GetRenderWindowBeforeInit) {
    EXPECT_EQ(context->getRenderWindow(), nullptr);
}

TEST_F(OffscreenRenderContextTest, GetRendererBeforeInit) {
    EXPECT_EQ(context->getRenderer(), nullptr);
}

TEST_F(OffscreenRenderContextTest, OffScreenRenderingEnabled) {
    context->initialize(800, 600);
    auto* renderWindow = context->getRenderWindow();
    ASSERT_NE(renderWindow, nullptr);
    EXPECT_TRUE(renderWindow->GetOffScreenRendering());
}

// Test size
TEST_F(OffscreenRenderContextTest, GetSizeAfterInit) {
    context->initialize(1024, 768);
    auto [width, height] = context->getSize();
    EXPECT_EQ(width, 1024u);
    EXPECT_EQ(height, 768u);
}

TEST_F(OffscreenRenderContextTest, GetSizeBeforeInit) {
    auto [width, height] = context->getSize();
    EXPECT_EQ(width, 0u);
    EXPECT_EQ(height, 0u);
}

// Test resize
TEST_F(OffscreenRenderContextTest, Resize) {
    context->initialize(800, 600);
    context->resize(1920, 1080);
    auto [width, height] = context->getSize();
    EXPECT_EQ(width, 1920u);
    EXPECT_EQ(height, 1080u);
}

TEST_F(OffscreenRenderContextTest, ResizeBeforeInit) {
    EXPECT_NO_THROW(context->resize(1920, 1080));
    // Size should remain 0 since not initialized
    auto [width, height] = context->getSize();
    EXPECT_EQ(width, 0u);
    EXPECT_EQ(height, 0u);
}

// Test OpenGL support check
TEST_F(OffscreenRenderContextTest, SupportsOpenGLBeforeInit) {
    EXPECT_FALSE(context->supportsOpenGL());
}

TEST_F(OffscreenRenderContextTest, SupportsOpenGLAfterInit) {
    context->initialize(64, 48);
    // Result depends on environment; just verify no crash
    (void)context->supportsOpenGL();
}

// Test capture
TEST_F(OffscreenRenderContextTest, CaptureFrameReturnsCorrectSize) {
    context->initialize(64, 48);
    if (!context->supportsOpenGL()) {
        GTEST_SKIP() << "OpenGL not available in this environment";
    }
    auto frame = context->captureFrame();
    // 64 * 48 * 4 (RGBA) = 12288 bytes
    EXPECT_EQ(frame.size(), 64u * 48u * 4u);
}

TEST_F(OffscreenRenderContextTest, CaptureFrameBeforeInit) {
    auto frame = context->captureFrame();
    EXPECT_TRUE(frame.empty());
}

TEST_F(OffscreenRenderContextTest, CaptureFrameAfterResize) {
    context->initialize(32, 32);
    if (!context->supportsOpenGL()) {
        GTEST_SKIP() << "OpenGL not available in this environment";
    }
    context->resize(64, 64);
    auto frame = context->captureFrame();
    EXPECT_EQ(frame.size(), 64u * 64u * 4u);
}

// Test move semantics
TEST_F(OffscreenRenderContextTest, MoveConstructor) {
    context->initialize(800, 600);
    OffscreenRenderContext moved(std::move(*context));
    EXPECT_TRUE(moved.isInitialized());
    auto [width, height] = moved.getSize();
    EXPECT_EQ(width, 800u);
    EXPECT_EQ(height, 600u);
}

TEST_F(OffscreenRenderContextTest, MoveAssignment) {
    context->initialize(800, 600);
    OffscreenRenderContext other;
    other = std::move(*context);
    EXPECT_TRUE(other.isInitialized());
    auto [width, height] = other.getSize();
    EXPECT_EQ(width, 800u);
    EXPECT_EQ(height, 600u);
}

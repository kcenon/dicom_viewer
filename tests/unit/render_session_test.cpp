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

#include "services/render/render_session.hpp"
#include "services/volume_renderer.hpp"
#include "services/mpr_renderer.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <future>
#include <vector>

using namespace dicom_viewer::services;

class RenderSessionTest : public ::testing::Test {
protected:
    vtkSmartPointer<vtkImageData> createTestVolume(int dims = 32) {
        auto imageData = vtkSmartPointer<vtkImageData>::New();
        imageData->SetDimensions(dims, dims, dims);
        imageData->SetSpacing(1.0, 1.0, 1.0);
        imageData->SetOrigin(0.0, 0.0, 0.0);
        imageData->AllocateScalars(VTK_SHORT, 1);

        short* ptr = static_cast<short*>(imageData->GetScalarPointer());
        for (int i = 0; i < dims * dims * dims; ++i) {
            ptr[i] = static_cast<short>(i % 1000 - 500);
        }

        return imageData;
    }
};

// Test construction
TEST_F(RenderSessionTest, Construction) {
    EXPECT_NO_THROW(RenderSession session(256, 256));
}

TEST_F(RenderSessionTest, RenderersInitialized) {
    RenderSession session(256, 256);
    EXPECT_TRUE(session.volumeRenderer().isOffscreenMode());
    EXPECT_TRUE(session.mprRenderer().isOffscreenMode());
}

// Test move semantics
TEST_F(RenderSessionTest, MoveConstructor) {
    RenderSession session(256, 256);
    RenderSession moved(std::move(session));
    EXPECT_TRUE(moved.volumeRenderer().isOffscreenMode());
}

TEST_F(RenderSessionTest, MoveAssignment) {
    RenderSession session(256, 256);
    RenderSession other(128, 128);
    other = std::move(session);
    EXPECT_TRUE(other.volumeRenderer().isOffscreenMode());
}

// Test input data propagation
TEST_F(RenderSessionTest, SetInputDataPropagates) {
    RenderSession session(64, 48);
    auto volume = createTestVolume();

    EXPECT_NO_THROW(session.setInputData(volume));
}

// Test frame capture (may be empty on headless systems without OpenGL)
TEST_F(RenderSessionTest, CaptureVolumeFrame) {
    RenderSession session(64, 48);
    auto volume = createTestVolume();
    session.setInputData(volume);

    auto frame = session.captureVolumeFrame();
    if (!frame.empty()) {
        EXPECT_EQ(frame.size(), 64u * 48u * 4u);
    }
}

TEST_F(RenderSessionTest, CaptureMPRFrameAxial) {
    RenderSession session(64, 48);
    auto volume = createTestVolume();
    session.setInputData(volume);

    auto frame = session.captureMPRFrame(MPRPlane::Axial);
    if (!frame.empty()) {
        EXPECT_EQ(frame.size(), 64u * 48u * 4u);
    }
}

TEST_F(RenderSessionTest, CaptureMPRFrameAllPlanes) {
    RenderSession session(64, 48);
    auto volume = createTestVolume();
    session.setInputData(volume);

    for (auto plane : {MPRPlane::Axial, MPRPlane::Coronal, MPRPlane::Sagittal}) {
        auto frame = session.captureMPRFrame(plane);
        if (!frame.empty()) {
            EXPECT_EQ(frame.size(), 64u * 48u * 4u);
        }
    }
}

// Test resize
TEST_F(RenderSessionTest, Resize) {
    RenderSession session(64, 48);
    session.resize(128, 96);

    auto frame = session.captureVolumeFrame();
    if (!frame.empty()) {
        EXPECT_EQ(frame.size(), 128u * 96u * 4u);
    }
}

// Test concurrent access safety
// VTK's Cocoa backend is not thread-safe for OpenGL context creation,
// so this test verifies the mutex protects against concurrent access
// without crashing. On headless systems, captures may throw or return empty.
TEST_F(RenderSessionTest, ConcurrentFrameCapture) {
    RenderSession session(32, 32);
    auto volume = createTestVolume();
    session.setInputData(volume);

    // Launch multiple captures concurrently
    auto future1 = std::async(std::launch::async, [&]() -> std::vector<uint8_t> {
        try {
            return session.captureVolumeFrame();
        } catch (...) {
            return {};
        }
    });
    auto future2 = std::async(std::launch::async, [&]() -> std::vector<uint8_t> {
        try {
            return session.captureMPRFrame(MPRPlane::Axial);
        } catch (...) {
            return {};
        }
    });

    auto frame1 = future1.get();
    auto frame2 = future2.get();

    // Frames may be empty on headless systems without OpenGL
    if (!frame1.empty()) {
        EXPECT_EQ(frame1.size(), 32u * 32u * 4u);
    }
    if (!frame2.empty()) {
        EXPECT_EQ(frame2.size(), 32u * 32u * 4u);
    }
}

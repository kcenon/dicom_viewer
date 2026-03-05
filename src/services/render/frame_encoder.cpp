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

#include "services/render/frame_encoder.hpp"

#include <vtkImageData.h>
#include <vtkJPEGWriter.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cstring>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class FrameEncoder::Impl {
public:
    // Create a vtkImageData from raw RGBA buffer (4 components)
    static vtkSmartPointer<vtkImageData> createImageRGBA(
        const uint8_t* rgba, uint32_t width, uint32_t height)
    {
        auto image = vtkSmartPointer<vtkImageData>::New();
        image->SetDimensions(
            static_cast<int>(width), static_cast<int>(height), 1);
        image->AllocateScalars(VTK_UNSIGNED_CHAR, 4);

        auto* dst = static_cast<uint8_t*>(image->GetScalarPointer());
        std::memcpy(dst, rgba,
                     static_cast<size_t>(width) * height * 4);

        return image;
    }

    // Create a vtkImageData from raw RGBA buffer, converting to RGB (3 components)
    static vtkSmartPointer<vtkImageData> createImageRGB(
        const uint8_t* rgba, uint32_t width, uint32_t height)
    {
        auto image = vtkSmartPointer<vtkImageData>::New();
        image->SetDimensions(
            static_cast<int>(width), static_cast<int>(height), 1);
        image->AllocateScalars(VTK_UNSIGNED_CHAR, 3);

        auto* dst = static_cast<uint8_t*>(image->GetScalarPointer());
        const size_t pixelCount = static_cast<size_t>(width) * height;

        for (size_t i = 0; i < pixelCount; ++i) {
            dst[i * 3 + 0] = rgba[i * 4 + 0];
            dst[i * 3 + 1] = rgba[i * 4 + 1];
            dst[i * 3 + 2] = rgba[i * 4 + 2];
        }

        return image;
    }

    // Extract bytes from a VTK writer result array
    static std::vector<uint8_t> extractResult(vtkUnsignedCharArray* result)
    {
        if (!result || result->GetNumberOfTuples() == 0) {
            return {};
        }

        auto* ptr = static_cast<uint8_t*>(result->GetVoidPointer(0));
        auto  len = static_cast<size_t>(
            result->GetNumberOfTuples() * result->GetNumberOfComponents());

        return {ptr, ptr + len};
    }
};

// ---------------------------------------------------------------------------
// FrameEncoder lifecycle
// ---------------------------------------------------------------------------
FrameEncoder::FrameEncoder()
    : impl_(std::make_unique<Impl>())
{
}

FrameEncoder::~FrameEncoder() = default;

FrameEncoder::FrameEncoder(FrameEncoder&&) noexcept = default;
FrameEncoder& FrameEncoder::operator=(FrameEncoder&&) noexcept = default;

// ---------------------------------------------------------------------------
// JPEG encoding
// ---------------------------------------------------------------------------
std::vector<uint8_t> FrameEncoder::encodeJpeg(
    const uint8_t* rgba, uint32_t width, uint32_t height, int quality)
{
    if (!rgba || width == 0 || height == 0) {
        return {};
    }

    quality = std::clamp(quality, 1, 100);

    // JPEG does not support alpha — convert RGBA to RGB
    auto image = Impl::createImageRGB(rgba, width, height);

    vtkNew<vtkJPEGWriter> writer;
    writer->WriteToMemoryOn();
    writer->SetInputData(image);
    writer->SetQuality(quality);
    writer->Write();

    return Impl::extractResult(writer->GetResult());
}

// ---------------------------------------------------------------------------
// PNG encoding
// ---------------------------------------------------------------------------
std::vector<uint8_t> FrameEncoder::encodePng(
    const uint8_t* rgba, uint32_t width, uint32_t height)
{
    if (!rgba || width == 0 || height == 0) {
        return {};
    }

    // PNG supports RGBA natively
    auto image = Impl::createImageRGBA(rgba, width, height);

    vtkNew<vtkPNGWriter> writer;
    writer->WriteToMemoryOn();
    writer->SetInputData(image);
    writer->Write();

    return Impl::extractResult(writer->GetResult());
}

// ---------------------------------------------------------------------------
// Format dispatch
// ---------------------------------------------------------------------------
std::vector<uint8_t> FrameEncoder::encode(
    const uint8_t* rgba, uint32_t width, uint32_t height,
    EncodeFormat format, int quality)
{
    switch (format) {
    case EncodeFormat::Jpeg:
        return encodeJpeg(rgba, width, height, quality);
    case EncodeFormat::Png:
        return encodePng(rgba, width, height);
    case EncodeFormat::H264Stream:
        // H.264 requires ffmpeg — not yet linked
        return {};
    }
    return {};
}

// ---------------------------------------------------------------------------
// H.264 availability
// ---------------------------------------------------------------------------
bool FrameEncoder::isH264Available()
{
    return false;
}

} // namespace dicom_viewer::services

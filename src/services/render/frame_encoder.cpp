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
#include "services/render/dirty_region_tracker.hpp"

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
// Delta frame encoding
// ---------------------------------------------------------------------------
DeltaFrame FrameEncoder::encodeDelta(
    const uint8_t* current, const uint8_t* previous,
    uint32_t width, uint32_t height, int quality)
{
    DeltaFrame result;

    if (!current || !previous || width == 0 || height == 0) {
        return result;
    }

    quality = std::clamp(quality, 1, 100);

    DirtyRegionTracker tracker;
    auto detection = tracker.detect(current, previous, width, height);

    result.dirtyRatio = detection.dirtyRatio;
    result.fullFrame = detection.fullFrameRequired;

    if (detection.regions.empty()) {
        return result;
    }

    if (detection.fullFrameRequired) {
        // Encode the entire frame as a single tile
        EncodedTile tile;
        tile.x = 0;
        tile.y = 0;
        tile.width = static_cast<uint16_t>(std::min(width, uint32_t(UINT16_MAX)));
        tile.height = static_cast<uint16_t>(std::min(height, uint32_t(UINT16_MAX)));
        tile.jpegData = encodeJpeg(current, width, height, quality);
        if (!tile.jpegData.empty()) {
            result.tiles.push_back(std::move(tile));
        }
        return result;
    }

    // Encode each dirty region as a JPEG tile
    for (const auto& rect : detection.regions) {
        auto tileRgba = DirtyRegionTracker::extractRegion(
            current, width, height, rect);
        if (tileRgba.empty()) {
            continue;
        }

        EncodedTile tile;
        tile.x = static_cast<uint16_t>(rect.x);
        tile.y = static_cast<uint16_t>(rect.y);
        tile.width = static_cast<uint16_t>(rect.width);
        tile.height = static_cast<uint16_t>(rect.height);
        tile.jpegData = encodeJpeg(
            tileRgba.data(), rect.width, rect.height, quality);

        if (!tile.jpegData.empty()) {
            result.tiles.push_back(std::move(tile));
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Delta serialization
// ---------------------------------------------------------------------------
std::vector<uint8_t> FrameEncoder::serializeDelta(const DeltaFrame& delta)
{
    // Header: [1B flags][4B tile_count]
    // Per tile: [2B x][2B y][2B w][2B h][4B jpeg_size][N bytes JPEG]

    // Calculate total size
    size_t totalSize = 1 + 4; // flags + tile_count
    for (const auto& tile : delta.tiles) {
        totalSize += 2 + 2 + 2 + 2 + 4 + tile.jpegData.size();
    }

    std::vector<uint8_t> data(totalSize);
    size_t offset = 0;

    // Flags byte: bit 0 = fullFrame
    uint8_t flags = delta.fullFrame ? 0x01 : 0x00;
    data[offset++] = flags;

    // Tile count (4 bytes, little-endian)
    uint32_t tileCount = static_cast<uint32_t>(delta.tiles.size());
    std::memcpy(data.data() + offset, &tileCount, 4);
    offset += 4;

    // Tiles
    for (const auto& tile : delta.tiles) {
        std::memcpy(data.data() + offset, &tile.x, 2);
        offset += 2;
        std::memcpy(data.data() + offset, &tile.y, 2);
        offset += 2;
        std::memcpy(data.data() + offset, &tile.width, 2);
        offset += 2;
        std::memcpy(data.data() + offset, &tile.height, 2);
        offset += 2;

        uint32_t jpegSize = static_cast<uint32_t>(tile.jpegData.size());
        std::memcpy(data.data() + offset, &jpegSize, 4);
        offset += 4;

        std::memcpy(data.data() + offset, tile.jpegData.data(), jpegSize);
        offset += jpegSize;
    }

    return data;
}

DeltaFrame FrameEncoder::deserializeDelta(const std::vector<uint8_t>& data)
{
    DeltaFrame result;

    // Minimum: 1B flags + 4B tile_count
    if (data.size() < 5) {
        return result;
    }

    size_t offset = 0;

    uint8_t flags = data[offset++];
    result.fullFrame = (flags & 0x01) != 0;

    uint32_t tileCount = 0;
    std::memcpy(&tileCount, data.data() + offset, 4);
    offset += 4;

    for (uint32_t i = 0; i < tileCount; ++i) {
        if (offset + 12 > data.size()) {
            break; // Not enough data for tile header
        }

        EncodedTile tile;
        std::memcpy(&tile.x, data.data() + offset, 2);
        offset += 2;
        std::memcpy(&tile.y, data.data() + offset, 2);
        offset += 2;
        std::memcpy(&tile.width, data.data() + offset, 2);
        offset += 2;
        std::memcpy(&tile.height, data.data() + offset, 2);
        offset += 2;

        uint32_t jpegSize = 0;
        std::memcpy(&jpegSize, data.data() + offset, 4);
        offset += 4;

        if (offset + jpegSize > data.size()) {
            break; // Not enough data for JPEG payload
        }

        tile.jpegData.assign(
            data.data() + offset, data.data() + offset + jpegSize);
        offset += jpegSize;

        result.tiles.push_back(std::move(tile));
    }

    return result;
}

// ---------------------------------------------------------------------------
// H.264 availability
// ---------------------------------------------------------------------------
bool FrameEncoder::isH264Available()
{
    return false;
}

} // namespace dicom_viewer::services

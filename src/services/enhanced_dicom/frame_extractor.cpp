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

#include "services/enhanced_dicom/frame_extractor.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <numeric>

#include <gdcmImageReader.h>
#include <gdcmImage.h>
#include <gdcmPixmapReader.h>

#include <itkImage.h>
#include <itkImportImageFilter.h>

namespace dicom_viewer::services {

class FrameExtractor::Impl {
public:
};

FrameExtractor::FrameExtractor() : impl_(std::make_unique<Impl>()) {}

FrameExtractor::~FrameExtractor() = default;

FrameExtractor::FrameExtractor(FrameExtractor&&) noexcept = default;
FrameExtractor& FrameExtractor::operator=(FrameExtractor&&) noexcept = default;

std::expected<std::vector<char>, EnhancedDicomError>
FrameExtractor::extractFrame(const std::string& filePath,
                             int frameIndex,
                             const EnhancedSeriesInfo& info)
{
    LOG_DEBUG(std::format("Extracting frame {} from {}", frameIndex, filePath));

    if (frameIndex < 0 || frameIndex >= info.numberOfFrames) {
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::InvalidInput,
            "Frame index " + std::to_string(frameIndex) + " out of range [0, "
                + std::to_string(info.numberOfFrames) + ")"
        });
    }

    try {
        gdcm::ImageReader reader;
        reader.SetFileName(filePath.c_str());
        if (!reader.Read()) {
            return std::unexpected(EnhancedDicomError{
                EnhancedDicomError::Code::ParseFailed,
                "Failed to read image data from: " + filePath
            });
        }

        const auto& image = reader.GetImage();
        size_t totalBytes = image.GetBufferLength();
        size_t bytesPerFrame = totalBytes / info.numberOfFrames;

        std::vector<char> fullBuffer(totalBytes);
        if (!image.GetBuffer(fullBuffer.data())) {
            return std::unexpected(EnhancedDicomError{
                EnhancedDicomError::Code::FrameExtractionFailed,
                "Failed to decode pixel data"
            });
        }

        // Extract the requested frame
        size_t offset = static_cast<size_t>(frameIndex) * bytesPerFrame;
        std::vector<char> frameBuffer(
            fullBuffer.begin() + static_cast<ptrdiff_t>(offset),
            fullBuffer.begin() + static_cast<ptrdiff_t>(offset + bytesPerFrame));

        return frameBuffer;

    } catch (const std::exception& e) {
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::FrameExtractionFailed,
            std::string("Exception extracting frame: ") + e.what()
        });
    }
}

namespace {

/// Sort frame indices by spatial position along the slice normal
std::vector<int> sortFramesBySpatialPosition(
    const EnhancedSeriesInfo& info,
    const std::vector<int>& frameIndices)
{
    if (frameIndices.empty()) {
        return {};
    }

    // Calculate slice normal from first frame's orientation
    const auto& orient = info.frames[frameIndices[0]].imageOrientation;
    std::array<double, 3> normal = {
        orient[1] * orient[5] - orient[2] * orient[4],
        orient[2] * orient[3] - orient[0] * orient[5],
        orient[0] * orient[4] - orient[1] * orient[3]
    };

    // Sort by projection onto slice normal
    auto sorted = frameIndices;
    std::sort(sorted.begin(), sorted.end(),
        [&info, &normal](int a, int b) {
            const auto& posA = info.frames[a].imagePosition;
            const auto& posB = info.frames[b].imagePosition;
            double projA = posA[0] * normal[0] + posA[1] * normal[1]
                         + posA[2] * normal[2];
            double projB = posB[0] * normal[0] + posB[1] * normal[1]
                         + posB[2] * normal[2];
            return projA < projB;
        });

    return sorted;
}

}  // anonymous namespace

std::expected<itk::Image<short, 3>::Pointer, EnhancedDicomError>
FrameExtractor::assembleVolume(const std::string& filePath,
                               const EnhancedSeriesInfo& info)
{
    // Assemble all frames
    std::vector<int> allIndices(info.numberOfFrames);
    std::iota(allIndices.begin(), allIndices.end(), 0);
    return assembleVolumeFromFrames(filePath, info, allIndices);
}

std::expected<itk::Image<short, 3>::Pointer, EnhancedDicomError>
FrameExtractor::assembleVolumeFromFrames(
    const std::string& filePath,
    const EnhancedSeriesInfo& info,
    const std::vector<int>& frameIndices)
{
    LOG_INFO(std::format("Assembling volume from {} frames", frameIndices.size()));

    if (frameIndices.empty()) {
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::InvalidInput,
            "No frame indices provided for volume assembly"
        });
    }

    try {
        // Read all pixel data at once using GDCM ImageReader
        gdcm::ImageReader reader;
        reader.SetFileName(filePath.c_str());
        if (!reader.Read()) {
            return std::unexpected(EnhancedDicomError{
                EnhancedDicomError::Code::ParseFailed,
                "Failed to read image data from: " + filePath
            });
        }

        const auto& image = reader.GetImage();
        size_t totalBytes = image.GetBufferLength();
        size_t bytesPerFrame =
            static_cast<size_t>(info.rows) * info.columns
            * (info.bitsAllocated / 8);

        std::vector<char> fullBuffer(totalBytes);
        if (!image.GetBuffer(fullBuffer.data())) {
            return std::unexpected(EnhancedDicomError{
                EnhancedDicomError::Code::FrameExtractionFailed,
                "Failed to decode pixel data"
            });
        }

        // Sort frames by spatial position
        auto sortedIndices = sortFramesBySpatialPosition(info, frameIndices);
        int sliceCount = static_cast<int>(sortedIndices.size());

        // Create ITK image via ImportImageFilter
        using ImportFilterType = itk::ImportImageFilter<short, 3>;
        auto importFilter = ImportFilterType::New();

        ImportFilterType::SizeType itkSize;
        itkSize[0] = info.columns;
        itkSize[1] = info.rows;
        itkSize[2] = sliceCount;

        ImportFilterType::IndexType start;
        start.Fill(0);

        ImportFilterType::RegionType region;
        region.SetIndex(start);
        region.SetSize(itkSize);
        importFilter->SetRegion(region);

        // Set origin from first frame
        const auto& firstFrame = info.frames[sortedIndices[0]];
        double origin[3] = {
            firstFrame.imagePosition[0],
            firstFrame.imagePosition[1],
            firstFrame.imagePosition[2]
        };
        importFilter->SetOrigin(origin);

        // Set spacing
        double spacing[3] = {info.pixelSpacingX, info.pixelSpacingY, 0.0};

        // Calculate Z spacing from frame positions
        if (sliceCount >= 2) {
            const auto& orient = firstFrame.imageOrientation;
            std::array<double, 3> normal = {
                orient[1] * orient[5] - orient[2] * orient[4],
                orient[2] * orient[3] - orient[0] * orient[5],
                orient[0] * orient[4] - orient[1] * orient[3]
            };

            const auto& pos0 = info.frames[sortedIndices[0]].imagePosition;
            const auto& pos1 = info.frames[sortedIndices[1]].imagePosition;
            double zDist = std::abs(
                (pos1[0] - pos0[0]) * normal[0]
                + (pos1[1] - pos0[1]) * normal[1]
                + (pos1[2] - pos0[2]) * normal[2]);
            spacing[2] = (zDist > 0.001) ? zDist : firstFrame.sliceThickness;
        } else {
            spacing[2] = firstFrame.sliceThickness;
        }
        importFilter->SetSpacing(spacing);

        // Set direction cosines
        ImportFilterType::DirectionType direction;
        direction.SetIdentity();
        direction[0][0] = firstFrame.imageOrientation[0];
        direction[1][0] = firstFrame.imageOrientation[1];
        direction[2][0] = firstFrame.imageOrientation[2];
        direction[0][1] = firstFrame.imageOrientation[3];
        direction[1][1] = firstFrame.imageOrientation[4];
        direction[2][1] = firstFrame.imageOrientation[5];
        // Z direction = cross product of row and column
        direction[0][2] = firstFrame.imageOrientation[1]
                        * firstFrame.imageOrientation[5]
                        - firstFrame.imageOrientation[2]
                        * firstFrame.imageOrientation[4];
        direction[1][2] = firstFrame.imageOrientation[2]
                        * firstFrame.imageOrientation[3]
                        - firstFrame.imageOrientation[0]
                        * firstFrame.imageOrientation[5];
        direction[2][2] = firstFrame.imageOrientation[0]
                        * firstFrame.imageOrientation[4]
                        - firstFrame.imageOrientation[1]
                        * firstFrame.imageOrientation[3];
        importFilter->SetDirection(direction);

        // Allocate output buffer and copy frame data
        size_t pixelsPerSlice =
            static_cast<size_t>(info.rows) * info.columns;
        size_t totalPixels = pixelsPerSlice * sliceCount;
        auto* outputBuffer = new short[totalPixels];

        bool isSigned = (info.pixelRepresentation == 1);
        int bytesPerPixel = info.bitsAllocated / 8;

        for (int sliceIdx = 0; sliceIdx < sliceCount; ++sliceIdx) {
            int frameIdx = sortedIndices[sliceIdx];
            const auto& frame = info.frames[frameIdx];

            size_t srcOffset =
                static_cast<size_t>(frameIdx) * bytesPerFrame;
            size_t dstOffset = static_cast<size_t>(sliceIdx) * pixelsPerSlice;

            const char* srcPtr = fullBuffer.data() + srcOffset;

            for (size_t px = 0; px < pixelsPerSlice; ++px) {
                short rawValue = 0;
                if (bytesPerPixel == 2) {
                    if (isSigned) {
                        std::memcpy(&rawValue, srcPtr + px * 2, 2);
                    } else {
                        uint16_t uval = 0;
                        std::memcpy(&uval, srcPtr + px * 2, 2);
                        rawValue = static_cast<short>(uval);
                    }
                } else if (bytesPerPixel == 1) {
                    if (isSigned) {
                        rawValue = static_cast<short>(
                            static_cast<int8_t>(srcPtr[px]));
                    } else {
                        rawValue = static_cast<short>(
                            static_cast<uint8_t>(srcPtr[px]));
                    }
                }

                // Apply per-frame rescale: HU = slope * raw + intercept
                double hu = frame.rescaleSlope * rawValue
                          + frame.rescaleIntercept;
                outputBuffer[dstOffset + px] =
                    static_cast<short>(std::clamp(hu, -32768.0, 32767.0));
            }
        }

        const bool letITKManageMemory = true;
        importFilter->SetImportPointer(outputBuffer, totalPixels,
                                       letITKManageMemory);
        importFilter->Update();

        auto output = importFilter->GetOutput();
        auto size = output->GetLargestPossibleRegion().GetSize();
        LOG_INFO(std::format("Volume assembled: {}x{}x{}", size[0], size[1],
                           size[2]));

        return output;

    } catch (const std::exception& e) {
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::FrameExtractionFailed,
            std::string("Exception assembling volume: ") + e.what()
        });
    }
}

}  // namespace dicom_viewer::services

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

#include "services/export/video_exporter.hpp"

#include <cmath>
#include <format>

#include <vtkNew.h>
#include <vtkOggTheoraWriter.h>
#include <vtkRenderWindow.h>
#include <vtkWindowToImageFilter.h>

namespace dicom_viewer::services {

// =========================================================================
// Pimpl
// =========================================================================

class VideoExporter::Impl {
public:
    ProgressCallback progressCallback;

    void reportProgress(double progress, const std::string& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
    }
};

VideoExporter::VideoExporter()
    : impl_(std::make_unique<Impl>()) {}

VideoExporter::~VideoExporter() = default;

VideoExporter::VideoExporter(VideoExporter&&) noexcept = default;
VideoExporter& VideoExporter::operator=(VideoExporter&&) noexcept = default;

void VideoExporter::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

// =========================================================================
// Validation
// =========================================================================

std::expected<void, ExportError>
VideoExporter::validateCineConfig(const CineConfig& config) {
    if (config.outputPath.empty()) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Output path is empty"});
    }

    if (config.totalPhases < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Total phases must be >= 1, got {}",
                        config.totalPhases)});
    }

    if (config.startPhase < 0 || config.startPhase >= config.totalPhases) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Start phase {} out of range [0, {})",
                        config.startPhase, config.totalPhases)});
    }

    const int endPhase =
        (config.endPhase < 0) ? config.totalPhases - 1 : config.endPhase;

    if (endPhase < config.startPhase || endPhase >= config.totalPhases) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("End phase {} out of range [{}, {})",
                        endPhase, config.startPhase, config.totalPhases)});
    }

    if (config.width < 1 || config.height < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Invalid resolution {}x{}",
                        config.width, config.height)});
    }

    if (config.fps < 1 || config.fps > 120) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("FPS must be in [1, 120], got {}", config.fps)});
    }

    if (config.loops < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Loops must be >= 1, got {}", config.loops)});
    }

    if (config.framesPerPhase < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Frames per phase must be >= 1, got {}",
                        config.framesPerPhase)});
    }

    return {};
}

std::expected<void, ExportError>
VideoExporter::validateRotationConfig(const RotationConfig& config) {
    if (config.outputPath.empty()) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Output path is empty"});
    }

    if (config.width < 1 || config.height < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Invalid resolution {}x{}",
                        config.width, config.height)});
    }

    if (config.fps < 1 || config.fps > 120) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("FPS must be in [1, 120], got {}", config.fps)});
    }

    if (std::abs(config.endAngle - config.startAngle) < 0.01) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Angle range is effectively zero"});
    }

    if (config.elevation < -90.0 || config.elevation > 90.0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Elevation must be in [-90, 90], got {}",
                        config.elevation)});
    }

    if (config.totalFrames < 2) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Total frames must be >= 2, got {}",
                        config.totalFrames)});
    }

    return {};
}

std::expected<void, ExportError>
VideoExporter::validateCombinedConfig(const CombinedConfig& config) {
    if (config.outputPath.empty()) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Output path is empty"});
    }

    if (config.width < 1 || config.height < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Invalid resolution {}x{}",
                        config.width, config.height)});
    }

    if (config.fps < 1 || config.fps > 120) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("FPS must be in [1, 120], got {}", config.fps)});
    }

    if (std::abs(config.endAngle - config.startAngle) < 0.01) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Angle range is effectively zero"});
    }

    if (config.elevation < -90.0 || config.elevation > 90.0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Elevation must be in [-90, 90], got {}",
                        config.elevation)});
    }

    if (config.totalPhases < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Total phases must be >= 1, got {}",
                        config.totalPhases)});
    }

    if (config.phaseLoops < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Phase loops must be >= 1, got {}",
                        config.phaseLoops)});
    }

    if (config.framesPerPhase < 1) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            std::format("Frames per phase must be >= 1, got {}",
                        config.framesPerPhase)});
    }

    return {};
}

// =========================================================================
// 2D Cine Export
// =========================================================================

std::expected<void, ExportError>
VideoExporter::exportCine2D(
    vtkRenderWindow* renderWindow,
    const CineConfig& config,
    PhaseCallback setPhase) const {

    if (!renderWindow) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Render window is null"});
    }

    if (!setPhase) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Phase callback is null"});
    }

    auto validation = validateCineConfig(config);
    if (!validation) {
        return validation;
    }

    const int endPhase =
        (config.endPhase < 0) ? config.totalPhases - 1 : config.endPhase;
    const int phaseCount = endPhase - config.startPhase + 1;
    const int totalFrames = phaseCount * config.loops * config.framesPerPhase;

    // Resize render window to requested resolution
    renderWindow->SetSize(config.width, config.height);

    // Set up frame capture filter
    vtkNew<vtkWindowToImageFilter> windowToImage;
    windowToImage->SetInput(renderWindow);
    windowToImage->SetInputBufferTypeToRGB();
    windowToImage->ReadFrontBufferOff();

    // Set up OGG Theora writer
    vtkNew<vtkOggTheoraWriter> writer;
    writer->SetInputConnection(windowToImage->GetOutputPort());
    writer->SetFileName(config.outputPath.string().c_str());
    writer->SetRate(config.fps);

    impl_->reportProgress(0.0, "Starting video export");

    writer->Start();
    if (writer->GetError()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            std::format("Failed to open video file: {}",
                        config.outputPath.string())});
    }

    int frameIndex = 0;

    try {
        for (int loop = 0; loop < config.loops; ++loop) {
            for (int phase = config.startPhase; phase <= endPhase; ++phase) {
                setPhase(phase);
                renderWindow->Render();

                windowToImage->Modified();
                windowToImage->Update();

                for (int f = 0; f < config.framesPerPhase; ++f) {
                    writer->Write();
                    if (writer->GetError()) {
                        writer->End();
                        return std::unexpected(ExportError{
                            ExportError::Code::EncodingFailed,
                            std::format("Failed writing frame {} (phase {})",
                                        frameIndex, phase)});
                    }
                    ++frameIndex;
                }

                double progress =
                    static_cast<double>(frameIndex) / totalFrames;
                impl_->reportProgress(
                    progress,
                    std::format("Encoding phase {}/{}", phase + 1,
                                config.totalPhases));
            }
        }
    } catch (const std::exception& e) {
        writer->End();
        return std::unexpected(ExportError{
            ExportError::Code::InternalError,
            std::format("Exception during video export: {}", e.what())});
    }

    writer->End();

    impl_->reportProgress(1.0, "Video export complete");

    return {};
}

// =========================================================================
// 3D Rotation Export
// =========================================================================

std::expected<void, ExportError>
VideoExporter::exportRotation3D(
    vtkRenderWindow* renderWindow,
    const RotationConfig& config,
    CameraCallback setCamera) const {

    if (!renderWindow) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Render window is null"});
    }

    if (!setCamera) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Camera callback is null"});
    }

    auto validation = validateRotationConfig(config);
    if (!validation) {
        return validation;
    }

    const double angleRange = config.endAngle - config.startAngle;
    const double angleStep = angleRange / (config.totalFrames - 1);

    renderWindow->SetSize(config.width, config.height);

    vtkNew<vtkWindowToImageFilter> windowToImage;
    windowToImage->SetInput(renderWindow);
    windowToImage->SetInputBufferTypeToRGB();
    windowToImage->ReadFrontBufferOff();

    vtkNew<vtkOggTheoraWriter> writer;
    writer->SetInputConnection(windowToImage->GetOutputPort());
    writer->SetFileName(config.outputPath.string().c_str());
    writer->SetRate(config.fps);

    impl_->reportProgress(0.0, "Starting 3D rotation export");

    writer->Start();
    if (writer->GetError()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            std::format("Failed to open video file: {}",
                        config.outputPath.string())});
    }

    try {
        for (int frame = 0; frame < config.totalFrames; ++frame) {
            double azimuth = config.startAngle + angleStep * frame;
            setCamera(azimuth, config.elevation);
            renderWindow->Render();

            windowToImage->Modified();
            windowToImage->Update();

            writer->Write();
            if (writer->GetError()) {
                writer->End();
                return std::unexpected(ExportError{
                    ExportError::Code::EncodingFailed,
                    std::format("Failed writing frame {} (azimuth {:.1f}°)",
                                frame, azimuth)});
            }

            double progress =
                static_cast<double>(frame + 1) / config.totalFrames;
            impl_->reportProgress(
                progress,
                std::format("Rotation {:.0f}°/{:.0f}°",
                            azimuth, config.endAngle));
        }
    } catch (const std::exception& e) {
        writer->End();
        return std::unexpected(ExportError{
            ExportError::Code::InternalError,
            std::format("Exception during rotation export: {}", e.what())});
    }

    writer->End();

    impl_->reportProgress(1.0, "3D rotation export complete");

    return {};
}

// =========================================================================
// Combined Rotation + Phase Export
// =========================================================================

std::expected<void, ExportError>
VideoExporter::exportCombined3D(
    vtkRenderWindow* renderWindow,
    const CombinedConfig& config,
    PhaseCallback setPhase,
    CameraCallback setCamera) const {

    if (!renderWindow) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Render window is null"});
    }

    if (!setPhase) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Phase callback is null"});
    }

    if (!setCamera) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Camera callback is null"});
    }

    auto validation = validateCombinedConfig(config);
    if (!validation) {
        return validation;
    }

    const int totalFrames =
        config.totalPhases * config.phaseLoops * config.framesPerPhase;
    const double angleRange = config.endAngle - config.startAngle;
    const double angleStep = angleRange / std::max(totalFrames - 1, 1);

    renderWindow->SetSize(config.width, config.height);

    vtkNew<vtkWindowToImageFilter> windowToImage;
    windowToImage->SetInput(renderWindow);
    windowToImage->SetInputBufferTypeToRGB();
    windowToImage->ReadFrontBufferOff();

    vtkNew<vtkOggTheoraWriter> writer;
    writer->SetInputConnection(windowToImage->GetOutputPort());
    writer->SetFileName(config.outputPath.string().c_str());
    writer->SetRate(config.fps);

    impl_->reportProgress(0.0, "Starting combined rotation+phase export");

    writer->Start();
    if (writer->GetError()) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            std::format("Failed to open video file: {}",
                        config.outputPath.string())});
    }

    int frameIndex = 0;

    try {
        for (int loop = 0; loop < config.phaseLoops; ++loop) {
            for (int phase = 0; phase < config.totalPhases; ++phase) {
                setPhase(phase);

                for (int f = 0; f < config.framesPerPhase; ++f) {
                    double azimuth =
                        config.startAngle + angleStep * frameIndex;
                    setCamera(azimuth, config.elevation);
                    renderWindow->Render();

                    windowToImage->Modified();
                    windowToImage->Update();

                    writer->Write();
                    if (writer->GetError()) {
                        writer->End();
                        return std::unexpected(ExportError{
                            ExportError::Code::EncodingFailed,
                            std::format(
                                "Failed writing frame {} (phase {}, "
                                "azimuth {:.1f}°)",
                                frameIndex, phase, azimuth)});
                    }
                    ++frameIndex;
                }

                double progress =
                    static_cast<double>(frameIndex) / totalFrames;
                impl_->reportProgress(
                    progress,
                    std::format("Phase {}/{}, rotation {:.0f}°",
                                phase + 1, config.totalPhases,
                                config.startAngle + angleStep * frameIndex));
            }
        }
    } catch (const std::exception& e) {
        writer->End();
        return std::unexpected(ExportError{
            ExportError::Code::InternalError,
            std::format("Exception during combined export: {}", e.what())});
    }

    writer->End();

    impl_->reportProgress(1.0, "Combined export complete");

    return {};
}

}  // namespace dicom_viewer::services

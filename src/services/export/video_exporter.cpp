#include "services/export/video_exporter.hpp"

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

}  // namespace dicom_viewer::services

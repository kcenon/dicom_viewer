#pragma once

#include "services/export/data_exporter.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

class vtkRenderWindow;

namespace dicom_viewer::services {

/**
 * @brief Video exporter for cine playback and 3D rotation animations
 *
 * Captures render window frames at each cardiac phase and encodes
 * them as OGG Theora video. Supports 2D cine phase animation and
 * 3D rotation capture.
 *
 * @example
 * @code
 * VideoExporter exporter;
 * exporter.setProgressCallback([](double p, const std::string& s) {
 *     std::cout << s << ": " << int(p * 100) << "%\n";
 * });
 *
 * VideoExporter::CineConfig config;
 * config.outputPath = "/tmp/cine.ogv";
 * config.totalPhases = 20;
 * config.fps = 15;
 *
 * auto result = exporter.exportCine2D(renderWindow, config,
 *     [&](int phase) { viewer->setPhase(phase); });
 * @endcode
 *
 * @trace SRS-FR-046
 */
class VideoExporter {
public:
    using ProgressCallback =
        std::function<void(double progress, const std::string& status)>;

    /// Callback to advance the viewer to a specific cardiac phase
    using PhaseCallback = std::function<void(int phase)>;

    /**
     * @brief Configuration for 2D cine phase animation export
     */
    struct CineConfig {
        std::filesystem::path outputPath;  ///< Output file path (.ogv)
        int width = 1920;                  ///< Frame width in pixels
        int height = 1080;                 ///< Frame height in pixels
        int fps = 15;                      ///< Frames per second
        int startPhase = 0;                ///< First phase to capture
        int endPhase = -1;                 ///< Last phase (-1 = totalPhases - 1)
        int totalPhases = 0;               ///< Total number of cardiac phases
        int loops = 1;                     ///< Number of animation loops
        int framesPerPhase = 1;            ///< Frames to hold each phase
    };

    VideoExporter();
    ~VideoExporter();

    VideoExporter(const VideoExporter&) = delete;
    VideoExporter& operator=(const VideoExporter&) = delete;
    VideoExporter(VideoExporter&&) noexcept;
    VideoExporter& operator=(VideoExporter&&) noexcept;

    /**
     * @brief Set progress callback for monitoring export
     * @param callback Function receiving (progress [0-1], status message)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Export 2D cine phase animation as OGG Theora video
     *
     * Iterates through cardiac phases, captures each frame from the
     * render window, and writes the sequence as an OGG Theora video file.
     *
     * @param renderWindow VTK render window to capture frames from
     * @param config Cine export configuration
     * @param setPhase Callback to advance viewer to a specific phase
     * @return void on success, ExportError on failure
     */
    [[nodiscard]] std::expected<void, ExportError>
    exportCine2D(vtkRenderWindow* renderWindow,
                 const CineConfig& config,
                 PhaseCallback setPhase) const;

    // -----------------------------------------------------------------
    // Validation (public for testing)
    // -----------------------------------------------------------------

    /**
     * @brief Validate cine configuration
     * @return Empty error on success, ExportError on invalid config
     */
    [[nodiscard]] static std::expected<void, ExportError>
    validateCineConfig(const CineConfig& config);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

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


/**
 * @file video_exporter.hpp
 * @brief Video exporter for cine playback and 3D rotation animations
 * @details Captures render window frames at cardiac phases and encodes as
 *          OGG Theora video. Supports 2D cine phase animation and 3D
 *          rotation capture with progress callbacks for long encoding
 *          operations.
 *
 * ## Thread Safety
 * - Render window frame capture must be synchronized with rendering
 * - Video encoding is a long-running operation; use background threads
 * - Progress callbacks are invoked from the encoding thread
 *
 * @author kcenon
 * @since 1.0.0
 */
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

    /// Callback to position the 3D camera at a given azimuth and elevation
    using CameraCallback =
        std::function<void(double azimuth, double elevation)>;

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

    /**
     * @brief Configuration for 3D rotation animation export
     */
    struct RotationConfig {
        std::filesystem::path outputPath;  ///< Output file path (.ogv)
        int width = 1920;                  ///< Frame width in pixels
        int height = 1080;                 ///< Frame height in pixels
        int fps = 30;                      ///< Frames per second
        double startAngle = 0.0;           ///< Start azimuth in degrees
        double endAngle = 360.0;           ///< End azimuth in degrees
        double elevation = 15.0;           ///< Camera elevation in degrees
        int totalFrames = 180;             ///< Total frames for the rotation
    };

    /**
     * @brief Configuration for combined rotation + phase animation export
     *
     * Camera rotates through the full angle range while phases cycle
     * simultaneously. Total frames = totalPhases * phaseLoops * framesPerPhase.
     */
    struct CombinedConfig {
        std::filesystem::path outputPath;  ///< Output file path (.ogv)
        int width = 1920;                  ///< Frame width in pixels
        int height = 1080;                 ///< Frame height in pixels
        int fps = 30;                      ///< Frames per second
        double startAngle = 0.0;           ///< Start azimuth in degrees
        double endAngle = 360.0;           ///< End azimuth in degrees
        double elevation = 15.0;           ///< Camera elevation in degrees
        int totalPhases = 0;               ///< Total cardiac phases
        int phaseLoops = 1;                ///< Phase cycles per rotation
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

    /**
     * @brief Export 3D rotation animation as OGG Theora video
     *
     * Orbits the camera around the scene from startAngle to endAngle
     * at a fixed elevation, capturing each frame.
     *
     * @param renderWindow VTK render window to capture frames from
     * @param config Rotation export configuration
     * @param setCamera Callback to position camera at (azimuth, elevation)
     * @return void on success, ExportError on failure
     */
    [[nodiscard]] std::expected<void, ExportError>
    exportRotation3D(vtkRenderWindow* renderWindow,
                     const RotationConfig& config,
                     CameraCallback setCamera) const;

    /**
     * @brief Export combined rotation + phase animation as OGG Theora video
     *
     * Camera orbits while cardiac phases cycle simultaneously.
     * Angle interpolation is distributed evenly across all frames.
     *
     * @param renderWindow VTK render window to capture frames from
     * @param config Combined export configuration
     * @param setPhase Callback to advance viewer to a specific phase
     * @param setCamera Callback to position camera at (azimuth, elevation)
     * @return void on success, ExportError on failure
     */
    [[nodiscard]] std::expected<void, ExportError>
    exportCombined3D(vtkRenderWindow* renderWindow,
                     const CombinedConfig& config,
                     PhaseCallback setPhase,
                     CameraCallback setCamera) const;

    // -----------------------------------------------------------------
    // Validation (public for testing)
    // -----------------------------------------------------------------

    [[nodiscard]] static std::expected<void, ExportError>
    validateCineConfig(const CineConfig& config);

    [[nodiscard]] static std::expected<void, ExportError>
    validateRotationConfig(const RotationConfig& config);

    [[nodiscard]] static std::expected<void, ExportError>
    validateCombinedConfig(const CombinedConfig& config);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

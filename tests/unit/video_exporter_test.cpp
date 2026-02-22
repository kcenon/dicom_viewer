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

#include <filesystem>

#include <gtest/gtest.h>

using namespace dicom_viewer::services;

// =============================================================================
// Config validation tests
// =============================================================================

TEST(VideoExporterTest, ValidCineConfigPasses) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 20;
    config.fps = 15;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_TRUE(result.has_value());
}

TEST(VideoExporterTest, EmptyOutputPathFails) {
    VideoExporter::CineConfig config;
    config.totalPhases = 10;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, ZeroTotalPhasesFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 0;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, NegativeTotalPhasesFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = -5;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, StartPhaseOutOfRangeFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.startPhase = 15;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, NegativeStartPhaseFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.startPhase = -1;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, EndPhaseBeforeStartFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.startPhase = 5;
    config.endPhase = 3;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, EndPhaseOutOfRangeFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.endPhase = 20;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, DefaultEndPhaseAllowed) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.endPhase = -1;  // default = all phases

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_TRUE(result.has_value());
}

TEST(VideoExporterTest, ZeroWidthFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.width = 0;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, NegativeHeightFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.height = -100;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, FPSTooHighFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.fps = 200;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, ZeroFPSFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.fps = 0;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, ZeroLoopsFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.loops = 0;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, ZeroFramesPerPhaseFails) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.framesPerPhase = 0;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, CustomPhaseRangePasses) {
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 20;
    config.startPhase = 5;
    config.endPhase = 15;
    config.loops = 2;
    config.framesPerPhase = 3;

    auto result = VideoExporter::validateCineConfig(config);
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// RotationConfig validation tests
// =============================================================================

TEST(VideoExporterTest, ValidRotationConfigPasses) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_TRUE(result.has_value());
}

TEST(VideoExporterTest, RotationEmptyPathFails) {
    VideoExporter::RotationConfig config;
    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, RotationZeroAngleRangeFails) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.startAngle = 90.0;
    config.endAngle = 90.0;

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, RotationElevationTooHighFails) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.elevation = 100.0;

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, RotationElevationTooLowFails) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.elevation = -95.0;

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, RotationOneFrameFails) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalFrames = 1;

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, RotationInvalidResolutionFails) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.width = 0;

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, RotationInvalidFPSFails) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.fps = 0;

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, RotationCustomAnglePasses) {
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.startAngle = -45.0;
    config.endAngle = 45.0;
    config.elevation = -30.0;
    config.totalFrames = 90;

    auto result = VideoExporter::validateRotationConfig(config);
    EXPECT_TRUE(result.has_value());
}

TEST(VideoExporterTest, RotationConfigDefaults) {
    VideoExporter::RotationConfig config;
    EXPECT_EQ(config.width, 1920);
    EXPECT_EQ(config.height, 1080);
    EXPECT_EQ(config.fps, 30);
    EXPECT_DOUBLE_EQ(config.startAngle, 0.0);
    EXPECT_DOUBLE_EQ(config.endAngle, 360.0);
    EXPECT_DOUBLE_EQ(config.elevation, 15.0);
    EXPECT_EQ(config.totalFrames, 180);
}

// =============================================================================
// CombinedConfig validation tests
// =============================================================================

TEST(VideoExporterTest, ValidCombinedConfigPasses) {
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 20;

    auto result = VideoExporter::validateCombinedConfig(config);
    EXPECT_TRUE(result.has_value());
}

TEST(VideoExporterTest, CombinedEmptyPathFails) {
    VideoExporter::CombinedConfig config;
    config.totalPhases = 10;

    auto result = VideoExporter::validateCombinedConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, CombinedZeroPhasesFails) {
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 0;

    auto result = VideoExporter::validateCombinedConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, CombinedZeroAngleRangeFails) {
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.startAngle = 180.0;
    config.endAngle = 180.0;

    auto result = VideoExporter::validateCombinedConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, CombinedZeroLoopsFails) {
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.phaseLoops = 0;

    auto result = VideoExporter::validateCombinedConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, CombinedZeroFramesPerPhaseFails) {
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.framesPerPhase = 0;

    auto result = VideoExporter::validateCombinedConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, CombinedInvalidElevationFails) {
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;
    config.elevation = 91.0;

    auto result = VideoExporter::validateCombinedConfig(config);
    EXPECT_FALSE(result.has_value());
}

TEST(VideoExporterTest, CombinedConfigDefaults) {
    VideoExporter::CombinedConfig config;
    EXPECT_EQ(config.width, 1920);
    EXPECT_EQ(config.height, 1080);
    EXPECT_EQ(config.fps, 30);
    EXPECT_DOUBLE_EQ(config.startAngle, 0.0);
    EXPECT_DOUBLE_EQ(config.endAngle, 360.0);
    EXPECT_DOUBLE_EQ(config.elevation, 15.0);
    EXPECT_EQ(config.totalPhases, 0);
    EXPECT_EQ(config.phaseLoops, 1);
    EXPECT_EQ(config.framesPerPhase, 1);
}

// =============================================================================
// Export error handling tests (Cine)
// =============================================================================

TEST(VideoExporterTest, NullRenderWindowReturnsError) {
    VideoExporter exporter;
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;

    auto result = exporter.exportCine2D(nullptr, config,
                                         [](int) {});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, NullPhaseCallbackReturnsError) {
    VideoExporter exporter;
    VideoExporter::CineConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;

    // Pass null callback
    auto result = exporter.exportCine2D(
        reinterpret_cast<vtkRenderWindow*>(0x1), config, nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, InvalidConfigInExportReturnsError) {
    VideoExporter exporter;
    VideoExporter::CineConfig config;
    // Empty output path → invalid config
    config.totalPhases = 10;

    auto result = exporter.exportCine2D(
        reinterpret_cast<vtkRenderWindow*>(0x1), config, [](int) {});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

// =============================================================================
// Export error handling tests (Rotation)
// =============================================================================

TEST(VideoExporterTest, RotationNullRenderWindowReturnsError) {
    VideoExporter exporter;
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";

    auto result = exporter.exportRotation3D(nullptr, config,
                                             [](double, double) {});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, RotationNullCameraCallbackReturnsError) {
    VideoExporter exporter;
    VideoExporter::RotationConfig config;
    config.outputPath = "/tmp/test.ogv";

    auto result = exporter.exportRotation3D(
        reinterpret_cast<vtkRenderWindow*>(0x1), config, nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, RotationInvalidConfigInExportReturnsError) {
    VideoExporter exporter;
    VideoExporter::RotationConfig config;
    // Empty path
    auto result = exporter.exportRotation3D(
        reinterpret_cast<vtkRenderWindow*>(0x1), config,
        [](double, double) {});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

// =============================================================================
// Export error handling tests (Combined)
// =============================================================================

TEST(VideoExporterTest, CombinedNullRenderWindowReturnsError) {
    VideoExporter exporter;
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;

    auto result = exporter.exportCombined3D(nullptr, config,
                                             [](int) {},
                                             [](double, double) {});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, CombinedNullPhaseCallbackReturnsError) {
    VideoExporter exporter;
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;

    auto result = exporter.exportCombined3D(
        reinterpret_cast<vtkRenderWindow*>(0x1), config,
        nullptr, [](double, double) {});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, CombinedNullCameraCallbackReturnsError) {
    VideoExporter exporter;
    VideoExporter::CombinedConfig config;
    config.outputPath = "/tmp/test.ogv";
    config.totalPhases = 10;

    auto result = exporter.exportCombined3D(
        reinterpret_cast<vtkRenderWindow*>(0x1), config,
        [](int) {}, nullptr);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST(VideoExporterTest, CombinedInvalidConfigInExportReturnsError) {
    VideoExporter exporter;
    VideoExporter::CombinedConfig config;
    config.totalPhases = 10;
    // Empty path

    auto result = exporter.exportCombined3D(
        reinterpret_cast<vtkRenderWindow*>(0x1), config,
        [](int) {}, [](double, double) {});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

// =============================================================================
// Construction and move tests
// =============================================================================

TEST(VideoExporterTest, DefaultConstruction) {
    VideoExporter exporter;
    // Should not crash
}

TEST(VideoExporterTest, MoveConstruction) {
    VideoExporter exporter;
    VideoExporter moved(std::move(exporter));
    // Should not crash
}

TEST(VideoExporterTest, MoveAssignment) {
    VideoExporter a;
    VideoExporter b;
    b = std::move(a);
    // Should not crash
}

TEST(VideoExporterTest, ProgressCallbackCanBeSet) {
    VideoExporter exporter;
    int callCount = 0;
    exporter.setProgressCallback(
        [&](double, const std::string&) { ++callCount; });
    // Callback is stored, not invoked without export
    EXPECT_EQ(callCount, 0);
}

// =============================================================================
// CineConfig default values
// =============================================================================

TEST(VideoExporterTest, CineConfigDefaults) {
    VideoExporter::CineConfig config;
    EXPECT_EQ(config.width, 1920);
    EXPECT_EQ(config.height, 1080);
    EXPECT_EQ(config.fps, 15);
    EXPECT_EQ(config.startPhase, 0);
    EXPECT_EQ(config.endPhase, -1);
    EXPECT_EQ(config.totalPhases, 0);
    EXPECT_EQ(config.loops, 1);
    EXPECT_EQ(config.framesPerPhase, 1);
    EXPECT_TRUE(config.outputPath.empty());
}

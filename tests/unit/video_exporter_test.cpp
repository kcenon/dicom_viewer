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
// Export error handling tests
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

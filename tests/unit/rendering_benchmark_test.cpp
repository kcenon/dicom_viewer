#include "core/image_converter.hpp"
#include "services/flow/vessel_analyzer.hpp"
#include "services/mpr_renderer.hpp"
#include "services/surface_renderer.hpp"
#include "services/volume_renderer.hpp"

#include "../test_utils/benchmark_fixture.hpp"
#include "../test_utils/flow_phantom_generator.hpp"
#include "../test_utils/volume_generator.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace dicom_viewer::services {
namespace {

using test_utils::PerformanceBenchmark;
using test_utils::createSphereVolume;
using test_utils::createSyntheticCTVolume;
using test_utils::createVolume;

// =============================================================================
// ImageConverter Benchmarks (ITK <-> VTK)
// =============================================================================

class ImageConverterBenchmarkTest : public PerformanceBenchmark {};

TEST_F(ImageConverterBenchmarkTest, ItkToVtk128Cube) {
    auto itkImage = createSyntheticCTVolume(128);

    std::chrono::milliseconds elapsed;
    auto vtkImage = measureTimeWithResult(
        [&] { return core::ImageConverter::itkToVtk(itkImage); }, elapsed);

    ASSERT_NE(vtkImage, nullptr);
    assertWithinThreshold(elapsed, 2000, "ITK->VTK conversion 128^3");
}

TEST_F(ImageConverterBenchmarkTest, VtkToItk128Cube) {
    auto itkImage = createSyntheticCTVolume(128);
    auto vtkImage = core::ImageConverter::itkToVtk(itkImage);
    ASSERT_NE(vtkImage, nullptr);

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] { return core::ImageConverter::vtkToItkCT(vtkImage); }, elapsed);

    ASSERT_NE(result, nullptr);
    assertWithinThreshold(elapsed, 2000, "VTK->ITK conversion 128^3");
}

TEST_F(ImageConverterBenchmarkTest, RoundTripConversion128Cube) {
    auto original = createSyntheticCTVolume(128);

    auto elapsed = measureTime([&] {
        auto vtkImg = core::ImageConverter::itkToVtk(original);
        ASSERT_NE(vtkImg, nullptr);
        auto roundTrip = core::ImageConverter::vtkToItkCT(vtkImg);
        ASSERT_NE(roundTrip, nullptr);
    });

    assertWithinThreshold(elapsed, 4000,
                          "ITK->VTK->ITK round-trip 128^3");
}

// =============================================================================
// Rendering Benchmarks
// =============================================================================

class RenderingBenchmarkTest : public PerformanceBenchmark {
protected:
    void SetUp() override {
        auto itkImage = createSyntheticCTVolume(128);
        vtkImage_ = core::ImageConverter::itkToVtk(itkImage);
        ASSERT_NE(vtkImage_, nullptr);
    }

    vtkSmartPointer<vtkImageData> vtkImage_;
};

TEST_F(RenderingBenchmarkTest, VolumeRendererSetup) {
    auto elapsed = measureTime([&] {
        VolumeRenderer renderer;
        renderer.setInputData(vtkImage_);

        auto volume = renderer.getVolume();
        ASSERT_NE(volume, nullptr);
    });

    assertWithinThreshold(elapsed, 2000,
                          "VolumeRenderer setup + first frame 128^3");
}

TEST_F(RenderingBenchmarkTest, MprRendererSliceExtraction) {
    MPRRenderer renderer;
    renderer.setInputData(vtkImage_);

    auto elapsed = measureTime([&] {
        // Extract slices at all three planes at center position
        renderer.setSlicePosition(MPRPlane::Axial, 64.0);
        renderer.setSlicePosition(MPRPlane::Coronal, 64.0);
        renderer.setSlicePosition(MPRPlane::Sagittal, 64.0);

        // Scroll through 10 slices to measure extraction performance
        for (int i = 0; i < 10; ++i) {
            renderer.scrollSlice(MPRPlane::Axial, 1);
        }
    });

    assertWithinThreshold(elapsed, 500,
                          "MPR slice extraction (3 planes + 10 scrolls)");
}

TEST_F(RenderingBenchmarkTest, SurfaceRendererExtraction) {
    SurfaceRenderer renderer;
    renderer.setInputData(vtkImage_);

    // Configure a bone-like surface extraction
    SurfaceConfig config;
    config.name = "bone";
    config.isovalue = 200.0;
    config.smoothingEnabled = true;
    config.smoothingIterations = 20;
    config.decimationEnabled = true;
    config.decimationReduction = 0.5;
    renderer.addSurface(config);

    auto elapsed = measureTime([&] {
        renderer.extractSurfaces();
    });

    auto data = renderer.getSurfaceData(0);
    std::cout << "[BENCHMARK] Surface extraction: "
              << data.triangleCount << " triangles" << std::endl;

    assertWithinThreshold(elapsed, 5000,
                          "Surface extraction + smoothing + decimation 128^3");
}

// =============================================================================
// Vesselness / Vorticity Benchmark
// =============================================================================

class VesselAnalyzerBenchmarkTest : public PerformanceBenchmark {};

TEST_F(VesselAnalyzerBenchmarkTest, VorticityComputation64Cube) {
    auto [phase, truth] =
        test_utils::generatePoiseuillePipe(64, 100.0, 20.0, 0);

    VesselAnalyzer analyzer;

    std::chrono::milliseconds elapsed;
    auto result = measureTimeWithResult(
        [&] { return analyzer.computeVorticity(phase); }, elapsed);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    ASSERT_NE(result->vorticityMagnitude, nullptr);
    assertWithinThreshold(elapsed, 5000, "Vorticity computation 64^3");
}

}  // namespace
}  // namespace dicom_viewer::services

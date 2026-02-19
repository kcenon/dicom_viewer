#include "services/export/mesh_exporter.hpp"
#include "services/export/data_exporter.hpp"

#include <gtest/gtest.h>
#include <QApplication>
#include <QFile>
#include <filesystem>
#include <fstream>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSphereSource.h>
#include <vtkCubeSource.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkUnsignedCharArray.h>

namespace dicom_viewer::services {
namespace {

class MeshExporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "mesh_exporter_test";
        std::filesystem::create_directories(testDir_);

        // Create synthetic label map (simple cube with label 1)
        createSyntheticLabelMap();

        // Create synthetic volume data
        createSyntheticVolume();

        // Create synthetic polydata (sphere)
        createSyntheticPolyData();
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    void createSyntheticLabelMap() {
        labelMap_ = vtkSmartPointer<vtkImageData>::New();
        labelMap_->SetDimensions(64, 64, 64);
        labelMap_->SetSpacing(1.0, 1.0, 1.0);
        labelMap_->SetOrigin(0.0, 0.0, 0.0);
        labelMap_->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

        // Fill with background (0)
        auto* scalars = labelMap_->GetPointData()->GetScalars();
        for (vtkIdType i = 0; i < scalars->GetNumberOfTuples(); ++i) {
            scalars->SetTuple1(i, 0);
        }

        // Create a cube of label 1 in the center (20x20x20)
        for (int z = 22; z < 42; ++z) {
            for (int y = 22; y < 42; ++y) {
                for (int x = 22; x < 42; ++x) {
                    int ijk[3] = {x, y, z};
                    vtkIdType id = labelMap_->ComputePointId(ijk);
                    scalars->SetTuple1(id, 1);
                }
            }
        }

        // Create a smaller cube of label 2 (10x10x10)
        for (int z = 5; z < 15; ++z) {
            for (int y = 5; y < 15; ++y) {
                for (int x = 5; x < 15; ++x) {
                    int ijk[3] = {x, y, z};
                    vtkIdType id = labelMap_->ComputePointId(ijk);
                    scalars->SetTuple1(id, 2);
                }
            }
        }
    }

    void createSyntheticVolume() {
        volumeData_ = vtkSmartPointer<vtkImageData>::New();
        volumeData_->SetDimensions(64, 64, 64);
        volumeData_->SetSpacing(1.0, 1.0, 1.0);
        volumeData_->SetOrigin(0.0, 0.0, 0.0);
        volumeData_->AllocateScalars(VTK_SHORT, 1);

        auto* scalars = volumeData_->GetPointData()->GetScalars();

        // Create a sphere of high intensity in the center
        double center[3] = {32.0, 32.0, 32.0};
        double radius = 15.0;

        for (int z = 0; z < 64; ++z) {
            for (int y = 0; y < 64; ++y) {
                for (int x = 0; x < 64; ++x) {
                    int ijk[3] = {x, y, z};
                    vtkIdType id = volumeData_->ComputePointId(ijk);
                    double dx = x - center[0];
                    double dy = y - center[1];
                    double dz = z - center[2];
                    double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

                    if (dist < radius) {
                        scalars->SetTuple1(id, 500);  // High HU (bone-like)
                    } else {
                        scalars->SetTuple1(id, -500); // Low HU (air-like)
                    }
                }
            }
        }
    }

    void createSyntheticPolyData() {
        vtkNew<vtkSphereSource> sphere;
        sphere->SetRadius(10.0);
        sphere->SetCenter(0, 0, 0);
        sphere->SetThetaResolution(32);
        sphere->SetPhiResolution(32);
        sphere->Update();

        polyData_ = sphere->GetOutput();
    }

    bool fileExists(const std::filesystem::path& path) {
        return std::filesystem::exists(path);
    }

    size_t getFileSize(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) return 0;
        return std::filesystem::file_size(path);
    }

    std::string readFileContent(const std::filesystem::path& path, size_t maxBytes = 1024) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return "";

        std::string content;
        content.resize(maxBytes);
        file.read(&content[0], maxBytes);
        content.resize(static_cast<size_t>(file.gcount()));
        return content;
    }

    std::filesystem::path testDir_;
    vtkSmartPointer<vtkImageData> labelMap_;
    vtkSmartPointer<vtkImageData> volumeData_;
    vtkSmartPointer<vtkPolyData> polyData_;
};

// =============================================================================
// MeshExportOptions tests
// =============================================================================

TEST_F(MeshExporterTest, MeshExportOptionsDefaultValues) {
    MeshExportOptions options;

    EXPECT_TRUE(options.smooth);
    EXPECT_EQ(options.smoothIterations, 20);
    EXPECT_DOUBLE_EQ(options.smoothRelaxation, 0.1);
    EXPECT_TRUE(options.decimate);
    EXPECT_DOUBLE_EQ(options.decimateTargetReduction, 0.5);
    EXPECT_TRUE(options.computeNormals);
    EXPECT_EQ(options.stlFormat, STLFormat::Binary);
    EXPECT_TRUE(options.plyIncludeColors);
    EXPECT_TRUE(options.plyIncludeNormals);
    EXPECT_EQ(options.coordSystem, CoordinateSystem::RAS);
    EXPECT_TRUE(options.applyScaling);
    EXPECT_DOUBLE_EQ(options.isoValue, 400.0);
}

// =============================================================================
// MeshExporter construction tests
// =============================================================================

TEST_F(MeshExporterTest, DefaultConstruction) {
    MeshExporter exporter;
    // Should not crash
}

TEST_F(MeshExporterTest, MoveConstruction) {
    MeshExporter exporter1;
    MeshExporter exporter2(std::move(exporter1));
    // Should not crash
}

TEST_F(MeshExporterTest, MoveAssignment) {
    MeshExporter exporter1;
    MeshExporter exporter2;
    exporter2 = std::move(exporter1);
    // Should not crash
}

// =============================================================================
// Utility method tests
// =============================================================================

TEST_F(MeshExporterTest, GetFileExtension) {
    EXPECT_EQ(MeshExporter::getFileExtension(MeshFormat::STL), ".stl");
    EXPECT_EQ(MeshExporter::getFileExtension(MeshFormat::PLY), ".ply");
    EXPECT_EQ(MeshExporter::getFileExtension(MeshFormat::OBJ), ".obj");
}

TEST_F(MeshExporterTest, DetectFormat) {
    auto stlFormat = MeshExporter::detectFormat("mesh.stl");
    ASSERT_TRUE(stlFormat.has_value());
    EXPECT_EQ(*stlFormat, MeshFormat::STL);

    auto plyFormat = MeshExporter::detectFormat("mesh.PLY");
    ASSERT_TRUE(plyFormat.has_value());
    EXPECT_EQ(*plyFormat, MeshFormat::PLY);

    auto objFormat = MeshExporter::detectFormat("mesh.obj");
    ASSERT_TRUE(objFormat.has_value());
    EXPECT_EQ(*objFormat, MeshFormat::OBJ);

    auto unknownFormat = MeshExporter::detectFormat("mesh.xyz");
    EXPECT_FALSE(unknownFormat.has_value());
}

TEST_F(MeshExporterTest, GetUniqueLabels) {
    auto labels = MeshExporter::getUniqueLabels(labelMap_);

    EXPECT_EQ(labels.size(), 2);
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), 1) != labels.end());
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), 2) != labels.end());
    // Background (0) should not be included
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), 0) == labels.end());
}

TEST_F(MeshExporterTest, GetUniqueLabels_NullInput) {
    auto labels = MeshExporter::getUniqueLabels(nullptr);
    EXPECT_TRUE(labels.empty());
}

// =============================================================================
// Export from PolyData tests
// =============================================================================

TEST_F(MeshExporterTest, ExportPolyDataToSTL_Binary) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "sphere.stl";

    MeshExportOptions options;
    options.stlFormat = STLFormat::Binary;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));
    EXPECT_GT(result->vertexCount, 0);
    EXPECT_GT(result->triangleCount, 0);
    EXPECT_GT(result->surfaceAreaMm2, 0.0);
    EXPECT_EQ(result->outputPath, outputPath);

    // Binary STL should start with 80 byte header
    auto content = readFileContent(outputPath, 100);
    EXPECT_GE(content.size(), 84);  // 80 header + 4 byte triangle count
}

TEST_F(MeshExporterTest, ExportPolyDataToSTL_ASCII) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "sphere_ascii.stl";

    MeshExportOptions options;
    options.stlFormat = STLFormat::ASCII;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));

    // ASCII STL should start with "solid"
    auto content = readFileContent(outputPath, 100);
    EXPECT_TRUE(content.find("solid") != std::string::npos);
}

TEST_F(MeshExporterTest, ExportPolyDataToPLY) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "sphere.ply";

    MeshExportOptions options;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::PLY, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));
    EXPECT_GT(result->vertexCount, 0);
    EXPECT_GT(result->triangleCount, 0);

    // PLY file should start with "ply"
    auto content = readFileContent(outputPath, 100);
    EXPECT_TRUE(content.find("ply") != std::string::npos);
}

TEST_F(MeshExporterTest, ExportPolyDataToOBJ) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "sphere.obj";

    MeshExportOptions options;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::OBJ, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));
    EXPECT_GT(result->vertexCount, 0);
    EXPECT_GT(result->triangleCount, 0);

    // OBJ file should contain vertex definitions (v lines)
    auto content = readFileContent(outputPath, 2000);
    EXPECT_TRUE(content.find("v ") != std::string::npos);
    // Face definitions may use different formats depending on VTK version
    // Check file is non-empty and has reasonable size
    EXPECT_GT(getFileSize(outputPath), 100);
}

TEST_F(MeshExporterTest, ExportPolyData_NullInput) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "null.stl";

    auto result = exporter.exportPolyData(
        nullptr, outputPath, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

// =============================================================================
// Export from Segmentation tests
// =============================================================================

TEST_F(MeshExporterTest, ExportFromSegmentation_Basic) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "segmentation.stl";

    MeshExportOptions options;
    options.smooth = true;
    options.smoothIterations = 10;
    options.decimate = true;
    options.decimateTargetReduction = 0.3;

    auto result = exporter.exportFromSegmentation(
        labelMap_, 1, outputPath, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));
    EXPECT_GT(result->vertexCount, 0);
    EXPECT_GT(result->triangleCount, 0);
    EXPECT_GT(result->surfaceAreaMm2, 0.0);
}

TEST_F(MeshExporterTest, ExportFromSegmentation_LabelZero) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "label0.stl";

    auto result = exporter.exportFromSegmentation(
        labelMap_, 0, outputPath, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST_F(MeshExporterTest, ExportFromSegmentation_NullLabelMap) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "null_label.stl";

    auto result = exporter.exportFromSegmentation(
        nullptr, 1, outputPath, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST_F(MeshExporterTest, ExportFromSegmentation_NonexistentLabel) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "nonexistent.stl";

    // Label 255 doesn't exist in our test data
    auto result = exporter.exportFromSegmentation(
        labelMap_, 255, outputPath, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST_F(MeshExporterTest, ExportAllLabels) {
    MeshExporter exporter;

    MeshExportOptions options;
    options.smooth = true;
    options.smoothIterations = 5;

    auto result = exporter.exportAllLabels(
        labelMap_, testDir_, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2);  // We have labels 1 and 2

    // Check that files were created
    EXPECT_TRUE(fileExists(testDir_ / "label_1.stl"));
    EXPECT_TRUE(fileExists(testDir_ / "label_2.stl"));
}

TEST_F(MeshExporterTest, ExportAllLabels_NonexistentDirectory) {
    MeshExporter exporter;
    auto nonexistentDir = testDir_ / "nonexistent_dir";

    auto result = exporter.exportAllLabels(
        labelMap_, nonexistentDir, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::FileAccessDenied);
}

// =============================================================================
// Export from Iso-Surface tests
// =============================================================================

TEST_F(MeshExporterTest, ExportIsoSurface_Basic) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "isosurface.stl";

    MeshExportOptions options;
    options.smooth = true;
    options.smoothIterations = 10;
    options.decimate = true;
    options.decimateTargetReduction = 0.3;

    // Extract surface at threshold between high and low intensities
    auto result = exporter.exportIsoSurface(
        volumeData_, 0.0, outputPath, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));
    EXPECT_GT(result->vertexCount, 0);
    EXPECT_GT(result->triangleCount, 0);
    EXPECT_GT(result->surfaceAreaMm2, 0.0);
}

TEST_F(MeshExporterTest, ExportIsoSurface_NullVolume) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "null_iso.stl";

    auto result = exporter.exportIsoSurface(
        nullptr, 300.0, outputPath, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST_F(MeshExporterTest, ExportIsoSurface_NoSurfaceFound) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "no_surface.stl";

    // Very high threshold that doesn't match anything
    auto result = exporter.exportIsoSurface(
        volumeData_, 10000.0, outputPath, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

// =============================================================================
// Preview statistics tests
// =============================================================================

TEST_F(MeshExporterTest, PreviewStatistics_FromSegmentation) {
    MeshExporter exporter;

    MeshExportOptions options;
    options.smooth = true;
    options.smoothIterations = 10;
    options.decimate = false;

    auto result = exporter.previewStatistics(labelMap_, 1, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->vertexCount, 0);
    EXPECT_GT(result->triangleCount, 0);
    EXPECT_GT(result->surfaceAreaMm2, 0.0);
    // Bounding box should be reasonable
    EXPECT_LT(result->boundingBox[0], result->boundingBox[1]);  // xmin < xmax
    EXPECT_LT(result->boundingBox[2], result->boundingBox[3]);  // ymin < ymax
    EXPECT_LT(result->boundingBox[4], result->boundingBox[5]);  // zmin < zmax
}

TEST_F(MeshExporterTest, PreviewIsoSurfaceStatistics) {
    MeshExporter exporter;

    MeshExportOptions options;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.previewIsoSurfaceStatistics(volumeData_, 0.0, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->vertexCount, 0);
    EXPECT_GT(result->triangleCount, 0);
    EXPECT_GT(result->surfaceAreaMm2, 0.0);
}

// =============================================================================
// Mesh processing tests
// =============================================================================

TEST_F(MeshExporterTest, ExportWithSmoothing) {
    MeshExporter exporter;

    MeshExportOptions optionsNoSmooth;
    optionsNoSmooth.smooth = false;
    optionsNoSmooth.decimate = false;

    MeshExportOptions optionsSmooth;
    optionsSmooth.smooth = true;
    optionsSmooth.smoothIterations = 50;
    optionsSmooth.decimate = false;

    auto statsNoSmooth = exporter.previewStatistics(labelMap_, 1, optionsNoSmooth);
    auto statsSmooth = exporter.previewStatistics(labelMap_, 1, optionsSmooth);

    ASSERT_TRUE(statsNoSmooth.has_value());
    ASSERT_TRUE(statsSmooth.has_value());

    // Smoothing should typically maintain similar vertex count
    // but may change surface area
    EXPECT_GT(statsNoSmooth->vertexCount, 0);
    EXPECT_GT(statsSmooth->vertexCount, 0);
}

TEST_F(MeshExporterTest, ExportWithDecimation) {
    MeshExporter exporter;

    MeshExportOptions optionsNoDecimate;
    optionsNoDecimate.smooth = false;
    optionsNoDecimate.decimate = false;

    MeshExportOptions optionsDecimate;
    optionsDecimate.smooth = false;
    optionsDecimate.decimate = true;
    optionsDecimate.decimateTargetReduction = 0.5;

    auto statsNoDecimate = exporter.previewStatistics(labelMap_, 1, optionsNoDecimate);
    auto statsDecimate = exporter.previewStatistics(labelMap_, 1, optionsDecimate);

    ASSERT_TRUE(statsNoDecimate.has_value());
    ASSERT_TRUE(statsDecimate.has_value());

    // Decimation should reduce triangle count
    EXPECT_LT(statsDecimate->triangleCount, statsNoDecimate->triangleCount);
}

// =============================================================================
// Progress callback tests
// =============================================================================

TEST_F(MeshExporterTest, ProgressCallback) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "progress_test.stl";

    bool progressCalled = false;
    double lastProgress = -1.0;

    exporter.setProgressCallback([&](double progress, const QString& status) {
        progressCalled = true;
        lastProgress = progress;
        EXPECT_FALSE(status.isEmpty());
        EXPECT_GE(progress, 0.0);
        EXPECT_LE(progress, 1.0);
    });

    MeshExportOptions options;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(progressCalled);
    EXPECT_EQ(lastProgress, 1.0);  // Should end at 100%
}

// =============================================================================
// File access error tests
// =============================================================================

TEST_F(MeshExporterTest, ExportToInvalidPath) {
    MeshExporter exporter;
    // Try to write to a non-existent directory
    auto outputPath = std::filesystem::path("/nonexistent/dir/mesh.stl");

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::FileAccessDenied);
}

// =============================================================================
// Coordinate system tests
// =============================================================================

TEST_F(MeshExporterTest, CoordinateSystem_RAS) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "ras.stl";

    MeshExportOptions options;
    options.coordSystem = CoordinateSystem::RAS;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));
}

TEST_F(MeshExporterTest, CoordinateSystem_LPS) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "lps.stl";

    MeshExportOptions options;
    options.coordSystem = CoordinateSystem::LPS;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL, options);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fileExists(outputPath));
}

// =============================================================================
// File format comparison tests
// =============================================================================

TEST_F(MeshExporterTest, BinarySTL_SmallerThanASCII) {
    MeshExporter exporter;

    auto binaryPath = testDir_ / "binary.stl";
    auto asciiPath = testDir_ / "ascii.stl";

    MeshExportOptions binaryOptions;
    binaryOptions.stlFormat = STLFormat::Binary;
    binaryOptions.smooth = false;
    binaryOptions.decimate = false;

    MeshExportOptions asciiOptions;
    asciiOptions.stlFormat = STLFormat::ASCII;
    asciiOptions.smooth = false;
    asciiOptions.decimate = false;

    auto binaryResult = exporter.exportPolyData(
        polyData_, binaryPath, MeshFormat::STL, binaryOptions);
    auto asciiResult = exporter.exportPolyData(
        polyData_, asciiPath, MeshFormat::STL, asciiOptions);

    ASSERT_TRUE(binaryResult.has_value());
    ASSERT_TRUE(asciiResult.has_value());

    // Binary should be smaller than ASCII
    auto binarySize = getFileSize(binaryPath);
    auto asciiSize = getFileSize(asciiPath);

    EXPECT_GT(binarySize, 0);
    EXPECT_GT(asciiSize, 0);
    EXPECT_LT(binarySize, asciiSize);
}

// =============================================================================
// Output validation and format verification tests (Issue #207)
// =============================================================================

TEST_F(MeshExporterTest, StlBinaryHeaderFormatValid) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "header_check.stl";

    MeshExportOptions options;
    options.stlFormat = STLFormat::Binary;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL, options);
    ASSERT_TRUE(result.has_value());

    // STL binary format: 80-byte header + 4-byte triangle count (uint32_t LE)
    std::ifstream file(outputPath, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    // Read and verify 80-byte header exists
    char header[80] = {};
    file.read(header, 80);
    ASSERT_EQ(file.gcount(), 80);

    // Read 4-byte triangle count
    uint32_t triangleCount = 0;
    file.read(reinterpret_cast<char*>(&triangleCount), sizeof(uint32_t));
    ASSERT_EQ(file.gcount(), 4);

    EXPECT_GT(triangleCount, 0u) << "STL file should contain triangles";
}

TEST_F(MeshExporterTest, StlTriangleCountMatchesResult) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "tricount_check.stl";

    MeshExportOptions options;
    options.stlFormat = STLFormat::Binary;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::STL, options);
    ASSERT_TRUE(result.has_value());

    // Read triangle count from binary STL (at offset 80)
    std::ifstream file(outputPath, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    file.seekg(80);  // Skip header
    uint32_t fileTriangleCount = 0;
    file.read(reinterpret_cast<char*>(&fileTriangleCount), sizeof(uint32_t));

    // Triangle count in file should match the result struct
    EXPECT_EQ(static_cast<size_t>(fileTriangleCount), result->triangleCount)
        << "STL binary triangle count should match MeshExportResult";
}

TEST_F(MeshExporterTest, PlyContainsVertexAndFaceDeclarations) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "format_check.ply";

    MeshExportOptions options;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::PLY, options);
    ASSERT_TRUE(result.has_value());

    std::string content = readFileContent(outputPath, 4096);

    // PLY header must contain these declarations
    EXPECT_NE(content.find("ply"), std::string::npos)
        << "PLY file must start with 'ply' magic";
    EXPECT_NE(content.find("element vertex"), std::string::npos)
        << "PLY file must declare vertex elements";
    EXPECT_NE(content.find("element face"), std::string::npos)
        << "PLY file must declare face elements";
    EXPECT_NE(content.find("end_header"), std::string::npos)
        << "PLY file must have end_header marker";
}

TEST_F(MeshExporterTest, ObjContainsVertexAndFaceLines) {
    MeshExporter exporter;
    auto outputPath = testDir_ / "format_check.obj";

    MeshExportOptions options;
    options.smooth = false;
    options.decimate = false;

    auto result = exporter.exportPolyData(
        polyData_, outputPath, MeshFormat::OBJ, options);
    ASSERT_TRUE(result.has_value());

    std::string content = readFileContent(outputPath, 131072);

    // OBJ format: 'v' lines for vertices, 'f' lines for faces
    EXPECT_NE(content.find("\nv "), std::string::npos)
        << "OBJ file must contain vertex lines (v x y z)";
    EXPECT_NE(content.find("\nf "), std::string::npos)
        << "OBJ file must contain face lines (f v1 v2 v3)";
}

TEST_F(MeshExporterTest, CoordinateSystemAffectsVertexPositions) {
    MeshExporter exporter;
    auto rasPath = testDir_ / "ras_coords.obj";
    auto lpsPath = testDir_ / "lps_coords.obj";

    MeshExportOptions rasOptions;
    rasOptions.coordSystem = CoordinateSystem::RAS;
    rasOptions.smooth = false;
    rasOptions.decimate = false;

    MeshExportOptions lpsOptions;
    lpsOptions.coordSystem = CoordinateSystem::LPS;
    lpsOptions.smooth = false;
    lpsOptions.decimate = false;

    auto rasResult = exporter.exportPolyData(
        polyData_, rasPath, MeshFormat::OBJ, rasOptions);
    auto lpsResult = exporter.exportPolyData(
        polyData_, lpsPath, MeshFormat::OBJ, lpsOptions);

    ASSERT_TRUE(rasResult.has_value());
    ASSERT_TRUE(lpsResult.has_value());

    // Both should have same triangle count
    EXPECT_EQ(rasResult->triangleCount, lpsResult->triangleCount);

    // But file contents should differ due to coordinate transformation
    // RAS and LPS differ in the sign of X and Y axes
    std::string rasContent = readFileContent(rasPath, 8192);
    std::string lpsContent = readFileContent(lpsPath, 8192);

    EXPECT_NE(rasContent, lpsContent)
        << "RAS and LPS coordinate systems should produce different vertex data";
}

}  // namespace
}  // namespace dicom_viewer::services

int main(int argc, char** argv) {
    // Initialize Qt application for QFile operations
    QApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

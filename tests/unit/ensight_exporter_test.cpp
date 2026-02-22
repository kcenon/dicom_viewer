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

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "services/export/ensight_exporter.hpp"

using namespace dicom_viewer::services;
using FloatImage3D = EnsightExporter::FloatImage3D;
using VectorImage3D = EnsightExporter::VectorImage3D;

namespace {

/**
 * @brief Create a scalar image filled with a constant value
 */
FloatImage3D::Pointer createScalarImage(int nx, int ny, int nz,
                                        float value = 0.0f,
                                        double spacingMm = 1.0) {
    auto image = FloatImage3D::New();
    FloatImage3D::RegionType region;
    FloatImage3D::SizeType size;
    size[0] = nx; size[1] = ny; size[2] = nz;
    region.SetSize(size);
    image->SetRegions(region);

    FloatImage3D::SpacingType spacing;
    spacing[0] = spacingMm; spacing[1] = spacingMm; spacing[2] = spacingMm;
    image->SetSpacing(spacing);

    FloatImage3D::PointType origin;
    origin[0] = 0.0; origin[1] = 0.0; origin[2] = 0.0;
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(value);
    return image;
}

/**
 * @brief Create a scalar image with linearly varying values
 */
FloatImage3D::Pointer createGradientScalarImage(int nx, int ny, int nz) {
    auto image = createScalarImage(nx, ny, nz);
    float* buf = image->GetBufferPointer();
    size_t total = static_cast<size_t>(nx) * ny * nz;
    for (size_t i = 0; i < total; ++i) {
        buf[i] = static_cast<float>(i) / static_cast<float>(total);
    }
    return image;
}

/**
 * @brief Create a 3-component vector image with known pattern
 */
VectorImage3D::Pointer createVectorImage(int nx, int ny, int nz,
                                         float vx = 1.0f,
                                         float vy = 0.0f,
                                         float vz = 0.0f) {
    auto image = VectorImage3D::New();
    VectorImage3D::RegionType region;
    VectorImage3D::SizeType size;
    size[0] = nx; size[1] = ny; size[2] = nz;
    region.SetSize(size);
    image->SetRegions(region);
    image->SetNumberOfComponentsPerPixel(3);

    VectorImage3D::SpacingType spacing;
    spacing[0] = 1.0; spacing[1] = 1.0; spacing[2] = 1.0;
    image->SetSpacing(spacing);

    image->Allocate();

    float* buf = image->GetBufferPointer();
    size_t total = static_cast<size_t>(nx) * ny * nz;
    for (size_t i = 0; i < total; ++i) {
        buf[i * 3 + 0] = vx;
        buf[i * 3 + 1] = vy;
        buf[i * 3 + 2] = vz;
    }
    return image;
}

/**
 * @brief Read an 80-byte string from a binary file
 */
std::string readBinaryString(std::ifstream& in) {
    char buf[80] = {};
    in.read(buf, 80);
    return std::string(buf, strnlen(buf, 80));
}

/**
 * @brief Read a 4-byte integer from a binary file
 */
int32_t readBinaryInt(std::ifstream& in) {
    int32_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(int32_t));
    return value;
}

/**
 * @brief Read a 4-byte float from a binary file
 */
float readBinaryFloat(std::ifstream& in) {
    float value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(float));
    return value;
}

}  // namespace

class EnsightExporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path()
                   / "ensight_exporter_test";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    std::filesystem::path testDir_;
};

// =============================================================================
// Case file tests
// =============================================================================

TEST_F(EnsightExporterTest, CaseFileFormat) {
    auto casePath = testDir_ / "test.case";
    auto result = EnsightExporter::writeCaseFile(
        casePath, "test",
        {"Magnitude", "Speed"}, {"Velocity"},
        5, {0.0, 0.033, 0.067, 0.100, 0.133});

    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Read and verify case file content
    std::ifstream in(casePath);
    ASSERT_TRUE(in.is_open());

    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("type: ensight gold"), std::string::npos);
    EXPECT_NE(content.find("model: test.geo"), std::string::npos);
    EXPECT_NE(content.find("scalar per node: Magnitude test.Magnitude****"),
              std::string::npos);
    EXPECT_NE(content.find("scalar per node: Speed test.Speed****"),
              std::string::npos);
    EXPECT_NE(content.find("vector per node: Velocity test.Velocity****"),
              std::string::npos);
    EXPECT_NE(content.find("number of steps:       5"), std::string::npos);
    EXPECT_NE(content.find("time values:"), std::string::npos);
}

TEST_F(EnsightExporterTest, CaseFileSinglePhaseNoTimeSection) {
    auto casePath = testDir_ / "single.case";
    auto result = EnsightExporter::writeCaseFile(
        casePath, "single",
        {"Magnitude"}, {},
        1, {0.0});

    ASSERT_TRUE(result.has_value());

    std::ifstream in(casePath);
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());

    // Single phase: no TIME section
    EXPECT_EQ(content.find("TIME"), std::string::npos);
}

// =============================================================================
// Geometry file tests
// =============================================================================

TEST_F(EnsightExporterTest, GeometryFileStructuredGrid) {
    auto image = createScalarImage(4, 3, 2, 0.0f, 2.0);  // 2mm spacing
    auto geoPath = testDir_ / "test.geo";

    auto result = EnsightExporter::writeGeometry(geoPath, image.GetPointer());
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Verify binary content
    std::ifstream in(geoPath, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    EXPECT_EQ(readBinaryString(in), "C Binary");
    EXPECT_EQ(readBinaryString(in), "Ensight Gold geometry file");
    readBinaryString(in);  // description line 2
    EXPECT_EQ(readBinaryString(in), "node id off");
    EXPECT_EQ(readBinaryString(in), "element id off");

    EXPECT_EQ(readBinaryString(in), "part");
    EXPECT_EQ(readBinaryInt(in), 1);
    readBinaryString(in);  // part description
    EXPECT_EQ(readBinaryString(in), "block");

    EXPECT_EQ(readBinaryInt(in), 4);  // NX
    EXPECT_EQ(readBinaryInt(in), 3);  // NY
    EXPECT_EQ(readBinaryInt(in), 2);  // NZ

    // Verify X coordinates: 0, 2, 4, 6 for each of 3*2=6 j,k combinations
    for (int k = 0; k < 2; ++k) {
        for (int j = 0; j < 3; ++j) {
            for (int i = 0; i < 4; ++i) {
                float x = readBinaryFloat(in);
                EXPECT_FLOAT_EQ(x, static_cast<float>(i * 2.0))
                    << "X at i=" << i << " j=" << j << " k=" << k;
            }
        }
    }

    // Verify first Y coordinate
    float y0 = readBinaryFloat(in);
    EXPECT_FLOAT_EQ(y0, 0.0f);
}

TEST_F(EnsightExporterTest, GeometryFileSize) {
    auto image = createScalarImage(8, 8, 8);
    auto geoPath = testDir_ / "size.geo";

    auto result = EnsightExporter::writeGeometry(geoPath, image.GetPointer());
    ASSERT_TRUE(result.has_value());

    auto fileSize = std::filesystem::file_size(geoPath);
    // Header: 5 * 80 = 400 bytes
    // Part: 80 + 4 + 80 + 80 = 244 bytes
    // Dims: 3 * 4 = 12 bytes
    // Coords: 3 * 512 * 4 = 6144 bytes
    // Total: 400 + 244 + 12 + 6144 = 6800 bytes
    EXPECT_EQ(fileSize, 6800u);
}

TEST_F(EnsightExporterTest, GeometryNullImage) {
    auto geoPath = testDir_ / "null.geo";
    auto result = EnsightExporter::writeGeometry(geoPath, nullptr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

// =============================================================================
// Scalar variable tests
// =============================================================================

TEST_F(EnsightExporterTest, ScalarVariableRoundtrip) {
    auto image = createGradientScalarImage(4, 3, 2);
    auto varPath = testDir_ / "test.Magnitude0001";

    auto result = EnsightExporter::writeScalarVariable(
        varPath, "Magnitude", image.GetPointer());
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Read back and verify
    std::ifstream in(varPath, std::ios::binary);
    EXPECT_EQ(readBinaryString(in), "Magnitude");
    EXPECT_EQ(readBinaryString(in), "part");
    EXPECT_EQ(readBinaryInt(in), 1);
    EXPECT_EQ(readBinaryString(in), "block");

    // Verify all values match original
    const float* buf = image->GetBufferPointer();
    for (int i = 0; i < 4 * 3 * 2; ++i) {
        float val = readBinaryFloat(in);
        EXPECT_FLOAT_EQ(val, buf[i]) << "Mismatch at voxel " << i;
    }
}

TEST_F(EnsightExporterTest, ScalarVariableFileSize) {
    auto image = createScalarImage(10, 10, 10, 42.0f);
    auto varPath = testDir_ / "test.Speed0001";

    auto result = EnsightExporter::writeScalarVariable(
        varPath, "Speed", image.GetPointer());
    ASSERT_TRUE(result.has_value());

    auto fileSize = std::filesystem::file_size(varPath);
    // Header: 80 (desc) + 80 (part) + 4 (int) + 80 (block) = 244 bytes
    // Data: 1000 * 4 = 4000 bytes
    EXPECT_EQ(fileSize, 4244u);
}

// =============================================================================
// Vector variable tests
// =============================================================================

TEST_F(EnsightExporterTest, VectorVariableRoundtrip) {
    auto image = createVectorImage(4, 3, 2, 1.0f, 2.0f, 3.0f);
    auto varPath = testDir_ / "test.Velocity0001";

    auto result = EnsightExporter::writeVectorVariable(
        varPath, "Velocity", image.GetPointer());
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Read back and verify
    std::ifstream in(varPath, std::ios::binary);
    readBinaryString(in);  // description
    readBinaryString(in);  // "part"
    readBinaryInt(in);     // part number
    readBinaryString(in);  // "block"

    size_t numNodes = 4 * 3 * 2;

    // Ensight block format: all Vx, then all Vy, then all Vz
    for (size_t i = 0; i < numNodes; ++i) {
        EXPECT_FLOAT_EQ(readBinaryFloat(in), 1.0f) << "Vx at node " << i;
    }
    for (size_t i = 0; i < numNodes; ++i) {
        EXPECT_FLOAT_EQ(readBinaryFloat(in), 2.0f) << "Vy at node " << i;
    }
    for (size_t i = 0; i < numNodes; ++i) {
        EXPECT_FLOAT_EQ(readBinaryFloat(in), 3.0f) << "Vz at node " << i;
    }
}

TEST_F(EnsightExporterTest, VectorVariableFileSize) {
    auto image = createVectorImage(10, 10, 10);
    auto varPath = testDir_ / "test.Velocity0001";

    auto result = EnsightExporter::writeVectorVariable(
        varPath, "Velocity", image.GetPointer());
    ASSERT_TRUE(result.has_value());

    auto fileSize = std::filesystem::file_size(varPath);
    // Header: 244 bytes (same as scalar)
    // Data: 3 * 1000 * 4 = 12000 bytes (3 components)
    EXPECT_EQ(fileSize, 12244u);
}

// =============================================================================
// Full export integration tests
// =============================================================================

TEST_F(EnsightExporterTest, ExportSinglePhase) {
    EnsightExporter exporter;

    EnsightExporter::PhaseData phase;
    phase.timeValue = 0.0;
    phase.scalars.push_back({"Magnitude", createScalarImage(8, 8, 8, 100.0f)});
    phase.vectors.push_back({"Velocity", createVectorImage(8, 8, 8, 10.0f, 5.0f, 2.0f)});

    EnsightExporter::ExportConfig config;
    config.outputDir = testDir_;
    config.caseName = "single_phase";

    auto result = exporter.exportData({phase}, config);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Verify files exist
    EXPECT_TRUE(std::filesystem::exists(testDir_ / "single_phase.case"));
    EXPECT_TRUE(std::filesystem::exists(testDir_ / "single_phase.geo"));
    EXPECT_TRUE(std::filesystem::exists(testDir_ / "single_phase.Magnitude0001"));
    EXPECT_TRUE(std::filesystem::exists(testDir_ / "single_phase.Velocity0001"));
}

TEST_F(EnsightExporterTest, ExportMultiPhase) {
    EnsightExporter exporter;

    std::vector<EnsightExporter::PhaseData> phases;
    for (int p = 0; p < 5; ++p) {
        EnsightExporter::PhaseData phase;
        phase.timeValue = p * 0.033;
        float mag = static_cast<float>(p * 20 + 50);
        phase.scalars.push_back({"Magnitude", createScalarImage(8, 8, 8, mag)});
        phase.scalars.push_back({"Speed", createScalarImage(8, 8, 8, mag * 0.5f)});
        phase.vectors.push_back({"Velocity", createVectorImage(8, 8, 8, mag, 0, 0)});
        phases.push_back(std::move(phase));
    }

    EnsightExporter::ExportConfig config;
    config.outputDir = testDir_;
    config.caseName = "multi_phase";

    auto result = exporter.exportData(phases, config);
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Verify all files exist (1 case + 1 geo + 5 phases * 3 vars = 17 files)
    EXPECT_TRUE(std::filesystem::exists(testDir_ / "multi_phase.case"));
    EXPECT_TRUE(std::filesystem::exists(testDir_ / "multi_phase.geo"));

    for (int p = 1; p <= 5; ++p) {
        std::ostringstream mag, spd, vel;
        mag << "multi_phase.Magnitude" << std::setw(4) << std::setfill('0') << p;
        spd << "multi_phase.Speed" << std::setw(4) << std::setfill('0') << p;
        vel << "multi_phase.Velocity" << std::setw(4) << std::setfill('0') << p;

        EXPECT_TRUE(std::filesystem::exists(testDir_ / mag.str()))
            << "Missing: " << mag.str();
        EXPECT_TRUE(std::filesystem::exists(testDir_ / spd.str()))
            << "Missing: " << spd.str();
        EXPECT_TRUE(std::filesystem::exists(testDir_ / vel.str()))
            << "Missing: " << vel.str();
    }

    // Verify case file references TIME section
    std::ifstream in(testDir_ / "multi_phase.case");
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("number of steps:       5"), std::string::npos);
    EXPECT_NE(content.find("scalar per node: Speed"), std::string::npos);
}

TEST_F(EnsightExporterTest, ExportEmptyPhasesReturnsError) {
    EnsightExporter exporter;
    EnsightExporter::ExportConfig config;
    config.outputDir = testDir_;

    auto result = exporter.exportData({}, config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST_F(EnsightExporterTest, ExportNonexistentDirReturnsError) {
    EnsightExporter exporter;
    EnsightExporter::PhaseData phase;
    phase.scalars.push_back({"Magnitude", createScalarImage(4, 4, 4)});

    EnsightExporter::ExportConfig config;
    config.outputDir = testDir_ / "nonexistent_sub_dir";

    auto result = exporter.exportData({phase}, config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::FileAccessDenied);
}

TEST_F(EnsightExporterTest, ExportNoVariablesReturnsError) {
    EnsightExporter exporter;
    EnsightExporter::PhaseData phase;
    // No scalars or vectors

    EnsightExporter::ExportConfig config;
    config.outputDir = testDir_;

    auto result = exporter.exportData({phase}, config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ExportError::Code::InvalidData);
}

TEST_F(EnsightExporterTest, ProgressCallbackIsCalled) {
    EnsightExporter exporter;

    int callCount = 0;
    double lastProgress = -1.0;
    exporter.setProgressCallback(
        [&](double progress, const std::string& /*status*/) {
            ++callCount;
            EXPECT_GE(progress, lastProgress);
            lastProgress = progress;
        });

    EnsightExporter::PhaseData phase;
    phase.scalars.push_back({"Magnitude", createScalarImage(4, 4, 4)});

    EnsightExporter::ExportConfig config;
    config.outputDir = testDir_;
    config.caseName = "progress_test";

    auto result = exporter.exportData({phase}, config);
    ASSERT_TRUE(result.has_value());

    EXPECT_GT(callCount, 0);
    EXPECT_DOUBLE_EQ(lastProgress, 1.0);
}

// =============================================================================
// Data integrity test: write and read back full pipeline
// =============================================================================

TEST_F(EnsightExporterTest, DataIntegrityRoundtrip) {
    // Write a known 4x3x2 scalar field and verify every value
    auto image = FloatImage3D::New();
    FloatImage3D::RegionType region;
    FloatImage3D::SizeType size;
    size[0] = 4; size[1] = 3; size[2] = 2;
    region.SetSize(size);
    image->SetRegions(region);
    image->Allocate();

    float* buf = image->GetBufferPointer();
    for (int i = 0; i < 24; ++i) {
        buf[i] = static_cast<float>(i * 1.5);
    }

    auto varPath = testDir_ / "integrity.scalar";
    auto result = EnsightExporter::writeScalarVariable(
        varPath, "TestField", image.GetPointer());
    ASSERT_TRUE(result.has_value());

    // Read back all values
    std::ifstream in(varPath, std::ios::binary);
    readBinaryString(in);  // description
    readBinaryString(in);  // "part"
    readBinaryInt(in);     // part number
    readBinaryString(in);  // "block"

    for (int i = 0; i < 24; ++i) {
        float val = readBinaryFloat(in);
        EXPECT_FLOAT_EQ(val, static_cast<float>(i * 1.5))
            << "Mismatch at index " << i;
    }
}

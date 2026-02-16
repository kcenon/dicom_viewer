#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <vector>

#include "core/data_serializer.hpp"
#include "core/zip_archive.hpp"

#include <nlohmann/json.hpp>

using namespace dicom_viewer::core;
using FloatImage3D = DataSerializer::FloatImage3D;
using VectorImage3D = DataSerializer::VectorImage3D;
using LabelMapType = DataSerializer::LabelMapType;

namespace {

FloatImage3D::Pointer createScalarImage(int nx, int ny, int nz,
                                        double spacing = 1.5,
                                        float fillValue = 0.0f) {
    auto image = FloatImage3D::New();
    FloatImage3D::RegionType region;
    FloatImage3D::SizeType size;
    size[0] = nx; size[1] = ny; size[2] = nz;
    region.SetSize(size);
    image->SetRegions(region);

    FloatImage3D::SpacingType sp;
    sp[0] = spacing; sp[1] = spacing; sp[2] = spacing;
    image->SetSpacing(sp);

    FloatImage3D::PointType origin;
    origin[0] = -10.0; origin[1] = 5.0; origin[2] = 20.0;
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(fillValue);
    return image;
}

VectorImage3D::Pointer createVectorImage(int nx, int ny, int nz,
                                         float vx = 1.0f,
                                         float vy = 2.0f,
                                         float vz = 3.0f) {
    auto image = VectorImage3D::New();
    VectorImage3D::RegionType region;
    VectorImage3D::SizeType size;
    size[0] = nx; size[1] = ny; size[2] = nz;
    region.SetSize(size);
    image->SetRegions(region);
    image->SetNumberOfComponentsPerPixel(3);

    VectorImage3D::SpacingType sp;
    sp[0] = 2.0; sp[1] = 2.0; sp[2] = 2.0;
    image->SetSpacing(sp);

    VectorImage3D::PointType origin;
    origin[0] = -5.0; origin[1] = 0.0; origin[2] = 10.0;
    image->SetOrigin(origin);

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

LabelMapType::Pointer createLabelMap(int nx, int ny, int nz) {
    auto image = LabelMapType::New();
    LabelMapType::RegionType region;
    LabelMapType::SizeType size;
    size[0] = nx; size[1] = ny; size[2] = nz;
    region.SetSize(size);
    image->SetRegions(region);

    LabelMapType::SpacingType sp;
    sp[0] = 1.0; sp[1] = 1.0; sp[2] = 1.0;
    image->SetSpacing(sp);

    image->Allocate(true);  // Fill with 0

    // Add some labels
    auto* buf = image->GetBufferPointer();
    size_t total = static_cast<size_t>(nx) * ny * nz;
    for (size_t i = 0; i < total / 3; ++i) buf[i] = 1;
    for (size_t i = total / 3; i < total / 2; ++i) buf[i] = 2;

    return image;
}

}  // anonymous namespace

// =============================================================================
// Scalar image NRRD roundtrip
// =============================================================================

TEST(DataSerializer, ScalarImageNRRDRoundtrip) {
    auto original = createScalarImage(8, 6, 4);
    float* buf = original->GetBufferPointer();
    for (int i = 0; i < 8 * 6 * 4; ++i) {
        buf[i] = static_cast<float>(i * 0.1);
    }

    auto nrrd = DataSerializer::scalarImageToNRRD(original.GetPointer());
    EXPECT_GT(nrrd.size(), 0u);

    auto result = DataSerializer::nrrdToScalarImage(nrrd);
    ASSERT_TRUE(result.has_value());

    auto decoded = *result;
    auto origSize = original->GetLargestPossibleRegion().GetSize();
    auto decSize = decoded->GetLargestPossibleRegion().GetSize();

    EXPECT_EQ(decSize[0], origSize[0]);
    EXPECT_EQ(decSize[1], origSize[1]);
    EXPECT_EQ(decSize[2], origSize[2]);

    // Check spacing preserved
    EXPECT_DOUBLE_EQ(decoded->GetSpacing()[0], original->GetSpacing()[0]);
    EXPECT_DOUBLE_EQ(decoded->GetSpacing()[1], original->GetSpacing()[1]);

    // Check origin preserved
    EXPECT_DOUBLE_EQ(decoded->GetOrigin()[0], original->GetOrigin()[0]);
    EXPECT_DOUBLE_EQ(decoded->GetOrigin()[1], original->GetOrigin()[1]);
    EXPECT_DOUBLE_EQ(decoded->GetOrigin()[2], original->GetOrigin()[2]);

    // Check all voxel values
    const float* origBuf = original->GetBufferPointer();
    const float* decBuf = decoded->GetBufferPointer();
    for (int i = 0; i < 8 * 6 * 4; ++i) {
        EXPECT_FLOAT_EQ(decBuf[i], origBuf[i]) << "Mismatch at index " << i;
    }
}

TEST(DataSerializer, ScalarImageNRRDHeader) {
    auto image = createScalarImage(10, 20, 30);
    auto nrrd = DataSerializer::scalarImageToNRRD(image.GetPointer());

    // Header should contain NRRD magic and metadata
    std::string header(nrrd.begin(), nrrd.begin() + std::min(nrrd.size(), size_t{500}));
    EXPECT_NE(header.find("NRRD0004"), std::string::npos);
    EXPECT_NE(header.find("type: float"), std::string::npos);
    EXPECT_NE(header.find("dimension: 3"), std::string::npos);
    EXPECT_NE(header.find("sizes: 10 20 30"), std::string::npos);
    EXPECT_NE(header.find("encoding: raw"), std::string::npos);
}

// =============================================================================
// Vector image NRRD roundtrip
// =============================================================================

TEST(DataSerializer, VectorImageNRRDRoundtrip) {
    auto original = createVectorImage(6, 4, 3, 10.0f, -5.0f, 7.5f);

    auto nrrd = DataSerializer::vectorImageToNRRD(original.GetPointer());
    EXPECT_GT(nrrd.size(), 0u);

    auto result = DataSerializer::nrrdToVectorImage(nrrd);
    ASSERT_TRUE(result.has_value());

    auto decoded = *result;
    EXPECT_EQ(decoded->GetNumberOfComponentsPerPixel(), 3u);

    auto origSize = original->GetLargestPossibleRegion().GetSize();
    auto decSize = decoded->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(decSize[0], origSize[0]);
    EXPECT_EQ(decSize[1], origSize[1]);
    EXPECT_EQ(decSize[2], origSize[2]);

    // Check spacing preserved
    EXPECT_DOUBLE_EQ(decoded->GetSpacing()[0], original->GetSpacing()[0]);

    // Check all voxel values
    const float* origBuf = original->GetBufferPointer();
    const float* decBuf = decoded->GetBufferPointer();
    size_t total = 6 * 4 * 3 * 3;
    for (size_t i = 0; i < total; ++i) {
        EXPECT_FLOAT_EQ(decBuf[i], origBuf[i]) << "Mismatch at index " << i;
    }
}

TEST(DataSerializer, VectorImageNRRDHeader) {
    auto image = createVectorImage(8, 8, 8);
    auto nrrd = DataSerializer::vectorImageToNRRD(image.GetPointer());

    std::string header(nrrd.begin(), nrrd.begin() + std::min(nrrd.size(), size_t{500}));
    EXPECT_NE(header.find("dimension: 4"), std::string::npos);
    EXPECT_NE(header.find("sizes: 3 8 8 8"), std::string::npos);
}

// =============================================================================
// Label map NRRD roundtrip
// =============================================================================

TEST(DataSerializer, LabelMapNRRDRoundtrip) {
    auto original = createLabelMap(16, 16, 8);

    auto nrrd = DataSerializer::labelMapToNRRD(original.GetPointer());
    auto result = DataSerializer::nrrdToLabelMap(nrrd);
    ASSERT_TRUE(result.has_value());

    auto decoded = *result;
    auto origSize = original->GetLargestPossibleRegion().GetSize();
    auto decSize = decoded->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(decSize[0], origSize[0]);
    EXPECT_EQ(decSize[1], origSize[1]);
    EXPECT_EQ(decSize[2], origSize[2]);

    // Check all voxel values
    const uint8_t* origBuf = original->GetBufferPointer();
    const uint8_t* decBuf = decoded->GetBufferPointer();
    size_t total = 16 * 16 * 8;
    for (size_t i = 0; i < total; ++i) {
        EXPECT_EQ(decBuf[i], origBuf[i]) << "Mismatch at index " << i;
    }
}

TEST(DataSerializer, LabelMapNRRDHeader) {
    auto image = createLabelMap(32, 32, 32);
    auto nrrd = DataSerializer::labelMapToNRRD(image.GetPointer());

    std::string header(nrrd.begin(), nrrd.begin() + std::min(nrrd.size(), size_t{500}));
    EXPECT_NE(header.find("type: unsigned char"), std::string::npos);
    EXPECT_NE(header.find("dimension: 3"), std::string::npos);
}

// =============================================================================
// Invalid NRRD decoding
// =============================================================================

TEST(DataSerializer, InvalidNRRDReturnsError) {
    std::vector<uint8_t> garbage = {0, 1, 2, 3, 4, 5};
    auto result = DataSerializer::nrrdToScalarImage(garbage);
    EXPECT_FALSE(result.has_value());
}

TEST(DataSerializer, WrongTypeNRRDReturnsError) {
    // Create a valid label map NRRD (unsigned char) and try to decode as float
    auto labelMap = createLabelMap(4, 4, 4);
    auto nrrd = DataSerializer::labelMapToNRRD(labelMap.GetPointer());
    auto result = DataSerializer::nrrdToScalarImage(nrrd);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// ZIP velocity data roundtrip
// =============================================================================

TEST(DataSerializer, VelocityDataZipRoundtrip) {
    std::vector<VectorImage3D::Pointer> velocities;
    std::vector<FloatImage3D::Pointer> magnitudes;

    for (int p = 0; p < 3; ++p) {
        float v = static_cast<float>(p * 10 + 5);
        velocities.push_back(createVectorImage(8, 8, 4, v, v * 0.5f, v * 0.1f));
        magnitudes.push_back(createScalarImage(8, 8, 4, 1.5, v * 2.0f));
    }

    // Save to ZIP
    ZipArchive zip;
    auto saveResult = DataSerializer::saveVelocityData(zip, velocities, magnitudes);
    ASSERT_TRUE(saveResult.has_value());

    // Write ZIP to temp file and read back
    auto tmpPath = std::filesystem::temp_directory_path()
                   / "data_serializer_test_vel.flo";
    auto writeResult = zip.writeTo(tmpPath);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = ZipArchive::readFrom(tmpPath);
    ASSERT_TRUE(readResult.has_value());

    // Load from ZIP
    std::vector<VectorImage3D::Pointer> loadedVel;
    std::vector<FloatImage3D::Pointer> loadedMag;
    auto loadResult = DataSerializer::loadVelocityData(*readResult, loadedVel, loadedMag);
    ASSERT_TRUE(loadResult.has_value());

    EXPECT_EQ(loadedVel.size(), 3u);
    EXPECT_EQ(loadedMag.size(), 3u);

    // Verify first phase velocity values
    const float* origBuf = velocities[0]->GetBufferPointer();
    const float* loadBuf = loadedVel[0]->GetBufferPointer();
    size_t total = 8 * 8 * 4 * 3;
    for (size_t i = 0; i < total; ++i) {
        EXPECT_FLOAT_EQ(loadBuf[i], origBuf[i]);
    }

    std::filesystem::remove(tmpPath);
}

// =============================================================================
// ZIP mask roundtrip
// =============================================================================

TEST(DataSerializer, MaskZipRoundtrip) {
    auto labelMap = createLabelMap(16, 16, 8);

    std::vector<DataSerializer::LabelDefinition> labels = {
        {1, "Aorta", {1.0f, 0.0f, 0.0f}, 0.8f},
        {2, "Left Ventricle", {0.0f, 1.0f, 0.0f}, 0.7f}
    };

    ZipArchive zip;
    auto saveResult = DataSerializer::saveMask(
        zip, labelMap.GetPointer(), labels);
    ASSERT_TRUE(saveResult.has_value());

    auto tmpPath = std::filesystem::temp_directory_path()
                   / "data_serializer_test_mask.flo";
    auto writeResult = zip.writeTo(tmpPath);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = ZipArchive::readFrom(tmpPath);
    ASSERT_TRUE(readResult.has_value());

    LabelMapType::Pointer loadedMap;
    std::vector<DataSerializer::LabelDefinition> loadedLabels;
    auto loadResult = DataSerializer::loadMask(*readResult, loadedMap, loadedLabels);
    ASSERT_TRUE(loadResult.has_value());

    // Verify label map dimensions
    auto origSize = labelMap->GetLargestPossibleRegion().GetSize();
    auto loadSize = loadedMap->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(loadSize[0], origSize[0]);
    EXPECT_EQ(loadSize[1], origSize[1]);
    EXPECT_EQ(loadSize[2], origSize[2]);

    // Verify label definitions
    ASSERT_EQ(loadedLabels.size(), 2u);
    EXPECT_EQ(loadedLabels[0].id, 1);
    EXPECT_EQ(loadedLabels[0].name, "Aorta");
    EXPECT_FLOAT_EQ(loadedLabels[0].color[0], 1.0f);
    EXPECT_FLOAT_EQ(loadedLabels[0].opacity, 0.8f);
    EXPECT_EQ(loadedLabels[1].id, 2);
    EXPECT_EQ(loadedLabels[1].name, "Left Ventricle");

    // Verify voxel data
    const uint8_t* origBuf = labelMap->GetBufferPointer();
    const uint8_t* loadBuf = loadedMap->GetBufferPointer();
    for (size_t i = 0; i < 16 * 16 * 8; ++i) {
        EXPECT_EQ(loadBuf[i], origBuf[i]);
    }

    std::filesystem::remove(tmpPath);
}

// =============================================================================
// ZIP analysis results roundtrip
// =============================================================================

TEST(DataSerializer, AnalysisResultsZipRoundtrip) {
    nlohmann::json results = {
        {"flow_metrics", {
            {"mean_flow_rate", 42.5},
            {"peak_velocity", 120.3},
            {"cardiac_output", 5.2}
        }},
        {"hemodynamics", {
            {"mean_wss", 1.5},
            {"max_wss", 8.3},
            {"total_energy_loss", 0.0015},
            {"mean_kinetic_energy", 0.0082}
        }},
        {"measurements", {
            {"distances", {{{"id", 1}, {"value_mm", 25.3}}}},
            {"angles", {{{"id", 1}, {"value_deg", 45.0}}}}
        }}
    };

    ZipArchive zip;
    auto saveResult = DataSerializer::saveAnalysisResults(zip, results);
    ASSERT_TRUE(saveResult.has_value());

    auto tmpPath = std::filesystem::temp_directory_path()
                   / "data_serializer_test_analysis.flo";
    zip.writeTo(tmpPath);

    auto readResult = ZipArchive::readFrom(tmpPath);
    ASSERT_TRUE(readResult.has_value());

    auto loadResult = DataSerializer::loadAnalysisResults(*readResult);
    ASSERT_TRUE(loadResult.has_value());

    auto& loaded = *loadResult;
    EXPECT_DOUBLE_EQ(loaded["flow_metrics"]["mean_flow_rate"].get<double>(), 42.5);
    EXPECT_DOUBLE_EQ(loaded["hemodynamics"]["total_energy_loss"].get<double>(), 0.0015);
    EXPECT_EQ(loaded["measurements"]["distances"].size(), 1u);

    std::filesystem::remove(tmpPath);
}

// =============================================================================
// Full project file roundtrip (all data types)
// =============================================================================

TEST(DataSerializer, FullProjectRoundtrip) {
    auto tmpPath = std::filesystem::temp_directory_path()
                   / "data_serializer_full_roundtrip.flo";

    // Create diverse data
    std::vector<VectorImage3D::Pointer> velocities = {
        createVectorImage(8, 8, 4, 50.0f, 25.0f, 10.0f),
        createVectorImage(8, 8, 4, 60.0f, 30.0f, 15.0f)
    };
    std::vector<FloatImage3D::Pointer> magnitudes = {
        createScalarImage(8, 8, 4, 1.5, 100.0f),
        createScalarImage(8, 8, 4, 1.5, 120.0f)
    };
    auto mask = createLabelMap(8, 8, 4);
    std::vector<DataSerializer::LabelDefinition> labels = {
        {1, "Vessel", {1.0f, 0.0f, 0.0f}, 1.0f}
    };
    nlohmann::json analysis = {{"mean_wss", 2.5}};

    // Save all data to single .flo file
    {
        ZipArchive zip;
        ASSERT_TRUE(DataSerializer::saveVelocityData(zip, velocities, magnitudes).has_value());
        ASSERT_TRUE(DataSerializer::saveMask(zip, mask.GetPointer(), labels).has_value());
        ASSERT_TRUE(DataSerializer::saveAnalysisResults(zip, analysis).has_value());
        ASSERT_TRUE(zip.writeTo(tmpPath).has_value());
    }

    // Load back
    {
        auto readResult = ZipArchive::readFrom(tmpPath);
        ASSERT_TRUE(readResult.has_value());

        std::vector<VectorImage3D::Pointer> loadedVel;
        std::vector<FloatImage3D::Pointer> loadedMag;
        ASSERT_TRUE(DataSerializer::loadVelocityData(*readResult, loadedVel, loadedMag).has_value());
        EXPECT_EQ(loadedVel.size(), 2u);
        EXPECT_EQ(loadedMag.size(), 2u);

        LabelMapType::Pointer loadedMask;
        std::vector<DataSerializer::LabelDefinition> loadedLabels;
        ASSERT_TRUE(DataSerializer::loadMask(*readResult, loadedMask, loadedLabels).has_value());
        EXPECT_EQ(loadedLabels.size(), 1u);
        EXPECT_EQ(loadedLabels[0].name, "Vessel");

        auto loadedAnalysis = DataSerializer::loadAnalysisResults(*readResult);
        ASSERT_TRUE(loadedAnalysis.has_value());
        EXPECT_DOUBLE_EQ((*loadedAnalysis)["mean_wss"].get<double>(), 2.5);
    }

    std::filesystem::remove(tmpPath);
}

// =============================================================================
// Compression efficiency
// =============================================================================

TEST(DataSerializer, ZipCompressionReducesSize) {
    // A 64^3 mostly-zero label map should compress well
    auto mask = LabelMapType::New();
    LabelMapType::RegionType region;
    LabelMapType::SizeType size;
    size[0] = 64; size[1] = 64; size[2] = 64;
    region.SetSize(size);
    mask->SetRegions(region);
    mask->Allocate(true);  // All zeros

    // Label a small cube
    auto* buf = mask->GetBufferPointer();
    for (int z = 20; z < 40; z++)
        for (int y = 20; y < 40; y++)
            for (int x = 20; x < 40; x++)
                buf[z * 64 * 64 + y * 64 + x] = 1;

    std::vector<DataSerializer::LabelDefinition> labels = {{1, "ROI", {1,0,0}, 1}};

    ZipArchive zip;
    DataSerializer::saveMask(zip, mask.GetPointer(), labels);

    auto tmpPath = std::filesystem::temp_directory_path()
                   / "data_serializer_compression.flo";
    zip.writeTo(tmpPath);

    auto fileSize = std::filesystem::file_size(tmpPath);
    size_t rawSize = 64 * 64 * 64;  // 262144 bytes raw

    // ZIP with DEFLATE should compress mostly-zero data significantly
    EXPECT_LT(fileSize, rawSize / 2)
        << "Compressed .flo should be <50% of raw label map size"
        << "\n  Raw: " << rawSize << " bytes"
        << "\n  Compressed: " << fileSize << " bytes";

    std::filesystem::remove(tmpPath);
}

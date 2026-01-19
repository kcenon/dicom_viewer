#include "services/measurement/shape_analyzer.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace dicom_viewer::services {
namespace {

class ShapeAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple label map (30x30x30) - larger to avoid overlaps
        testLabelMap_ = LabelMapType::New();

        LabelMapType::SizeType size;
        size[0] = 30;
        size[1] = 30;
        size[2] = 30;

        LabelMapType::IndexType start;
        start.Fill(0);

        LabelMapType::RegionType region;
        region.SetSize(size);
        region.SetIndex(start);

        testLabelMap_->SetRegions(region);
        testLabelMap_->Allocate();
        testLabelMap_->FillBuffer(0);

        // Set spacing (1mm x 1mm x 1mm)
        LabelMapType::SpacingType spacing;
        spacing[0] = 1.0;
        spacing[1] = 1.0;
        spacing[2] = 1.0;
        testLabelMap_->SetSpacing(spacing);

        // Label 1: Cube (0-9, 0-9, 0-9) = 10x10x10 = 1000 voxels
        for (int z = 0; z <= 9; ++z) {
            for (int y = 0; y <= 9; ++y) {
                for (int x = 0; x <= 9; ++x) {
                    LabelMapType::IndexType idx = {x, y, z};
                    testLabelMap_->SetPixel(idx, 1);
                }
            }
        }

        // Label 2: Elongated cuboid (12-27, 12-15, 12-15) = 16x4x4 = 256 voxels
        for (int z = 12; z <= 15; ++z) {
            for (int y = 12; y <= 15; ++y) {
                for (int x = 12; x <= 27; ++x) {
                    LabelMapType::IndexType idx = {x, y, z};
                    testLabelMap_->SetPixel(idx, 2);
                }
            }
        }

        // Label 3: Flat disc (12-19, 20-27, 20-21) = 8x8x2 = 128 voxels
        for (int z = 20; z <= 21; ++z) {
            for (int y = 20; y <= 27; ++y) {
                for (int x = 12; x <= 19; ++x) {
                    LabelMapType::IndexType idx = {x, y, z};
                    testLabelMap_->SetPixel(idx, 3);
                }
            }
        }
    }

    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(testCsvPath_)) {
            std::filesystem::remove(testCsvPath_);
        }
    }

    using LabelMapType = ShapeAnalyzer::LabelMapType;
    using SpacingType = ShapeAnalyzer::SpacingType;

    LabelMapType::Pointer testLabelMap_;
    SpacingType testSpacing_ = {1.0, 1.0, 1.0};
    std::filesystem::path testCsvPath_ = std::filesystem::temp_directory_path() / "test_shape.csv";
};

// =============================================================================
// ShapeAnalysisResult struct tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, ResultDefaultValues) {
    ShapeAnalysisResult result;

    EXPECT_EQ(result.labelId, 0);
    EXPECT_TRUE(result.labelName.empty());
    EXPECT_EQ(result.voxelCount, 0);
    EXPECT_DOUBLE_EQ(result.volumeMm3, 0.0);
    EXPECT_FALSE(result.elongation.has_value());
    EXPECT_FALSE(result.flatness.has_value());
    EXPECT_FALSE(result.compactness.has_value());
    EXPECT_FALSE(result.roundness.has_value());
    EXPECT_FALSE(result.principalAxes.has_value());
}

TEST_F(ShapeAnalyzerTest, ResultToString) {
    ShapeAnalysisResult result;
    result.labelId = 1;
    result.labelName = "Tumor";
    result.voxelCount = 1000;
    result.volumeMm3 = 1000.0;
    result.elongation = 0.5;
    result.flatness = 0.3;
    result.compactness = 0.8;
    result.roundness = 0.7;

    std::string str = result.toString();

    EXPECT_NE(str.find("Tumor"), std::string::npos);
    EXPECT_NE(str.find("1000"), std::string::npos);
    EXPECT_NE(str.find("Elongation"), std::string::npos);
    EXPECT_NE(str.find("Flatness"), std::string::npos);
    EXPECT_NE(str.find("Compactness"), std::string::npos);
    EXPECT_NE(str.find("Roundness"), std::string::npos);
}

TEST_F(ShapeAnalyzerTest, ResultGetCsvHeader) {
    auto header = ShapeAnalysisResult::getCsvHeader();

    EXPECT_FALSE(header.empty());
    EXPECT_EQ(header[0], "LabelID");
    EXPECT_EQ(header[1], "LabelName");
    EXPECT_EQ(header[2], "VoxelCount");

    // Check for shape descriptor columns
    auto hasColumn = [&header](const std::string& name) {
        return std::find(header.begin(), header.end(), name) != header.end();
    };

    EXPECT_TRUE(hasColumn("Elongation"));
    EXPECT_TRUE(hasColumn("Flatness"));
    EXPECT_TRUE(hasColumn("Compactness"));
    EXPECT_TRUE(hasColumn("Roundness"));
}

TEST_F(ShapeAnalyzerTest, ResultGetCsvRow) {
    ShapeAnalysisResult result;
    result.labelId = 1;
    result.labelName = "TestLabel";
    result.voxelCount = 1000;
    result.volumeMm3 = 1000.0;

    auto row = result.getCsvRow();

    EXPECT_FALSE(row.empty());
    EXPECT_EQ(row[0], "1");
    EXPECT_EQ(row[1], "TestLabel");
    EXPECT_EQ(row[2], "1000");
}

// =============================================================================
// ShapeAnalysisError tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, ErrorSuccess) {
    ShapeAnalysisError error;
    EXPECT_TRUE(error.isSuccess());
    EXPECT_EQ(error.code, ShapeAnalysisError::Code::Success);
}

TEST_F(ShapeAnalyzerTest, ErrorToString) {
    ShapeAnalysisError error;
    error.code = ShapeAnalysisError::Code::InvalidLabelMap;
    error.message = "test message";

    std::string result = error.toString();
    EXPECT_NE(result.find("Invalid label map"), std::string::npos);
    EXPECT_NE(result.find("test message"), std::string::npos);
}

// =============================================================================
// ShapeAnalyzer basic tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzerDefaultConstruction) {
    ShapeAnalyzer analyzer;
    // Should not crash
}

TEST_F(ShapeAnalyzerTest, AnalyzerMoveConstruction) {
    ShapeAnalyzer analyzer1;
    ShapeAnalyzer analyzer2(std::move(analyzer1));
    // Should not crash
}

TEST_F(ShapeAnalyzerTest, AnalyzerNullLabelMapError) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(nullptr, 1, testSpacing_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ShapeAnalysisError::Code::InvalidLabelMap);
}

TEST_F(ShapeAnalyzerTest, AnalyzerBackgroundLabelError) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 0, testSpacing_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ShapeAnalysisError::Code::LabelNotFound);
}

TEST_F(ShapeAnalyzerTest, AnalyzerInvalidSpacingError) {
    ShapeAnalyzer analyzer;
    SpacingType invalidSpacing = {0.0, 1.0, 1.0};

    auto result = analyzer.analyze(testLabelMap_, 1, invalidSpacing);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ShapeAnalysisError::Code::InvalidSpacing);
}

TEST_F(ShapeAnalyzerTest, AnalyzerLabelNotFoundError) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 99, testSpacing_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ShapeAnalysisError::Code::LabelNotFound);
}

// =============================================================================
// Shape analysis tests - Cube (symmetric shape)
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzeCubeBasicMetrics) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_EQ(result->labelId, 1);
        EXPECT_EQ(result->voxelCount, 1000);  // 10x10x10 cube
        EXPECT_DOUBLE_EQ(result->volumeMm3, 1000.0);
    }
}

TEST_F(ShapeAnalyzerTest, AnalyzeCubeShapeDescriptors) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        // Cube should have low elongation (near spherical)
        EXPECT_TRUE(result->elongation.has_value());
        EXPECT_LT(result->elongation.value(), 0.3);

        // Cube should have low flatness
        EXPECT_TRUE(result->flatness.has_value());
        EXPECT_LT(result->flatness.value(), 0.3);

        // Cube should have high compactness (volume/bounding box volume)
        // Note: Can exceed 1.0 when using OBB since PCA axes create tighter bounds
        EXPECT_TRUE(result->compactness.has_value());
        EXPECT_GT(result->compactness.value(), 0.5);
    }
}

TEST_F(ShapeAnalyzerTest, AnalyzeCubePrincipalAxes) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result && result->principalAxes.has_value()) {
        const auto& axes = result->principalAxes.value();

        // Centroid should be near (4.5, 4.5, 4.5) - center of cube at (0-9, 0-9, 0-9)
        EXPECT_NEAR(axes.centroid[0], 4.5, 0.5);
        EXPECT_NEAR(axes.centroid[1], 4.5, 0.5);
        EXPECT_NEAR(axes.centroid[2], 4.5, 0.5);

        // Eigenvalues should be similar for a cube
        EXPECT_GT(axes.eigenvalues[0], 0.0);
        EXPECT_GT(axes.eigenvalues[1], 0.0);
        EXPECT_GT(axes.eigenvalues[2], 0.0);

        // All axes should have similar lengths
        double ratio12 = axes.axesLengths[1] / axes.axesLengths[0];
        double ratio23 = axes.axesLengths[2] / axes.axesLengths[1];
        EXPECT_GT(ratio12, 0.7);
        EXPECT_GT(ratio23, 0.7);
    }
}

// =============================================================================
// Shape analysis tests - Elongated shape
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzeElongatedShapeDescriptors) {
    ShapeAnalyzer analyzer;

    // Label 2 is 16x4x4 - elongated along X axis
    auto result = analyzer.analyze(testLabelMap_, 2, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_EQ(result->voxelCount, 256);  // 16x4x4

        // Should have high elongation (linear shape)
        EXPECT_TRUE(result->elongation.has_value());
        EXPECT_GT(result->elongation.value(), 0.5);

        // Should have low flatness (not disc-like)
        EXPECT_TRUE(result->flatness.has_value());
        EXPECT_LT(result->flatness.value(), 0.5);
    }
}

TEST_F(ShapeAnalyzerTest, AnalyzeElongatedPrincipalAxes) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 2, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result && result->principalAxes.has_value()) {
        const auto& axes = result->principalAxes.value();

        // Major axis should be significantly longer than others
        EXPECT_GT(axes.axesLengths[0], axes.axesLengths[1] * 1.5);

        // The major eigenvector should align with X axis (approximately)
        double xComponent = std::abs(axes.eigenvectors[0][0]);
        EXPECT_GT(xComponent, 0.7);  // Dominant X component
    }
}

// =============================================================================
// Shape analysis tests - Flat/disc shape
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzeFlatShapeDescriptors) {
    ShapeAnalyzer analyzer;

    // Label 3 is 8x8x2 - flat disc
    auto result = analyzer.analyze(testLabelMap_, 3, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_EQ(result->voxelCount, 128);  // 8x8x2

        // Should have moderate elongation
        EXPECT_TRUE(result->elongation.has_value());

        // Should have high flatness (disc-like)
        EXPECT_TRUE(result->flatness.has_value());
        EXPECT_GT(result->flatness.value(), 0.5);
    }
}

// =============================================================================
// Bounding box tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzeCubeAABB) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result && result->axisAlignedBoundingBox.has_value()) {
        const auto& aabb = result->axisAlignedBoundingBox.value();

        // AABB should be 10x10x10 mm
        EXPECT_DOUBLE_EQ(aabb.dimensions[0], 10.0);
        EXPECT_DOUBLE_EQ(aabb.dimensions[1], 10.0);
        EXPECT_DOUBLE_EQ(aabb.dimensions[2], 10.0);

        // Volume should be 1000 mm^3
        EXPECT_DOUBLE_EQ(aabb.volume, 1000.0);
    }
}

TEST_F(ShapeAnalyzerTest, AnalyzeElongatedAABB) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 2, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result && result->axisAlignedBoundingBox.has_value()) {
        const auto& aabb = result->axisAlignedBoundingBox.value();

        // AABB should be 16x4x4 mm
        EXPECT_DOUBLE_EQ(aabb.dimensions[0], 16.0);
        EXPECT_DOUBLE_EQ(aabb.dimensions[1], 4.0);
        EXPECT_DOUBLE_EQ(aabb.dimensions[2], 4.0);
    }
}

TEST_F(ShapeAnalyzerTest, AnalyzeCubeOBB) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.analyze(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result && result->orientedBoundingBox.has_value()) {
        const auto& obb = result->orientedBoundingBox.value();

        // OBB should have orientation vectors
        EXPECT_TRUE(obb.orientation.has_value());

        // OBB dimensions should be similar for a cube
        double maxDim = *std::max_element(obb.dimensions.begin(), obb.dimensions.end());
        double minDim = *std::min_element(obb.dimensions.begin(), obb.dimensions.end());
        EXPECT_GT(minDim / maxDim, 0.5);
    }
}

// =============================================================================
// AnalyzeAll tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzeAllLabels) {
    ShapeAnalyzer analyzer;

    auto results = analyzer.analyzeAll(testLabelMap_, testSpacing_);
    EXPECT_EQ(results.size(), 3);  // Labels 1, 2, and 3

    int successCount = 0;
    for (const auto& result : results) {
        if (result.has_value()) {
            ++successCount;
        }
    }
    EXPECT_EQ(successCount, 3);
}

TEST_F(ShapeAnalyzerTest, AnalyzeAllEmptyLabelMap) {
    ShapeAnalyzer analyzer;

    // Create empty label map
    auto emptyLabelMap = LabelMapType::New();
    emptyLabelMap->SetRegions(testLabelMap_->GetLargestPossibleRegion());
    emptyLabelMap->Allocate();
    emptyLabelMap->FillBuffer(0);

    auto results = analyzer.analyzeAll(emptyLabelMap, testSpacing_);
    EXPECT_TRUE(results.empty());
}

// =============================================================================
// Principal axes computation tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, ComputePrincipalAxesOnly) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.computePrincipalAxes(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_GT(result->eigenvalues[0], 0.0);
        EXPECT_GT(result->axesLengths[0], 0.0);

        // Eigenvalues should be in descending order
        EXPECT_GE(result->eigenvalues[0], result->eigenvalues[1]);
        EXPECT_GE(result->eigenvalues[1], result->eigenvalues[2]);
    }
}

TEST_F(ShapeAnalyzerTest, ComputePrincipalAxesInvalidLabel) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.computePrincipalAxes(testLabelMap_, 99, testSpacing_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ShapeAnalysisError::Code::LabelNotFound);
}

// =============================================================================
// OBB computation tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, ComputeOBBOnly) {
    ShapeAnalyzer analyzer;

    auto result = analyzer.computeOrientedBoundingBox(testLabelMap_, 1, testSpacing_);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_GT(result->volume, 0.0);
        EXPECT_TRUE(result->orientation.has_value());

        // All dimensions should be positive
        for (int i = 0; i < 3; ++i) {
            EXPECT_GT(result->dimensions[i], 0.0);
        }
    }
}

// =============================================================================
// Export tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, ExportToCsv) {
    ShapeAnalyzer analyzer;

    auto allResults = analyzer.analyzeAll(testLabelMap_, testSpacing_);

    std::vector<ShapeAnalysisResult> successfulResults;
    for (const auto& res : allResults) {
        if (res.has_value()) {
            successfulResults.push_back(res.value());
        }
    }

    auto exportResult = ShapeAnalyzer::exportToCsv(successfulResults, testCsvPath_);
    EXPECT_TRUE(exportResult.has_value());
    EXPECT_TRUE(std::filesystem::exists(testCsvPath_));

    // Verify file content
    std::ifstream file(testCsvPath_);
    int lineCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lineCount;
    }
    EXPECT_EQ(lineCount, 4);  // Header + 3 data rows
}

TEST_F(ShapeAnalyzerTest, ExportToCsvInvalidPath) {
    std::vector<ShapeAnalysisResult> results;
    ShapeAnalysisResult r1;
    results.push_back(r1);

    auto result = ShapeAnalyzer::exportToCsv(results, "/invalid/path/file.csv");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ShapeAnalysisError::Code::InternalError);
}

// =============================================================================
// Progress callback test
// =============================================================================

TEST_F(ShapeAnalyzerTest, ProgressCallback) {
    ShapeAnalyzer analyzer;

    int callCount = 0;
    double lastProgress = 0.0;

    analyzer.setProgressCallback([&](double progress) {
        ++callCount;
        lastProgress = progress;
    });

    auto unused = analyzer.analyzeAll(testLabelMap_, testSpacing_);
    (void)unused;

    EXPECT_EQ(callCount, 3);  // Three labels
    EXPECT_DOUBLE_EQ(lastProgress, 1.0);
}

// =============================================================================
// Options tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzeWithMinimalOptions) {
    ShapeAnalyzer analyzer;
    ShapeAnalysisOptions options;
    options.computeElongation = true;
    options.computeFlatness = false;
    options.computeCompactness = false;
    options.computeRoundness = false;
    options.computePrincipalAxes = false;
    options.computeAxisAlignedBoundingBox = false;
    options.computeOrientedBoundingBox = false;

    auto result = analyzer.analyze(testLabelMap_, 1, testSpacing_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_TRUE(result->elongation.has_value());
        EXPECT_FALSE(result->flatness.has_value());
        EXPECT_FALSE(result->compactness.has_value());
        EXPECT_FALSE(result->roundness.has_value());
        EXPECT_FALSE(result->principalAxes.has_value());
        EXPECT_FALSE(result->axisAlignedBoundingBox.has_value());
        EXPECT_FALSE(result->orientedBoundingBox.has_value());
    }
}

TEST_F(ShapeAnalyzerTest, AnalyzeWithOnlyAABB) {
    ShapeAnalyzer analyzer;
    ShapeAnalysisOptions options;
    options.computeElongation = false;
    options.computeFlatness = false;
    options.computeCompactness = false;
    options.computeRoundness = false;
    options.computePrincipalAxes = false;
    options.computeAxisAlignedBoundingBox = true;
    options.computeOrientedBoundingBox = false;

    auto result = analyzer.analyze(testLabelMap_, 1, testSpacing_, options);
    EXPECT_TRUE(result.has_value());

    if (result) {
        EXPECT_FALSE(result->elongation.has_value());
        EXPECT_TRUE(result->axisAlignedBoundingBox.has_value());
        EXPECT_FALSE(result->orientedBoundingBox.has_value());
    }
}

// =============================================================================
// Different spacing tests
// =============================================================================

TEST_F(ShapeAnalyzerTest, AnalyzeWithDifferentSpacing) {
    ShapeAnalyzer analyzer;
    SpacingType spacing = {0.5, 0.5, 2.0};

    auto result = analyzer.analyze(testLabelMap_, 1, spacing);
    EXPECT_TRUE(result.has_value());

    if (result) {
        // Volume = 1000 voxels * (0.5 * 0.5 * 2.0) = 500 mm^3
        EXPECT_DOUBLE_EQ(result->volumeMm3, 500.0);

        // With anisotropic spacing, the cube becomes elongated in Z
        // So elongation should be higher
        EXPECT_TRUE(result->elongation.has_value());
    }
}

}  // namespace
}  // namespace dicom_viewer::services

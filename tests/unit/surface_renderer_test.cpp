#include <gtest/gtest.h>

#include "services/surface_renderer.hpp"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>

using namespace dicom_viewer::services;

class SurfaceRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = std::make_unique<SurfaceRenderer>();
    }

    void TearDown() override {
        renderer.reset();
    }

    vtkSmartPointer<vtkImageData> createTestVolume(int dims = 64) {
        auto imageData = vtkSmartPointer<vtkImageData>::New();
        imageData->SetDimensions(dims, dims, dims);
        imageData->SetSpacing(1.0, 1.0, 1.0);
        imageData->SetOrigin(0.0, 0.0, 0.0);
        imageData->AllocateScalars(VTK_SHORT, 1);

        // Fill with sphere pattern for testing surface extraction
        short* ptr = static_cast<short*>(imageData->GetScalarPointer());
        double center = dims / 2.0;
        double radius = dims / 3.0;

        for (int z = 0; z < dims; ++z) {
            for (int y = 0; y < dims; ++y) {
                for (int x = 0; x < dims; ++x) {
                    int idx = z * dims * dims + y * dims + x;
                    double dist = std::sqrt(
                        std::pow(x - center, 2) +
                        std::pow(y - center, 2) +
                        std::pow(z - center, 2)
                    );
                    // Inside sphere: high value (bone-like), outside: low value
                    ptr[idx] = dist < radius ? 500 : -500;
                }
            }
        }

        return imageData;
    }

    std::unique_ptr<SurfaceRenderer> renderer;
};

// Test construction
TEST_F(SurfaceRendererTest, DefaultConstruction) {
    EXPECT_NE(renderer, nullptr);
    EXPECT_EQ(renderer->getSurfaceCount(), 0);
}

// Test move semantics
TEST_F(SurfaceRendererTest, MoveConstructor) {
    renderer->addPresetSurface(TissueType::Bone);
    SurfaceRenderer moved(std::move(*renderer));
    EXPECT_EQ(moved.getSurfaceCount(), 1);
}

TEST_F(SurfaceRendererTest, MoveAssignment) {
    renderer->addPresetSurface(TissueType::Bone);
    SurfaceRenderer other;
    other = std::move(*renderer);
    EXPECT_EQ(other.getSurfaceCount(), 1);
}

// Test input data
TEST_F(SurfaceRendererTest, SetInputDataAcceptsValidVolume) {
    auto volume = createTestVolume();
    EXPECT_NO_THROW(renderer->setInputData(volume));
}

TEST_F(SurfaceRendererTest, SetInputDataAcceptsNullptr) {
    EXPECT_NO_THROW(renderer->setInputData(nullptr));
}

// Test adding surfaces
TEST_F(SurfaceRendererTest, AddSurfaceWithConfig) {
    SurfaceConfig config;
    config.name = "Test Surface";
    config.isovalue = 100.0;
    config.color = {1.0, 0.0, 0.0};
    config.opacity = 0.8;

    size_t index = renderer->addSurface(config);
    EXPECT_EQ(index, 0);
    EXPECT_EQ(renderer->getSurfaceCount(), 1);
}

TEST_F(SurfaceRendererTest, AddPresetSurfaceBone) {
    size_t index = renderer->addPresetSurface(TissueType::Bone);
    EXPECT_EQ(index, 0);

    auto config = renderer->getSurfaceConfig(0);
    EXPECT_EQ(config.name, "Bone");
    EXPECT_GT(config.isovalue, 0.0);
}

TEST_F(SurfaceRendererTest, AddPresetSurfaceSoftTissue) {
    size_t index = renderer->addPresetSurface(TissueType::SoftTissue);
    EXPECT_EQ(index, 0);

    auto config = renderer->getSurfaceConfig(0);
    EXPECT_EQ(config.name, "Soft Tissue");
}

TEST_F(SurfaceRendererTest, AddPresetSurfaceSkin) {
    size_t index = renderer->addPresetSurface(TissueType::Skin);
    EXPECT_EQ(index, 0);

    auto config = renderer->getSurfaceConfig(0);
    EXPECT_EQ(config.name, "Skin");
    EXPECT_LT(config.isovalue, 0.0);  // Skin is typically negative HU
}

TEST_F(SurfaceRendererTest, AddMultipleSurfaces) {
    renderer->addPresetSurface(TissueType::Bone);
    renderer->addPresetSurface(TissueType::SoftTissue);
    renderer->addPresetSurface(TissueType::Skin);

    EXPECT_EQ(renderer->getSurfaceCount(), 3);
}

// Test removing surfaces
TEST_F(SurfaceRendererTest, RemoveSurface) {
    renderer->addPresetSurface(TissueType::Bone);
    renderer->addPresetSurface(TissueType::SoftTissue);

    EXPECT_EQ(renderer->getSurfaceCount(), 2);
    renderer->removeSurface(0);
    EXPECT_EQ(renderer->getSurfaceCount(), 1);

    // Remaining should be soft tissue
    auto config = renderer->getSurfaceConfig(0);
    EXPECT_EQ(config.name, "Soft Tissue");
}

TEST_F(SurfaceRendererTest, RemoveSurfaceInvalidIndex) {
    renderer->addPresetSurface(TissueType::Bone);
    EXPECT_NO_THROW(renderer->removeSurface(99));  // Should not crash
    EXPECT_EQ(renderer->getSurfaceCount(), 1);
}

TEST_F(SurfaceRendererTest, ClearSurfaces) {
    renderer->addPresetSurface(TissueType::Bone);
    renderer->addPresetSurface(TissueType::SoftTissue);
    renderer->addPresetSurface(TissueType::Skin);

    renderer->clearSurfaces();
    EXPECT_EQ(renderer->getSurfaceCount(), 0);
}

// Test surface configuration
TEST_F(SurfaceRendererTest, GetSurfaceConfigThrowsForInvalidIndex) {
    EXPECT_THROW(renderer->getSurfaceConfig(0), std::out_of_range);
}

TEST_F(SurfaceRendererTest, UpdateSurface) {
    renderer->addPresetSurface(TissueType::Bone);

    SurfaceConfig newConfig = SurfaceRenderer::getPresetSkin();
    renderer->updateSurface(0, newConfig);

    auto config = renderer->getSurfaceConfig(0);
    EXPECT_EQ(config.name, "Skin");
}

TEST_F(SurfaceRendererTest, UpdateSurfaceInvalidIndex) {
    EXPECT_NO_THROW(renderer->updateSurface(99, SurfaceConfig{}));
}

// Test visibility
TEST_F(SurfaceRendererTest, SetSurfaceVisibility) {
    renderer->addPresetSurface(TissueType::Bone);

    renderer->setSurfaceVisibility(0, false);
    auto config = renderer->getSurfaceConfig(0);
    EXPECT_FALSE(config.visible);

    renderer->setSurfaceVisibility(0, true);
    config = renderer->getSurfaceConfig(0);
    EXPECT_TRUE(config.visible);
}

TEST_F(SurfaceRendererTest, SetSurfaceVisibilityInvalidIndex) {
    EXPECT_NO_THROW(renderer->setSurfaceVisibility(99, true));
}

// Test color
TEST_F(SurfaceRendererTest, SetSurfaceColor) {
    renderer->addPresetSurface(TissueType::Bone);

    renderer->setSurfaceColor(0, 1.0, 0.0, 0.0);
    auto config = renderer->getSurfaceConfig(0);

    EXPECT_DOUBLE_EQ(config.color[0], 1.0);
    EXPECT_DOUBLE_EQ(config.color[1], 0.0);
    EXPECT_DOUBLE_EQ(config.color[2], 0.0);
}

TEST_F(SurfaceRendererTest, SetSurfaceColorInvalidIndex) {
    EXPECT_NO_THROW(renderer->setSurfaceColor(99, 1.0, 0.0, 0.0));
}

// Test opacity
TEST_F(SurfaceRendererTest, SetSurfaceOpacity) {
    renderer->addPresetSurface(TissueType::Bone);

    renderer->setSurfaceOpacity(0, 0.5);
    auto config = renderer->getSurfaceConfig(0);
    EXPECT_DOUBLE_EQ(config.opacity, 0.5);
}

TEST_F(SurfaceRendererTest, SetSurfaceOpacityInvalidIndex) {
    EXPECT_NO_THROW(renderer->setSurfaceOpacity(99, 0.5));
}

// Test quality settings
TEST_F(SurfaceRendererTest, SetSurfaceQualityLow) {
    renderer->addPresetSurface(TissueType::Bone);
    EXPECT_NO_THROW(renderer->setSurfaceQuality(SurfaceQuality::Low));
}

TEST_F(SurfaceRendererTest, SetSurfaceQualityMedium) {
    renderer->addPresetSurface(TissueType::Bone);
    EXPECT_NO_THROW(renderer->setSurfaceQuality(SurfaceQuality::Medium));
}

TEST_F(SurfaceRendererTest, SetSurfaceQualityHigh) {
    renderer->addPresetSurface(TissueType::Bone);
    EXPECT_NO_THROW(renderer->setSurfaceQuality(SurfaceQuality::High));
}

// Test actors
TEST_F(SurfaceRendererTest, GetActorReturnsValidActor) {
    renderer->addPresetSurface(TissueType::Bone);
    auto actor = renderer->getActor(0);
    EXPECT_NE(actor, nullptr);
}

TEST_F(SurfaceRendererTest, GetActorReturnsNullForInvalidIndex) {
    auto actor = renderer->getActor(99);
    EXPECT_EQ(actor, nullptr);
}

TEST_F(SurfaceRendererTest, GetAllActors) {
    renderer->addPresetSurface(TissueType::Bone);
    renderer->addPresetSurface(TissueType::SoftTissue);

    auto actors = renderer->getAllActors();
    EXPECT_EQ(actors.size(), 2);
}

TEST_F(SurfaceRendererTest, GetAllActorsEmpty) {
    auto actors = renderer->getAllActors();
    EXPECT_TRUE(actors.empty());
}

// Test renderer integration
TEST_F(SurfaceRendererTest, AddToRenderer) {
    auto vtkRen = vtkSmartPointer<vtkRenderer>::New();
    renderer->addPresetSurface(TissueType::Bone);

    EXPECT_NO_THROW(renderer->addToRenderer(vtkRen));
    EXPECT_EQ(vtkRen->GetActors()->GetNumberOfItems(), 1);
}

TEST_F(SurfaceRendererTest, AddToRendererNull) {
    renderer->addPresetSurface(TissueType::Bone);
    EXPECT_NO_THROW(renderer->addToRenderer(nullptr));
}

TEST_F(SurfaceRendererTest, RemoveFromRenderer) {
    auto vtkRen = vtkSmartPointer<vtkRenderer>::New();
    renderer->addPresetSurface(TissueType::Bone);
    renderer->addToRenderer(vtkRen);

    EXPECT_EQ(vtkRen->GetActors()->GetNumberOfItems(), 1);

    renderer->removeFromRenderer(vtkRen);
    EXPECT_EQ(vtkRen->GetActors()->GetNumberOfItems(), 0);
}

TEST_F(SurfaceRendererTest, RemoveFromRendererNull) {
    renderer->addPresetSurface(TissueType::Bone);
    EXPECT_NO_THROW(renderer->removeFromRenderer(nullptr));
}

// Test surface extraction
TEST_F(SurfaceRendererTest, ExtractSurfacesWithData) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);
    renderer->addPresetSurface(TissueType::Bone);

    EXPECT_NO_THROW(renderer->extractSurfaces());
}

TEST_F(SurfaceRendererTest, ExtractSurfacesWithoutData) {
    renderer->addPresetSurface(TissueType::Bone);
    EXPECT_NO_THROW(renderer->extractSurfaces());  // Should not crash
}

TEST_F(SurfaceRendererTest, ExtractSurfacesGeneratesTriangles) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);
    renderer->addPresetSurface(TissueType::Bone);
    renderer->extractSurfaces();

    auto data = renderer->getSurfaceData(0);
    EXPECT_GT(data.triangleCount, 0);
}

// Test surface data
TEST_F(SurfaceRendererTest, GetSurfaceDataAfterExtraction) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);
    renderer->addPresetSurface(TissueType::Bone);
    renderer->extractSurfaces();

    auto data = renderer->getSurfaceData(0);
    EXPECT_EQ(data.name, "Bone");
    EXPECT_NE(data.actor, nullptr);
    EXPECT_GT(data.triangleCount, 0);
    EXPECT_GT(data.surfaceArea, 0.0);
}

TEST_F(SurfaceRendererTest, GetSurfaceDataInvalidIndex) {
    auto data = renderer->getSurfaceData(99);
    EXPECT_TRUE(data.name.empty());
    EXPECT_EQ(data.actor, nullptr);
}

// Test update
TEST_F(SurfaceRendererTest, UpdateDoesNotThrow) {
    EXPECT_NO_THROW(renderer->update());
}

TEST_F(SurfaceRendererTest, UpdateWithData) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);
    renderer->addPresetSurface(TissueType::Bone);

    EXPECT_NO_THROW(renderer->update());
}

// Test preset configurations
TEST_F(SurfaceRendererTest, PresetBone) {
    auto config = SurfaceRenderer::getPresetBone();
    EXPECT_EQ(config.name, "Bone");
    EXPECT_GT(config.isovalue, 0.0);
    EXPECT_TRUE(config.smoothingEnabled);
    EXPECT_TRUE(config.decimationEnabled);
}

TEST_F(SurfaceRendererTest, PresetBoneHighDensity) {
    auto config = SurfaceRenderer::getPresetBoneHighDensity();
    EXPECT_EQ(config.name, "Bone (High Density)");
    EXPECT_GT(config.isovalue, SurfaceRenderer::getPresetBone().isovalue);
}

TEST_F(SurfaceRendererTest, PresetSoftTissue) {
    auto config = SurfaceRenderer::getPresetSoftTissue();
    EXPECT_EQ(config.name, "Soft Tissue");
    EXPECT_LT(config.opacity, 1.0);  // Should be somewhat transparent
}

TEST_F(SurfaceRendererTest, PresetSkin) {
    auto config = SurfaceRenderer::getPresetSkin();
    EXPECT_EQ(config.name, "Skin");
    EXPECT_LT(config.isovalue, 0.0);  // Skin is typically negative HU
}

TEST_F(SurfaceRendererTest, PresetLung) {
    auto config = SurfaceRenderer::getPresetLung();
    EXPECT_EQ(config.name, "Lung");
    EXPECT_LT(config.isovalue, -400.0);  // Lung is very negative HU
}

TEST_F(SurfaceRendererTest, PresetBloodVessels) {
    auto config = SurfaceRenderer::getPresetBloodVessels();
    EXPECT_EQ(config.name, "Blood Vessels");
    EXPECT_DOUBLE_EQ(config.color[0], 0.8);  // Red
}

// Test multi-surface rendering
TEST_F(SurfaceRendererTest, MultiSurfaceWithDifferentColors) {
    renderer->addPresetSurface(TissueType::Bone);
    renderer->addPresetSurface(TissueType::SoftTissue);

    auto boneConfig = renderer->getSurfaceConfig(0);
    auto tissueConfig = renderer->getSurfaceConfig(1);

    // Colors should be different
    EXPECT_NE(boneConfig.color[0], tissueConfig.color[0]);
}

TEST_F(SurfaceRendererTest, MultiSurfaceExtraction) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    renderer->addPresetSurface(TissueType::Bone);
    renderer->addPresetSurface(TissueType::Skin);

    EXPECT_NO_THROW(renderer->extractSurfaces());

    auto actors = renderer->getAllActors();
    EXPECT_EQ(actors.size(), 2);
}

// Test surface with smoothing disabled
TEST_F(SurfaceRendererTest, SurfaceWithSmoothingDisabled) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    SurfaceConfig config = SurfaceRenderer::getPresetBone();
    config.smoothingEnabled = false;
    renderer->addSurface(config);

    EXPECT_NO_THROW(renderer->extractSurfaces());
}

// Test surface with decimation disabled
TEST_F(SurfaceRendererTest, SurfaceWithDecimationDisabled) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    SurfaceConfig config = SurfaceRenderer::getPresetBone();
    config.decimationEnabled = false;
    renderer->addSurface(config);

    EXPECT_NO_THROW(renderer->extractSurfaces());
}

// Test surface with both smoothing and decimation disabled
TEST_F(SurfaceRendererTest, SurfaceWithAllProcessingDisabled) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    SurfaceConfig config = SurfaceRenderer::getPresetBone();
    config.smoothingEnabled = false;
    config.decimationEnabled = false;
    renderer->addSurface(config);

    EXPECT_NO_THROW(renderer->extractSurfaces());
}

// =============================================================================
// Error recovery and boundary tests (Issue #205)
// =============================================================================

TEST_F(SurfaceRendererTest, EmptyMeshInputDoesNotCrash) {
    // No input data set — extractSurfaces should handle gracefully
    EXPECT_NO_THROW(renderer->extractSurfaces());
    EXPECT_EQ(renderer->getSurfaceCount(), 0u);

    // Set null input then try extraction
    renderer->setInputData(nullptr);
    SurfaceConfig config = SurfaceRenderer::getPresetBone();
    renderer->addSurface(config);
    EXPECT_NO_THROW(renderer->extractSurfaces());
}

TEST_F(SurfaceRendererTest, LargeVolumeExtraction) {
    // 128³ volume — tests memory handling with larger marching cubes mesh
    auto largeVolume = createTestVolume(128);
    renderer->setInputData(largeVolume);

    SurfaceConfig config = SurfaceRenderer::getPresetBone();
    renderer->addSurface(config);

    EXPECT_NO_THROW(renderer->extractSurfaces());

    auto data = renderer->getSurfaceData(0);
    EXPECT_GT(data.triangleCount, 0u)
        << "128³ volume with sphere should generate triangles";
}

TEST_F(SurfaceRendererTest, SurfaceNormalsAfterModification) {
    auto volume = createTestVolume();
    renderer->setInputData(volume);

    SurfaceConfig config = SurfaceRenderer::getPresetBone();
    renderer->addSurface(config);
    renderer->extractSurfaces();

    auto dataBefore = renderer->getSurfaceData(0);
    size_t trianglesBefore = dataBefore.triangleCount;

    // Modify surface config (change isovalue) and re-extract
    config.isovalue = 100.0;  // Different threshold
    renderer->updateSurface(0, config);
    renderer->extractSurfaces();

    auto dataAfter = renderer->getSurfaceData(0);
    // Different isovalue should produce different mesh
    // (may have different triangle count)
    EXPECT_GT(dataAfter.triangleCount, 0u);
    // The mesh was re-extracted, so surface normals are recomputed
    EXPECT_NE(dataBefore.triangleCount, dataAfter.triangleCount)
        << "Different isovalue should produce different mesh";
}

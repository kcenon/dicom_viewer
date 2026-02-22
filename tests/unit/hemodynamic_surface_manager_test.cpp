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

#include "services/hemodynamic_surface_manager.hpp"
#include "services/surface_renderer.hpp"

#include <vtkFloatArray.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

using namespace dicom_viewer::services;

namespace {

/// Create a sphere mesh with a named per-vertex scalar array
vtkSmartPointer<vtkPolyData> createMeshWithArray(
    const std::string& arrayName, double maxVal, int numPoints = -1)
{
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetRadius(20.0);
    sphere->SetThetaResolution(12);
    sphere->SetPhiResolution(12);
    sphere->Update();

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->DeepCopy(sphere->GetOutput());

    auto nPts = polyData->GetNumberOfPoints();
    if (numPoints > 0 && numPoints < nPts) nPts = numPoints;

    auto scalars = vtkSmartPointer<vtkFloatArray>::New();
    scalars->SetName(arrayName.c_str());
    scalars->SetNumberOfComponents(1);
    scalars->SetNumberOfTuples(polyData->GetNumberOfPoints());

    for (vtkIdType i = 0; i < polyData->GetNumberOfPoints(); ++i) {
        float val = static_cast<float>(i) / static_cast<float>(polyData->GetNumberOfPoints()) * maxVal;
        scalars->SetValue(i, val);
    }

    polyData->GetPointData()->AddArray(scalars);
    polyData->GetPointData()->SetActiveScalars(arrayName.c_str());

    return polyData;
}

/// Create a mesh with multiple named scalar arrays
vtkSmartPointer<vtkPolyData> createMeshWithMultipleArrays()
{
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetRadius(20.0);
    sphere->SetThetaResolution(12);
    sphere->SetPhiResolution(12);
    sphere->Update();

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->DeepCopy(sphere->GetOutput());

    auto nPts = polyData->GetNumberOfPoints();

    // WSS array
    auto wss = vtkSmartPointer<vtkFloatArray>::New();
    wss->SetName("WSS");
    wss->SetNumberOfTuples(nPts);

    // OSI array
    auto osi = vtkSmartPointer<vtkFloatArray>::New();
    osi->SetName("OSI");
    osi->SetNumberOfTuples(nPts);

    // TAWSS array
    auto tawss = vtkSmartPointer<vtkFloatArray>::New();
    tawss->SetName("TAWSS");
    tawss->SetNumberOfTuples(nPts);

    for (vtkIdType i = 0; i < nPts; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(nPts);
        wss->SetValue(i, t * 5.0f);        // WSS in [0, 5] Pa
        osi->SetValue(i, t * 0.5f);        // OSI in [0, 0.5]
        tawss->SetValue(i, t * 3.0f);      // TAWSS in [0, 3] Pa
    }

    polyData->GetPointData()->AddArray(wss);
    polyData->GetPointData()->AddArray(osi);
    polyData->GetPointData()->AddArray(tawss);

    return polyData;
}

} // anonymous namespace

class HemodynamicSurfaceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = std::make_unique<SurfaceRenderer>();
        manager = std::make_unique<HemodynamicSurfaceManager>();
    }

    std::unique_ptr<SurfaceRenderer> renderer;
    std::unique_ptr<HemodynamicSurfaceManager> manager;
};

// =============================================================================
// Construction and defaults
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, DefaultConstruction) {
    EXPECT_FALSE(manager->wssIndex().has_value());
    EXPECT_FALSE(manager->osiIndex().has_value());
    EXPECT_FALSE(manager->afiIndex().has_value());
    EXPECT_FALSE(manager->rrtIndex().has_value());
}

TEST_F(HemodynamicSurfaceManagerTest, MoveConstruction) {
    auto mesh = createMeshWithArray("WSS", 5.0);
    manager->showWSS(*renderer, mesh, 5.0);

    HemodynamicSurfaceManager moved(std::move(*manager));
    EXPECT_TRUE(moved.wssIndex().has_value());
}

// =============================================================================
// WSS surface coloring
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, ShowWSS_AddsSurface) {
    auto mesh = createMeshWithArray("WSS", 5.0);

    auto idx = manager->showWSS(*renderer, mesh, 5.0);
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(renderer->getSurfaceCount(), 1);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowWSS_TracksIndex) {
    auto mesh = createMeshWithArray("WSS", 5.0);
    auto idx = manager->showWSS(*renderer, mesh, 5.0);

    ASSERT_TRUE(manager->wssIndex().has_value());
    EXPECT_EQ(manager->wssIndex().value(), idx);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowWSS_SetsScalarRange) {
    auto mesh = createMeshWithArray("WSS", 5.0);
    manager->showWSS(*renderer, mesh, 5.0);

    auto [minVal, maxVal] = renderer->surfaceScalarRange(0);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 5.0);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowWSS_HasValidActor) {
    auto mesh = createMeshWithArray("WSS", 5.0);
    auto idx = manager->showWSS(*renderer, mesh, 5.0);

    auto actor = renderer->getActor(idx);
    ASSERT_NE(actor, nullptr);
    EXPECT_NE(actor->GetMapper(), nullptr);
}

// =============================================================================
// OSI surface coloring
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, ShowOSI_AddsSurface) {
    auto mesh = createMeshWithArray("OSI", 0.5);

    auto idx = manager->showOSI(*renderer, mesh);
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(renderer->getSurfaceCount(), 1);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowOSI_TracksIndex) {
    auto mesh = createMeshWithArray("OSI", 0.5);
    auto idx = manager->showOSI(*renderer, mesh);

    ASSERT_TRUE(manager->osiIndex().has_value());
    EXPECT_EQ(manager->osiIndex().value(), idx);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowOSI_FixedRange) {
    auto mesh = createMeshWithArray("OSI", 0.5);
    manager->showOSI(*renderer, mesh);

    auto [minVal, maxVal] = renderer->surfaceScalarRange(0);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 0.5);
}

// =============================================================================
// AFI computation
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, ComputeAFI_NullInput) {
    auto result = HemodynamicSurfaceManager::computeAFI(nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(HemodynamicSurfaceManagerTest, ComputeAFI_NoTAWSSArray) {
    auto mesh = createMeshWithArray("WSS", 5.0);
    auto result = HemodynamicSurfaceManager::computeAFI(mesh);
    EXPECT_EQ(result, nullptr);
}

TEST_F(HemodynamicSurfaceManagerTest, ComputeAFI_ZeroTAWSS) {
    auto mesh = createMeshWithArray("TAWSS", 0.0);
    auto result = HemodynamicSurfaceManager::computeAFI(mesh);
    // All TAWSS values are 0 → mean is 0 → returns nullptr
    EXPECT_EQ(result, nullptr);
}

TEST_F(HemodynamicSurfaceManagerTest, ComputeAFI_ValidComputation) {
    auto mesh = createMeshWithArray("TAWSS", 4.0);
    auto result = HemodynamicSurfaceManager::computeAFI(mesh);
    ASSERT_NE(result, nullptr);

    // Check AFI array exists
    auto* afiArray = result->GetPointData()->GetArray("AFI");
    ASSERT_NE(afiArray, nullptr);
    EXPECT_EQ(afiArray->GetNumberOfTuples(), mesh->GetNumberOfPoints());
}

TEST_F(HemodynamicSurfaceManagerTest, ComputeAFI_MeanIsOne) {
    // With linear distribution [0, maxVal], mean AFI should be close to 1.0
    // (since AFI = TAWSS / mean(TAWSS))
    auto mesh = createMeshWithArray("TAWSS", 4.0);
    auto result = HemodynamicSurfaceManager::computeAFI(mesh);
    ASSERT_NE(result, nullptr);

    auto* afiArray = result->GetPointData()->GetArray("AFI");
    ASSERT_NE(afiArray, nullptr);

    // Compute mean AFI
    double sum = 0.0;
    auto n = afiArray->GetNumberOfTuples();
    for (vtkIdType i = 0; i < n; ++i) {
        sum += afiArray->GetComponent(i, 0);
    }
    double meanAFI = sum / static_cast<double>(n);
    EXPECT_NEAR(meanAFI, 1.0, 0.05);
}

TEST_F(HemodynamicSurfaceManagerTest, ComputeAFI_PreservesOriginalData) {
    auto mesh = createMeshWithArray("TAWSS", 4.0);
    auto result = HemodynamicSurfaceManager::computeAFI(mesh);
    ASSERT_NE(result, nullptr);

    // Original TAWSS array should still be present in the output
    auto* tawssArray = result->GetPointData()->GetArray("TAWSS");
    ASSERT_NE(tawssArray, nullptr);
    EXPECT_EQ(tawssArray->GetNumberOfTuples(), mesh->GetNumberOfPoints());
}

// =============================================================================
// AFI surface coloring
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, ShowAFI_AddsSurface) {
    auto mesh = createMeshWithArray("TAWSS", 4.0);
    auto idx = manager->showAFI(*renderer, mesh);

    EXPECT_EQ(renderer->getSurfaceCount(), 1);
    ASSERT_TRUE(manager->afiIndex().has_value());
    EXPECT_EQ(manager->afiIndex().value(), idx);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowAFI_SetsMinRangeToTwo) {
    auto mesh = createMeshWithArray("TAWSS", 4.0);
    manager->showAFI(*renderer, mesh);

    auto [minVal, maxVal] = renderer->surfaceScalarRange(0);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    // maxAFI should be at least 2.0
    EXPECT_GE(maxVal, 2.0);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowAFI_FallbackWithoutTAWSS) {
    // Mesh without TAWSS array — should still add a surface (fallback)
    auto mesh = createMeshWithArray("WSS", 5.0);
    auto idx = manager->showAFI(*renderer, mesh);

    EXPECT_EQ(renderer->getSurfaceCount(), 1);
    ASSERT_TRUE(manager->afiIndex().has_value());
    EXPECT_EQ(manager->afiIndex().value(), idx);
}

// =============================================================================
// RRT surface coloring
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, ShowRRT_AddsSurface) {
    auto mesh = createMeshWithArray("RRT", 100.0);

    auto idx = manager->showRRT(*renderer, mesh, 100.0);
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(renderer->getSurfaceCount(), 1);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowRRT_TracksIndex) {
    auto mesh = createMeshWithArray("RRT", 100.0);
    auto idx = manager->showRRT(*renderer, mesh, 100.0);

    ASSERT_TRUE(manager->rrtIndex().has_value());
    EXPECT_EQ(manager->rrtIndex().value(), idx);
}

TEST_F(HemodynamicSurfaceManagerTest, ShowRRT_SetsScalarRange) {
    auto mesh = createMeshWithArray("RRT", 100.0);
    manager->showRRT(*renderer, mesh, 100.0);

    auto [minVal, maxVal] = renderer->surfaceScalarRange(0);
    EXPECT_DOUBLE_EQ(minVal, 0.0);
    EXPECT_DOUBLE_EQ(maxVal, 100.0);
}

// =============================================================================
// Multiple parameters simultaneously
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, AllFourParameters) {
    auto multiMesh = createMeshWithMultipleArrays();

    auto wssIdx = manager->showWSS(*renderer, multiMesh, 5.0);
    auto osiIdx = manager->showOSI(*renderer, multiMesh);
    auto afiIdx = manager->showAFI(*renderer, multiMesh);

    auto rrtMesh = createMeshWithArray("RRT", 50.0);
    auto rrtIdx = manager->showRRT(*renderer, rrtMesh, 50.0);

    EXPECT_EQ(renderer->getSurfaceCount(), 4);

    ASSERT_TRUE(manager->wssIndex().has_value());
    ASSERT_TRUE(manager->osiIndex().has_value());
    ASSERT_TRUE(manager->afiIndex().has_value());
    ASSERT_TRUE(manager->rrtIndex().has_value());

    EXPECT_EQ(manager->wssIndex().value(), wssIdx);
    EXPECT_EQ(manager->osiIndex().value(), osiIdx);
    EXPECT_EQ(manager->afiIndex().value(), afiIdx);
    EXPECT_EQ(manager->rrtIndex().value(), rrtIdx);

    // All indices should be different
    EXPECT_NE(wssIdx, osiIdx);
    EXPECT_NE(osiIdx, afiIdx);
    EXPECT_NE(afiIdx, rrtIdx);
}

TEST_F(HemodynamicSurfaceManagerTest, IndependentVisibility) {
    auto mesh = createMeshWithArray("WSS", 5.0);
    auto wssIdx = manager->showWSS(*renderer, mesh, 5.0);

    auto osiMesh = createMeshWithArray("OSI", 0.5);
    auto osiIdx = manager->showOSI(*renderer, osiMesh);

    // Toggle WSS visibility independently
    renderer->setSurfaceVisibility(wssIdx, false);
    auto wssConfig = renderer->getSurfaceConfig(wssIdx);
    auto osiConfig = renderer->getSurfaceConfig(osiIdx);
    EXPECT_FALSE(wssConfig.visible);
    EXPECT_TRUE(osiConfig.visible);
}

// =============================================================================
// AFI Lookup Table (via SurfaceRenderer)
// =============================================================================

TEST_F(HemodynamicSurfaceManagerTest, AFILookupTable_ValidCreation) {
    auto lut = SurfaceRenderer::createAFILookupTable(2.0);
    ASSERT_NE(lut, nullptr);
    EXPECT_EQ(lut->GetNumberOfTableValues(), 256);

    auto range = lut->GetRange();
    EXPECT_DOUBLE_EQ(range[0], 0.0);
    EXPECT_DOUBLE_EQ(range[1], 2.0);
}

TEST_F(HemodynamicSurfaceManagerTest, AFILookupTable_GreenAtLow) {
    auto lut = SurfaceRenderer::createAFILookupTable(2.0);
    double rgba[4];
    lut->GetTableValue(0, rgba);

    // At min (0): should be green (r≈0, g≈0.8, b≈0)
    EXPECT_LT(rgba[0], 0.1);    // Low red
    EXPECT_GT(rgba[1], 0.7);    // High green
    EXPECT_LT(rgba[2], 0.1);    // Low blue
}

TEST_F(HemodynamicSurfaceManagerTest, AFILookupTable_YellowAtMid) {
    auto lut = SurfaceRenderer::createAFILookupTable(2.0);
    double rgba[4];
    lut->GetTableValue(128, rgba);

    // At middle (~AFI=1): should be yellow/greenish-yellow
    EXPECT_GT(rgba[0], 0.8);    // High red
    EXPECT_GT(rgba[1], 0.8);    // High green
    EXPECT_LT(rgba[2], 0.1);    // Low blue
}

TEST_F(HemodynamicSurfaceManagerTest, AFILookupTable_RedAtHigh) {
    auto lut = SurfaceRenderer::createAFILookupTable(2.0);
    double rgba[4];
    lut->GetTableValue(255, rgba);

    // At max (2.0): should be red
    EXPECT_NEAR(rgba[0], 1.0, 0.01);
    EXPECT_LT(rgba[1], 0.1);
    EXPECT_LT(rgba[2], 0.1);
}

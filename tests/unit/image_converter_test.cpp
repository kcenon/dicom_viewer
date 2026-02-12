#include "core/image_converter.hpp"

#include <gtest/gtest.h>
#include <cmath>

#include <itkImage.h>
#include <itkImageRegionIterator.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace dicom_viewer::core {
namespace {

// =============================================================================
// Helpers: Create synthetic ITK images with known values
// =============================================================================

using CTImageType = ImageConverter::CTImageType;
using MRImageType = ImageConverter::MRImageType;
using FloatImageType = ImageConverter::FloatImageType;
using MaskImageType = ImageConverter::MaskImageType;

template <typename ImageType>
typename ImageType::Pointer createTestImage(
    unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ,
    double spacingX = 1.0, double spacingY = 1.0, double spacingZ = 1.0,
    double originX = 0.0, double originY = 0.0, double originZ = 0.0)
{
    auto image = ImageType::New();

    typename ImageType::SizeType size;
    size[0] = sizeX;
    size[1] = sizeY;
    size[2] = sizeZ;

    typename ImageType::IndexType start;
    start.Fill(0);

    typename ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);
    image->SetRegions(region);

    typename ImageType::SpacingType spacing;
    spacing[0] = spacingX;
    spacing[1] = spacingY;
    spacing[2] = spacingZ;
    image->SetSpacing(spacing);

    typename ImageType::PointType origin;
    origin[0] = originX;
    origin[1] = originY;
    origin[2] = originZ;
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(0);

    return image;
}

template <typename ImageType>
void fillWithGradient(typename ImageType::Pointer image) {
    using IteratorType = itk::ImageRegionIterator<ImageType>;
    IteratorType it(image, image->GetLargestPossibleRegion());
    typename ImageType::PixelType val = 0;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        it.Set(val);
        ++val;
    }
}

template <typename ImageType>
void fillWithConstant(typename ImageType::Pointer image,
                      typename ImageType::PixelType value) {
    image->FillBuffer(value);
}

// =============================================================================
// ITK to VTK Conversion — CT (short)
// =============================================================================

class ItkToVtkCTTest : public ::testing::Test {
protected:
    void SetUp() override {
        image_ = createTestImage<CTImageType>(10, 12, 8, 0.5, 0.5, 2.0);
        fillWithGradient<CTImageType>(image_);
    }
    CTImageType::Pointer image_;
};

TEST_F(ItkToVtkCTTest, DimensionsPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    int dims[3];
    vtkImg->GetDimensions(dims);
    EXPECT_EQ(dims[0], 10);
    EXPECT_EQ(dims[1], 12);
    EXPECT_EQ(dims[2], 8);
}

TEST_F(ItkToVtkCTTest, SpacingPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    double spacing[3];
    vtkImg->GetSpacing(spacing);
    EXPECT_NEAR(spacing[0], 0.5, 1e-6);
    EXPECT_NEAR(spacing[1], 0.5, 1e-6);
    EXPECT_NEAR(spacing[2], 2.0, 1e-6);
}

TEST_F(ItkToVtkCTTest, OriginPreserved) {
    auto img = createTestImage<CTImageType>(4, 4, 4, 1.0, 1.0, 1.0,
                                            10.5, -20.3, 150.7);
    fillWithConstant<CTImageType>(img, 100);
    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    double origin[3];
    vtkImg->GetOrigin(origin);
    EXPECT_NEAR(origin[0], 10.5, 1e-6);
    EXPECT_NEAR(origin[1], -20.3, 1e-6);
    EXPECT_NEAR(origin[2], 150.7, 1e-6);
}

TEST_F(ItkToVtkCTTest, PixelValuesPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    // Check first few pixels match the gradient
    short* vtkData = static_cast<short*>(vtkImg->GetScalarPointer());
    ASSERT_NE(vtkData, nullptr);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(vtkData[0], image_->GetPixel(idx));

    idx = {1, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 1);
}

TEST_F(ItkToVtkCTTest, NegativePixelValuesPreserved) {
    auto img = createTestImage<CTImageType>(4, 4, 4);
    // CT images commonly have negative HU values
    fillWithConstant<CTImageType>(img, -1024);
    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    short* vtkData = static_cast<short*>(vtkImg->GetScalarPointer());
    EXPECT_EQ(vtkData[0], -1024);
}

TEST_F(ItkToVtkCTTest, TotalVoxelCountMatches) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    int numVoxels = vtkImg->GetNumberOfPoints();
    EXPECT_EQ(numVoxels, 10 * 12 * 8);
}

// =============================================================================
// ITK to VTK Conversion — MR (unsigned short)
// =============================================================================

class ItkToVtkMRTest : public ::testing::Test {
protected:
    void SetUp() override {
        image_ = createTestImage<MRImageType>(8, 8, 6, 0.9375, 0.9375, 3.0);
        fillWithGradient<MRImageType>(image_);
    }
    MRImageType::Pointer image_;
};

TEST_F(ItkToVtkMRTest, DimensionsPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    int dims[3];
    vtkImg->GetDimensions(dims);
    EXPECT_EQ(dims[0], 8);
    EXPECT_EQ(dims[1], 8);
    EXPECT_EQ(dims[2], 6);
}

TEST_F(ItkToVtkMRTest, SpacingPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    double spacing[3];
    vtkImg->GetSpacing(spacing);
    EXPECT_NEAR(spacing[0], 0.9375, 1e-6);
    EXPECT_NEAR(spacing[1], 0.9375, 1e-6);
    EXPECT_NEAR(spacing[2], 3.0, 1e-6);
}

TEST_F(ItkToVtkMRTest, PixelValuesPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    unsigned short* vtkData = static_cast<unsigned short*>(vtkImg->GetScalarPointer());
    ASSERT_NE(vtkData, nullptr);
    EXPECT_EQ(vtkData[0], 0);
    EXPECT_EQ(vtkData[1], 1);
}

TEST_F(ItkToVtkMRTest, HighIntensityPreserved) {
    auto img = createTestImage<MRImageType>(4, 4, 2);
    fillWithConstant<MRImageType>(img, 4095);  // 12-bit MR max
    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    unsigned short* vtkData = static_cast<unsigned short*>(vtkImg->GetScalarPointer());
    EXPECT_EQ(vtkData[0], 4095);
}

// =============================================================================
// ITK to VTK Conversion — Float
// =============================================================================

class ItkToVtkFloatTest : public ::testing::Test {
protected:
    void SetUp() override {
        image_ = createTestImage<FloatImageType>(6, 6, 4, 1.5, 1.5, 1.5);
        fillWithGradient<FloatImageType>(image_);
    }
    FloatImageType::Pointer image_;
};

TEST_F(ItkToVtkFloatTest, DimensionsPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    int dims[3];
    vtkImg->GetDimensions(dims);
    EXPECT_EQ(dims[0], 6);
    EXPECT_EQ(dims[1], 6);
    EXPECT_EQ(dims[2], 4);
}

TEST_F(ItkToVtkFloatTest, SpacingPreserved) {
    auto vtkImg = ImageConverter::itkToVtk(image_);
    ASSERT_NE(vtkImg, nullptr);

    double spacing[3];
    vtkImg->GetSpacing(spacing);
    EXPECT_NEAR(spacing[0], 1.5, 1e-6);
    EXPECT_NEAR(spacing[1], 1.5, 1e-6);
    EXPECT_NEAR(spacing[2], 1.5, 1e-6);
}

TEST_F(ItkToVtkFloatTest, FloatingPointPrecision) {
    auto img = createTestImage<FloatImageType>(2, 2, 2);
    FloatImageType::IndexType idx = {0, 0, 0};
    img->SetPixel(idx, 3.14159265f);
    idx = {1, 0, 0};
    img->SetPixel(idx, -273.15f);

    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    float* vtkData = static_cast<float*>(vtkImg->GetScalarPointer());
    EXPECT_FLOAT_EQ(vtkData[0], 3.14159265f);
    EXPECT_FLOAT_EQ(vtkData[1], -273.15f);
}

// =============================================================================
// VTK to ITK Round-Trip — CT (short)
// =============================================================================

class RoundTripCTTest : public ::testing::Test {
protected:
    void SetUp() override {
        original_ = createTestImage<CTImageType>(8, 10, 6, 0.488, 0.488, 2.5,
                                                 -100.0, -150.0, 50.0);
        fillWithGradient<CTImageType>(original_);
    }
    CTImageType::Pointer original_;
};

TEST_F(RoundTripCTTest, PixelValuesPreservedAfterRoundTrip) {
    auto vtkImg = ImageConverter::itkToVtk(original_);
    ASSERT_NE(vtkImg, nullptr);

    auto roundTrip = ImageConverter::vtkToItkCT(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    // Compare pixel values
    auto origSize = original_->GetLargestPossibleRegion().GetSize();
    auto rtSize = roundTrip->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(origSize[0], rtSize[0]);
    EXPECT_EQ(origSize[1], rtSize[1]);
    EXPECT_EQ(origSize[2], rtSize[2]);

    // Sample check: verify all pixels match
    using IteratorType = itk::ImageRegionIterator<CTImageType>;
    IteratorType origIt(original_, original_->GetLargestPossibleRegion());
    IteratorType rtIt(roundTrip, roundTrip->GetLargestPossibleRegion());

    for (origIt.GoToBegin(), rtIt.GoToBegin();
         !origIt.IsAtEnd() && !rtIt.IsAtEnd();
         ++origIt, ++rtIt)
    {
        EXPECT_EQ(origIt.Get(), rtIt.Get());
    }
}

TEST_F(RoundTripCTTest, SpacingPreservedAfterRoundTrip) {
    auto vtkImg = ImageConverter::itkToVtk(original_);
    auto roundTrip = ImageConverter::vtkToItkCT(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    auto origSpacing = original_->GetSpacing();
    auto rtSpacing = roundTrip->GetSpacing();

    EXPECT_NEAR(origSpacing[0], rtSpacing[0], 1e-6);
    EXPECT_NEAR(origSpacing[1], rtSpacing[1], 1e-6);
    EXPECT_NEAR(origSpacing[2], rtSpacing[2], 1e-6);
}

TEST_F(RoundTripCTTest, NegativeHUValuesPreservedAfterRoundTrip) {
    auto img = createTestImage<CTImageType>(4, 4, 4);
    fillWithConstant<CTImageType>(img, -500);

    auto vtkImg = ImageConverter::itkToVtk(img);
    auto roundTrip = ImageConverter::vtkToItkCT(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(roundTrip->GetPixel(idx), -500);

    idx = {3, 3, 3};
    EXPECT_EQ(roundTrip->GetPixel(idx), -500);
}

// =============================================================================
// VTK to ITK Round-Trip — Float
// =============================================================================

class RoundTripFloatTest : public ::testing::Test {
protected:
    void SetUp() override {
        original_ = createTestImage<FloatImageType>(6, 6, 4, 1.0, 1.0, 3.0,
                                                    50.0, 50.0, 0.0);
        fillWithGradient<FloatImageType>(original_);
    }
    FloatImageType::Pointer original_;
};

TEST_F(RoundTripFloatTest, PixelValuesPreservedAfterRoundTrip) {
    auto vtkImg = ImageConverter::itkToVtk(original_);
    ASSERT_NE(vtkImg, nullptr);

    auto roundTrip = ImageConverter::vtkToItkFloat(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    auto origSize = original_->GetLargestPossibleRegion().GetSize();
    auto rtSize = roundTrip->GetLargestPossibleRegion().GetSize();
    EXPECT_EQ(origSize[0], rtSize[0]);
    EXPECT_EQ(origSize[1], rtSize[1]);
    EXPECT_EQ(origSize[2], rtSize[2]);

    using IteratorType = itk::ImageRegionIterator<FloatImageType>;
    IteratorType origIt(original_, original_->GetLargestPossibleRegion());
    IteratorType rtIt(roundTrip, roundTrip->GetLargestPossibleRegion());

    for (origIt.GoToBegin(), rtIt.GoToBegin();
         !origIt.IsAtEnd() && !rtIt.IsAtEnd();
         ++origIt, ++rtIt)
    {
        EXPECT_FLOAT_EQ(origIt.Get(), rtIt.Get());
    }
}

TEST_F(RoundTripFloatTest, SpacingPreservedAfterRoundTrip) {
    auto vtkImg = ImageConverter::itkToVtk(original_);
    auto roundTrip = ImageConverter::vtkToItkFloat(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    auto origSpacing = original_->GetSpacing();
    auto rtSpacing = roundTrip->GetSpacing();

    EXPECT_NEAR(origSpacing[0], rtSpacing[0], 1e-6);
    EXPECT_NEAR(origSpacing[1], rtSpacing[1], 1e-6);
    EXPECT_NEAR(origSpacing[2], rtSpacing[2], 1e-6);
}

TEST_F(RoundTripFloatTest, NegativeFloatValuesPreserved) {
    auto img = createTestImage<FloatImageType>(3, 3, 3);
    FloatImageType::IndexType idx = {0, 0, 0};
    img->SetPixel(idx, -999.99f);
    idx = {1, 1, 1};
    img->SetPixel(idx, 0.001f);
    idx = {2, 2, 2};
    img->SetPixel(idx, 32767.5f);

    auto vtkImg = ImageConverter::itkToVtk(img);
    auto roundTrip = ImageConverter::vtkToItkFloat(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    idx = {0, 0, 0};
    EXPECT_FLOAT_EQ(roundTrip->GetPixel(idx), -999.99f);
    idx = {1, 1, 1};
    EXPECT_FLOAT_EQ(roundTrip->GetPixel(idx), 0.001f);
    idx = {2, 2, 2};
    EXPECT_FLOAT_EQ(roundTrip->GetPixel(idx), 32767.5f);
}

// =============================================================================
// HU Conversion (applyHUConversion)
// =============================================================================

class HUConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        image_ = createTestImage<CTImageType>(4, 4, 4);
    }
    CTImageType::Pointer image_;
};

TEST_F(HUConversionTest, IdentityConversion) {
    // slope=1, intercept=0 → values unchanged
    fillWithConstant<CTImageType>(image_, 500);
    ImageConverter::applyHUConversion(image_, 1.0, 0.0);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 500);
}

TEST_F(HUConversionTest, StandardCTRescale) {
    // Standard CT: slope=1, intercept=-1024
    // Raw value 1024 → HU = 1024 * 1 + (-1024) = 0 (water)
    fillWithConstant<CTImageType>(image_, 1024);
    ImageConverter::applyHUConversion(image_, 1.0, -1024.0);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 0);
}

TEST_F(HUConversionTest, AirHUValue) {
    // Air: raw=0, slope=1, intercept=-1024 → HU = -1024
    fillWithConstant<CTImageType>(image_, 0);
    ImageConverter::applyHUConversion(image_, 1.0, -1024.0);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), -1024);
}

TEST_F(HUConversionTest, BoneHUValue) {
    // Bone: raw=2024, slope=1, intercept=-1024 → HU = 1000
    fillWithConstant<CTImageType>(image_, 2024);
    ImageConverter::applyHUConversion(image_, 1.0, -1024.0);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 1000);
}

TEST_F(HUConversionTest, CustomSlopeAndIntercept) {
    // slope=2, intercept=100 → value 50 → 50*2 + 100 = 200
    fillWithConstant<CTImageType>(image_, 50);
    ImageConverter::applyHUConversion(image_, 2.0, 100.0);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 200);
}

TEST_F(HUConversionTest, FractionalSlopeRounding) {
    // slope=0.5, intercept=0 → value 3 → 3*0.5+0 = 1.5 → cast to short = 1
    fillWithConstant<CTImageType>(image_, 3);
    ImageConverter::applyHUConversion(image_, 0.5, 0.0);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), static_cast<short>(3 * 0.5));
}

TEST_F(HUConversionTest, AllVoxelsConverted) {
    // Verify every voxel is converted, not just the first
    fillWithConstant<CTImageType>(image_, 100);
    ImageConverter::applyHUConversion(image_, 1.0, -50.0);

    using IteratorType = itk::ImageRegionIterator<CTImageType>;
    IteratorType it(image_, image_->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        EXPECT_EQ(it.Get(), 50);
    }
}

TEST_F(HUConversionTest, GradientConversion) {
    // Apply to gradient: each voxel has unique value
    fillWithGradient<CTImageType>(image_);
    ImageConverter::applyHUConversion(image_, 1.0, 10.0);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 10);  // 0 * 1 + 10

    idx = {1, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 11);  // 1 * 1 + 10

    idx = {2, 0, 0};
    EXPECT_EQ(image_->GetPixel(idx), 12);  // 2 * 1 + 10
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(ImageConverterEdgeCase, SingleVoxelImage) {
    auto img = createTestImage<CTImageType>(1, 1, 1);
    CTImageType::IndexType idx = {0, 0, 0};
    img->SetPixel(idx, 42);

    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    int dims[3];
    vtkImg->GetDimensions(dims);
    EXPECT_EQ(dims[0], 1);
    EXPECT_EQ(dims[1], 1);
    EXPECT_EQ(dims[2], 1);

    short* data = static_cast<short*>(vtkImg->GetScalarPointer());
    EXPECT_EQ(data[0], 42);
}

TEST(ImageConverterEdgeCase, SingleVoxelRoundTrip) {
    auto img = createTestImage<CTImageType>(1, 1, 1, 0.5, 0.5, 5.0);
    CTImageType::IndexType idx = {0, 0, 0};
    img->SetPixel(idx, -1000);

    auto vtkImg = ImageConverter::itkToVtk(img);
    auto roundTrip = ImageConverter::vtkToItkCT(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    EXPECT_EQ(roundTrip->GetPixel(idx), -1000);
}

TEST(ImageConverterEdgeCase, NonIsotropicSpacing) {
    // Typical CT: fine in-plane, coarse axial
    auto img = createTestImage<CTImageType>(4, 4, 2, 0.3125, 0.3125, 5.0);
    fillWithConstant<CTImageType>(img, 100);

    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    double spacing[3];
    vtkImg->GetSpacing(spacing);
    EXPECT_NEAR(spacing[0], 0.3125, 1e-6);
    EXPECT_NEAR(spacing[1], 0.3125, 1e-6);
    EXPECT_NEAR(spacing[2], 5.0, 1e-6);
}

TEST(ImageConverterEdgeCase, NegativeOrigin) {
    auto img = createTestImage<CTImageType>(4, 4, 4, 1.0, 1.0, 1.0,
                                            -250.0, -250.0, -500.0);
    fillWithConstant<CTImageType>(img, 0);

    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    double origin[3];
    vtkImg->GetOrigin(origin);
    EXPECT_NEAR(origin[0], -250.0, 1e-6);
    EXPECT_NEAR(origin[1], -250.0, 1e-6);
    EXPECT_NEAR(origin[2], -500.0, 1e-6);
}

TEST(ImageConverterEdgeCase, MaxShortValue) {
    auto img = createTestImage<CTImageType>(2, 2, 2);
    fillWithConstant<CTImageType>(img, 32767);  // short max

    auto vtkImg = ImageConverter::itkToVtk(img);
    auto roundTrip = ImageConverter::vtkToItkCT(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(roundTrip->GetPixel(idx), 32767);
}

TEST(ImageConverterEdgeCase, MinShortValue) {
    auto img = createTestImage<CTImageType>(2, 2, 2);
    fillWithConstant<CTImageType>(img, -32768);  // short min

    auto vtkImg = ImageConverter::itkToVtk(img);
    auto roundTrip = ImageConverter::vtkToItkCT(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    CTImageType::IndexType idx = {0, 0, 0};
    EXPECT_EQ(roundTrip->GetPixel(idx), -32768);
}

TEST(ImageConverterEdgeCase, MaxUnsignedShortValue) {
    auto img = createTestImage<MRImageType>(2, 2, 2);
    fillWithConstant<MRImageType>(img, 65535);  // unsigned short max

    auto vtkImg = ImageConverter::itkToVtk(img);
    ASSERT_NE(vtkImg, nullptr);

    unsigned short* data = static_cast<unsigned short*>(vtkImg->GetScalarPointer());
    EXPECT_EQ(data[0], 65535);
}

TEST(ImageConverterEdgeCase, AllZeroImage) {
    auto img = createTestImage<CTImageType>(4, 4, 4);
    // Already zero-filled by createTestImage

    auto vtkImg = ImageConverter::itkToVtk(img);
    auto roundTrip = ImageConverter::vtkToItkCT(vtkImg);
    ASSERT_NE(roundTrip, nullptr);

    using IteratorType = itk::ImageRegionIterator<CTImageType>;
    IteratorType it(roundTrip, roundTrip->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        EXPECT_EQ(it.Get(), 0);
    }
}

TEST(ImageConverterEdgeCase, HUConversionOnSingleVoxel) {
    auto img = createTestImage<CTImageType>(1, 1, 1);
    CTImageType::IndexType idx = {0, 0, 0};
    img->SetPixel(idx, 1024);

    ImageConverter::applyHUConversion(img, 1.0, -1024.0);
    EXPECT_EQ(img->GetPixel(idx), 0);
}

// =============================================================================
// Cross-type consistency (CT vs Float for same spatial metadata)
// =============================================================================

TEST(ImageConverterCrossType, SameSpacingAcrossTypes) {
    auto ctImg = createTestImage<CTImageType>(4, 4, 4, 0.75, 0.75, 2.5);
    auto floatImg = createTestImage<FloatImageType>(4, 4, 4, 0.75, 0.75, 2.5);

    auto vtkCT = ImageConverter::itkToVtk(ctImg);
    auto vtkFloat = ImageConverter::itkToVtk(floatImg);

    double ctSpacing[3], floatSpacing[3];
    vtkCT->GetSpacing(ctSpacing);
    vtkFloat->GetSpacing(floatSpacing);

    EXPECT_NEAR(ctSpacing[0], floatSpacing[0], 1e-10);
    EXPECT_NEAR(ctSpacing[1], floatSpacing[1], 1e-10);
    EXPECT_NEAR(ctSpacing[2], floatSpacing[2], 1e-10);
}

TEST(ImageConverterCrossType, SameOriginAcrossTypes) {
    auto ctImg = createTestImage<CTImageType>(4, 4, 4, 1.0, 1.0, 1.0,
                                              -100.0, 50.0, 200.0);
    auto mrImg = createTestImage<MRImageType>(4, 4, 4, 1.0, 1.0, 1.0,
                                              -100.0, 50.0, 200.0);

    auto vtkCT = ImageConverter::itkToVtk(ctImg);
    auto vtkMR = ImageConverter::itkToVtk(mrImg);

    double ctOrigin[3], mrOrigin[3];
    vtkCT->GetOrigin(ctOrigin);
    vtkMR->GetOrigin(mrOrigin);

    EXPECT_NEAR(ctOrigin[0], mrOrigin[0], 1e-10);
    EXPECT_NEAR(ctOrigin[1], mrOrigin[1], 1e-10);
    EXPECT_NEAR(ctOrigin[2], mrOrigin[2], 1e-10);
}

}  // namespace
}  // namespace dicom_viewer::core

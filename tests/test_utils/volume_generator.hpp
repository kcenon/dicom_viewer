#pragma once

/// @file volume_generator.hpp
/// @brief Synthetic volume generators for performance and stress testing
///
/// Creates deterministic ITK images at various sizes for benchmarking
/// image processing operations. All generators produce platform-independent,
/// reproducible data.

#include <cmath>
#include <random>

#include <itkImage.h>
#include <itkImageRegionIterator.h>

namespace dicom_viewer::test_utils {

/// Supported image types for volume generation
using ShortImageType = itk::Image<short, 3>;
using UCharImageType = itk::Image<unsigned char, 3>;
using FloatImageType = itk::Image<float, 3>;

/// Create an isotropic volume of given cubic dimension
/// @param cubicSize Side length in voxels (e.g. 64 produces 64x64x64)
/// @param spacing Voxel spacing in mm (default 1.0mm isotropic)
/// @return Allocated ITK image filled with zeros
inline ShortImageType::Pointer createVolume(int cubicSize,
                                            double spacing = 1.0) {
    auto image = ShortImageType::New();

    ShortImageType::SizeType size;
    size[0] = cubicSize;
    size[1] = cubicSize;
    size[2] = cubicSize;

    ShortImageType::IndexType start;
    start.Fill(0);

    ShortImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    image->SetRegions(region);

    const double sp[3] = {spacing, spacing, spacing};
    image->SetSpacing(sp);

    const double origin[3] = {0.0, 0.0, 0.0};
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(0);

    return image;
}

/// Create a volume with a central sphere of given intensity
/// @param cubicSize Side length in voxels
/// @param sphereRadius Radius of the sphere in voxels
/// @param sphereValue Intensity value inside the sphere
/// @param backgroundValue Intensity value outside the sphere
/// @param spacing Voxel spacing in mm
/// @return Volume with embedded sphere
inline ShortImageType::Pointer createSphereVolume(
    int cubicSize,
    double sphereRadius,
    short sphereValue = 1000,
    short backgroundValue = 0,
    double spacing = 1.0) {
    auto image = createVolume(cubicSize, spacing);
    image->FillBuffer(backgroundValue);

    double center = static_cast<double>(cubicSize) / 2.0;
    double radiusSq = sphereRadius * sphereRadius;

    itk::ImageRegionIterator<ShortImageType> it(
        image, image->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        double dx = static_cast<double>(idx[0]) - center;
        double dy = static_cast<double>(idx[1]) - center;
        double dz = static_cast<double>(idx[2]) - center;

        if (dx * dx + dy * dy + dz * dz <= radiusSq) {
            it.Set(sphereValue);
        }
    }

    return image;
}

/// Create a volume with synthetic CT-like tissue distribution
/// @param cubicSize Side length in voxels
/// @param spacing Voxel spacing in mm
/// @return Volume with gradient and embedded structures
inline ShortImageType::Pointer createSyntheticCTVolume(
    int cubicSize,
    double spacing = 1.0) {
    auto image = createVolume(cubicSize, spacing);

    double center = static_cast<double>(cubicSize) / 2.0;
    std::mt19937 rng(42);  // Fixed seed for determinism
    std::normal_distribution<double> noise(0.0, 10.0);

    itk::ImageRegionIterator<ShortImageType> it(
        image, image->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        double dx = static_cast<double>(idx[0]) - center;
        double dy = static_cast<double>(idx[1]) - center;
        double dz = static_cast<double>(idx[2]) - center;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        short value;
        if (dist < center * 0.3) {
            // Inner region: soft tissue (~40 HU)
            value = static_cast<short>(40.0 + noise(rng));
        } else if (dist < center * 0.6) {
            // Middle region: muscle (~60 HU)
            value = static_cast<short>(60.0 + noise(rng));
        } else if (dist < center * 0.8) {
            // Outer region: fat (~-80 HU)
            value = static_cast<short>(-80.0 + noise(rng));
        } else {
            // Background: air (~-1000 HU)
            value = -1000;
        }

        it.Set(value);
    }

    return image;
}

/// Create a binary mask volume with a sphere
/// @param cubicSize Side length in voxels
/// @param sphereRadius Sphere radius in voxels
/// @param spacing Voxel spacing in mm
/// @return Binary mask with foreground=1, background=0
inline UCharImageType::Pointer createBinaryMaskVolume(
    int cubicSize,
    double sphereRadius,
    double spacing = 1.0) {
    auto image = UCharImageType::New();

    UCharImageType::SizeType size;
    size[0] = cubicSize;
    size[1] = cubicSize;
    size[2] = cubicSize;

    UCharImageType::IndexType start;
    start.Fill(0);

    UCharImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    image->SetRegions(region);

    const double sp[3] = {spacing, spacing, spacing};
    image->SetSpacing(sp);
    const double origin[3] = {0.0, 0.0, 0.0};
    image->SetOrigin(origin);

    image->Allocate();
    image->FillBuffer(0);

    double centerCoord = static_cast<double>(cubicSize) / 2.0;
    double radiusSq = sphereRadius * sphereRadius;

    itk::ImageRegionIterator<UCharImageType> it(image, region);

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        double dx = static_cast<double>(idx[0]) - centerCoord;
        double dy = static_cast<double>(idx[1]) - centerCoord;
        double dz = static_cast<double>(idx[2]) - centerCoord;

        if (dx * dx + dy * dy + dz * dz <= radiusSq) {
            it.Set(1);
        }
    }

    return image;
}

}  // namespace dicom_viewer::test_utils

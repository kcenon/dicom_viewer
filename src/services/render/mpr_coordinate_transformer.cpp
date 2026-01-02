#include "services/mpr_coordinate_transformer.hpp"

#include <algorithm>
#include <cmath>

namespace dicom_viewer::services {

class MPRCoordinateTransformer::Impl {
public:
    vtkSmartPointer<vtkImageData> imageData;
    std::array<int, 3> dimensions = {0, 0, 0};
    std::array<double, 3> spacing = {1.0, 1.0, 1.0};
    std::array<double, 3> origin = {0.0, 0.0, 0.0};

    void updateFromImageData() {
        if (!imageData) {
            dimensions = {0, 0, 0};
            spacing = {1.0, 1.0, 1.0};
            origin = {0.0, 0.0, 0.0};
            return;
        }

        int* dims = imageData->GetDimensions();
        dimensions = {dims[0], dims[1], dims[2]};

        double* sp = imageData->GetSpacing();
        spacing = {sp[0], sp[1], sp[2]};

        double* org = imageData->GetOrigin();
        origin = {org[0], org[1], org[2]};
    }
};

MPRCoordinateTransformer::MPRCoordinateTransformer()
    : impl_(std::make_unique<Impl>()) {}

MPRCoordinateTransformer::~MPRCoordinateTransformer() = default;

MPRCoordinateTransformer::MPRCoordinateTransformer(MPRCoordinateTransformer&&) noexcept = default;
MPRCoordinateTransformer& MPRCoordinateTransformer::operator=(MPRCoordinateTransformer&&) noexcept = default;

void MPRCoordinateTransformer::setImageData(vtkSmartPointer<vtkImageData> imageData) {
    impl_->imageData = imageData;
    impl_->updateFromImageData();
}

std::array<int, 3> MPRCoordinateTransformer::getDimensions() const {
    return impl_->dimensions;
}

std::array<double, 3> MPRCoordinateTransformer::getSpacing() const {
    return impl_->spacing;
}

std::array<double, 3> MPRCoordinateTransformer::getOrigin() const {
    return impl_->origin;
}

std::optional<VolumeCoordinate>
MPRCoordinateTransformer::screenToVolume(const ScreenCoordinate& screen,
                                          MPRPlane plane,
                                          double slicePosition) const {
    if (!impl_->imageData) {
        return std::nullopt;
    }

    VolumeCoordinate result;

    // Transform based on MPR plane orientation
    // Screen coordinates are in the local 2D coordinate system of the resliced image
    // We need to map them to 3D volume coordinates
    switch (plane) {
        case MPRPlane::Axial:
            // Axial: XY plane, screen (x,y) -> volume (x,y), Z from slicePosition
            result.x = screen.x;
            result.y = screen.y;
            result.z = slicePosition;
            break;

        case MPRPlane::Coronal:
            // Coronal: XZ plane, screen (x,y) -> volume (x,z), Y from slicePosition
            result.x = screen.x;
            result.y = slicePosition;
            result.z = screen.y;
            break;

        case MPRPlane::Sagittal:
            // Sagittal: YZ plane, screen (x,y) -> volume (y,z), X from slicePosition
            result.x = slicePosition;
            result.y = screen.x;
            result.z = screen.y;
            break;
    }

    return result;
}

std::optional<ScreenCoordinate>
MPRCoordinateTransformer::volumeToScreen(const VolumeCoordinate& volume,
                                          MPRPlane plane) const {
    if (!impl_->imageData) {
        return std::nullopt;
    }

    ScreenCoordinate result;

    switch (plane) {
        case MPRPlane::Axial:
            // Axial: volume (x,y) -> screen (x,y)
            result.x = volume.x;
            result.y = volume.y;
            break;

        case MPRPlane::Coronal:
            // Coronal: volume (x,z) -> screen (x,y)
            result.x = volume.x;
            result.y = volume.z;
            break;

        case MPRPlane::Sagittal:
            // Sagittal: volume (y,z) -> screen (x,y)
            result.x = volume.y;
            result.y = volume.z;
            break;
    }

    return result;
}

VoxelIndex MPRCoordinateTransformer::volumeToVoxel(const VolumeCoordinate& volume) const {
    VoxelIndex result;

    // Convert world coordinates to voxel indices
    result.i = static_cast<int>(std::round(
        (volume.x - impl_->origin[0]) / impl_->spacing[0]));
    result.j = static_cast<int>(std::round(
        (volume.y - impl_->origin[1]) / impl_->spacing[1]));
    result.k = static_cast<int>(std::round(
        (volume.z - impl_->origin[2]) / impl_->spacing[2]));

    return result;
}

VolumeCoordinate MPRCoordinateTransformer::voxelToVolume(const VoxelIndex& voxel) const {
    VolumeCoordinate result;

    // Convert voxel indices to world coordinates (center of voxel)
    result.x = impl_->origin[0] + voxel.i * impl_->spacing[0];
    result.y = impl_->origin[1] + voxel.j * impl_->spacing[1];
    result.z = impl_->origin[2] + voxel.k * impl_->spacing[2];

    return result;
}

std::optional<VoxelIndex>
MPRCoordinateTransformer::screenToVoxel(const ScreenCoordinate& screen,
                                         MPRPlane plane,
                                         double slicePosition) const {
    auto volumeCoord = screenToVolume(screen, plane, slicePosition);
    if (!volumeCoord) {
        return std::nullopt;
    }

    return volumeToVoxel(*volumeCoord);
}

int MPRCoordinateTransformer::getSliceIndex(MPRPlane plane, double worldPosition) const {
    int axisIndex = 0;
    switch (plane) {
        case MPRPlane::Axial:
            axisIndex = 2;  // Z axis
            break;
        case MPRPlane::Coronal:
            axisIndex = 1;  // Y axis
            break;
        case MPRPlane::Sagittal:
            axisIndex = 0;  // X axis
            break;
    }

    int sliceIndex = static_cast<int>(std::round(
        (worldPosition - impl_->origin[axisIndex]) / impl_->spacing[axisIndex]));

    // Clamp to valid range
    return std::clamp(sliceIndex, 0, impl_->dimensions[axisIndex] - 1);
}

double MPRCoordinateTransformer::getWorldPosition(MPRPlane plane, int sliceIndex) const {
    int axisIndex = 0;
    switch (plane) {
        case MPRPlane::Axial:
            axisIndex = 2;  // Z axis
            break;
        case MPRPlane::Coronal:
            axisIndex = 1;  // Y axis
            break;
        case MPRPlane::Sagittal:
            axisIndex = 0;  // X axis
            break;
    }

    return impl_->origin[axisIndex] + sliceIndex * impl_->spacing[axisIndex];
}

std::pair<int, int> MPRCoordinateTransformer::getSliceRange(MPRPlane plane) const {
    int axisIndex = 0;
    switch (plane) {
        case MPRPlane::Axial:
            axisIndex = 2;  // Z axis
            break;
        case MPRPlane::Coronal:
            axisIndex = 1;  // Y axis
            break;
        case MPRPlane::Sagittal:
            axisIndex = 0;  // X axis
            break;
    }

    return {0, impl_->dimensions[axisIndex] - 1};
}

} // namespace dicom_viewer::services

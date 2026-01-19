#include "services/mpr_coordinate_transformer.hpp"

namespace dicom_viewer::services {

MPRCoordinateTransformer::MPRCoordinateTransformer()
    : impl_(std::make_unique<coordinate::MPRCoordinateTransformer>()) {}

MPRCoordinateTransformer::~MPRCoordinateTransformer() = default;

MPRCoordinateTransformer::MPRCoordinateTransformer(MPRCoordinateTransformer&&) noexcept = default;
MPRCoordinateTransformer& MPRCoordinateTransformer::operator=(MPRCoordinateTransformer&&) noexcept = default;

void MPRCoordinateTransformer::setImageData(vtkSmartPointer<vtkImageData> imageData) {
    impl_->setImageData(imageData);
}

std::array<int, 3> MPRCoordinateTransformer::getDimensions() const {
    return impl_->getDimensions();
}

std::array<double, 3> MPRCoordinateTransformer::getSpacing() const {
    return impl_->getSpacing();
}

std::array<double, 3> MPRCoordinateTransformer::getOrigin() const {
    return impl_->getOrigin();
}

std::optional<VolumeCoordinate>
MPRCoordinateTransformer::screenToVolume(const ScreenCoordinate& screen,
                                          MPRPlane plane,
                                          double slicePosition) const {
    return impl_->screenToWorld(screen, plane, slicePosition);
}

std::optional<ScreenCoordinate>
MPRCoordinateTransformer::volumeToScreen(const VolumeCoordinate& volume,
                                          MPRPlane plane) const {
    return impl_->worldToScreen(volume, plane);
}

VoxelIndex MPRCoordinateTransformer::volumeToVoxel(const VolumeCoordinate& volume) const {
    return impl_->worldToVoxel(volume);
}

VolumeCoordinate MPRCoordinateTransformer::voxelToVolume(const VoxelIndex& voxel) const {
    return impl_->voxelToWorld(voxel);
}

std::optional<VoxelIndex>
MPRCoordinateTransformer::screenToVoxel(const ScreenCoordinate& screen,
                                         MPRPlane plane,
                                         double slicePosition) const {
    return impl_->screenToVoxel(screen, plane, slicePosition);
}

int MPRCoordinateTransformer::getSliceIndex(MPRPlane plane, double worldPosition) const {
    return impl_->getSliceIndex(plane, worldPosition);
}

double MPRCoordinateTransformer::getWorldPosition(MPRPlane plane, int sliceIndex) const {
    return impl_->getWorldPosition(plane, sliceIndex);
}

std::pair<int, int> MPRCoordinateTransformer::getSliceRange(MPRPlane plane) const {
    return impl_->getSliceRange(plane);
}

}  // namespace dicom_viewer::services

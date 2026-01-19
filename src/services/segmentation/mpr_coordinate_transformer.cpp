#include "services/segmentation/mpr_coordinate_transformer.hpp"

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

std::optional<MPRCoordinateTransformer::Index3D>
MPRCoordinateTransformer::worldToIndex(double worldX, double worldY, double worldZ) const {
    auto result = impl_->worldToVoxel(worldX, worldY, worldZ);
    if (!result) {
        return std::nullopt;
    }
    return Index3D{result->i, result->j, result->k};
}

MPRCoordinateTransformer::WorldPoint3D
MPRCoordinateTransformer::indexToWorld(const Index3D& index) const {
    auto result = impl_->voxelToWorld(coordinate::VoxelIndex{index.x, index.y, index.z});
    return WorldPoint3D{result.x, result.y, result.z};
}

std::optional<MPRCoordinateTransformer::Index3D>
MPRCoordinateTransformer::planeCoordToIndex(
    MPRPlane plane, int x, int y, double slicePosition) const {

    auto result = impl_->planeCoordToVoxel(plane, x, y, slicePosition);
    if (!result) {
        return std::nullopt;
    }
    return Index3D{result->i, result->j, result->k};
}

std::optional<Point2D>
MPRCoordinateTransformer::indexToPlaneCoord(MPRPlane plane, const Index3D& index) const {
    auto result = impl_->voxelToPlaneCoord(
        plane, coordinate::VoxelIndex{index.x, index.y, index.z});
    if (!result) {
        return std::nullopt;
    }
    return Point2D{result->x, result->y};
}

int MPRCoordinateTransformer::worldPositionToSliceIndex(
    MPRPlane plane, double worldPosition) const {
    return impl_->getSliceIndex(plane, worldPosition);
}

double MPRCoordinateTransformer::sliceIndexToWorldPosition(
    MPRPlane plane, int sliceIndex) const {
    return impl_->getWorldPosition(plane, sliceIndex);
}

std::optional<MPRCoordinateTransformer::SegmentationCoordinates>
MPRCoordinateTransformer::transformForSegmentation(
    MPRPlane plane, int viewX, int viewY, double slicePosition) const {

    auto result = impl_->transformForSegmentation(plane, viewX, viewY, slicePosition);
    if (!result) {
        return std::nullopt;
    }

    SegmentationCoordinates coords;
    coords.point2D = Point2D{result->point2D.x, result->point2D.y};
    coords.sliceIndex = result->sliceIndex;
    coords.index3D = Index3D{result->index3D.i, result->index3D.j, result->index3D.k};

    return coords;
}

std::pair<int, int> MPRCoordinateTransformer::getSliceRange(MPRPlane plane) const {
    return impl_->getSliceRange(plane);
}

bool MPRCoordinateTransformer::isValidIndex(const Index3D& index) const {
    return impl_->isValidVoxel(coordinate::VoxelIndex{index.x, index.y, index.z});
}

std::array<int, 3> MPRCoordinateTransformer::getPlaneAxisMapping(MPRPlane plane) const {
    return impl_->getPlaneAxisMapping(plane);
}

}  // namespace dicom_viewer::services

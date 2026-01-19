#include "services/coordinate/mpr_coordinate_transformer.hpp"

#include <algorithm>
#include <cmath>

namespace dicom_viewer::services::coordinate {

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

    [[nodiscard]] int getAxisIndex(MPRPlane plane) const {
        switch (plane) {
            case MPRPlane::Axial:    return 2;  // Z axis
            case MPRPlane::Coronal:  return 1;  // Y axis
            case MPRPlane::Sagittal: return 0;  // X axis
        }
        return 2;
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

// ==================== World ↔ Voxel Transformations ====================

std::optional<VoxelIndex> MPRCoordinateTransformer::worldToVoxel(
    double worldX, double worldY, double worldZ) const {

    if (!impl_->imageData) {
        return std::nullopt;
    }

    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;
    const auto& dims = impl_->dimensions;

    double ix = (worldX - origin[0]) / spacing[0];
    double iy = (worldY - origin[1]) / spacing[1];
    double iz = (worldZ - origin[2]) / spacing[2];

    int i = static_cast<int>(std::round(ix));
    int j = static_cast<int>(std::round(iy));
    int k = static_cast<int>(std::round(iz));

    if (i < 0 || i >= dims[0] ||
        j < 0 || j >= dims[1] ||
        k < 0 || k >= dims[2]) {
        return std::nullopt;
    }

    return VoxelIndex{i, j, k};
}

VoxelIndex MPRCoordinateTransformer::worldToVoxel(const WorldCoordinate& world) const {
    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;

    return VoxelIndex{
        static_cast<int>(std::round((world.x - origin[0]) / spacing[0])),
        static_cast<int>(std::round((world.y - origin[1]) / spacing[1])),
        static_cast<int>(std::round((world.z - origin[2]) / spacing[2]))
    };
}

WorldCoordinate MPRCoordinateTransformer::voxelToWorld(const VoxelIndex& voxel) const {
    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;

    return WorldCoordinate{
        origin[0] + voxel.i * spacing[0],
        origin[1] + voxel.j * spacing[1],
        origin[2] + voxel.k * spacing[2]
    };
}

// ==================== Screen ↔ World Transformations ====================

std::optional<WorldCoordinate> MPRCoordinateTransformer::screenToWorld(
    const ScreenCoordinate& screen,
    MPRPlane plane,
    double slicePosition) const {

    if (!impl_->imageData) {
        return std::nullopt;
    }

    WorldCoordinate result;

    switch (plane) {
        case MPRPlane::Axial:
            result.x = screen.x;
            result.y = screen.y;
            result.z = slicePosition;
            break;

        case MPRPlane::Coronal:
            result.x = screen.x;
            result.y = slicePosition;
            result.z = screen.y;
            break;

        case MPRPlane::Sagittal:
            result.x = slicePosition;
            result.y = screen.x;
            result.z = screen.y;
            break;
    }

    return result;
}

std::optional<ScreenCoordinate> MPRCoordinateTransformer::worldToScreen(
    const WorldCoordinate& world,
    MPRPlane plane) const {

    if (!impl_->imageData) {
        return std::nullopt;
    }

    ScreenCoordinate result;

    switch (plane) {
        case MPRPlane::Axial:
            result.x = world.x;
            result.y = world.y;
            break;

        case MPRPlane::Coronal:
            result.x = world.x;
            result.y = world.z;
            break;

        case MPRPlane::Sagittal:
            result.x = world.y;
            result.y = world.z;
            break;
    }

    return result;
}

// ==================== Screen ↔ Voxel Transformations ====================

std::optional<VoxelIndex> MPRCoordinateTransformer::screenToVoxel(
    const ScreenCoordinate& screen,
    MPRPlane plane,
    double slicePosition) const {

    auto world = screenToWorld(screen, plane, slicePosition);
    if (!world) {
        return std::nullopt;
    }

    return worldToVoxel(*world);
}

// ==================== Plane Coordinate ↔ Voxel Transformations ====================

std::optional<VoxelIndex> MPRCoordinateTransformer::planeCoordToVoxel(
    MPRPlane plane, int x, int y, double slicePosition) const {

    if (!impl_->imageData) {
        return std::nullopt;
    }

    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;

    VoxelIndex voxel;

    switch (plane) {
        case MPRPlane::Axial:
            voxel.i = x;
            voxel.j = y;
            voxel.k = static_cast<int>(std::round((slicePosition - origin[2]) / spacing[2]));
            break;

        case MPRPlane::Coronal:
            voxel.i = x;
            voxel.j = static_cast<int>(std::round((slicePosition - origin[1]) / spacing[1]));
            voxel.k = y;
            break;

        case MPRPlane::Sagittal:
            voxel.i = static_cast<int>(std::round((slicePosition - origin[0]) / spacing[0]));
            voxel.j = x;
            voxel.k = y;
            break;
    }

    if (!isValidVoxel(voxel)) {
        return std::nullopt;
    }

    return voxel;
}

std::optional<Point2D> MPRCoordinateTransformer::voxelToPlaneCoord(
    MPRPlane plane, const VoxelIndex& voxel) const {

    if (!isValidVoxel(voxel)) {
        return std::nullopt;
    }

    Point2D point;

    switch (plane) {
        case MPRPlane::Axial:
            point.x = voxel.i;
            point.y = voxel.j;
            break;

        case MPRPlane::Coronal:
            point.x = voxel.i;
            point.y = voxel.k;
            break;

        case MPRPlane::Sagittal:
            point.x = voxel.j;
            point.y = voxel.k;
            break;
    }

    return point;
}

// ==================== Slice Index Operations ====================

int MPRCoordinateTransformer::getSliceIndex(MPRPlane plane, double worldPosition) const {
    int axisIndex = impl_->getAxisIndex(plane);

    int sliceIndex = static_cast<int>(std::round(
        (worldPosition - impl_->origin[axisIndex]) / impl_->spacing[axisIndex]));

    return std::clamp(sliceIndex, 0, impl_->dimensions[axisIndex] - 1);
}

double MPRCoordinateTransformer::getWorldPosition(MPRPlane plane, int sliceIndex) const {
    int axisIndex = impl_->getAxisIndex(plane);
    return impl_->origin[axisIndex] + sliceIndex * impl_->spacing[axisIndex];
}

std::pair<int, int> MPRCoordinateTransformer::getSliceRange(MPRPlane plane) const {
    int axisIndex = impl_->getAxisIndex(plane);
    return {0, impl_->dimensions[axisIndex] - 1};
}

// ==================== Segmentation Support ====================

std::optional<SegmentationCoordinates> MPRCoordinateTransformer::transformForSegmentation(
    MPRPlane plane, int viewX, int viewY, double slicePosition) const {

    auto voxel = planeCoordToVoxel(plane, viewX, viewY, slicePosition);
    if (!voxel) {
        return std::nullopt;
    }

    SegmentationCoordinates coords;
    coords.index3D = *voxel;

    switch (plane) {
        case MPRPlane::Axial:
            coords.point2D = Point2D{voxel->i, voxel->j};
            coords.sliceIndex = voxel->k;
            break;

        case MPRPlane::Coronal:
            coords.point2D = Point2D{voxel->i, voxel->k};
            coords.sliceIndex = voxel->j;
            break;

        case MPRPlane::Sagittal:
            coords.point2D = Point2D{voxel->j, voxel->k};
            coords.sliceIndex = voxel->i;
            break;
    }

    return coords;
}

std::array<int, 3> MPRCoordinateTransformer::getPlaneAxisMapping(MPRPlane plane) const {
    switch (plane) {
        case MPRPlane::Axial:    return {0, 1, 2};  // H=X, V=Y, Slice=Z
        case MPRPlane::Coronal:  return {0, 2, 1};  // H=X, V=Z, Slice=Y
        case MPRPlane::Sagittal: return {1, 2, 0};  // H=Y, V=Z, Slice=X
    }
    return {0, 1, 2};
}

// ==================== Validation ====================

bool MPRCoordinateTransformer::isValidVoxel(const VoxelIndex& voxel) const {
    const auto& dims = impl_->dimensions;

    return voxel.i >= 0 && voxel.i < dims[0] &&
           voxel.j >= 0 && voxel.j < dims[1] &&
           voxel.k >= 0 && voxel.k < dims[2];
}

}  // namespace dicom_viewer::services::coordinate

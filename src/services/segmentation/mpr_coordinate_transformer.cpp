#include "services/segmentation/mpr_coordinate_transformer.hpp"

#include <algorithm>
#include <cmath>

namespace dicom_viewer::services {

class MPRCoordinateTransformer::Impl {
public:
    vtkSmartPointer<vtkImageData> imageData;

    // Cached image properties
    std::array<int, 3> dimensions = {0, 0, 0};
    std::array<double, 3> spacing = {1.0, 1.0, 1.0};
    std::array<double, 3> origin = {0.0, 0.0, 0.0};

    void updateCachedProperties() {
        if (imageData) {
            int* dims = imageData->GetDimensions();
            dimensions = {dims[0], dims[1], dims[2]};

            double* sp = imageData->GetSpacing();
            spacing = {sp[0], sp[1], sp[2]};

            double* orig = imageData->GetOrigin();
            origin = {orig[0], orig[1], orig[2]};
        } else {
            dimensions = {0, 0, 0};
            spacing = {1.0, 1.0, 1.0};
            origin = {0.0, 0.0, 0.0};
        }
    }
};

MPRCoordinateTransformer::MPRCoordinateTransformer()
    : impl_(std::make_unique<Impl>()) {}

MPRCoordinateTransformer::~MPRCoordinateTransformer() = default;

MPRCoordinateTransformer::MPRCoordinateTransformer(MPRCoordinateTransformer&&) noexcept = default;
MPRCoordinateTransformer& MPRCoordinateTransformer::operator=(MPRCoordinateTransformer&&) noexcept = default;

void MPRCoordinateTransformer::setImageData(vtkSmartPointer<vtkImageData> imageData) {
    impl_->imageData = imageData;
    impl_->updateCachedProperties();
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

std::optional<MPRCoordinateTransformer::Index3D>
MPRCoordinateTransformer::worldToIndex(double worldX, double worldY, double worldZ) const {
    if (!impl_->imageData) {
        return std::nullopt;
    }

    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;
    const auto& dims = impl_->dimensions;

    // Convert world coordinates to continuous index
    double ix = (worldX - origin[0]) / spacing[0];
    double iy = (worldY - origin[1]) / spacing[1];
    double iz = (worldZ - origin[2]) / spacing[2];

    // Round to nearest integer index
    int x = static_cast<int>(std::round(ix));
    int y = static_cast<int>(std::round(iy));
    int z = static_cast<int>(std::round(iz));

    // Check bounds
    if (x < 0 || x >= dims[0] ||
        y < 0 || y >= dims[1] ||
        z < 0 || z >= dims[2]) {
        return std::nullopt;
    }

    return Index3D{x, y, z};
}

MPRCoordinateTransformer::WorldPoint3D
MPRCoordinateTransformer::indexToWorld(const Index3D& index) const {
    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;

    return WorldPoint3D{
        origin[0] + index.x * spacing[0],
        origin[1] + index.y * spacing[1],
        origin[2] + index.z * spacing[2]
    };
}

std::optional<MPRCoordinateTransformer::Index3D>
MPRCoordinateTransformer::planeCoordToIndex(
    MPRPlane plane, int x, int y, double slicePosition) const {

    if (!impl_->imageData) {
        return std::nullopt;
    }

    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;
    const auto& dims = impl_->dimensions;

    Index3D index;

    switch (plane) {
        case MPRPlane::Axial:
            // Axial: X maps to X, Y maps to Y, slice is Z
            index.x = x;
            index.y = y;
            index.z = static_cast<int>(std::round((slicePosition - origin[2]) / spacing[2]));
            break;

        case MPRPlane::Coronal:
            // Coronal: X maps to X, Y maps to Z, slice is Y
            index.x = x;
            index.y = static_cast<int>(std::round((slicePosition - origin[1]) / spacing[1]));
            index.z = y;
            break;

        case MPRPlane::Sagittal:
            // Sagittal: X maps to Y, Y maps to Z, slice is X
            index.x = static_cast<int>(std::round((slicePosition - origin[0]) / spacing[0]));
            index.y = x;
            index.z = y;
            break;
    }

    // Validate bounds
    if (!isValidIndex(index)) {
        return std::nullopt;
    }

    return index;
}

std::optional<Point2D>
MPRCoordinateTransformer::indexToPlaneCoord(MPRPlane plane, const Index3D& index) const {
    if (!isValidIndex(index)) {
        return std::nullopt;
    }

    Point2D point;

    switch (plane) {
        case MPRPlane::Axial:
            // Axial: X→X, Y→Y
            point.x = index.x;
            point.y = index.y;
            break;

        case MPRPlane::Coronal:
            // Coronal: X→X, Z→Y
            point.x = index.x;
            point.y = index.z;
            break;

        case MPRPlane::Sagittal:
            // Sagittal: Y→X, Z→Y
            point.x = index.y;
            point.y = index.z;
            break;
    }

    return point;
}

int MPRCoordinateTransformer::worldPositionToSliceIndex(
    MPRPlane plane, double worldPosition) const {

    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;

    switch (plane) {
        case MPRPlane::Axial:
            return static_cast<int>(std::round((worldPosition - origin[2]) / spacing[2]));
        case MPRPlane::Coronal:
            return static_cast<int>(std::round((worldPosition - origin[1]) / spacing[1]));
        case MPRPlane::Sagittal:
            return static_cast<int>(std::round((worldPosition - origin[0]) / spacing[0]));
    }

    return 0;
}

double MPRCoordinateTransformer::sliceIndexToWorldPosition(
    MPRPlane plane, int sliceIndex) const {

    const auto& origin = impl_->origin;
    const auto& spacing = impl_->spacing;

    switch (plane) {
        case MPRPlane::Axial:
            return origin[2] + sliceIndex * spacing[2];
        case MPRPlane::Coronal:
            return origin[1] + sliceIndex * spacing[1];
        case MPRPlane::Sagittal:
            return origin[0] + sliceIndex * spacing[0];
    }

    return 0.0;
}

std::optional<MPRCoordinateTransformer::SegmentationCoordinates>
MPRCoordinateTransformer::transformForSegmentation(
    MPRPlane plane, int viewX, int viewY, double slicePosition) const {

    // Get the 3D index from the 2D view coordinates
    auto index3D = planeCoordToIndex(plane, viewX, viewY, slicePosition);
    if (!index3D) {
        return std::nullopt;
    }

    // For ManualSegmentationController:
    // - point2D is always in XY coordinates of the label map
    // - sliceIndex is always the Z index
    // But we need to map based on the plane orientation

    SegmentationCoordinates coords;
    coords.index3D = *index3D;

    switch (plane) {
        case MPRPlane::Axial:
            // Axial view: drawing on XY plane at slice Z
            coords.point2D = Point2D{index3D->x, index3D->y};
            coords.sliceIndex = index3D->z;
            break;

        case MPRPlane::Coronal:
            // Coronal view: drawing on XZ plane at slice Y
            // Need to remap: the controller expects XY + Z slice
            // We treat X as X, Z as "Y", and Y as the "slice"
            coords.point2D = Point2D{index3D->x, index3D->z};
            coords.sliceIndex = index3D->y;
            break;

        case MPRPlane::Sagittal:
            // Sagittal view: drawing on YZ plane at slice X
            // Remap: Y as X, Z as Y, X as slice
            coords.point2D = Point2D{index3D->y, index3D->z};
            coords.sliceIndex = index3D->x;
            break;
    }

    return coords;
}

std::pair<int, int> MPRCoordinateTransformer::getSliceRange(MPRPlane plane) const {
    const auto& dims = impl_->dimensions;

    switch (plane) {
        case MPRPlane::Axial:
            return {0, dims[2] - 1};  // Z range
        case MPRPlane::Coronal:
            return {0, dims[1] - 1};  // Y range
        case MPRPlane::Sagittal:
            return {0, dims[0] - 1};  // X range
    }

    return {0, 0};
}

bool MPRCoordinateTransformer::isValidIndex(const Index3D& index) const {
    const auto& dims = impl_->dimensions;

    return index.x >= 0 && index.x < dims[0] &&
           index.y >= 0 && index.y < dims[1] &&
           index.z >= 0 && index.z < dims[2];
}

std::array<int, 3> MPRCoordinateTransformer::getPlaneAxisMapping(MPRPlane plane) const {
    // Returns {horizontalAxis, verticalAxis, sliceAxis}
    // 0 = X, 1 = Y, 2 = Z
    switch (plane) {
        case MPRPlane::Axial:
            return {0, 1, 2};  // H=X, V=Y, Slice=Z
        case MPRPlane::Coronal:
            return {0, 2, 1};  // H=X, V=Z, Slice=Y
        case MPRPlane::Sagittal:
            return {1, 2, 0};  // H=Y, V=Z, Slice=X
    }

    return {0, 1, 2};
}

} // namespace dicom_viewer::services

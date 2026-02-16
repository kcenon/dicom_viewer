#include "services/segmentation/centerline_tracer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkLinearInterpolateImageFunction.h>

namespace dicom_viewer::services {

// =========================================================================
// Helper utilities
// =========================================================================

namespace {

/// Compute Euclidean distance between two 3D points
double distance3D(const Point3D& a, const Point3D& b) {
    double dx = a[0] - b[0];
    double dy = a[1] - b[1];
    double dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/// Normalize a 3D vector in-place; returns magnitude
double normalize3D(Point3D& v) {
    double mag = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (mag > 1e-12) {
        v[0] /= mag;
        v[1] /= mag;
        v[2] /= mag;
    }
    return mag;
}

/// Cross product of two 3D vectors
Point3D cross3D(const Point3D& a, const Point3D& b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

/// 26-connectivity neighbor offsets
struct Neighbor {
    int dx, dy, dz;
    double dist;  // physical distance factor (1, sqrt(2), sqrt(3))
};

std::vector<Neighbor> buildNeighbors() {
    std::vector<Neighbor> neighbors;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                neighbors.push_back({dx, dy, dz, dist});
            }
        }
    }
    return neighbors;
}

/// Convert flat index to 3D index
inline void flatToIndex(size_t flat, int nx, int ny,
                         int& x, int& y, int& z) {
    z = static_cast<int>(flat / (nx * ny));
    size_t rem = flat % (nx * ny);
    y = static_cast<int>(rem / nx);
    x = static_cast<int>(rem % nx);
}

/// Convert 3D index to flat index
inline size_t indexToFlat(int x, int y, int z, int nx, int ny) {
    return static_cast<size_t>(z) * nx * ny +
           static_cast<size_t>(y) * nx +
           static_cast<size_t>(x);
}

}  // anonymous namespace

// =========================================================================
// Physical ↔ voxel coordinate conversion
// =========================================================================

bool CenterlineTracer::physicalToIndex(const FloatImage3D* image,
                                        const Point3D& point,
                                        FloatImage3D::IndexType& index) {
    if (!image) return false;

    FloatImage3D::PointType pt;
    pt[0] = point[0];
    pt[1] = point[1];
    pt[2] = point[2];

    using ContinuousIndexType = itk::ContinuousIndex<double, 3>;
    ContinuousIndexType cidx;
    if (!image->TransformPhysicalPointToContinuousIndex(pt, cidx)) {
        return false;
    }

    auto region = image->GetLargestPossibleRegion();
    auto size = region.GetSize();
    for (int i = 0; i < 3; ++i) {
        index[i] = static_cast<long>(std::round(cidx[i]));
        if (index[i] < 0 || index[i] >= static_cast<long>(size[i])) {
            return false;
        }
    }
    return true;
}

// =========================================================================
// Dijkstra 3D shortest path
// =========================================================================

std::expected<CenterlineTracer::CenterlineResult, SegmentationError>
CenterlineTracer::traceCenterline(const FloatImage3D* image,
                                   const Point3D& startPoint,
                                   const Point3D& endPoint,
                                   const TraceConfig& config) {
    if (!image) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"});
    }

    // Convert physical points to voxel indices
    FloatImage3D::IndexType startIdx, endIdx;
    if (!physicalToIndex(image, startPoint, startIdx)) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Start point is outside image bounds"});
    }
    if (!physicalToIndex(image, endPoint, endIdx)) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "End point is outside image bounds"});
    }

    auto region = image->GetLargestPossibleRegion();
    auto size = region.GetSize();
    const int nx = static_cast<int>(size[0]);
    const int ny = static_cast<int>(size[1]);
    const int nz = static_cast<int>(size[2]);
    const size_t totalVoxels = static_cast<size_t>(nx) * ny * nz;

    auto spacing = image->GetSpacing();

    // Build cost map: lower cost = preferred path
    // Find intensity range for normalization
    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::lowest();
    itk::ImageRegionConstIterator<FloatImage3D> rangeIt(image, region);
    for (rangeIt.GoToBegin(); !rangeIt.IsAtEnd(); ++rangeIt) {
        float v = rangeIt.Get();
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }

    float intensityRange = maxVal - minVal;
    if (intensityRange < 1e-6f) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Image has uniform intensity"});
    }

    // Compute cost for each voxel
    std::vector<float> costMap(totalVoxels);
    itk::ImageRegionConstIterator<FloatImage3D> costIt(image, region);
    size_t vi = 0;
    for (costIt.GoToBegin(); !costIt.IsAtEnd(); ++costIt, ++vi) {
        // Normalize to [0, 1]
        float normalized = (costIt.Get() - minVal) / intensityRange;
        if (config.brightVessels) {
            // Bright vessels → low cost
            costMap[vi] = std::pow(1.0f - normalized + 0.01f,
                                   static_cast<float>(config.costExponent));
        } else {
            // Dark vessels → low cost
            costMap[vi] = std::pow(normalized + 0.01f,
                                   static_cast<float>(config.costExponent));
        }
    }

    // Dijkstra shortest path
    size_t startFlat = indexToFlat(startIdx[0], startIdx[1], startIdx[2],
                                    nx, ny);
    size_t endFlat = indexToFlat(endIdx[0], endIdx[1], endIdx[2], nx, ny);

    std::vector<float> dist(totalVoxels, std::numeric_limits<float>::max());
    std::vector<size_t> prev(totalVoxels, totalVoxels);  // invalid sentinel

    dist[startFlat] = 0.0f;

    // Min-heap: (distance, flat_index)
    using PQElement = std::pair<float, size_t>;
    std::priority_queue<PQElement, std::vector<PQElement>,
                        std::greater<PQElement>> pq;
    pq.push({0.0f, startFlat});

    auto neighbors = buildNeighbors();
    bool found = false;

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (u == endFlat) {
            found = true;
            break;
        }

        if (d > dist[u]) continue;  // stale entry

        int ux, uy, uz;
        flatToIndex(u, nx, ny, ux, uy, uz);

        for (const auto& nb : neighbors) {
            int vx = ux + nb.dx;
            int vy = uy + nb.dy;
            int vz = uz + nb.dz;

            if (vx < 0 || vx >= nx || vy < 0 || vy >= ny ||
                vz < 0 || vz >= nz) {
                continue;
            }

            size_t v = indexToFlat(vx, vy, vz, nx, ny);

            // Physical distance between neighbors
            double physDist = std::sqrt(
                (nb.dx * spacing[0]) * (nb.dx * spacing[0]) +
                (nb.dy * spacing[1]) * (nb.dy * spacing[1]) +
                (nb.dz * spacing[2]) * (nb.dz * spacing[2]));

            // Edge weight = average cost * physical distance
            float edgeCost = (costMap[u] + costMap[v]) * 0.5f *
                             static_cast<float>(physDist);

            float newDist = dist[u] + edgeCost;
            if (newDist < dist[v]) {
                dist[v] = newDist;
                prev[v] = u;
                pq.push({newDist, v});
            }
        }
    }

    if (!found) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            "No path found between start and end points"});
    }

    // Backtrack path
    std::vector<Point3D> rawPath;
    for (size_t cur = endFlat; cur != totalVoxels; cur = prev[cur]) {
        int cx, cy, cz;
        flatToIndex(cur, nx, ny, cx, cy, cz);

        // Convert voxel index to physical coordinate
        FloatImage3D::IndexType idx = {{cx, cy, cz}};
        FloatImage3D::PointType pt;
        image->TransformIndexToPhysicalPoint(idx, pt);
        rawPath.push_back({pt[0], pt[1], pt[2]});
    }
    std::reverse(rawPath.begin(), rawPath.end());

    // Smooth path
    auto smoothedPath = smoothPath(rawPath, 3);

    // Estimate radius at each smoothed point
    std::vector<double> radii(smoothedPath.size());
    for (size_t i = 0; i < smoothedPath.size(); ++i) {
        // Compute local tangent
        Point3D tangent = {0.0, 0.0, 1.0};
        if (i > 0 && i < smoothedPath.size() - 1) {
            tangent[0] = smoothedPath[i + 1][0] - smoothedPath[i - 1][0];
            tangent[1] = smoothedPath[i + 1][1] - smoothedPath[i - 1][1];
            tangent[2] = smoothedPath[i + 1][2] - smoothedPath[i - 1][2];
        } else if (i == 0 && smoothedPath.size() > 1) {
            tangent[0] = smoothedPath[1][0] - smoothedPath[0][0];
            tangent[1] = smoothedPath[1][1] - smoothedPath[0][1];
            tangent[2] = smoothedPath[1][2] - smoothedPath[0][2];
        }
        normalize3D(tangent);

        radii[i] = estimateLocalRadius(image, smoothedPath[i], tangent,
                                        config.initialRadiusMm);
    }

    // Compute total length
    double totalLength = 0.0;
    for (size_t i = 1; i < smoothedPath.size(); ++i) {
        totalLength += distance3D(smoothedPath[i - 1], smoothedPath[i]);
    }

    CenterlineResult result;
    result.points = std::move(smoothedPath);
    result.radii = std::move(radii);
    result.totalLengthMm = totalLength;
    return result;
}

// =========================================================================
// Catmull-Rom spline smoothing
// =========================================================================

std::vector<Point3D>
CenterlineTracer::smoothPath(const std::vector<Point3D>& rawPoints,
                              int subdivisions) {
    if (rawPoints.size() < 3) {
        return rawPoints;
    }

    if (subdivisions < 1) {
        subdivisions = 1;
    }

    std::vector<Point3D> result;
    result.reserve(rawPoints.size() * subdivisions);

    for (size_t i = 0; i < rawPoints.size() - 1; ++i) {
        // Catmull-Rom requires 4 control points: P_{i-1}, P_i, P_{i+1}, P_{i+2}
        const auto& p0 = (i == 0) ? rawPoints[0] : rawPoints[i - 1];
        const auto& p1 = rawPoints[i];
        const auto& p2 = rawPoints[i + 1];
        const auto& p3 = (i + 2 < rawPoints.size())
                              ? rawPoints[i + 2]
                              : rawPoints[rawPoints.size() - 1];

        for (int s = 0; s < subdivisions; ++s) {
            double t = static_cast<double>(s) / subdivisions;
            double t2 = t * t;
            double t3 = t2 * t;

            // Catmull-Rom basis
            Point3D pt;
            for (int d = 0; d < 3; ++d) {
                pt[d] = 0.5 * ((2.0 * p1[d]) +
                                (-p0[d] + p2[d]) * t +
                                (2.0 * p0[d] - 5.0 * p1[d] + 4.0 * p2[d] - p3[d]) * t2 +
                                (-p0[d] + 3.0 * p1[d] - 3.0 * p2[d] + p3[d]) * t3);
            }
            result.push_back(pt);
        }
    }

    // Add the last point
    result.push_back(rawPoints.back());
    return result;
}

// =========================================================================
// Local radius estimation
// =========================================================================

double CenterlineTracer::estimateLocalRadius(const FloatImage3D* image,
                                              const Point3D& center,
                                              const Point3D& tangent,
                                              double maxRadiusMm) {
    if (!image || maxRadiusMm <= 0.0) {
        return 1.0;
    }

    // Create two orthogonal vectors perpendicular to tangent
    Point3D arbitrary = {0.0, 0.0, 1.0};
    if (std::abs(tangent[2]) > 0.9) {
        arbitrary = {1.0, 0.0, 0.0};
    }

    Point3D perp1 = cross3D(tangent, arbitrary);
    normalize3D(perp1);
    Point3D perp2 = cross3D(tangent, perp1);
    normalize3D(perp2);

    // Set up interpolator
    using InterpolatorType =
        itk::LinearInterpolateImageFunction<FloatImage3D, double>;
    auto interp = InterpolatorType::New();
    interp->SetInputImage(image);

    // Get center intensity
    FloatImage3D::PointType centerPt;
    centerPt[0] = center[0];
    centerPt[1] = center[1];
    centerPt[2] = center[2];

    if (!interp->IsInsideBuffer(centerPt)) {
        return 1.0;
    }

    double centerIntensity = interp->Evaluate(centerPt);

    // Half-max threshold for vessel boundary
    auto spacing = image->GetSpacing();
    double minSpacing = std::min({spacing[0], spacing[1], spacing[2]});
    double stepMm = minSpacing * 0.5;
    double threshold = centerIntensity * 0.5;

    // Sample in 8 radial directions
    constexpr int numDirections = 8;
    double totalRadius = 0.0;
    int validCount = 0;

    for (int dir = 0; dir < numDirections; ++dir) {
        double angle = dir * (2.0 * M_PI / numDirections);
        double dx = perp1[0] * std::cos(angle) + perp2[0] * std::sin(angle);
        double dy = perp1[1] * std::cos(angle) + perp2[1] * std::sin(angle);
        double dz = perp1[2] * std::cos(angle) + perp2[2] * std::sin(angle);

        double foundRadius = maxRadiusMm;
        for (double r = stepMm; r <= maxRadiusMm; r += stepMm) {
            FloatImage3D::PointType samplePt;
            samplePt[0] = center[0] + dx * r;
            samplePt[1] = center[1] + dy * r;
            samplePt[2] = center[2] + dz * r;

            if (!interp->IsInsideBuffer(samplePt)) {
                foundRadius = r;
                break;
            }

            double val = interp->Evaluate(samplePt);
            if (val < threshold) {
                foundRadius = r;
                break;
            }
        }

        totalRadius += foundRadius;
        ++validCount;
    }

    if (validCount == 0) {
        return 1.0;
    }

    return totalRadius / validCount;
}

// =========================================================================
// Tubular mask generation
// =========================================================================

std::expected<CenterlineTracer::BinaryMaskType::Pointer, SegmentationError>
CenterlineTracer::generateMask(const CenterlineResult& centerline,
                                double radiusOverrideMm,
                                const FloatImage3D* referenceImage) {
    if (!referenceImage) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Reference image is null"});
    }

    if (centerline.points.empty()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Centerline has no points"});
    }

    // Create output mask with same geometry as reference
    auto mask = BinaryMaskType::New();
    mask->SetRegions(referenceImage->GetLargestPossibleRegion());
    mask->SetSpacing(referenceImage->GetSpacing());
    mask->SetOrigin(referenceImage->GetOrigin());
    mask->SetDirection(referenceImage->GetDirection());
    mask->Allocate();
    mask->FillBuffer(0);

    auto region = mask->GetLargestPossibleRegion();

    // For each voxel, compute minimum distance to centerline
    itk::ImageRegionIterator<BinaryMaskType> it(mask, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        FloatImage3D::PointType pt;
        referenceImage->TransformIndexToPhysicalPoint(idx, pt);

        Point3D voxelPos = {pt[0], pt[1], pt[2]};

        // Find minimum distance to any centerline segment
        double minDist = std::numeric_limits<double>::max();
        size_t nearestIdx = 0;

        for (size_t i = 0; i < centerline.points.size(); ++i) {
            double d = distance3D(voxelPos, centerline.points[i]);
            if (d < minDist) {
                minDist = d;
                nearestIdx = i;
            }
        }

        // Determine radius at nearest point
        double radius = radiusOverrideMm;
        if (radius < 0.0 && nearestIdx < centerline.radii.size()) {
            radius = centerline.radii[nearestIdx];
        }
        if (radius <= 0.0) {
            radius = 1.0;
        }

        if (minDist <= radius) {
            it.Set(1);
        }
    }

    return mask;
}

// =========================================================================
// Overload: traceCenterline without config (uses default)
// =========================================================================

std::expected<CenterlineTracer::CenterlineResult, SegmentationError>
CenterlineTracer::traceCenterline(const FloatImage3D* image,
                                   const Point3D& startPoint,
                                   const Point3D& endPoint) {
    return traceCenterline(image, startPoint, endPoint, TraceConfig{});
}

}  // namespace dicom_viewer::services

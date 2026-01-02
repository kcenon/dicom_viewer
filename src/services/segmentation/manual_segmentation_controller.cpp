#include "services/segmentation/manual_segmentation_controller.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <set>

#include <itkImageRegionIterator.h>

namespace dicom_viewer::services {

/**
 * @brief Implementation class for ManualSegmentationController
 */
class ManualSegmentationController::Impl {
public:
    LabelMapType::Pointer labelMap_;
    SegmentationTool activeTool_ = SegmentationTool::None;
    BrushParameters brushParams_;
    FillParameters fillParams_;
    FreehandParameters freehandParams_;
    uint8_t activeLabel_ = 1;
    bool isDrawing_ = false;
    Point2D lastPosition_;
    int lastSliceIndex_ = -1;
    ModificationCallback modificationCallback_;

    // Freehand drawing state
    std::vector<Point2D> freehandPath_;

    /**
     * @brief Apply brush stroke at a position
     */
    void applyBrush(const Point2D& position, int sliceIndex, uint8_t value) {
        if (!labelMap_) {
            return;
        }

        auto region = labelMap_->GetLargestPossibleRegion();
        auto size = region.GetSize();

        int halfSize = brushParams_.size / 2;

        for (int dy = -halfSize; dy <= halfSize; ++dy) {
            for (int dx = -halfSize; dx <= halfSize; ++dx) {
                int px = position.x + dx;
                int py = position.y + dy;

                // Bounds check
                if (px < 0 || px >= static_cast<int>(size[0]) ||
                    py < 0 || py >= static_cast<int>(size[1]) ||
                    sliceIndex < 0 || sliceIndex >= static_cast<int>(size[2])) {
                    continue;
                }

                // Shape check
                if (brushParams_.shape == BrushShape::Circle) {
                    double distance = std::sqrt(dx * dx + dy * dy);
                    if (distance > halfSize) {
                        continue;
                    }
                }

                LabelMapType::IndexType index;
                index[0] = px;
                index[1] = py;
                index[2] = sliceIndex;

                labelMap_->SetPixel(index, value);
            }
        }
    }

    /**
     * @brief Draw line between two points using Bresenham's algorithm
     */
    void drawLine(const Point2D& from, const Point2D& to, int sliceIndex, uint8_t value) {
        int x0 = from.x;
        int y0 = from.y;
        int x1 = to.x;
        int y1 = to.y;

        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        while (true) {
            applyBrush(Point2D{x0, y0}, sliceIndex, value);

            if (x0 == x1 && y0 == y1) {
                break;
            }

            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    /**
     * @brief Apply flood fill at position
     */
    void applyFill(const Point2D& position, int sliceIndex, uint8_t value) {
        if (!labelMap_) {
            return;
        }

        auto region = labelMap_->GetLargestPossibleRegion();
        auto size = region.GetSize();

        // Bounds check
        if (position.x < 0 || position.x >= static_cast<int>(size[0]) ||
            position.y < 0 || position.y >= static_cast<int>(size[1]) ||
            sliceIndex < 0 || sliceIndex >= static_cast<int>(size[2])) {
            return;
        }

        LabelMapType::IndexType startIndex;
        startIndex[0] = position.x;
        startIndex[1] = position.y;
        startIndex[2] = sliceIndex;

        uint8_t originalValue = labelMap_->GetPixel(startIndex);

        // Don't fill if already the target value
        if (originalValue == value) {
            return;
        }

        std::queue<Point2D> queue;
        queue.push(position);

        // Track visited pixels to avoid infinite loops
        std::vector<std::vector<bool>> visited(
            size[0], std::vector<bool>(size[1], false)
        );
        visited[position.x][position.y] = true;

        // 4-connectivity or 8-connectivity
        std::vector<std::pair<int, int>> neighbors;
        neighbors = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        if (fillParams_.use8Connectivity) {
            neighbors.insert(neighbors.end(),
                {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}});
        }

        while (!queue.empty()) {
            Point2D current = queue.front();
            queue.pop();

            LabelMapType::IndexType idx;
            idx[0] = current.x;
            idx[1] = current.y;
            idx[2] = sliceIndex;

            labelMap_->SetPixel(idx, value);

            for (const auto& [dx, dy] : neighbors) {
                int nx = current.x + dx;
                int ny = current.y + dy;

                if (nx < 0 || nx >= static_cast<int>(size[0]) ||
                    ny < 0 || ny >= static_cast<int>(size[1])) {
                    continue;
                }

                if (visited[nx][ny]) {
                    continue;
                }

                LabelMapType::IndexType neighborIdx;
                neighborIdx[0] = nx;
                neighborIdx[1] = ny;
                neighborIdx[2] = sliceIndex;

                uint8_t neighborValue = labelMap_->GetPixel(neighborIdx);
                if (neighborValue == originalValue) {
                    visited[nx][ny] = true;
                    queue.push(Point2D{nx, ny});
                }
            }
        }
    }

    /**
     * @brief Calculate perpendicular distance from point to line segment
     */
    static double perpendicularDistance(const Point2D& point,
                                        const Point2D& lineStart,
                                        const Point2D& lineEnd) {
        double dx = lineEnd.x - lineStart.x;
        double dy = lineEnd.y - lineStart.y;

        // If line is a point, return distance to that point
        double lineLengthSq = dx * dx + dy * dy;
        if (lineLengthSq == 0.0) {
            double pdx = point.x - lineStart.x;
            double pdy = point.y - lineStart.y;
            return std::sqrt(pdx * pdx + pdy * pdy);
        }

        // Calculate perpendicular distance
        double t = ((point.x - lineStart.x) * dx +
                    (point.y - lineStart.y) * dy) / lineLengthSq;

        t = std::max(0.0, std::min(1.0, t));

        double projX = lineStart.x + t * dx;
        double projY = lineStart.y + t * dy;

        double distX = point.x - projX;
        double distY = point.y - projY;

        return std::sqrt(distX * distX + distY * distY);
    }

    /**
     * @brief Douglas-Peucker path simplification algorithm
     */
    std::vector<Point2D> simplifyPath(const std::vector<Point2D>& path,
                                      double tolerance) {
        if (path.size() < 3) {
            return path;
        }

        // Find the point with maximum distance
        double maxDistance = 0.0;
        size_t maxIndex = 0;

        for (size_t i = 1; i < path.size() - 1; ++i) {
            double distance = perpendicularDistance(
                path[i], path.front(), path.back()
            );
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = i;
            }
        }

        // If max distance is greater than tolerance, recursively simplify
        if (maxDistance > tolerance) {
            std::vector<Point2D> left(path.begin(), path.begin() + maxIndex + 1);
            std::vector<Point2D> right(path.begin() + maxIndex, path.end());

            auto simplifiedLeft = simplifyPath(left, tolerance);
            auto simplifiedRight = simplifyPath(right, tolerance);

            // Combine results (remove duplicate point at junction)
            simplifiedLeft.pop_back();
            simplifiedLeft.insert(simplifiedLeft.end(),
                                  simplifiedRight.begin(),
                                  simplifiedRight.end());
            return simplifiedLeft;
        }

        // Return only endpoints
        return {path.front(), path.back()};
    }

    /**
     * @brief Apply Gaussian smoothing to path
     */
    std::vector<Point2D> smoothPath(const std::vector<Point2D>& path,
                                    int windowSize) {
        if (path.size() < 3 || windowSize < 3) {
            return path;
        }

        std::vector<Point2D> smoothed;
        smoothed.reserve(path.size());

        int halfWindow = windowSize / 2;

        // Generate Gaussian weights
        std::vector<double> weights(windowSize);
        double sigma = windowSize / 4.0;
        double sum = 0.0;

        for (int i = 0; i < windowSize; ++i) {
            int offset = i - halfWindow;
            weights[i] = std::exp(-0.5 * offset * offset / (sigma * sigma));
            sum += weights[i];
        }

        // Normalize weights
        for (auto& w : weights) {
            w /= sum;
        }

        // Apply smoothing
        for (size_t i = 0; i < path.size(); ++i) {
            double sumX = 0.0;
            double sumY = 0.0;
            double weightSum = 0.0;

            for (int j = -halfWindow; j <= halfWindow; ++j) {
                int idx = static_cast<int>(i) + j;

                // Clamp to path bounds
                if (idx < 0) idx = 0;
                if (idx >= static_cast<int>(path.size())) {
                    idx = static_cast<int>(path.size()) - 1;
                }

                int weightIdx = j + halfWindow;
                sumX += path[idx].x * weights[weightIdx];
                sumY += path[idx].y * weights[weightIdx];
                weightSum += weights[weightIdx];
            }

            smoothed.push_back(Point2D{
                static_cast<int>(std::round(sumX / weightSum)),
                static_cast<int>(std::round(sumY / weightSum))
            });
        }

        return smoothed;
    }

    /**
     * @brief Calculate distance between two points
     */
    static double distance(const Point2D& a, const Point2D& b) {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    /**
     * @brief Check if path forms a closed loop
     */
    bool isPathClosed(const std::vector<Point2D>& path,
                      double threshold) const {
        if (path.size() < 3) {
            return false;
        }
        return distance(path.front(), path.back()) <= threshold;
    }

    /**
     * @brief Draw freehand path on label map
     */
    void drawFreehandPath(const std::vector<Point2D>& path,
                          int sliceIndex,
                          uint8_t value) {
        if (path.size() < 2) {
            return;
        }

        // Draw lines between consecutive points
        for (size_t i = 0; i < path.size() - 1; ++i) {
            drawLine(path[i], path[i + 1], sliceIndex, value);
        }
    }

    /**
     * @brief Fill interior of closed polygon using scanline algorithm
     */
    void fillPolygon(const std::vector<Point2D>& polygon,
                     int sliceIndex,
                     uint8_t value) {
        if (polygon.size() < 3 || !labelMap_) {
            return;
        }

        auto region = labelMap_->GetLargestPossibleRegion();
        auto size = region.GetSize();

        // Find bounding box
        int minY = std::numeric_limits<int>::max();
        int maxY = std::numeric_limits<int>::min();

        for (const auto& pt : polygon) {
            minY = std::min(minY, pt.y);
            maxY = std::max(maxY, pt.y);
        }

        // Clamp to image bounds
        minY = std::max(0, minY);
        maxY = std::min(static_cast<int>(size[1]) - 1, maxY);

        // Scanline fill
        for (int y = minY; y <= maxY; ++y) {
            std::vector<int> intersections;

            // Find intersections with polygon edges
            for (size_t i = 0; i < polygon.size(); ++i) {
                size_t j = (i + 1) % polygon.size();

                const auto& p1 = polygon[i];
                const auto& p2 = polygon[j];

                // Check if edge crosses scanline
                if ((p1.y <= y && p2.y > y) || (p2.y <= y && p1.y > y)) {
                    // Calculate x intersection
                    double x = static_cast<double>(p1.x) +
                               static_cast<double>(y - p1.y) /
                               static_cast<double>(p2.y - p1.y) *
                               static_cast<double>(p2.x - p1.x);
                    intersections.push_back(static_cast<int>(std::round(x)));
                }
            }

            // Sort intersections
            std::sort(intersections.begin(), intersections.end());

            // Fill between pairs of intersections
            for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                int x1 = std::max(0, intersections[i]);
                int x2 = std::min(static_cast<int>(size[0]) - 1,
                                  intersections[i + 1]);

                for (int x = x1; x <= x2; ++x) {
                    LabelMapType::IndexType idx;
                    idx[0] = x;
                    idx[1] = y;
                    idx[2] = sliceIndex;
                    labelMap_->SetPixel(idx, value);
                }
            }
        }
    }

    /**
     * @brief Process freehand path with simplification and smoothing
     */
    std::vector<Point2D> processFreehandPath() {
        if (freehandPath_.size() < 2) {
            return freehandPath_;
        }

        std::vector<Point2D> result = freehandPath_;

        // Apply smoothing first (before simplification)
        if (freehandParams_.enableSmoothing) {
            result = smoothPath(result, freehandParams_.smoothingWindowSize);
        }

        // Apply simplification
        if (freehandParams_.enableSimplification) {
            result = simplifyPath(result, freehandParams_.simplificationTolerance);
        }

        return result;
    }

    /**
     * @brief Finalize freehand drawing
     */
    void finalizeFreehand(int sliceIndex, uint8_t value) {
        auto processedPath = processFreehandPath();

        if (processedPath.size() < 2) {
            return;
        }

        // Check if path should be closed
        bool shouldClose = freehandParams_.fillInterior &&
                           isPathClosed(processedPath,
                                        freehandParams_.closeThreshold);

        if (shouldClose) {
            // Close the path
            if (processedPath.front() != processedPath.back()) {
                processedPath.push_back(processedPath.front());
            }

            // Draw outline
            drawFreehandPath(processedPath, sliceIndex, value);

            // Fill interior
            fillPolygon(processedPath, sliceIndex, value);
        } else {
            // Just draw the path
            drawFreehandPath(processedPath, sliceIndex, value);
        }

        // Clear path for next drawing
        freehandPath_.clear();
    }
};

ManualSegmentationController::ManualSegmentationController()
    : pImpl_(std::make_unique<Impl>()) {}

ManualSegmentationController::~ManualSegmentationController() = default;

ManualSegmentationController::ManualSegmentationController(ManualSegmentationController&&) noexcept = default;
ManualSegmentationController& ManualSegmentationController::operator=(ManualSegmentationController&&) noexcept = default;

std::expected<void, SegmentationError>
ManualSegmentationController::initializeLabelMap(int width, int height, int depth) {
    if (width <= 0 || height <= 0 || depth <= 0) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Dimensions must be positive"
        });
    }

    try {
        pImpl_->labelMap_ = LabelMapType::New();

        LabelMapType::RegionType region;
        LabelMapType::IndexType start;
        start.Fill(0);

        LabelMapType::SizeType size;
        size[0] = width;
        size[1] = height;
        size[2] = depth;

        region.SetSize(size);
        region.SetIndex(start);

        pImpl_->labelMap_->SetRegions(region);
        pImpl_->labelMap_->Allocate();
        pImpl_->labelMap_->FillBuffer(0);

        return {};
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("ITK error: ") + e.GetDescription()
        });
    }
}

std::expected<void, SegmentationError>
ManualSegmentationController::setLabelMap(LabelMapType::Pointer labelMap) {
    if (!labelMap) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Label map cannot be null"
        });
    }

    pImpl_->labelMap_ = labelMap;
    return {};
}

ManualSegmentationController::LabelMapType::Pointer
ManualSegmentationController::getLabelMap() const {
    return pImpl_->labelMap_;
}

void ManualSegmentationController::setActiveTool(SegmentationTool tool) {
    // Cancel any in-progress operation when switching tools
    if (pImpl_->isDrawing_) {
        cancelOperation();
    }
    pImpl_->activeTool_ = tool;
}

SegmentationTool ManualSegmentationController::getActiveTool() const noexcept {
    return pImpl_->activeTool_;
}

bool ManualSegmentationController::setBrushSize(int size) {
    if (size < 1 || size > 50) {
        return false;
    }
    pImpl_->brushParams_.size = size;
    return true;
}

int ManualSegmentationController::getBrushSize() const noexcept {
    return pImpl_->brushParams_.size;
}

void ManualSegmentationController::setBrushShape(BrushShape shape) {
    pImpl_->brushParams_.shape = shape;
}

BrushShape ManualSegmentationController::getBrushShape() const noexcept {
    return pImpl_->brushParams_.shape;
}

bool ManualSegmentationController::setBrushParameters(const BrushParameters& params) {
    if (!params.isValid()) {
        return false;
    }
    pImpl_->brushParams_ = params;
    return true;
}

BrushParameters ManualSegmentationController::getBrushParameters() const noexcept {
    return pImpl_->brushParams_;
}

void ManualSegmentationController::setFillParameters(const FillParameters& params) {
    pImpl_->fillParams_ = params;
}

FillParameters ManualSegmentationController::getFillParameters() const noexcept {
    return pImpl_->fillParams_;
}

bool ManualSegmentationController::setFreehandParameters(const FreehandParameters& params) {
    if (!params.isValid()) {
        return false;
    }
    pImpl_->freehandParams_ = params;
    return true;
}

FreehandParameters ManualSegmentationController::getFreehandParameters() const noexcept {
    return pImpl_->freehandParams_;
}

std::vector<Point2D> ManualSegmentationController::getFreehandPath() const {
    return pImpl_->freehandPath_;
}

bool ManualSegmentationController::setActiveLabel(uint8_t labelId) {
    if (labelId == 0) {
        return false;  // 0 is reserved for background
    }
    pImpl_->activeLabel_ = labelId;
    return true;
}

uint8_t ManualSegmentationController::getActiveLabel() const noexcept {
    return pImpl_->activeLabel_;
}

void ManualSegmentationController::onMousePress(const Point2D& position, int sliceIndex) {
    if (!pImpl_->labelMap_ || pImpl_->activeTool_ == SegmentationTool::None) {
        return;
    }

    pImpl_->isDrawing_ = true;
    pImpl_->lastPosition_ = position;
    pImpl_->lastSliceIndex_ = sliceIndex;

    switch (pImpl_->activeTool_) {
        case SegmentationTool::Brush:
            pImpl_->applyBrush(position, sliceIndex, pImpl_->activeLabel_);
            break;

        case SegmentationTool::Eraser:
            pImpl_->applyBrush(position, sliceIndex, 0);  // Erase = set to 0
            break;

        case SegmentationTool::Fill:
            pImpl_->applyFill(position, sliceIndex, pImpl_->activeLabel_);
            pImpl_->isDrawing_ = false;  // Fill is instant
            break;

        case SegmentationTool::Freehand:
            // Start new freehand path
            pImpl_->freehandPath_.clear();
            pImpl_->freehandPath_.push_back(position);
            break;

        default:
            // Other tools handled separately
            break;
    }

    if (pImpl_->modificationCallback_) {
        pImpl_->modificationCallback_(sliceIndex);
    }
}

void ManualSegmentationController::onMouseMove(const Point2D& position, int sliceIndex) {
    if (!pImpl_->isDrawing_ || !pImpl_->labelMap_) {
        return;
    }

    // Only process if position changed
    if (position == pImpl_->lastPosition_ && sliceIndex == pImpl_->lastSliceIndex_) {
        return;
    }

    switch (pImpl_->activeTool_) {
        case SegmentationTool::Brush:
            pImpl_->drawLine(pImpl_->lastPosition_, position, sliceIndex, pImpl_->activeLabel_);
            break;

        case SegmentationTool::Eraser:
            pImpl_->drawLine(pImpl_->lastPosition_, position, sliceIndex, 0);
            break;

        case SegmentationTool::Freehand:
            // Add point to path (skip if too close to last point)
            if (pImpl_->freehandPath_.empty() ||
                Impl::distance(position, pImpl_->freehandPath_.back()) >= 1.0) {
                pImpl_->freehandPath_.push_back(position);
            }
            break;

        default:
            break;
    }

    pImpl_->lastPosition_ = position;
    pImpl_->lastSliceIndex_ = sliceIndex;

    if (pImpl_->modificationCallback_) {
        pImpl_->modificationCallback_(sliceIndex);
    }
}

void ManualSegmentationController::onMouseRelease(const Point2D& position, int sliceIndex) {
    if (!pImpl_->isDrawing_) {
        return;
    }

    // Final stroke to release position
    if (position != pImpl_->lastPosition_) {
        switch (pImpl_->activeTool_) {
            case SegmentationTool::Brush:
                pImpl_->drawLine(pImpl_->lastPosition_, position, sliceIndex, pImpl_->activeLabel_);
                break;

            case SegmentationTool::Eraser:
                pImpl_->drawLine(pImpl_->lastPosition_, position, sliceIndex, 0);
                break;

            case SegmentationTool::Freehand:
                // Add final point
                pImpl_->freehandPath_.push_back(position);
                break;

            default:
                break;
        }
    }

    // Finalize freehand drawing if applicable
    if (pImpl_->activeTool_ == SegmentationTool::Freehand) {
        pImpl_->finalizeFreehand(sliceIndex, pImpl_->activeLabel_);
    }

    pImpl_->isDrawing_ = false;

    if (pImpl_->modificationCallback_) {
        pImpl_->modificationCallback_(sliceIndex);
    }
}

void ManualSegmentationController::cancelOperation() {
    pImpl_->isDrawing_ = false;
    pImpl_->freehandPath_.clear();
}

bool ManualSegmentationController::isDrawing() const noexcept {
    return pImpl_->isDrawing_;
}

void ManualSegmentationController::setModificationCallback(ModificationCallback callback) {
    pImpl_->modificationCallback_ = std::move(callback);
}

void ManualSegmentationController::clearAll() {
    if (pImpl_->labelMap_) {
        pImpl_->labelMap_->FillBuffer(0);

        if (pImpl_->modificationCallback_) {
            pImpl_->modificationCallback_(-1);  // -1 indicates all slices
        }
    }
}

void ManualSegmentationController::clearLabel(uint8_t labelId) {
    if (!pImpl_->labelMap_) {
        return;
    }

    using IteratorType = itk::ImageRegionIterator<LabelMapType>;
    IteratorType it(pImpl_->labelMap_, pImpl_->labelMap_->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() == labelId) {
            it.Set(0);
        }
    }

    if (pImpl_->modificationCallback_) {
        pImpl_->modificationCallback_(-1);
    }
}

} // namespace dicom_viewer::services

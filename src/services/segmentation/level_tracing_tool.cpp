#include "services/segmentation/level_tracing_tool.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <set>
#include <vector>

#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>

namespace dicom_viewer::services {

// =========================================================================
// Internal helpers
// =========================================================================

namespace {

/// Check if an index is within image bounds
bool isInBounds(int x, int y, int width, int height) {
    return x >= 0 && x < width && y >= 0 && y < height;
}

/// Compute image intensity range
std::pair<float, float> computeIntensityRange(const LevelTracingTool::FloatSlice2D* slice) {
    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::lowest();

    auto region = slice->GetLargestPossibleRegion();
    itk::ImageRegionConstIterator<LevelTracingTool::FloatSlice2D> it(slice, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        float v = it.Get();
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }
    return {minVal, maxVal};
}

/// BFS flood-fill from seed within intensity tolerance band.
/// Returns a binary visited mask (1 = within-band connected to seed).
LevelTracingTool::BinarySlice2D::Pointer
floodFill(const LevelTracingTool::FloatSlice2D* slice,
          const LevelTracingTool::IndexPoint& seed,
          float lowerBound, float upperBound) {
    auto region = slice->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int width = static_cast<int>(size[0]);
    int height = static_cast<int>(size[1]);

    auto mask = LevelTracingTool::BinarySlice2D::New();
    mask->SetRegions(region);
    mask->SetSpacing(slice->GetSpacing());
    mask->SetOrigin(slice->GetOrigin());
    mask->SetDirection(slice->GetDirection());
    mask->Allocate();
    mask->FillBuffer(0);

    // 4-connectivity BFS
    std::queue<LevelTracingTool::IndexPoint> queue;
    queue.push(seed);

    LevelTracingTool::BinarySlice2D::IndexType idx = {{seed[0], seed[1]}};
    mask->SetPixel(idx, 1);

    constexpr int dx4[] = {1, -1, 0, 0};
    constexpr int dy4[] = {0, 0, 1, -1};

    while (!queue.empty()) {
        auto [cx, cy] = queue.front();
        queue.pop();

        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx4[d];
            int ny = cy + dy4[d];

            if (!isInBounds(nx, ny, width, height)) continue;

            LevelTracingTool::BinarySlice2D::IndexType nIdx = {{nx, ny}};
            if (mask->GetPixel(nIdx) != 0) continue;

            LevelTracingTool::FloatSlice2D::IndexType srcIdx = {{nx, ny}};
            float val = slice->GetPixel(srcIdx);
            if (val >= lowerBound && val <= upperBound) {
                mask->SetPixel(nIdx, 1);
                queue.push({nx, ny});
            }
        }
    }

    return mask;
}

/// Moore neighborhood contour tracing.
/// Extracts the ordered boundary of the foreground region in the mask,
/// starting from the topmost-leftmost boundary pixel.
std::vector<LevelTracingTool::IndexPoint>
mooreContourTrace(const LevelTracingTool::BinarySlice2D* mask) {
    auto size = mask->GetLargestPossibleRegion().GetSize();
    int width = static_cast<int>(size[0]);
    int height = static_cast<int>(size[1]);

    // Find the starting boundary pixel: scan top-to-bottom, left-to-right
    LevelTracingTool::IndexPoint start = {-1, -1};
    for (int y = 0; y < height && start[0] < 0; ++y) {
        for (int x = 0; x < width; ++x) {
            LevelTracingTool::BinarySlice2D::IndexType idx = {{x, y}};
            if (mask->GetPixel(idx) > 0) {
                start = {x, y};
                break;
            }
        }
    }

    if (start[0] < 0) {
        return {};  // No foreground pixels
    }

    // Moore neighborhood: 8 directions (clockwise from right)
    // Direction index: 0=right, 1=bottom-right, 2=down, 3=bottom-left,
    //                  4=left, 5=top-left, 6=up, 7=top-right
    constexpr int mx[] = {1, 1, 0, -1, -1, -1, 0, 1};
    constexpr int my[] = {0, 1, 1, 1, 0, -1, -1, -1};

    auto getPixel = [&](int x, int y) -> bool {
        if (!isInBounds(x, y, width, height)) return false;
        LevelTracingTool::BinarySlice2D::IndexType idx = {{x, y}};
        return mask->GetPixel(idx) > 0;
    };

    std::vector<LevelTracingTool::IndexPoint> contour;
    contour.push_back(start);

    // The backtrack direction: we entered the start pixel from the left
    int backDir = 4;  // Came from the left (entered from west)

    LevelTracingTool::IndexPoint current = start;
    bool firstStep = true;

    // Jacob's stopping criterion: stop when we return to start AND
    // the next boundary pixel matches the second pixel in the contour
    while (true) {
        // Start scanning from (backDir + 1) mod 8 clockwise
        int startDir = (backDir + 1) % 8;
        bool found = false;

        for (int i = 0; i < 8; ++i) {
            int dir = (startDir + i) % 8;
            int nx = current[0] + mx[dir];
            int ny = current[1] + my[dir];

            if (getPixel(nx, ny)) {
                LevelTracingTool::IndexPoint next = {nx, ny};

                // Jacob's stopping criterion
                if (!firstStep && next[0] == start[0] && next[1] == start[1]) {
                    // Check if this completes the contour
                    if (contour.size() > 2) {
                        return contour;
                    }
                }

                contour.push_back(next);
                // Backtrack direction is opposite of the direction we moved
                backDir = (dir + 4) % 8;
                current = next;
                found = true;
                firstStep = false;
                break;
            }
        }

        if (!found) {
            // Isolated pixel or dead end
            break;
        }

        // Safety: prevent infinite loops for pathological cases
        if (contour.size() > static_cast<size_t>(2 * width * height)) {
            break;
        }
    }

    return contour;
}

}  // anonymous namespace

// =========================================================================
// traceContour
// =========================================================================

std::expected<std::vector<LevelTracingTool::IndexPoint>, SegmentationError>
LevelTracingTool::traceContour(const FloatSlice2D* slice,
                                const IndexPoint& seedPoint,
                                const Config& config) {
    if (!slice) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input slice is null"});
    }

    auto region = slice->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int width = static_cast<int>(size[0]);
    int height = static_cast<int>(size[1]);

    if (!isInBounds(seedPoint[0], seedPoint[1], width, height)) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Seed point is outside image bounds"});
    }

    // Get seed intensity
    FloatSlice2D::IndexType seedIdx = {{seedPoint[0], seedPoint[1]}};
    float seedIntensity = slice->GetPixel(seedIdx);

    // Compute tolerance band
    auto [minVal, maxVal] = computeIntensityRange(slice);
    float range = maxVal - minVal;
    if (range < 1e-6f) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Image has uniform intensity"});
    }

    float tolerance = range * static_cast<float>(config.tolerancePct / 100.0);
    float lowerBound = seedIntensity - tolerance;
    float upperBound = seedIntensity + tolerance;

    // Flood-fill from seed within tolerance band
    auto filledMask = floodFill(slice, seedPoint, lowerBound, upperBound);

    // Extract boundary contour using Moore neighborhood tracing
    auto contour = mooreContourTrace(filledMask.GetPointer());

    if (contour.empty()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            "No contour found at seed intensity level"});
    }

    return contour;
}

std::expected<std::vector<LevelTracingTool::IndexPoint>, SegmentationError>
LevelTracingTool::traceContour(const FloatSlice2D* slice,
                                const IndexPoint& seedPoint) {
    return traceContour(slice, seedPoint, Config{});
}

// =========================================================================
// contourToMask
// =========================================================================

std::expected<LevelTracingTool::BinarySlice2D::Pointer, SegmentationError>
LevelTracingTool::contourToMask(const std::vector<IndexPoint>& contour,
                                 const FloatSlice2D* referenceSlice) {
    if (!referenceSlice) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Reference slice is null"});
    }

    if (contour.size() < 3) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Contour has fewer than 3 points"});
    }

    auto region = referenceSlice->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int width = static_cast<int>(size[0]);
    int height = static_cast<int>(size[1]);

    auto mask = BinarySlice2D::New();
    mask->SetRegions(region);
    mask->SetSpacing(referenceSlice->GetSpacing());
    mask->SetOrigin(referenceSlice->GetOrigin());
    mask->SetDirection(referenceSlice->GetDirection());
    mask->Allocate();
    mask->FillBuffer(0);

    // Scanline ray-casting: for each row, count contour crossings
    // A pixel is inside if the number of crossings to its left is odd
    for (int y = 0; y < height; ++y) {
        // Collect X-coordinates where contour edges cross this scanline
        std::vector<int> crossings;
        size_t n = contour.size();

        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            int y0 = contour[i][1];
            int y1 = contour[j][1];

            // Skip horizontal edges
            if (y0 == y1) continue;

            // Check if scanline y crosses this edge
            int yMin = std::min(y0, y1);
            int yMax = std::max(y0, y1);

            if (y < yMin || y >= yMax) continue;

            // Linear interpolation for crossing x-coordinate
            double t = static_cast<double>(y - y0) / (y1 - y0);
            double crossX = contour[i][0] + t * (contour[j][0] - contour[i][0]);
            crossings.push_back(static_cast<int>(std::round(crossX)));
        }

        // Sort crossings and fill between pairs
        std::sort(crossings.begin(), crossings.end());

        for (size_t k = 0; k + 1 < crossings.size(); k += 2) {
            int xStart = std::max(0, crossings[k]);
            int xEnd = std::min(width - 1, crossings[k + 1]);
            for (int x = xStart; x <= xEnd; ++x) {
                BinarySlice2D::IndexType idx = {{x, y}};
                mask->SetPixel(idx, 1);
            }
        }
    }

    // Also mark contour pixels themselves
    for (const auto& pt : contour) {
        if (isInBounds(pt[0], pt[1], width, height)) {
            BinarySlice2D::IndexType idx = {{pt[0], pt[1]}};
            mask->SetPixel(idx, 1);
        }
    }

    return mask;
}

// =========================================================================
// traceAndFill
// =========================================================================

std::expected<LevelTracingTool::BinarySlice2D::Pointer, SegmentationError>
LevelTracingTool::traceAndFill(const FloatSlice2D* slice,
                                const IndexPoint& seedPoint,
                                const Config& config) {
    if (!slice) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input slice is null"});
    }

    auto region = slice->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int width = static_cast<int>(size[0]);
    int height = static_cast<int>(size[1]);

    if (!isInBounds(seedPoint[0], seedPoint[1], width, height)) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Seed point is outside image bounds"});
    }

    // Get seed intensity and compute tolerance band
    FloatSlice2D::IndexType seedIdx = {{seedPoint[0], seedPoint[1]}};
    float seedIntensity = slice->GetPixel(seedIdx);

    auto [minVal, maxVal] = computeIntensityRange(slice);
    float range = maxVal - minVal;
    if (range < 1e-6f) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Image has uniform intensity"});
    }

    float tolerance = range * static_cast<float>(config.tolerancePct / 100.0);
    float lowerBound = seedIntensity - tolerance;
    float upperBound = seedIntensity + tolerance;

    // Flood-fill directly produces the filled mask
    auto mask = floodFill(slice, seedPoint, lowerBound, upperBound);

    // Verify non-empty result
    bool hasPixels = false;
    itk::ImageRegionConstIterator<BinarySlice2D> it(mask, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0) {
            hasPixels = true;
            break;
        }
    }

    if (!hasPixels) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            "No region found at seed intensity level"});
    }

    // Set foreground value if different from default (1)
    if (config.foregroundValue != 1) {
        itk::ImageRegionIterator<BinarySlice2D> fillIt(mask, region);
        for (fillIt.GoToBegin(); !fillIt.IsAtEnd(); ++fillIt) {
            if (fillIt.Get() > 0) {
                fillIt.Set(config.foregroundValue);
            }
        }
    }

    return mask;
}

std::expected<LevelTracingTool::BinarySlice2D::Pointer, SegmentationError>
LevelTracingTool::traceAndFill(const FloatSlice2D* slice,
                                const IndexPoint& seedPoint) {
    return traceAndFill(slice, seedPoint, Config{});
}

}  // namespace dicom_viewer::services

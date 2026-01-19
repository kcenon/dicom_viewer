#include "services/measurement/roi_statistics.hpp"
#include "core/logging.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

#include <itkImageRegionIterator.h>
#include <itkLabelStatisticsImageFilter.h>

namespace dicom_viewer::services {

namespace {
auto& getLogger() {
    static auto logger = logging::LoggerFactory::create("RoiStatistics");
    return logger;
}
}

// =============================================================================
// RoiStatistics implementation
// =============================================================================

std::string RoiStatistics::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "ROI Statistics";
    if (!roiLabel.empty()) {
        oss << " (" << roiLabel << ")";
    }
    oss << ":\n";

    oss << "  Mean:      " << mean << " HU\n";
    oss << "  Std Dev:   " << stdDev << " HU\n";
    oss << "  Min:       " << min << " HU\n";
    oss << "  Max:       " << max << " HU\n";
    oss << "  Median:    " << median << " HU\n";
    oss << "  Voxels:    " << voxelCount << "\n";

    if (areaMm2 > 0) {
        oss << "  Area:      " << areaMm2 << " mm^2\n";
    }
    if (volumeMm3 > 0) {
        oss << "  Volume:    " << volumeMm3 << " mm^3\n";
    }

    oss << "  5th %ile:  " << percentile5 << " HU\n";
    oss << "  25th %ile: " << percentile25 << " HU\n";
    oss << "  75th %ile: " << percentile75 << " HU\n";
    oss << "  95th %ile: " << percentile95 << " HU\n";

    return oss.str();
}

std::expected<void, std::string>
RoiStatistics::exportToCsv(const std::filesystem::path& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected("Failed to open file for writing: " + filePath.string());
    }

    // Write header
    auto header = getCsvHeader();
    for (size_t i = 0; i < header.size(); ++i) {
        file << header[i];
        if (i < header.size() - 1) file << ",";
    }
    file << "\n";

    // Write data row
    auto row = getCsvRow();
    for (size_t i = 0; i < row.size(); ++i) {
        file << row[i];
        if (i < row.size() - 1) file << ",";
    }
    file << "\n";

    return {};
}

std::vector<std::string> RoiStatistics::getCsvHeader() {
    return {
        "ROI_ID", "Label", "Mean", "StdDev", "Min", "Max", "Median",
        "VoxelCount", "AreaMm2", "VolumeMm3",
        "Percentile5", "Percentile25", "Percentile75", "Percentile95",
        "Skewness", "Kurtosis", "Entropy"
    };
}

std::vector<std::string> RoiStatistics::getCsvRow() const {
    auto format = [](double val) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << val;
        return oss.str();
    };

    return {
        std::to_string(roiId),
        roiLabel,
        format(mean),
        format(stdDev),
        format(min),
        format(max),
        format(median),
        std::to_string(voxelCount),
        format(areaMm2),
        format(volumeMm3),
        format(percentile5),
        format(percentile25),
        format(percentile75),
        format(percentile95),
        format(skewness),
        format(kurtosis),
        format(entropy)
    };
}

// =============================================================================
// RoiStatisticsCalculator::Impl
// =============================================================================

class RoiStatisticsCalculator::Impl {
public:
    ImageType::Pointer image;
    std::array<double, 3> spacing = {1.0, 1.0, 1.0};

    double histogramMin = -1024.0;
    double histogramMax = 3071.0;
    int histogramBins = 256;

    ProgressCallback progressCallback;

    // Calculate basic statistics from a vector of values
    void calculateBasicStats(const std::vector<short>& values, RoiStatistics& stats) {
        if (values.empty()) return;

        const auto n = static_cast<double>(values.size());
        stats.voxelCount = static_cast<int64_t>(values.size());

        // Min, Max
        auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
        stats.min = static_cast<double>(*minIt);
        stats.max = static_cast<double>(*maxIt);

        // Mean
        double sum = std::accumulate(values.begin(), values.end(), 0.0);
        stats.mean = sum / n;

        // Standard deviation
        double sqSum = 0.0;
        for (auto v : values) {
            double diff = static_cast<double>(v) - stats.mean;
            sqSum += diff * diff;
        }
        stats.stdDev = std::sqrt(sqSum / n);

        // Median and percentiles (requires sorting)
        std::vector<short> sorted = values;
        std::sort(sorted.begin(), sorted.end());

        auto percentile = [&sorted](double p) -> double {
            double idx = p * static_cast<double>(sorted.size() - 1);
            size_t lower = static_cast<size_t>(std::floor(idx));
            size_t upper = static_cast<size_t>(std::ceil(idx));
            if (lower == upper) {
                return static_cast<double>(sorted[lower]);
            }
            double frac = idx - std::floor(idx);
            return static_cast<double>(sorted[lower]) * (1.0 - frac) +
                   static_cast<double>(sorted[upper]) * frac;
        };

        stats.median = percentile(0.5);
        stats.percentile5 = percentile(0.05);
        stats.percentile25 = percentile(0.25);
        stats.percentile75 = percentile(0.75);
        stats.percentile95 = percentile(0.95);

        // Skewness and Kurtosis
        if (stats.stdDev > 0.0) {
            double skewSum = 0.0;
            double kurtSum = 0.0;
            for (auto v : values) {
                double z = (static_cast<double>(v) - stats.mean) / stats.stdDev;
                skewSum += z * z * z;
                kurtSum += z * z * z * z;
            }
            stats.skewness = skewSum / n;
            stats.kurtosis = kurtSum / n - 3.0; // Excess kurtosis
        }

        // Histogram
        calculateHistogram(values, stats);

        // Entropy from histogram
        calculateEntropy(stats);
    }

    void calculateHistogram(const std::vector<short>& values, RoiStatistics& stats) {
        stats.histogram.resize(histogramBins, 0);
        stats.histogramMin = histogramMin;
        stats.histogramMax = histogramMax;
        stats.histogramBins = histogramBins;

        double binWidth = (histogramMax - histogramMin) / static_cast<double>(histogramBins);

        for (auto v : values) {
            double dv = static_cast<double>(v);
            if (dv < histogramMin || dv > histogramMax) continue;

            int bin = static_cast<int>((dv - histogramMin) / binWidth);
            if (bin >= histogramBins) bin = histogramBins - 1;
            if (bin < 0) bin = 0;

            stats.histogram[bin]++;
        }
    }

    void calculateEntropy(RoiStatistics& stats) {
        if (stats.voxelCount == 0) {
            stats.entropy = 0.0;
            return;
        }

        double entropy = 0.0;
        for (auto count : stats.histogram) {
            if (count > 0) {
                double p = static_cast<double>(count) / static_cast<double>(stats.voxelCount);
                entropy -= p * std::log2(p);
            }
        }
        stats.entropy = entropy;
    }

    // Create binary mask for 2D ROI
    std::vector<bool> createRoiMask(const AreaMeasurement& roi,
                                     int sliceWidth, int sliceHeight,
                                     int sliceIndex) {
        std::vector<bool> mask(sliceWidth * sliceHeight, false);

        // Get image origin and spacing for coordinate conversion
        auto origin = image->GetOrigin();
        auto imgSpacing = image->GetSpacing();

        switch (roi.type) {
            case RoiType::Rectangle:
                fillRectangleMask(roi, mask, sliceWidth, sliceHeight, origin, imgSpacing);
                break;
            case RoiType::Ellipse:
                fillEllipseMask(roi, mask, sliceWidth, sliceHeight, origin, imgSpacing);
                break;
            case RoiType::Polygon:
            case RoiType::Freehand:
                fillPolygonMask(roi, mask, sliceWidth, sliceHeight, origin, imgSpacing);
                break;
        }

        return mask;
    }

    void fillRectangleMask(const AreaMeasurement& roi, std::vector<bool>& mask,
                           int width, int height,
                           const ImageType::PointType& origin,
                           const ImageType::SpacingType& imgSpacing) {
        if (roi.points.size() < 2) return;

        // Convert world coordinates to pixel coordinates
        int minX = static_cast<int>((roi.points[0][0] - origin[0]) / imgSpacing[0]);
        int minY = static_cast<int>((roi.points[0][1] - origin[1]) / imgSpacing[1]);
        int maxX = static_cast<int>((roi.points[1][0] - origin[0]) / imgSpacing[0]);
        int maxY = static_cast<int>((roi.points[1][1] - origin[1]) / imgSpacing[1]);

        if (minX > maxX) std::swap(minX, maxX);
        if (minY > maxY) std::swap(minY, maxY);

        // Clamp to image bounds
        minX = std::max(0, minX);
        minY = std::max(0, minY);
        maxX = std::min(width - 1, maxX);
        maxY = std::min(height - 1, maxY);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                mask[y * width + x] = true;
            }
        }
    }

    void fillEllipseMask(const AreaMeasurement& roi, std::vector<bool>& mask,
                         int width, int height,
                         const ImageType::PointType& origin,
                         const ImageType::SpacingType& imgSpacing) {
        // Centroid and semi-axes from measurement
        double cx = (roi.centroid[0] - origin[0]) / imgSpacing[0];
        double cy = (roi.centroid[1] - origin[1]) / imgSpacing[1];
        double a = roi.semiAxisA / imgSpacing[0];
        double b = roi.semiAxisB / imgSpacing[1];

        if (a <= 0 || b <= 0) return;

        int minX = std::max(0, static_cast<int>(std::floor(cx - a)));
        int maxX = std::min(width - 1, static_cast<int>(std::ceil(cx + a)));
        int minY = std::max(0, static_cast<int>(std::floor(cy - b)));
        int maxY = std::min(height - 1, static_cast<int>(std::ceil(cy + b)));

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                double dx = (static_cast<double>(x) - cx) / a;
                double dy = (static_cast<double>(y) - cy) / b;
                if (dx * dx + dy * dy <= 1.0) {
                    mask[y * width + x] = true;
                }
            }
        }
    }

    void fillPolygonMask(const AreaMeasurement& roi, std::vector<bool>& mask,
                         int width, int height,
                         const ImageType::PointType& origin,
                         const ImageType::SpacingType& imgSpacing) {
        if (roi.points.size() < 3) return;

        // Convert world coordinates to pixel coordinates
        std::vector<std::pair<double, double>> pixelPoints;
        pixelPoints.reserve(roi.points.size());
        for (const auto& pt : roi.points) {
            pixelPoints.emplace_back(
                (pt[0] - origin[0]) / imgSpacing[0],
                (pt[1] - origin[1]) / imgSpacing[1]
            );
        }

        // Find bounding box
        double minX = pixelPoints[0].first, maxX = pixelPoints[0].first;
        double minY = pixelPoints[0].second, maxY = pixelPoints[0].second;
        for (const auto& pt : pixelPoints) {
            minX = std::min(minX, pt.first);
            maxX = std::max(maxX, pt.first);
            minY = std::min(minY, pt.second);
            maxY = std::max(maxY, pt.second);
        }

        int iMinX = std::max(0, static_cast<int>(std::floor(minX)));
        int iMaxX = std::min(width - 1, static_cast<int>(std::ceil(maxX)));
        int iMinY = std::max(0, static_cast<int>(std::floor(minY)));
        int iMaxY = std::min(height - 1, static_cast<int>(std::ceil(maxY)));

        // Point-in-polygon test using ray casting
        for (int y = iMinY; y <= iMaxY; ++y) {
            for (int x = iMinX; x <= iMaxX; ++x) {
                if (pointInPolygon(static_cast<double>(x), static_cast<double>(y), pixelPoints)) {
                    mask[y * width + x] = true;
                }
            }
        }
    }

    bool pointInPolygon(double x, double y,
                        const std::vector<std::pair<double, double>>& polygon) {
        bool inside = false;
        size_t n = polygon.size();

        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            double xi = polygon[i].first, yi = polygon[i].second;
            double xj = polygon[j].first, yj = polygon[j].second;

            if (((yi > y) != (yj > y)) &&
                (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        return inside;
    }
};

// =============================================================================
// RoiStatisticsCalculator implementation
// =============================================================================

RoiStatisticsCalculator::RoiStatisticsCalculator()
    : impl_(std::make_unique<Impl>())
{
}

RoiStatisticsCalculator::~RoiStatisticsCalculator() = default;

RoiStatisticsCalculator::RoiStatisticsCalculator(RoiStatisticsCalculator&&) noexcept = default;
RoiStatisticsCalculator& RoiStatisticsCalculator::operator=(RoiStatisticsCalculator&&) noexcept = default;

void RoiStatisticsCalculator::setImage(ImageType::Pointer image) {
    impl_->image = image;
    if (image) {
        auto spacing = image->GetSpacing();
        impl_->spacing = {spacing[0], spacing[1], spacing[2]};
    }
}

void RoiStatisticsCalculator::setPixelSpacing(double spacingX, double spacingY, double spacingZ) {
    impl_->spacing = {spacingX, spacingY, spacingZ};
}

void RoiStatisticsCalculator::setHistogramParameters(double minValue, double maxValue, int numBins) {
    impl_->histogramMin = minValue;
    impl_->histogramMax = maxValue;
    impl_->histogramBins = numBins;
}

void RoiStatisticsCalculator::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<RoiStatistics, StatisticsError>
RoiStatisticsCalculator::calculate(const AreaMeasurement& roi, int sliceIndex) {
    if (!impl_->image) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::InvalidImage,
            "No image set"
        });
    }

    auto region = impl_->image->GetLargestPossibleRegion();
    auto size = region.GetSize();

    if (sliceIndex < 0 || static_cast<size_t>(sliceIndex) >= size[2]) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::InvalidRoi,
            "Slice index out of range"
        });
    }

    int width = static_cast<int>(size[0]);
    int height = static_cast<int>(size[1]);

    // Create mask for ROI
    auto mask = impl_->createRoiMask(roi, width, height, sliceIndex);

    // Extract pixel values within mask
    std::vector<short> values;
    values.reserve(roi.areaMm2 > 0 ? static_cast<size_t>(roi.areaMm2) : 1000);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (mask[y * width + x]) {
                ImageType::IndexType idx = {x, y, sliceIndex};
                values.push_back(impl_->image->GetPixel(idx));
            }
        }
    }

    if (values.empty()) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::NoPixelsInRoi,
            "No pixels found within ROI"
        });
    }

    RoiStatistics stats;
    stats.roiId = roi.id;
    stats.roiLabel = roi.label;
    stats.areaMm2 = roi.areaMm2;

    impl_->calculateBasicStats(values, stats);

    return stats;
}

std::expected<RoiStatistics, StatisticsError>
RoiStatisticsCalculator::calculate(LabelMapType::Pointer labelMap, uint8_t labelId) {
    if (!impl_->image) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::InvalidImage,
            "No image set"
        });
    }

    if (!labelMap) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::InvalidRoi,
            "No label map provided"
        });
    }

    // Use ITK LabelStatisticsImageFilter
    using FilterType = itk::LabelStatisticsImageFilter<ImageType, LabelMapType>;
    auto filter = FilterType::New();
    filter->SetInput(impl_->image);
    filter->SetLabelInput(labelMap);
    filter->UseHistogramsOn();
    filter->SetHistogramParameters(impl_->histogramBins, impl_->histogramMin, impl_->histogramMax);

    try {
        filter->Update();
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::CalculationFailed,
            e.GetDescription()
        });
    }

    if (!filter->HasLabel(labelId)) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::NoPixelsInRoi,
            "Label not found in label map"
        });
    }

    RoiStatistics stats;
    stats.roiId = labelId;
    stats.mean = filter->GetMean(labelId);
    stats.stdDev = filter->GetSigma(labelId);
    stats.min = filter->GetMinimum(labelId);
    stats.max = filter->GetMaximum(labelId);
    stats.median = filter->GetMedian(labelId);
    stats.voxelCount = static_cast<int64_t>(filter->GetCount(labelId));

    // Calculate volume
    double voxelVolume = impl_->spacing[0] * impl_->spacing[1] * impl_->spacing[2];
    stats.volumeMm3 = static_cast<double>(stats.voxelCount) * voxelVolume;

    // Copy histogram
    auto histogram = filter->GetHistogram(labelId);
    if (histogram) {
        stats.histogram.resize(impl_->histogramBins);
        for (int i = 0; i < impl_->histogramBins; ++i) {
            stats.histogram[i] = static_cast<int64_t>(histogram->GetFrequency(i));
        }
        stats.histogramMin = impl_->histogramMin;
        stats.histogramMax = impl_->histogramMax;
        stats.histogramBins = impl_->histogramBins;
    }

    // Calculate entropy
    impl_->calculateEntropy(stats);

    return stats;
}

std::vector<std::expected<RoiStatistics, StatisticsError>>
RoiStatisticsCalculator::calculateMultiple(const std::vector<AreaMeasurement>& rois, int sliceIndex) {
    std::vector<std::expected<RoiStatistics, StatisticsError>> results;
    results.reserve(rois.size());

    for (size_t i = 0; i < rois.size(); ++i) {
        results.push_back(calculate(rois[i], sliceIndex));

        if (impl_->progressCallback) {
            impl_->progressCallback(static_cast<double>(i + 1) / static_cast<double>(rois.size()));
        }
    }

    return results;
}

std::expected<void, StatisticsError>
RoiStatisticsCalculator::exportMultipleToCsv(const std::vector<RoiStatistics>& statistics,
                                              const std::filesystem::path& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(StatisticsError{
            StatisticsError::Code::ExportFailed,
            "Failed to open file: " + filePath.string()
        });
    }

    // Write header
    auto header = RoiStatistics::getCsvHeader();
    for (size_t i = 0; i < header.size(); ++i) {
        file << header[i];
        if (i < header.size() - 1) file << ",";
    }
    file << "\n";

    // Write data rows
    for (const auto& stats : statistics) {
        auto row = stats.getCsvRow();
        for (size_t i = 0; i < row.size(); ++i) {
            file << row[i];
            if (i < row.size() - 1) file << ",";
        }
        file << "\n";
    }

    return {};
}

std::string RoiStatisticsCalculator::compareStatistics(const RoiStatistics& stats1,
                                                        const RoiStatistics& stats2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "Comparison: " << stats1.roiLabel << " vs " << stats2.roiLabel << "\n";
    oss << "-------------------------------------------\n";
    oss << "Metric           | " << std::setw(12) << stats1.roiLabel
        << " | " << std::setw(12) << stats2.roiLabel << " | Diff\n";
    oss << "-------------------------------------------\n";

    auto row = [&oss](const std::string& name, double v1, double v2) {
        oss << std::left << std::setw(16) << name << " | "
            << std::right << std::setw(12) << v1 << " | "
            << std::setw(12) << v2 << " | "
            << std::setw(10) << (v2 - v1) << "\n";
    };

    row("Mean", stats1.mean, stats2.mean);
    row("Std Dev", stats1.stdDev, stats2.stdDev);
    row("Min", stats1.min, stats2.min);
    row("Max", stats1.max, stats2.max);
    row("Median", stats1.median, stats2.median);

    return oss.str();
}

}  // namespace dicom_viewer::services

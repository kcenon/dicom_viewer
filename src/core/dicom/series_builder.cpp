#include "core/series_builder.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <future>
#include <numeric>

#include <kcenon/common/logging/log_macros.h>

namespace dicom_viewer::core {

class SeriesBuilder::Impl {
public:
    DicomLoader loader;
    ProgressCallback progressCallback;
    DicomMetadata lastMetadata;

    void reportProgress(size_t current, size_t total, const std::string& message)
    {
        if (progressCallback) {
            progressCallback(current, total, message);
        }
    }
};

SeriesBuilder::SeriesBuilder()
    : impl_(std::make_unique<Impl>())
{
}

SeriesBuilder::~SeriesBuilder() = default;

SeriesBuilder::SeriesBuilder(SeriesBuilder&&) noexcept = default;
SeriesBuilder& SeriesBuilder::operator=(SeriesBuilder&&) noexcept = default;

void SeriesBuilder::setProgressCallback(ProgressCallback callback)
{
    impl_->progressCallback = std::move(callback);
}

std::expected<std::vector<SeriesInfo>, DicomErrorInfo>
SeriesBuilder::scanForSeries(const std::filesystem::path& directoryPath)
{
    LOG_INFO(std::format("Scanning for series in: {}", directoryPath.string()));
    impl_->reportProgress(0, 100, "Scanning directory...");

    auto scanResult = impl_->loader.scanDirectory(directoryPath);
    if (!scanResult) {
        LOG_ERROR(std::format("Failed to scan directory: {}", scanResult.error().message));
        return std::unexpected(scanResult.error());
    }

    std::vector<SeriesInfo> seriesInfoList;
    const auto& seriesMap = scanResult.value();
    size_t index = 0;
    size_t total = seriesMap.size();

    for (const auto& [uid, slices] : seriesMap) {
        impl_->reportProgress(index, total, "Processing series: " + uid.substr(0, 20) + "...");

        SeriesInfo info;
        info.seriesInstanceUid = uid;
        info.slices = slices;
        info.sliceCount = slices.size();

        // Load metadata from first slice
        if (!slices.empty()) {
            auto metaResult = impl_->loader.loadFile(slices.front().filePath);
            if (metaResult) {
                info.seriesDescription = metaResult->seriesDescription;
                info.modality = metaResult->modality;
                info.pixelSpacingX = metaResult->pixelSpacingX;
                info.pixelSpacingY = metaResult->pixelSpacingY;
                info.dimensions[0] = metaResult->columns;
                info.dimensions[1] = metaResult->rows;
                info.dimensions[2] = slices.size();
            }
        }

        // Calculate slice spacing
        if (slices.size() >= 2) {
            info.sliceSpacing = calculateSliceSpacing(slices);
        }

        seriesInfoList.push_back(std::move(info));
        ++index;
    }

    LOG_INFO(std::format("Scan complete: found {} series", seriesInfoList.size()));
    impl_->reportProgress(total, total, "Scan complete");
    return seriesInfoList;
}

std::expected<CTImageType::Pointer, DicomErrorInfo>
SeriesBuilder::buildCTVolume(const SeriesInfo& series)
{
    LOG_INFO(std::format("Building CT volume for series: {}", series.seriesInstanceUid.substr(0, 20)));

    if (series.slices.empty()) {
        LOG_ERROR("No slices in series");
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            "No slices in series"
        });
    }

    impl_->reportProgress(0, 100, "Building CT volume...");

    if (!validateSeriesConsistency(series.slices)) {
        LOG_WARNING("Inconsistent slice spacing detected in series");
        impl_->reportProgress(10, 100, "Warning: Inconsistent slice spacing detected");
    }

    impl_->reportProgress(20, 100, "Loading slices...");

    auto result = impl_->loader.loadCTSeries(series.slices);
    if (result) {
        impl_->lastMetadata = impl_->loader.getMetadata();
        LOG_INFO("CT volume built successfully");
        impl_->reportProgress(100, 100, "Volume built successfully");
    } else {
        LOG_ERROR(std::format("Failed to build CT volume: {}", result.error().message));
    }

    return result;
}

std::expected<MRImageType::Pointer, DicomErrorInfo>
SeriesBuilder::buildMRVolume(const SeriesInfo& series)
{
    LOG_INFO(std::format("Building MR volume for series: {}", series.seriesInstanceUid.substr(0, 20)));

    if (series.slices.empty()) {
        LOG_ERROR("No slices in series");
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            "No slices in series"
        });
    }

    impl_->reportProgress(0, 100, "Building MR volume...");

    if (!validateSeriesConsistency(series.slices)) {
        LOG_WARNING("Inconsistent slice spacing detected in series");
        impl_->reportProgress(10, 100, "Warning: Inconsistent slice spacing detected");
    }

    impl_->reportProgress(20, 100, "Loading slices...");

    auto result = impl_->loader.loadMRSeries(series.slices);
    if (result) {
        impl_->lastMetadata = impl_->loader.getMetadata();
        LOG_INFO("MR volume built successfully");
        impl_->reportProgress(100, 100, "Volume built successfully");
    } else {
        LOG_ERROR(std::format("Failed to build MR volume: {}", result.error().message));
    }

    return result;
}

std::future<std::expected<CTImageType::Pointer, DicomErrorInfo>>
SeriesBuilder::buildCTVolumeAsync(const SeriesInfo& series)
{
    return std::async(std::launch::async,
        [this, series]() {
            return buildCTVolume(series);
        });
}

const DicomMetadata& SeriesBuilder::getMetadata() const
{
    return impl_->lastMetadata;
}

double SeriesBuilder::calculateSliceSpacing(const std::vector<SliceInfo>& slices)
{
    if (slices.size() < 2) {
        return 1.0;
    }

    // Sort slices by position along slice normal for order-independent calculation
    auto sorted = slices;
    const auto& ori = sorted[0].imageOrientation;
    // Compute slice normal as cross product of row and column direction cosines
    double nx = ori[1] * ori[5] - ori[2] * ori[4];
    double ny = ori[2] * ori[3] - ori[0] * ori[5];
    double nz = ori[0] * ori[4] - ori[1] * ori[3];

    std::sort(sorted.begin(), sorted.end(),
        [nx, ny, nz](const SliceInfo& a, const SliceInfo& b) {
            double projA = a.imagePosition[0] * nx
                         + a.imagePosition[1] * ny
                         + a.imagePosition[2] * nz;
            double projB = b.imagePosition[0] * nx
                         + b.imagePosition[1] * ny
                         + b.imagePosition[2] * nz;
            return projA < projB;
        });

    // Calculate distances between consecutive sorted slices
    std::vector<double> spacings;
    spacings.reserve(sorted.size() - 1);

    for (size_t i = 1; i < sorted.size(); ++i) {
        const auto& prev = sorted[i - 1];
        const auto& curr = sorted[i];

        // Calculate 3D distance between slice positions
        double dx = curr.imagePosition[0] - prev.imagePosition[0];
        double dy = curr.imagePosition[1] - prev.imagePosition[1];
        double dz = curr.imagePosition[2] - prev.imagePosition[2];
        double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (distance > 1e-6) {
            spacings.push_back(distance);
        }
    }

    if (spacings.empty()) {
        // Fallback: use slice location difference
        double locDiff = std::abs(sorted.back().sliceLocation - sorted.front().sliceLocation);
        return locDiff / (sorted.size() - 1);
    }

    // Return median spacing for robustness against outliers
    std::sort(spacings.begin(), spacings.end());
    return spacings[spacings.size() / 2];
}

bool SeriesBuilder::validateSeriesConsistency(const std::vector<SliceInfo>& slices)
{
    if (slices.size() < 2) {
        return true;
    }

    // Check slice spacing consistency
    double expectedSpacing = calculateSliceSpacing(slices);
    constexpr double tolerance = 0.1; // 10% tolerance

    for (size_t i = 1; i < slices.size(); ++i) {
        const auto& prev = slices[i - 1];
        const auto& curr = slices[i];

        double dx = curr.imagePosition[0] - prev.imagePosition[0];
        double dy = curr.imagePosition[1] - prev.imagePosition[1];
        double dz = curr.imagePosition[2] - prev.imagePosition[2];
        double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (std::abs(distance - expectedSpacing) > expectedSpacing * tolerance) {
            return false;
        }
    }

    // Check orientation consistency
    const auto& refOrientation = slices.front().imageOrientation;
    for (const auto& slice : slices) {
        for (size_t i = 0; i < 6; ++i) {
            if (std::abs(slice.imageOrientation[i] - refOrientation[i]) > 1e-4) {
                return false;
            }
        }
    }

    return true;
}

} // namespace dicom_viewer::core

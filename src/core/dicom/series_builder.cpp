#include "core/series_builder.hpp"

#include <algorithm>
#include <cmath>
#include <future>
#include <numeric>

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
    impl_->reportProgress(0, 100, "Scanning directory...");

    auto scanResult = impl_->loader.scanDirectory(directoryPath);
    if (!scanResult) {
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

    impl_->reportProgress(total, total, "Scan complete");
    return seriesInfoList;
}

std::expected<CTImageType::Pointer, DicomErrorInfo>
SeriesBuilder::buildCTVolume(const SeriesInfo& series)
{
    if (series.slices.empty()) {
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            "No slices in series"
        });
    }

    impl_->reportProgress(0, 100, "Building CT volume...");

    // Validate series consistency
    if (!validateSeriesConsistency(series.slices)) {
        impl_->reportProgress(10, 100, "Warning: Inconsistent slice spacing detected");
    }

    impl_->reportProgress(20, 100, "Loading slices...");

    auto result = impl_->loader.loadCTSeries(series.slices);
    if (result) {
        impl_->lastMetadata = impl_->loader.getMetadata();
        impl_->reportProgress(100, 100, "Volume built successfully");
    }

    return result;
}

std::expected<MRImageType::Pointer, DicomErrorInfo>
SeriesBuilder::buildMRVolume(const SeriesInfo& series)
{
    if (series.slices.empty()) {
        return std::unexpected(DicomErrorInfo{
            DicomError::SeriesAssemblyFailed,
            "No slices in series"
        });
    }

    impl_->reportProgress(0, 100, "Building MR volume...");

    if (!validateSeriesConsistency(series.slices)) {
        impl_->reportProgress(10, 100, "Warning: Inconsistent slice spacing detected");
    }

    impl_->reportProgress(20, 100, "Loading slices...");

    auto result = impl_->loader.loadMRSeries(series.slices);
    if (result) {
        impl_->lastMetadata = impl_->loader.getMetadata();
        impl_->reportProgress(100, 100, "Volume built successfully");
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

    // Calculate distances between consecutive slices
    std::vector<double> spacings;
    spacings.reserve(slices.size() - 1);

    for (size_t i = 1; i < slices.size(); ++i) {
        const auto& prev = slices[i - 1];
        const auto& curr = slices[i];

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
        double locDiff = std::abs(slices.back().sliceLocation - slices.front().sliceLocation);
        return locDiff / (slices.size() - 1);
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

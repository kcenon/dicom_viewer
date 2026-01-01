#include "services/segmentation/region_growing_segmenter.hpp"
#include "services/segmentation/threshold_segmenter.hpp"

#include <itkConnectedThresholdImageFilter.h>
#include <itkConfidenceConnectedImageFilter.h>
#include <itkCommand.h>

namespace dicom_viewer::services {

namespace {

/**
 * @brief ITK progress observer for callback integration
 */
class ProgressObserver : public itk::Command {
public:
    using Self = ProgressObserver;
    using Superclass = itk::Command;
    using Pointer = itk::SmartPointer<Self>;

    itkNewMacro(Self);

    void setCallback(RegionGrowingSegmenter::ProgressCallback callback) {
        callback_ = std::move(callback);
    }

    void Execute(itk::Object* caller, const itk::EventObject& event) override {
        Execute(static_cast<const itk::Object*>(caller), event);
    }

    void Execute(const itk::Object* caller, const itk::EventObject& event) override {
        if (!callback_) return;

        if (itk::ProgressEvent().CheckEvent(&event)) {
            const auto* process = dynamic_cast<const itk::ProcessObject*>(caller);
            if (process) {
                callback_(process->GetProgress());
            }
        }
    }

private:
    RegionGrowingSegmenter::ProgressCallback callback_;
};

} // anonymous namespace

std::expected<RegionGrowingSegmenter::BinaryMaskType::Pointer, SegmentationError>
RegionGrowingSegmenter::connectedThreshold(
    ImageType::Pointer input,
    const std::vector<SeedPoint>& seeds,
    double lowerThreshold,
    double upperThreshold
) const {
    ConnectedThresholdParameters params;
    params.seeds = seeds;
    params.lowerThreshold = lowerThreshold;
    params.upperThreshold = upperThreshold;
    return connectedThreshold(input, params);
}

std::expected<RegionGrowingSegmenter::BinaryMaskType::Pointer, SegmentationError>
RegionGrowingSegmenter::connectedThreshold(
    ImageType::Pointer input,
    const ConnectedThresholdParameters& params
) const {
    // Validate input
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    // Validate parameters
    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid parameters: seeds empty or lower > upper threshold"
        });
    }

    // Validate seed points
    if (auto error = validateSeeds(input, params.seeds)) {
        return std::unexpected(*error);
    }

    try {
        using FilterType = itk::ConnectedThresholdImageFilter<ImageType, BinaryMaskType>;
        auto filter = FilterType::New();

        filter->SetInput(input);
        filter->SetLower(static_cast<ImageType::PixelType>(params.lowerThreshold));
        filter->SetUpper(static_cast<ImageType::PixelType>(params.upperThreshold));
        filter->SetReplaceValue(params.replaceValue);

        // Add all seed points
        for (const auto& seed : params.seeds) {
            ImageType::IndexType index;
            index[0] = seed.x;
            index[1] = seed.y;
            index[2] = seed.z;
            filter->AddSeed(index);
        }

        // Attach progress observer if callback is set
        if (progressCallback_) {
            auto observer = ProgressObserver::New();
            observer->setCallback(progressCallback_);
            filter->AddObserver(itk::ProgressEvent(), observer);
        }

        filter->Update();

        return filter->GetOutput();
    }
    catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

std::expected<RegionGrowingSegmenter::BinaryMaskType::Pointer, SegmentationError>
RegionGrowingSegmenter::confidenceConnected(
    ImageType::Pointer input,
    const std::vector<SeedPoint>& seeds,
    double multiplier,
    unsigned int iterations
) const {
    ConfidenceConnectedParameters params;
    params.seeds = seeds;
    params.multiplier = multiplier;
    params.numberOfIterations = iterations;
    return confidenceConnected(input, params);
}

std::expected<RegionGrowingSegmenter::BinaryMaskType::Pointer, SegmentationError>
RegionGrowingSegmenter::confidenceConnected(
    ImageType::Pointer input,
    const ConfidenceConnectedParameters& params
) const {
    // Validate input
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    // Validate parameters
    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid parameters: seeds empty, multiplier <= 0, or iterations = 0"
        });
    }

    // Validate seed points
    if (auto error = validateSeeds(input, params.seeds)) {
        return std::unexpected(*error);
    }

    try {
        using FilterType = itk::ConfidenceConnectedImageFilter<ImageType, BinaryMaskType>;
        auto filter = FilterType::New();

        filter->SetInput(input);
        filter->SetMultiplier(params.multiplier);
        filter->SetNumberOfIterations(params.numberOfIterations);
        filter->SetInitialNeighborhoodRadius(params.initialNeighborhoodRadius);
        filter->SetReplaceValue(params.replaceValue);

        // Add all seed points
        for (const auto& seed : params.seeds) {
            ImageType::IndexType index;
            index[0] = seed.x;
            index[1] = seed.y;
            index[2] = seed.z;
            filter->AddSeed(index);
        }

        // Attach progress observer if callback is set
        if (progressCallback_) {
            auto observer = ProgressObserver::New();
            observer->setCallback(progressCallback_);
            filter->AddObserver(itk::ProgressEvent(), observer);
        }

        filter->Update();

        return filter->GetOutput();
    }
    catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

bool RegionGrowingSegmenter::isValidSeedPoint(
    ImageType::Pointer input,
    const SeedPoint& seed
) {
    if (!input) {
        return false;
    }

    auto region = input->GetLargestPossibleRegion();
    auto size = region.GetSize();
    auto startIndex = region.GetIndex();

    return seed.x >= startIndex[0] && seed.x < startIndex[0] + static_cast<int>(size[0]) &&
           seed.y >= startIndex[1] && seed.y < startIndex[1] + static_cast<int>(size[1]) &&
           seed.z >= startIndex[2] && seed.z < startIndex[2] + static_cast<int>(size[2]);
}

std::optional<SegmentationError> RegionGrowingSegmenter::validateSeeds(
    ImageType::Pointer input,
    const std::vector<SeedPoint>& seeds
) const {
    for (size_t i = 0; i < seeds.size(); ++i) {
        if (!isValidSeedPoint(input, seeds[i])) {
            return SegmentationError{
                SegmentationError::Code::InvalidParameters,
                "Seed point " + std::to_string(i) + " (" +
                std::to_string(seeds[i].x) + ", " +
                std::to_string(seeds[i].y) + ", " +
                std::to_string(seeds[i].z) + ") is out of image bounds"
            };
        }
    }
    return std::nullopt;
}

void RegionGrowingSegmenter::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

} // namespace dicom_viewer::services

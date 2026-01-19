#include "services/segmentation/threshold_segmenter.hpp"
#include "core/logging.hpp"

#include <itkBinaryThresholdImageFilter.h>
#include <itkOtsuThresholdImageFilter.h>
#include <itkOtsuMultipleThresholdsImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkCommand.h>

namespace dicom_viewer::services {

namespace {
auto& getLogger() {
    static auto logger = logging::LoggerFactory::create("ThresholdSegmenter");
    return logger;
}
}

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

    void setCallback(ThresholdSegmenter::ProgressCallback callback) {
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
    ThresholdSegmenter::ProgressCallback callback_;
};

} // anonymous namespace

std::expected<ThresholdSegmenter::BinaryMaskType::Pointer, SegmentationError>
ThresholdSegmenter::manualThreshold(
    ImageType::Pointer input,
    double lowerThreshold,
    double upperThreshold
) const {
    ThresholdParameters params;
    params.lowerThreshold = lowerThreshold;
    params.upperThreshold = upperThreshold;
    return manualThreshold(input, params);
}

std::expected<ThresholdSegmenter::BinaryMaskType::Pointer, SegmentationError>
ThresholdSegmenter::manualThreshold(
    ImageType::Pointer input,
    const ThresholdParameters& params
) const {
    getLogger()->info("Manual threshold: [{:.1f}, {:.1f}]", params.lowerThreshold, params.upperThreshold);

    if (!input) {
        getLogger()->error("Input image is null");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        getLogger()->error("Invalid parameters: lower > upper");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Lower threshold must be <= upper threshold"
        });
    }

    try {
        using FilterType = itk::BinaryThresholdImageFilter<ImageType, BinaryMaskType>;
        auto filter = FilterType::New();

        filter->SetInput(input);
        filter->SetLowerThreshold(static_cast<ImageType::PixelType>(params.lowerThreshold));
        filter->SetUpperThreshold(static_cast<ImageType::PixelType>(params.upperThreshold));
        filter->SetInsideValue(params.insideValue);
        filter->SetOutsideValue(params.outsideValue);

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

std::expected<OtsuThresholdResult, SegmentationError>
ThresholdSegmenter::otsuThreshold(ImageType::Pointer input) const {
    return otsuThreshold(input, OtsuParameters{});
}

std::expected<OtsuThresholdResult, SegmentationError>
ThresholdSegmenter::otsuThreshold(
    ImageType::Pointer input,
    const OtsuParameters& params
) const {
    // Validate input
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    try {
        using FilterType = itk::OtsuThresholdImageFilter<ImageType, BinaryMaskType>;
        auto filter = FilterType::New();

        filter->SetInput(input);
        filter->SetNumberOfHistogramBins(params.numberOfHistogramBins);
        filter->SetInsideValue(1);
        filter->SetOutsideValue(0);

        // Attach progress observer if callback is set
        if (progressCallback_) {
            auto observer = ProgressObserver::New();
            observer->setCallback(progressCallback_);
            filter->AddObserver(itk::ProgressEvent(), observer);
        }

        filter->Update();

        OtsuThresholdResult result;
        result.threshold = static_cast<double>(filter->GetThreshold());
        result.mask = filter->GetOutput();

        return result;
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

std::expected<OtsuMultiThresholdResult, SegmentationError>
ThresholdSegmenter::otsuMultiThreshold(
    ImageType::Pointer input,
    unsigned int numThresholds
) const {
    return otsuMultiThreshold(input, numThresholds, OtsuParameters{});
}

std::expected<OtsuMultiThresholdResult, SegmentationError>
ThresholdSegmenter::otsuMultiThreshold(
    ImageType::Pointer input,
    unsigned int numThresholds,
    const OtsuParameters& params
) const {
    // Validate input
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    // Validate number of thresholds
    if (numThresholds < 1 || numThresholds > 255) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Number of thresholds must be between 1 and 255"
        });
    }

    try {
        using FilterType = itk::OtsuMultipleThresholdsImageFilter<ImageType, LabelMapType>;
        auto filter = FilterType::New();

        filter->SetInput(input);
        filter->SetNumberOfThresholds(numThresholds);
        filter->SetNumberOfHistogramBins(params.numberOfHistogramBins);
        filter->SetValleyEmphasis(params.valleyEmphasis);

        // Attach progress observer if callback is set
        if (progressCallback_) {
            auto observer = ProgressObserver::New();
            observer->setCallback(progressCallback_);
            filter->AddObserver(itk::ProgressEvent(), observer);
        }

        filter->Update();

        OtsuMultiThresholdResult result;

        // Copy thresholds
        const auto& thresholds = filter->GetThresholds();
        result.thresholds.reserve(thresholds.size());
        for (const auto& t : thresholds) {
            result.thresholds.push_back(static_cast<double>(t));
        }

        result.labelMap = filter->GetOutput();

        return result;
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

std::expected<itk::Image<unsigned char, 2>::Pointer, SegmentationError>
ThresholdSegmenter::thresholdSlice(
    ImageType::Pointer input,
    unsigned int sliceIndex,
    double lowerThreshold,
    double upperThreshold
) const {
    // Validate input
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    // Validate parameters
    if (lowerThreshold > upperThreshold) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Lower threshold must be <= upper threshold"
        });
    }

    // Validate slice index
    auto region = input->GetLargestPossibleRegion();
    auto size = region.GetSize();
    if (sliceIndex >= size[2]) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Slice index out of range"
        });
    }

    try {
        // Extract 2D slice
        using ExtractFilterType = itk::ExtractImageFilter<ImageType, itk::Image<short, 2>>;
        auto extractFilter = ExtractFilterType::New();
        extractFilter->SetDirectionCollapseToSubmatrix();

        ImageType::RegionType extractRegion = region;
        extractRegion.SetSize(2, 0);  // Collapse Z dimension
        extractRegion.SetIndex(2, sliceIndex);

        extractFilter->SetExtractionRegion(extractRegion);
        extractFilter->SetInput(input);

        // Apply threshold to 2D slice
        using Image2DType = itk::Image<short, 2>;
        using Mask2DType = itk::Image<unsigned char, 2>;
        using ThresholdFilterType = itk::BinaryThresholdImageFilter<Image2DType, Mask2DType>;
        auto thresholdFilter = ThresholdFilterType::New();

        thresholdFilter->SetInput(extractFilter->GetOutput());
        thresholdFilter->SetLowerThreshold(static_cast<short>(lowerThreshold));
        thresholdFilter->SetUpperThreshold(static_cast<short>(upperThreshold));
        thresholdFilter->SetInsideValue(1);
        thresholdFilter->SetOutsideValue(0);

        thresholdFilter->Update();

        return thresholdFilter->GetOutput();
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

void ThresholdSegmenter::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

} // namespace dicom_viewer::services

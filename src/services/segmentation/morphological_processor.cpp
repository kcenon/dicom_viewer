#include "services/segmentation/morphological_processor.hpp"

#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryCrossStructuringElement.h>
#include <itkBinaryMorphologicalOpeningImageFilter.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkBinaryDilateImageFilter.h>
#include <itkBinaryErodeImageFilter.h>
#include <itkBinaryFillholeImageFilter.h>
#include <itkBinaryShapeKeepNObjectsImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkAddImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkCommand.h>

#include <set>

namespace dicom_viewer::services {

namespace {

/**
 * @brief ITK progress observer for callback integration
 */
class MorphProgressObserver : public itk::Command {
public:
    using Self = MorphProgressObserver;
    using Superclass = itk::Command;
    using Pointer = itk::SmartPointer<Self>;

    itkNewMacro(Self);

    void setCallback(MorphologicalProcessor::ProgressCallback callback) {
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
    MorphologicalProcessor::ProgressCallback callback_;
};

/**
 * @brief Create ball structuring element
 */
template <unsigned int Dimension>
auto createBallStructuringElement(int radius) {
    using PixelType = unsigned char;
    using StructuringElementType = itk::BinaryBallStructuringElement<PixelType, Dimension>;

    typename StructuringElementType::SizeType radiusSize;
    radiusSize.Fill(static_cast<typename StructuringElementType::SizeType::SizeValueType>(radius));

    StructuringElementType element;
    element.SetRadius(radiusSize);
    element.CreateStructuringElement();

    return element;
}

/**
 * @brief Create cross structuring element
 */
template <unsigned int Dimension>
auto createCrossStructuringElement(int radius) {
    using PixelType = unsigned char;
    using StructuringElementType = itk::BinaryCrossStructuringElement<PixelType, Dimension>;

    StructuringElementType element;
    element.SetRadius(static_cast<unsigned long>(radius));
    element.CreateStructuringElement();

    return element;
}

}  // anonymous namespace

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::apply(
    BinaryMaskType::Pointer input,
    MorphologicalOperation operation,
    const Parameters& params
) const {
    switch (operation) {
        case MorphologicalOperation::Opening:
            return opening(input, params);
        case MorphologicalOperation::Closing:
            return closing(input, params);
        case MorphologicalOperation::Dilation:
            return dilation(input, params);
        case MorphologicalOperation::Erosion:
            return erosion(input, params);
        case MorphologicalOperation::FillHoles:
            return fillHoles(input, params.foregroundValue);
        case MorphologicalOperation::IslandRemoval:
            return keepLargestComponents(input, 1);
    }

    return std::unexpected(SegmentationError{
        SegmentationError::Code::InvalidParameters,
        "Unknown morphological operation"
    });
}

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::apply(
    BinaryMaskType::Pointer input,
    MorphologicalOperation operation,
    int radius
) const {
    Parameters params;
    params.radius = radius;
    return apply(input, operation, params);
}

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::opening(
    BinaryMaskType::Pointer input,
    const Parameters& params
) const {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Radius must be between 1 and 10"
        });
    }

    try {
        if (params.structuringElement == StructuringElementShape::Ball) {
            using StructuringElementType = itk::BinaryBallStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryMorphologicalOpeningImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createBallStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);
            filter->SetBackgroundValue(params.backgroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        } else {
            using StructuringElementType = itk::BinaryCrossStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryMorphologicalOpeningImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createCrossStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);
            filter->SetBackgroundValue(params.backgroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        }
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

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::closing(
    BinaryMaskType::Pointer input,
    const Parameters& params
) const {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Radius must be between 1 and 10"
        });
    }

    try {
        if (params.structuringElement == StructuringElementShape::Ball) {
            using StructuringElementType = itk::BinaryBallStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryMorphologicalClosingImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createBallStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        } else {
            using StructuringElementType = itk::BinaryCrossStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryMorphologicalClosingImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createCrossStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        }
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

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::dilation(
    BinaryMaskType::Pointer input,
    const Parameters& params
) const {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Radius must be between 1 and 10"
        });
    }

    try {
        if (params.structuringElement == StructuringElementShape::Ball) {
            using StructuringElementType = itk::BinaryBallStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryDilateImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createBallStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);
            filter->SetBackgroundValue(params.backgroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        } else {
            using StructuringElementType = itk::BinaryCrossStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryDilateImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createCrossStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);
            filter->SetBackgroundValue(params.backgroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        }
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

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::erosion(
    BinaryMaskType::Pointer input,
    const Parameters& params
) const {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Radius must be between 1 and 10"
        });
    }

    try {
        if (params.structuringElement == StructuringElementShape::Ball) {
            using StructuringElementType = itk::BinaryBallStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryErodeImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createBallStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);
            filter->SetBackgroundValue(params.backgroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        } else {
            using StructuringElementType = itk::BinaryCrossStructuringElement<
                BinaryMaskType::PixelType, 3>;
            using FilterType = itk::BinaryErodeImageFilter<
                BinaryMaskType, BinaryMaskType, StructuringElementType>;

            auto element = createCrossStructuringElement<3>(params.radius);
            auto filter = FilterType::New();
            filter->SetInput(input);
            filter->SetKernel(element);
            filter->SetForegroundValue(params.foregroundValue);
            filter->SetBackgroundValue(params.backgroundValue);

            if (progressCallback_) {
                auto observer = MorphProgressObserver::New();
                observer->setCallback(progressCallback_);
                filter->AddObserver(itk::ProgressEvent(), observer);
            }

            filter->Update();
            return filter->GetOutput();
        }
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

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::fillHoles(
    BinaryMaskType::Pointer input,
    unsigned char foregroundValue
) const {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    try {
        using FilterType = itk::BinaryFillholeImageFilter<BinaryMaskType>;
        auto filter = FilterType::New();
        filter->SetInput(input);
        filter->SetForegroundValue(foregroundValue);
        filter->SetFullyConnected(true);

        if (progressCallback_) {
            auto observer = MorphProgressObserver::New();
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

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::keepLargestComponents(
    BinaryMaskType::Pointer input,
    int numComponents
) const {
    IslandRemovalParameters params;
    params.numberOfComponents = numComponents;
    return keepLargestComponents(input, params);
}

std::expected<MorphologicalProcessor::BinaryMaskType::Pointer, SegmentationError>
MorphologicalProcessor::keepLargestComponents(
    BinaryMaskType::Pointer input,
    const IslandRemovalParameters& params
) const {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Number of components must be between 1 and 255"
        });
    }

    try {
        using FilterType = itk::BinaryShapeKeepNObjectsImageFilter<BinaryMaskType>;
        auto filter = FilterType::New();
        filter->SetInput(input);
        filter->SetForegroundValue(params.foregroundValue);
        filter->SetBackgroundValue(0);
        filter->SetNumberOfObjects(static_cast<unsigned int>(params.numberOfComponents));
        filter->SetAttribute(params.sortByVolume ?
            FilterType::LabelObjectType::NUMBER_OF_PIXELS :
            FilterType::LabelObjectType::PHYSICAL_SIZE);

        if (progressCallback_) {
            auto observer = MorphProgressObserver::New();
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

std::expected<itk::Image<unsigned char, 2>::Pointer, SegmentationError>
MorphologicalProcessor::applyToSlice(
    BinaryMaskType::Pointer input,
    unsigned int sliceIndex,
    MorphologicalOperation operation,
    const Parameters& params
) const {
    if (!input) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    auto region = input->GetLargestPossibleRegion();
    auto size = region.GetSize();
    if (sliceIndex >= size[2]) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Slice index out of range"
        });
    }

    if (!params.isValid()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid parameters"
        });
    }

    try {
        // Extract 2D slice
        using Image2DType = itk::Image<unsigned char, 2>;
        using ExtractFilterType = itk::ExtractImageFilter<BinaryMaskType, Image2DType>;
        auto extractFilter = ExtractFilterType::New();
        extractFilter->SetDirectionCollapseToSubmatrix();

        BinaryMaskType::RegionType extractRegion = region;
        extractRegion.SetSize(2, 0);
        extractRegion.SetIndex(2, sliceIndex);
        extractFilter->SetExtractionRegion(extractRegion);
        extractFilter->SetInput(input);
        extractFilter->Update();

        auto slice2D = extractFilter->GetOutput();

        // Apply 2D morphological operation
        using StructuringElement2D = itk::BinaryBallStructuringElement<unsigned char, 2>;
        auto element = createBallStructuringElement<2>(params.radius);

        Image2DType::Pointer result;

        switch (operation) {
            case MorphologicalOperation::Opening: {
                using FilterType = itk::BinaryMorphologicalOpeningImageFilter<
                    Image2DType, Image2DType, StructuringElement2D>;
                auto filter = FilterType::New();
                filter->SetInput(slice2D);
                filter->SetKernel(element);
                filter->SetForegroundValue(params.foregroundValue);
                filter->SetBackgroundValue(params.backgroundValue);
                filter->Update();
                result = filter->GetOutput();
                break;
            }
            case MorphologicalOperation::Closing: {
                using FilterType = itk::BinaryMorphologicalClosingImageFilter<
                    Image2DType, Image2DType, StructuringElement2D>;
                auto filter = FilterType::New();
                filter->SetInput(slice2D);
                filter->SetKernel(element);
                filter->SetForegroundValue(params.foregroundValue);
                filter->Update();
                result = filter->GetOutput();
                break;
            }
            case MorphologicalOperation::Dilation: {
                using FilterType = itk::BinaryDilateImageFilter<
                    Image2DType, Image2DType, StructuringElement2D>;
                auto filter = FilterType::New();
                filter->SetInput(slice2D);
                filter->SetKernel(element);
                filter->SetForegroundValue(params.foregroundValue);
                filter->SetBackgroundValue(params.backgroundValue);
                filter->Update();
                result = filter->GetOutput();
                break;
            }
            case MorphologicalOperation::Erosion: {
                using FilterType = itk::BinaryErodeImageFilter<
                    Image2DType, Image2DType, StructuringElement2D>;
                auto filter = FilterType::New();
                filter->SetInput(slice2D);
                filter->SetKernel(element);
                filter->SetForegroundValue(params.foregroundValue);
                filter->SetBackgroundValue(params.backgroundValue);
                filter->Update();
                result = filter->GetOutput();
                break;
            }
            case MorphologicalOperation::FillHoles: {
                using FilterType = itk::BinaryFillholeImageFilter<Image2DType>;
                auto filter = FilterType::New();
                filter->SetInput(slice2D);
                filter->SetForegroundValue(params.foregroundValue);
                filter->Update();
                result = filter->GetOutput();
                break;
            }
            case MorphologicalOperation::IslandRemoval: {
                // For 2D island removal, we use the slice as-is for now
                // Full 2D island removal would need additional implementation
                result = slice2D;
                break;
            }
        }

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

std::expected<MorphologicalProcessor::LabelMapType::Pointer, SegmentationError>
MorphologicalProcessor::applyToLabel(
    LabelMapType::Pointer labelMap,
    unsigned char labelId,
    MorphologicalOperation operation,
    const Parameters& params
) const {
    if (!labelMap) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Label map is null"
        });
    }

    if (labelId == 0) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Cannot apply morphological operation to background (label 0)"
        });
    }

    try {
        // Extract binary mask for the specified label
        using ThresholdType = itk::BinaryThresholdImageFilter<LabelMapType, BinaryMaskType>;
        auto thresholdFilter = ThresholdType::New();
        thresholdFilter->SetInput(labelMap);
        thresholdFilter->SetLowerThreshold(labelId);
        thresholdFilter->SetUpperThreshold(labelId);
        thresholdFilter->SetInsideValue(1);
        thresholdFilter->SetOutsideValue(0);
        thresholdFilter->Update();

        // Apply morphological operation
        auto processedMask = apply(thresholdFilter->GetOutput(), operation, params);
        if (!processedMask) {
            return std::unexpected(processedMask.error());
        }

        // Create output label map by copying input
        auto outputLabelMap = LabelMapType::New();
        outputLabelMap->SetRegions(labelMap->GetLargestPossibleRegion());
        outputLabelMap->SetSpacing(labelMap->GetSpacing());
        outputLabelMap->SetOrigin(labelMap->GetOrigin());
        outputLabelMap->SetDirection(labelMap->GetDirection());
        outputLabelMap->Allocate();

        // Copy all labels except the one being modified
        using IteratorType = itk::ImageRegionIterator<LabelMapType>;
        using ConstIteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        using MaskIteratorType = itk::ImageRegionConstIterator<BinaryMaskType>;

        ConstIteratorType inputIt(labelMap, labelMap->GetLargestPossibleRegion());
        MaskIteratorType maskIt(processedMask.value(),
                                processedMask.value()->GetLargestPossibleRegion());
        IteratorType outputIt(outputLabelMap, outputLabelMap->GetLargestPossibleRegion());

        for (inputIt.GoToBegin(), maskIt.GoToBegin(), outputIt.GoToBegin();
             !inputIt.IsAtEnd();
             ++inputIt, ++maskIt, ++outputIt) {

            unsigned char inputLabel = inputIt.Get();
            unsigned char maskValue = maskIt.Get();

            if (inputLabel == labelId) {
                // Original position of this label
                outputIt.Set(maskValue > 0 ? labelId : 0);
            } else if (maskValue > 0) {
                // New position from processed mask - assign label if background
                outputIt.Set(inputLabel == 0 ? labelId : inputLabel);
            } else {
                // Keep original label
                outputIt.Set(inputLabel);
            }
        }

        return outputLabelMap;
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

std::expected<MorphologicalProcessor::LabelMapType::Pointer, SegmentationError>
MorphologicalProcessor::applyToAllLabels(
    LabelMapType::Pointer labelMap,
    MorphologicalOperation operation,
    const Parameters& params
) const {
    if (!labelMap) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Label map is null"
        });
    }

    try {
        // Find all unique labels
        std::set<unsigned char> labels;
        using ConstIteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        ConstIteratorType it(labelMap, labelMap->GetLargestPossibleRegion());
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            unsigned char value = it.Get();
            if (value != 0) {
                labels.insert(value);
            }
        }

        if (labels.empty()) {
            // No labels to process, return copy of input
            auto output = LabelMapType::New();
            output->SetRegions(labelMap->GetLargestPossibleRegion());
            output->SetSpacing(labelMap->GetSpacing());
            output->SetOrigin(labelMap->GetOrigin());
            output->SetDirection(labelMap->GetDirection());
            output->Allocate();
            output->FillBuffer(0);
            return output;
        }

        // Apply operation to each label
        LabelMapType::Pointer currentLabelMap = labelMap;
        int processedCount = 0;
        int totalLabels = static_cast<int>(labels.size());

        for (unsigned char labelId : labels) {
            auto result = applyToLabel(currentLabelMap, labelId, operation, params);
            if (!result) {
                return std::unexpected(result.error());
            }
            currentLabelMap = result.value();

            // Report progress
            if (progressCallback_) {
                ++processedCount;
                progressCallback_(static_cast<double>(processedCount) / totalLabels);
            }
        }

        return currentLabelMap;
    }
    catch (const std::exception& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

void MorphologicalProcessor::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

std::string MorphologicalProcessor::operationToString(MorphologicalOperation operation) {
    switch (operation) {
        case MorphologicalOperation::Opening:
            return "Opening";
        case MorphologicalOperation::Closing:
            return "Closing";
        case MorphologicalOperation::Dilation:
            return "Dilation";
        case MorphologicalOperation::Erosion:
            return "Erosion";
        case MorphologicalOperation::FillHoles:
            return "Fill Holes";
        case MorphologicalOperation::IslandRemoval:
            return "Island Removal";
    }
    return "Unknown";
}

std::string MorphologicalProcessor::structuringElementToString(StructuringElementShape shape) {
    switch (shape) {
        case StructuringElementShape::Ball:
            return "Ball";
        case StructuringElementShape::Cross:
            return "Cross";
    }
    return "Unknown";
}

}  // namespace dicom_viewer::services

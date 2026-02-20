#include "services/segmentation/slice_interpolator.hpp"
#include "services/segmentation/threshold_segmenter.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <format>

#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkExtractImageFilter.h>
#include <itkImageRegionIterator.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageDuplicator.h>
#include <itkCommand.h>

#include <set>
#include <algorithm>
#include <cmath>

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

    void setCallback(SliceInterpolator::ProgressCallback callback) {
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
    SliceInterpolator::ProgressCallback callback_;
};

}  // anonymous namespace

std::vector<int> SliceInterpolator::detectAnnotatedSlices(
    LabelMapType::Pointer labelMap,
    uint8_t labelId
) const {
    std::vector<int> annotatedSlices;

    if (!labelMap) {
        LOG_WARNING("Input label map is null");
        return annotatedSlices;
    }

    auto region = labelMap->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int numSlices = static_cast<int>(size[2]);

    LOG_DEBUG(std::format("Scanning {} slices for label {}", numSlices, labelId));

    for (int z = 0; z < numSlices; ++z) {
        // Extract slice region
        LabelMapType::RegionType sliceRegion;
        LabelMapType::IndexType start = {{0, 0, z}};
        LabelMapType::SizeType sliceSize = {{size[0], size[1], 1}};
        sliceRegion.SetIndex(start);
        sliceRegion.SetSize(sliceSize);

        // Check if slice contains the label
        itk::ImageRegionConstIterator<LabelMapType> it(labelMap, sliceRegion);
        bool hasLabel = false;

        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == labelId) {
                hasLabel = true;
                break;
            }
        }

        if (hasLabel) {
            annotatedSlices.push_back(z);
        }
    }

    LOG_INFO(std::format("Found {} annotated slices for label {}",
        annotatedSlices.size(), labelId));

    return annotatedSlices;
}

std::vector<uint8_t> SliceInterpolator::detectLabels(
    LabelMapType::Pointer labelMap
) const {
    std::set<uint8_t> labelSet;

    if (!labelMap) {
        LOG_WARNING("Input label map is null");
        return {};
    }

    itk::ImageRegionConstIterator<LabelMapType> it(
        labelMap, labelMap->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        uint8_t value = it.Get();
        if (value != 0) {  // Exclude background
            labelSet.insert(value);
        }
    }

    std::vector<uint8_t> labels(labelSet.begin(), labelSet.end());
    LOG_INFO(std::format("Detected {} unique labels", labels.size()));

    return labels;
}

std::expected<InterpolationResult, SegmentationError>
SliceInterpolator::interpolate(
    LabelMapType::Pointer labelMap,
    const InterpolationParameters& params
) const {
    LOG_INFO(std::format("Starting interpolation with method={}",
        static_cast<int>(params.method)));

    if (!labelMap) {
        LOG_ERROR("Input label map is null");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input label map is null"
        });
    }

    if (!params.isValid()) {
        LOG_ERROR("Invalid interpolation parameters");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid interpolation parameters"
        });
    }

    // Determine which labels to process
    std::vector<uint8_t> labelsToProcess = params.labelIds;
    LOG_INFO(std::format("Initial labelsToProcess size: {}", labelsToProcess.size()));

    if (labelsToProcess.empty()) {
        LOG_INFO("labelsToProcess is empty, detecting labels");
        labelsToProcess = detectLabels(labelMap);
    }

    LOG_INFO(std::format("Final labelsToProcess size: {}", labelsToProcess.size()));

    if (labelsToProcess.empty()) {
        LOG_WARNING("No labels found to interpolate");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "No labels found in the label map"
        });
    }

    try {
        InterpolationResult output;

        // Start with a copy of the original label map directly into output
        using DuplicatorType = itk::ImageDuplicator<LabelMapType>;
        auto duplicator = DuplicatorType::New();
        duplicator->SetInputImage(labelMap);
        duplicator->Update();
        output.interpolatedMask = duplicator->GetOutput();
        output.interpolatedMask->DisconnectPipeline();

        LOG_DEBUG(std::format("Processing {} labels", labelsToProcess.size()));

        // Process each label
        for (uint8_t labelId : labelsToProcess) {
            LOG_DEBUG(std::format("Processing label {}", static_cast<int>(labelId)));

            // Detect source slices before interpolation
            auto sourceSlices = detectAnnotatedSlices(labelMap, labelId);

            for (int slice : sourceSlices) {
                if (std::find(output.sourceSlices.begin(), output.sourceSlices.end(), slice)
                    == output.sourceSlices.end()) {
                    output.sourceSlices.push_back(slice);
                }
            }

            if (sourceSlices.size() < 2) {
                LOG_DEBUG(std::format("Label {} has fewer than 2 annotated slices, skipping",
                    labelId));
                continue;
            }

            // Extract binary mask for this label
            LabelMapType::Pointer binaryMask;
            try {
                binaryMask = extractLabel(labelMap, labelId);
            } catch (const std::exception& e) {
                LOG_ERROR(std::format("Exception in extractLabel: {}", e.what()));
                continue;
            } catch (...) {
                LOG_ERROR("Unknown exception in extractLabel");
                continue;
            }

            if (!binaryMask) {
                LOG_WARNING("Binary mask extraction failed, skipping label");
                continue;
            }

            // Apply interpolation
            LabelMapType::Pointer interpolated;
            switch (params.method) {
                case InterpolationMethod::Morphological:
                    // Use shape-based as morphological requires ITK remote module
                    interpolated = shapeBasedInterpolation(binaryMask, labelId);
                    break;
                case InterpolationMethod::ShapeBased:
                    interpolated = shapeBasedInterpolation(binaryMask, labelId);
                    break;
                case InterpolationMethod::Linear:
                    interpolated = linearInterpolation(binaryMask, labelId);
                    break;
            }

            if (!interpolated) {
                LOG_WARNING(std::format("Interpolation failed for label {}", labelId));
                continue;
            }

            // Merge back into output.interpolatedMask
            auto mergedResult = mergeLabel(output.interpolatedMask, interpolated, labelId);

            if (!mergedResult) {
                LOG_ERROR("Merge failed, skipping label");
                continue;
            }

            // Update output.interpolatedMask with merged result
            output.interpolatedMask = mergedResult;
            output.interpolatedMask->DisconnectPipeline();

            // Detect interpolated slices
            auto allSlices = detectAnnotatedSlices(output.interpolatedMask, labelId);
            for (int slice : allSlices) {
                bool isSource = std::find(sourceSlices.begin(), sourceSlices.end(), slice)
                    != sourceSlices.end();
                if (!isSource) {
                    if (std::find(output.interpolatedSlices.begin(),
                        output.interpolatedSlices.end(), slice)
                        == output.interpolatedSlices.end()) {
                        output.interpolatedSlices.push_back(slice);
                    }
                }
            }
        }

        // Sort slices
        std::sort(output.sourceSlices.begin(), output.sourceSlices.end());
        std::sort(output.interpolatedSlices.begin(), output.interpolatedSlices.end());

        LOG_INFO(std::format("Interpolation complete: {} source slices, {} interpolated slices",
            output.sourceSlices.size(), output.interpolatedSlices.size()));

        return output;
    } catch (const itk::ExceptionObject& e) {
        LOG_ERROR(std::format("ITK exception during interpolation: {}", e.what()));
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.what()
        });
    }
}

std::expected<InterpolationResult, SegmentationError>
SliceInterpolator::interpolateRange(
    LabelMapType::Pointer labelMap,
    uint8_t labelId,
    int startSlice,
    int endSlice
) const {
    if (!labelMap) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input label map is null"
        });
    }

    auto size = labelMap->GetLargestPossibleRegion().GetSize();
    int numSlices = static_cast<int>(size[2]);

    if (startSlice < 0 || endSlice >= numSlices || startSlice > endSlice) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid slice range"
        });
    }

    InterpolationParameters params;
    params.labelIds = {labelId};
    params.startSlice = startSlice;
    params.endSlice = endSlice;

    return interpolate(labelMap, params);
}

std::expected<SliceInterpolator::SliceType::Pointer, SegmentationError>
SliceInterpolator::previewSlice(
    LabelMapType::Pointer labelMap,
    uint8_t labelId,
    int targetSlice
) const {
    if (!labelMap) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input label map is null"
        });
    }

    auto size = labelMap->GetLargestPossibleRegion().GetSize();
    if (targetSlice < 0 || targetSlice >= static_cast<int>(size[2])) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Target slice index out of bounds"
        });
    }

    // Perform full interpolation using shape-based method
    InterpolationParameters params;
    params.labelIds = {labelId};
    params.method = InterpolationMethod::ShapeBased;

    auto result = interpolate(labelMap, params);
    if (!result) {
        return std::unexpected(result.error());
    }

    // Extract the target slice
    return extractSlice(result->interpolatedMask, targetSlice);
}

void SliceInterpolator::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

SliceInterpolator::LabelMapType::Pointer
SliceInterpolator::morphologicalInterpolation(
    LabelMapType::Pointer input,
    uint8_t labelId
) const {
    // ITK MorphologicalContourInterpolator requires a remote module that is
    // not part of the standard ITK distribution. Fall back to shape-based
    // interpolation which provides similar results for most use cases.
    LOG_DEBUG("Morphological method uses shape-based interpolation");
    return shapeBasedInterpolation(input, labelId);
}

SliceInterpolator::LabelMapType::Pointer
SliceInterpolator::shapeBasedInterpolation(
    LabelMapType::Pointer input,
    uint8_t labelId
) const {
    LOG_DEBUG(std::format("Shape-based interpolation for label {}", static_cast<int>(labelId)));

    if (!input) {
        LOG_ERROR("shapeBasedInterpolation: input is null");
        return nullptr;
    }

    auto size = input->GetLargestPossibleRegion().GetSize();

    // Detect annotated slices
    auto annotatedSlices = detectAnnotatedSlices(input, labelId);
    if (annotatedSlices.size() < 2) {
        LOG_DEBUG("Need at least 2 annotated slices for interpolation");
        return input;  // Return input unchanged
    }

    LOG_DEBUG(std::format("Interpolating {} annotated slices", annotatedSlices.size()));

    // Create output image initialized to zero
    auto output = LabelMapType::New();
    output->SetRegions(input->GetLargestPossibleRegion());
    output->SetSpacing(input->GetSpacing());
    output->SetOrigin(input->GetOrigin());
    output->SetDirection(input->GetDirection());
    output->Allocate();
    output->FillBuffer(0);

    if (progressCallback_) {
        progressCallback_(0.1);
    }

    // Copy annotated slices and interpolate gaps
    int totalGaps = 0;
    for (size_t i = 0; i + 1 < annotatedSlices.size(); ++i) {
        totalGaps += annotatedSlices[i + 1] - annotatedSlices[i] - 1;
    }
    int processedGaps = 0;

    for (size_t i = 0; i < annotatedSlices.size(); ++i) {
        int currentSlice = annotatedSlices[i];

        // Copy the annotated slice to output
        for (unsigned int y = 0; y < size[1]; ++y) {
            for (unsigned int x = 0; x < size[0]; ++x) {
                LabelMapType::IndexType idx = {{
                    static_cast<long>(x),
                    static_cast<long>(y),
                    static_cast<long>(currentSlice)
                }};
                output->SetPixel(idx, input->GetPixel(idx));
            }
        }

        // Interpolate gap to next slice if there is one
        if (i + 1 < annotatedSlices.size()) {
            int nextSlice = annotatedSlices[i + 1];
            int gapSize = nextSlice - currentSlice - 1;

            if (gapSize > 0) {
                LOG_DEBUG(std::format("shapeBasedInterpolation: Interpolating slices {} to {}",
                    currentSlice + 1, nextSlice - 1));

                // Compute distance maps for current and next slice regions
                // For simplicity, use linear blending of the two slices' shapes
                for (int z = currentSlice + 1; z < nextSlice; ++z) {
                    double t = static_cast<double>(z - currentSlice) /
                              static_cast<double>(nextSlice - currentSlice);

                    // For each pixel, check if it's inside the interpolated shape
                    // Using a simple approach: blend based on which slice it's closer to
                    for (unsigned int y = 0; y < size[1]; ++y) {
                        for (unsigned int x = 0; x < size[0]; ++x) {
                            LabelMapType::IndexType idxCurrent = {{
                                static_cast<long>(x),
                                static_cast<long>(y),
                                static_cast<long>(currentSlice)
                            }};
                            LabelMapType::IndexType idxNext = {{
                                static_cast<long>(x),
                                static_cast<long>(y),
                                static_cast<long>(nextSlice)
                            }};

                            bool inCurrent = (input->GetPixel(idxCurrent) == labelId);
                            bool inNext = (input->GetPixel(idxNext) == labelId);

                            // Simple interpolation: pixel is inside if it's inside
                            // in both slices, or inside the one it's closer to
                            bool shouldFill = false;
                            if (inCurrent && inNext) {
                                shouldFill = true;  // Inside both
                            } else if (inCurrent && t < 0.5) {
                                shouldFill = true;  // Closer to current and inside current
                            } else if (inNext && t >= 0.5) {
                                shouldFill = true;  // Closer to next and inside next
                            }

                            if (shouldFill) {
                                LabelMapType::IndexType outputIdx = {{
                                    static_cast<long>(x),
                                    static_cast<long>(y),
                                    static_cast<long>(z)
                                }};
                                output->SetPixel(outputIdx, labelId);
                            }
                        }
                    }

                    processedGaps++;
                    if (progressCallback_ && totalGaps > 0) {
                        progressCallback_(0.1 + 0.8 * static_cast<double>(processedGaps) / totalGaps);
                    }
                }
            }
        }
    }

    if (progressCallback_) {
        progressCallback_(1.0);
    }

    output->DisconnectPipeline();
    return output;
}

SliceInterpolator::LabelMapType::Pointer
SliceInterpolator::linearInterpolation(
    LabelMapType::Pointer input,
    uint8_t labelId
) const {
    LOG_DEBUG(std::format("Linear interpolation for label {}", static_cast<int>(labelId)));

    // Linear interpolation fills gaps smoothly between annotated slices
    // using linear blending based on position

    // Detect annotated slices for this label
    auto annotatedSlices = detectAnnotatedSlices(input, labelId);
    if (annotatedSlices.size() < 2) {
        LOG_WARNING("Need at least 2 annotated slices for linear interpolation");
        return input;  // Return input unchanged
    }

    auto size = input->GetLargestPossibleRegion().GetSize();

    // Create output image
    auto output = LabelMapType::New();
    output->SetRegions(input->GetLargestPossibleRegion());
    output->SetSpacing(input->GetSpacing());
    output->SetOrigin(input->GetOrigin());
    output->SetDirection(input->GetDirection());
    output->Allocate();
    output->FillBuffer(0);

    // Copy annotated slices and interpolate gaps
    for (size_t i = 0; i < annotatedSlices.size(); ++i) {
        int currentSlice = annotatedSlices[i];

        // Copy the annotated slice to output
        for (unsigned int y = 0; y < size[1]; ++y) {
            for (unsigned int x = 0; x < size[0]; ++x) {
                LabelMapType::IndexType idx = {{
                    static_cast<long>(x),
                    static_cast<long>(y),
                    static_cast<long>(currentSlice)
                }};
                output->SetPixel(idx, input->GetPixel(idx));
            }
        }

        // Interpolate gap to next slice if there is one
        if (i + 1 < annotatedSlices.size()) {
            int nextSlice = annotatedSlices[i + 1];
            int gapSize = nextSlice - currentSlice - 1;

            if (gapSize > 0) {
                // Linear interpolation fills based on the union of both shapes
                for (int z = currentSlice + 1; z < nextSlice; ++z) {
                    double t = static_cast<double>(z - currentSlice) /
                              static_cast<double>(nextSlice - currentSlice);

                    for (unsigned int y = 0; y < size[1]; ++y) {
                        for (unsigned int x = 0; x < size[0]; ++x) {
                            LabelMapType::IndexType idxCurrent = {{
                                static_cast<long>(x),
                                static_cast<long>(y),
                                static_cast<long>(currentSlice)
                            }};
                            LabelMapType::IndexType idxNext = {{
                                static_cast<long>(x),
                                static_cast<long>(y),
                                static_cast<long>(nextSlice)
                            }};

                            bool inCurrent = (input->GetPixel(idxCurrent) == labelId);
                            bool inNext = (input->GetPixel(idxNext) == labelId);

                            // Linear interpolation fills the pixel if it's inside either
                            // shape, with weight based on distance from each slice
                            bool shouldFill = false;
                            if (inCurrent && inNext) {
                                shouldFill = true;
                            } else if (inCurrent) {
                                // Fade out from current slice
                                shouldFill = (t < 0.7);  // Keep filled closer to current
                            } else if (inNext) {
                                // Fade in from next slice
                                shouldFill = (t > 0.3);  // Start filling closer to next
                            }

                            if (shouldFill) {
                                LabelMapType::IndexType outputIdx = {{
                                    static_cast<long>(x),
                                    static_cast<long>(y),
                                    static_cast<long>(z)
                                }};
                                output->SetPixel(outputIdx, labelId);
                            }
                        }
                    }
                }
            }
        }
    }

    output->DisconnectPipeline();
    return output;
}

SliceInterpolator::LabelMapType::Pointer
SliceInterpolator::extractLabel(
    LabelMapType::Pointer labelMap,
    uint8_t labelId
) const {
    if (!labelMap) {
        LOG_ERROR("extractLabel: labelMap is null");
        return nullptr;
    }

    using ThresholdFilterType = itk::BinaryThresholdImageFilter<
        LabelMapType, LabelMapType>;
    auto filter = ThresholdFilterType::New();
    filter->SetInput(labelMap);
    filter->SetLowerThreshold(labelId);
    filter->SetUpperThreshold(labelId);
    filter->SetInsideValue(labelId);
    filter->SetOutsideValue(0);

    try {
        filter->Update();
        LabelMapType::Pointer filterOutput = filter->GetOutput();
        filterOutput->DisconnectPipeline();
        return filterOutput;
    } catch (const itk::ExceptionObject& e) {
        LOG_ERROR(std::format("Label extraction failed: {}", e.what()));
        return nullptr;
    }
}

SliceInterpolator::LabelMapType::Pointer
SliceInterpolator::mergeLabel(
    LabelMapType::Pointer labelMap,
    LabelMapType::Pointer interpolated,
    uint8_t labelId
) const {
    // Verify that both images have the same region
    auto labelRegion = labelMap->GetLargestPossibleRegion();
    auto interpRegion = interpolated->GetLargestPossibleRegion();

    if (labelRegion.GetSize() != interpRegion.GetSize()) {
        LOG_ERROR("Region size mismatch in mergeLabel");
        return nullptr;
    }

    // Create output image
    auto output = LabelMapType::New();
    output->SetRegions(labelRegion);
    output->SetSpacing(labelMap->GetSpacing());
    output->SetOrigin(labelMap->GetOrigin());
    output->SetDirection(labelMap->GetDirection());
    output->Allocate();

    // Copy original and merge interpolated using the same region
    itk::ImageRegionConstIterator<LabelMapType> originalIt(labelMap, labelRegion);
    itk::ImageRegionConstIterator<LabelMapType> interpIt(interpolated, labelRegion);
    itk::ImageRegionIterator<LabelMapType> outputIt(output, labelRegion);

    for (originalIt.GoToBegin(), interpIt.GoToBegin(), outputIt.GoToBegin();
         !originalIt.IsAtEnd();
         ++originalIt, ++interpIt, ++outputIt) {

        uint8_t originalValue = originalIt.Get();
        uint8_t interpValue = interpIt.Get();

        if (interpValue == labelId) {
            outputIt.Set(labelId);
        } else {
            outputIt.Set(originalValue);
        }
    }

    // Ensure output is disconnected from any pipeline
    output->DisconnectPipeline();

    return output;
}

SliceInterpolator::SliceType::Pointer
SliceInterpolator::extractSlice(
    LabelMapType::Pointer volume,
    int sliceIndex
) const {
    auto size = volume->GetLargestPossibleRegion().GetSize();

    using ExtractFilterType = itk::ExtractImageFilter<LabelMapType, SliceType>;
    auto extractor = ExtractFilterType::New();

    LabelMapType::RegionType extractRegion;
    LabelMapType::IndexType start = {{0, 0, sliceIndex}};
    LabelMapType::SizeType extractSize = {{size[0], size[1], 0}};
    extractRegion.SetIndex(start);
    extractRegion.SetSize(extractSize);

    extractor->SetInput(volume);
    extractor->SetExtractionRegion(extractRegion);
    extractor->SetDirectionCollapseToSubmatrix();

    try {
        extractor->Update();
        return extractor->GetOutput();
    } catch (const itk::ExceptionObject& e) {
        LOG_ERROR(std::format("Slice extraction failed: {}", e.what()));
        return nullptr;
    }
}

}  // namespace dicom_viewer::services

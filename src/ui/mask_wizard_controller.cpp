#include "ui/mask_wizard_controller.hpp"
#include "services/segmentation/label_manager.hpp"

#include <itkConnectedComponentImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkMinimumMaximumImageFilter.h>
#include <itkRegionOfInterestImageFilter.h>
#include <itkRelabelComponentImageFilter.h>

#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

namespace dicom_viewer::ui {

namespace {

/// Default colors for connected components (repeating palette)
constexpr int kComponentPaletteSize = 8;
const QColor kComponentPalette[kComponentPaletteSize] = {
    QColor(0xE7, 0x4C, 0x3C),  // red
    QColor(0x2E, 0xCC, 0x71),  // green
    QColor(0x34, 0x98, 0xDB),  // blue
    QColor(0xF3, 0x9C, 0x12),  // orange
    QColor(0x9B, 0x59, 0xB6),  // purple
    QColor(0x1A, 0xBC, 0x9C),  // teal
    QColor(0xE9, 0x1E, 0x63),  // pink
    QColor(0x00, 0xBC, 0xD4),  // cyan
};

/// Maximum number of components to display (prevents UI overload)
constexpr int kMaxComponents = 50;

/// Debounce delay for threshold slider changes (ms)
constexpr int kThresholdDebounceMs = 200;

}  // namespace

using ImageType = services::ThresholdSegmenter::ImageType;
using BinaryMaskType = services::ThresholdSegmenter::BinaryMaskType;
using LabelMapType = MaskWizardController::LabelMapType;

class MaskWizardController::Impl {
public:
    MaskWizard* wizard = nullptr;
    services::LabelManager* labelManager = nullptr;
    Context context;

    // Service instances (owned)
    services::ThresholdSegmenter segmenter;
    services::PhaseTracker tracker;

    // Intermediate results
    ImageType::Pointer croppedImage;         // After crop (or full image)
    BinaryMaskType::Pointer thresholdMask;   // Result of Step 2
    using ComponentLabel = itk::Image<unsigned short, 3>;
    ComponentLabel::Pointer componentLabelMap;  // Result of CC labeling
    int componentCount = 0;

    // Phase tracking result
    services::PhaseTracker::TrackingResult trackingResult;
    bool propagationComplete = false;

    // Debounce timer for threshold changes
    QTimer* debounceTimer = nullptr;
    int pendingMinThreshold = 0;
    int pendingMaxThreshold = 0;

    // Async watcher for propagation
    QFutureWatcher<std::expected<services::PhaseTracker::TrackingResult,
                                  services::SegmentationError>>* propagationWatcher = nullptr;

    /**
     * @brief Apply crop to source image (or return full image if no crop)
     */
    ImageType::Pointer applyCrop() {
        if (!context.sourceImage) return nullptr;

        if (wizard->isCropFullVolume()) {
            return context.sourceImage;
        }

        auto crop = wizard->cropRegion();
        auto region = context.sourceImage->GetLargestPossibleRegion();
        auto size = region.GetSize();

        // Clamp crop bounds to image dimensions
        int xMin = std::clamp(crop.xMin, 0, static_cast<int>(size[0]) - 1);
        int xMax = std::clamp(crop.xMax, 0, static_cast<int>(size[0]) - 1);
        int yMin = std::clamp(crop.yMin, 0, static_cast<int>(size[1]) - 1);
        int yMax = std::clamp(crop.yMax, 0, static_cast<int>(size[1]) - 1);
        int zMin = std::clamp(crop.zMin, 0, static_cast<int>(size[2]) - 1);
        int zMax = std::clamp(crop.zMax, 0, static_cast<int>(size[2]) - 1);

        ImageType::IndexType start;
        start[0] = xMin;
        start[1] = yMin;
        start[2] = zMin;

        ImageType::SizeType cropSize;
        cropSize[0] = xMax - xMin + 1;
        cropSize[1] = yMax - yMin + 1;
        cropSize[2] = zMax - zMin + 1;

        ImageType::RegionType desiredRegion(start, cropSize);

        using ROIFilterType = itk::RegionOfInterestImageFilter<ImageType, ImageType>;
        auto roiFilter = ROIFilterType::New();
        roiFilter->SetInput(context.sourceImage);
        roiFilter->SetRegionOfInterest(desiredRegion);

        try {
            roiFilter->Update();
            return roiFilter->GetOutput();
        } catch (const itk::ExceptionObject&) {
            return context.sourceImage;  // Fallback to full image
        }
    }

    /**
     * @brief Run connected component analysis on the threshold mask
     */
    void runConnectedComponentAnalysis() {
        if (!thresholdMask) return;

        using ConnectedFilter = itk::ConnectedComponentImageFilter<
            BinaryMaskType, ComponentLabel>;
        auto connected = ConnectedFilter::New();
        connected->SetInput(thresholdMask);
        connected->SetFullyConnected(false);  // 6-connectivity in 3D

        // Relabel by size (largest component = label 1)
        using RelabelFilter = itk::RelabelComponentImageFilter<
            ComponentLabel, ComponentLabel>;
        auto relabel = RelabelFilter::New();
        relabel->SetInput(connected->GetOutput());
        relabel->SetMinimumObjectSize(10);  // Ignore tiny noise components

        try {
            relabel->Update();
        } catch (const itk::ExceptionObject&) {
            return;
        }

        componentLabelMap = relabel->GetOutput();
        componentCount = std::min(
            static_cast<int>(relabel->GetNumberOfObjects()),
            kMaxComponents);

        // Build ComponentInfo list for the wizard
        std::vector<ComponentInfo> components;
        components.reserve(componentCount);

        for (int i = 0; i < componentCount; ++i) {
            ComponentInfo info;
            info.label = i + 1;
            info.voxelCount = static_cast<int>(relabel->GetSizeOfObjectInPixels(i + 1));
            info.color = kComponentPalette[i % kComponentPaletteSize];
            info.selected = (i == 0);  // Select largest by default
            components.push_back(info);
        }

        wizard->setComponents(components);
    }

    /**
     * @brief Build a binary mask from selected components
     */
    BinaryMaskType::Pointer buildSelectedComponentMask() {
        if (!componentLabelMap) return nullptr;

        auto selectedIndices = wizard->selectedComponentIndices();
        if (selectedIndices.empty()) return nullptr;

        // Build set of selected labels (1-based)
        std::set<unsigned short> selectedLabels;
        for (int idx : selectedIndices) {
            selectedLabels.insert(static_cast<unsigned short>(idx + 1));
        }

        auto mask = BinaryMaskType::New();
        mask->SetRegions(componentLabelMap->GetLargestPossibleRegion());
        mask->SetSpacing(componentLabelMap->GetSpacing());
        mask->SetOrigin(componentLabelMap->GetOrigin());
        mask->SetDirection(componentLabelMap->GetDirection());
        mask->Allocate();
        mask->FillBuffer(0);

        itk::ImageRegionConstIterator<ComponentLabel> inputIt(
            componentLabelMap, componentLabelMap->GetLargestPossibleRegion());
        itk::ImageRegionIterator<BinaryMaskType> outputIt(
            mask, mask->GetLargestPossibleRegion());

        while (!inputIt.IsAtEnd()) {
            if (selectedLabels.contains(inputIt.Get())) {
                outputIt.Set(1);
            }
            ++inputIt;
            ++outputIt;
        }

        return mask;
    }
};

MaskWizardController::MaskWizardController(MaskWizard* wizard, QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
    impl_->wizard = wizard;

    // Debounce timer for threshold slider changes
    impl_->debounceTimer = new QTimer(this);
    impl_->debounceTimer->setSingleShot(true);
    impl_->debounceTimer->setInterval(kThresholdDebounceMs);

    // --- Step 2: Threshold wiring ---

    connect(wizard, &MaskWizard::thresholdChanged,
            this, [this](int min, int max) {
        impl_->pendingMinThreshold = min;
        impl_->pendingMaxThreshold = max;
        impl_->debounceTimer->start();
    });

    connect(impl_->debounceTimer, &QTimer::timeout,
            this, [this]() {
        if (!impl_->croppedImage) return;

        auto result = impl_->segmenter.manualThreshold(
            impl_->croppedImage,
            static_cast<double>(impl_->pendingMinThreshold),
            static_cast<double>(impl_->pendingMaxThreshold));

        if (result) {
            impl_->thresholdMask = *result;
        }
    });

    connect(wizard, &MaskWizard::otsuRequested,
            this, [this]() {
        if (!impl_->croppedImage) return;

        auto result = impl_->segmenter.otsuThreshold(impl_->croppedImage);
        if (result) {
            impl_->thresholdMask = result->mask;
            impl_->wizard->setOtsuThreshold(result->threshold);
        } else {
            emit errorOccurred(
                QString::fromStdString(result.error().toString()));
        }
    });

    // --- Page transition: Step 2 → Step 3 triggers CC analysis ---

    connect(wizard, QOverload<int>::of(&QWizard::currentIdChanged),
            this, [this](int id) {
        auto step = static_cast<MaskWizardStep>(id);

        if (step == MaskWizardStep::Separate) {
            // Ensure we have a threshold mask before CC analysis
            if (!impl_->thresholdMask && impl_->croppedImage) {
                auto result = impl_->segmenter.manualThreshold(
                    impl_->croppedImage,
                    static_cast<double>(impl_->wizard->thresholdMin()),
                    static_cast<double>(impl_->wizard->thresholdMax()));
                if (result) {
                    impl_->thresholdMask = *result;
                }
            }
            impl_->runConnectedComponentAnalysis();
        }

        // Apply crop when leaving Step 1
        if (step == MaskWizardStep::Threshold) {
            impl_->croppedImage = impl_->applyCrop();

            // Set threshold range based on image intensity
            if (impl_->croppedImage) {
                using MinMaxFilter = itk::MinimumMaximumImageFilter<ImageType>;
                auto minMax = MinMaxFilter::New();
                minMax->SetInput(impl_->croppedImage);
                try {
                    minMax->Update();
                    impl_->wizard->setThresholdRange(
                        minMax->GetMinimum(), minMax->GetMaximum());
                } catch (const itk::ExceptionObject&) {
                    // Use defaults if min/max computation fails
                }
            }
        }
    });

    // --- Step 4: Phase tracking wiring ---

    connect(wizard, &MaskWizard::propagationRequested,
            this, [this]() {
        auto selectedMask = impl_->buildSelectedComponentMask();
        if (!selectedMask) {
            emit errorOccurred(tr("No components selected for propagation"));
            return;
        }

        if (impl_->context.magnitudePhases.size() < 2) {
            // Single phase: skip propagation, use the mask directly
            impl_->wizard->setTrackStatus(tr("Single phase - no propagation needed"));
            impl_->wizard->setTrackProgress(100);
            impl_->propagationComplete = true;
            return;
        }

        // Cast BinaryMask (uint8) to LabelMap (uint8) - same type, just aliasing
        auto referenceMask = LabelMapType::New();
        referenceMask->SetRegions(selectedMask->GetLargestPossibleRegion());
        referenceMask->SetSpacing(selectedMask->GetSpacing());
        referenceMask->SetOrigin(selectedMask->GetOrigin());
        referenceMask->SetDirection(selectedMask->GetDirection());
        referenceMask->Allocate();

        itk::ImageRegionConstIterator<BinaryMaskType> srcIt(
            selectedMask, selectedMask->GetLargestPossibleRegion());
        itk::ImageRegionIterator<LabelMapType> dstIt(
            referenceMask, referenceMask->GetLargestPossibleRegion());
        while (!srcIt.IsAtEnd()) {
            dstIt.Set(srcIt.Get());
            ++srcIt;
            ++dstIt;
        }

        impl_->wizard->setTrackStatus(tr("Running propagation..."));
        impl_->wizard->setTrackProgress(0);

        // Configure tracking
        services::PhaseTracker::TrackingConfig config;
        config.referencePhase = impl_->wizard->referencePhase();

        // Set progress callback (thread-safe via QMetaObject::invokeMethod)
        impl_->tracker.setProgressCallback(
            [this](int current, int total) {
                if (total <= 0) return;
                int percent = (current * 100) / total;
                QMetaObject::invokeMethod(impl_->wizard, [this, percent, current, total]() {
                    impl_->wizard->setTrackProgress(percent);
                    impl_->wizard->setTrackStatus(
                        tr("Processing phase %1 of %2...").arg(current).arg(total));
                }, Qt::QueuedConnection);
            });

        // Run propagation asynchronously
        auto magnitudePhases = impl_->context.magnitudePhases;
        auto* watcher = new QFutureWatcher<
            std::expected<services::PhaseTracker::TrackingResult,
                          services::SegmentationError>>(this);

        connect(watcher, &QFutureWatcher<
                    std::expected<services::PhaseTracker::TrackingResult,
                                  services::SegmentationError>>::finished,
                this, [this, watcher]() {
            auto result = watcher->result();
            if (result) {
                impl_->trackingResult = *result;
                impl_->propagationComplete = true;
                impl_->wizard->setTrackProgress(100);

                int warnings = result->warningCount;
                if (warnings > 0) {
                    impl_->wizard->setTrackStatus(
                        tr("Propagation complete (%1 phase(s) with quality warnings)")
                            .arg(warnings));
                } else {
                    impl_->wizard->setTrackStatus(tr("Propagation complete"));
                }
            } else {
                impl_->wizard->setTrackStatus(
                    tr("Propagation failed: %1")
                        .arg(QString::fromStdString(result.error().toString())));
                emit errorOccurred(
                    QString::fromStdString(result.error().toString()));
            }
            watcher->deleteLater();
        });

        auto future = QtConcurrent::run(
            [tracker = &impl_->tracker, referenceMask, magnitudePhases, config]() {
                return tracker->propagateMask(referenceMask, magnitudePhases, config);
            });
        watcher->setFuture(future);
    });

    // --- Output: Wizard finished → create label ---

    connect(wizard, &MaskWizard::wizardFinished,
            this, [this](const MaskWizardResult& /*result*/) {
        if (!impl_->labelManager) return;

        LabelMapType::Pointer finalMask;

        if (impl_->propagationComplete && !impl_->trackingResult.phases.empty()) {
            // Use the reference phase mask from propagation result
            int refPhase = impl_->trackingResult.referencePhase;
            if (refPhase >= 0
                && refPhase < static_cast<int>(impl_->trackingResult.phases.size())) {
                finalMask = impl_->trackingResult.phases[refPhase].mask;
            }
        }

        // Fallback: use the selected component mask
        if (!finalMask) {
            auto selectedMask = impl_->buildSelectedComponentMask();
            if (selectedMask) {
                // Cast to LabelMapType
                finalMask = LabelMapType::New();
                finalMask->SetRegions(selectedMask->GetLargestPossibleRegion());
                finalMask->SetSpacing(selectedMask->GetSpacing());
                finalMask->SetOrigin(selectedMask->GetOrigin());
                finalMask->SetDirection(selectedMask->GetDirection());
                finalMask->Allocate();

                itk::ImageRegionConstIterator<BinaryMaskType> srcIt(
                    selectedMask, selectedMask->GetLargestPossibleRegion());
                itk::ImageRegionIterator<LabelMapType> dstIt(
                    finalMask, finalMask->GetLargestPossibleRegion());
                while (!srcIt.IsAtEnd()) {
                    dstIt.Set(srcIt.Get());
                    ++srcIt;
                    ++dstIt;
                }
            }
        }

        if (finalMask) {
            // Generate auto-incremented label name
            auto labelCount = impl_->labelManager->getLabelCount();
            auto name = "Mask Wizard " + std::to_string(labelCount + 1);
            auto labelResult = impl_->labelManager->addLabel(name);
            if (labelResult) {
                (void)impl_->labelManager->setLabelMap(finalMask);
            }

            emit maskCreated(finalMask);
        }
    });
}

MaskWizardController::~MaskWizardController() = default;

void MaskWizardController::setContext(const Context& context)
{
    impl_->context = context;
    impl_->croppedImage = context.sourceImage;  // Start with full image

    // Configure wizard with phase data
    if (!context.magnitudePhases.empty()) {
        impl_->wizard->setPhaseCount(
            static_cast<int>(context.magnitudePhases.size()));
        impl_->wizard->setReferencePhase(context.currentPhase);
    }
}

void MaskWizardController::setLabelManager(services::LabelManager* manager)
{
    impl_->labelManager = manager;
}

} // namespace dicom_viewer::ui

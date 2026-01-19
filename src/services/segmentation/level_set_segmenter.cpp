#include "services/segmentation/level_set_segmenter.hpp"
#include "services/segmentation/threshold_segmenter.hpp"
#include "core/logging.hpp"

#include <itkGeodesicActiveContourLevelSetImageFilter.h>
#include <itkThresholdSegmentationLevelSetImageFilter.h>
#include <itkGradientMagnitudeRecursiveGaussianImageFilter.h>
#include <itkSigmoidImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkCommand.h>
#include <itkImageRegionIterator.h>

#include <cmath>

namespace dicom_viewer::services {

namespace {
auto& getLogger() {
    static auto logger = logging::LoggerFactory::create("LevelSetSegmenter");
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

    void setCallback(LevelSetSegmenter::ProgressCallback callback) {
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
    LevelSetSegmenter::ProgressCallback callback_;
};

} // anonymous namespace

std::expected<LevelSetResult, SegmentationError>
LevelSetSegmenter::geodesicActiveContour(
    ImageType::Pointer input,
    const LevelSetParameters& params
) const {
    getLogger()->info("Geodesic Active Contour: {} seeds, radius={:.1f}, maxIter={}",
        params.seedPoints.size(), params.seedRadius, params.maxIterations);

    if (!input) {
        getLogger()->error("Input image is null");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        getLogger()->error("Invalid parameters");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid parameters: check seeds, radius, iterations, and thresholds"
        });
    }

    if (auto error = validateSeeds(input, params.seedPoints)) {
        return std::unexpected(*error);
    }

    try {
        // Create feature image (edge potential)
        auto featureImage = createFeatureImage(input, params.sigma);
        if (!featureImage) {
            return std::unexpected(SegmentationError{
                SegmentationError::Code::ProcessingFailed,
                "Failed to create feature image"
            });
        }

        // Create initial level set from seeds
        auto initialLevelSet = createInitialLevelSet(input, params.seedPoints, params.seedRadius);
        if (!initialLevelSet) {
            return std::unexpected(SegmentationError{
                SegmentationError::Code::ProcessingFailed,
                "Failed to create initial level set"
            });
        }

        // Set up Geodesic Active Contour filter
        using GACFilterType = itk::GeodesicActiveContourLevelSetImageFilter<
            FloatImageType, FloatImageType>;
        auto gacFilter = GACFilterType::New();

        gacFilter->SetInput(initialLevelSet);
        gacFilter->SetFeatureImage(featureImage);

        gacFilter->SetPropagationScaling(params.propagationScaling * params.featureScaling);
        gacFilter->SetCurvatureScaling(params.curvatureScaling);
        gacFilter->SetAdvectionScaling(params.advectionScaling);

        gacFilter->SetMaximumRMSError(params.rmsThreshold);
        gacFilter->SetNumberOfIterations(params.maxIterations);

        // Attach progress observer
        if (progressCallback_) {
            auto observer = ProgressObserver::New();
            observer->setCallback(progressCallback_);
            gacFilter->AddObserver(itk::ProgressEvent(), observer);
        }

        getLogger()->debug("Starting level set evolution...");
        gacFilter->Update();

        auto elapsedIterations = static_cast<int>(gacFilter->GetElapsedIterations());
        auto finalRMS = gacFilter->GetRMSChange();

        getLogger()->info("Level set converged: {} iterations, RMS={:.6f}",
            elapsedIterations, finalRMS);

        // Convert level set to binary mask
        auto mask = levelSetToMask(gacFilter->GetOutput());

        LevelSetResult result;
        result.mask = mask;
        result.iterations = elapsedIterations;
        result.finalRMS = finalRMS;

        return result;
    }
    catch (const itk::ExceptionObject& e) {
        getLogger()->error("ITK exception: {}", e.GetDescription());
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        getLogger()->error("Standard exception: {}", e.what());
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

std::expected<LevelSetResult, SegmentationError>
LevelSetSegmenter::thresholdLevelSet(
    ImageType::Pointer input,
    const ThresholdLevelSetParameters& params
) const {
    getLogger()->info("Threshold Level Set: {} seeds, range=[{:.1f}, {:.1f}], maxIter={}",
        params.seedPoints.size(), params.lowerThreshold, params.upperThreshold,
        params.maxIterations);

    if (!input) {
        getLogger()->error("Input image is null");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Input image is null"
        });
    }

    if (!params.isValid()) {
        getLogger()->error("Invalid parameters");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Invalid parameters: check seeds, radius, thresholds, and iterations"
        });
    }

    if (auto error = validateSeeds(input, params.seedPoints)) {
        return std::unexpected(*error);
    }

    try {
        // Cast input to float
        using CastFilterType = itk::CastImageFilter<ImageType, FloatImageType>;
        auto castFilter = CastFilterType::New();
        castFilter->SetInput(input);
        castFilter->Update();

        // Create initial level set from seeds
        auto initialLevelSet = createInitialLevelSet(input, params.seedPoints, params.seedRadius);
        if (!initialLevelSet) {
            return std::unexpected(SegmentationError{
                SegmentationError::Code::ProcessingFailed,
                "Failed to create initial level set"
            });
        }

        // Set up Threshold Level Set filter
        using ThresholdLSFilterType = itk::ThresholdSegmentationLevelSetImageFilter<
            FloatImageType, FloatImageType>;
        auto thresholdFilter = ThresholdLSFilterType::New();

        thresholdFilter->SetInput(initialLevelSet);
        thresholdFilter->SetFeatureImage(castFilter->GetOutput());

        thresholdFilter->SetLowerThreshold(params.lowerThreshold);
        thresholdFilter->SetUpperThreshold(params.upperThreshold);

        thresholdFilter->SetCurvatureScaling(params.curvatureScaling);
        thresholdFilter->SetPropagationScaling(params.propagationScaling);

        thresholdFilter->SetMaximumRMSError(params.rmsThreshold);
        thresholdFilter->SetNumberOfIterations(params.maxIterations);

        // Attach progress observer
        if (progressCallback_) {
            auto observer = ProgressObserver::New();
            observer->setCallback(progressCallback_);
            thresholdFilter->AddObserver(itk::ProgressEvent(), observer);
        }

        getLogger()->debug("Starting threshold level set evolution...");
        thresholdFilter->Update();

        auto elapsedIterations = static_cast<int>(thresholdFilter->GetElapsedIterations());
        auto finalRMS = thresholdFilter->GetRMSChange();

        getLogger()->info("Threshold level set converged: {} iterations, RMS={:.6f}",
            elapsedIterations, finalRMS);

        // Convert level set to binary mask
        auto mask = levelSetToMask(thresholdFilter->GetOutput());

        LevelSetResult result;
        result.mask = mask;
        result.iterations = elapsedIterations;
        result.finalRMS = finalRMS;

        return result;
    }
    catch (const itk::ExceptionObject& e) {
        getLogger()->error("ITK exception: {}", e.GetDescription());
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK exception: ") + e.GetDescription()
        });
    }
    catch (const std::exception& e) {
        getLogger()->error("Standard exception: {}", e.what());
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InternalError,
            std::string("Standard exception: ") + e.what()
        });
    }
}

bool LevelSetSegmenter::isValidSeedPoint(
    ImageType::Pointer input,
    const LevelSetSeedPoint& seed
) {
    if (!input) {
        return false;
    }

    auto region = input->GetLargestPossibleRegion();
    auto size = region.GetSize();
    auto startIndex = region.GetIndex();

    // Convert physical coordinates to index
    ImageType::PointType point;
    point[0] = seed.x;
    point[1] = seed.y;
    point[2] = seed.z;

    ImageType::IndexType index;
    if (!input->TransformPhysicalPointToIndex(point, index)) {
        return false;
    }

    return index[0] >= startIndex[0] && index[0] < startIndex[0] + static_cast<long>(size[0]) &&
           index[1] >= startIndex[1] && index[1] < startIndex[1] + static_cast<long>(size[1]) &&
           index[2] >= startIndex[2] && index[2] < startIndex[2] + static_cast<long>(size[2]);
}

void LevelSetSegmenter::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

LevelSetSegmenter::FloatImageType::Pointer
LevelSetSegmenter::createFeatureImage(
    ImageType::Pointer input,
    double sigma
) const {
    try {
        // Cast to float
        using CastFilterType = itk::CastImageFilter<ImageType, FloatImageType>;
        auto castFilter = CastFilterType::New();
        castFilter->SetInput(input);

        // Compute gradient magnitude with Gaussian smoothing
        using GradientFilterType = itk::GradientMagnitudeRecursiveGaussianImageFilter<
            FloatImageType, FloatImageType>;
        auto gradientFilter = GradientFilterType::New();
        gradientFilter->SetInput(castFilter->GetOutput());
        gradientFilter->SetSigma(sigma);

        // Apply sigmoid function to create edge potential
        // Maps high gradients to low values (edges are barriers)
        using SigmoidFilterType = itk::SigmoidImageFilter<FloatImageType, FloatImageType>;
        auto sigmoidFilter = SigmoidFilterType::New();
        sigmoidFilter->SetInput(gradientFilter->GetOutput());
        sigmoidFilter->SetAlpha(-0.5);  // Steepness
        sigmoidFilter->SetBeta(3.0);    // Midpoint
        sigmoidFilter->SetOutputMinimum(0.0);
        sigmoidFilter->SetOutputMaximum(1.0);

        sigmoidFilter->Update();

        return sigmoidFilter->GetOutput();
    }
    catch (const itk::ExceptionObject& e) {
        getLogger()->error("Failed to create feature image: {}", e.GetDescription());
        return nullptr;
    }
}

LevelSetSegmenter::FloatImageType::Pointer
LevelSetSegmenter::createInitialLevelSet(
    ImageType::Pointer input,
    const std::vector<LevelSetSeedPoint>& seedPoints,
    double radius
) const {
    try {
        // Create a binary image with seed regions
        auto seedImage = MaskType::New();
        seedImage->SetRegions(input->GetLargestPossibleRegion());
        seedImage->SetOrigin(input->GetOrigin());
        seedImage->SetSpacing(input->GetSpacing());
        seedImage->SetDirection(input->GetDirection());
        seedImage->Allocate();
        seedImage->FillBuffer(0);

        // Mark seed regions as spheres
        auto spacing = input->GetSpacing();
        auto region = input->GetLargestPossibleRegion();

        for (const auto& seed : seedPoints) {
            ImageType::PointType centerPoint;
            centerPoint[0] = seed.x;
            centerPoint[1] = seed.y;
            centerPoint[2] = seed.z;

            ImageType::IndexType centerIndex;
            if (!input->TransformPhysicalPointToIndex(centerPoint, centerIndex)) {
                continue;
            }

            // Calculate radius in voxels for each dimension
            int radiusVoxelsX = static_cast<int>(std::ceil(radius / spacing[0]));
            int radiusVoxelsY = static_cast<int>(std::ceil(radius / spacing[1]));
            int radiusVoxelsZ = static_cast<int>(std::ceil(radius / spacing[2]));

            // Iterate over bounding box of sphere
            for (int dz = -radiusVoxelsZ; dz <= radiusVoxelsZ; ++dz) {
                for (int dy = -radiusVoxelsY; dy <= radiusVoxelsY; ++dy) {
                    for (int dx = -radiusVoxelsX; dx <= radiusVoxelsX; ++dx) {
                        ImageType::IndexType idx;
                        idx[0] = centerIndex[0] + dx;
                        idx[1] = centerIndex[1] + dy;
                        idx[2] = centerIndex[2] + dz;

                        if (!region.IsInside(idx)) {
                            continue;
                        }

                        // Check if voxel is inside sphere (in physical coordinates)
                        double distSq = (dx * spacing[0]) * (dx * spacing[0]) +
                                       (dy * spacing[1]) * (dy * spacing[1]) +
                                       (dz * spacing[2]) * (dz * spacing[2]);

                        if (distSq <= radius * radius) {
                            seedImage->SetPixel(idx, 1);
                        }
                    }
                }
            }
        }

        // Compute signed distance map
        // Inside seed regions will have negative values
        using DistanceMapFilterType = itk::SignedMaurerDistanceMapImageFilter<
            MaskType, FloatImageType>;
        auto distanceFilter = DistanceMapFilterType::New();
        distanceFilter->SetInput(seedImage);
        distanceFilter->SetInsideIsPositive(false);  // Inside is negative
        distanceFilter->SetUseImageSpacing(true);
        distanceFilter->SetSquaredDistance(false);

        distanceFilter->Update();

        // Negate to get proper level set (negative inside)
        auto output = distanceFilter->GetOutput();

        return output;
    }
    catch (const itk::ExceptionObject& e) {
        getLogger()->error("Failed to create initial level set: {}", e.GetDescription());
        return nullptr;
    }
}

std::optional<SegmentationError> LevelSetSegmenter::validateSeeds(
    ImageType::Pointer input,
    const std::vector<LevelSetSeedPoint>& seedPoints
) const {
    for (size_t i = 0; i < seedPoints.size(); ++i) {
        if (!isValidSeedPoint(input, seedPoints[i])) {
            return SegmentationError{
                SegmentationError::Code::InvalidParameters,
                "Seed point " + std::to_string(i) + " (" +
                std::to_string(seedPoints[i].x) + ", " +
                std::to_string(seedPoints[i].y) + ", " +
                std::to_string(seedPoints[i].z) + ") is out of image bounds"
            };
        }
    }
    return std::nullopt;
}

LevelSetSegmenter::MaskType::Pointer
LevelSetSegmenter::levelSetToMask(
    FloatImageType::Pointer levelSet
) const {
    // Threshold at zero: negative values are inside
    using ThresholdFilterType = itk::BinaryThresholdImageFilter<
        FloatImageType, MaskType>;
    auto thresholdFilter = ThresholdFilterType::New();
    thresholdFilter->SetInput(levelSet);
    thresholdFilter->SetLowerThreshold(-1e20);
    thresholdFilter->SetUpperThreshold(0.0);
    thresholdFilter->SetInsideValue(1);
    thresholdFilter->SetOutsideValue(0);

    thresholdFilter->Update();

    return thresholdFilter->GetOutput();
}

} // namespace dicom_viewer::services

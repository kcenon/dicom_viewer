#include "services/flow/velocity_field_assembler.hpp"

#include <cmath>

#include <itkCastImageFilter.h>
#include <itkComposeImageFilter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>
#include <itkMultiplyImageFilter.h>
#include <itkSubtractImageFilter.h>

#include "core/logging.hpp"

namespace {

auto& getLogger() {
    static auto logger = dicom_viewer::logging::LoggerFactory::create(
        "VelocityFieldAssembler");
    return logger;
}

using FloatImage3D = dicom_viewer::services::FloatImage3D;
using VectorImage3D = dicom_viewer::services::VectorImage3D;

/// Read a 3D scalar volume from a list of DICOM slice files
FloatImage3D::Pointer readScalarVolume(
    const std::vector<std::string>& sliceFiles) {
    using ReaderType = itk::ImageSeriesReader<FloatImage3D>;
    auto reader = ReaderType::New();
    auto gdcmIO = itk::GDCMImageIO::New();
    reader->SetImageIO(gdcmIO);
    reader->SetFileNames(sliceFiles);
    reader->Update();
    return reader->GetOutput();
}

/// Compose 3 scalar images into a vector image
VectorImage3D::Pointer composeVectorField(
    FloatImage3D::Pointer vx,
    FloatImage3D::Pointer vy,
    FloatImage3D::Pointer vz) {
    using ComposerType = itk::ComposeImageFilter<FloatImage3D, VectorImage3D>;
    auto composer = ComposerType::New();
    composer->SetInput(0, vx);
    composer->SetInput(1, vy);
    composer->SetInput(2, vz);
    composer->Update();
    return composer->GetOutput();
}

/// Apply VENC scaling to all pixels of a scalar image (in-place)
void applyVENCScalingToImage(
    FloatImage3D::Pointer image,
    double venc,
    bool isSigned) {
    using IteratorType = itk::ImageRegionIterator<FloatImage3D>;
    IteratorType it(image, image->GetLargestPossibleRegion());

    if (isSigned) {
        // Signed: assume pixel range is already centered around 0
        // Find max absolute pixel value for normalization
        float maxAbs = 0.0f;
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            float absVal = std::abs(it.Get());
            if (absVal > maxAbs) {
                maxAbs = absVal;
            }
        }

        if (maxAbs > 0.0f) {
            float scale = static_cast<float>(venc) / maxAbs;
            for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
                it.Set(it.Get() * scale);
            }
        }
    } else {
        // Unsigned: pixel range [0, maxVal], midpoint = maxVal/2
        float maxVal = 0.0f;
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() > maxVal) {
                maxVal = it.Get();
            }
        }

        if (maxVal > 0.0f) {
            float midpoint = maxVal / 2.0f;
            float scale = static_cast<float>(venc) / midpoint;
            for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
                it.Set((it.Get() - midpoint) * scale);
            }
        }
    }
}

}  // anonymous namespace

namespace dicom_viewer::services {

class VelocityFieldAssembler::Impl {
public:
    VelocityFieldAssembler::ProgressCallback progressCallback;

    void reportProgress(double progress) const {
        if (progressCallback) {
            progressCallback(progress);
        }
    }
};

VelocityFieldAssembler::VelocityFieldAssembler()
    : impl_(std::make_unique<Impl>()) {}

VelocityFieldAssembler::~VelocityFieldAssembler() = default;

VelocityFieldAssembler::VelocityFieldAssembler(VelocityFieldAssembler&&) noexcept = default;
VelocityFieldAssembler& VelocityFieldAssembler::operator=(VelocityFieldAssembler&&) noexcept = default;

void VelocityFieldAssembler::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<std::vector<VelocityPhase>, FlowError>
VelocityFieldAssembler::assembleAllPhases(const FlowSeriesInfo& seriesInfo) const {
    auto logger = getLogger();

    if (seriesInfo.frameMatrix.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "No frame matrix data provided"});
    }

    impl_->reportProgress(0.0);

    std::vector<VelocityPhase> phases;
    phases.reserve(seriesInfo.phaseCount);

    for (int i = 0; i < seriesInfo.phaseCount; ++i) {
        auto result = assemblePhase(seriesInfo, i);
        if (!result) {
            logger->warn("Failed to assemble phase {}: {}",
                         i, result.error().toString());
            continue;
        }
        phases.push_back(std::move(result.value()));

        impl_->reportProgress(
            static_cast<double>(i + 1) / static_cast<double>(seriesInfo.phaseCount));
    }

    if (phases.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::ParseFailed,
            "No phases could be assembled"});
    }

    logger->info("Assembled {} of {} phases", phases.size(), seriesInfo.phaseCount);

    impl_->reportProgress(1.0);
    return phases;
}

std::expected<VelocityPhase, FlowError>
VelocityFieldAssembler::assemblePhase(
    const FlowSeriesInfo& seriesInfo, int phaseIndex) const {
    auto logger = getLogger();

    if (phaseIndex < 0 ||
        phaseIndex >= static_cast<int>(seriesInfo.frameMatrix.size())) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Phase index " + std::to_string(phaseIndex) + " out of range [0, " +
                std::to_string(seriesInfo.frameMatrix.size()) + ")"});
    }

    const auto& phaseFrames = seriesInfo.frameMatrix[phaseIndex];

    // Check required velocity components
    auto hasComponent = [&](VelocityComponent comp) {
        auto it = phaseFrames.find(comp);
        return it != phaseFrames.end() && !it->second.empty();
    };

    if (!hasComponent(VelocityComponent::Vx) ||
        !hasComponent(VelocityComponent::Vy) ||
        !hasComponent(VelocityComponent::Vz)) {
        return std::unexpected(FlowError{
            FlowError::Code::InconsistentData,
            "Phase " + std::to_string(phaseIndex) +
                " missing velocity components (need Vx, Vy, Vz)"});
    }

    try {
        // Read velocity component volumes
        logger->debug("Reading velocity components for phase {}", phaseIndex);

        auto vxImage = readScalarVolume(phaseFrames.at(VelocityComponent::Vx));
        auto vyImage = readScalarVolume(phaseFrames.at(VelocityComponent::Vy));
        auto vzImage = readScalarVolume(phaseFrames.at(VelocityComponent::Vz));

        // Apply VENC scaling to convert pixel values → velocity (cm/s)
        double vencX = seriesInfo.venc[0];
        double vencY = seriesInfo.venc[1];
        double vencZ = seriesInfo.venc[2];

        applyVENCScalingToImage(vxImage, vencX, seriesInfo.isSignedPhase);
        applyVENCScalingToImage(vyImage, vencY, seriesInfo.isSignedPhase);
        applyVENCScalingToImage(vzImage, vencZ, seriesInfo.isSignedPhase);

        // Compose into vector field
        auto vectorField = composeVectorField(vxImage, vyImage, vzImage);

        // Read magnitude image if available
        FloatImage3D::Pointer magnitudeImage;
        if (hasComponent(VelocityComponent::Magnitude)) {
            magnitudeImage = readScalarVolume(
                phaseFrames.at(VelocityComponent::Magnitude));
        }

        VelocityPhase phase;
        phase.velocityField = vectorField;
        phase.magnitudeImage = magnitudeImage;
        phase.phaseIndex = phaseIndex;
        phase.triggerTime = seriesInfo.temporalResolution * phaseIndex;

        auto size = vectorField->GetLargestPossibleRegion().GetSize();
        logger->debug("Phase {} assembled: {}x{}x{}, VENC=[{:.1f},{:.1f},{:.1f}]",
                      phaseIndex, size[0], size[1], size[2],
                      vencX, vencY, vencZ);

        return phase;

    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(FlowError{
            FlowError::Code::ParseFailed,
            "ITK error assembling phase " + std::to_string(phaseIndex) +
                ": " + e.GetDescription()});
    } catch (const std::exception& e) {
        return std::unexpected(FlowError{
            FlowError::Code::InternalError,
            "Error assembling phase " + std::to_string(phaseIndex) +
                ": " + e.what()});
    }
}

float VelocityFieldAssembler::applyVENCScaling(
    float pixelValue, double venc, int maxPixelValue, bool isSigned) {
    if (maxPixelValue == 0) {
        return 0.0f;
    }

    if (isSigned) {
        // Signed: velocity = (pixel_value / max_possible_value) × VENC
        return static_cast<float>(
            (static_cast<double>(pixelValue) / static_cast<double>(maxPixelValue)) * venc);
    }

    // Unsigned: velocity = ((pixel_value - midpoint) / midpoint) × VENC
    double midpoint = static_cast<double>(maxPixelValue) / 2.0;
    return static_cast<float>(
        ((static_cast<double>(pixelValue) - midpoint) / midpoint) * venc);
}

}  // namespace dicom_viewer::services

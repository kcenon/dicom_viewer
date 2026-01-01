#include "core/hounsfield_converter.hpp"

#include <algorithm>
#include <cmath>

#include <itkImageRegionIterator.h>

namespace dicom_viewer::core {

namespace hounsfield {

const char* getTissueTypeName(double hu) {
    if (hu <= Air + 100) {
        return "Air";
    }
    if (isInTissueRange(hu, Lung)) {
        return Lung.name;
    }
    if (isInTissueRange(hu, Fat)) {
        return Fat.name;
    }
    if (std::abs(hu - Water) < 10.0) {
        return "Water";
    }
    if (isInTissueRange(hu, Muscle)) {
        return Muscle.name;
    }
    if (isInTissueRange(hu, Blood)) {
        return Blood.name;
    }
    if (isInTissueRange(hu, Liver)) {
        return Liver.name;
    }
    if (isInTissueRange(hu, SoftTissue)) {
        return SoftTissue.name;
    }
    if (isInTissueRange(hu, CancellousBone)) {
        return CancellousBone.name;
    }
    if (isInTissueRange(hu, CorticalBone)) {
        return CorticalBone.name;
    }
    return "Unknown";
}

} // namespace hounsfield

double HounsfieldConverter::convert(int storedValue, double slope, double intercept) {
    return static_cast<double>(storedValue) * slope + intercept;
}

double HounsfieldConverter::convert(int storedValue, const RescaleParameters& params) {
    return convert(storedValue, params.slope, params.intercept);
}

int HounsfieldConverter::convertToStoredValue(double huValue, double slope, double intercept) {
    if (std::abs(slope) < std::numeric_limits<double>::epsilon()) {
        return 0;
    }
    return static_cast<int>(std::round((huValue - intercept) / slope));
}

void HounsfieldConverter::applyToImage(
    CTImageType::Pointer image,
    double slope,
    double intercept,
    bool clamp)
{
    if (!image) {
        return;
    }

    using IteratorType = itk::ImageRegionIterator<CTImageType>;
    IteratorType it(image, image->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        double hu = convert(it.Get(), slope, intercept);

        if (clamp) {
            hu = clampHU(hu);
        }

        it.Set(static_cast<short>(std::round(hu)));
    }
}

void HounsfieldConverter::applyToImage(
    CTImageType::Pointer image,
    const RescaleParameters& params,
    bool clamp)
{
    applyToImage(image, params.slope, params.intercept, clamp);
}

bool HounsfieldConverter::validateParameters(double slope, double intercept) {
    // Slope must be non-zero
    if (std::abs(slope) < std::numeric_limits<double>::epsilon()) {
        return false;
    }

    // Check for NaN or infinity
    if (!std::isfinite(slope) || !std::isfinite(intercept)) {
        return false;
    }

    return true;
}

double HounsfieldConverter::clampHU(double hu) {
    return std::clamp(hu, hounsfield::MinHU, hounsfield::MaxHU);
}

HounsfieldConverter::RescaleParameters HounsfieldConverter::getDefaultParameters() {
    return RescaleParameters{
        hounsfield::DefaultSlope,
        hounsfield::DefaultIntercept
    };
}

} // namespace dicom_viewer::core

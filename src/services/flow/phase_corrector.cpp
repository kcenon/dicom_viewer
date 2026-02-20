#include "services/flow/phase_corrector.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>

#include <itkBinaryErodeImageFilter.h>
#include <itkFlatStructuringElement.h>
#include <itkImageDuplicator.h>
#include <itkImageRegionIterator.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkOtsuThresholdImageFilter.h>

#include <kcenon/common/logging/log_macros.h>

namespace {

using FloatImage3D = dicom_viewer::services::FloatImage3D;
using VectorImage3D = dicom_viewer::services::VectorImage3D;
using MaskImage3D = dicom_viewer::services::MaskImage3D;

/// Deep-copy a vector image
VectorImage3D::Pointer duplicateVectorImage(VectorImage3D::Pointer input) {
    using DuplicatorType = itk::ImageDuplicator<VectorImage3D>;
    auto duplicator = DuplicatorType::New();
    duplicator->SetInputImage(input);
    duplicator->Update();
    return duplicator->GetOutput();
}

/// Deep-copy a scalar image
FloatImage3D::Pointer duplicateScalarImage(FloatImage3D::Pointer input) {
    using DuplicatorType = itk::ImageDuplicator<FloatImage3D>;
    auto duplicator = DuplicatorType::New();
    duplicator->SetInputImage(input);
    duplicator->Update();
    return duplicator->GetOutput();
}

/// Count the number of polynomial terms for given order in 3D
int polynomialTermCount(int order) {
    // order 1: 1 + x + y + z = 4 terms
    // order 2: + xx + yy + zz + xy + xz + yz = 10 terms
    // order 3: + xxx + yyy + zzz + xxy + xxz + xyy + yyz + xzz + yzz + xyz = 20 terms
    return (order + 1) * (order + 2) * (order + 3) / 6;
}

}  // anonymous namespace

namespace dicom_viewer::services {

class PhaseCorrector::Impl {
public:
    PhaseCorrector::ProgressCallback progressCallback;

    void reportProgress(double progress) const {
        if (progressCallback) {
            progressCallback(progress);
        }
    }
};

PhaseCorrector::PhaseCorrector()
    : impl_(std::make_unique<Impl>()) {}

PhaseCorrector::~PhaseCorrector() = default;

PhaseCorrector::PhaseCorrector(PhaseCorrector&&) noexcept = default;
PhaseCorrector& PhaseCorrector::operator=(PhaseCorrector&&) noexcept = default;

void PhaseCorrector::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<VelocityPhase, FlowError>
PhaseCorrector::correctPhase(
    const VelocityPhase& phase, double venc,
    const PhaseCorrectionConfig& config) const {
    if (!config.isValid()) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Invalid correction configuration"});
    }

    if (!phase.velocityField) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "Velocity field is null"});
    }

    if (venc <= 0.0) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "VENC must be positive, got: " + std::to_string(venc)});
    }

    impl_->reportProgress(0.0);

    try {
        // Create corrected copies
        VelocityPhase corrected;
        corrected.velocityField = duplicateVectorImage(phase.velocityField);
        corrected.magnitudeImage = phase.magnitudeImage
            ? duplicateScalarImage(phase.magnitudeImage) : nullptr;
        corrected.phaseIndex = phase.phaseIndex;
        corrected.triggerTime = phase.triggerTime;

        impl_->reportProgress(0.1);

        // Step 1: Aliasing unwrap
        if (config.enableAliasingUnwrap) {
            LOG_DEBUG(std::format("Unwrapping velocity aliasing for phase {}",
                                  phase.phaseIndex));
            unwrapAliasing(corrected.velocityField, venc,
                           config.aliasingThreshold);
        }

        impl_->reportProgress(0.4);

        // Step 2: Eddy current correction
        if (config.enableEddyCurrentCorrection && corrected.magnitudeImage) {
            LOG_DEBUG(std::format("Correcting eddy currents for phase {}",
                                  phase.phaseIndex));
            correctEddyCurrent(corrected.velocityField,
                               corrected.magnitudeImage,
                               config.polynomialOrder);
        }

        impl_->reportProgress(0.8);

        // Step 3: Maxwell correction is optional — requires sequence parameters
        // that are typically not available in standard DICOM. Skipped unless
        // explicitly implemented with vendor-specific metadata.

        impl_->reportProgress(1.0);

        LOG_DEBUG(std::format("Phase {} correction complete", phase.phaseIndex));
        return corrected;

    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(FlowError{
            FlowError::Code::InternalError,
            "ITK error during phase correction: " +
                std::string(e.GetDescription())});
    } catch (const std::exception& e) {
        return std::unexpected(FlowError{
            FlowError::Code::InternalError,
            "Error during phase correction: " + std::string(e.what())});
    }
}

void PhaseCorrector::unwrapAliasing(
    VectorImage3D::Pointer velocity, double venc, double threshold) {
    if (!velocity) return;

    auto region = velocity->GetLargestPossibleRegion();
    auto size = region.GetSize();
    double jumpThreshold = threshold * venc;
    double twoVenc = 2.0 * venc;
    int numComponents = velocity->GetNumberOfComponentsPerPixel();

    // Neighbor-based phase unwrapping along each axis
    for (int comp = 0; comp < numComponents; ++comp) {
        // Scan along X axis
        for (unsigned int z = 0; z < size[2]; ++z) {
            for (unsigned int y = 0; y < size[1]; ++y) {
                double accumulated = 0.0;
                VectorImage3D::IndexType prevIdx = {{0, static_cast<long>(y),
                                                     static_cast<long>(z)}};
                auto prevPixel = velocity->GetPixel(prevIdx);
                double prevVal = prevPixel[comp];

                for (unsigned int x = 1; x < size[0]; ++x) {
                    VectorImage3D::IndexType idx = {
                        {static_cast<long>(x), static_cast<long>(y),
                         static_cast<long>(z)}};
                    auto pixel = velocity->GetPixel(idx);
                    double curVal = pixel[comp];
                    double diff = curVal - prevVal;

                    if (diff > jumpThreshold) {
                        accumulated -= twoVenc;
                    } else if (diff < -jumpThreshold) {
                        accumulated += twoVenc;
                    }

                    if (accumulated != 0.0) {
                        pixel[comp] = static_cast<float>(curVal + accumulated);
                        velocity->SetPixel(idx, pixel);
                    }

                    prevVal = curVal;
                }
            }
        }
    }
}

MaskImage3D::Pointer PhaseCorrector::createStationaryMask(
    FloatImage3D::Pointer magnitude) {
    if (!magnitude) return nullptr;

    // Otsu threshold to separate tissue from background
    using OtsuType = itk::OtsuThresholdImageFilter<FloatImage3D, MaskImage3D>;
    auto otsu = OtsuType::New();
    otsu->SetInput(magnitude);
    otsu->SetInsideValue(0);     // Below threshold = background
    otsu->SetOutsideValue(255);  // Above threshold = tissue
    otsu->Update();

    auto mask = otsu->GetOutput();

    // Erode mask to exclude tissue boundaries (reduce partial volume effects)
    using StructType = itk::FlatStructuringElement<3>;
    StructType::RadiusType radius;
    radius.Fill(1);
    auto element = StructType::Ball(radius);

    using ErodeType = itk::BinaryErodeImageFilter<MaskImage3D, MaskImage3D,
                                                   StructType>;
    auto erode = ErodeType::New();
    erode->SetInput(mask);
    erode->SetKernel(element);
    erode->SetForegroundValue(255);
    erode->SetBackgroundValue(0);
    erode->Update();

    return erode->GetOutput();
}

std::vector<double> PhaseCorrector::fitPolynomialBackground(
    FloatImage3D::Pointer scalarField,
    MaskImage3D::Pointer mask, int order) {
    int numTerms = polynomialTermCount(order);
    std::vector<double> coeffs(numTerms, 0.0);

    if (!scalarField || !mask) return coeffs;

    auto region = scalarField->GetLargestPossibleRegion();
    auto size = region.GetSize();

    // Normalize coordinates to [-1, 1] range
    double scaleX = (size[0] > 1) ? 2.0 / (size[0] - 1) : 1.0;
    double scaleY = (size[1] > 1) ? 2.0 / (size[1] - 1) : 1.0;
    double scaleZ = (size[2] > 1) ? 2.0 / (size[2] - 1) : 1.0;

    // Collect samples from masked region
    std::vector<std::vector<double>> A;  // Design matrix rows
    std::vector<double> b;               // Observed values

    using IterType = itk::ImageRegionIteratorWithIndex<FloatImage3D>;
    IterType it(scalarField, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto idx = it.GetIndex();
        if (mask->GetPixel(idx) == 0) continue;

        double x = idx[0] * scaleX - 1.0;
        double y = idx[1] * scaleY - 1.0;
        double z = idx[2] * scaleZ - 1.0;

        // Build polynomial basis row
        std::vector<double> row;
        row.reserve(numTerms);

        // Generate terms up to given order
        for (int p = 0; p <= order; ++p) {
            for (int i = p; i >= 0; --i) {
                for (int j = p - i; j >= 0; --j) {
                    int k = p - i - j;
                    double term = std::pow(x, i) * std::pow(y, j) * std::pow(z, k);
                    row.push_back(term);
                }
            }
        }

        A.push_back(std::move(row));
        b.push_back(it.Get());
    }

    if (A.size() < static_cast<size_t>(numTerms)) {
        // Not enough samples for fitting
        return coeffs;
    }

    // Solve via normal equations: (A^T A) x = A^T b
    // Using simple Cholesky-free approach for small systems
    std::vector<std::vector<double>> ATA(numTerms,
                                          std::vector<double>(numTerms, 0.0));
    std::vector<double> ATb(numTerms, 0.0);

    for (size_t s = 0; s < A.size(); ++s) {
        for (int i = 0; i < numTerms; ++i) {
            ATb[i] += A[s][i] * b[s];
            for (int j = i; j < numTerms; ++j) {
                ATA[i][j] += A[s][i] * A[s][j];
            }
        }
    }

    // Symmetrize
    for (int i = 0; i < numTerms; ++i) {
        for (int j = 0; j < i; ++j) {
            ATA[i][j] = ATA[j][i];
        }
    }

    // Gaussian elimination with partial pivoting
    std::vector<double> augmented(numTerms);
    for (int i = 0; i < numTerms; ++i) {
        augmented[i] = ATb[i];
    }

    for (int col = 0; col < numTerms; ++col) {
        // Find pivot
        int maxRow = col;
        double maxVal = std::abs(ATA[col][col]);
        for (int row = col + 1; row < numTerms; ++row) {
            if (std::abs(ATA[row][col]) > maxVal) {
                maxVal = std::abs(ATA[row][col]);
                maxRow = row;
            }
        }

        if (maxVal < 1e-12) continue;  // Singular or near-singular

        // Swap rows
        if (maxRow != col) {
            std::swap(ATA[col], ATA[maxRow]);
            std::swap(augmented[col], augmented[maxRow]);
        }

        // Eliminate below
        for (int row = col + 1; row < numTerms; ++row) {
            double factor = ATA[row][col] / ATA[col][col];
            for (int j = col; j < numTerms; ++j) {
                ATA[row][j] -= factor * ATA[col][j];
            }
            augmented[row] -= factor * augmented[col];
        }
    }

    // Back substitution
    for (int i = numTerms - 1; i >= 0; --i) {
        double sum = augmented[i];
        for (int j = i + 1; j < numTerms; ++j) {
            sum -= ATA[i][j] * coeffs[j];
        }
        if (std::abs(ATA[i][i]) > 1e-12) {
            coeffs[i] = sum / ATA[i][i];
        }
    }

    return coeffs;
}

double PhaseCorrector::evaluatePolynomial(
    const std::vector<double>& coeffs,
    double x, double y, double z, int order) {
    double result = 0.0;
    int idx = 0;

    for (int p = 0; p <= order; ++p) {
        for (int i = p; i >= 0; --i) {
            for (int j = p - i; j >= 0; --j) {
                int k = p - i - j;
                if (idx < static_cast<int>(coeffs.size())) {
                    result += coeffs[idx] *
                              std::pow(x, i) * std::pow(y, j) * std::pow(z, k);
                }
                ++idx;
            }
        }
    }

    return result;
}

void PhaseCorrector::correctEddyCurrent(
    VectorImage3D::Pointer velocity,
    FloatImage3D::Pointer magnitude,
    int polynomialOrder) {
    if (!velocity || !magnitude) return;

    // Create stationary tissue mask
    auto mask = createStationaryMask(magnitude);
    if (!mask) {
        LOG_WARNING("Failed to create stationary tissue mask");
        return;
    }

    auto region = velocity->GetLargestPossibleRegion();
    auto size = region.GetSize();
    int numComponents = velocity->GetNumberOfComponentsPerPixel();

    double scaleX = (size[0] > 1) ? 2.0 / (size[0] - 1) : 1.0;
    double scaleY = (size[1] > 1) ? 2.0 / (size[1] - 1) : 1.0;
    double scaleZ = (size[2] > 1) ? 2.0 / (size[2] - 1) : 1.0;

    // Process each velocity component separately
    for (int comp = 0; comp < numComponents; ++comp) {
        // Extract single component as scalar image
        auto componentImage = FloatImage3D::New();
        componentImage->SetRegions(region);
        componentImage->SetSpacing(velocity->GetSpacing());
        componentImage->SetOrigin(velocity->GetOrigin());
        componentImage->SetDirection(velocity->GetDirection());
        componentImage->Allocate();

        using VecIterType = itk::ImageRegionIteratorWithIndex<VectorImage3D>;
        using ScalarIterType = itk::ImageRegionIterator<FloatImage3D>;

        VecIterType vecIt(velocity, region);
        ScalarIterType scalarIt(componentImage, region);
        for (vecIt.GoToBegin(), scalarIt.GoToBegin();
             !vecIt.IsAtEnd(); ++vecIt, ++scalarIt) {
            scalarIt.Set(vecIt.Get()[comp]);
        }

        // Fit polynomial background
        auto coeffs = fitPolynomialBackground(componentImage, mask,
                                               polynomialOrder);

        // Subtract polynomial background from velocity
        for (vecIt.GoToBegin(); !vecIt.IsAtEnd(); ++vecIt) {
            auto idx = vecIt.GetIndex();
            double x = idx[0] * scaleX - 1.0;
            double y = idx[1] * scaleY - 1.0;
            double z = idx[2] * scaleZ - 1.0;

            double bgValue = evaluatePolynomial(coeffs, x, y, z,
                                                 polynomialOrder);
            auto pixel = vecIt.Get();
            pixel[comp] = static_cast<float>(pixel[comp] - bgValue);
            velocity->SetPixel(idx, pixel);
        }

        LOG_DEBUG(std::format("Eddy current correction: component {} fitted with {} terms",
                              comp, coeffs.size()));
    }
}

}  // namespace dicom_viewer::services

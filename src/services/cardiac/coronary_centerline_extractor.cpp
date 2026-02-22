// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "services/cardiac/coronary_centerline_extractor.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>
#include <queue>

#include <kcenon/common/logging/log_macros.h>

#include <itkCastImageFilter.h>
#include <itkFastMarchingImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkHessianRecursiveGaussianImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
#include <itkSymmetricEigenAnalysisImageFilter.h>
#include <itkSymmetricSecondRankTensor.h>

namespace dicom_viewer::services {

using ImageType = itk::Image<short, 3>;
using FloatImageType = itk::Image<float, 3>;
using HessianType = itk::SymmetricSecondRankTensor<double, 3>;
using HessianImageType = itk::Image<HessianType, 3>;

class CoronaryCenterlineExtractor::Impl {
public:
    // Compute Frangi vesselness for a single scale
    FloatImageType::Pointer computeVesselnessAtScale(
        ImageType::Pointer image, double sigma,
        double alpha, double beta, double gamma) const;

    // Backtrack from endpoint to seed through arrival time field
    std::vector<CenterlinePoint> backtrack(
        FloatImageType::Pointer timeMap,
        const std::array<double, 3>& seedPoint,
        const std::array<double, 3>& endPoint,
        double stepSize) const;

    // Compute tangent and normal at each centerline point
    static void computeFrenetFrame(std::vector<CenterlinePoint>& points);

    // Cubic B-spline basis function
    static double bsplineBasis(int i, int degree, double t,
                               const std::vector<double>& knots);
};

CoronaryCenterlineExtractor::CoronaryCenterlineExtractor()
    : impl_(std::make_unique<Impl>()) {}

CoronaryCenterlineExtractor::~CoronaryCenterlineExtractor() = default;

CoronaryCenterlineExtractor::CoronaryCenterlineExtractor(CoronaryCenterlineExtractor&&) noexcept = default;
CoronaryCenterlineExtractor& CoronaryCenterlineExtractor::operator=(CoronaryCenterlineExtractor&&) noexcept = default;

// =============================================================================
// Frangi Vesselness Filter
// =============================================================================

FloatImageType::Pointer CoronaryCenterlineExtractor::Impl::computeVesselnessAtScale(
    ImageType::Pointer image, double sigma,
    double alpha, double beta, double gamma) const
{
    // Step 1: Compute Hessian at this scale
    using HessianFilterType = itk::HessianRecursiveGaussianImageFilter<ImageType>;
    auto hessianFilter = HessianFilterType::New();
    hessianFilter->SetInput(image);
    hessianFilter->SetSigma(sigma);
    hessianFilter->Update();

    auto hessianImage = hessianFilter->GetOutput();

    // Step 2: Compute eigenvalues
    auto region = hessianImage->GetLargestPossibleRegion();
    auto vesselnessImage = FloatImageType::New();
    vesselnessImage->SetRegions(region);
    vesselnessImage->SetSpacing(image->GetSpacing());
    vesselnessImage->SetOrigin(image->GetOrigin());
    vesselnessImage->SetDirection(image->GetDirection());
    vesselnessImage->Allocate();
    vesselnessImage->FillBuffer(0.0f);

    double alpha2 = 2.0 * alpha * alpha;
    double beta2 = 2.0 * beta * beta;
    double gamma2 = 2.0 * gamma * gamma;

    itk::ImageRegionConstIterator<HessianImageType> hIt(hessianImage, region);
    itk::ImageRegionIterator<FloatImageType> vIt(vesselnessImage, region);

    for (hIt.GoToBegin(), vIt.GoToBegin(); !hIt.IsAtEnd(); ++hIt, ++vIt) {
        auto tensor = hIt.Get();

        // Extract eigenvalues using ITK's symmetric eigen analysis
        using EigenValueArrayType = itk::FixedArray<double, 3>;
        EigenValueArrayType eigenValues;

        // Manual eigenvalue computation for 3x3 symmetric matrix
        // Use the characteristic equation approach
        double a00 = tensor(0, 0);
        double a01 = tensor(0, 1);
        double a02 = tensor(0, 2);
        double a11 = tensor(1, 1);
        double a12 = tensor(1, 2);
        double a22 = tensor(2, 2);

        // Compute matrix invariants
        double p1 = a01 * a01 + a02 * a02 + a12 * a12;

        if (p1 < 1e-20) {
            // Matrix is diagonal
            eigenValues[0] = a00;
            eigenValues[1] = a11;
            eigenValues[2] = a22;
        } else {
            double q = (a00 + a11 + a22) / 3.0;
            double p2 = (a00 - q) * (a00 - q) + (a11 - q) * (a11 - q)
                       + (a22 - q) * (a22 - q) + 2.0 * p1;
            double p = std::sqrt(p2 / 6.0);

            // B = (1/p) * (A - q*I)
            double b00 = (a00 - q) / p;
            double b01 = a01 / p;
            double b02 = a02 / p;
            double b11 = (a11 - q) / p;
            double b12 = a12 / p;
            double b22 = (a22 - q) / p;

            // det(B)
            double detB = b00 * (b11 * b22 - b12 * b12)
                        - b01 * (b01 * b22 - b12 * b02)
                        + b02 * (b01 * b12 - b11 * b02);
            double r = detB / 2.0;
            r = std::clamp(r, -1.0, 1.0);

            double phi = std::acos(r) / 3.0;

            eigenValues[0] = q + 2.0 * p * std::cos(phi);
            eigenValues[2] = q + 2.0 * p * std::cos(phi + (2.0 * M_PI / 3.0));
            eigenValues[1] = 3.0 * q - eigenValues[0] - eigenValues[2];
        }

        // Sort by absolute value: |λ1| ≤ |λ2| ≤ |λ3|
        std::sort(eigenValues.Begin(), eigenValues.End(),
                  [](double a, double b) { return std::abs(a) < std::abs(b); });

        double l1 = eigenValues[0];
        double l2 = eigenValues[1];
        double l3 = eigenValues[2];

        // Vessel condition: λ1 ≈ 0, λ2 < 0, λ3 < 0 (bright vessels on dark background)
        if (l2 >= 0.0 || l3 >= 0.0) {
            vIt.Set(0.0f);
            continue;
        }

        // Frangi ratios
        double absL2 = std::abs(l2);
        double absL3 = std::abs(l3);
        double RA = absL2 / (absL3 + 1e-10);  // plate vs. line
        double RB = std::abs(l1) / (std::sqrt(absL2 * absL3) + 1e-10);  // blob vs. line
        double S = std::sqrt(l1 * l1 + l2 * l2 + l3 * l3);  // Frobenius norm

        double vesselness = (1.0 - std::exp(-RA * RA / alpha2))
                          * std::exp(-RB * RB / beta2)
                          * (1.0 - std::exp(-S * S / gamma2));

        vIt.Set(static_cast<float>(vesselness));
    }

    return vesselnessImage;
}

std::expected<FloatImageType::Pointer, CardiacError>
CoronaryCenterlineExtractor::computeVesselness(
    ImageType::Pointer image,
    const VesselnessParams& params) const
{
    if (!image) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null image pointer for vesselness computation"
        });
    }

    if (params.sigmaSteps < 1 || params.sigmaMin <= 0.0 || params.sigmaMax <= 0.0) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Invalid vesselness parameters"
        });
    }

    LOG_INFO(std::format("Computing multi-scale vesselness (sigma {:.1f}-{:.1f}, {} steps)",
                         params.sigmaMin, params.sigmaMax, params.sigmaSteps));

    auto region = image->GetLargestPossibleRegion();

    // Initialize maximum vesselness image
    auto maxVesselness = FloatImageType::New();
    maxVesselness->SetRegions(region);
    maxVesselness->SetSpacing(image->GetSpacing());
    maxVesselness->SetOrigin(image->GetOrigin());
    maxVesselness->SetDirection(image->GetDirection());
    maxVesselness->Allocate();
    maxVesselness->FillBuffer(0.0f);

    // Compute vesselness at each scale and take maximum
    for (int step = 0; step < params.sigmaSteps; ++step) {
        double sigma = params.sigmaMin;
        if (params.sigmaSteps > 1) {
            double t = static_cast<double>(step) / (params.sigmaSteps - 1);
            // Logarithmic scale spacing (better for vessel analysis)
            sigma = params.sigmaMin * std::pow(params.sigmaMax / params.sigmaMin, t);
        }

        LOG_DEBUG(std::format("  Scale sigma={:.2f} mm", sigma));

        auto scaleVesselness = impl_->computeVesselnessAtScale(
            image, sigma, params.alpha, params.beta, params.gamma);

        // Take maximum across scales
        itk::ImageRegionConstIterator<FloatImageType> sIt(scaleVesselness, region);
        itk::ImageRegionIterator<FloatImageType> mIt(maxVesselness, region);

        for (sIt.GoToBegin(), mIt.GoToBegin(); !sIt.IsAtEnd(); ++sIt, ++mIt) {
            if (sIt.Get() > mIt.Get()) {
                mIt.Set(sIt.Get());
            }
        }
    }

    LOG_INFO("Vesselness computation complete");
    return maxVesselness;
}

// =============================================================================
// Centerline Extraction via Minimal Path
// =============================================================================

std::vector<CenterlinePoint> CoronaryCenterlineExtractor::Impl::backtrack(
    FloatImageType::Pointer timeMap,
    const std::array<double, 3>& seedPoint,
    const std::array<double, 3>& endPoint,
    double stepSize) const
{
    std::vector<CenterlinePoint> path;
    auto spacing = timeMap->GetSpacing();
    auto region = timeMap->GetLargestPossibleRegion();

    // Start at endpoint and backtrack toward seed
    std::array<double, 3> current = endPoint;
    double minStepSize = std::min({spacing[0], spacing[1], spacing[2]}) * 0.5;
    double actualStepSize = std::max(stepSize, minStepSize);

    int maxIterations = 100000;
    double seedDist2Threshold = actualStepSize * actualStepSize * 4.0;

    for (int iter = 0; iter < maxIterations; ++iter) {
        CenterlinePoint pt;
        pt.position = current;
        path.push_back(pt);

        // Check if we've reached the seed
        double dx = current[0] - seedPoint[0];
        double dy = current[1] - seedPoint[1];
        double dz = current[2] - seedPoint[2];
        double dist2 = dx * dx + dy * dy + dz * dz;

        if (dist2 < seedDist2Threshold) {
            CenterlinePoint seedPt;
            seedPt.position = seedPoint;
            path.push_back(seedPt);
            break;
        }

        // Compute gradient of time map at current position using central differences
        FloatImageType::PointType physPoint;
        physPoint[0] = current[0];
        physPoint[1] = current[1];
        physPoint[2] = current[2];

        FloatImageType::IndexType idx;
        if (!timeMap->TransformPhysicalPointToIndex(physPoint, idx)) {
            break;  // Out of bounds
        }

        if (!region.IsInside(idx)) {
            break;
        }

        // Central differences for gradient
        std::array<double, 3> gradient = {0.0, 0.0, 0.0};
        for (int dim = 0; dim < 3; ++dim) {
            FloatImageType::IndexType idxPlus = idx;
            FloatImageType::IndexType idxMinus = idx;

            auto size = region.GetSize();
            if (idx[dim] > 0 && idx[dim] < static_cast<long>(size[dim]) - 1) {
                idxPlus[dim] = idx[dim] + 1;
                idxMinus[dim] = idx[dim] - 1;
                gradient[dim] = (timeMap->GetPixel(idxPlus) - timeMap->GetPixel(idxMinus))
                              / (2.0 * spacing[dim]);
            } else if (idx[dim] == 0) {
                idxPlus[dim] = idx[dim] + 1;
                gradient[dim] = (timeMap->GetPixel(idxPlus) - timeMap->GetPixel(idx))
                              / spacing[dim];
            } else {
                idxMinus[dim] = idx[dim] - 1;
                gradient[dim] = (timeMap->GetPixel(idx) - timeMap->GetPixel(idxMinus))
                              / spacing[dim];
            }
        }

        // Normalize gradient
        double gradMag = std::sqrt(gradient[0] * gradient[0]
                                 + gradient[1] * gradient[1]
                                 + gradient[2] * gradient[2]);

        if (gradMag < 1e-10) {
            break;  // Stuck
        }

        // Step in negative gradient direction (toward lower time values → seed)
        for (int dim = 0; dim < 3; ++dim) {
            current[dim] -= actualStepSize * gradient[dim] / gradMag;
        }
    }

    // Reverse so path goes from seed to endpoint
    std::reverse(path.begin(), path.end());
    return path;
}

std::expected<CenterlineResult, CardiacError>
CoronaryCenterlineExtractor::extractCenterline(
    const std::array<double, 3>& seedPoint,
    const std::array<double, 3>& endPoint,
    FloatImageType::Pointer vesselness,
    ImageType::Pointer originalImage) const
{
    if (!vesselness || !originalImage) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Null image pointer for centerline extraction"
        });
    }

    LOG_INFO(std::format("Extracting centerline from ({:.1f},{:.1f},{:.1f}) to ({:.1f},{:.1f},{:.1f})",
                         seedPoint[0], seedPoint[1], seedPoint[2],
                         endPoint[0], endPoint[1], endPoint[2]));

    // Convert vesselness to speed image: speed = epsilon + vesselness
    auto region = vesselness->GetLargestPossibleRegion();
    auto speedImage = FloatImageType::New();
    speedImage->SetRegions(region);
    speedImage->SetSpacing(vesselness->GetSpacing());
    speedImage->SetOrigin(vesselness->GetOrigin());
    speedImage->SetDirection(vesselness->GetDirection());
    speedImage->Allocate();

    constexpr double epsilon = 0.001;
    itk::ImageRegionConstIterator<FloatImageType> vIt(vesselness, region);
    itk::ImageRegionIterator<FloatImageType> sIt(speedImage, region);
    for (vIt.GoToBegin(), sIt.GoToBegin(); !vIt.IsAtEnd(); ++vIt, ++sIt) {
        sIt.Set(static_cast<float>(epsilon + vIt.Get()));
    }

    // FastMarching from seed point
    using FastMarchingType = itk::FastMarchingImageFilter<FloatImageType, FloatImageType>;
    auto fastMarching = FastMarchingType::New();
    fastMarching->SetInput(speedImage);

    using NodeType = FastMarchingType::NodeType;
    using NodeContainer = FastMarchingType::NodeContainer;

    auto seeds = NodeContainer::New();
    seeds->Initialize();

    FloatImageType::PointType seedPhys;
    seedPhys[0] = seedPoint[0];
    seedPhys[1] = seedPoint[1];
    seedPhys[2] = seedPoint[2];

    FloatImageType::IndexType seedIdx;
    if (!vesselness->TransformPhysicalPointToIndex(seedPhys, seedIdx)) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Seed point is outside image bounds"
        });
    }

    NodeType seedNode;
    seedNode.SetValue(0.0);
    seedNode.SetIndex(seedIdx);
    seeds->InsertElement(0, seedNode);

    fastMarching->SetTrialPoints(seeds);
    fastMarching->SetOutputSize(region.GetSize());
    fastMarching->SetOutputRegion(region);
    fastMarching->SetOutputSpacing(vesselness->GetSpacing());
    fastMarching->SetOutputOrigin(vesselness->GetOrigin());
    fastMarching->SetOutputDirection(vesselness->GetDirection());

    // Set a reasonable stopping value
    fastMarching->SetStoppingValue(1e6);

    try {
        fastMarching->Update();
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            std::string("FastMarching failed: ") + e.GetDescription()
        });
    }

    auto timeMap = fastMarching->GetOutput();

    // Backtrack from endpoint to seed
    auto spacing = vesselness->GetSpacing();
    double minSpacing = std::min({spacing[0], spacing[1], spacing[2]});
    auto pathPoints = impl_->backtrack(timeMap, seedPoint, endPoint, minSpacing * 0.5);

    if (pathPoints.size() < 2) {
        return std::unexpected(CardiacError{
            CardiacError::Code::InternalError,
            "Failed to extract centerline path (too few points)"
        });
    }

    // Build result
    CenterlineResult result;
    result.points = std::move(pathPoints);
    Impl::computeFrenetFrame(result.points);
    result.totalLength = computeLength(result.points);

    LOG_INFO(std::format("Centerline extracted: {} points, {:.1f} mm length",
                         result.points.size(), result.totalLength));

    return result;
}

// =============================================================================
// B-Spline Smoothing
// =============================================================================

double CoronaryCenterlineExtractor::Impl::bsplineBasis(
    int i, int degree, double t, const std::vector<double>& knots)
{
    if (degree == 0) {
        return (t >= knots[i] && t < knots[i + 1]) ? 1.0 : 0.0;
    }

    double denom1 = knots[i + degree] - knots[i];
    double denom2 = knots[i + degree + 1] - knots[i + 1];

    double term1 = 0.0;
    double term2 = 0.0;

    if (denom1 > 1e-10) {
        term1 = (t - knots[i]) / denom1 * bsplineBasis(i, degree - 1, t, knots);
    }
    if (denom2 > 1e-10) {
        term2 = (knots[i + degree + 1] - t) / denom2 * bsplineBasis(i + 1, degree - 1, t, knots);
    }

    return term1 + term2;
}

std::vector<CenterlinePoint>
CoronaryCenterlineExtractor::smoothCenterline(
    const std::vector<CenterlinePoint>& rawPath,
    int controlPointCount) const
{
    if (rawPath.size() < 4) {
        return rawPath;
    }

    int n = static_cast<int>(rawPath.size());
    int numCtrl = std::min(controlPointCount, n);
    if (numCtrl < 4) numCtrl = 4;

    LOG_DEBUG(std::format("Smoothing centerline: {} points → {} control points",
                          n, numCtrl));

    // Compute arc-length parameterization
    std::vector<double> arcLen(n, 0.0);
    for (int i = 1; i < n; ++i) {
        double dx = rawPath[i].position[0] - rawPath[i-1].position[0];
        double dy = rawPath[i].position[1] - rawPath[i-1].position[1];
        double dz = rawPath[i].position[2] - rawPath[i-1].position[2];
        arcLen[i] = arcLen[i-1] + std::sqrt(dx*dx + dy*dy + dz*dz);
    }

    double totalLen = arcLen.back();
    if (totalLen < 1e-10) return rawPath;

    // Normalize to [0, 1]
    for (auto& s : arcLen) {
        s /= totalLen;
    }

    // Sample control points uniformly along arc length
    std::vector<std::array<double, 3>> controlPoints(numCtrl);
    std::vector<double> controlRadii(numCtrl, 0.0);

    for (int c = 0; c < numCtrl; ++c) {
        double targetS = static_cast<double>(c) / (numCtrl - 1);
        // Find bracketing points
        int lo = 0;
        for (int i = 1; i < n; ++i) {
            if (arcLen[i] >= targetS) {
                lo = i - 1;
                break;
            }
            lo = i;
        }
        int hi = std::min(lo + 1, n - 1);

        double segLen = arcLen[hi] - arcLen[lo];
        double t = (segLen > 1e-10) ? (targetS - arcLen[lo]) / segLen : 0.0;

        for (int d = 0; d < 3; ++d) {
            controlPoints[c][d] = rawPath[lo].position[d]
                                + t * (rawPath[hi].position[d] - rawPath[lo].position[d]);
        }
        controlRadii[c] = rawPath[lo].radius + t * (rawPath[hi].radius - rawPath[lo].radius);
    }

    // Generate uniform knot vector for cubic B-spline
    int degree = 3;
    int numKnots = numCtrl + degree + 1;
    std::vector<double> knots(numKnots);
    for (int i = 0; i <= degree; ++i) {
        knots[i] = 0.0;
    }
    for (int i = degree + 1; i < numKnots - degree - 1; ++i) {
        knots[i] = static_cast<double>(i - degree) / (numCtrl - degree);
    }
    for (int i = numKnots - degree - 1; i < numKnots; ++i) {
        knots[i] = 1.0;
    }

    // Evaluate B-spline at output sample points
    int outputPoints = std::max(n, 100);
    std::vector<CenterlinePoint> smoothed(outputPoints);

    for (int s = 0; s < outputPoints; ++s) {
        double t = static_cast<double>(s) / (outputPoints - 1);
        // Clamp to avoid endpoint issues
        t = std::clamp(t, 0.0, 1.0 - 1e-10);

        std::array<double, 3> pos = {0.0, 0.0, 0.0};
        double rad = 0.0;

        for (int c = 0; c < numCtrl; ++c) {
            double basis = Impl::bsplineBasis(c, degree, t, knots);
            for (int d = 0; d < 3; ++d) {
                pos[d] += basis * controlPoints[c][d];
            }
            rad += basis * controlRadii[c];
        }

        smoothed[s].position = pos;
        smoothed[s].radius = rad;
    }

    // Compute Frenet frame for smoothed path
    Impl::computeFrenetFrame(smoothed);

    return smoothed;
}

// =============================================================================
// Frenet Frame Computation
// =============================================================================

void CoronaryCenterlineExtractor::Impl::computeFrenetFrame(
    std::vector<CenterlinePoint>& points)
{
    int n = static_cast<int>(points.size());
    if (n < 2) return;

    for (int i = 0; i < n; ++i) {
        std::array<double, 3> tangent = {0.0, 0.0, 0.0};

        if (i == 0) {
            for (int d = 0; d < 3; ++d) {
                tangent[d] = points[1].position[d] - points[0].position[d];
            }
        } else if (i == n - 1) {
            for (int d = 0; d < 3; ++d) {
                tangent[d] = points[n-1].position[d] - points[n-2].position[d];
            }
        } else {
            for (int d = 0; d < 3; ++d) {
                tangent[d] = points[i+1].position[d] - points[i-1].position[d];
            }
        }

        // Normalize tangent
        double mag = std::sqrt(tangent[0]*tangent[0]
                             + tangent[1]*tangent[1]
                             + tangent[2]*tangent[2]);
        if (mag > 1e-10) {
            for (auto& t : tangent) t /= mag;
        }
        points[i].tangent = tangent;

        // Compute normal: find vector most perpendicular to tangent
        // Use the minimum component trick
        std::array<double, 3> ref = {0.0, 0.0, 0.0};
        int minIdx = 0;
        double minVal = std::abs(tangent[0]);
        for (int d = 1; d < 3; ++d) {
            if (std::abs(tangent[d]) < minVal) {
                minVal = std::abs(tangent[d]);
                minIdx = d;
            }
        }
        ref[minIdx] = 1.0;

        // Normal = ref - (ref · tangent) * tangent (Gram-Schmidt)
        double dot = ref[0]*tangent[0] + ref[1]*tangent[1] + ref[2]*tangent[2];
        std::array<double, 3> normal;
        for (int d = 0; d < 3; ++d) {
            normal[d] = ref[d] - dot * tangent[d];
        }

        // Normalize
        double nmag = std::sqrt(normal[0]*normal[0]
                              + normal[1]*normal[1]
                              + normal[2]*normal[2]);
        if (nmag > 1e-10) {
            for (auto& nn : normal) nn /= nmag;
        }
        points[i].normal = normal;
    }
}

// =============================================================================
// Radius Estimation
// =============================================================================

void CoronaryCenterlineExtractor::estimateRadii(
    std::vector<CenterlinePoint>& points,
    ImageType::Pointer image) const
{
    if (!image || points.empty()) return;

    auto region = image->GetLargestPossibleRegion();

    for (auto& pt : points) {
        // Cast rays in 4 perpendicular directions (normal, binormal, -normal, -binormal)
        // Binormal = tangent × normal
        std::array<double, 3> binormal;
        binormal[0] = pt.tangent[1]*pt.normal[2] - pt.tangent[2]*pt.normal[1];
        binormal[1] = pt.tangent[2]*pt.normal[0] - pt.tangent[0]*pt.normal[2];
        binormal[2] = pt.tangent[0]*pt.normal[1] - pt.tangent[1]*pt.normal[0];

        std::array<std::array<double, 3>, 4> dirs = {{
            pt.normal,
            {{ -pt.normal[0], -pt.normal[1], -pt.normal[2] }},
            binormal,
            {{ -binormal[0], -binormal[1], -binormal[2] }}
        }};

        // Get center HU value for reference
        ImageType::PointType centerPhys;
        centerPhys[0] = pt.position[0];
        centerPhys[1] = pt.position[1];
        centerPhys[2] = pt.position[2];

        ImageType::IndexType centerIdx;
        if (!image->TransformPhysicalPointToIndex(centerPhys, centerIdx) ||
            !region.IsInside(centerIdx)) {
            pt.radius = 1.0;  // Default
            continue;
        }

        short centerHU = image->GetPixel(centerIdx);
        double halfMax = centerHU / 2.0;

        double totalRadius = 0.0;
        int validDirs = 0;

        for (const auto& dir : dirs) {
            double stepSize = 0.2;  // mm
            double maxDist = 10.0;  // mm max search radius

            for (double dist = stepSize; dist <= maxDist; dist += stepSize) {
                ImageType::PointType samplePhys;
                samplePhys[0] = pt.position[0] + dist * dir[0];
                samplePhys[1] = pt.position[1] + dist * dir[1];
                samplePhys[2] = pt.position[2] + dist * dir[2];

                ImageType::IndexType sampleIdx;
                if (!image->TransformPhysicalPointToIndex(samplePhys, sampleIdx) ||
                    !region.IsInside(sampleIdx)) {
                    totalRadius += dist;
                    ++validDirs;
                    break;
                }

                short hu = image->GetPixel(sampleIdx);
                if (hu < halfMax) {
                    totalRadius += dist;
                    ++validDirs;
                    break;
                }
            }
        }

        pt.radius = (validDirs > 0) ? (totalRadius / validDirs) : 1.0;
    }
}

// =============================================================================
// Stenosis Measurement
// =============================================================================

void CoronaryCenterlineExtractor::measureStenosis(
    CenterlineResult& result,
    ImageType::Pointer image) const
{
    if (result.points.empty() || !image) return;

    // Estimate radii if not yet done
    bool hasRadii = false;
    for (const auto& pt : result.points) {
        if (pt.radius > 0.0) {
            hasRadii = true;
            break;
        }
    }
    if (!hasRadii) {
        estimateRadii(result.points, image);
    }

    // Find minimum and reference diameters
    double minDiameter = std::numeric_limits<double>::max();
    double refDiameter = 0.0;

    // Reference diameter: average of proximal 20% of points
    int proximalEnd = std::max(1, static_cast<int>(result.points.size() * 0.2));
    double refSum = 0.0;
    int refCount = 0;

    for (int i = 0; i < proximalEnd; ++i) {
        double d = result.points[i].radius * 2.0;
        refSum += d;
        ++refCount;
    }
    refDiameter = (refCount > 0) ? (refSum / refCount) : 0.0;

    // Find minimum diameter
    for (const auto& pt : result.points) {
        double d = pt.radius * 2.0;
        if (d < minDiameter && d > 0.0) {
            minDiameter = d;
        }
    }

    result.minLumenDiameter = (minDiameter < std::numeric_limits<double>::max())
                            ? minDiameter : 0.0;
    result.referenceDiameter = refDiameter;

    if (refDiameter > 0.0) {
        result.stenosisPercent = (1.0 - result.minLumenDiameter / refDiameter) * 100.0;
        result.stenosisPercent = std::clamp(result.stenosisPercent, 0.0, 100.0);
    }

    LOG_INFO(std::format("Stenosis: min={:.2f}mm, ref={:.2f}mm, {}%",
                         result.minLumenDiameter, result.referenceDiameter,
                         result.stenosisPercent));
}

// =============================================================================
// Utility
// =============================================================================

double CoronaryCenterlineExtractor::computeLength(
    const std::vector<CenterlinePoint>& points)
{
    double length = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        double dx = points[i].position[0] - points[i-1].position[0];
        double dy = points[i].position[1] - points[i-1].position[1];
        double dz = points[i].position[2] - points[i-1].position[2];
        length += std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    return length;
}

}  // namespace dicom_viewer::services

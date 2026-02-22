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

#include "services/measurement/shape_analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>

#include <kcenon/common/logging/log_macros.h>

#include <itkImageRegionConstIterator.h>
#include <itkLabelStatisticsImageFilter.h>

namespace dicom_viewer::services {

// =============================================================================
// ShapeAnalysisResult implementation
// =============================================================================

std::string ShapeAnalysisResult::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);

    oss << "Shape Analysis";
    if (!labelName.empty()) {
        oss << " (" << labelName << ")";
    }
    oss << ":\n";

    oss << "  Label ID:    " << static_cast<int>(labelId) << "\n";
    oss << "  Voxel Count: " << voxelCount << "\n";
    oss << "  Volume:      " << volumeMm3 << " mm^3\n";

    if (sphericity.has_value()) {
        oss << "  Sphericity:  " << sphericity.value() << "\n";
    }

    oss << "\n  Shape Descriptors:\n";

    if (elongation.has_value()) {
        oss << "    Elongation:  " << elongation.value() << "\n";
    }
    if (flatness.has_value()) {
        oss << "    Flatness:    " << flatness.value() << "\n";
    }
    if (compactness.has_value()) {
        oss << "    Compactness: " << compactness.value() << "\n";
    }
    if (roundness.has_value()) {
        oss << "    Roundness:   " << roundness.value() << "\n";
    }

    if (principalAxes.has_value()) {
        const auto& axes = principalAxes.value();
        oss << "\n  Principal Axes:\n";
        oss << "    Centroid: (" << axes.centroid[0] << ", "
            << axes.centroid[1] << ", " << axes.centroid[2] << ") mm\n";
        oss << "    Axes Lengths: " << axes.axesLengths[0] << " x "
            << axes.axesLengths[1] << " x " << axes.axesLengths[2] << " mm\n";
        oss << "    Eigenvalues:  " << axes.eigenvalues[0] << ", "
            << axes.eigenvalues[1] << ", " << axes.eigenvalues[2] << "\n";
    }

    if (axisAlignedBoundingBox.has_value()) {
        const auto& aabb = axisAlignedBoundingBox.value();
        oss << "\n  AABB: " << aabb.dimensions[0] << " x "
            << aabb.dimensions[1] << " x " << aabb.dimensions[2] << " mm\n";
    }

    if (orientedBoundingBox.has_value()) {
        const auto& obb = orientedBoundingBox.value();
        oss << "  OBB:  " << obb.dimensions[0] << " x "
            << obb.dimensions[1] << " x " << obb.dimensions[2] << " mm\n";
    }

    return oss.str();
}

std::vector<std::string> ShapeAnalysisResult::getCsvHeader() {
    return {
        "LabelID", "LabelName", "VoxelCount", "VolumeMm3",
        "SurfaceAreaMm2", "Sphericity",
        "Elongation", "Flatness", "Compactness", "Roundness",
        "CentroidX", "CentroidY", "CentroidZ",
        "MajorAxisLength", "MiddleAxisLength", "MinorAxisLength",
        "Eigenvalue1", "Eigenvalue2", "Eigenvalue3",
        "AABB_X", "AABB_Y", "AABB_Z",
        "OBB_Major", "OBB_Middle", "OBB_Minor"
    };
}

std::vector<std::string> ShapeAnalysisResult::getCsvRow() const {
    auto format = [](double val) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << val;
        return oss.str();
    };

    auto formatOpt = [&format](const std::optional<double>& val) {
        return val.has_value() ? format(val.value()) : "";
    };

    std::vector<std::string> row;
    row.push_back(std::to_string(labelId));
    row.push_back(labelName);
    row.push_back(std::to_string(voxelCount));
    row.push_back(format(volumeMm3));
    row.push_back(formatOpt(surfaceAreaMm2));
    row.push_back(formatOpt(sphericity));
    row.push_back(formatOpt(elongation));
    row.push_back(formatOpt(flatness));
    row.push_back(formatOpt(compactness));
    row.push_back(formatOpt(roundness));

    if (principalAxes.has_value()) {
        const auto& axes = principalAxes.value();
        row.push_back(format(axes.centroid[0]));
        row.push_back(format(axes.centroid[1]));
        row.push_back(format(axes.centroid[2]));
        row.push_back(format(axes.axesLengths[0]));
        row.push_back(format(axes.axesLengths[1]));
        row.push_back(format(axes.axesLengths[2]));
        row.push_back(format(axes.eigenvalues[0]));
        row.push_back(format(axes.eigenvalues[1]));
        row.push_back(format(axes.eigenvalues[2]));
    } else {
        for (int i = 0; i < 9; ++i) {
            row.emplace_back("");
        }
    }

    if (axisAlignedBoundingBox.has_value()) {
        const auto& aabb = axisAlignedBoundingBox.value();
        row.push_back(format(aabb.dimensions[0]));
        row.push_back(format(aabb.dimensions[1]));
        row.push_back(format(aabb.dimensions[2]));
    } else {
        row.emplace_back("");
        row.emplace_back("");
        row.emplace_back("");
    }

    if (orientedBoundingBox.has_value()) {
        const auto& obb = orientedBoundingBox.value();
        row.push_back(format(obb.dimensions[0]));
        row.push_back(format(obb.dimensions[1]));
        row.push_back(format(obb.dimensions[2]));
    } else {
        row.emplace_back("");
        row.emplace_back("");
        row.emplace_back("");
    }

    return row;
}

// =============================================================================
// ShapeAnalyzer::Impl
// =============================================================================

class ShapeAnalyzer::Impl {
public:
    ProgressCallback progressCallback;

    // Find all unique label IDs in the label map
    std::set<uint8_t> findAllLabels(LabelMapType::Pointer labelMap) {
        std::set<uint8_t> labels;

        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(labelMap, labelMap->GetLargestPossibleRegion());

        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            uint8_t val = it.Get();
            if (val > 0) {
                labels.insert(val);
            }
        }

        return labels;
    }

    // Collect voxel coordinates for a specific label
    std::vector<Vector3D> collectVoxelCoordinates(
        LabelMapType::Pointer labelMap,
        uint8_t labelId,
        const SpacingType& spacing
    ) {
        std::vector<Vector3D> coordinates;
        auto origin = labelMap->GetOrigin();

        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(labelMap, labelMap->GetLargestPossibleRegion());

        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == labelId) {
                auto idx = it.GetIndex();
                Vector3D worldCoord = {
                    origin[0] + static_cast<double>(idx[0]) * spacing[0],
                    origin[1] + static_cast<double>(idx[1]) * spacing[1],
                    origin[2] + static_cast<double>(idx[2]) * spacing[2]
                };
                coordinates.push_back(worldCoord);
            }
        }

        return coordinates;
    }

    // Calculate centroid from voxel coordinates
    Vector3D calculateCentroid(const std::vector<Vector3D>& coords) {
        if (coords.empty()) {
            return {0.0, 0.0, 0.0};
        }

        Vector3D centroid = {0.0, 0.0, 0.0};
        for (const auto& c : coords) {
            centroid[0] += c[0];
            centroid[1] += c[1];
            centroid[2] += c[2];
        }

        double n = static_cast<double>(coords.size());
        centroid[0] /= n;
        centroid[1] /= n;
        centroid[2] /= n;

        return centroid;
    }

    // Perform PCA to get eigenvalues and eigenvectors
    // Uses Jacobi iteration for 3x3 symmetric matrix
    bool performPCA(
        const std::vector<Vector3D>& coords,
        const Vector3D& centroid,
        std::array<double, 3>& eigenvalues,
        std::array<Vector3D, 3>& eigenvectors
    ) {
        if (coords.size() < 3) {
            return false;
        }

        // Build covariance matrix
        double cov[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};

        for (const auto& c : coords) {
            double dx = c[0] - centroid[0];
            double dy = c[1] - centroid[1];
            double dz = c[2] - centroid[2];

            cov[0][0] += dx * dx;
            cov[0][1] += dx * dy;
            cov[0][2] += dx * dz;
            cov[1][1] += dy * dy;
            cov[1][2] += dy * dz;
            cov[2][2] += dz * dz;
        }

        double n = static_cast<double>(coords.size());
        for (int i = 0; i < 3; ++i) {
            for (int j = i; j < 3; ++j) {
                cov[i][j] /= n;
                if (i != j) {
                    cov[j][i] = cov[i][j];
                }
            }
        }

        // Jacobi eigenvalue algorithm for 3x3 symmetric matrix
        double v[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};  // Eigenvectors
        double a[3][3];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                a[i][j] = cov[i][j];
            }
        }

        const int maxIterations = 50;
        const double tolerance = 1e-10;

        for (int iter = 0; iter < maxIterations; ++iter) {
            // Find largest off-diagonal element
            int p = 0, q = 1;
            double maxVal = std::abs(a[0][1]);
            if (std::abs(a[0][2]) > maxVal) { p = 0; q = 2; maxVal = std::abs(a[0][2]); }
            if (std::abs(a[1][2]) > maxVal) { p = 1; q = 2; maxVal = std::abs(a[1][2]); }

            if (maxVal < tolerance) {
                break;
            }

            // Calculate rotation angle
            double theta = 0.0;
            if (std::abs(a[p][p] - a[q][q]) < tolerance) {
                theta = M_PI / 4.0;
            } else {
                theta = 0.5 * std::atan2(2.0 * a[p][q], a[p][p] - a[q][q]);
            }

            double c = std::cos(theta);
            double s = std::sin(theta);

            // Apply rotation
            double app = a[p][p];
            double aqq = a[q][q];
            double apq = a[p][q];

            a[p][p] = c * c * app + s * s * aqq - 2 * s * c * apq;
            a[q][q] = s * s * app + c * c * aqq + 2 * s * c * apq;
            a[p][q] = a[q][p] = 0.0;

            for (int i = 0; i < 3; ++i) {
                if (i != p && i != q) {
                    double aip = a[i][p];
                    double aiq = a[i][q];
                    a[i][p] = a[p][i] = c * aip - s * aiq;
                    a[i][q] = a[q][i] = s * aip + c * aiq;
                }
            }

            // Update eigenvectors
            for (int i = 0; i < 3; ++i) {
                double vip = v[i][p];
                double viq = v[i][q];
                v[i][p] = c * vip - s * viq;
                v[i][q] = s * vip + c * viq;
            }
        }

        // Extract eigenvalues
        std::array<std::pair<double, int>, 3> evalIdx = {{
            {a[0][0], 0},
            {a[1][1], 1},
            {a[2][2], 2}
        }};

        // Sort in descending order
        std::sort(evalIdx.begin(), evalIdx.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        for (int i = 0; i < 3; ++i) {
            eigenvalues[i] = std::max(evalIdx[i].first, 0.0);
            int idx = evalIdx[i].second;
            eigenvectors[i] = {v[0][idx], v[1][idx], v[2][idx]};
        }

        return true;
    }

    // Calculate axis-aligned bounding box
    BoundingBox calculateAABB(
        LabelMapType::Pointer labelMap,
        uint8_t labelId,
        const SpacingType& spacing
    ) {
        auto region = labelMap->GetLargestPossibleRegion();
        auto size = region.GetSize();
        auto origin = labelMap->GetOrigin();

        int minX = static_cast<int>(size[0]), maxX = -1;
        int minY = static_cast<int>(size[1]), maxY = -1;
        int minZ = static_cast<int>(size[2]), maxZ = -1;

        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(labelMap, region);

        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == labelId) {
                auto idx = it.GetIndex();
                minX = std::min(minX, static_cast<int>(idx[0]));
                maxX = std::max(maxX, static_cast<int>(idx[0]));
                minY = std::min(minY, static_cast<int>(idx[1]));
                maxY = std::max(maxY, static_cast<int>(idx[1]));
                minZ = std::min(minZ, static_cast<int>(idx[2]));
                maxZ = std::max(maxZ, static_cast<int>(idx[2]));
            }
        }

        BoundingBox aabb;
        if (maxX >= minX && maxY >= minY && maxZ >= minZ) {
            aabb.dimensions[0] = static_cast<double>(maxX - minX + 1) * spacing[0];
            aabb.dimensions[1] = static_cast<double>(maxY - minY + 1) * spacing[1];
            aabb.dimensions[2] = static_cast<double>(maxZ - minZ + 1) * spacing[2];

            aabb.center[0] = origin[0] + (minX + maxX) * 0.5 * spacing[0];
            aabb.center[1] = origin[1] + (minY + maxY) * 0.5 * spacing[1];
            aabb.center[2] = origin[2] + (minZ + maxZ) * 0.5 * spacing[2];

            aabb.volume = aabb.dimensions[0] * aabb.dimensions[1] * aabb.dimensions[2];
        }

        return aabb;
    }

    // Calculate oriented bounding box using principal axes
    BoundingBox calculateOBB(
        const std::vector<Vector3D>& coords,
        const PrincipalAxes& axes
    ) {
        BoundingBox obb;
        if (coords.empty()) {
            return obb;
        }

        // Project all points onto principal axes
        double minProj[3] = {std::numeric_limits<double>::max(),
                            std::numeric_limits<double>::max(),
                            std::numeric_limits<double>::max()};
        double maxProj[3] = {std::numeric_limits<double>::lowest(),
                            std::numeric_limits<double>::lowest(),
                            std::numeric_limits<double>::lowest()};

        for (const auto& c : coords) {
            double dx = c[0] - axes.centroid[0];
            double dy = c[1] - axes.centroid[1];
            double dz = c[2] - axes.centroid[2];

            for (int i = 0; i < 3; ++i) {
                double proj = dx * axes.eigenvectors[i][0] +
                             dy * axes.eigenvectors[i][1] +
                             dz * axes.eigenvectors[i][2];
                minProj[i] = std::min(minProj[i], proj);
                maxProj[i] = std::max(maxProj[i], proj);
            }
        }

        // OBB dimensions
        for (int i = 0; i < 3; ++i) {
            obb.dimensions[i] = maxProj[i] - minProj[i];
        }

        // OBB center (in world coordinates)
        double centerOffset[3] = {
            (maxProj[0] + minProj[0]) * 0.5,
            (maxProj[1] + minProj[1]) * 0.5,
            (maxProj[2] + minProj[2]) * 0.5
        };

        obb.center = axes.centroid;
        for (int i = 0; i < 3; ++i) {
            obb.center[0] += centerOffset[i] * axes.eigenvectors[i][0];
            obb.center[1] += centerOffset[i] * axes.eigenvectors[i][1];
            obb.center[2] += centerOffset[i] * axes.eigenvectors[i][2];
        }

        obb.volume = obb.dimensions[0] * obb.dimensions[1] * obb.dimensions[2];
        obb.orientation = axes.eigenvectors;

        return obb;
    }

    // Calculate elongation: 1 - (λ₂/λ₁)
    double calculateElongation(const std::array<double, 3>& eigenvalues) {
        if (eigenvalues[0] <= 0.0) {
            return 0.0;
        }
        return 1.0 - (eigenvalues[1] / eigenvalues[0]);
    }

    // Calculate flatness: 1 - (λ₃/λ₂)
    double calculateFlatness(const std::array<double, 3>& eigenvalues) {
        if (eigenvalues[1] <= 0.0) {
            return 0.0;
        }
        return 1.0 - (eigenvalues[2] / eigenvalues[1]);
    }

    // Calculate compactness: volume / bounding box volume
    double calculateCompactness(double volume, const BoundingBox& bbox) {
        if (bbox.volume <= 0.0) {
            return 0.0;
        }
        return volume / bbox.volume;
    }

    // Calculate roundness: 4V / (π × max_axis³)
    double calculateRoundness(double volume, const std::array<double, 3>& axesLengths) {
        double maxAxis = axesLengths[0];  // Already sorted descending
        if (maxAxis <= 0.0) {
            return 0.0;
        }
        return (4.0 * volume) / (M_PI * maxAxis * maxAxis * maxAxis);
    }

    // Calculate axes lengths from eigenvalues
    // Standard deviation along each axis = sqrt(eigenvalue)
    // Full length approximated as 4 * standard deviation (covers ~95% of distribution)
    std::array<double, 3> calculateAxesLengths(const std::array<double, 3>& eigenvalues) {
        return {
            4.0 * std::sqrt(std::max(eigenvalues[0], 0.0)),
            4.0 * std::sqrt(std::max(eigenvalues[1], 0.0)),
            4.0 * std::sqrt(std::max(eigenvalues[2], 0.0))
        };
    }
};

// =============================================================================
// ShapeAnalyzer implementation
// =============================================================================

ShapeAnalyzer::ShapeAnalyzer()
    : impl_(std::make_unique<Impl>())
{
}

ShapeAnalyzer::~ShapeAnalyzer() = default;

ShapeAnalyzer::ShapeAnalyzer(ShapeAnalyzer&&) noexcept = default;
ShapeAnalyzer& ShapeAnalyzer::operator=(ShapeAnalyzer&&) noexcept = default;

void ShapeAnalyzer::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<ShapeAnalysisResult, ShapeAnalysisError>
ShapeAnalyzer::analyze(LabelMapType::Pointer labelMap,
                        uint8_t labelId,
                        const SpacingType& spacing,
                        const ShapeAnalysisOptions& options) {
    LOG_DEBUG(std::format("Analyzing shape for label {}", labelId));

    if (!labelMap) {
        LOG_ERROR("Label map is null");
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InvalidLabelMap,
            "Label map is null"
        });
    }

    if (labelId == 0) {
        LOG_ERROR("Label ID 0 is reserved");
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::LabelNotFound,
            "Label ID 0 is reserved for background"
        });
    }

    if (spacing[0] <= 0 || spacing[1] <= 0 || spacing[2] <= 0) {
        LOG_ERROR("Invalid spacing values");
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InvalidSpacing,
            "Spacing values must be positive"
        });
    }

    // Collect voxel coordinates
    auto coords = impl_->collectVoxelCoordinates(labelMap, labelId, spacing);
    if (coords.empty()) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::LabelNotFound,
            "No voxels found with label ID " + std::to_string(labelId)
        });
    }

    if (coords.size() < 3) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InsufficientVoxels,
            "At least 3 voxels required for shape analysis"
        });
    }

    ShapeAnalysisResult result;
    result.labelId = labelId;
    result.voxelCount = static_cast<int64_t>(coords.size());

    // Calculate volume
    double voxelVolume = spacing[0] * spacing[1] * spacing[2];
    result.volumeMm3 = static_cast<double>(coords.size()) * voxelVolume;

    // Perform PCA if any dependent metric is requested
    PrincipalAxes axes;
    bool pcaNeeded = options.computeElongation || options.computeFlatness ||
                     options.computeRoundness || options.computePrincipalAxes ||
                     options.computeOrientedBoundingBox;

    if (pcaNeeded) {
        axes.centroid = impl_->calculateCentroid(coords);

        bool pcaSuccess = impl_->performPCA(coords, axes.centroid,
                                            axes.eigenvalues, axes.eigenvectors);
        if (!pcaSuccess) {
            LOG_WARNING(std::format("PCA failed for label {}", labelId));
        } else {
            axes.axesLengths = impl_->calculateAxesLengths(axes.eigenvalues);

            if (options.computePrincipalAxes) {
                result.principalAxes = axes;
            }

            if (options.computeElongation) {
                result.elongation = impl_->calculateElongation(axes.eigenvalues);
            }

            if (options.computeFlatness) {
                result.flatness = impl_->calculateFlatness(axes.eigenvalues);
            }

            if (options.computeRoundness) {
                result.roundness = impl_->calculateRoundness(result.volumeMm3, axes.axesLengths);
            }

            if (options.computeOrientedBoundingBox) {
                result.orientedBoundingBox = impl_->calculateOBB(coords, axes);
            }
        }
    }

    // Calculate AABB
    if (options.computeAxisAlignedBoundingBox) {
        result.axisAlignedBoundingBox = impl_->calculateAABB(labelMap, labelId, spacing);
    }

    // Calculate compactness using OBB if available, otherwise AABB
    if (options.computeCompactness) {
        if (result.orientedBoundingBox.has_value()) {
            result.compactness = impl_->calculateCompactness(
                result.volumeMm3, result.orientedBoundingBox.value());
        } else if (result.axisAlignedBoundingBox.has_value()) {
            result.compactness = impl_->calculateCompactness(
                result.volumeMm3, result.axisAlignedBoundingBox.value());
        }
    }

    return result;
}

std::vector<std::expected<ShapeAnalysisResult, ShapeAnalysisError>>
ShapeAnalyzer::analyzeAll(LabelMapType::Pointer labelMap,
                          const SpacingType& spacing,
                          const ShapeAnalysisOptions& options) {
    std::vector<std::expected<ShapeAnalysisResult, ShapeAnalysisError>> results;

    if (!labelMap) {
        results.push_back(std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InvalidLabelMap,
            "Label map is null"
        }));
        return results;
    }

    auto labels = impl_->findAllLabels(labelMap);
    if (labels.empty()) {
        return results;
    }

    results.reserve(labels.size());

    size_t processed = 0;
    for (uint8_t labelId : labels) {
        results.push_back(analyze(labelMap, labelId, spacing, options));

        if (impl_->progressCallback) {
            impl_->progressCallback(
                static_cast<double>(++processed) / static_cast<double>(labels.size())
            );
        }
    }

    return results;
}

std::expected<PrincipalAxes, ShapeAnalysisError>
ShapeAnalyzer::computePrincipalAxes(LabelMapType::Pointer labelMap,
                                     uint8_t labelId,
                                     const SpacingType& spacing) {
    if (!labelMap) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InvalidLabelMap,
            "Label map is null"
        });
    }

    if (labelId == 0) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::LabelNotFound,
            "Label ID 0 is reserved for background"
        });
    }

    if (spacing[0] <= 0 || spacing[1] <= 0 || spacing[2] <= 0) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InvalidSpacing,
            "Spacing values must be positive"
        });
    }

    auto coords = impl_->collectVoxelCoordinates(labelMap, labelId, spacing);
    if (coords.empty()) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::LabelNotFound,
            "No voxels found with label ID " + std::to_string(labelId)
        });
    }

    if (coords.size() < 3) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InsufficientVoxels,
            "At least 3 voxels required for PCA"
        });
    }

    PrincipalAxes axes;
    axes.centroid = impl_->calculateCentroid(coords);

    bool success = impl_->performPCA(coords, axes.centroid,
                                     axes.eigenvalues, axes.eigenvectors);
    if (!success) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::PCAFailed,
            "Failed to compute eigenvalues"
        });
    }

    axes.axesLengths = impl_->calculateAxesLengths(axes.eigenvalues);
    return axes;
}

std::expected<BoundingBox, ShapeAnalysisError>
ShapeAnalyzer::computeOrientedBoundingBox(LabelMapType::Pointer labelMap,
                                          uint8_t labelId,
                                          const SpacingType& spacing) {
    auto axesResult = computePrincipalAxes(labelMap, labelId, spacing);
    if (!axesResult.has_value()) {
        return std::unexpected(axesResult.error());
    }

    auto coords = impl_->collectVoxelCoordinates(labelMap, labelId, spacing);
    return impl_->calculateOBB(coords, axesResult.value());
}

std::expected<void, ShapeAnalysisError>
ShapeAnalyzer::exportToCsv(const std::vector<ShapeAnalysisResult>& results,
                            const std::filesystem::path& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(ShapeAnalysisError{
            ShapeAnalysisError::Code::InternalError,
            "Failed to open file: " + filePath.string()
        });
    }

    // Write header
    auto header = ShapeAnalysisResult::getCsvHeader();
    for (size_t i = 0; i < header.size(); ++i) {
        file << header[i];
        if (i < header.size() - 1) file << ",";
    }
    file << "\n";

    // Write data rows
    for (const auto& result : results) {
        auto row = result.getCsvRow();
        for (size_t i = 0; i < row.size(); ++i) {
            file << row[i];
            if (i < row.size() - 1) file << ",";
        }
        file << "\n";
    }

    return {};
}

}  // namespace dicom_viewer::services

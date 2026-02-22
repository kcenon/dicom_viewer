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

/**
 * @file shape_analyzer.hpp
 * @brief Shape morphology analysis for segmented regions
 * @details Analyzes geometric properties of segmented label map regions
 *          including surface area, volume, sphericity, elongation,
 *          and center of mass. Operates on ITK binary label maps
 *          with proper physical spacing consideration.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <itkImage.h>
#include <itkSmartPointer.h>

namespace dicom_viewer::services {

/**
 * @brief 3D vector type for shape analysis
 */
using Vector3D = std::array<double, 3>;

/**
 * @brief Shape analysis options controlling which metrics to compute
 *
 * @trace SRS-FR-040
 */
struct ShapeAnalysisOptions {
    /// Compute elongation from PCA eigenvalue ratios
    bool computeElongation = true;

    /// Compute flatness from PCA eigenvalue ratios
    bool computeFlatness = true;

    /// Compute compactness (volume / bounding box volume ratio)
    bool computeCompactness = true;

    /// Compute roundness (deviation from spherical shape)
    bool computeRoundness = true;

    /// Compute principal axes lengths and orientations via PCA
    bool computePrincipalAxes = true;

    /// Compute axis-aligned bounding box (AABB)
    bool computeAxisAlignedBoundingBox = true;

    /// Compute oriented bounding box aligned to principal axes (OBB)
    bool computeOrientedBoundingBox = true;
};

/**
 * @brief Principal axes information from PCA analysis
 *
 * @trace SRS-FR-041
 */
struct PrincipalAxes {
    /// Center of mass in world coordinates (mm)
    Vector3D centroid = {0.0, 0.0, 0.0};

    /// Eigenvalues [major, middle, minor] representing variance along each axis
    std::array<double, 3> eigenvalues = {0.0, 0.0, 0.0};

    /// Eigenvectors defining the principal axis orientations
    std::array<Vector3D, 3> eigenvectors = {
        Vector3D{1.0, 0.0, 0.0},
        Vector3D{0.0, 1.0, 0.0},
        Vector3D{0.0, 0.0, 1.0}
    };

    /// Lengths of principal axes in mm [major, middle, minor]
    std::array<double, 3> axesLengths = {0.0, 0.0, 0.0};
};

/**
 * @brief Bounding box information
 *
 * @trace SRS-FR-042
 */
struct BoundingBox {
    /// Center of the bounding box in world coordinates (mm)
    Vector3D center = {0.0, 0.0, 0.0};

    /// Dimensions [x, y, z] for AABB or [major, middle, minor] for OBB in mm
    std::array<double, 3> dimensions = {0.0, 0.0, 0.0};

    /// Volume of the bounding box in mm^3
    double volume = 0.0;

    /// Orientation vectors (only for OBB, empty for AABB)
    std::optional<std::array<Vector3D, 3>> orientation;
};

/**
 * @brief Error information for shape analysis operations
 *
 * @trace SRS-FR-040
 */
struct ShapeAnalysisError {
    enum class Code {
        Success,
        InvalidLabelMap,
        InvalidSpacing,
        LabelNotFound,
        InsufficientVoxels,
        PCAFailed,
        InternalError
    };

    Code code = Code::Success;
    std::string message;

    [[nodiscard]] bool isSuccess() const noexcept {
        return code == Code::Success;
    }

    [[nodiscard]] std::string toString() const {
        switch (code) {
            case Code::Success: return "Success";
            case Code::InvalidLabelMap: return "Invalid label map: " + message;
            case Code::InvalidSpacing: return "Invalid spacing: " + message;
            case Code::LabelNotFound: return "Label not found: " + message;
            case Code::InsufficientVoxels: return "Insufficient voxels: " + message;
            case Code::PCAFailed: return "PCA failed: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Complete shape analysis result for a segmented region
 *
 * Contains shape descriptors (elongation, flatness, compactness, roundness),
 * principal axes information, and bounding box data.
 *
 * @trace SRS-FR-040 ~ SRS-FR-044
 */
struct ShapeAnalysisResult {
    /// Label ID (1-255)
    uint8_t labelId = 0;

    /// Label name for display
    std::string labelName;

    /// Number of voxels in the segmented region
    int64_t voxelCount = 0;

    /// Volume in cubic millimeters
    double volumeMm3 = 0.0;

    /// Surface area in square millimeters (optional, requires mesh generation)
    std::optional<double> surfaceAreaMm2;

    /// Sphericity: ratio of surface area of equivalent sphere to actual surface area
    /// (1.0 = perfect sphere, <1.0 = irregular shape)
    std::optional<double> sphericity;

    // -------------------------------------------------------------------------
    // Shape Descriptors
    // -------------------------------------------------------------------------

    /// Elongation: 1 - (λ₂/λ₁), range [0,1], 0 = spherical, 1 = linear
    std::optional<double> elongation;

    /// Flatness: 1 - (λ₃/λ₂), range [0,1], 0 = cylindrical, 1 = flat/disc
    std::optional<double> flatness;

    /// Compactness: volume / bounding box volume, range [0,1]
    std::optional<double> compactness;

    /// Roundness: 4V / (π × max_axis³), range [0,1]
    std::optional<double> roundness;

    // -------------------------------------------------------------------------
    // Principal Axes
    // -------------------------------------------------------------------------

    /// Principal axes information from PCA
    std::optional<PrincipalAxes> principalAxes;

    // -------------------------------------------------------------------------
    // Bounding Boxes
    // -------------------------------------------------------------------------

    /// Axis-aligned bounding box (AABB)
    std::optional<BoundingBox> axisAlignedBoundingBox;

    /// Oriented bounding box aligned to principal axes (OBB)
    std::optional<BoundingBox> orientedBoundingBox;

    /**
     * @brief Convert result to formatted string
     * @return Formatted shape analysis string
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Get header row for CSV export
     * @return Vector of column names
     */
    [[nodiscard]] static std::vector<std::string> getCsvHeader();

    /**
     * @brief Get data row for CSV export
     * @return Vector of values as strings
     */
    [[nodiscard]] std::vector<std::string> getCsvRow() const;
};

/**
 * @brief Analyzer for advanced 3D shape metrics of segmented regions
 *
 * Provides Principal Component Analysis (PCA) based shape descriptors
 * including elongation, flatness, compactness, and roundness.
 * These metrics are valuable for:
 * - Tumor characterization (sphericity indicates malignancy)
 * - Longitudinal monitoring (shape changes over time)
 * - Research and clinical trials
 *
 * @example
 * @code
 * ShapeAnalyzer analyzer;
 *
 * // Analyze single label
 * auto result = analyzer.analyze(labelMap, labelId, spacing);
 * if (result) {
 *     std::cout << "Elongation: " << result->elongation.value() << std::endl;
 *     std::cout << "Flatness: " << result->flatness.value() << std::endl;
 * }
 *
 * // Get principal axes only (lightweight)
 * auto axes = analyzer.computePrincipalAxes(labelMap, labelId, spacing);
 * if (axes) {
 *     std::cout << "Major axis length: " << axes->axesLengths[0] << " mm" << std::endl;
 * }
 * @endcode
 *
 * @trace SRS-FR-040 ~ SRS-FR-044
 */
class ShapeAnalyzer {
public:
    /// Label map type for segmentation
    using LabelMapType = itk::Image<uint8_t, 3>;

    /// Spacing type [x, y, z] in mm
    using SpacingType = std::array<double, 3>;

    /// Callback for progress updates
    using ProgressCallback = std::function<void(double progress)>;

    ShapeAnalyzer();
    ~ShapeAnalyzer();

    // Non-copyable, movable
    ShapeAnalyzer(const ShapeAnalyzer&) = delete;
    ShapeAnalyzer& operator=(const ShapeAnalyzer&) = delete;
    ShapeAnalyzer(ShapeAnalyzer&&) noexcept;
    ShapeAnalyzer& operator=(ShapeAnalyzer&&) noexcept;

    /**
     * @brief Set progress callback for long operations
     * @param callback Callback function receiving progress (0.0 to 1.0)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Perform full shape analysis for a single segmentation label
     *
     * @param labelMap Label map containing segmentation
     * @param labelId Label ID to analyze (1-255)
     * @param spacing Voxel spacing [x, y, z] in mm
     * @param options Analysis options controlling which metrics to compute
     * @return Shape analysis result or error
     */
    [[nodiscard]] std::expected<ShapeAnalysisResult, ShapeAnalysisError>
    analyze(LabelMapType::Pointer labelMap,
            uint8_t labelId,
            const SpacingType& spacing,
            const ShapeAnalysisOptions& options = {});

    /**
     * @brief Analyze all labels in the label map
     *
     * @param labelMap Label map containing segmentation
     * @param spacing Voxel spacing [x, y, z] in mm
     * @param options Analysis options controlling which metrics to compute
     * @return Vector of shape analysis results (one per label found)
     */
    [[nodiscard]] std::vector<std::expected<ShapeAnalysisResult, ShapeAnalysisError>>
    analyzeAll(LabelMapType::Pointer labelMap,
               const SpacingType& spacing,
               const ShapeAnalysisOptions& options = {});

    /**
     * @brief Compute principal axes only (lightweight analysis)
     *
     * @param labelMap Label map containing segmentation
     * @param labelId Label ID to analyze (1-255)
     * @param spacing Voxel spacing [x, y, z] in mm
     * @return Principal axes information or error
     */
    [[nodiscard]] std::expected<PrincipalAxes, ShapeAnalysisError>
    computePrincipalAxes(LabelMapType::Pointer labelMap,
                         uint8_t labelId,
                         const SpacingType& spacing);

    /**
     * @brief Compute oriented bounding box aligned to principal axes
     *
     * @param labelMap Label map containing segmentation
     * @param labelId Label ID to analyze (1-255)
     * @param spacing Voxel spacing [x, y, z] in mm
     * @return Oriented bounding box or error
     */
    [[nodiscard]] std::expected<BoundingBox, ShapeAnalysisError>
    computeOrientedBoundingBox(LabelMapType::Pointer labelMap,
                               uint8_t labelId,
                               const SpacingType& spacing);

    /**
     * @brief Export shape analysis results to CSV file
     *
     * @param results Vector of shape analysis results to export
     * @param filePath Output file path
     * @return Success or error
     */
    [[nodiscard]] static std::expected<void, ShapeAnalysisError>
    exportToCsv(const std::vector<ShapeAnalysisResult>& results,
                const std::filesystem::path& filePath);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

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
 * @file mesh_exporter.hpp
 * @brief STL and OBJ mesh file exporter with coordinate system conversion
 * @details Exports segmentation-derived or iso-surface meshes to STL
 *          (binary/ASCII) and OBJ formats. Supports RAS/LPS coordinate
 *          system conversion for compatibility with external analysis tools.
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <array>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <vtkSmartPointer.h>

class vtkImageData;
class vtkPolyData;
class QString;

namespace dicom_viewer::services {

// Forward declaration - ExportError is defined in data_exporter.hpp
// but we define a compatible version here to avoid circular dependencies
struct ExportError;

/**
 * @brief Mesh source type for export
 *
 * @trace SRS-FR-048
 */
enum class MeshSource {
    Segmentation,   ///< From segmentation label map
    IsoSurface      ///< From volume data iso-surface extraction
};

/**
 * @brief STL file format options
 */
enum class STLFormat {
    Binary,     ///< Binary format (smaller file size)
    ASCII       ///< ASCII format (human-readable)
};

/**
 * @brief Coordinate system for mesh export
 */
enum class CoordinateSystem {
    RAS,    ///< Right-Anterior-Superior (neurological convention)
    LPS     ///< Left-Posterior-Superior (radiological convention)
};

/**
 * @brief Export format for mesh files
 */
enum class MeshFormat {
    STL,    ///< STereoLithography format
    PLY,    ///< Polygon File Format
    OBJ     ///< Wavefront OBJ format
};

/**
 * @brief Options for mesh export operations
 *
 * @trace SRS-FR-048
 */
struct MeshExportOptions {
    /// Mesh quality settings
    bool smooth = true;                     ///< Enable Laplacian smoothing
    int smoothIterations = 20;              ///< Number of smoothing iterations
    double smoothRelaxation = 0.1;          ///< Smoothing relaxation factor

    bool decimate = true;                   ///< Enable mesh decimation
    double decimateTargetReduction = 0.5;   ///< Target reduction ratio [0-1]

    bool computeNormals = true;             ///< Compute vertex normals

    /// STL format options
    STLFormat stlFormat = STLFormat::Binary;

    /// PLY options
    bool plyIncludeColors = true;           ///< Include vertex colors in PLY
    bool plyIncludeNormals = true;          ///< Include normals in PLY

    /// Coordinate system
    CoordinateSystem coordSystem = CoordinateSystem::RAS;
    bool applyScaling = true;               ///< Apply voxel spacing to coordinates

    /// Iso-surface extraction (for IsoSurface source)
    double isoValue = 400.0;                ///< HU value for iso-surface
};

/**
 * @brief Result of mesh export operation
 *
 * @trace SRS-FR-048
 */
struct MeshExportResult {
    size_t vertexCount = 0;                 ///< Number of vertices
    size_t triangleCount = 0;               ///< Number of triangles
    double surfaceAreaMm2 = 0.0;            ///< Surface area in mm^2
    double volumeMm3 = 0.0;                 ///< Volume in mm^3
    std::filesystem::path outputPath;       ///< Path to exported file
};

/**
 * @brief Statistics for mesh preview without export
 *
 * @trace SRS-FR-048
 */
struct MeshStatistics {
    size_t vertexCount = 0;                 ///< Number of vertices
    size_t triangleCount = 0;               ///< Number of triangles
    double surfaceAreaMm2 = 0.0;            ///< Surface area in mm^2
    double volumeMm3 = 0.0;                 ///< Volume in mm^3
    std::array<double, 6> boundingBox{};    ///< [xmin, xmax, ymin, ymax, zmin, zmax]
};

/**
 * @brief Exporter for 3D mesh data to STL, PLY, and OBJ formats
 *
 * Exports 3D surface meshes from segmentation masks or iso-surface extraction
 * for use in 3D printing, CAD software integration, and surgical planning.
 *
 * @example
 * @code
 * MeshExporter exporter;
 *
 * // Set progress callback
 * exporter.setProgressCallback([](double progress, const QString& status) {
 *     qDebug() << status << ":" << progress * 100 << "%";
 * });
 *
 * // Export from segmentation label map
 * MeshExportOptions options;
 * options.smooth = true;
 * options.smoothIterations = 20;
 * options.decimate = true;
 * options.decimateTargetReduction = 0.5;
 *
 * auto result = exporter.exportFromSegmentation(
 *     labelMap,
 *     1,  // label ID
 *     "/path/to/output.stl",
 *     MeshFormat::STL,
 *     options
 * );
 *
 * if (result) {
 *     qDebug() << "Exported" << result->triangleCount << "triangles";
 * }
 *
 * // Export iso-surface from volume data
 * options.isoValue = 300.0;  // Bone threshold
 * auto isoResult = exporter.exportIsoSurface(
 *     volumeData,
 *     300.0,
 *     "/path/to/bone.stl",
 *     MeshFormat::STL,
 *     options
 * );
 * @endcode
 *
 * @trace SRS-FR-048
 */
class MeshExporter {
public:
    using ProgressCallback = std::function<void(double progress, const QString& status)>;
    using LabelMapType = vtkSmartPointer<vtkImageData>;
    using ImageType = vtkSmartPointer<vtkImageData>;

    MeshExporter();
    ~MeshExporter();

    // Non-copyable, movable
    MeshExporter(const MeshExporter&) = delete;
    MeshExporter& operator=(const MeshExporter&) = delete;
    MeshExporter(MeshExporter&&) noexcept;
    MeshExporter& operator=(MeshExporter&&) noexcept;

    /**
     * @brief Set progress callback
     * @param callback Function called with progress (0.0-1.0) and status message
     */
    void setProgressCallback(ProgressCallback callback);

    // =========================================================================
    // Export from Segmentation
    // =========================================================================

    /**
     * @brief Export mesh from segmentation label map
     *
     * Extracts surface mesh from a specific label in the segmentation mask
     * using Marching Cubes algorithm.
     *
     * @param labelMap Segmentation label map (unsigned char voxels)
     * @param labelId Label ID to extract (1-255)
     * @param outputPath Output file path
     * @param format Export format (STL, PLY, or OBJ)
     * @param options Export options
     * @return Export result or error
     */
    [[nodiscard]] std::expected<MeshExportResult, ExportError> exportFromSegmentation(
        LabelMapType labelMap,
        uint8_t labelId,
        const std::filesystem::path& outputPath,
        MeshFormat format,
        const MeshExportOptions& options = {}) const;

    /**
     * @brief Export all labels from segmentation to separate files
     *
     * @param labelMap Segmentation label map
     * @param outputDirectory Output directory for mesh files
     * @param format Export format
     * @param options Export options
     * @return Vector of export results or error
     */
    [[nodiscard]] std::expected<std::vector<MeshExportResult>, ExportError> exportAllLabels(
        LabelMapType labelMap,
        const std::filesystem::path& outputDirectory,
        MeshFormat format,
        const MeshExportOptions& options = {}) const;

    // =========================================================================
    // Export from Iso-Surface
    // =========================================================================

    /**
     * @brief Export iso-surface from volume data
     *
     * Extracts surface at specified iso-value (e.g., HU threshold for CT)
     * using Marching Cubes algorithm.
     *
     * @param volume Volume image data
     * @param isoValue Iso-value for surface extraction
     * @param outputPath Output file path
     * @param format Export format
     * @param options Export options
     * @return Export result or error
     */
    [[nodiscard]] std::expected<MeshExportResult, ExportError> exportIsoSurface(
        ImageType volume,
        double isoValue,
        const std::filesystem::path& outputPath,
        MeshFormat format,
        const MeshExportOptions& options = {}) const;

    // =========================================================================
    // Export from PolyData
    // =========================================================================

    /**
     * @brief Export existing VTK PolyData to mesh file
     *
     * Useful for exporting meshes from SurfaceRenderer.
     *
     * @param polyData VTK polydata to export
     * @param outputPath Output file path
     * @param format Export format
     * @param options Export options
     * @return Export result or error
     */
    [[nodiscard]] std::expected<MeshExportResult, ExportError> exportPolyData(
        vtkSmartPointer<vtkPolyData> polyData,
        const std::filesystem::path& outputPath,
        MeshFormat format,
        const MeshExportOptions& options = {}) const;

    // =========================================================================
    // Preview and Statistics
    // =========================================================================

    /**
     * @brief Preview mesh statistics without exporting
     *
     * @param labelMap Segmentation label map
     * @param labelId Label ID to analyze
     * @param options Export options (affects mesh processing)
     * @return Mesh statistics or error
     */
    [[nodiscard]] std::expected<MeshStatistics, ExportError> previewStatistics(
        LabelMapType labelMap,
        uint8_t labelId,
        const MeshExportOptions& options = {}) const;

    /**
     * @brief Preview iso-surface statistics without exporting
     *
     * @param volume Volume image data
     * @param isoValue Iso-value for surface extraction
     * @param options Export options
     * @return Mesh statistics or error
     */
    [[nodiscard]] std::expected<MeshStatistics, ExportError> previewIsoSurfaceStatistics(
        ImageType volume,
        double isoValue,
        const MeshExportOptions& options = {}) const;

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Get file extension for format
     * @param format Mesh format
     * @return File extension (e.g., ".stl")
     */
    [[nodiscard]] static std::string getFileExtension(MeshFormat format);

    /**
     * @brief Get format from file path
     * @param path File path with extension
     * @return Detected format or nullopt
     */
    [[nodiscard]] static std::optional<MeshFormat> detectFormat(
        const std::filesystem::path& path);

    /**
     * @brief Get unique labels from segmentation
     * @param labelMap Segmentation label map
     * @return Vector of label IDs present in the map (excluding 0)
     */
    [[nodiscard]] static std::vector<uint8_t> getUniqueLabels(LabelMapType labelMap);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

#include "services/export/mesh_exporter.hpp"
#include "services/export/data_exporter.hpp"
#include "core/logging.hpp"

#include <QString>
#include <QFileInfo>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkMarchingCubes.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkDecimatePro.h>
#include <vtkPolyDataNormals.h>
#include <vtkMassProperties.h>
#include <vtkSTLWriter.h>
#include <vtkPLYWriter.h>
#include <vtkOBJWriter.h>
#include <vtkThreshold.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkImageThreshold.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkTriangleFilter.h>

#include <algorithm>
#include <set>
#include <cmath>

namespace dicom_viewer::services {

namespace {

/**
 * @brief Apply coordinate system transformation
 */
vtkSmartPointer<vtkPolyData> applyCoordinateTransform(
    vtkSmartPointer<vtkPolyData> polyData,
    CoordinateSystem targetSystem)
{
    if (targetSystem == CoordinateSystem::RAS) {
        // VTK uses RAS by default, no transformation needed
        return polyData;
    }

    // Convert RAS to LPS: flip X and Y axes
    vtkNew<vtkTransform> transform;
    transform->Scale(-1.0, -1.0, 1.0);

    vtkNew<vtkTransformPolyDataFilter> transformFilter;
    transformFilter->SetInputData(polyData);
    transformFilter->SetTransform(transform);
    transformFilter->Update();

    return transformFilter->GetOutput();
}

/**
 * @brief Extract surface from label map using Marching Cubes
 */
vtkSmartPointer<vtkPolyData> extractSurfaceFromLabel(
    vtkSmartPointer<vtkImageData> labelMap,
    uint8_t labelId)
{
    // Create binary mask for the specific label
    vtkNew<vtkImageThreshold> threshold;
    threshold->SetInputData(labelMap);
    threshold->ThresholdBetween(labelId, labelId);
    threshold->SetInValue(255);
    threshold->SetOutValue(0);
    threshold->SetOutputScalarTypeToUnsignedChar();
    threshold->Update();

    // Extract surface using Marching Cubes
    vtkNew<vtkMarchingCubes> marchingCubes;
    marchingCubes->SetInputData(threshold->GetOutput());
    marchingCubes->SetValue(0, 127.5);  // Mid-value threshold
    marchingCubes->ComputeNormalsOn();
    marchingCubes->ComputeGradientsOff();
    marchingCubes->Update();

    return marchingCubes->GetOutput();
}

/**
 * @brief Extract iso-surface from volume data
 */
vtkSmartPointer<vtkPolyData> extractIsoSurface(
    vtkSmartPointer<vtkImageData> volume,
    double isoValue)
{
    vtkNew<vtkMarchingCubes> marchingCubes;
    marchingCubes->SetInputData(volume);
    marchingCubes->SetValue(0, isoValue);
    marchingCubes->ComputeNormalsOn();
    marchingCubes->ComputeGradientsOff();
    marchingCubes->Update();

    return marchingCubes->GetOutput();
}

/**
 * @brief Apply mesh smoothing
 */
vtkSmartPointer<vtkPolyData> smoothMesh(
    vtkSmartPointer<vtkPolyData> polyData,
    int iterations,
    double passBand)
{
    if (iterations <= 0) {
        return polyData;
    }

    vtkNew<vtkWindowedSincPolyDataFilter> smoother;
    smoother->SetInputData(polyData);
    smoother->SetNumberOfIterations(iterations);
    smoother->SetPassBand(passBand);
    smoother->BoundarySmoothingOff();
    smoother->FeatureEdgeSmoothingOff();
    smoother->NonManifoldSmoothingOn();
    smoother->NormalizeCoordinatesOn();
    smoother->Update();

    return smoother->GetOutput();
}

/**
 * @brief Apply mesh decimation
 */
vtkSmartPointer<vtkPolyData> decimateMesh(
    vtkSmartPointer<vtkPolyData> polyData,
    double targetReduction)
{
    if (targetReduction <= 0.0 || targetReduction >= 1.0) {
        return polyData;
    }

    vtkNew<vtkDecimatePro> decimator;
    decimator->SetInputData(polyData);
    decimator->SetTargetReduction(targetReduction);
    decimator->PreserveTopologyOn();
    decimator->BoundaryVertexDeletionOff();
    decimator->Update();

    return decimator->GetOutput();
}

/**
 * @brief Compute mesh normals
 */
vtkSmartPointer<vtkPolyData> computeNormals(vtkSmartPointer<vtkPolyData> polyData)
{
    vtkNew<vtkPolyDataNormals> normalGenerator;
    normalGenerator->SetInputData(polyData);
    normalGenerator->ComputePointNormalsOn();
    normalGenerator->ComputeCellNormalsOn();
    normalGenerator->SplittingOff();
    normalGenerator->ConsistencyOn();
    normalGenerator->AutoOrientNormalsOn();
    normalGenerator->Update();

    return normalGenerator->GetOutput();
}

/**
 * @brief Clean and triangulate mesh
 */
vtkSmartPointer<vtkPolyData> cleanAndTriangulate(vtkSmartPointer<vtkPolyData> polyData)
{
    // Remove duplicate points
    vtkNew<vtkCleanPolyData> cleaner;
    cleaner->SetInputData(polyData);
    cleaner->SetTolerance(0.0);
    cleaner->Update();

    // Ensure all faces are triangles
    vtkNew<vtkTriangleFilter> triangulator;
    triangulator->SetInputConnection(cleaner->GetOutputPort());
    triangulator->Update();

    return triangulator->GetOutput();
}

/**
 * @brief Process mesh with all options
 */
vtkSmartPointer<vtkPolyData> processMesh(
    vtkSmartPointer<vtkPolyData> polyData,
    const MeshExportOptions& options)
{
    auto result = polyData;

    // Clean and triangulate first
    result = cleanAndTriangulate(result);

    // Apply smoothing
    if (options.smooth && options.smoothIterations > 0) {
        // Convert relaxation factor to pass band
        double passBand = 0.1 * (1.0 - options.smoothRelaxation);
        result = smoothMesh(result, options.smoothIterations, passBand);
    }

    // Apply decimation
    if (options.decimate && options.decimateTargetReduction > 0.0) {
        result = decimateMesh(result, options.decimateTargetReduction);
    }

    // Compute normals
    if (options.computeNormals) {
        result = computeNormals(result);
    }

    // Apply coordinate system transformation
    result = applyCoordinateTransform(result, options.coordSystem);

    return result;
}

/**
 * @brief Get mesh statistics
 */
MeshStatistics getMeshStatistics(vtkSmartPointer<vtkPolyData> polyData)
{
    MeshStatistics stats;

    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        return stats;
    }

    stats.vertexCount = static_cast<size_t>(polyData->GetNumberOfPoints());
    stats.triangleCount = static_cast<size_t>(polyData->GetNumberOfCells());

    // Compute mass properties
    vtkNew<vtkMassProperties> massProperties;
    massProperties->SetInputData(polyData);
    massProperties->Update();

    stats.surfaceAreaMm2 = massProperties->GetSurfaceArea();
    stats.volumeMm3 = std::abs(massProperties->GetVolume());

    // Get bounding box
    double bounds[6];
    polyData->GetBounds(bounds);
    for (int i = 0; i < 6; ++i) {
        stats.boundingBox[i] = bounds[i];
    }

    return stats;
}

}  // namespace

// =============================================================================
// MeshExporter::Impl
// =============================================================================

class MeshExporter::Impl {
public:
    ProgressCallback progressCallback;
    std::shared_ptr<spdlog::logger> logger;

    Impl() : logger(logging::LoggerFactory::create("MeshExporter")) {}

    void reportProgress(double progress, const QString& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
        if (logger) {
            logger->debug("{}: {:.1f}%", status.toStdString(), progress * 100.0);
        }
    }

    std::expected<void, ExportError> writeSTL(
        vtkSmartPointer<vtkPolyData> polyData,
        const std::filesystem::path& outputPath,
        STLFormat format) const
    {
        vtkNew<vtkSTLWriter> writer;
        writer->SetInputData(polyData);
        writer->SetFileName(outputPath.string().c_str());

        if (format == STLFormat::Binary) {
            writer->SetFileTypeToBinary();
        } else {
            writer->SetFileTypeToASCII();
        }

        if (!writer->Write()) {
            return std::unexpected(ExportError{
                ExportError::Code::InternalError,
                "Failed to write STL file: " + outputPath.string()
            });
        }

        return {};
    }

    std::expected<void, ExportError> writePLY(
        vtkSmartPointer<vtkPolyData> polyData,
        const std::filesystem::path& outputPath,
        bool includeColors) const
    {
        vtkNew<vtkPLYWriter> writer;
        writer->SetInputData(polyData);
        writer->SetFileName(outputPath.string().c_str());
        writer->SetFileTypeToBinary();

        if (includeColors && polyData->GetPointData()->GetScalars()) {
            writer->SetArrayName(polyData->GetPointData()->GetScalars()->GetName());
        }

        if (!writer->Write()) {
            return std::unexpected(ExportError{
                ExportError::Code::InternalError,
                "Failed to write PLY file: " + outputPath.string()
            });
        }

        return {};
    }

    std::expected<void, ExportError> writeOBJ(
        vtkSmartPointer<vtkPolyData> polyData,
        const std::filesystem::path& outputPath) const
    {
        vtkNew<vtkOBJWriter> writer;
        writer->SetInputData(polyData);
        writer->SetFileName(outputPath.string().c_str());

        if (!writer->Write()) {
            return std::unexpected(ExportError{
                ExportError::Code::InternalError,
                "Failed to write OBJ file: " + outputPath.string()
            });
        }

        return {};
    }

    std::expected<void, ExportError> writeMesh(
        vtkSmartPointer<vtkPolyData> polyData,
        const std::filesystem::path& outputPath,
        MeshFormat format,
        const MeshExportOptions& options) const
    {
        switch (format) {
            case MeshFormat::STL:
                return writeSTL(polyData, outputPath, options.stlFormat);
            case MeshFormat::PLY:
                return writePLY(polyData, outputPath, options.plyIncludeColors);
            case MeshFormat::OBJ:
                return writeOBJ(polyData, outputPath);
        }
        return std::unexpected(ExportError{
            ExportError::Code::UnsupportedFormat,
            "Unknown mesh format"
        });
    }

    std::expected<MeshExportResult, ExportError> exportMesh(
        vtkSmartPointer<vtkPolyData> polyData,
        const std::filesystem::path& outputPath,
        MeshFormat format,
        const MeshExportOptions& options) const
    {
        if (!polyData || polyData->GetNumberOfPoints() == 0) {
            return std::unexpected(ExportError{
                ExportError::Code::InvalidData,
                "Empty or invalid mesh data"
            });
        }

        // Check output directory exists
        auto parentPath = outputPath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
            return std::unexpected(ExportError{
                ExportError::Code::FileAccessDenied,
                "Output directory does not exist: " + parentPath.string()
            });
        }

        reportProgress(0.1, "Processing mesh...");

        // Process mesh with options
        auto processedMesh = processMesh(polyData, options);

        reportProgress(0.7, "Computing statistics...");

        // Get statistics
        auto stats = getMeshStatistics(processedMesh);

        reportProgress(0.8, "Writing file...");

        // Write mesh file
        auto writeResult = writeMesh(processedMesh, outputPath, format, options);
        if (!writeResult) {
            return std::unexpected(writeResult.error());
        }

        reportProgress(1.0, "Export complete");

        // Build result
        MeshExportResult result;
        result.vertexCount = stats.vertexCount;
        result.triangleCount = stats.triangleCount;
        result.surfaceAreaMm2 = stats.surfaceAreaMm2;
        result.volumeMm3 = stats.volumeMm3;
        result.outputPath = outputPath;

        if (logger) {
            logger->info("Exported mesh: {} vertices, {} triangles, {:.2f} mm² surface area",
                         result.vertexCount, result.triangleCount, result.surfaceAreaMm2);
        }

        return result;
    }
};

// =============================================================================
// MeshExporter public methods
// =============================================================================

MeshExporter::MeshExporter() : impl_(std::make_unique<Impl>()) {}

MeshExporter::~MeshExporter() = default;

MeshExporter::MeshExporter(MeshExporter&&) noexcept = default;
MeshExporter& MeshExporter::operator=(MeshExporter&&) noexcept = default;

void MeshExporter::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

// =============================================================================
// Export from Segmentation
// =============================================================================

std::expected<MeshExportResult, ExportError> MeshExporter::exportFromSegmentation(
    LabelMapType labelMap,
    uint8_t labelId,
    const std::filesystem::path& outputPath,
    MeshFormat format,
    const MeshExportOptions& options) const
{
    if (!labelMap) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Label map is null"
        });
    }

    if (labelId == 0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Label ID 0 is reserved for background"
        });
    }

    impl_->reportProgress(0.0, "Extracting surface from segmentation...");

    auto polyData = extractSurfaceFromLabel(labelMap, labelId);

    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "No surface found for label " + std::to_string(labelId)
        });
    }

    return impl_->exportMesh(polyData, outputPath, format, options);
}

std::expected<std::vector<MeshExportResult>, ExportError> MeshExporter::exportAllLabels(
    LabelMapType labelMap,
    const std::filesystem::path& outputDirectory,
    MeshFormat format,
    const MeshExportOptions& options) const
{
    if (!labelMap) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Label map is null"
        });
    }

    if (!std::filesystem::exists(outputDirectory)) {
        return std::unexpected(ExportError{
            ExportError::Code::FileAccessDenied,
            "Output directory does not exist: " + outputDirectory.string()
        });
    }

    auto labels = getUniqueLabels(labelMap);
    if (labels.empty()) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "No labels found in segmentation"
        });
    }

    std::vector<MeshExportResult> results;
    results.reserve(labels.size());

    std::string extension = getFileExtension(format);

    for (size_t i = 0; i < labels.size(); ++i) {
        uint8_t labelId = labels[i];
        double progress = static_cast<double>(i) / static_cast<double>(labels.size());
        impl_->reportProgress(progress,
            QString("Exporting label %1 of %2...").arg(i + 1).arg(labels.size()));

        std::string filename = "label_" + std::to_string(labelId) + extension;
        auto outputPath = outputDirectory / filename;

        auto result = exportFromSegmentation(labelMap, labelId, outputPath, format, options);
        if (result) {
            results.push_back(*result);
        } else {
            // Log warning but continue with other labels
            if (impl_->logger) {
                impl_->logger->warn("Failed to export label {}: {}",
                                    labelId, result.error().toString());
            }
        }
    }

    impl_->reportProgress(1.0, "Export complete");
    return results;
}

// =============================================================================
// Export from Iso-Surface
// =============================================================================

std::expected<MeshExportResult, ExportError> MeshExporter::exportIsoSurface(
    ImageType volume,
    double isoValue,
    const std::filesystem::path& outputPath,
    MeshFormat format,
    const MeshExportOptions& options) const
{
    if (!volume) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Volume data is null"
        });
    }

    impl_->reportProgress(0.0, "Extracting iso-surface...");

    auto polyData = extractIsoSurface(volume, isoValue);

    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "No surface found at iso-value " + std::to_string(isoValue)
        });
    }

    return impl_->exportMesh(polyData, outputPath, format, options);
}

// =============================================================================
// Export from PolyData
// =============================================================================

std::expected<MeshExportResult, ExportError> MeshExporter::exportPolyData(
    vtkSmartPointer<vtkPolyData> polyData,
    const std::filesystem::path& outputPath,
    MeshFormat format,
    const MeshExportOptions& options) const
{
    return impl_->exportMesh(polyData, outputPath, format, options);
}

// =============================================================================
// Preview and Statistics
// =============================================================================

std::expected<MeshStatistics, ExportError> MeshExporter::previewStatistics(
    LabelMapType labelMap,
    uint8_t labelId,
    const MeshExportOptions& options) const
{
    if (!labelMap) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Label map is null"
        });
    }

    if (labelId == 0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Label ID 0 is reserved for background"
        });
    }

    impl_->reportProgress(0.0, "Extracting surface for preview...");

    auto polyData = extractSurfaceFromLabel(labelMap, labelId);

    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "No surface found for label " + std::to_string(labelId)
        });
    }

    impl_->reportProgress(0.3, "Processing mesh...");

    auto processedMesh = processMesh(polyData, options);

    impl_->reportProgress(0.9, "Computing statistics...");

    auto stats = getMeshStatistics(processedMesh);

    impl_->reportProgress(1.0, "Preview complete");

    return stats;
}

std::expected<MeshStatistics, ExportError> MeshExporter::previewIsoSurfaceStatistics(
    ImageType volume,
    double isoValue,
    const MeshExportOptions& options) const
{
    if (!volume) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "Volume data is null"
        });
    }

    impl_->reportProgress(0.0, "Extracting iso-surface for preview...");

    auto polyData = extractIsoSurface(volume, isoValue);

    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        return std::unexpected(ExportError{
            ExportError::Code::InvalidData,
            "No surface found at iso-value " + std::to_string(isoValue)
        });
    }

    impl_->reportProgress(0.3, "Processing mesh...");

    auto processedMesh = processMesh(polyData, options);

    impl_->reportProgress(0.9, "Computing statistics...");

    auto stats = getMeshStatistics(processedMesh);

    impl_->reportProgress(1.0, "Preview complete");

    return stats;
}

// =============================================================================
// Utility Methods
// =============================================================================

std::string MeshExporter::getFileExtension(MeshFormat format) {
    switch (format) {
        case MeshFormat::STL: return ".stl";
        case MeshFormat::PLY: return ".ply";
        case MeshFormat::OBJ: return ".obj";
    }
    return ".stl";
}

std::optional<MeshFormat> MeshExporter::detectFormat(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".stl") return MeshFormat::STL;
    if (ext == ".ply") return MeshFormat::PLY;
    if (ext == ".obj") return MeshFormat::OBJ;

    return std::nullopt;
}

std::vector<uint8_t> MeshExporter::getUniqueLabels(LabelMapType labelMap) {
    std::set<uint8_t> labelSet;

    if (!labelMap) {
        return {};
    }

    auto* scalars = labelMap->GetPointData()->GetScalars();
    if (!scalars) {
        return {};
    }

    vtkIdType numPoints = scalars->GetNumberOfTuples();
    for (vtkIdType i = 0; i < numPoints; ++i) {
        auto value = static_cast<uint8_t>(scalars->GetComponent(i, 0));
        if (value > 0) {  // Exclude background (0)
            labelSet.insert(value);
        }
    }

    return std::vector<uint8_t>(labelSet.begin(), labelSet.end());
}

}  // namespace dicom_viewer::services

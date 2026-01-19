#include "services/measurement/volume_calculator.hpp"
#include "core/logging.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>

#include <itkImageRegionConstIterator.h>
#include <itkLabelStatisticsImageFilter.h>

#include <vtkImageData.h>
#include <vtkMarchingCubes.h>
#include <vtkMassProperties.h>
#include <vtkSmartPointer.h>

namespace dicom_viewer::services {

// =============================================================================
// VolumeResult implementation
// =============================================================================

std::string VolumeResult::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "Volume Measurement";
    if (!labelName.empty()) {
        oss << " (" << labelName << ")";
    }
    oss << ":\n";

    oss << "  Label ID:    " << static_cast<int>(labelId) << "\n";
    oss << "  Voxel Count: " << voxelCount << "\n";
    oss << "  Volume:      " << volumeMm3 << " mm^3\n";
    oss << "               " << volumeCm3 << " cm^3\n";
    oss << "               " << volumeML << " mL\n";

    if (surfaceAreaMm2.has_value()) {
        oss << "  Surface:     " << surfaceAreaMm2.value() << " mm^2\n";
    }

    if (sphericity.has_value()) {
        oss << "  Sphericity:  " << sphericity.value() << "\n";
    }

    oss << "  Bounding:    " << boundingBoxMm[0] << " x "
        << boundingBoxMm[1] << " x " << boundingBoxMm[2] << " mm\n";

    return oss.str();
}

std::vector<std::string> VolumeResult::getCsvHeader() {
    return {
        "LabelID", "LabelName", "VoxelCount",
        "VolumeMm3", "VolumeCm3", "VolumeML",
        "SurfaceAreaMm2", "Sphericity",
        "BoundingX_mm", "BoundingY_mm", "BoundingZ_mm"
    };
}

std::vector<std::string> VolumeResult::getCsvRow() const {
    auto format = [](double val) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << val;
        return oss.str();
    };

    auto formatOpt = [&format](const std::optional<double>& val) {
        return val.has_value() ? format(val.value()) : "";
    };

    return {
        std::to_string(labelId),
        labelName,
        std::to_string(voxelCount),
        format(volumeMm3),
        format(volumeCm3),
        format(volumeML),
        formatOpt(surfaceAreaMm2),
        formatOpt(sphericity),
        format(boundingBoxMm[0]),
        format(boundingBoxMm[1]),
        format(boundingBoxMm[2])
    };
}

// =============================================================================
// VolumeComparisonTable implementation
// =============================================================================

std::string VolumeComparisonTable::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "Volume Comparison Table\n";
    oss << "==================================================\n";
    oss << std::left << std::setw(20) << "Label"
        << std::right << std::setw(15) << "Volume (mL)"
        << std::setw(12) << "Percentage" << "\n";
    oss << "--------------------------------------------------\n";

    for (size_t i = 0; i < results.size(); ++i) {
        oss << std::left << std::setw(20);
        if (results[i].labelName.empty()) {
            oss << ("Label " + std::to_string(results[i].labelId));
        } else {
            oss << results[i].labelName;
        }
        oss << std::right << std::setw(15) << results[i].volumeML
            << std::setw(11) << percentages[i] << "%" << "\n";
    }

    oss << "--------------------------------------------------\n";
    oss << std::left << std::setw(20) << "Total"
        << std::right << std::setw(15) << (totalVolumeMm3 / 1000.0)
        << std::setw(11) << "100.00" << "%" << "\n";

    return oss.str();
}

std::expected<void, std::string>
VolumeComparisonTable::exportToCsv(const std::filesystem::path& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected("Failed to open file for writing: " + filePath.string());
    }

    // Write header
    auto header = VolumeResult::getCsvHeader();
    for (const auto& col : header) {
        file << col << ",";
    }
    file << "Percentage\n";

    // Write data rows
    for (size_t i = 0; i < results.size(); ++i) {
        auto row = results[i].getCsvRow();
        for (const auto& val : row) {
            file << val << ",";
        }
        std::ostringstream pct;
        pct << std::fixed << std::setprecision(2) << percentages[i];
        file << pct.str() << "\n";
    }

    // Write total row
    file << ",Total," << std::accumulate(
        results.begin(), results.end(), int64_t(0),
        [](int64_t sum, const VolumeResult& r) { return sum + r.voxelCount; }
    ) << ",";

    std::ostringstream total;
    total << std::fixed << std::setprecision(4);
    total << totalVolumeMm3 << "," << (totalVolumeMm3 / 1000.0) << ","
          << (totalVolumeMm3 / 1000.0) << ",,,,,100.00\n";
    file << total.str();

    return {};
}

// =============================================================================
// VolumeCalculator::Impl
// =============================================================================

class VolumeCalculator::Impl {
public:
    ProgressCallback progressCallback;
    std::shared_ptr<spdlog::logger> logger;

    Impl() : logger(logging::LoggerFactory::create("VolumeCalculator")) {}

    // Find all unique label IDs in the label map
    std::set<uint8_t> findAllLabels(LabelMapType::Pointer labelMap) {
        std::set<uint8_t> labels;

        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(labelMap, labelMap->GetLargestPossibleRegion());

        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            uint8_t val = it.Get();
            if (val > 0) {  // Exclude background
                labels.insert(val);
            }
        }

        return labels;
    }

    // Calculate bounding box for a label
    std::array<double, 3> calculateBoundingBox(
        LabelMapType::Pointer labelMap,
        uint8_t labelId,
        const SpacingType& spacing
    ) {
        auto region = labelMap->GetLargestPossibleRegion();
        auto size = region.GetSize();

        int minX = static_cast<int>(size[0]), maxX = 0;
        int minY = static_cast<int>(size[1]), maxY = 0;
        int minZ = static_cast<int>(size[2]), maxZ = 0;

        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(labelMap, region);

        LabelMapType::IndexType idx;
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == labelId) {
                idx = it.GetIndex();
                minX = std::min(minX, static_cast<int>(idx[0]));
                maxX = std::max(maxX, static_cast<int>(idx[0]));
                minY = std::min(minY, static_cast<int>(idx[1]));
                maxY = std::max(maxY, static_cast<int>(idx[1]));
                minZ = std::min(minZ, static_cast<int>(idx[2]));
                maxZ = std::max(maxZ, static_cast<int>(idx[2]));
            }
        }

        if (maxX < minX) {
            return {0.0, 0.0, 0.0};
        }

        return {
            static_cast<double>(maxX - minX + 1) * spacing[0],
            static_cast<double>(maxY - minY + 1) * spacing[1],
            static_cast<double>(maxZ - minZ + 1) * spacing[2]
        };
    }

    // Create VTK image data from label map for marching cubes
    vtkSmartPointer<vtkImageData> createVtkBinaryImage(
        LabelMapType::Pointer labelMap,
        uint8_t labelId,
        const SpacingType& spacing
    ) {
        auto region = labelMap->GetLargestPossibleRegion();
        auto size = region.GetSize();
        auto origin = labelMap->GetOrigin();

        auto vtkImage = vtkSmartPointer<vtkImageData>::New();
        vtkImage->SetDimensions(
            static_cast<int>(size[0]),
            static_cast<int>(size[1]),
            static_cast<int>(size[2])
        );
        vtkImage->SetSpacing(spacing[0], spacing[1], spacing[2]);
        vtkImage->SetOrigin(origin[0], origin[1], origin[2]);
        vtkImage->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

        unsigned char* ptr = static_cast<unsigned char*>(vtkImage->GetScalarPointer());

        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(labelMap, region);

        size_t i = 0;
        for (it.GoToBegin(); !it.IsAtEnd(); ++it, ++i) {
            ptr[i] = (it.Get() == labelId) ? 255 : 0;
        }

        return vtkImage;
    }

    // Calculate surface area using marching cubes
    std::optional<double> calculateSurfaceArea(
        LabelMapType::Pointer labelMap,
        uint8_t labelId,
        const SpacingType& spacing
    ) {
        try {
            auto vtkImage = createVtkBinaryImage(labelMap, labelId, spacing);

            auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
            marchingCubes->SetInputData(vtkImage);
            marchingCubes->SetValue(0, 127.5);  // Threshold between 0 and 255
            marchingCubes->ComputeNormalsOff();
            marchingCubes->Update();

            auto mesh = marchingCubes->GetOutput();
            if (mesh->GetNumberOfPolys() == 0) {
                return std::nullopt;
            }

            auto massProperties = vtkSmartPointer<vtkMassProperties>::New();
            massProperties->SetInputData(mesh);
            massProperties->Update();

            return massProperties->GetSurfaceArea();
        } catch (...) {
            return std::nullopt;
        }
    }

    // Calculate sphericity (ratio of surface area of equivalent sphere to actual)
    std::optional<double> calculateSphericity(double volume, double surfaceArea) {
        if (surfaceArea <= 0.0 || volume <= 0.0) {
            return std::nullopt;
        }

        // Surface area of sphere with same volume
        // V = (4/3) * pi * r^3  =>  r = cbrt(3V / 4pi)
        // A = 4 * pi * r^2
        double r = std::cbrt(3.0 * volume / (4.0 * M_PI));
        double sphereSurfaceArea = 4.0 * M_PI * r * r;

        return sphereSurfaceArea / surfaceArea;
    }
};

// =============================================================================
// VolumeCalculator implementation
// =============================================================================

VolumeCalculator::VolumeCalculator()
    : impl_(std::make_unique<Impl>())
{
}

VolumeCalculator::~VolumeCalculator() = default;

VolumeCalculator::VolumeCalculator(VolumeCalculator&&) noexcept = default;
VolumeCalculator& VolumeCalculator::operator=(VolumeCalculator&&) noexcept = default;

void VolumeCalculator::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<VolumeResult, VolumeError>
VolumeCalculator::calculate(LabelMapType::Pointer labelMap,
                             uint8_t labelId,
                             const SpacingType& spacing,
                             bool computeSurfaceArea) {
    impl_->logger->debug("Calculating volume for label {}", labelId);

    if (!labelMap) {
        impl_->logger->error("Label map is null");
        return std::unexpected(VolumeError{
            VolumeError::Code::InvalidLabelMap,
            "Label map is null"
        });
    }

    if (labelId == 0) {
        impl_->logger->error("Label ID 0 is reserved");
        return std::unexpected(VolumeError{
            VolumeError::Code::LabelNotFound,
            "Label ID 0 is reserved for background"
        });
    }

    if (spacing[0] <= 0 || spacing[1] <= 0 || spacing[2] <= 0) {
        impl_->logger->error("Invalid spacing values");
        return std::unexpected(VolumeError{
            VolumeError::Code::InvalidSpacing,
            "Spacing values must be positive"
        });
    }

    // Count voxels with the specified label
    int64_t voxelCount = 0;
    using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
    IteratorType it(labelMap, labelMap->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() == labelId) {
            ++voxelCount;
        }
    }

    if (voxelCount == 0) {
        return std::unexpected(VolumeError{
            VolumeError::Code::LabelNotFound,
            "No voxels found with label ID " + std::to_string(labelId)
        });
    }

    VolumeResult result;
    result.labelId = labelId;
    result.voxelCount = voxelCount;

    // Calculate volume
    double voxelVolume = spacing[0] * spacing[1] * spacing[2];
    result.volumeMm3 = static_cast<double>(voxelCount) * voxelVolume;
    result.volumeCm3 = result.volumeMm3 / 1000.0;
    result.volumeML = result.volumeCm3;  // 1 cm^3 = 1 mL

    // Calculate bounding box
    result.boundingBoxMm = impl_->calculateBoundingBox(labelMap, labelId, spacing);

    // Calculate surface area if requested
    if (computeSurfaceArea) {
        result.surfaceAreaMm2 = impl_->calculateSurfaceArea(labelMap, labelId, spacing);

        if (result.surfaceAreaMm2.has_value()) {
            result.sphericity = impl_->calculateSphericity(
                result.volumeMm3, result.surfaceAreaMm2.value()
            );
        }
    }

    return result;
}

std::vector<std::expected<VolumeResult, VolumeError>>
VolumeCalculator::calculateAll(LabelMapType::Pointer labelMap,
                                const SpacingType& spacing,
                                bool computeSurfaceArea) {
    std::vector<std::expected<VolumeResult, VolumeError>> results;

    if (!labelMap) {
        results.push_back(std::unexpected(VolumeError{
            VolumeError::Code::InvalidLabelMap,
            "Label map is null"
        }));
        return results;
    }

    auto labels = impl_->findAllLabels(labelMap);
    if (labels.empty()) {
        return results;  // No labels found, return empty vector
    }

    results.reserve(labels.size());

    size_t processed = 0;
    for (uint8_t labelId : labels) {
        results.push_back(calculate(labelMap, labelId, spacing, computeSurfaceArea));

        if (impl_->progressCallback) {
            impl_->progressCallback(
                static_cast<double>(++processed) / static_cast<double>(labels.size())
            );
        }
    }

    return results;
}

VolumeComparisonTable
VolumeCalculator::createComparisonTable(const std::vector<VolumeResult>& results) {
    VolumeComparisonTable table;
    table.results = results;

    // Calculate total volume
    table.totalVolumeMm3 = 0.0;
    for (const auto& result : results) {
        table.totalVolumeMm3 += result.volumeMm3;
    }

    // Calculate percentages
    table.percentages.reserve(results.size());
    for (const auto& result : results) {
        if (table.totalVolumeMm3 > 0.0) {
            table.percentages.push_back(
                (result.volumeMm3 / table.totalVolumeMm3) * 100.0
            );
        } else {
            table.percentages.push_back(0.0);
        }
    }

    return table;
}

VolumeTimePoint
VolumeCalculator::calculateChange(const VolumeResult& current,
                                   const VolumeResult& previous,
                                   const std::string& studyDate,
                                   const std::string& studyDescription) {
    VolumeTimePoint timePoint;
    timePoint.studyDate = studyDate;
    timePoint.studyDescription = studyDescription;
    timePoint.volume = current;

    double change = current.volumeMm3 - previous.volumeMm3;
    timePoint.changeFromPreviousMm3 = change;

    if (previous.volumeMm3 > 0.0) {
        timePoint.changePercentage = (change / previous.volumeMm3) * 100.0;
    }

    return timePoint;
}

std::expected<void, VolumeError>
VolumeCalculator::exportToCsv(const std::vector<VolumeResult>& results,
                               const std::filesystem::path& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(VolumeError{
            VolumeError::Code::ExportFailed,
            "Failed to open file: " + filePath.string()
        });
    }

    // Write header
    auto header = VolumeResult::getCsvHeader();
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

std::expected<void, VolumeError>
VolumeCalculator::exportTrackingToCsv(const std::vector<VolumeTimePoint>& timePoints,
                                       const std::filesystem::path& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return std::unexpected(VolumeError{
            VolumeError::Code::ExportFailed,
            "Failed to open file: " + filePath.string()
        });
    }

    // Write header
    file << "StudyDate,StudyDescription,LabelID,LabelName,VoxelCount,"
         << "VolumeMm3,VolumeCm3,VolumeML,ChangeMm3,ChangePercent\n";

    auto format = [](double val) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << val;
        return oss.str();
    };

    // Write data rows
    for (const auto& tp : timePoints) {
        file << tp.studyDate << ","
             << tp.studyDescription << ","
             << static_cast<int>(tp.volume.labelId) << ","
             << tp.volume.labelName << ","
             << tp.volume.voxelCount << ","
             << format(tp.volume.volumeMm3) << ","
             << format(tp.volume.volumeCm3) << ","
             << format(tp.volume.volumeML) << ",";

        if (tp.changeFromPreviousMm3.has_value()) {
            file << format(tp.changeFromPreviousMm3.value());
        }
        file << ",";

        if (tp.changePercentage.has_value()) {
            file << format(tp.changePercentage.value());
        }
        file << "\n";
    }

    return {};
}

}  // namespace dicom_viewer::services

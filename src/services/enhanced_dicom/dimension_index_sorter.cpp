#include "services/enhanced_dicom/dimension_index_sorter.hpp"
#include "services/enhanced_dicom/frame_extractor.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>

#include <gdcmDataSet.h>
#include <gdcmReader.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmTag.h>

namespace dicom_viewer::services {

namespace {

/// DICOM tags for DimensionIndexSequence parsing
const gdcm::Tag kDimensionIndexSequence{0x0020, 0x9222};
const gdcm::Tag kDimensionIndexPointer{0x0020, 0x9165};
const gdcm::Tag kFunctionalGroupPointer{0x0020, 0x9167};
const gdcm::Tag kDimensionOrganizationUID{0x0020, 0x9164};
const gdcm::Tag kDimensionDescriptionLabel{0x0020, 0x9421};

/// Get string value from a GDCM DataSet element
std::string getStringValue(const gdcm::DataSet& ds, const gdcm::Tag& tag) {
    if (!ds.FindDataElement(tag)) {
        return "";
    }
    const auto& de = ds.GetDataElement(tag);
    if (de.IsEmpty() || de.GetByteValue() == nullptr) {
        return "";
    }
    std::string value(de.GetByteValue()->GetPointer(),
                      de.GetByteValue()->GetLength());
    while (!value.empty() && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    return value;
}

/// Get DICOM tag value stored as AT (Attribute Tag) VR
uint32_t getTagValue(const gdcm::DataSet& ds, const gdcm::Tag& tag) {
    if (!ds.FindDataElement(tag)) {
        return 0;
    }
    const auto& de = ds.GetDataElement(tag);
    if (de.IsEmpty() || de.GetByteValue() == nullptr) {
        return 0;
    }
    const auto* bv = de.GetByteValue();
    if (bv->GetLength() < 4) {
        return 0;
    }

    // AT VR: stored as two uint16_t values (group, element)
    const auto* data = reinterpret_cast<const uint16_t*>(bv->GetPointer());
    uint16_t group = data[0];
    uint16_t element = data[1];
    return (static_cast<uint32_t>(group) << 16) | element;
}

/// Compute slice normal from image orientation
std::array<double, 3> computeSliceNormal(
    const std::array<double, 6>& orientation)
{
    return {
        orientation[1] * orientation[5] - orientation[2] * orientation[4],
        orientation[2] * orientation[3] - orientation[0] * orientation[5],
        orientation[0] * orientation[4] - orientation[1] * orientation[3]
    };
}

/// Project position onto normal
double projectOntoNormal(const std::array<double, 3>& position,
                         const std::array<double, 3>& normal)
{
    return position[0] * normal[0] +
           position[1] * normal[1] +
           position[2] * normal[2];
}

}  // anonymous namespace

class DimensionIndexSorter::Impl {
public:
    FrameExtractor frameExtractor;
};

DimensionIndexSorter::DimensionIndexSorter()
    : impl_(std::make_unique<Impl>()) {}

DimensionIndexSorter::~DimensionIndexSorter() = default;

DimensionIndexSorter::DimensionIndexSorter(DimensionIndexSorter&&) noexcept
    = default;
DimensionIndexSorter& DimensionIndexSorter::operator=(
    DimensionIndexSorter&&) noexcept = default;

std::expected<DimensionOrganization, EnhancedDicomError>
DimensionIndexSorter::parseDimensionIndex(const std::string& filePath)
{
    LOG_DEBUG(std::format("Parsing DimensionIndexSequence from: {}", filePath));

    gdcm::Reader reader;
    reader.SetFileName(filePath.c_str());
    if (!reader.Read()) {
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::ParseFailed,
            "Failed to read DICOM file: " + filePath
        });
    }

    const auto& ds = reader.GetFile().GetDataSet();

    if (!ds.FindDataElement(kDimensionIndexSequence)) {
        LOG_DEBUG("No DimensionIndexSequence found in file");
        return DimensionOrganization{};  // Empty = no dimension info
    }

    const auto& de = ds.GetDataElement(kDimensionIndexSequence);
    auto sq = de.GetValueAsSQ();
    if (!sq || sq->GetNumberOfItems() == 0) {
        LOG_DEBUG("DimensionIndexSequence is empty");
        return DimensionOrganization{};
    }

    DimensionOrganization org;
    int itemCount = static_cast<int>(sq->GetNumberOfItems());

    for (int i = 0; i < itemCount; ++i) {
        const auto& item = sq->GetItem(static_cast<unsigned int>(i + 1));
        const auto& itemDs = item.GetNestedDataSet();

        DimensionDefinition def;
        def.dimensionIndexPointer = getTagValue(itemDs, kDimensionIndexPointer);
        def.functionalGroupPointer =
            getTagValue(itemDs, kFunctionalGroupPointer);
        def.dimensionOrganizationUID =
            getStringValue(itemDs, kDimensionOrganizationUID);
        def.dimensionDescription =
            getStringValue(itemDs, kDimensionDescriptionLabel);

        if (def.dimensionIndexPointer != 0) {
            org.dimensions.push_back(std::move(def));
            LOG_DEBUG(std::format(
                "Dimension {}: pointer=0x{:08X}, group=0x{:08X}, desc={}",
                i, org.dimensions.back().dimensionIndexPointer,
                org.dimensions.back().functionalGroupPointer,
                org.dimensions.back().dimensionDescription));
        }
    }

    LOG_INFO(std::format("Parsed {} dimensions from DimensionIndexSequence",
                        org.dimensions.size()));
    return org;
}

std::vector<EnhancedFrameInfo> DimensionIndexSorter::sortFrames(
    const std::vector<EnhancedFrameInfo>& frames,
    const DimensionOrganization& dimOrg)
{
    if (frames.empty()) {
        return {};
    }

    // If no dimension organization, fall back to spatial sort
    if (dimOrg.dimensions.empty()) {
        LOG_DEBUG(
            "No dimension organization; falling back to spatial sort");
        return sortFramesBySpatialPosition(frames);
    }

    // Build dimension pointer list for lexicographic comparison
    std::vector<uint32_t> dimPointers;
    dimPointers.reserve(dimOrg.dimensions.size());
    for (const auto& dim : dimOrg.dimensions) {
        dimPointers.push_back(dim.dimensionIndexPointer);
    }

    auto sorted = frames;
    std::sort(sorted.begin(), sorted.end(),
        [&dimPointers](const EnhancedFrameInfo& a,
                       const EnhancedFrameInfo& b) {
            // Lexicographic comparison across dimension indices
            for (uint32_t ptr : dimPointers) {
                int valA = 0;
                int valB = 0;

                auto itA = a.dimensionIndices.find(ptr);
                if (itA != a.dimensionIndices.end()) {
                    valA = itA->second;
                }

                auto itB = b.dimensionIndices.find(ptr);
                if (itB != b.dimensionIndices.end()) {
                    valB = itB->second;
                }

                if (valA != valB) {
                    return valA < valB;
                }
            }

            // If all dimension indices are equal, preserve original order
            return a.frameIndex < b.frameIndex;
        });

    LOG_DEBUG(std::format("Sorted {} frames by {} dimensions",
                         sorted.size(), dimPointers.size()));
    return sorted;
}

std::vector<EnhancedFrameInfo> DimensionIndexSorter::sortFramesBySpatialPosition(
    const std::vector<EnhancedFrameInfo>& frames)
{
    if (frames.empty()) {
        return {};
    }

    // Compute slice normal from first frame's orientation
    auto normal = computeSliceNormal(frames[0].imageOrientation);

    auto sorted = frames;
    std::sort(sorted.begin(), sorted.end(),
        [&normal](const EnhancedFrameInfo& a, const EnhancedFrameInfo& b) {
            double projA = projectOntoNormal(a.imagePosition, normal);
            double projB = projectOntoNormal(b.imagePosition, normal);
            if (std::abs(projA - projB) > 1e-6) {
                return projA < projB;
            }
            return a.frameIndex < b.frameIndex;
        });

    LOG_DEBUG(std::format("Sorted {} frames by spatial position",
                         sorted.size()));
    return sorted;
}

std::map<int, std::vector<EnhancedFrameInfo>>
DimensionIndexSorter::groupByDimension(
    const std::vector<EnhancedFrameInfo>& frames,
    uint32_t dimensionPointer)
{
    std::map<int, std::vector<EnhancedFrameInfo>> groups;

    for (const auto& frame : frames) {
        int value = 0;
        auto it = frame.dimensionIndices.find(dimensionPointer);
        if (it != frame.dimensionIndices.end()) {
            value = it->second;
        }
        groups[value].push_back(frame);
    }

    LOG_DEBUG(std::format("Grouped {} frames into {} groups by dimension 0x{:08X}",
                         frames.size(), groups.size(), dimensionPointer));
    return groups;
}

std::expected<
    std::map<int, itk::Image<short, 3>::Pointer>,
    EnhancedDicomError>
DimensionIndexSorter::reconstructVolumes(
    const EnhancedSeriesInfo& info,
    const DimensionOrganization& dimOrg)
{
    LOG_INFO(std::format("Reconstructing volumes from {} frames with {} dimensions",
                        info.frames.size(), dimOrg.dimensions.size()));

    if (info.frames.empty()) {
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::InvalidInput,
            "No frames available for volume reconstruction"
        });
    }

    // Sort frames by dimension indices first
    auto sortedFrames = sortFrames(info.frames, dimOrg);

    // If only one dimension (spatial only), assemble single volume
    if (dimOrg.dimensions.size() <= 1) {
        std::vector<int> indices;
        indices.reserve(sortedFrames.size());
        for (const auto& frame : sortedFrames) {
            indices.push_back(frame.frameIndex);
        }

        auto volumeResult = impl_->frameExtractor.assembleVolumeFromFrames(
            info.filePath, info, indices);
        if (!volumeResult) {
            return std::unexpected(volumeResult.error());
        }

        std::map<int, itk::Image<short, 3>::Pointer> result;
        result[0] = volumeResult.value();
        return result;
    }

    // Multi-dimensional: group by outermost dimension, assemble each group
    uint32_t outerDimPointer = dimOrg.dimensions[0].dimensionIndexPointer;
    auto groups = groupByDimension(sortedFrames, outerDimPointer);

    std::map<int, itk::Image<short, 3>::Pointer> result;

    for (auto& [groupValue, groupFrames] : groups) {
        // Sort within-group frames spatially (innermost dimensions)
        DimensionOrganization innerOrg;
        for (size_t i = 1; i < dimOrg.dimensions.size(); ++i) {
            innerOrg.dimensions.push_back(dimOrg.dimensions[i]);
        }

        auto sortedGroup = sortFrames(groupFrames, innerOrg);

        std::vector<int> indices;
        indices.reserve(sortedGroup.size());
        for (const auto& frame : sortedGroup) {
            indices.push_back(frame.frameIndex);
        }

        auto volumeResult = impl_->frameExtractor.assembleVolumeFromFrames(
            info.filePath, info, indices);
        if (!volumeResult) {
            LOG_ERROR(std::format(
                "Failed to assemble volume for dimension group {}: {}",
                groupValue, volumeResult.error().toString()));
            return std::unexpected(volumeResult.error());
        }

        result[groupValue] = volumeResult.value();
        LOG_DEBUG(std::format("Assembled volume for group {}: {} frames",
                             groupValue, indices.size()));
    }

    LOG_INFO(std::format("Reconstructed {} volumes from {} dimension groups",
                        result.size(), groups.size()));
    return result;
}

}  // namespace dicom_viewer::services

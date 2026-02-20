#include "services/enhanced_dicom/functional_group_parser.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <algorithm>
#include <format>
#include <sstream>
#include <string>

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmReader.h>
#include <gdcmSequenceOfItems.h>
#include <gdcmTag.h>

namespace dicom_viewer::services {

namespace {

/// DICOM tags for functional group sequences
const gdcm::Tag kSharedFunctionalGroups{0x5200, 0x9229};
const gdcm::Tag kPerFrameFunctionalGroups{0x5200, 0x9230};

// Functional group macro tags
const gdcm::Tag kPlanePositionSequence{0x0020, 0x9113};
const gdcm::Tag kPlaneOrientationSequence{0x0020, 0x9116};
const gdcm::Tag kPixelMeasuresSequence{0x0028, 0x9110};
const gdcm::Tag kPixelValueTransformationSequence{0x0028, 0x9145};
const gdcm::Tag kFrameContentSequence{0x0020, 0x9111};

// Data element tags within functional groups
const gdcm::Tag kImagePositionPatient{0x0020, 0x0032};
const gdcm::Tag kImageOrientationPatient{0x0020, 0x0037};
const gdcm::Tag kPixelSpacing{0x0028, 0x0030};
const gdcm::Tag kSliceThickness{0x0018, 0x0050};
const gdcm::Tag kRescaleIntercept{0x0028, 0x1052};
const gdcm::Tag kRescaleSlope{0x0028, 0x1053};
const gdcm::Tag kDimensionIndexValues{0x0020, 0x9157};
const gdcm::Tag kTemporalPositionIndex{0x0020, 0x9128};
const gdcm::Tag kTriggerTime{0x0018, 0x1060};
const gdcm::Tag kStackId{0x0020, 0x9056};
const gdcm::Tag kInStackPositionNumber{0x0020, 0x9057};

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
    // Trim trailing whitespace/null
    while (!value.empty() && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    return value;
}

/// Parse a backslash-separated multi-value string into doubles
std::vector<double> parseDoubleValues(const std::string& str) {
    std::vector<double> values;
    if (str.empty()) {
        return values;
    }
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, '\\')) {
        try {
            values.push_back(std::stod(token));
        } catch (...) {
            values.push_back(0.0);
        }
    }
    return values;
}

/// Get the first item's DataSet from a sequence element
const gdcm::DataSet* getFirstSequenceItem(const gdcm::DataSet& ds,
                                           const gdcm::Tag& seqTag) {
    if (!ds.FindDataElement(seqTag)) {
        return nullptr;
    }
    const auto& de = ds.GetDataElement(seqTag);
    auto sq = de.GetValueAsSQ();
    if (!sq || sq->GetNumberOfItems() == 0) {
        return nullptr;
    }
    return &sq->GetItem(1).GetNestedDataSet();
}

/// Parse integer values from DimensionIndexValues (0020,9157)
std::vector<int> parseDimensionIndexValues(const gdcm::DataSet& ds) {
    std::vector<int> indices;
    if (!ds.FindDataElement(kDimensionIndexValues)) {
        return indices;
    }
    const auto& de = ds.GetDataElement(kDimensionIndexValues);
    if (de.IsEmpty() || de.GetByteValue() == nullptr) {
        return indices;
    }
    const auto* bv = de.GetByteValue();
    size_t count = bv->GetLength() / sizeof(uint32_t);
    const auto* data = reinterpret_cast<const uint32_t*>(bv->GetPointer());
    for (size_t i = 0; i < count; ++i) {
        indices.push_back(static_cast<int>(data[i]));
    }
    return indices;
}

}  // anonymous namespace

class FunctionalGroupParser::Impl {
public:
};

FunctionalGroupParser::FunctionalGroupParser()
    : impl_(std::make_unique<Impl>()) {}

FunctionalGroupParser::~FunctionalGroupParser() = default;

FunctionalGroupParser::FunctionalGroupParser(FunctionalGroupParser&&) noexcept
    = default;
FunctionalGroupParser& FunctionalGroupParser::operator=(
    FunctionalGroupParser&&) noexcept = default;

void FunctionalGroupParser::parseSharedGroups(const std::string& filePath,
                                              EnhancedSeriesInfo& info)
{
    LOG_DEBUG(std::format("Parsing shared functional groups from: {}", filePath));

    gdcm::Reader reader;
    reader.SetFileName(filePath.c_str());
    if (!reader.Read()) {
        LOG_WARNING(std::format("Failed to read DICOM file for shared groups: {}",
                            filePath));
        return;
    }

    const auto& ds = reader.GetFile().GetDataSet();

    // Get SharedFunctionalGroupsSequence (5200,9229)
    const auto* sharedItem = getFirstSequenceItem(ds, kSharedFunctionalGroups);
    if (sharedItem == nullptr) {
        LOG_DEBUG("No SharedFunctionalGroupsSequence found");
        return;
    }

    // Parse PixelMeasuresSequence → pixel spacing, slice thickness
    if (const auto* pixelMeasures =
            getFirstSequenceItem(*sharedItem, kPixelMeasuresSequence))
    {
        std::string spacingStr = getStringValue(*pixelMeasures, kPixelSpacing);
        auto spacing = parseDoubleValues(spacingStr);
        if (spacing.size() >= 2) {
            info.pixelSpacingX = spacing[0];
            info.pixelSpacingY = spacing[1];
        }

        std::string thicknessStr =
            getStringValue(*pixelMeasures, kSliceThickness);
        if (!thicknessStr.empty()) {
            try {
                double thickness = std::stod(thicknessStr);
                // Apply to all frames as default
                for (auto& frame : info.frames) {
                    frame.sliceThickness = thickness;
                }
            } catch (...) {}
        }

        LOG_DEBUG(std::format("Shared pixel spacing: {}x{}", info.pixelSpacingX,
                             info.pixelSpacingY));
    }

    // Parse PixelValueTransformationSequence → rescale slope/intercept
    if (const auto* pvt =
            getFirstSequenceItem(*sharedItem,
                                 kPixelValueTransformationSequence))
    {
        std::string slopeStr = getStringValue(*pvt, kRescaleSlope);
        std::string interceptStr = getStringValue(*pvt, kRescaleIntercept);

        double slope = 1.0;
        double intercept = 0.0;
        if (!slopeStr.empty()) {
            try { slope = std::stod(slopeStr); } catch (...) {}
        }
        if (!interceptStr.empty()) {
            try { intercept = std::stod(interceptStr); } catch (...) {}
        }

        // Apply shared rescale to all frames as default
        for (auto& frame : info.frames) {
            frame.rescaleSlope = slope;
            frame.rescaleIntercept = intercept;
        }
    }

    // Parse PlaneOrientationSequence → shared orientation
    if (const auto* planeOrientation =
            getFirstSequenceItem(*sharedItem, kPlaneOrientationSequence))
    {
        std::string orientStr =
            getStringValue(*planeOrientation, kImageOrientationPatient);
        auto orientValues = parseDoubleValues(orientStr);
        if (orientValues.size() >= 6) {
            for (auto& frame : info.frames) {
                for (size_t i = 0; i < 6; ++i) {
                    frame.imageOrientation[i] = orientValues[i];
                }
            }
        }
    }

    LOG_DEBUG("Shared functional groups parsed successfully");
}

std::vector<EnhancedFrameInfo> FunctionalGroupParser::parsePerFrameGroups(
    const std::string& filePath,
    int numberOfFrames,
    const EnhancedSeriesInfo& sharedInfo)
{
    LOG_DEBUG(std::format("Parsing per-frame functional groups ({} frames)",
                         numberOfFrames));

    std::vector<EnhancedFrameInfo> frames(numberOfFrames);

    // Initialize with shared defaults
    for (int i = 0; i < numberOfFrames; ++i) {
        frames[i].frameIndex = i;
        frames[i].rescaleSlope = 1.0;
        frames[i].rescaleIntercept = 0.0;
    }

    gdcm::Reader reader;
    reader.SetFileName(filePath.c_str());
    if (!reader.Read()) {
        LOG_ERROR(std::format(
            "Failed to read DICOM file for per-frame groups: {}", filePath));
        return frames;
    }

    const auto& ds = reader.GetFile().GetDataSet();

    // Get PerFrameFunctionalGroupsSequence (5200,9230)
    if (!ds.FindDataElement(kPerFrameFunctionalGroups)) {
        LOG_WARNING("No PerFrameFunctionalGroupsSequence found");
        return frames;
    }

    const auto& de = ds.GetDataElement(kPerFrameFunctionalGroups);
    auto sq = de.GetValueAsSQ();
    if (!sq) {
        LOG_WARNING(
            "PerFrameFunctionalGroupsSequence is not a valid sequence");
        return frames;
    }

    int itemCount =
        std::min(static_cast<int>(sq->GetNumberOfItems()), numberOfFrames);

    for (int i = 0; i < itemCount; ++i) {
        // GDCM sequences are 1-indexed
        const auto& item = sq->GetItem(static_cast<unsigned int>(i + 1));
        const auto& itemDs = item.GetNestedDataSet();

        auto& frame = frames[i];

        // Parse PlanePositionSequence → image position
        if (const auto* planePos =
                getFirstSequenceItem(itemDs, kPlanePositionSequence))
        {
            std::string posStr =
                getStringValue(*planePos, kImagePositionPatient);
            auto posValues = parseDoubleValues(posStr);
            if (posValues.size() >= 3) {
                frame.imagePosition[0] = posValues[0];
                frame.imagePosition[1] = posValues[1];
                frame.imagePosition[2] = posValues[2];
            }
        }

        // Parse PlaneOrientationSequence → image orientation (per-frame override)
        if (const auto* planeOrient =
                getFirstSequenceItem(itemDs, kPlaneOrientationSequence))
        {
            std::string orientStr =
                getStringValue(*planeOrient, kImageOrientationPatient);
            auto orientValues = parseDoubleValues(orientStr);
            if (orientValues.size() >= 6) {
                for (size_t j = 0; j < 6; ++j) {
                    frame.imageOrientation[j] = orientValues[j];
                }
            }
        }

        // Parse PixelValueTransformationSequence (per-frame override)
        if (const auto* pvt = getFirstSequenceItem(
                itemDs, kPixelValueTransformationSequence))
        {
            std::string slopeStr = getStringValue(*pvt, kRescaleSlope);
            std::string interceptStr = getStringValue(*pvt, kRescaleIntercept);
            if (!slopeStr.empty()) {
                try { frame.rescaleSlope = std::stod(slopeStr); } catch (...) {}
            }
            if (!interceptStr.empty()) {
                try {
                    frame.rescaleIntercept = std::stod(interceptStr);
                } catch (...) {}
            }
        }

        // Parse FrameContentSequence → dimension indices, temporal info
        if (const auto* frameContent =
                getFirstSequenceItem(itemDs, kFrameContentSequence))
        {
            // DimensionIndexValues (0020,9157) — unsigned long array
            auto dimValues = parseDimensionIndexValues(*frameContent);
            // Store as generic map with ordinal key (index position)
            for (size_t j = 0; j < dimValues.size(); ++j) {
                frame.dimensionIndices[static_cast<uint32_t>(j)] = dimValues[j];
            }

            // TemporalPositionIndex (0020,9128)
            std::string tempIdx =
                getStringValue(*frameContent, kTemporalPositionIndex);
            if (!tempIdx.empty()) {
                try {
                    frame.temporalPositionIndex = std::stoi(tempIdx);
                } catch (...) {}
            }

            // InStackPositionNumber (0020,9057)
            std::string inStackStr =
                getStringValue(*frameContent, kInStackPositionNumber);
            if (!inStackStr.empty()) {
                try {
                    // Store as a well-known dimension index
                    frame.dimensionIndices[kInStackPositionNumber.GetElementTag()]
                        = std::stoi(inStackStr);
                } catch (...) {}
            }
        }

        // Trigger Time (0018,1060) — may be at frame level outside sequences
        {
            std::string triggerStr = getStringValue(itemDs, kTriggerTime);
            if (!triggerStr.empty()) {
                try {
                    frame.triggerTime = std::stod(triggerStr);
                } catch (...) {}
            }
        }
    }

    LOG_DEBUG(std::format("Parsed per-frame groups for {} frames", itemCount));
    return frames;
}

}  // namespace dicom_viewer::services

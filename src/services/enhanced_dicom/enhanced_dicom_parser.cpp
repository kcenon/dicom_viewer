#include "services/enhanced_dicom/enhanced_dicom_parser.hpp"
#include "services/enhanced_dicom/dimension_index_sorter.hpp"
#include "services/enhanced_dicom/frame_extractor.hpp"
#include "services/enhanced_dicom/functional_group_parser.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <sstream>

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmReader.h>
#include <gdcmTag.h>
#include <gdcmVR.h>

namespace dicom_viewer::services {

namespace {

const gdcm::Tag kSOPClassUID{0x0008, 0x0016};
const gdcm::Tag kSOPInstanceUID{0x0008, 0x0018};
const gdcm::Tag kNumberOfFrames{0x0028, 0x0008};
const gdcm::Tag kRows{0x0028, 0x0010};
const gdcm::Tag kColumns{0x0028, 0x0011};
const gdcm::Tag kBitsAllocated{0x0028, 0x0100};
const gdcm::Tag kBitsStored{0x0028, 0x0101};
const gdcm::Tag kHighBit{0x0028, 0x0102};
const gdcm::Tag kPixelRepresentation{0x0028, 0x0103};
const gdcm::Tag kTransferSyntaxUID{0x0002, 0x0010};
const gdcm::Tag kModality{0x0008, 0x0060};
const gdcm::Tag kPatientId{0x0010, 0x0020};
const gdcm::Tag kPatientName{0x0010, 0x0010};
const gdcm::Tag kStudyInstanceUID{0x0020, 0x000d};
const gdcm::Tag kSeriesInstanceUID{0x0020, 0x000e};
const gdcm::Tag kSeriesDescription{0x0008, 0x103e};

/// Get string value from top-level GDCM DataSet
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

/// Get integer attribute value (handles both IS string and US/SS binary VR)
int getIntValue(const gdcm::DataSet& ds, const gdcm::Tag& tag,
                int defaultValue = 0)
{
    if (!ds.FindDataElement(tag)) {
        return defaultValue;
    }
    const auto& de = ds.GetDataElement(tag);
    if (de.IsEmpty() || de.GetByteValue() == nullptr) {
        return defaultValue;
    }
    const auto* bv = de.GetByteValue();
    // Check VR for binary integer types
    gdcm::VR vr = de.GetVR();
    if (vr == gdcm::VR::US && bv->GetLength() >= sizeof(uint16_t)) {
        uint16_t val = 0;
        std::memcpy(&val, bv->GetPointer(), sizeof(uint16_t));
        return static_cast<int>(val);
    }
    if (vr == gdcm::VR::SS && bv->GetLength() >= sizeof(int16_t)) {
        int16_t val = 0;
        std::memcpy(&val, bv->GetPointer(), sizeof(int16_t));
        return static_cast<int>(val);
    }
    // IS VR or other string-based integer representations
    std::string str(bv->GetPointer(), bv->GetLength());
    while (!str.empty() && (str.back() == ' ' || str.back() == '\0')) {
        str.pop_back();
    }
    if (str.empty()) {
        return defaultValue;
    }
    try {
        return std::stoi(str);
    } catch (...) {
        return defaultValue;
    }
}

}  // anonymous namespace

class EnhancedDicomParser::Impl {
public:
    FunctionalGroupParser groupParser;
    FrameExtractor frameExtractor;
    DimensionIndexSorter dimensionSorter;
    DimensionOrganization dimOrg;
    ProgressCallback progressCallback;

    void reportProgress(double progress) {
        if (progressCallback) {
            progressCallback(progress);
        }
    }
};

EnhancedDicomParser::EnhancedDicomParser()
    : impl_(std::make_unique<Impl>()) {}

EnhancedDicomParser::~EnhancedDicomParser() = default;

EnhancedDicomParser::EnhancedDicomParser(EnhancedDicomParser&&) noexcept
    = default;
EnhancedDicomParser& EnhancedDicomParser::operator=(
    EnhancedDicomParser&&) noexcept = default;

void EnhancedDicomParser::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

bool EnhancedDicomParser::isEnhancedDicom(const std::string& filePath) {
    gdcm::Reader reader;
    reader.SetFileName(filePath.c_str());
    if (!reader.Read()) {
        return false;
    }

    const auto& ds = reader.GetFile().GetDataSet();
    std::string sopClassUid = getStringValue(ds, kSOPClassUID);
    return isEnhancedSopClass(sopClassUid);
}

bool EnhancedDicomParser::detectEnhancedIOD(const std::string& sopClassUid) {
    return isEnhancedSopClass(sopClassUid);
}

std::expected<EnhancedSeriesInfo, EnhancedDicomError>
EnhancedDicomParser::parseFile(const std::string& filePath)
{
    LOG_INFO(std::format("Parsing Enhanced DICOM file: {}", filePath));
    impl_->reportProgress(0.0);

    // Step 1: Read file header and top-level attributes
    gdcm::Reader reader;
    reader.SetFileName(filePath.c_str());
    if (!reader.Read()) {
        LOG_ERROR(std::format("Failed to read DICOM file: {}", filePath));
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::ParseFailed,
            "Failed to read DICOM file: " + filePath
        });
    }

    const auto& file = reader.GetFile();
    const auto& ds = file.GetDataSet();
    const auto& header = file.GetHeader();

    // Verify SOP Class UID
    std::string sopClassUid = getStringValue(ds, kSOPClassUID);
    if (!isEnhancedSopClass(sopClassUid)) {
        LOG_WARNING(std::format("Not an Enhanced IOD: {}", sopClassUid));
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::NotEnhancedIOD,
            "SOP Class UID " + sopClassUid + " is not an Enhanced IOD"
        });
    }

    EnhancedSeriesInfo info;
    info.sopClassUid = sopClassUid;
    info.sopInstanceUid = getStringValue(ds, kSOPInstanceUID);
    info.filePath = filePath;

    // Number of frames
    info.numberOfFrames = getIntValue(ds, kNumberOfFrames, 1);
    if (info.numberOfFrames <= 0) {
        return std::unexpected(EnhancedDicomError{
            EnhancedDicomError::Code::InconsistentData,
            "Invalid NumberOfFrames: "
                + std::to_string(info.numberOfFrames)
        });
    }

    // Image dimensions
    info.rows = getIntValue(ds, kRows);
    info.columns = getIntValue(ds, kColumns);
    info.bitsAllocated = getIntValue(ds, kBitsAllocated, 16);
    info.bitsStored = getIntValue(ds, kBitsStored, info.bitsAllocated);
    info.highBit = getIntValue(ds, kHighBit, info.bitsStored - 1);
    info.pixelRepresentation = getIntValue(ds, kPixelRepresentation, 0);

    // Patient/Study/Series metadata
    info.modality = getStringValue(ds, kModality);
    info.patientId = getStringValue(ds, kPatientId);
    info.patientName = getStringValue(ds, kPatientName);
    info.studyInstanceUid = getStringValue(ds, kStudyInstanceUID);
    info.seriesInstanceUid = getStringValue(ds, kSeriesInstanceUID);
    info.seriesDescription = getStringValue(ds, kSeriesDescription);

    // Transfer syntax from file meta information
    info.transferSyntaxUid = getStringValue(header, kTransferSyntaxUID);

    LOG_INFO(std::format(
        "Enhanced {} ({}): {}x{}, {} frames, {} bits",
        enhancedSopClassName(sopClassUid), info.modality,
        info.columns, info.rows, info.numberOfFrames, info.bitsAllocated));

    impl_->reportProgress(0.2);

    // Step 2: Parse per-frame functional groups
    info.frames = impl_->groupParser.parsePerFrameGroups(
        filePath, info.numberOfFrames, info);

    impl_->reportProgress(0.6);

    // Step 3: Parse shared functional groups (overrides per-frame defaults)
    impl_->groupParser.parseSharedGroups(filePath, info);

    impl_->reportProgress(0.7);

    // Step 4: Parse DimensionIndexSequence and sort frames
    auto dimResult = impl_->dimensionSorter.parseDimensionIndex(filePath);
    if (dimResult) {
        impl_->dimOrg = dimResult.value();
        if (!impl_->dimOrg.dimensions.empty()) {
            info.frames = impl_->dimensionSorter.sortFrames(
                info.frames, impl_->dimOrg);
            LOG_INFO(std::format(
                "Frames sorted by {} dimensions from DimensionIndexSequence",
                impl_->dimOrg.dimensions.size()));
        }
    }

    impl_->reportProgress(0.9);

    LOG_INFO(std::format("Enhanced DICOM parsed: {} frames with metadata",
                        info.frames.size()));
    impl_->reportProgress(1.0);

    return info;
}

std::expected<itk::Image<short, 3>::Pointer, EnhancedDicomError>
EnhancedDicomParser::assembleVolume(const EnhancedSeriesInfo& info)
{
    return impl_->frameExtractor.assembleVolume(info.filePath, info);
}

std::expected<itk::Image<short, 3>::Pointer, EnhancedDicomError>
EnhancedDicomParser::assembleVolume(const EnhancedSeriesInfo& info,
                                    const std::vector<int>& frameIndices)
{
    return impl_->frameExtractor.assembleVolumeFromFrames(
        info.filePath, info, frameIndices);
}

const DimensionOrganization& EnhancedDicomParser::getDimensionOrganization() const
{
    return impl_->dimOrg;
}

std::expected<
    std::map<int, itk::Image<short, 3>::Pointer>,
    EnhancedDicomError>
EnhancedDicomParser::reconstructMultiPhaseVolumes(
    const EnhancedSeriesInfo& info)
{
    return impl_->dimensionSorter.reconstructVolumes(info, impl_->dimOrg);
}

}  // namespace dicom_viewer::services

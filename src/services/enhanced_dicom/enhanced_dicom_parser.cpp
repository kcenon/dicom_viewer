#include "services/enhanced_dicom/enhanced_dicom_parser.hpp"
#include "services/enhanced_dicom/frame_extractor.hpp"
#include "services/enhanced_dicom/functional_group_parser.hpp"
#include "core/logging.hpp"

#include <algorithm>
#include <sstream>

#include <gdcmAttribute.h>
#include <gdcmDataSet.h>
#include <gdcmReader.h>
#include <gdcmTag.h>

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

/// Get integer attribute value
int getIntValue(const gdcm::DataSet& ds, const gdcm::Tag& tag,
                int defaultValue = 0)
{
    std::string str = getStringValue(ds, tag);
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
    std::shared_ptr<spdlog::logger> logger;
    FunctionalGroupParser groupParser;
    FrameExtractor frameExtractor;
    ProgressCallback progressCallback;

    Impl() : logger(logging::LoggerFactory::create("EnhancedDicomParser")) {}

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
    impl_->logger->info("Parsing Enhanced DICOM file: {}", filePath);
    impl_->reportProgress(0.0);

    // Step 1: Read file header and top-level attributes
    gdcm::Reader reader;
    reader.SetFileName(filePath.c_str());
    if (!reader.Read()) {
        impl_->logger->error("Failed to read DICOM file: {}", filePath);
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
        impl_->logger->warn("Not an Enhanced IOD: {}", sopClassUid);
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

    impl_->logger->info(
        "Enhanced {} ({}): {}x{}, {} frames, {} bits",
        enhancedSopClassName(sopClassUid), info.modality,
        info.columns, info.rows, info.numberOfFrames, info.bitsAllocated);

    impl_->reportProgress(0.2);

    // Step 2: Parse per-frame functional groups
    info.frames = impl_->groupParser.parsePerFrameGroups(
        filePath, info.numberOfFrames, info);

    impl_->reportProgress(0.6);

    // Step 3: Parse shared functional groups (overrides per-frame defaults)
    impl_->groupParser.parseSharedGroups(filePath, info);

    impl_->reportProgress(0.8);

    impl_->logger->info("Enhanced DICOM parsed: {} frames with metadata",
                        info.frames.size());
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

}  // namespace dicom_viewer::services

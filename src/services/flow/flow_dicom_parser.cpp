#include "services/flow/flow_dicom_parser.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <set>

#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkMetaDataObject.h>

#include <kcenon/common/logging/log_macros.h>

#include "services/flow/vendor_parsers/ge_flow_parser.hpp"
#include "services/flow/vendor_parsers/philips_flow_parser.hpp"
#include "services/flow/vendor_parsers/siemens_flow_parser.hpp"

namespace {

std::string getMetaString(const itk::MetaDataDictionary& dict,
                          const std::string& key) {
    std::string value;
    itk::ExposeMetaData<std::string>(dict, key, value);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    return value;
}

itk::MetaDataDictionary readDicomMetadata(const std::string& filePath) {
    auto gdcmIO = itk::GDCMImageIO::New();
    gdcmIO->SetFileName(filePath.c_str());
    gdcmIO->ReadImageInformation();
    return gdcmIO->GetMetaDataDictionary();
}

}  // anonymous namespace

namespace dicom_viewer::services {

class FlowDicomParser::Impl {
public:
    FlowDicomParser::ProgressCallback progressCallback;
    std::map<FlowVendorType, std::unique_ptr<IVendorFlowParser>> vendorParsers;

    Impl() {
        vendorParsers[FlowVendorType::Siemens] =
            std::make_unique<SiemensFlowParser>();
        vendorParsers[FlowVendorType::Philips] =
            std::make_unique<PhilipsFlowParser>();
        vendorParsers[FlowVendorType::GE] =
            std::make_unique<GEFlowParser>();
    }

    IVendorFlowParser* selectParser(FlowVendorType vendor) {
        auto it = vendorParsers.find(vendor);
        if (it != vendorParsers.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    void reportProgress(double progress) const {
        if (progressCallback) {
            progressCallback(progress);
        }
    }
};

FlowDicomParser::FlowDicomParser()
    : impl_(std::make_unique<Impl>()) {}

FlowDicomParser::~FlowDicomParser() = default;

FlowDicomParser::FlowDicomParser(FlowDicomParser&&) noexcept = default;
FlowDicomParser& FlowDicomParser::operator=(FlowDicomParser&&) noexcept = default;

void FlowDicomParser::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

bool FlowDicomParser::is4DFlowSeries(
    const std::vector<std::string>& dicomFiles) {
    if (dicomFiles.empty()) {
        return false;
    }

    try {
        auto dict = readDicomMetadata(dicomFiles.front());

        // Check (0018,0020) Scanning Sequence for "PC" (Phase Contrast)
        auto scanSeq = getMetaString(dict, "0018|0020");
        std::transform(scanSeq.begin(), scanSeq.end(), scanSeq.begin(),
                       ::toupper);
        bool hasPC = scanSeq.find("PC") != std::string::npos;

        // Check (0018,9014) Phase Contrast = "YES"
        auto phaseContrast = getMetaString(dict, "0018|9014");
        std::transform(phaseContrast.begin(), phaseContrast.end(),
                       phaseContrast.begin(), ::toupper);
        bool hasPhaseContrast = phaseContrast.find("YES") != std::string::npos;

        // Check (0008,0008) Image Type for "P" (phase) or "VELOCITY"
        auto imageType = getMetaString(dict, "0008|0008");
        std::transform(imageType.begin(), imageType.end(), imageType.begin(),
                       ::toupper);
        bool hasVelocity = imageType.find("VELOCITY") != std::string::npos ||
                           imageType.find("\\P\\") != std::string::npos;

        // Check (0018,9197) Velocity Encoding presence
        auto venc = getMetaString(dict, "0018|9197");
        bool hasVENC = !venc.empty();

        // At least two indicators should be present
        int indicators = (hasPC ? 1 : 0) + (hasPhaseContrast ? 1 : 0) +
                         (hasVelocity ? 1 : 0) + (hasVENC ? 1 : 0);
        return indicators >= 2;

    } catch (const itk::ExceptionObject& e) {
        LOG_WARNING(std::format("Failed to read DICOM metadata: {}", e.GetDescription()));
        return false;
    } catch (...) {
        return false;
    }
}

FlowVendorType FlowDicomParser::detectVendor(
    const std::vector<std::string>& dicomFiles) {
    if (dicomFiles.empty()) {
        return FlowVendorType::Unknown;
    }

    try {
        auto dict = readDicomMetadata(dicomFiles.front());

        // (0008,0070) Manufacturer
        auto manufacturer = getMetaString(dict, "0008|0070");
        std::transform(manufacturer.begin(), manufacturer.end(),
                       manufacturer.begin(), ::toupper);

        if (manufacturer.find("SIEMENS") != std::string::npos) {
            return FlowVendorType::Siemens;
        }
        if (manufacturer.find("PHILIPS") != std::string::npos) {
            return FlowVendorType::Philips;
        }
        if (manufacturer.find("GE") != std::string::npos) {
            return FlowVendorType::GE;
        }

        return FlowVendorType::Unknown;

    } catch (...) {
        return FlowVendorType::Unknown;
    }
}

std::expected<FlowSeriesInfo, FlowError>
FlowDicomParser::parseSeries(const std::vector<std::string>& dicomFiles) const {
    if (dicomFiles.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::InvalidInput,
            "No DICOM files provided"});
    }

    impl_->reportProgress(0.0);

    // Step 1: Detect vendor
    auto vendor = detectVendor(dicomFiles);
    if (vendor == FlowVendorType::Unknown) {
        return std::unexpected(FlowError{
            FlowError::Code::UnsupportedVendor,
            "Could not detect scanner vendor from DICOM metadata"});
    }

    auto* parser = impl_->selectParser(vendor);
    if (!parser) {
        return std::unexpected(FlowError{
            FlowError::Code::UnsupportedVendor,
            "No parser available for vendor: " + vendorToString(vendor)});
    }

    LOG_INFO(std::format("Detected vendor: {}, parsing {} files",
                         vendorToString(vendor), dicomFiles.size()));

    impl_->reportProgress(0.1);

    // Step 2: Read all frame metadata
    std::vector<FlowFrame> frames;
    frames.reserve(dicomFiles.size());

    std::set<double> triggerTimes;
    double maxVenc = 0.0;

    for (size_t i = 0; i < dicomFiles.size(); ++i) {
        try {
            auto dict = readDicomMetadata(dicomFiles[i]);

            FlowFrame frame;
            frame.filePath = dicomFiles[i];
            frame.sopInstanceUid = getMetaString(dict, "0008|0018");
            frame.component = parser->classifyComponent(dict);
            frame.venc = parser->extractVENC(dict);
            frame.cardiacPhase = parser->extractPhaseIndex(dict);
            frame.triggerTime = parser->extractTriggerTime(dict);

            triggerTimes.insert(frame.triggerTime);
            if (frame.venc > maxVenc) {
                maxVenc = frame.venc;
            }

            frames.push_back(std::move(frame));

        } catch (const itk::ExceptionObject& e) {
            LOG_WARNING(std::format("Failed to parse frame {}: {}", dicomFiles[i],
                                    e.GetDescription()));
        }

        // Report progress (10% - 70%)
        impl_->reportProgress(0.1 + 0.6 * static_cast<double>(i + 1) /
                                        static_cast<double>(dicomFiles.size()));
    }

    if (frames.empty()) {
        return std::unexpected(FlowError{
            FlowError::Code::ParseFailed,
            "No valid 4D Flow frames found"});
    }

    // Step 3: Determine phase count from trigger times
    // Normalize phase indices to 0-based
    std::set<int> uniquePhases;
    for (const auto& f : frames) {
        uniquePhases.insert(f.cardiacPhase);
    }

    // Create phase index mapping
    std::map<int, int> phaseMap;
    int idx = 0;
    for (int rawPhase : uniquePhases) {
        phaseMap[rawPhase] = idx++;
    }

    // Remap all frames to 0-based phase indices
    for (auto& f : frames) {
        f.cardiacPhase = phaseMap[f.cardiacPhase];
    }

    int phaseCount = static_cast<int>(uniquePhases.size());

    impl_->reportProgress(0.75);

    // Step 4: Build frame matrix [phase][component] → file paths
    FlowSeriesInfo info;
    info.vendor = vendor;
    info.phaseCount = phaseCount;
    info.venc = {maxVenc, maxVenc, maxVenc};

    // Compute temporal resolution from trigger times
    if (triggerTimes.size() > 1) {
        auto it = triggerTimes.begin();
        double first = *it++;
        double second = *it;
        info.temporalResolution = second - first;
    }

    // Read patient/study metadata from first file
    try {
        auto dict = readDicomMetadata(dicomFiles.front());
        info.patientId = getMetaString(dict, "0010|0020");
        info.studyDate = getMetaString(dict, "0008|0020");
        info.seriesDescription = getMetaString(dict, "0008|103e");
        info.seriesInstanceUid = getMetaString(dict, "0020|000e");

        // Detect signed/unsigned phase encoding
        auto pixelRep = getMetaString(dict, "0028|0103");
        info.isSignedPhase = (pixelRep == "1");
    } catch (...) {
        // Metadata extraction is best-effort
    }

    info.frameMatrix.resize(phaseCount);

    for (const auto& frame : frames) {
        if (frame.cardiacPhase >= 0 && frame.cardiacPhase < phaseCount) {
            info.frameMatrix[frame.cardiacPhase][frame.component]
                .push_back(frame.filePath);
        }
    }

    // Sort file paths within each component by slice position
    for (auto& phaseMap : info.frameMatrix) {
        for (auto& [comp, files] : phaseMap) {
            std::sort(files.begin(), files.end());
        }
    }

    impl_->reportProgress(1.0);

    LOG_INFO(std::format("Parsed 4D Flow series: {} phases, VENC={:.1f} cm/s, {} frames",
                         info.phaseCount, maxVenc, frames.size()));

    return info;
}

}  // namespace dicom_viewer::services

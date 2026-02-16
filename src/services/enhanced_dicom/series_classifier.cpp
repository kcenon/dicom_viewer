#include "services/enhanced_dicom/series_classifier.hpp"

#include "core/series_builder.hpp"

#include <algorithm>
#include <string>

#include <itkGDCMImageIO.h>
#include <itkMetaDataObject.h>

namespace {

/// Extract a trimmed string value from an ITK metadata dictionary
std::string getMetaString(const itk::MetaDataDictionary& dict,
                          const std::string& key) {
    std::string value;
    itk::ExposeMetaData<std::string>(dict, key, value);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    return value;
}

/// Convert a string to upper case (in-place copy)
std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

/// Read DICOM metadata from a file via GDCMImageIO
itk::MetaDataDictionary readDicomMetadata(const std::string& filePath) {
    auto gdcmIO = itk::GDCMImageIO::New();
    gdcmIO->SetFileName(filePath.c_str());
    gdcmIO->ReadImageInformation();
    return gdcmIO->GetMetaDataDictionary();
}

// =========================================================================
// DICOM tag keys (format: "GROUP|ELEMENT")
// =========================================================================
constexpr const char* kModality           = "0008|0060";
constexpr const char* kImageType          = "0008|0008";
constexpr const char* kSeriesDescription  = "0008|103e";
constexpr const char* kSeriesInstanceUID  = "0020|000e";
constexpr const char* kScanningSequence   = "0018|0020";
constexpr const char* kPhaseContrast      = "0018|9014";
constexpr const char* kManufacturer       = "0008|0070";
constexpr const char* kNumberOfFrames     = "0028|0008";

// Vendor-specific velocity direction tags
constexpr const char* kSiemensFlowDir     = "0051|1014";
constexpr const char* kPhilipsPrivateVenc = "2001|101a";
constexpr const char* kGEPrivateVenc      = "0019|10cc";

using dicom_viewer::services::SeriesType;

/// Check if the series has phase-contrast indicators
bool hasPhaseContrastIndicators(const itk::MetaDataDictionary& dict) {
    auto scanSeq = toUpper(getMetaString(dict, kScanningSequence));
    if (scanSeq.find("PC") != std::string::npos) {
        return true;
    }

    auto pc = toUpper(getMetaString(dict, kPhaseContrast));
    if (pc.find("YES") != std::string::npos) {
        return true;
    }

    return false;
}

/// Detect velocity encoding direction from vendor-specific tags
/// Returns the specific 4D Flow phase direction, or Unknown if not a phase image
SeriesType detect4DFlowDirection(const itk::MetaDataDictionary& dict) {
    auto imageType = toUpper(getMetaString(dict, kImageType));

    // Check if this is a magnitude image
    if (imageType.find("\\M\\") != std::string::npos ||
        imageType.find("\\M_") != std::string::npos ||
        imageType.find("MAG") != std::string::npos) {
        return SeriesType::Flow4D_Magnitude;
    }

    // Check if this is a phase/velocity image
    bool isPhase = imageType.find("\\P\\") != std::string::npos ||
                   imageType.find("VELOCITY") != std::string::npos ||
                   imageType.find("PHASE") != std::string::npos;

    if (!isPhase) {
        // If no phase/magnitude indicator, check for any velocity encoding
        auto venc = getMetaString(dict, "0018|9197");
        if (venc.empty()) {
            return SeriesType::Flow4D_Magnitude;
        }
        isPhase = true;
    }

    // Determine direction from vendor-specific tags

    // Siemens: private tag (0051,1014)
    auto siemensDir = toUpper(getMetaString(dict, kSiemensFlowDir));
    if (!siemensDir.empty()) {
        if (siemensDir.find("AP") != std::string::npos) {
            return SeriesType::Flow4D_Phase_AP;
        }
        if (siemensDir.find("FH") != std::string::npos ||
            siemensDir.find("SI") != std::string::npos) {
            return SeriesType::Flow4D_Phase_FH;
        }
        if (siemensDir.find("RL") != std::string::npos) {
            return SeriesType::Flow4D_Phase_RL;
        }
    }

    // Philips: private tags (2001,xxxx) or series description
    auto philipsVenc = getMetaString(dict, kPhilipsPrivateVenc);
    if (!philipsVenc.empty()) {
        auto desc = toUpper(getMetaString(dict, kSeriesDescription));
        if (desc.find("_AP") != std::string::npos ||
            desc.find("AP_") != std::string::npos ||
            desc.find("_AP_") != std::string::npos) {
            return SeriesType::Flow4D_Phase_AP;
        }
        if (desc.find("_FH") != std::string::npos ||
            desc.find("FH_") != std::string::npos ||
            desc.find("_FH_") != std::string::npos) {
            return SeriesType::Flow4D_Phase_FH;
        }
        if (desc.find("_RL") != std::string::npos ||
            desc.find("RL_") != std::string::npos ||
            desc.find("_RL_") != std::string::npos) {
            return SeriesType::Flow4D_Phase_RL;
        }
    }

    // GE: private tag (0019,10cc) or series description
    auto geVenc = getMetaString(dict, kGEPrivateVenc);
    if (!geVenc.empty()) {
        auto desc = toUpper(getMetaString(dict, kSeriesDescription));
        if (desc.find("AP") != std::string::npos) {
            return SeriesType::Flow4D_Phase_AP;
        }
        if (desc.find("FH") != std::string::npos ||
            desc.find("SI") != std::string::npos) {
            return SeriesType::Flow4D_Phase_FH;
        }
        if (desc.find("RL") != std::string::npos) {
            return SeriesType::Flow4D_Phase_RL;
        }
    }

    // Fallback: check series description for direction hints
    auto desc = toUpper(getMetaString(dict, kSeriesDescription));
    if (desc.find("_AP") != std::string::npos || desc.find("AP_") != std::string::npos) {
        return SeriesType::Flow4D_Phase_AP;
    }
    if (desc.find("_FH") != std::string::npos || desc.find("FH_") != std::string::npos) {
        return SeriesType::Flow4D_Phase_FH;
    }
    if (desc.find("_RL") != std::string::npos || desc.find("RL_") != std::string::npos) {
        return SeriesType::Flow4D_Phase_RL;
    }

    // Phase image but direction undetermined — default to AP
    return SeriesType::Flow4D_Phase_AP;
}

/// Check if series is 2D (single frame or small number)
bool is2DSeries(const itk::MetaDataDictionary& dict) {
    auto nFrames = getMetaString(dict, kNumberOfFrames);
    if (nFrames.empty()) {
        return true;
    }
    try {
        return std::stoi(nFrames) <= 1;
    } catch (...) {
        return true;
    }
}

}  // anonymous namespace

namespace dicom_viewer::services {

ClassifiedSeries SeriesClassifier::classifyFile(const std::string& filePath) {
    try {
        auto dict = readDicomMetadata(filePath);
        return classify(dict);
    } catch (...) {
        return ClassifiedSeries{SeriesType::Unknown, "", "", "", false};
    }
}

ClassifiedSeries SeriesClassifier::classify(
    const itk::MetaDataDictionary& metadata) {

    ClassifiedSeries result;
    result.seriesUid = getMetaString(metadata, kSeriesInstanceUID);
    result.description = getMetaString(metadata, kSeriesDescription);
    result.modality = getMetaString(metadata, kModality);

    auto modalityUpper = toUpper(result.modality);
    auto descUpper = toUpper(result.description);
    auto imageType = toUpper(getMetaString(metadata, kImageType));

    // ------------------------------------------------------------------
    // Rule 1: CT — modality tag is definitive
    // ------------------------------------------------------------------
    if (modalityUpper == "CT") {
        result.type = SeriesType::CT;
        result.is4DFlow = false;
        return result;
    }

    // ------------------------------------------------------------------
    // Rule 2: DIXON — description-based (before 4D Flow check)
    // ------------------------------------------------------------------
    if (descUpper.find("DIXON") != std::string::npos) {
        result.type = SeriesType::DIXON;
        result.is4DFlow = false;
        return result;
    }

    // ------------------------------------------------------------------
    // Rule 3: StarVIBE — description-based
    // ------------------------------------------------------------------
    if (descUpper.find("STARVIBE") != std::string::npos ||
        descUpper.find("STAR_VIBE") != std::string::npos) {
        result.type = SeriesType::Starvibe;
        result.is4DFlow = false;
        return result;
    }

    // ------------------------------------------------------------------
    // Rule 4: TOF — description-based
    // ------------------------------------------------------------------
    if (descUpper.find("TOF") != std::string::npos &&
        descUpper.find("TIME OF FLIGHT") != std::string::npos
        ? true
        : descUpper.find("TOF") != std::string::npos) {
        result.type = SeriesType::TOF;
        result.is4DFlow = false;
        return result;
    }

    // ------------------------------------------------------------------
    // Rule 5: CINE — description-based (must not be phase contrast)
    // ------------------------------------------------------------------
    if (descUpper.find("CINE") != std::string::npos &&
        !hasPhaseContrastIndicators(metadata)) {
        result.type = SeriesType::CINE;
        result.is4DFlow = false;
        return result;
    }

    // ------------------------------------------------------------------
    // Rule 6: Phase contrast series (4D Flow or 2D VENC)
    // ------------------------------------------------------------------
    if (hasPhaseContrastIndicators(metadata) ||
        imageType.find("VELOCITY") != std::string::npos) {

        // 2D VENC: single-slice phase contrast
        if (is2DSeries(metadata) &&
            descUpper.find("2D") != std::string::npos) {
            result.type = SeriesType::VENC_2D;
            result.is4DFlow = false;
            return result;
        }

        // 4D Flow: determine direction
        result.type = detect4DFlowDirection(metadata);
        result.is4DFlow = true;
        return result;
    }

    // ------------------------------------------------------------------
    // Rule 7: PC-MRA — often reconstructed from 4D Flow
    // ------------------------------------------------------------------
    if (descUpper.find("MRA") != std::string::npos ||
        descUpper.find("ANGIO") != std::string::npos ||
        descUpper.find("PC-MRA") != std::string::npos ||
        descUpper.find("PCMRA") != std::string::npos) {
        result.type = SeriesType::PC_MRA;
        result.is4DFlow = false;
        return result;
    }

    // ------------------------------------------------------------------
    // Rule 8: CINE fallback (description contains CINE even with PC)
    // ------------------------------------------------------------------
    if (descUpper.find("CINE") != std::string::npos) {
        result.type = SeriesType::CINE;
        result.is4DFlow = false;
        return result;
    }

    result.type = SeriesType::Unknown;
    result.is4DFlow = false;
    return result;
}

std::vector<ClassifiedSeries> SeriesClassifier::classifyStudy(
    const std::vector<std::string>& seriesFiles) {
    std::vector<ClassifiedSeries> results;
    results.reserve(seriesFiles.size());
    for (const auto& file : seriesFiles) {
        results.push_back(classifyFile(file));
    }
    return results;
}

std::vector<ClassifiedSeries> SeriesClassifier::classifyScannedSeries(
    const std::vector<core::SeriesInfo>& scannedSeries) {
    std::vector<ClassifiedSeries> results;
    results.reserve(scannedSeries.size());
    for (const auto& info : scannedSeries) {
        if (info.slices.empty()) {
            results.push_back(ClassifiedSeries{
                SeriesType::Unknown,
                info.seriesInstanceUid,
                info.seriesDescription,
                info.modality,
                false
            });
        } else {
            results.push_back(
                classifyFile(info.slices.front().filePath.string()));
        }
    }
    return results;
}

bool SeriesClassifier::is4DFlowType(SeriesType type) noexcept {
    switch (type) {
        case SeriesType::Flow4D_Magnitude:
        case SeriesType::Flow4D_Phase_AP:
        case SeriesType::Flow4D_Phase_FH:
        case SeriesType::Flow4D_Phase_RL:
            return true;
        default:
            return false;
    }
}

}  // namespace dicom_viewer::services

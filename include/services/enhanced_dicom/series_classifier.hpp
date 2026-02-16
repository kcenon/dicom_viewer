#pragma once

#include <string>
#include <vector>

#include <itkMetaDataDictionary.h>

namespace dicom_viewer::services {

/**
 * @brief Classification of DICOM series modality/acquisition type
 *
 * Covers 4D Flow components, standard MRI sequences, and CT.
 *
 * @trace SRS-FR-049
 */
enum class SeriesType {
    Flow4D_Magnitude,   ///< 4D Flow magnitude image
    Flow4D_Phase_AP,    ///< 4D Flow phase — Anterior-Posterior
    Flow4D_Phase_FH,    ///< 4D Flow phase — Foot-Head
    Flow4D_Phase_RL,    ///< 4D Flow phase — Right-Left
    PC_MRA,             ///< Phase-contrast MR angiography
    CINE,               ///< Cardiac CINE MRI
    DIXON,              ///< DIXON water/fat separation
    Starvibe,           ///< Siemens StarVIBE (radial VIBE)
    CT,                 ///< Computed Tomography
    TOF,                ///< Time-of-Flight angiography
    VENC_2D,            ///< 2D phase-contrast velocity encoding
    Unknown             ///< Unrecognized series type
};

/**
 * @brief Result of DICOM series classification
 */
struct ClassifiedSeries {
    SeriesType type = SeriesType::Unknown;
    std::string seriesUid;
    std::string description;
    std::string modality;
    bool is4DFlow = false;  ///< true for magnitude + phase series
};

/**
 * @brief Convert SeriesType enum to human-readable string
 */
[[nodiscard]] inline std::string seriesToString(SeriesType type) {
    switch (type) {
        case SeriesType::Flow4D_Magnitude: return "4D Flow Magnitude";
        case SeriesType::Flow4D_Phase_AP: return "4D Flow Phase AP";
        case SeriesType::Flow4D_Phase_FH: return "4D Flow Phase FH";
        case SeriesType::Flow4D_Phase_RL: return "4D Flow Phase RL";
        case SeriesType::PC_MRA: return "PC-MRA";
        case SeriesType::CINE: return "CINE";
        case SeriesType::DIXON: return "DIXON";
        case SeriesType::Starvibe: return "StarVIBE";
        case SeriesType::CT: return "CT";
        case SeriesType::TOF: return "TOF";
        case SeriesType::VENC_2D: return "2D VENC";
        case SeriesType::Unknown: return "Unknown";
    }
    return "Unknown";
}

/**
 * @brief Classifier for multi-modal DICOM series
 *
 * Identifies series types from DICOM header tags:
 * - Modality (0008,0060)
 * - SeriesDescription (0008,103E)
 * - ImageType (0008,0008)
 * - Scanning Sequence (0018,0020)
 * - Velocity encoding tags (vendor-specific)
 *
 * Stateless — all methods are static.
 *
 * @trace SRS-FR-049
 */
class SeriesClassifier {
public:
    /**
     * @brief Classify a single DICOM series from a file path
     *
     * Reads the first file's metadata and classifies the series.
     *
     * @param filePath Path to one DICOM file from the series
     * @return Classification result
     */
    [[nodiscard]] static ClassifiedSeries classifyFile(
        const std::string& filePath);

    /**
     * @brief Classify from a pre-read ITK metadata dictionary
     *
     * Useful when metadata has already been loaded (avoids re-reading).
     * Also used for testing with synthetic metadata.
     *
     * @param metadata ITK metadata dictionary from GDCMImageIO
     * @return Classification result
     */
    [[nodiscard]] static ClassifiedSeries classify(
        const itk::MetaDataDictionary& metadata);

    /**
     * @brief Classify all series in a study
     *
     * Takes a list of representative file paths (one per series)
     * and returns the classification for each.
     *
     * @param seriesFiles One DICOM file path per series
     * @return Vector of classification results (same order)
     */
    [[nodiscard]] static std::vector<ClassifiedSeries> classifyStudy(
        const std::vector<std::string>& seriesFiles);

    /**
     * @brief Check if a SeriesType is a 4D Flow component
     */
    [[nodiscard]] static bool is4DFlowType(SeriesType type) noexcept;
};

}  // namespace dicom_viewer::services

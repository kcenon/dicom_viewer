#include "services/dicom_find_scu.hpp"
#include "services/dicom_echo_scu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

#include <spdlog/spdlog.h>

#ifdef DICOM_VIEWER_USE_PACS_SYSTEM
// pacs_system headers for new implementation
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>
#include <pacs/network/association.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/services/query_scu.hpp>
#include <pacs/encoding/vr_type.hpp>
#else
// DCMTK headers for legacy implementation
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>
#endif

namespace dicom_viewer::services {

std::string DateRange::toDicomFormat() const {
    if (from && to) {
        if (*from == *to) {
            return *from;
        }
        return *from + "-" + *to;
    } else if (from) {
        return *from + "-";
    } else if (to) {
        return "-" + *to;
    }
    return "";
}

namespace {

/**
 * @brief Convert QueryLevel to DICOM string representation
 */
const char* queryLevelToString(QueryLevel level) {
    switch (level) {
        case QueryLevel::Patient: return "PATIENT";
        case QueryLevel::Study:   return "STUDY";
        case QueryLevel::Series:  return "SERIES";
        case QueryLevel::Image:   return "IMAGE";
    }
    return "STUDY";
}

/**
 * @brief Get SOP Class UID for the query root
 */
const char* getSopClassUid(QueryRoot root) {
    switch (root) {
        case QueryRoot::PatientRoot:
            return DicomFindSCU::PATIENT_ROOT_FIND_SOP_CLASS_UID;
        case QueryRoot::StudyRoot:
            return DicomFindSCU::STUDY_ROOT_FIND_SOP_CLASS_UID;
    }
    return DicomFindSCU::STUDY_ROOT_FIND_SOP_CLASS_UID;
}

#ifdef DICOM_VIEWER_USE_PACS_SYSTEM

// Additional tag definitions not in pacs_system
namespace local_tags {
inline constexpr pacs::core::dicom_tag body_part_examined{0x0018, 0x0015};
}

/**
 * @brief Helper to safely get string from pacs_system dataset
 */
std::string getStringFromDataset(const pacs::core::dicom_dataset& dataset,
                                  pacs::core::dicom_tag tag) {
    return dataset.get_string(tag, "");
}

/**
 * @brief Helper to safely get integer from pacs_system dataset
 */
int32_t getIntFromDataset(const pacs::core::dicom_dataset& dataset,
                           pacs::core::dicom_tag tag) {
    auto value = dataset.get_numeric<int32_t>(tag);
    if (value) {
        return *value;
    }

    // Try as string and convert
    std::string strValue = dataset.get_string(tag, "");
    if (!strValue.empty()) {
        try {
            return std::stoi(strValue);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

/**
 * @brief Parse a patient result from pacs_system dataset
 */
PatientResult parsePatientResult(const pacs::core::dicom_dataset& dataset) {
    PatientResult result;
    result.patientId = getStringFromDataset(dataset, pacs::core::tags::patient_id);
    result.patientName = getStringFromDataset(dataset, pacs::core::tags::patient_name);
    result.patientBirthDate = getStringFromDataset(dataset, pacs::core::tags::patient_birth_date);
    result.patientSex = getStringFromDataset(dataset, pacs::core::tags::patient_sex);
    result.numberOfStudies = getIntFromDataset(dataset, pacs::core::tags::number_of_patient_related_studies);
    return result;
}

/**
 * @brief Parse a study result from pacs_system dataset
 */
StudyResult parseStudyResult(const pacs::core::dicom_dataset& dataset) {
    StudyResult result;
    result.studyInstanceUid = getStringFromDataset(dataset, pacs::core::tags::study_instance_uid);
    result.studyDate = getStringFromDataset(dataset, pacs::core::tags::study_date);
    result.studyTime = getStringFromDataset(dataset, pacs::core::tags::study_time);
    result.studyDescription = getStringFromDataset(dataset, pacs::core::tags::study_description);
    result.accessionNumber = getStringFromDataset(dataset, pacs::core::tags::accession_number);
    result.referringPhysician = getStringFromDataset(dataset, pacs::core::tags::referring_physician_name);
    result.patientId = getStringFromDataset(dataset, pacs::core::tags::patient_id);
    result.patientName = getStringFromDataset(dataset, pacs::core::tags::patient_name);
    result.modalitiesInStudy = getStringFromDataset(dataset, pacs::core::tags::modalities_in_study);
    result.numberOfSeries = getIntFromDataset(dataset, pacs::core::tags::number_of_study_related_series);
    result.numberOfInstances = getIntFromDataset(dataset, pacs::core::tags::number_of_study_related_instances);
    return result;
}

/**
 * @brief Parse a series result from pacs_system dataset
 */
SeriesResult parseSeriesResult(const pacs::core::dicom_dataset& dataset) {
    SeriesResult result;
    result.seriesInstanceUid = getStringFromDataset(dataset, pacs::core::tags::series_instance_uid);
    result.studyInstanceUid = getStringFromDataset(dataset, pacs::core::tags::study_instance_uid);
    result.modality = getStringFromDataset(dataset, pacs::core::tags::modality);
    result.seriesNumber = getIntFromDataset(dataset, pacs::core::tags::series_number);
    result.seriesDescription = getStringFromDataset(dataset, pacs::core::tags::series_description);
    result.seriesDate = getStringFromDataset(dataset, pacs::core::tags::series_date);
    result.seriesTime = getStringFromDataset(dataset, pacs::core::tags::series_time);
    result.bodyPartExamined = getStringFromDataset(dataset, local_tags::body_part_examined);
    result.numberOfInstances = getIntFromDataset(dataset, pacs::core::tags::number_of_series_related_instances);
    return result;
}

/**
 * @brief Parse an image result from pacs_system dataset
 */
ImageResult parseImageResult(const pacs::core::dicom_dataset& dataset) {
    ImageResult result;
    result.sopInstanceUid = getStringFromDataset(dataset, pacs::core::tags::sop_instance_uid);
    result.sopClassUid = getStringFromDataset(dataset, pacs::core::tags::sop_class_uid);
    result.seriesInstanceUid = getStringFromDataset(dataset, pacs::core::tags::series_instance_uid);
    result.instanceNumber = getIntFromDataset(dataset, pacs::core::tags::instance_number);
    result.contentDate = getStringFromDataset(dataset, pacs::core::tags::content_date);
    result.contentTime = getStringFromDataset(dataset, pacs::core::tags::content_time);
    return result;
}

#else  // DCMTK implementation

/**
 * @brief Helper to safely get string from dataset
 */
std::string getStringFromDataset(DcmDataset* dataset, const DcmTagKey& tag) {
    if (!dataset) return "";

    OFString value;
    if (dataset->findAndGetOFString(tag, value).good()) {
        return std::string(value.c_str());
    }
    return "";
}

/**
 * @brief Helper to safely get integer from dataset
 */
int32_t getIntFromDataset(DcmDataset* dataset, const DcmTagKey& tag) {
    if (!dataset) return 0;

    Sint32 value = 0;
    if (dataset->findAndGetSint32(tag, value).good()) {
        return static_cast<int32_t>(value);
    }

    // Try as string and convert
    OFString strValue;
    if (dataset->findAndGetOFString(tag, strValue).good() && !strValue.empty()) {
        try {
            return std::stoi(strValue.c_str());
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

/**
 * @brief Parse a patient result from dataset
 */
PatientResult parsePatientResult(DcmDataset* dataset) {
    PatientResult result;
    result.patientId = getStringFromDataset(dataset, DCM_PatientID);
    result.patientName = getStringFromDataset(dataset, DCM_PatientName);
    result.patientBirthDate = getStringFromDataset(dataset, DCM_PatientBirthDate);
    result.patientSex = getStringFromDataset(dataset, DCM_PatientSex);
    result.numberOfStudies = getIntFromDataset(dataset, DCM_NumberOfPatientRelatedStudies);
    return result;
}

/**
 * @brief Parse a study result from dataset
 */
StudyResult parseStudyResult(DcmDataset* dataset) {
    StudyResult result;
    result.studyInstanceUid = getStringFromDataset(dataset, DCM_StudyInstanceUID);
    result.studyDate = getStringFromDataset(dataset, DCM_StudyDate);
    result.studyTime = getStringFromDataset(dataset, DCM_StudyTime);
    result.studyDescription = getStringFromDataset(dataset, DCM_StudyDescription);
    result.accessionNumber = getStringFromDataset(dataset, DCM_AccessionNumber);
    result.referringPhysician = getStringFromDataset(dataset, DCM_ReferringPhysicianName);
    result.patientId = getStringFromDataset(dataset, DCM_PatientID);
    result.patientName = getStringFromDataset(dataset, DCM_PatientName);
    result.modalitiesInStudy = getStringFromDataset(dataset, DCM_ModalitiesInStudy);
    result.numberOfSeries = getIntFromDataset(dataset, DCM_NumberOfStudyRelatedSeries);
    result.numberOfInstances = getIntFromDataset(dataset, DCM_NumberOfStudyRelatedInstances);
    return result;
}

/**
 * @brief Parse a series result from dataset
 */
SeriesResult parseSeriesResult(DcmDataset* dataset) {
    SeriesResult result;
    result.seriesInstanceUid = getStringFromDataset(dataset, DCM_SeriesInstanceUID);
    result.studyInstanceUid = getStringFromDataset(dataset, DCM_StudyInstanceUID);
    result.modality = getStringFromDataset(dataset, DCM_Modality);
    result.seriesNumber = getIntFromDataset(dataset, DCM_SeriesNumber);
    result.seriesDescription = getStringFromDataset(dataset, DCM_SeriesDescription);
    result.seriesDate = getStringFromDataset(dataset, DCM_SeriesDate);
    result.seriesTime = getStringFromDataset(dataset, DCM_SeriesTime);
    result.bodyPartExamined = getStringFromDataset(dataset, DCM_BodyPartExamined);
    result.numberOfInstances = getIntFromDataset(dataset, DCM_NumberOfSeriesRelatedInstances);
    return result;
}

/**
 * @brief Parse an image result from dataset
 */
ImageResult parseImageResult(DcmDataset* dataset) {
    ImageResult result;
    result.sopInstanceUid = getStringFromDataset(dataset, DCM_SOPInstanceUID);
    result.sopClassUid = getStringFromDataset(dataset, DCM_SOPClassUID);
    result.seriesInstanceUid = getStringFromDataset(dataset, DCM_SeriesInstanceUID);
    result.instanceNumber = getIntFromDataset(dataset, DCM_InstanceNumber);
    result.contentDate = getStringFromDataset(dataset, DCM_ContentDate);
    result.contentTime = getStringFromDataset(dataset, DCM_ContentTime);
    return result;
}

#endif  // DICOM_VIEWER_USE_PACS_SYSTEM

} // anonymous namespace

#ifdef DICOM_VIEWER_USE_PACS_SYSTEM

// pacs_system-based implementation
class DicomFindSCU::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    std::expected<FindResult, PacsErrorInfo> find(
        const PacsServerConfig& config,
        const FindQuery& query
    ) {
        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid PACS server configuration"
            });
        }

        if (isQuerying_.exchange(true)) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "A query is already in progress"
            });
        }

        cancelled_.store(false);
        auto result = performFind(config, query);
        isQuerying_.store(false);

        return result;
    }

    void cancel() {
        cancelled_.store(true);
    }

    bool isQuerying() const {
        return isQuerying_.load();
    }

private:
    std::atomic<bool> isQuerying_{false};
    std::atomic<bool> cancelled_{false};

    std::expected<FindResult, PacsErrorInfo> performFind(
        const PacsServerConfig& config,
        const FindQuery& query
    ) {
        auto startTime = std::chrono::steady_clock::now();
        FindResult findResult;

        // Build association configuration
        pacs::network::association_config assocConfig;
        assocConfig.calling_ae_title = config.callingAeTitle;
        assocConfig.called_ae_title = config.calledAeTitle;
        assocConfig.max_pdu_length = config.maxPduSize;

        // Get SOP Class UID for the query model
        const char* sopClassUid = getSopClassUid(query.root);

        // Add presentation context for Query/Retrieve FIND
        pacs::network::proposed_presentation_context findCtx;
        findCtx.id = 1;
        findCtx.abstract_syntax = sopClassUid;
        findCtx.transfer_syntaxes = {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2.2",  // Explicit VR Big Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        };
        assocConfig.proposed_contexts.push_back(findCtx);

        // Check for cancellation before connection
        if (cancelled_.load()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        spdlog::info("Requesting C-FIND association with {}:{} (AE: {})",
                     config.hostname, config.port, config.calledAeTitle);

        // Connect to remote SCP
        auto timeout = std::chrono::duration_cast<pacs::network::association::duration>(
            config.connectionTimeout
        );
        auto connectResult = pacs::network::association::connect(
            config.hostname,
            config.port,
            assocConfig,
            timeout
        );

        if (!connectResult.is_ok()) {
            const auto& err = connectResult.error();
            return mapAssociationError(err);
        }

        auto assoc = std::move(connectResult.value());

        // Check if Query/Retrieve FIND SOP Class was accepted
        if (!assoc.has_accepted_context(sopClassUid)) {
            spdlog::error("Query/Retrieve FIND SOP Class was not accepted by the server");
            assoc.abort();
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Query/Retrieve FIND SOP Class was not accepted by the server"
            });
        }

        // Check for cancellation before sending query
        if (cancelled_.load()) {
            assoc.abort();
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        // Build query dataset
        pacs::core::dicom_dataset queryDataset;
        buildQueryDataset(queryDataset, query);

        // Configure query_scu
        pacs::services::query_scu_config scuConfig;
        scuConfig.model = (query.root == QueryRoot::PatientRoot)
            ? pacs::services::query_model::patient_root
            : pacs::services::query_model::study_root;

        switch (query.level) {
            case QueryLevel::Patient:
                scuConfig.level = pacs::services::query_level::patient;
                break;
            case QueryLevel::Study:
                scuConfig.level = pacs::services::query_level::study;
                break;
            case QueryLevel::Series:
                scuConfig.level = pacs::services::query_level::series;
                break;
            case QueryLevel::Image:
                scuConfig.level = pacs::services::query_level::instance;
                break;
        }
        scuConfig.timeout = config.dimseTimeout;

        pacs::services::query_scu scu(scuConfig);

        spdlog::debug("Sending C-FIND request");

        // Execute C-FIND with streaming callback
        auto queryResult = scu.find_streaming(
            assoc,
            queryDataset,
            [this, &findResult, level = query.level](const pacs::core::dicom_dataset& response) {
                if (cancelled_.load()) {
                    spdlog::debug("C-FIND cancelled, stopping result collection");
                    return false;  // Stop receiving
                }

                // Parse and add result based on query level
                switch (level) {
                    case QueryLevel::Patient:
                        findResult.patients.push_back(parsePatientResult(response));
                        break;
                    case QueryLevel::Study:
                        findResult.studies.push_back(parseStudyResult(response));
                        break;
                    case QueryLevel::Series:
                        findResult.series.push_back(parseSeriesResult(response));
                        break;
                    case QueryLevel::Image:
                        findResult.images.push_back(parseImageResult(response));
                        break;
                }
                return true;  // Continue receiving
            }
        );

        auto endTime = std::chrono::steady_clock::now();
        findResult.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );

        // Release association gracefully
        auto dimseTimeout = std::chrono::duration_cast<pacs::network::association::duration>(
            config.dimseTimeout
        );
        auto releaseResult = assoc.release(dimseTimeout);
        if (!releaseResult.is_ok()) {
            spdlog::warn("Failed to release association gracefully: {}",
                         releaseResult.error().message);
        }

        if (!queryResult.is_ok()) {
            const auto& err = queryResult.error();
            spdlog::error("C-FIND failed: {}", err.message);

            if (cancelled_.load()) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::NetworkError,
                    "Operation cancelled"
                });
            }

            if (err.code == pacs::error_codes::receive_timeout) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::Timeout,
                    "C-FIND timeout: " + err.message
                });
            }

            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "C-FIND failed: " + err.message
            });
        }

        spdlog::info("C-FIND completed: {} results (latency: {}ms)",
                     findResult.totalCount(), findResult.latency.count());

        return findResult;
    }

    void buildQueryDataset(pacs::core::dicom_dataset& dataset, const FindQuery& query) {
        using namespace pacs::core;
        using vr = pacs::encoding::vr_type;

        // Set Query/Retrieve Level
        dataset.set_string(tags::query_retrieve_level, vr::CS, queryLevelToString(query.level));

        // Patient level attributes
        if (query.level >= QueryLevel::Patient) {
            dataset.set_string(tags::patient_id, vr::LO,
                query.patientId.value_or(""));
            dataset.set_string(tags::patient_name, vr::PN,
                query.patientName.value_or(""));

            if (query.patientBirthDate) {
                dataset.set_string(tags::patient_birth_date, vr::DA,
                    query.patientBirthDate->toDicomFormat());
            } else {
                dataset.set_string(tags::patient_birth_date, vr::DA, "");
            }
            dataset.set_string(tags::patient_sex, vr::CS, "");

            if (query.level == QueryLevel::Patient) {
                dataset.set_string(tags::number_of_patient_related_studies, vr::IS, "");
            }
        }

        // Study level attributes
        if (query.level >= QueryLevel::Study || query.root == QueryRoot::StudyRoot) {
            dataset.set_string(tags::study_instance_uid, vr::UI,
                query.studyInstanceUid.value_or(""));

            if (query.studyDate) {
                dataset.set_string(tags::study_date, vr::DA,
                    query.studyDate->toDicomFormat());
            } else {
                dataset.set_string(tags::study_date, vr::DA, "");
            }
            dataset.set_string(tags::study_time, vr::TM, "");
            dataset.set_string(tags::study_description, vr::LO,
                query.studyDescription.value_or(""));
            dataset.set_string(tags::accession_number, vr::SH,
                query.accessionNumber.value_or(""));
            dataset.set_string(tags::referring_physician_name, vr::PN, "");
            dataset.set_string(tags::modalities_in_study, vr::CS,
                query.modalitiesInStudy.value_or(""));

            if (query.level == QueryLevel::Study) {
                dataset.set_string(tags::number_of_study_related_series, vr::IS, "");
                dataset.set_string(tags::number_of_study_related_instances, vr::IS, "");
            }
        }

        // Series level attributes
        if (query.level >= QueryLevel::Series) {
            dataset.set_string(tags::series_instance_uid, vr::UI,
                query.seriesInstanceUid.value_or(""));
            dataset.set_string(tags::modality, vr::CS,
                query.modality.value_or(""));

            if (query.seriesNumber) {
                dataset.set_string(tags::series_number, vr::IS,
                    std::to_string(*query.seriesNumber));
            } else {
                dataset.set_string(tags::series_number, vr::IS, "");
            }
            dataset.set_string(tags::series_description, vr::LO,
                query.seriesDescription.value_or(""));
            dataset.set_string(tags::series_date, vr::DA, "");
            dataset.set_string(tags::series_time, vr::TM, "");
            dataset.set_string(local_tags::body_part_examined, vr::CS, "");

            if (query.level == QueryLevel::Series) {
                dataset.set_string(tags::number_of_series_related_instances, vr::IS, "");
            }
        }

        // Image level attributes
        if (query.level == QueryLevel::Image) {
            dataset.set_string(tags::sop_instance_uid, vr::UI,
                query.sopInstanceUid.value_or(""));
            dataset.set_string(tags::sop_class_uid, vr::UI, "");

            if (query.instanceNumber) {
                dataset.set_string(tags::instance_number, vr::IS,
                    std::to_string(*query.instanceNumber));
            } else {
                dataset.set_string(tags::instance_number, vr::IS, "");
            }
            dataset.set_string(tags::content_date, vr::DA, "");
            dataset.set_string(tags::content_time, vr::TM, "");
        }
    }

    std::unexpected<PacsErrorInfo> mapAssociationError(const pacs::error_info& err) {
        int code = err.code;

        if (code == pacs::error_codes::connection_failed ||
            code == pacs::error_codes::connection_timeout) {
            spdlog::error("Connection failed: {}", err.message);
            return std::unexpected(PacsErrorInfo{
                PacsError::ConnectionFailed,
                "Failed to connect: " + err.message
            });
        }

        if (code == pacs::error_codes::association_rejected) {
            spdlog::error("Association rejected: {}", err.message);
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Association rejected: " + err.message
            });
        }

        if (code == pacs::error_codes::receive_timeout ||
            code == pacs::error_codes::connection_timeout) {
            spdlog::error("Connection timeout: {}", err.message);
            return std::unexpected(PacsErrorInfo{
                PacsError::Timeout,
                "Connection timeout: " + err.message
            });
        }

        spdlog::error("Network error: {}", err.message);
        return std::unexpected(PacsErrorInfo{
            PacsError::NetworkError,
            "Network error: " + err.message
        });
    }
};

#else  // DCMTK-based legacy implementation

/**
 * @brief Callback context for storing C-FIND results
 */
struct FindCallbackContext {
    QueryLevel level;
    FindResult* result;
    std::atomic<bool>* cancelled;
};

/**
 * @brief Callback function for processing C-FIND responses
 */
static void findCallback(
    void* callbackData,
    T_DIMSE_C_FindRQ* /*request*/,
    int responseCount,
    T_DIMSE_C_FindRSP* response,
    DcmDataset* responseIdentifiers
) {
    auto* ctx = static_cast<FindCallbackContext*>(callbackData);

    if (ctx->cancelled->load()) {
        spdlog::debug("C-FIND cancelled, ignoring response #{}", responseCount);
        return;
    }

    if (response->DimseStatus == STATUS_Pending ||
        response->DimseStatus == STATUS_FIND_Pending_WarningUnsupportedOptionalKeys) {

        if (!responseIdentifiers) {
            spdlog::warn("C-FIND response #{} has no identifiers", responseCount);
            return;
        }

        spdlog::debug("Processing C-FIND response #{}", responseCount);

        switch (ctx->level) {
            case QueryLevel::Patient:
                ctx->result->patients.push_back(parsePatientResult(responseIdentifiers));
                break;
            case QueryLevel::Study:
                ctx->result->studies.push_back(parseStudyResult(responseIdentifiers));
                break;
            case QueryLevel::Series:
                ctx->result->series.push_back(parseSeriesResult(responseIdentifiers));
                break;
            case QueryLevel::Image:
                ctx->result->images.push_back(parseImageResult(responseIdentifiers));
                break;
        }
    }
}

class DicomFindSCU::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    std::expected<FindResult, PacsErrorInfo> find(
        const PacsServerConfig& config,
        const FindQuery& query
    ) {
        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid PACS server configuration"
            });
        }

        if (isQuerying_.exchange(true)) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "A query is already in progress"
            });
        }

        cancelled_.store(false);
        auto result = performFind(config, query);
        isQuerying_.store(false);

        return result;
    }

    void cancel() {
        cancelled_.store(true);
    }

    bool isQuerying() const {
        return isQuerying_.load();
    }

private:
    std::atomic<bool> isQuerying_{false};
    std::atomic<bool> cancelled_{false};

    std::expected<FindResult, PacsErrorInfo> performFind(
        const PacsServerConfig& config,
        const FindQuery& query
    ) {
        auto startTime = std::chrono::steady_clock::now();
        FindResult findResult;

        // Initialize network
        T_ASC_Network* network = nullptr;
        OFCondition cond = ASC_initializeNetwork(
            NET_REQUESTOR,
            0,
            static_cast<int>(config.connectionTimeout.count()),
            &network
        );

        if (cond.bad()) {
            spdlog::error("Failed to initialize network: {}", cond.text());
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                std::string("Failed to initialize network: ") + cond.text()
            });
        }

        // Create association parameters
        T_ASC_Parameters* params = nullptr;
        cond = ASC_createAssociationParameters(&params, config.maxPduSize);
        if (cond.bad()) {
            ASC_dropNetwork(&network);
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to create association parameters: ") + cond.text()
            });
        }

        // Set AE titles
        ASC_setAPTitles(
            params,
            config.callingAeTitle.c_str(),
            config.calledAeTitle.c_str(),
            nullptr
        );

        // Set peer address
        std::string peerAddress = config.hostname + ":" + std::to_string(config.port);
        ASC_setPresentationAddresses(
            params,
            OFStandard::getHostName().c_str(),
            peerAddress.c_str()
        );

        // Add presentation context for Query/Retrieve FIND
        const char* transferSyntaxes[] = {
            UID_LittleEndianExplicitTransferSyntax,
            UID_BigEndianExplicitTransferSyntax,
            UID_LittleEndianImplicitTransferSyntax
        };

        const char* sopClassUid = getSopClassUid(query.root);

        cond = ASC_addPresentationContext(
            params,
            1,
            sopClassUid,
            transferSyntaxes,
            3
        );

        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to add presentation context: ") + cond.text()
            });
        }

        // Check for cancellation
        if (cancelled_.load()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                "Operation cancelled"
            });
        }

        // Create association
        T_ASC_Association* assoc = nullptr;
        spdlog::info("Requesting C-FIND association with {}:{} (AE: {})",
                     config.hostname, config.port, config.calledAeTitle);

        cond = ASC_requestAssociation(network, params, &assoc);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);

            if (cond == DUL_ASSOCIATIONREJECTED) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::AssociationRejected,
                    std::string("Association rejected: ") + cond.text()
                });
            }

            return std::unexpected(PacsErrorInfo{
                PacsError::ConnectionFailed,
                std::string("Failed to request association: ") + cond.text()
            });
        }

        // Check if SOP class was accepted
        T_ASC_PresentationContextID presId = ASC_findAcceptedPresentationContextID(
            assoc, sopClassUid
        );

        if (presId == 0) {
            ASC_releaseAssociation(assoc);
            ASC_destroyAssociation(&assoc);
            ASC_dropNetwork(&network);
            return std::unexpected(PacsErrorInfo{
                PacsError::AssociationRejected,
                "Query/Retrieve FIND SOP Class was not accepted by the server"
            });
        }

        // Build query dataset
        DcmDataset queryDataset;
        buildQueryDataset(&queryDataset, query);

        // Prepare C-FIND request
        T_DIMSE_C_FindRQ findRequest;
        memset(&findRequest, 0, sizeof(findRequest));
        findRequest.MessageID = assoc->nextMsgID++;
        strncpy(findRequest.AffectedSOPClassUID, sopClassUid,
                sizeof(findRequest.AffectedSOPClassUID) - 1);
        findRequest.DataSetType = DIMSE_DATASET_PRESENT;
        findRequest.Priority = DIMSE_PRIORITY_MEDIUM;

        // Prepare callback context
        FindCallbackContext callbackCtx;
        callbackCtx.level = query.level;
        callbackCtx.result = &findResult;
        callbackCtx.cancelled = &cancelled_;

        T_DIMSE_C_FindRSP findResponse;
        DcmDataset* statusDetail = nullptr;
        int responseCount = 0;

        spdlog::debug("Sending C-FIND request (Message ID: {})", findRequest.MessageID);

        // Execute C-FIND
        cond = DIMSE_findUser(
            assoc,
            presId,
            &findRequest,
            &queryDataset,
            responseCount,
            findCallback,
            &callbackCtx,
            DIMSE_BLOCKING,
            static_cast<int>(config.dimseTimeout.count()),
            &findResponse,
            &statusDetail
        );

        auto endTime = std::chrono::steady_clock::now();
        findResult.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );

        // Clean up status detail
        if (statusDetail) {
            delete statusDetail;
        }

        // Release association
        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        ASC_dropNetwork(&network);

        if (cond.bad()) {
            if (cancelled_.load()) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::NetworkError,
                    "Operation cancelled"
                });
            }
            if (cond == DIMSE_NODATAAVAILABLE || cond == DIMSE_READPDVFAILED) {
                return std::unexpected(PacsErrorInfo{
                    PacsError::Timeout,
                    std::string("C-FIND timeout: ") + cond.text()
                });
            }
            return std::unexpected(PacsErrorInfo{
                PacsError::NetworkError,
                std::string("C-FIND failed: ") + cond.text()
            });
        }

        // Check DIMSE status
        if (findResponse.DimseStatus != STATUS_Success &&
            findResponse.DimseStatus != STATUS_FIND_Cancel) {
            // Non-fatal statuses are acceptable
            spdlog::debug("C-FIND completed with status: 0x{:04X}",
                          findResponse.DimseStatus);
        }

        spdlog::info("C-FIND completed: {} results (latency: {}ms)",
                     findResult.totalCount(), findResult.latency.count());

        return findResult;
    }

    void buildQueryDataset(DcmDataset* dataset, const FindQuery& query) {
        // Set Query/Retrieve Level
        dataset->putAndInsertString(DCM_QueryRetrieveLevel, queryLevelToString(query.level));

        // Patient level attributes
        if (query.level >= QueryLevel::Patient) {
            dataset->putAndInsertString(DCM_PatientID,
                query.patientId.value_or("").c_str());
            dataset->putAndInsertString(DCM_PatientName,
                query.patientName.value_or("").c_str());

            if (query.patientBirthDate) {
                dataset->putAndInsertString(DCM_PatientBirthDate,
                    query.patientBirthDate->toDicomFormat().c_str());
            } else {
                dataset->putAndInsertString(DCM_PatientBirthDate, "");
            }
            dataset->putAndInsertString(DCM_PatientSex, "");

            if (query.level == QueryLevel::Patient) {
                dataset->putAndInsertString(DCM_NumberOfPatientRelatedStudies, "");
            }
        }

        // Study level attributes
        if (query.level >= QueryLevel::Study || query.root == QueryRoot::StudyRoot) {
            dataset->putAndInsertString(DCM_StudyInstanceUID,
                query.studyInstanceUid.value_or("").c_str());

            if (query.studyDate) {
                dataset->putAndInsertString(DCM_StudyDate,
                    query.studyDate->toDicomFormat().c_str());
            } else {
                dataset->putAndInsertString(DCM_StudyDate, "");
            }
            dataset->putAndInsertString(DCM_StudyTime, "");
            dataset->putAndInsertString(DCM_StudyDescription,
                query.studyDescription.value_or("").c_str());
            dataset->putAndInsertString(DCM_AccessionNumber,
                query.accessionNumber.value_or("").c_str());
            dataset->putAndInsertString(DCM_ReferringPhysicianName, "");
            dataset->putAndInsertString(DCM_ModalitiesInStudy,
                query.modalitiesInStudy.value_or("").c_str());

            if (query.level == QueryLevel::Study) {
                dataset->putAndInsertString(DCM_NumberOfStudyRelatedSeries, "");
                dataset->putAndInsertString(DCM_NumberOfStudyRelatedInstances, "");
            }
        }

        // Series level attributes
        if (query.level >= QueryLevel::Series) {
            dataset->putAndInsertString(DCM_SeriesInstanceUID,
                query.seriesInstanceUid.value_or("").c_str());
            dataset->putAndInsertString(DCM_Modality,
                query.modality.value_or("").c_str());

            if (query.seriesNumber) {
                dataset->putAndInsertString(DCM_SeriesNumber,
                    std::to_string(*query.seriesNumber).c_str());
            } else {
                dataset->putAndInsertString(DCM_SeriesNumber, "");
            }
            dataset->putAndInsertString(DCM_SeriesDescription,
                query.seriesDescription.value_or("").c_str());
            dataset->putAndInsertString(DCM_SeriesDate, "");
            dataset->putAndInsertString(DCM_SeriesTime, "");
            dataset->putAndInsertString(DCM_BodyPartExamined, "");

            if (query.level == QueryLevel::Series) {
                dataset->putAndInsertString(DCM_NumberOfSeriesRelatedInstances, "");
            }
        }

        // Image level attributes
        if (query.level == QueryLevel::Image) {
            dataset->putAndInsertString(DCM_SOPInstanceUID,
                query.sopInstanceUid.value_or("").c_str());
            dataset->putAndInsertString(DCM_SOPClassUID, "");

            if (query.instanceNumber) {
                dataset->putAndInsertString(DCM_InstanceNumber,
                    std::to_string(*query.instanceNumber).c_str());
            } else {
                dataset->putAndInsertString(DCM_InstanceNumber, "");
            }
            dataset->putAndInsertString(DCM_ContentDate, "");
            dataset->putAndInsertString(DCM_ContentTime, "");
        }
    }
};

#endif  // DICOM_VIEWER_USE_PACS_SYSTEM

// Public interface implementation

DicomFindSCU::DicomFindSCU()
    : impl_(std::make_unique<Impl>()) {
}

DicomFindSCU::~DicomFindSCU() = default;

DicomFindSCU::DicomFindSCU(DicomFindSCU&&) noexcept = default;
DicomFindSCU& DicomFindSCU::operator=(DicomFindSCU&&) noexcept = default;

std::expected<FindResult, PacsErrorInfo> DicomFindSCU::find(
    const PacsServerConfig& config,
    const FindQuery& query
) {
    return impl_->find(config, query);
}

void DicomFindSCU::cancel() {
    impl_->cancel();
}

bool DicomFindSCU::isQuerying() const {
    return impl_->isQuerying();
}

} // namespace dicom_viewer::services

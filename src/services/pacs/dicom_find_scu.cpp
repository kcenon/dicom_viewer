#include "services/dicom_find_scu.hpp"
#include "services/dicom_echo_scu.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

// DCMTK headers
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>

#include <spdlog/spdlog.h>

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

} // anonymous namespace

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

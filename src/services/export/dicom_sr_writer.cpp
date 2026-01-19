#include "services/export/dicom_sr_writer.hpp"

#include <QDateTime>
#include <QString>

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>

// DCMTK headers
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>
#include <dcmtk/dcmsr/dsrdoc.h>
#include <dcmtk/dcmsr/dsrcodtn.h>
#include <dcmtk/dcmsr/dsrnumtn.h>
#include <dcmtk/dcmsr/dsrscotn.h>
#include <dcmtk/dcmsr/dsrsc3tn.h>
#include <dcmtk/dcmsr/dsrtextn.h>
#include <dcmtk/dcmsr/dsruidtn.h>

#include <spdlog/spdlog.h>

namespace dicom_viewer::services {

namespace {

std::string formatDateTime(const std::chrono::system_clock::time_point& tp,
                          const char* format) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, format);
    return oss.str();
}

DSRCodedEntryValue toDsrCode(const DicomCode& code) {
    return DSRCodedEntryValue(
        code.value.c_str(),
        code.scheme.c_str(),
        code.meaning.c_str()
    );
}

void setDatasetString(DcmDataset* dataset, const DcmTag& tag, const std::string& value) {
    if (!value.empty()) {
        dataset->putAndInsertString(tag, value.c_str());
    }
}

}  // namespace

class DicomSRWriter::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    ProgressCallback progressCallback;

    void reportProgress(double progress, const QString& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
        spdlog::debug("SR Writer: {} ({:.1f}%)", status.toStdString(), progress * 100.0);
    }

    std::expected<SRCreationResult, SRError> createSR(
        const SRContent& content,
        const SRWriterOptions& options) const {

        reportProgress(0.0, "Creating SR document...");

        // Create SR document
        DSRDocument doc;

        // Set document type to Comprehensive SR
        if (doc.createNewDocument(DSRTypes::DT_ComprehensiveSR).bad()) {
            return std::unexpected(SRError{
                SRError::Code::InternalError,
                "Failed to create SR document"
            });
        }

        reportProgress(0.1, "Setting patient information...");

        // Set patient information using DSRDocument methods
        if (options.includePatientInfo) {
            doc.setPatientName(content.patient.patientName.c_str());
            doc.setPatientID(content.patient.patientId.c_str());
            doc.setPatientBirthDate(content.patient.patientBirthDate.c_str());
            doc.setPatientSex(content.patient.patientSex.c_str());
        }

        reportProgress(0.2, "Setting study information...");

        // Set study information using DSRDocument methods
        if (options.includeStudyInfo) {
            doc.setStudyDate(content.study.studyDate.c_str());
            doc.setStudyTime(content.study.studyTime.c_str());
            doc.setStudyDescription(content.study.studyDescription.c_str());
            doc.setAccessionNumber(content.study.accessionNumber.c_str());
            doc.setReferringPhysicianName(content.study.referringPhysicianName.c_str());
        }

        // Generate new UIDs for the SR
        char uidBuffer[128];
        std::string srSeriesUid = dcmGenerateUniqueIdentifier(uidBuffer, SITE_SERIES_UID_ROOT);
        std::string srSopUid = dcmGenerateUniqueIdentifier(uidBuffer, SITE_INSTANCE_UID_ROOT);

        // Set series information
        doc.setSeriesDescription(options.seriesDescription.toStdString().c_str());
        doc.setSeriesNumber(std::to_string(options.seriesNumber).c_str());

        // Set instance information
        doc.setInstanceNumber(std::to_string(options.instanceNumber).c_str());

        // Set manufacturer information
        doc.setManufacturer(options.manufacturer.toStdString().c_str());

        // Set content date/time
        std::string contentDate = formatDateTime(content.performedDateTime, "%Y%m%d");
        std::string contentTime = formatDateTime(content.performedDateTime, "%H%M%S");
        doc.setContentDate(contentDate.c_str());
        doc.setContentTime(contentTime.c_str());

        reportProgress(0.3, "Building content tree...");

        // Get the document tree for content manipulation
        DSRDocumentTree& tree = doc.getTree();

        // Set document title (root container)
        tree.addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            toDsrCode(SRCodes::ImagingMeasurementReport)
        );

        reportProgress(0.4, "Adding measurements...");

        size_t totalMeasurements = 0;

        // Add distance measurements
        if (!content.distances.empty()) {
            addMeasurementContainer(tree, "Distance Measurements");

            for (const auto& dist : content.distances) {
                addDistanceMeasurement(tree, dist, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }

            tree.goUp();  // Back to root
        }

        reportProgress(0.5, "Adding angle measurements...");

        // Add angle measurements
        if (!content.angles.empty()) {
            addMeasurementContainer(tree, "Angle Measurements");

            for (const auto& angle : content.angles) {
                addAngleMeasurement(tree, angle, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }

            tree.goUp();  // Back to root
        }

        reportProgress(0.6, "Adding area measurements...");

        // Add area measurements
        if (!content.areas.empty()) {
            addMeasurementContainer(tree, "Area Measurements");

            for (const auto& area : content.areas) {
                addAreaMeasurement(tree, area, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }

            tree.goUp();  // Back to root
        }

        reportProgress(0.7, "Adding volume measurements...");

        // Add volume measurements
        if (!content.volumes.empty()) {
            addMeasurementContainer(tree, "Volume Measurements");

            for (const auto& vol : content.volumes) {
                addVolumeMeasurement(tree, vol);
                ++totalMeasurements;
            }

            tree.goUp();  // Back to root
        }

        reportProgress(0.8, "Adding ROI statistics...");

        // Add ROI statistics if requested
        if (options.includeROIStatistics && !content.roiStatistics.empty()) {
            addMeasurementContainer(tree, "ROI Statistics");

            for (const auto& stats : content.roiStatistics) {
                addROIStatistics(tree, stats);
                ++totalMeasurements;
            }

            tree.goUp();  // Back to root
        }

        reportProgress(0.9, "Finalizing document...");

        // Add referenced images
        for (const auto& refSopUid : content.referencedSopInstanceUids) {
            tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_UIDRef);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("121232", "DCM", "Referenced SOP Instance UID")
            );
            tree.getCurrentContentItem().setStringValue(refSopUid.c_str());
            tree.goUp();
        }

        reportProgress(1.0, "SR document created successfully");

        spdlog::info("Created DICOM SR with {} measurements, SOP Instance UID: {}",
                     totalMeasurements, srSopUid);

        return SRCreationResult{
            .sopInstanceUid = srSopUid,
            .seriesInstanceUid = srSeriesUid,
            .filePath = std::nullopt,
            .measurementCount = totalMeasurements
        };
    }

    std::expected<SRCreationResult, SRError> saveToFile(
        const SRContent& content,
        const std::filesystem::path& outputPath,
        const SRWriterOptions& options) const {

        // Validate output path
        auto parentPath = outputPath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
            return std::unexpected(SRError{
                SRError::Code::FileAccessDenied,
                "Output directory does not exist: " + parentPath.string()
            });
        }

        reportProgress(0.0, "Creating SR for file export...");

        // Create the SR document
        DSRDocument doc;
        if (doc.createNewDocument(DSRTypes::DT_ComprehensiveSR).bad()) {
            return std::unexpected(SRError{
                SRError::Code::InternalError,
                "Failed to create SR document"
            });
        }

        // Populate the document
        if (options.includePatientInfo) {
            doc.setPatientName(content.patient.patientName.c_str());
            doc.setPatientID(content.patient.patientId.c_str());
            doc.setPatientBirthDate(content.patient.patientBirthDate.c_str());
            doc.setPatientSex(content.patient.patientSex.c_str());
        }

        if (options.includeStudyInfo) {
            doc.setStudyDate(content.study.studyDate.c_str());
            doc.setStudyTime(content.study.studyTime.c_str());
            doc.setStudyDescription(content.study.studyDescription.c_str());
            doc.setAccessionNumber(content.study.accessionNumber.c_str());
        }

        char uidBuffer[128];
        std::string srSeriesUid = dcmGenerateUniqueIdentifier(uidBuffer, SITE_SERIES_UID_ROOT);
        std::string srSopUid = dcmGenerateUniqueIdentifier(uidBuffer, SITE_INSTANCE_UID_ROOT);

        doc.setSeriesDescription(options.seriesDescription.toStdString().c_str());
        doc.setSeriesNumber(std::to_string(options.seriesNumber).c_str());
        doc.setInstanceNumber(std::to_string(options.instanceNumber).c_str());
        doc.setManufacturer(options.manufacturer.toStdString().c_str());

        std::string contentDate = formatDateTime(content.performedDateTime, "%Y%m%d");
        std::string contentTime = formatDateTime(content.performedDateTime, "%H%M%S");
        doc.setContentDate(contentDate.c_str());
        doc.setContentTime(contentTime.c_str());

        reportProgress(0.3, "Building content tree...");

        DSRDocumentTree& tree = doc.getTree();
        tree.addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            toDsrCode(SRCodes::ImagingMeasurementReport)
        );

        size_t totalMeasurements = 0;

        // Add all measurements
        if (!content.distances.empty()) {
            addMeasurementContainer(tree, "Distance Measurements");
            for (const auto& dist : content.distances) {
                addDistanceMeasurement(tree, dist, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (!content.angles.empty()) {
            addMeasurementContainer(tree, "Angle Measurements");
            for (const auto& angle : content.angles) {
                addAngleMeasurement(tree, angle, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (!content.areas.empty()) {
            addMeasurementContainer(tree, "Area Measurements");
            for (const auto& area : content.areas) {
                addAreaMeasurement(tree, area, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (!content.volumes.empty()) {
            addMeasurementContainer(tree, "Volume Measurements");
            for (const auto& vol : content.volumes) {
                addVolumeMeasurement(tree, vol);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (options.includeROIStatistics && !content.roiStatistics.empty()) {
            addMeasurementContainer(tree, "ROI Statistics");
            for (const auto& stats : content.roiStatistics) {
                addROIStatistics(tree, stats);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        reportProgress(0.7, "Writing to DICOM file...");

        // Write to DICOM file
        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();

        OFCondition status = doc.write(*dataset);
        if (status.bad()) {
            return std::unexpected(SRError{
                SRError::Code::EncodingFailed,
                std::string("Failed to write SR document: ") + status.text()
            });
        }

        // Set UIDs in the dataset (these are not automatically set by DSRDocument)
        setDatasetString(dataset, DCM_StudyInstanceUID, content.study.studyInstanceUid);
        setDatasetString(dataset, DCM_SeriesInstanceUID, srSeriesUid);
        setDatasetString(dataset, DCM_SOPInstanceUID, srSopUid);
        setDatasetString(dataset, DCM_SOPClassUID, UID_ComprehensiveSRStorage);
        setDatasetString(dataset, DCM_Modality, "SR");

        // Set additional metadata
        setDatasetString(dataset, DCM_InstitutionName, content.institutionName);
        setDatasetString(dataset, DCM_OperatorsName, content.operatorName);
        setDatasetString(dataset, DCM_SoftwareVersions, options.softwareVersion.toStdString());

        // Set meta header
        fileFormat.getMetaInfo()->putAndInsertString(DCM_MediaStorageSOPClassUID, UID_ComprehensiveSRStorage);
        fileFormat.getMetaInfo()->putAndInsertString(DCM_MediaStorageSOPInstanceUID, srSopUid.c_str());
        fileFormat.getMetaInfo()->putAndInsertString(DCM_TransferSyntaxUID, UID_LittleEndianExplicitTransferSyntax);

        status = fileFormat.saveFile(
            outputPath.string().c_str(),
            EXS_LittleEndianExplicit
        );

        if (status.bad()) {
            return std::unexpected(SRError{
                SRError::Code::FileAccessDenied,
                std::string("Failed to save file: ") + status.text()
            });
        }

        reportProgress(1.0, "SR file saved successfully");

        spdlog::info("Saved DICOM SR to: {}", outputPath.string());

        return SRCreationResult{
            .sopInstanceUid = srSopUid,
            .seriesInstanceUid = srSeriesUid,
            .filePath = outputPath,
            .measurementCount = totalMeasurements
        };
    }

    std::expected<SRCreationResult, SRError> storeToPacs(
        const SRContent& content,
        const PacsServerConfig& pacsConfig,
        const SRWriterOptions& options) const {

        if (!pacsConfig.isValid()) {
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                "Invalid PACS configuration"
            });
        }

        reportProgress(0.0, "Creating SR for PACS storage...");

        // Create the SR document
        DSRDocument doc;
        if (doc.createNewDocument(DSRTypes::DT_ComprehensiveSR).bad()) {
            return std::unexpected(SRError{
                SRError::Code::InternalError,
                "Failed to create SR document"
            });
        }

        // Populate document
        if (options.includePatientInfo) {
            doc.setPatientName(content.patient.patientName.c_str());
            doc.setPatientID(content.patient.patientId.c_str());
            doc.setPatientBirthDate(content.patient.patientBirthDate.c_str());
            doc.setPatientSex(content.patient.patientSex.c_str());
        }

        if (options.includeStudyInfo) {
            doc.setStudyDate(content.study.studyDate.c_str());
            doc.setStudyTime(content.study.studyTime.c_str());
            doc.setStudyDescription(content.study.studyDescription.c_str());
            doc.setAccessionNumber(content.study.accessionNumber.c_str());
        }

        char uidBuffer[128];
        std::string srSeriesUid = dcmGenerateUniqueIdentifier(uidBuffer, SITE_SERIES_UID_ROOT);
        std::string srSopUid = dcmGenerateUniqueIdentifier(uidBuffer, SITE_INSTANCE_UID_ROOT);

        doc.setSeriesDescription(options.seriesDescription.toStdString().c_str());
        doc.setSeriesNumber(std::to_string(options.seriesNumber).c_str());
        doc.setInstanceNumber(std::to_string(options.instanceNumber).c_str());
        doc.setManufacturer(options.manufacturer.toStdString().c_str());

        std::string contentDate = formatDateTime(content.performedDateTime, "%Y%m%d");
        std::string contentTime = formatDateTime(content.performedDateTime, "%H%M%S");
        doc.setContentDate(contentDate.c_str());
        doc.setContentTime(contentTime.c_str());

        DSRDocumentTree& tree = doc.getTree();
        tree.addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            toDsrCode(SRCodes::ImagingMeasurementReport)
        );

        size_t totalMeasurements = 0;

        if (!content.distances.empty()) {
            addMeasurementContainer(tree, "Distance Measurements");
            for (const auto& dist : content.distances) {
                addDistanceMeasurement(tree, dist, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (!content.angles.empty()) {
            addMeasurementContainer(tree, "Angle Measurements");
            for (const auto& angle : content.angles) {
                addAngleMeasurement(tree, angle, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (!content.areas.empty()) {
            addMeasurementContainer(tree, "Area Measurements");
            for (const auto& area : content.areas) {
                addAreaMeasurement(tree, area, options.includeSpatialCoordinates);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (!content.volumes.empty()) {
            addMeasurementContainer(tree, "Volume Measurements");
            for (const auto& vol : content.volumes) {
                addVolumeMeasurement(tree, vol);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        if (options.includeROIStatistics && !content.roiStatistics.empty()) {
            addMeasurementContainer(tree, "ROI Statistics");
            for (const auto& stats : content.roiStatistics) {
                addROIStatistics(tree, stats);
                ++totalMeasurements;
            }
            tree.goUp();
        }

        reportProgress(0.4, "Preparing for PACS transfer...");

        // Create DICOM file format for transfer
        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();

        OFCondition status = doc.write(*dataset);
        if (status.bad()) {
            return std::unexpected(SRError{
                SRError::Code::EncodingFailed,
                std::string("Failed to encode SR document: ") + status.text()
            });
        }

        // Set UIDs in the dataset
        setDatasetString(dataset, DCM_StudyInstanceUID, content.study.studyInstanceUid);
        setDatasetString(dataset, DCM_SeriesInstanceUID, srSeriesUid);
        setDatasetString(dataset, DCM_SOPInstanceUID, srSopUid);
        setDatasetString(dataset, DCM_SOPClassUID, UID_ComprehensiveSRStorage);
        setDatasetString(dataset, DCM_Modality, "SR");

        reportProgress(0.5, "Connecting to PACS...");

        // Initialize network
        T_ASC_Network* network = nullptr;
        OFCondition cond = ASC_initializeNetwork(
            NET_REQUESTOR,
            0,
            static_cast<int>(pacsConfig.connectionTimeout.count()),
            &network
        );

        if (cond.bad()) {
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                std::string("Failed to initialize network: ") + cond.text()
            });
        }

        // Create association parameters
        T_ASC_Parameters* params = nullptr;
        cond = ASC_createAssociationParameters(&params, pacsConfig.maxPduSize);
        if (cond.bad()) {
            ASC_dropNetwork(&network);
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                std::string("Failed to create association parameters: ") + cond.text()
            });
        }

        ASC_setAPTitles(
            params,
            pacsConfig.callingAeTitle.c_str(),
            pacsConfig.calledAeTitle.c_str(),
            nullptr
        );

        std::string peerAddress = pacsConfig.hostname + ":" + std::to_string(pacsConfig.port);
        ASC_setPresentationAddresses(
            params,
            OFStandard::getHostName().c_str(),
            peerAddress.c_str()
        );

        // Add Comprehensive SR Storage presentation context
        const char* transferSyntaxes[] = {
            UID_LittleEndianExplicitTransferSyntax,
            UID_BigEndianExplicitTransferSyntax,
            UID_LittleEndianImplicitTransferSyntax
        };

        cond = ASC_addPresentationContext(
            params,
            1,
            UID_ComprehensiveSRStorage,
            transferSyntaxes,
            3
        );

        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                std::string("Failed to add presentation context: ") + cond.text()
            });
        }

        reportProgress(0.6, "Establishing association...");

        // Create association
        T_ASC_Association* assoc = nullptr;
        cond = ASC_requestAssociation(network, params, &assoc);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            ASC_dropNetwork(&network);
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                std::string("Failed to establish association: ") + cond.text()
            });
        }

        // Check if SR Storage was accepted
        T_ASC_PresentationContextID presId = ASC_findAcceptedPresentationContextID(
            assoc, UID_ComprehensiveSRStorage
        );

        if (presId == 0) {
            ASC_releaseAssociation(assoc);
            ASC_destroyAssociation(&assoc);
            ASC_dropNetwork(&network);
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                "Comprehensive SR Storage not accepted by PACS"
            });
        }

        reportProgress(0.7, "Sending SR to PACS...");

        // Prepare C-STORE request
        T_DIMSE_C_StoreRQ req;
        memset(&req, 0, sizeof(req));
        req.MessageID = assoc->nextMsgID++;
        strncpy(req.AffectedSOPClassUID, UID_ComprehensiveSRStorage, sizeof(req.AffectedSOPClassUID) - 1);
        strncpy(req.AffectedSOPInstanceUID, srSopUid.c_str(), sizeof(req.AffectedSOPInstanceUID) - 1);
        req.DataSetType = DIMSE_DATASET_PRESENT;
        req.Priority = DIMSE_PRIORITY_MEDIUM;

        T_DIMSE_C_StoreRSP rsp;
        DcmDataset* statusDetail = nullptr;

        cond = DIMSE_storeUser(
            assoc,
            presId,
            &req,
            nullptr,  // no file name
            dataset,
            nullptr,  // no progress callback
            nullptr,  // no callback data
            DIMSE_BLOCKING,
            static_cast<int>(pacsConfig.dimseTimeout.count()),
            &rsp,
            &statusDetail
        );

        delete statusDetail;

        // Clean up association
        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        ASC_dropNetwork(&network);

        if (cond.bad()) {
            return std::unexpected(SRError{
                SRError::Code::PacsStoreFailed,
                std::string("C-STORE failed: ") + cond.text()
            });
        }

        if (rsp.DimseStatus != STATUS_Success) {
            return std::unexpected(SRError{
                SRError::Code::PacsStoreFailed,
                "C-STORE returned status: " + std::to_string(rsp.DimseStatus)
            });
        }

        reportProgress(1.0, "SR stored to PACS successfully");

        spdlog::info("Stored DICOM SR to PACS: {} (SOP Instance UID: {})",
                     pacsConfig.calledAeTitle, srSopUid);

        return SRCreationResult{
            .sopInstanceUid = srSopUid,
            .seriesInstanceUid = srSeriesUid,
            .filePath = std::nullopt,
            .measurementCount = totalMeasurements
        };
    }

    SRValidationResult validate(const SRContent& content) const {
        SRValidationResult result;
        result.valid = true;

        // Check patient info
        if (content.patient.patientId.empty()) {
            result.warnings.push_back("Patient ID is empty");
        }

        if (content.patient.patientName.empty()) {
            result.warnings.push_back("Patient name is empty");
        }

        // Check study info
        if (content.study.studyInstanceUid.empty()) {
            result.errors.push_back("Study Instance UID is required");
            result.valid = false;
        }

        // Check for content
        bool hasContent = !content.distances.empty() ||
                          !content.angles.empty() ||
                          !content.areas.empty() ||
                          !content.volumes.empty() ||
                          !content.roiStatistics.empty();

        if (!hasContent) {
            result.errors.push_back("No measurements or content provided");
            result.valid = false;
        }

        // Validate measurements
        for (const auto& dist : content.distances) {
            if (dist.distanceMm < 0) {
                result.errors.push_back("Distance measurement has negative value: " + dist.label);
                result.valid = false;
            }
        }

        for (const auto& angle : content.angles) {
            if (angle.angleDegrees < 0 || angle.angleDegrees > 360) {
                result.warnings.push_back("Angle measurement outside expected range: " + angle.label);
            }
        }

        for (const auto& area : content.areas) {
            if (area.areaMm2 < 0) {
                result.errors.push_back("Area measurement has negative value: " + area.label);
                result.valid = false;
            }
        }

        for (const auto& vol : content.volumes) {
            if (vol.volumeMm3 < 0) {
                result.errors.push_back("Volume measurement has negative value: " + vol.labelName);
                result.valid = false;
            }
        }

        return result;
    }

private:
    void addMeasurementContainer(DSRDocumentTree& tree, const std::string& title) const {
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            DSRCodedEntryValue("125007", "DCM", title.c_str())
        );
        tree.goDown();  // Enter the container
    }

    void addDistanceMeasurement(DSRDocumentTree& tree,
                               const DistanceMeasurement& dist,
                               bool includeSpatialCoords) const {
        // Add measurement container
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            DSRCodedEntryValue("125309", "DCM", "Measurement Group")
        );
        tree.goDown();

        // Add tracking identifier
        if (!dist.label.empty()) {
            tree.addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("112039", "DCM", "Tracking Identifier")
            );
            tree.getCurrentContentItem().setStringValue(dist.label.c_str());
        }

        // Add numeric measurement
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Length));

        DSRNumericMeasurementValue numValue(
            std::to_string(dist.distanceMm).c_str(),
            toDsrCode(SRCodes::Millimeter)
        );
        tree.getCurrentContentItem().setNumericValue(numValue);

        // Add spatial coordinates if requested
        if (includeSpatialCoords) {
            // Point 1
            tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_SCoord3D);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("111030", "DCM", "Image Region")
            );

            DSRSpatialCoordinates3DValue scoord3d(DSRTypes::GT3_Point);
            scoord3d.getGraphicDataList().addItem(
                static_cast<Float64>(dist.point1[0]),
                static_cast<Float64>(dist.point1[1]),
                static_cast<Float64>(dist.point1[2])
            );
            tree.getCurrentContentItem().setSpatialCoordinates3D(scoord3d);

            // Point 2
            tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_SCoord3D);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("111030", "DCM", "Image Region")
            );

            DSRSpatialCoordinates3DValue scoord3d2(DSRTypes::GT3_Point);
            scoord3d2.getGraphicDataList().addItem(
                static_cast<Float64>(dist.point2[0]),
                static_cast<Float64>(dist.point2[1]),
                static_cast<Float64>(dist.point2[2])
            );
            tree.getCurrentContentItem().setSpatialCoordinates3D(scoord3d2);
        }

        tree.goUp();  // Back to parent
    }

    void addAngleMeasurement(DSRDocumentTree& tree,
                            const AngleMeasurement& angle,
                            bool includeSpatialCoords) const {
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            DSRCodedEntryValue("125309", "DCM", "Measurement Group")
        );
        tree.goDown();

        if (!angle.label.empty()) {
            tree.addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("112039", "DCM", "Tracking Identifier")
            );
            tree.getCurrentContentItem().setStringValue(angle.label.c_str());
        }

        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Angle));

        DSRNumericMeasurementValue numValue(
            std::to_string(angle.angleDegrees).c_str(),
            toDsrCode(SRCodes::Degree)
        );
        tree.getCurrentContentItem().setNumericValue(numValue);

        if (includeSpatialCoords) {
            // Vertex point
            tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_SCoord3D);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("111030", "DCM", "Image Region")
            );

            DSRSpatialCoordinates3DValue scoord3d(DSRTypes::GT3_Point);
            scoord3d.getGraphicDataList().addItem(
                static_cast<Float64>(angle.vertex[0]),
                static_cast<Float64>(angle.vertex[1]),
                static_cast<Float64>(angle.vertex[2])
            );
            tree.getCurrentContentItem().setSpatialCoordinates3D(scoord3d);
        }

        tree.goUp();
    }

    void addAreaMeasurement(DSRDocumentTree& tree,
                           const AreaMeasurement& area,
                           bool includeSpatialCoords) const {
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            DSRCodedEntryValue("125309", "DCM", "Measurement Group")
        );
        tree.goDown();

        if (!area.label.empty()) {
            tree.addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("112039", "DCM", "Tracking Identifier")
            );
            tree.getCurrentContentItem().setStringValue(area.label.c_str());
        }

        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Area));

        DSRNumericMeasurementValue numValue(
            std::to_string(area.areaMm2).c_str(),
            toDsrCode(SRCodes::SquareMillimeter)
        );
        tree.getCurrentContentItem().setNumericValue(numValue);

        if (includeSpatialCoords && !area.points.empty()) {
            tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_SCoord3D);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("111030", "DCM", "Image Region")
            );

            DSRSpatialCoordinates3DValue scoord3d(DSRTypes::GT3_Polygon);
            for (const auto& pt : area.points) {
                scoord3d.getGraphicDataList().addItem(
                    static_cast<Float64>(pt[0]),
                    static_cast<Float64>(pt[1]),
                    static_cast<Float64>(pt[2])
                );
            }
            tree.getCurrentContentItem().setSpatialCoordinates3D(scoord3d);
        }

        tree.goUp();
    }

    void addVolumeMeasurement(DSRDocumentTree& tree,
                             const VolumeResult& vol) const {
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            DSRCodedEntryValue("125309", "DCM", "Measurement Group")
        );
        tree.goDown();

        if (!vol.labelName.empty()) {
            tree.addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("112039", "DCM", "Tracking Identifier")
            );
            tree.getCurrentContentItem().setStringValue(vol.labelName.c_str());
        }

        // Volume in cubic centimeters
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Volume));

        double volumeCm3 = vol.volumeMm3 / 1000.0;  // Convert mm3 to cm3
        DSRNumericMeasurementValue numValue(
            std::to_string(volumeCm3).c_str(),
            toDsrCode(SRCodes::CubicCentimeter)
        );
        tree.getCurrentContentItem().setNumericValue(numValue);

        // Surface area if available
        if (vol.surfaceAreaMm2.has_value() && vol.surfaceAreaMm2.value() > 0) {
            tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("118565009", "SCT", "Surface Area")
            );

            DSRNumericMeasurementValue surfaceValue(
                std::to_string(vol.surfaceAreaMm2.value()).c_str(),
                toDsrCode(SRCodes::SquareMillimeter)
            );
            tree.getCurrentContentItem().setNumericValue(surfaceValue);
        }

        tree.goUp();
    }

    void addROIStatistics(DSRDocumentTree& tree,
                         const SRROIStatistics& stats) const {
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container);
        tree.getCurrentContentItem().setConceptName(
            DSRCodedEntryValue("125309", "DCM", "Measurement Group")
        );
        tree.goDown();

        if (!stats.label.empty()) {
            tree.addContentItem(DSRTypes::RT_hasObsContext, DSRTypes::VT_Text);
            tree.getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("112039", "DCM", "Tracking Identifier")
            );
            tree.getCurrentContentItem().setStringValue(stats.label.c_str());
        }

        // Mean
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Mean));
        DSRNumericMeasurementValue meanValue(
            std::to_string(stats.mean).c_str(),
            toDsrCode(SRCodes::HounsfieldUnit)
        );
        tree.getCurrentContentItem().setNumericValue(meanValue);

        // Standard deviation
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::StandardDeviation));
        DSRNumericMeasurementValue stdValue(
            std::to_string(stats.stdDev).c_str(),
            toDsrCode(SRCodes::HounsfieldUnit)
        );
        tree.getCurrentContentItem().setNumericValue(stdValue);

        // Minimum
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Minimum));
        DSRNumericMeasurementValue minValue(
            std::to_string(stats.min).c_str(),
            toDsrCode(SRCodes::HounsfieldUnit)
        );
        tree.getCurrentContentItem().setNumericValue(minValue);

        // Maximum
        tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
        tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Maximum));
        DSRNumericMeasurementValue maxValue(
            std::to_string(stats.max).c_str(),
            toDsrCode(SRCodes::HounsfieldUnit)
        );
        tree.getCurrentContentItem().setNumericValue(maxValue);

        // Area
        if (stats.areaMm2 > 0) {
            tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Num);
            tree.getCurrentContentItem().setConceptName(toDsrCode(SRCodes::Area));
            DSRNumericMeasurementValue areaValue(
                std::to_string(stats.areaMm2).c_str(),
                toDsrCode(SRCodes::SquareMillimeter)
            );
            tree.getCurrentContentItem().setNumericValue(areaValue);
        }

        tree.goUp();
    }
};

// Public interface implementation

DicomSRWriter::DicomSRWriter()
    : impl_(std::make_unique<Impl>()) {
}

DicomSRWriter::~DicomSRWriter() = default;

DicomSRWriter::DicomSRWriter(DicomSRWriter&&) noexcept = default;
DicomSRWriter& DicomSRWriter::operator=(DicomSRWriter&&) noexcept = default;

void DicomSRWriter::setProgressCallback(ProgressCallback callback) {
    impl_->progressCallback = std::move(callback);
}

std::expected<SRCreationResult, SRError> DicomSRWriter::createSR(
    const SRContent& content,
    const SRWriterOptions& options) const {
    return impl_->createSR(content, options);
}

std::expected<SRCreationResult, SRError> DicomSRWriter::saveToFile(
    const SRContent& content,
    const std::filesystem::path& outputPath,
    const SRWriterOptions& options) const {
    return impl_->saveToFile(content, outputPath, options);
}

std::expected<SRCreationResult, SRError> DicomSRWriter::storeToPacs(
    const SRContent& content,
    const PacsServerConfig& pacsConfig,
    const SRWriterOptions& options) const {
    return impl_->storeToPacs(content, pacsConfig, options);
}

SRValidationResult DicomSRWriter::validate(const SRContent& content) const {
    return impl_->validate(content);
}

std::string DicomSRWriter::generateUid() {
    char buffer[128];
    return dcmGenerateUniqueIdentifier(buffer, SITE_INSTANCE_UID_ROOT);
}

std::vector<std::string> DicomSRWriter::getSupportedSopClasses() {
    return {
        COMPREHENSIVE_SR_SOP_CLASS,
        ENHANCED_SR_SOP_CLASS
    };
}

std::vector<DicomCode> DicomSRWriter::getAnatomicRegionCodes() {
    return {
        SRCodes::Liver,
        SRCodes::Lung,
        SRCodes::Kidney,
        SRCodes::Brain,
        SRCodes::Heart,
        SRCodes::Spine,
        SRCodes::Abdomen,
        SRCodes::Chest,
        SRCodes::Pelvis
    };
}

}  // namespace dicom_viewer::services

#include "services/export/dicom_sr_writer.hpp"

#include <QDateTime>
#include <QString>

#include <atomic>
#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>

#include <kcenon/common/logging/log_macros.h>

// pacs_system headers
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>
#include <pacs/network/association.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/services/sop_classes/sr_storage.hpp>
#include <pacs/services/storage_scu.hpp>

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

// SR-related DICOM tags (not in pacs_system's tag_constants.hpp)
namespace sr_tags {
    inline constexpr pacs::core::dicom_tag value_type{0x0040, 0xA040};
    inline constexpr pacs::core::dicom_tag concept_name_code_sequence{0x0040, 0xA043};
    inline constexpr pacs::core::dicom_tag content_sequence{0x0040, 0xA730};
    inline constexpr pacs::core::dicom_tag relationship_type{0x0040, 0xA010};
    inline constexpr pacs::core::dicom_tag text_value{0x0040, 0xA160};
    inline constexpr pacs::core::dicom_tag measured_value_sequence{0x0040, 0xA300};
    inline constexpr pacs::core::dicom_tag measurement_units_code_sequence{0x0040, 0x08EA};
    inline constexpr pacs::core::dicom_tag numeric_value{0x0040, 0xA30A};
    inline constexpr pacs::core::dicom_tag code_value{0x0008, 0x0100};
    inline constexpr pacs::core::dicom_tag coding_scheme_designator{0x0008, 0x0102};
    inline constexpr pacs::core::dicom_tag code_meaning{0x0008, 0x0104};
    inline constexpr pacs::core::dicom_tag graphic_type{0x0070, 0x0023};
    inline constexpr pacs::core::dicom_tag graphic_data{0x0070, 0x0022};
    inline constexpr pacs::core::dicom_tag referenced_frame_of_reference_uid{0x3006, 0x0024};
    inline constexpr pacs::core::dicom_tag completion_flag{0x0040, 0xA491};
    inline constexpr pacs::core::dicom_tag verification_flag{0x0040, 0xA493};
    inline constexpr pacs::core::dicom_tag uid_value{0x0040, 0xA124};
}

// Helper to create a coded entry dataset
pacs::core::dicom_dataset createCodedEntry(const DicomCode& code) {
    using namespace pacs::encoding;
    pacs::core::dicom_dataset ds;
    ds.set_string(sr_tags::code_value, vr_type::SH, code.value);
    ds.set_string(sr_tags::coding_scheme_designator, vr_type::SH, code.scheme);
    ds.set_string(sr_tags::code_meaning, vr_type::LO, code.meaning);
    return ds;
}

// Helper to create a content item with concept name
pacs::core::dicom_dataset createContentItem(
    const std::string& valueType,
    const std::string& relationshipType,
    const DicomCode& conceptName
) {
    using namespace pacs::encoding;
    pacs::core::dicom_dataset item;
    item.set_string(sr_tags::value_type, vr_type::CS, valueType);
    item.set_string(sr_tags::relationship_type, vr_type::CS, relationshipType);

    auto& conceptSeq = item.get_or_create_sequence(sr_tags::concept_name_code_sequence);
    conceptSeq.push_back(createCodedEntry(conceptName));

    return item;
}

// Generate a unique DICOM UID
std::string generatePacsUid() {
    // Use implementation UID root + timestamp + random
    static const std::string uidRoot = "1.2.826.0.1.3680043.9.7125.";
    static std::atomic<uint32_t> counter{0};

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 999999);

    std::ostringstream oss;
    oss << uidRoot << ms << "." << counter.fetch_add(1) << "." << dist(gen);
    return oss.str();
}

}  // namespace

// pacs_system-based implementation
class DicomSRWriter::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    ProgressCallback progressCallback;

    void reportProgress(double progress, const QString& status) const {
        if (progressCallback) {
            progressCallback(progress, status);
        }
        LOG_DEBUG(std::format("SR Writer: {} ({:.1f}%)", status.toStdString(), progress * 100.0));
    }

    std::expected<SRCreationResult, SRError> createSR(
        const SRContent& content,
        const SRWriterOptions& options) const {

        reportProgress(0.0, "Creating SR document...");

        // Generate UIDs
        std::string srSeriesUid = generatePacsUid();
        std::string srSopUid = generatePacsUid();

        // Build SR dataset
        auto srDataset = buildSRDataset(content, options, srSeriesUid, srSopUid);
        if (!srDataset) {
            return std::unexpected(srDataset.error());
        }

        size_t totalMeasurements = countMeasurements(content);

        reportProgress(1.0, "SR document created successfully");

        LOG_INFO(std::format("Created DICOM SR with {} measurements, SOP Instance UID: {}",
                     totalMeasurements, srSopUid));

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

        // Generate UIDs
        std::string srSeriesUid = generatePacsUid();
        std::string srSopUid = generatePacsUid();

        // Build SR dataset
        auto srDataset = buildSRDataset(content, options, srSeriesUid, srSopUid);
        if (!srDataset) {
            return std::unexpected(srDataset.error());
        }

        reportProgress(0.7, "Writing to DICOM file...");

        // Create DICOM file and save
        auto file = pacs::core::dicom_file::create(
            std::move(*srDataset),
            pacs::encoding::transfer_syntax::explicit_vr_little_endian
        );

        auto saveResult = file.save(outputPath);
        if (!saveResult.is_ok()) {
            return std::unexpected(SRError{
                SRError::Code::FileAccessDenied,
                "Failed to save file: " + saveResult.error().message
            });
        }

        size_t totalMeasurements = countMeasurements(content);

        reportProgress(1.0, "SR file saved successfully");

        LOG_INFO(std::format("Saved DICOM SR to: {}", outputPath.string()));

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

        // Generate UIDs
        std::string srSeriesUid = generatePacsUid();
        std::string srSopUid = generatePacsUid();

        // Build SR dataset
        auto srDataset = buildSRDataset(content, options, srSeriesUid, srSopUid);
        if (!srDataset) {
            return std::unexpected(srDataset.error());
        }

        reportProgress(0.4, "Connecting to PACS...");

        // Build association configuration
        pacs::network::association_config assocConfig;
        assocConfig.calling_ae_title = pacsConfig.callingAeTitle;
        assocConfig.called_ae_title = pacsConfig.calledAeTitle;
        assocConfig.max_pdu_length = pacsConfig.maxPduSize;

        // Add Comprehensive SR Storage presentation context
        pacs::network::proposed_presentation_context srCtx;
        srCtx.id = 1;
        srCtx.abstract_syntax = std::string(pacs::services::sop_classes::comprehensive_sr_storage_uid);
        srCtx.transfer_syntaxes = {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2.2",  // Explicit VR Big Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        };
        assocConfig.proposed_contexts.push_back(srCtx);

        // Connect to PACS
        auto timeout = std::chrono::duration_cast<pacs::network::association::duration>(
            pacsConfig.connectionTimeout
        );
        auto connectResult = pacs::network::association::connect(
            pacsConfig.hostname,
            pacsConfig.port,
            assocConfig,
            timeout
        );

        if (!connectResult.is_ok()) {
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                "Failed to connect to PACS: " + connectResult.error().message
            });
        }

        auto assoc = std::move(connectResult.value());

        // Check if SR Storage was accepted
        if (!assoc.has_accepted_context(std::string(pacs::services::sop_classes::comprehensive_sr_storage_uid))) {
            assoc.abort();
            return std::unexpected(SRError{
                SRError::Code::PacsConnectionFailed,
                "Comprehensive SR Storage not accepted by PACS"
            });
        }

        reportProgress(0.6, "Sending SR to PACS...");

        // Store the SR
        pacs::services::storage_scu scu;
        auto storeResult = scu.store(assoc, *srDataset);

        // Release association
        auto dimseTimeout = std::chrono::duration_cast<pacs::network::association::duration>(
            pacsConfig.dimseTimeout
        );
        (void)assoc.release(dimseTimeout);

        if (!storeResult.is_ok()) {
            return std::unexpected(SRError{
                SRError::Code::PacsStoreFailed,
                "C-STORE failed: " + storeResult.error().message
            });
        }

        if (!storeResult.value().is_success()) {
            return std::unexpected(SRError{
                SRError::Code::PacsStoreFailed,
                "C-STORE returned status: " + std::to_string(storeResult.value().status)
            });
        }

        size_t totalMeasurements = countMeasurements(content);

        reportProgress(1.0, "SR stored to PACS successfully");

        LOG_INFO(std::format("Stored DICOM SR to PACS: {} (SOP Instance UID: {})",
                     pacsConfig.calledAeTitle, srSopUid));

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
    size_t countMeasurements(const SRContent& content) const {
        return content.distances.size() +
               content.angles.size() +
               content.areas.size() +
               content.volumes.size() +
               content.roiStatistics.size();
    }

    std::expected<pacs::core::dicom_dataset, SRError> buildSRDataset(
        const SRContent& content,
        const SRWriterOptions& options,
        const std::string& seriesUid,
        const std::string& sopUid) const {

        using namespace pacs::core;
        using namespace pacs::encoding;

        dicom_dataset ds;

        // SOP Common Module
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      std::string(pacs::services::sop_classes::comprehensive_sr_storage_uid));
        ds.set_string(tags::sop_instance_uid, vr_type::UI, sopUid);

        // Patient Module
        if (options.includePatientInfo) {
            ds.set_string(tags::patient_name, vr_type::PN, content.patient.patientName);
            ds.set_string(tags::patient_id, vr_type::LO, content.patient.patientId);
            ds.set_string(tags::patient_birth_date, vr_type::DA, content.patient.patientBirthDate);
            ds.set_string(tags::patient_sex, vr_type::CS, content.patient.patientSex);
        }

        // Study Module
        if (options.includeStudyInfo) {
            ds.set_string(tags::study_instance_uid, vr_type::UI, content.study.studyInstanceUid);
            ds.set_string(tags::study_date, vr_type::DA, content.study.studyDate);
            ds.set_string(tags::study_time, vr_type::TM, content.study.studyTime);
            ds.set_string(tags::study_description, vr_type::LO, content.study.studyDescription);
            ds.set_string(tags::accession_number, vr_type::SH, content.study.accessionNumber);
            ds.set_string(tags::referring_physician_name, vr_type::PN, content.study.referringPhysicianName);
        }

        // Series Module
        ds.set_string(tags::modality, vr_type::CS, "SR");
        ds.set_string(tags::series_instance_uid, vr_type::UI, seriesUid);
        ds.set_string(tags::series_description, vr_type::LO, options.seriesDescription.toStdString());
        ds.set_string(tags::series_number, vr_type::IS, std::to_string(options.seriesNumber));

        // Instance Number
        ds.set_string(tags::instance_number, vr_type::IS, std::to_string(options.instanceNumber));

        // Equipment Module
        ds.set_string(tags::manufacturer, vr_type::LO, options.manufacturer.toStdString());

        // Additional metadata
        ds.set_string(tags::institution_name, vr_type::LO, content.institutionName);
        ds.set_string(tags::operators_name, vr_type::PN, content.operatorName);

        // Content Date/Time
        std::string contentDate = formatDateTime(content.performedDateTime, "%Y%m%d");
        std::string contentTime = formatDateTime(content.performedDateTime, "%H%M%S");
        ds.set_string(tags::content_date, vr_type::DA, contentDate);
        ds.set_string(tags::content_time, vr_type::TM, contentTime);

        // SR Document General Module
        ds.set_string(sr_tags::completion_flag, vr_type::CS, "COMPLETE");
        ds.set_string(sr_tags::verification_flag, vr_type::CS, "UNVERIFIED");

        // Build Content Sequence (SR document tree)
        auto& contentSeq = ds.get_or_create_sequence(sr_tags::content_sequence);

        // Root container - Imaging Measurement Report
        auto rootContainer = createContentItem("CONTAINER", "CONTAINS", SRCodes::ImagingMeasurementReport);
        auto& rootContent = rootContainer.get_or_create_sequence(sr_tags::content_sequence);

        // Add distance measurements
        if (!content.distances.empty()) {
            auto distContainer = createContentItem("CONTAINER", "CONTAINS",
                DicomCode{"125007", "DCM", "Distance Measurements"});
            auto& distContent = distContainer.get_or_create_sequence(sr_tags::content_sequence);

            for (const auto& dist : content.distances) {
                addDistanceMeasurement(distContent, dist, options.includeSpatialCoordinates);
            }

            rootContent.push_back(std::move(distContainer));
        }

        // Add angle measurements
        if (!content.angles.empty()) {
            auto angleContainer = createContentItem("CONTAINER", "CONTAINS",
                DicomCode{"125007", "DCM", "Angle Measurements"});
            auto& angleContent = angleContainer.get_or_create_sequence(sr_tags::content_sequence);

            for (const auto& angle : content.angles) {
                addAngleMeasurement(angleContent, angle, options.includeSpatialCoordinates);
            }

            rootContent.push_back(std::move(angleContainer));
        }

        // Add area measurements
        if (!content.areas.empty()) {
            auto areaContainer = createContentItem("CONTAINER", "CONTAINS",
                DicomCode{"125007", "DCM", "Area Measurements"});
            auto& areaContent = areaContainer.get_or_create_sequence(sr_tags::content_sequence);

            for (const auto& area : content.areas) {
                addAreaMeasurement(areaContent, area, options.includeSpatialCoordinates);
            }

            rootContent.push_back(std::move(areaContainer));
        }

        // Add volume measurements
        if (!content.volumes.empty()) {
            auto volContainer = createContentItem("CONTAINER", "CONTAINS",
                DicomCode{"125007", "DCM", "Volume Measurements"});
            auto& volContent = volContainer.get_or_create_sequence(sr_tags::content_sequence);

            for (const auto& vol : content.volumes) {
                addVolumeMeasurement(volContent, vol);
            }

            rootContent.push_back(std::move(volContainer));
        }

        // Add ROI statistics
        if (options.includeROIStatistics && !content.roiStatistics.empty()) {
            auto statsContainer = createContentItem("CONTAINER", "CONTAINS",
                DicomCode{"125007", "DCM", "ROI Statistics"});
            auto& statsContent = statsContainer.get_or_create_sequence(sr_tags::content_sequence);

            for (const auto& stats : content.roiStatistics) {
                addROIStatistics(statsContent, stats);
            }

            rootContent.push_back(std::move(statsContainer));
        }

        // Add referenced SOP Instance UIDs
        for (const auto& refSopUid : content.referencedSopInstanceUids) {
            auto uidRef = createContentItem("UIDREF", "CONTAINS",
                DicomCode{"121232", "DCM", "Referenced SOP Instance UID"});
            uidRef.set_string(sr_tags::uid_value, vr_type::UI, refSopUid);
            rootContent.push_back(std::move(uidRef));
        }

        contentSeq.push_back(std::move(rootContainer));

        return ds;
    }

    void addDistanceMeasurement(std::vector<pacs::core::dicom_dataset>& content,
                                const DistanceMeasurement& dist,
                                bool includeSpatialCoords) const {
        using namespace pacs::encoding;

        // Measurement Group container
        auto group = createContentItem("CONTAINER", "CONTAINS",
            DicomCode{"125309", "DCM", "Measurement Group"});
        auto& groupContent = group.get_or_create_sequence(sr_tags::content_sequence);

        // Tracking Identifier
        if (!dist.label.empty()) {
            auto trackingId = createContentItem("TEXT", "HAS OBS CONTEXT",
                DicomCode{"112039", "DCM", "Tracking Identifier"});
            trackingId.set_string(sr_tags::text_value, vr_type::UT, dist.label);
            groupContent.push_back(std::move(trackingId));
        }

        // Numeric measurement
        auto numItem = createContentItem("NUM", "CONTAINS", SRCodes::Length);
        auto& measuredValueSeq = numItem.get_or_create_sequence(sr_tags::measured_value_sequence);

        pacs::core::dicom_dataset measuredValue;
        auto& unitSeq = measuredValue.get_or_create_sequence(sr_tags::measurement_units_code_sequence);
        unitSeq.push_back(createCodedEntry(SRCodes::Millimeter));
        measuredValue.set_string(sr_tags::numeric_value, vr_type::DS, std::to_string(dist.distanceMm));
        measuredValueSeq.push_back(std::move(measuredValue));

        groupContent.push_back(std::move(numItem));

        // Spatial coordinates
        if (includeSpatialCoords) {
            addSpatialCoord3D(groupContent, "POINT", {dist.point1});
            addSpatialCoord3D(groupContent, "POINT", {dist.point2});
        }

        content.push_back(std::move(group));
    }

    void addAngleMeasurement(std::vector<pacs::core::dicom_dataset>& content,
                             const AngleMeasurement& angle,
                             bool includeSpatialCoords) const {
        using namespace pacs::encoding;

        auto group = createContentItem("CONTAINER", "CONTAINS",
            DicomCode{"125309", "DCM", "Measurement Group"});
        auto& groupContent = group.get_or_create_sequence(sr_tags::content_sequence);

        if (!angle.label.empty()) {
            auto trackingId = createContentItem("TEXT", "HAS OBS CONTEXT",
                DicomCode{"112039", "DCM", "Tracking Identifier"});
            trackingId.set_string(sr_tags::text_value, vr_type::UT, angle.label);
            groupContent.push_back(std::move(trackingId));
        }

        auto numItem = createContentItem("NUM", "CONTAINS", SRCodes::Angle);
        auto& measuredValueSeq = numItem.get_or_create_sequence(sr_tags::measured_value_sequence);

        pacs::core::dicom_dataset measuredValue;
        auto& unitSeq = measuredValue.get_or_create_sequence(sr_tags::measurement_units_code_sequence);
        unitSeq.push_back(createCodedEntry(SRCodes::Degree));
        measuredValue.set_string(sr_tags::numeric_value, vr_type::DS, std::to_string(angle.angleDegrees));
        measuredValueSeq.push_back(std::move(measuredValue));

        groupContent.push_back(std::move(numItem));

        if (includeSpatialCoords) {
            addSpatialCoord3D(groupContent, "POINT", {angle.vertex});
        }

        content.push_back(std::move(group));
    }

    void addAreaMeasurement(std::vector<pacs::core::dicom_dataset>& content,
                            const AreaMeasurement& area,
                            bool includeSpatialCoords) const {
        using namespace pacs::encoding;

        auto group = createContentItem("CONTAINER", "CONTAINS",
            DicomCode{"125309", "DCM", "Measurement Group"});
        auto& groupContent = group.get_or_create_sequence(sr_tags::content_sequence);

        if (!area.label.empty()) {
            auto trackingId = createContentItem("TEXT", "HAS OBS CONTEXT",
                DicomCode{"112039", "DCM", "Tracking Identifier"});
            trackingId.set_string(sr_tags::text_value, vr_type::UT, area.label);
            groupContent.push_back(std::move(trackingId));
        }

        auto numItem = createContentItem("NUM", "CONTAINS", SRCodes::Area);
        auto& measuredValueSeq = numItem.get_or_create_sequence(sr_tags::measured_value_sequence);

        pacs::core::dicom_dataset measuredValue;
        auto& unitSeq = measuredValue.get_or_create_sequence(sr_tags::measurement_units_code_sequence);
        unitSeq.push_back(createCodedEntry(SRCodes::SquareMillimeter));
        measuredValue.set_string(sr_tags::numeric_value, vr_type::DS, std::to_string(area.areaMm2));
        measuredValueSeq.push_back(std::move(measuredValue));

        groupContent.push_back(std::move(numItem));

        if (includeSpatialCoords && !area.points.empty()) {
            addSpatialCoord3D(groupContent, "POLYGON", area.points);
        }

        content.push_back(std::move(group));
    }

    void addVolumeMeasurement(std::vector<pacs::core::dicom_dataset>& content,
                              const VolumeResult& vol) const {
        using namespace pacs::encoding;

        auto group = createContentItem("CONTAINER", "CONTAINS",
            DicomCode{"125309", "DCM", "Measurement Group"});
        auto& groupContent = group.get_or_create_sequence(sr_tags::content_sequence);

        if (!vol.labelName.empty()) {
            auto trackingId = createContentItem("TEXT", "HAS OBS CONTEXT",
                DicomCode{"112039", "DCM", "Tracking Identifier"});
            trackingId.set_string(sr_tags::text_value, vr_type::UT, vol.labelName);
            groupContent.push_back(std::move(trackingId));
        }

        // Volume in cubic centimeters
        auto numItem = createContentItem("NUM", "CONTAINS", SRCodes::Volume);
        auto& measuredValueSeq = numItem.get_or_create_sequence(sr_tags::measured_value_sequence);

        double volumeCm3 = vol.volumeMm3 / 1000.0;
        pacs::core::dicom_dataset measuredValue;
        auto& unitSeq = measuredValue.get_or_create_sequence(sr_tags::measurement_units_code_sequence);
        unitSeq.push_back(createCodedEntry(SRCodes::CubicCentimeter));
        measuredValue.set_string(sr_tags::numeric_value, vr_type::DS, std::to_string(volumeCm3));
        measuredValueSeq.push_back(std::move(measuredValue));

        groupContent.push_back(std::move(numItem));

        // Surface area if available
        if (vol.surfaceAreaMm2.has_value() && vol.surfaceAreaMm2.value() > 0) {
            auto surfaceItem = createContentItem("NUM", "CONTAINS",
                DicomCode{"118565009", "SCT", "Surface Area"});
            auto& surfaceValueSeq = surfaceItem.get_or_create_sequence(sr_tags::measured_value_sequence);

            pacs::core::dicom_dataset surfaceValue;
            auto& surfaceUnitSeq = surfaceValue.get_or_create_sequence(sr_tags::measurement_units_code_sequence);
            surfaceUnitSeq.push_back(createCodedEntry(SRCodes::SquareMillimeter));
            surfaceValue.set_string(sr_tags::numeric_value, vr_type::DS,
                std::to_string(vol.surfaceAreaMm2.value()));
            surfaceValueSeq.push_back(std::move(surfaceValue));

            groupContent.push_back(std::move(surfaceItem));
        }

        content.push_back(std::move(group));
    }

    void addROIStatistics(std::vector<pacs::core::dicom_dataset>& content,
                          const SRROIStatistics& stats) const {
        using namespace pacs::encoding;

        auto group = createContentItem("CONTAINER", "CONTAINS",
            DicomCode{"125309", "DCM", "Measurement Group"});
        auto& groupContent = group.get_or_create_sequence(sr_tags::content_sequence);

        if (!stats.label.empty()) {
            auto trackingId = createContentItem("TEXT", "HAS OBS CONTEXT",
                DicomCode{"112039", "DCM", "Tracking Identifier"});
            trackingId.set_string(sr_tags::text_value, vr_type::UT, stats.label);
            groupContent.push_back(std::move(trackingId));
        }

        // Mean
        addNumericMeasurement(groupContent, SRCodes::Mean, stats.mean, SRCodes::HounsfieldUnit);

        // Standard Deviation
        addNumericMeasurement(groupContent, SRCodes::StandardDeviation, stats.stdDev, SRCodes::HounsfieldUnit);

        // Minimum
        addNumericMeasurement(groupContent, SRCodes::Minimum, stats.min, SRCodes::HounsfieldUnit);

        // Maximum
        addNumericMeasurement(groupContent, SRCodes::Maximum, stats.max, SRCodes::HounsfieldUnit);

        // Area
        if (stats.areaMm2 > 0) {
            addNumericMeasurement(groupContent, SRCodes::Area, stats.areaMm2, SRCodes::SquareMillimeter);
        }

        content.push_back(std::move(group));
    }

    void addNumericMeasurement(std::vector<pacs::core::dicom_dataset>& content,
                               const DicomCode& conceptCode,
                               double value,
                               const DicomCode& unitCode) const {
        using namespace pacs::encoding;

        auto numItem = createContentItem("NUM", "CONTAINS", conceptCode);
        auto& measuredValueSeq = numItem.get_or_create_sequence(sr_tags::measured_value_sequence);

        pacs::core::dicom_dataset measuredValue;
        auto& unitSeq = measuredValue.get_or_create_sequence(sr_tags::measurement_units_code_sequence);
        unitSeq.push_back(createCodedEntry(unitCode));
        measuredValue.set_string(sr_tags::numeric_value, vr_type::DS, std::to_string(value));
        measuredValueSeq.push_back(std::move(measuredValue));

        content.push_back(std::move(numItem));
    }

    void addSpatialCoord3D(std::vector<pacs::core::dicom_dataset>& content,
                           const std::string& graphicType,
                           const std::vector<Point3D>& points) const {
        using namespace pacs::encoding;

        auto scoord = createContentItem("SCOORD3D", "CONTAINS",
            DicomCode{"111030", "DCM", "Image Region"});

        scoord.set_string(sr_tags::graphic_type, vr_type::CS, graphicType);

        // Build graphic data string (x1\y1\z1\x2\y2\z2\...)
        std::ostringstream oss;
        bool first = true;
        for (const auto& pt : points) {
            if (!first) oss << "\\";
            oss << pt[0] << "\\" << pt[1] << "\\" << pt[2];
            first = false;
        }
        scoord.set_string(sr_tags::graphic_data, vr_type::FL, oss.str());

        content.push_back(std::move(scoord));
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
    return generatePacsUid();
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

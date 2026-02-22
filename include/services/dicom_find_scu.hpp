// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "pacs_config.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declare PacsErrorInfo from dicom_echo_scu.hpp
namespace dicom_viewer::services {
struct PacsErrorInfo;
}

namespace dicom_viewer::services {

/**
 * @brief Query/Retrieve Information Model root
 */
enum class QueryRoot {
    PatientRoot,  ///< Patient Root Q/R Information Model
    StudyRoot     ///< Study Root Q/R Information Model
};

/**
 * @brief Query level within the hierarchy
 */
enum class QueryLevel {
    Patient,  ///< Patient level query
    Study,    ///< Study level query
    Series,   ///< Series level query
    Image     ///< Image (Instance) level query
};

/**
 * @brief Date range for query filtering
 */
struct DateRange {
    std::optional<std::string> from;  ///< Start date (YYYYMMDD format)
    std::optional<std::string> to;    ///< End date (YYYYMMDD format)

    /**
     * @brief Create a single date query
     */
    static DateRange singleDate(const std::string& date) {
        return DateRange{date, date};
    }

    /**
     * @brief Create an open-ended date range (from date onwards)
     */
    static DateRange fromDate(const std::string& date) {
        return DateRange{date, std::nullopt};
    }

    /**
     * @brief Create a date range up to a specific date
     */
    static DateRange toDate(const std::string& date) {
        return DateRange{std::nullopt, date};
    }

    /**
     * @brief Convert to DICOM date range format
     */
    [[nodiscard]] std::string toDicomFormat() const;
};

/**
 * @brief Query parameters for C-FIND operations
 *
 * Supports wildcards (*) for pattern matching in string fields.
 * Date fields support range queries using DateRange.
 */
struct FindQuery {
    /// Query root (Patient or Study)
    QueryRoot root = QueryRoot::PatientRoot;

    /// Query level
    QueryLevel level = QueryLevel::Study;

    /// Patient Name (0010,0010) - supports wildcards
    std::optional<std::string> patientName;

    /// Patient ID (0010,0020)
    std::optional<std::string> patientId;

    /// Patient Birth Date (0010,0030)
    std::optional<DateRange> patientBirthDate;

    /// Study Date (0008,0020)
    std::optional<DateRange> studyDate;

    /// Study Description (0008,1030)
    std::optional<std::string> studyDescription;

    /// Study Instance UID (0020,000D)
    std::optional<std::string> studyInstanceUid;

    /// Accession Number (0008,0050)
    std::optional<std::string> accessionNumber;

    /// Modality (0008,0060)
    std::optional<std::string> modality;

    /// Modalities in Study (0008,0061)
    std::optional<std::string> modalitiesInStudy;

    /// Series Instance UID (0020,000E)
    std::optional<std::string> seriesInstanceUid;

    /// Series Number (0020,0011)
    std::optional<int32_t> seriesNumber;

    /// Series Description (0008,103E)
    std::optional<std::string> seriesDescription;

    /// SOP Instance UID (0008,0018)
    std::optional<std::string> sopInstanceUid;

    /// Instance Number (0020,0013)
    std::optional<int32_t> instanceNumber;
};

/**
 * @brief Patient-level query result
 */
struct PatientResult {
    std::string patientId;        ///< Patient ID (0010,0020)
    std::string patientName;      ///< Patient Name (0010,0010)
    std::string patientBirthDate; ///< Birth Date (0010,0030)
    std::string patientSex;       ///< Patient Sex (0010,0040)
    int32_t numberOfStudies = 0;  ///< Number of Patient Related Studies (0020,1200)
};

/**
 * @brief Study-level query result
 */
struct StudyResult {
    std::string studyInstanceUid;  ///< Study Instance UID (0020,000D)
    std::string studyDate;         ///< Study Date (0008,0020)
    std::string studyTime;         ///< Study Time (0008,0030)
    std::string studyDescription;  ///< Study Description (0008,1030)
    std::string accessionNumber;   ///< Accession Number (0008,0050)
    std::string referringPhysician;///< Referring Physician's Name (0008,0090)
    std::string patientId;         ///< Patient ID (0010,0020)
    std::string patientName;       ///< Patient Name (0010,0010)
    std::string modalitiesInStudy; ///< Modalities in Study (0008,0061)
    int32_t numberOfSeries = 0;    ///< Number of Study Related Series (0020,1206)
    int32_t numberOfInstances = 0; ///< Number of Study Related Instances (0020,1208)
};

/**
 * @brief Series-level query result
 */
struct SeriesResult {
    std::string seriesInstanceUid; ///< Series Instance UID (0020,000E)
    std::string studyInstanceUid;  ///< Study Instance UID (0020,000D)
    std::string modality;          ///< Modality (0008,0060)
    int32_t seriesNumber = 0;      ///< Series Number (0020,0011)
    std::string seriesDescription; ///< Series Description (0008,103E)
    std::string seriesDate;        ///< Series Date (0008,0021)
    std::string seriesTime;        ///< Series Time (0008,0031)
    std::string bodyPartExamined;  ///< Body Part Examined (0018,0015)
    int32_t numberOfInstances = 0; ///< Number of Series Related Instances (0020,1209)
};

/**
 * @brief Image (Instance) level query result
 */
struct ImageResult {
    std::string sopInstanceUid;    ///< SOP Instance UID (0008,0018)
    std::string sopClassUid;       ///< SOP Class UID (0008,0016)
    std::string seriesInstanceUid; ///< Series Instance UID (0020,000E)
    int32_t instanceNumber = 0;    ///< Instance Number (0020,0013)
    std::string contentDate;       ///< Content Date (0008,0023)
    std::string contentTime;       ///< Content Time (0008,0033)
};

/**
 * @brief Result of a C-FIND query operation
 */
struct FindResult {
    /// Query latency
    std::chrono::milliseconds latency{0};

    /// Patient-level results (when query level is Patient)
    std::vector<PatientResult> patients;

    /// Study-level results (when query level is Study)
    std::vector<StudyResult> studies;

    /// Series-level results (when query level is Series)
    std::vector<SeriesResult> series;

    /// Image-level results (when query level is Image)
    std::vector<ImageResult> images;

    /**
     * @brief Get total number of results across all levels
     */
    [[nodiscard]] size_t totalCount() const {
        return patients.size() + studies.size() + series.size() + images.size();
    }
};

/**
 * @brief DICOM C-FIND Service Class User (SCU)
 *
 * Implements the DICOM Query/Retrieve Service Classes for searching
 * patient/study/series/image data on PACS servers.
 *
 * Supports:
 * - Patient Root Query/Retrieve Information Model - FIND (1.2.840.10008.5.1.4.1.2.1.1)
 * - Study Root Query/Retrieve Information Model - FIND (1.2.840.10008.5.1.4.1.2.2.1)
 *
 * @example
 * @code
 * DicomFindSCU finder;
 * PacsServerConfig config;
 * config.hostname = "pacs.hospital.com";
 * config.port = 104;
 * config.calledAeTitle = "PACS_SERVER";
 *
 * FindQuery query;
 * query.root = QueryRoot::StudyRoot;
 * query.level = QueryLevel::Study;
 * query.patientName = "SMITH*";
 * query.studyDate = DateRange::fromDate("20240101");
 *
 * auto result = finder.find(config, query);
 * if (result) {
 *     for (const auto& study : result->studies) {
 *         std::cout << "Study: " << study.studyDescription << "\n";
 *     }
 * } else {
 *     std::cerr << "Query failed: " << result.error().toString() << "\n";
 * }
 * @endcode
 *
 * @trace SRS-FR-035
 */
class DicomFindSCU {
public:
    /// Patient Root Query/Retrieve Information Model - FIND SOP Class UID
    static constexpr const char* PATIENT_ROOT_FIND_SOP_CLASS_UID =
        "1.2.840.10008.5.1.4.1.2.1.1";

    /// Study Root Query/Retrieve Information Model - FIND SOP Class UID
    static constexpr const char* STUDY_ROOT_FIND_SOP_CLASS_UID =
        "1.2.840.10008.5.1.4.1.2.2.1";

    DicomFindSCU();
    ~DicomFindSCU();

    // Non-copyable, movable
    DicomFindSCU(const DicomFindSCU&) = delete;
    DicomFindSCU& operator=(const DicomFindSCU&) = delete;
    DicomFindSCU(DicomFindSCU&&) noexcept;
    DicomFindSCU& operator=(DicomFindSCU&&) noexcept;

    /**
     * @brief Execute a C-FIND query against a PACS server
     *
     * Establishes a DICOM association with the server and sends
     * a C-FIND request with the specified query parameters.
     *
     * @param config Server configuration
     * @param query Query parameters
     * @return FindResult on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<FindResult, PacsErrorInfo>
    find(const PacsServerConfig& config, const FindQuery& query);

    /**
     * @brief Cancel any ongoing query operation
     *
     * Thread-safe method to abort current operation.
     */
    void cancel();

    /**
     * @brief Check if a query is currently in progress
     */
    [[nodiscard]] bool isQuerying() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

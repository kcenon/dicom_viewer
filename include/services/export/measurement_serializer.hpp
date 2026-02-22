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

#include "services/export/report_generator.hpp"
#include "services/measurement/measurement_types.hpp"
#include "services/segmentation/segmentation_label.hpp"

#include <QDateTime>
#include <QString>

#include <array>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Error information for serialization operations
 *
 * @trace SRS-FR-049
 */
struct SerializationError {
    enum class Code {
        Success,
        FileAccessDenied,
        FileNotFound,
        InvalidJson,
        InvalidSchema,
        VersionMismatch,
        StudyMismatch,
        InternalError
    };

    Code code = Code::Success;
    std::string message;

    [[nodiscard]] bool isSuccess() const noexcept {
        return code == Code::Success;
    }

    [[nodiscard]] std::string toString() const {
        switch (code) {
            case Code::Success: return "Success";
            case Code::FileAccessDenied: return "File access denied: " + message;
            case Code::FileNotFound: return "File not found: " + message;
            case Code::InvalidJson: return "Invalid JSON: " + message;
            case Code::InvalidSchema: return "Invalid schema: " + message;
            case Code::VersionMismatch: return "Version mismatch: " + message;
            case Code::StudyMismatch: return "Study mismatch: " + message;
            case Code::InternalError: return "Internal error: " + message;
        }
        return "Unknown error";
    }
};

/**
 * @brief Session data container for measurement serialization
 *
 * Contains all data needed to save/restore a measurement session.
 *
 * @trace SRS-FR-049
 */
struct SessionData {
    // Study reference
    QString studyInstanceUID;
    QString seriesInstanceUID;
    PatientInfo patient;

    // Measurements
    std::vector<DistanceMeasurement> distances;
    std::vector<AngleMeasurement> angles;
    std::vector<AreaMeasurement> areas;

    // Segmentation
    std::optional<std::filesystem::path> labelMapPath;
    std::vector<SegmentationLabel> labels;

    // View state
    double windowWidth = 400.0;
    double windowCenter = 40.0;
    std::array<int, 3> slicePositions = {0, 0, 0};

    // Metadata
    QString version;
    QDateTime created;
    QDateTime modified;
};

/**
 * @brief Serializer for measurement sessions
 *
 * Implements save/load functionality for measurement sessions using JSON format.
 * Supports versioned schema for forward compatibility and validation.
 *
 * @example
 * @code
 * MeasurementSerializer serializer;
 *
 * SessionData session;
 * session.studyInstanceUID = "1.2.840.113619...";
 * session.distances = myDistanceMeasurements;
 * session.angles = myAngleMeasurements;
 *
 * // Save session
 * auto saveResult = serializer.save(session, "/path/to/measurements.dvmeas");
 * if (!saveResult) {
 *     std::cerr << "Save failed: " << saveResult.error().toString() << std::endl;
 * }
 *
 * // Load session
 * auto loadResult = serializer.load("/path/to/measurements.dvmeas");
 * if (loadResult) {
 *     const auto& loaded = *loadResult;
 *     // Use loaded data
 * }
 * @endcode
 *
 * @trace SRS-FR-049
 */
class MeasurementSerializer {
public:
    /// File extension for measurement session files
    static constexpr const char* FILE_EXTENSION = ".dvmeas";

    /// Current schema version
    static constexpr const char* CURRENT_VERSION = "1.0.0";

    /// Application identifier
    static constexpr const char* APPLICATION_ID = "DICOM Viewer";

    MeasurementSerializer();
    ~MeasurementSerializer();

    // Non-copyable, movable
    MeasurementSerializer(const MeasurementSerializer&) = delete;
    MeasurementSerializer& operator=(const MeasurementSerializer&) = delete;
    MeasurementSerializer(MeasurementSerializer&&) noexcept;
    MeasurementSerializer& operator=(MeasurementSerializer&&) noexcept;

    /**
     * @brief Save session to file
     *
     * Serializes the session data to JSON format and writes to the specified file.
     * Automatically sets the version and modified timestamp.
     *
     * @param session Session data to save
     * @param filePath Output file path (should end with .dvmeas)
     * @return Success or error information
     */
    [[nodiscard]] std::expected<void, SerializationError> save(
        const SessionData& session,
        const std::filesystem::path& filePath) const;

    /**
     * @brief Load session from file
     *
     * Reads and parses JSON from the specified file, validates the schema,
     * and returns the deserialized session data.
     *
     * @param filePath Input file path
     * @return Session data or error information
     */
    [[nodiscard]] std::expected<SessionData, SerializationError> load(
        const std::filesystem::path& filePath) const;

    /**
     * @brief Validate file without full load
     *
     * Performs quick validation of file structure and schema version
     * without loading all data.
     *
     * @param filePath File path to validate
     * @return true if valid, false with error information if invalid
     */
    [[nodiscard]] std::expected<bool, SerializationError> validate(
        const std::filesystem::path& filePath) const;

    /**
     * @brief Check if session is compatible with current study
     *
     * Compares the study UID in the session with the provided current study UID.
     *
     * @param session Session data to check
     * @param currentStudyUID Current study instance UID
     * @return true if compatible (same study), false otherwise
     */
    [[nodiscard]] static bool isCompatible(
        const SessionData& session,
        const QString& currentStudyUID);

    /**
     * @brief Get file filter string for file dialogs
     * @return Filter string like "DICOM Viewer Measurements (*.dvmeas)"
     */
    [[nodiscard]] static QString getFileFilter();

    /**
     * @brief Get supported versions for migration
     * @return Vector of version strings that can be migrated
     */
    [[nodiscard]] static std::vector<QString> getSupportedVersions();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dicom_viewer::services

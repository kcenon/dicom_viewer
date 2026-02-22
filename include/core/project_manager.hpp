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

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace dicom_viewer::core {

/**
 * @brief Error codes for project operations
 */
enum class ProjectError {
    FileOpenFailed,
    FileWriteFailed,
    InvalidFormat,
    ManifestMissing,
    VersionIncompatible,
    SerializationError,
    InternalError
};

/**
 * @brief Metadata about a recent project
 */
struct RecentProject {
    std::filesystem::path path;
    std::string name;
    std::string timestamp;  ///< ISO 8601 format
};

/**
 * @brief Patient information stored in the project
 */
struct PatientInfo {
    std::string patientId;
    std::string patientName;
    std::string studyDate;
    std::string studyDescription;
    std::string seriesDescription;
    std::string modality;
};

/**
 * @brief Display settings persisted in the project
 */
struct DisplaySettings {
    double windowCenter = 0.0;
    double windowWidth = 0.0;
    bool overlayVisible = true;
    double overlayOpacity = 0.5;
};

/**
 * @brief View state persisted in the project
 */
struct ViewState {
    int sliceIndex = 0;
    int phaseIndex = 0;
    std::string activeView = "axial";  ///< axial, sagittal, coronal
    std::string layoutMode = "single"; ///< single, dual, quad
};

/**
 * @brief DICOM source references
 */
struct DicomReferences {
    std::vector<std::filesystem::path> filePaths;
    std::string seriesInstanceUid;
    std::string studyInstanceUid;
};

/**
 * @brief Manages .flo project file operations
 *
 * Provides save, load, and new project functionality using a ZIP-based
 * container format. The .flo file contains JSON metadata, settings,
 * and (in future) compressed data fields.
 *
 * @trace SRS-FR-050
 */
class ProjectManager {
public:
    /// Callback when project state changes
    using StateChangeCallback = std::function<void()>;

    static constexpr const char* kFileExtension = ".flo";
    static constexpr int kFormatVersion = 1;
    static constexpr int kMaxRecentProjects = 10;

    ProjectManager();
    ~ProjectManager();

    // Non-copyable but movable
    ProjectManager(const ProjectManager&) = delete;
    ProjectManager& operator=(const ProjectManager&) = delete;
    ProjectManager(ProjectManager&&) noexcept;
    ProjectManager& operator=(ProjectManager&&) noexcept;

    /**
     * @brief Create a new empty project
     *
     * Clears all current project state and resets modified flag.
     */
    void newProject();

    /**
     * @brief Save the project to a file
     *
     * @param path Output file path (should end with .flo)
     * @return Success or ProjectError
     */
    [[nodiscard]] std::expected<void, ProjectError>
    saveProject(const std::filesystem::path& path);

    /**
     * @brief Load a project from a file
     *
     * @param path Input file path
     * @return Success or ProjectError
     */
    [[nodiscard]] std::expected<void, ProjectError>
    loadProject(const std::filesystem::path& path);

    /**
     * @brief Check if the project has been modified since last save
     */
    [[nodiscard]] bool isModified() const noexcept;

    /**
     * @brief Mark the project as modified
     */
    void markModified();

    /**
     * @brief Get the current project file path
     * @return Path, or empty if unsaved
     */
    [[nodiscard]] std::filesystem::path currentPath() const;

    /**
     * @brief Get the project name (derived from filename)
     */
    [[nodiscard]] std::string projectName() const;

    // -- Data accessors --

    void setPatientInfo(const PatientInfo& info);
    [[nodiscard]] const PatientInfo& patientInfo() const noexcept;

    void setDicomReferences(const DicomReferences& refs);
    [[nodiscard]] const DicomReferences& dicomReferences() const noexcept;

    void setDisplaySettings(const DisplaySettings& settings);
    [[nodiscard]] const DisplaySettings& displaySettings() const noexcept;

    void setViewState(const ViewState& state);
    [[nodiscard]] const ViewState& viewState() const noexcept;

    // -- Recent projects --

    /**
     * @brief Add a project path to the recent projects list
     *
     * If the path already exists, it is moved to the front.
     * The list is capped at kMaxRecentProjects entries.
     * Persists to disk if a recent projects path is configured.
     *
     * @param path Project file path
     * @param name Display name (defaults to filename stem)
     */
    void addToRecent(const std::filesystem::path& path,
                     const std::string& name = "");

    /**
     * @brief Get the list of recent projects (newest first)
     */
    [[nodiscard]] std::vector<RecentProject> recentProjects() const;

    /**
     * @brief Clear the recent projects list and persist
     */
    void clearRecentProjects();

    /**
     * @brief Set the file path used to persist recent projects
     *
     * If not set, recent projects are stored in memory only.
     * The file uses JSON format.
     *
     * @param path Path to the recent projects JSON file
     */
    void setRecentProjectsPath(const std::filesystem::path& path);

    // -- State change notification --

    void setStateChangeCallback(StateChangeCallback callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::core

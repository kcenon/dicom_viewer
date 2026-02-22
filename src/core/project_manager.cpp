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

#include "core/project_manager.hpp"
#include "core/zip_archive.hpp"

#include <chrono>
#include <fstream>

#include <nlohmann/json.hpp>

namespace dicom_viewer::core {

// =============================================================================
// JSON serialization helpers
// =============================================================================

namespace {

nlohmann::json patientInfoToJson(const PatientInfo& info) {
    return {
        {"patient_id", info.patientId},
        {"patient_name", info.patientName},
        {"study_date", info.studyDate},
        {"study_description", info.studyDescription},
        {"series_description", info.seriesDescription},
        {"modality", info.modality}
    };
}

PatientInfo patientInfoFromJson(const nlohmann::json& j) {
    PatientInfo info;
    info.patientId = j.value("patient_id", "");
    info.patientName = j.value("patient_name", "");
    info.studyDate = j.value("study_date", "");
    info.studyDescription = j.value("study_description", "");
    info.seriesDescription = j.value("series_description", "");
    info.modality = j.value("modality", "");
    return info;
}

nlohmann::json dicomRefsToJson(const DicomReferences& refs) {
    nlohmann::json paths = nlohmann::json::array();
    for (const auto& p : refs.filePaths) {
        paths.push_back(p.string());
    }
    return {
        {"file_paths", paths},
        {"series_instance_uid", refs.seriesInstanceUid},
        {"study_instance_uid", refs.studyInstanceUid}
    };
}

DicomReferences dicomRefsFromJson(const nlohmann::json& j) {
    DicomReferences refs;
    if (j.contains("file_paths")) {
        for (const auto& p : j["file_paths"]) {
            refs.filePaths.emplace_back(p.get<std::string>());
        }
    }
    refs.seriesInstanceUid = j.value("series_instance_uid", "");
    refs.studyInstanceUid = j.value("study_instance_uid", "");
    return refs;
}

nlohmann::json displaySettingsToJson(const DisplaySettings& s) {
    return {
        {"window_center", s.windowCenter},
        {"window_width", s.windowWidth},
        {"overlay_visible", s.overlayVisible},
        {"overlay_opacity", s.overlayOpacity}
    };
}

DisplaySettings displaySettingsFromJson(const nlohmann::json& j) {
    DisplaySettings s;
    s.windowCenter = j.value("window_center", 0.0);
    s.windowWidth = j.value("window_width", 0.0);
    s.overlayVisible = j.value("overlay_visible", true);
    s.overlayOpacity = j.value("overlay_opacity", 0.5);
    return s;
}

nlohmann::json viewStateToJson(const ViewState& v) {
    return {
        {"slice_index", v.sliceIndex},
        {"phase_index", v.phaseIndex},
        {"active_view", v.activeView},
        {"layout_mode", v.layoutMode}
    };
}

ViewState viewStateFromJson(const nlohmann::json& j) {
    ViewState v;
    v.sliceIndex = j.value("slice_index", 0);
    v.phaseIndex = j.value("phase_index", 0);
    v.activeView = j.value("active_view", "axial");
    v.layoutMode = j.value("layout_mode", "single");
    return v;
}

std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&time, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

} // anonymous namespace

// =============================================================================
// Implementation
// =============================================================================

class ProjectManager::Impl {
public:
    std::filesystem::path currentPath_;
    bool modified_ = false;

    PatientInfo patientInfo_;
    DicomReferences dicomRefs_;
    DisplaySettings displaySettings_;
    ViewState viewState_;

    std::vector<RecentProject> recentProjects_;
    std::filesystem::path recentProjectsPath_;

    StateChangeCallback stateChangeCallback_;

    void notifyStateChange() {
        if (stateChangeCallback_) {
            stateChangeCallback_();
        }
    }

    void saveRecentProjects() {
        if (recentProjectsPath_.empty()) {
            return;
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& rp : recentProjects_) {
            arr.push_back({
                {"path", rp.path.string()},
                {"name", rp.name},
                {"timestamp", rp.timestamp}
            });
        }
        std::ofstream out(recentProjectsPath_);
        if (out.is_open()) {
            out << arr.dump(2);
        }
    }

    void loadRecentProjects() {
        if (recentProjectsPath_.empty() ||
            !std::filesystem::exists(recentProjectsPath_)) {
            return;
        }
        std::ifstream in(recentProjectsPath_);
        if (!in.is_open()) {
            return;
        }
        try {
            auto arr = nlohmann::json::parse(in);
            recentProjects_.clear();
            for (const auto& item : arr) {
                RecentProject rp;
                rp.path = item.value("path", "");
                rp.name = item.value("name", "");
                rp.timestamp = item.value("timestamp", "");
                if (!rp.path.empty()) {
                    recentProjects_.push_back(std::move(rp));
                }
            }
        } catch (...) {
            // Ignore corrupt recent projects file
        }
    }
};

ProjectManager::ProjectManager()
    : impl_(std::make_unique<Impl>()) {}

ProjectManager::~ProjectManager() = default;

ProjectManager::ProjectManager(ProjectManager&&) noexcept = default;
ProjectManager& ProjectManager::operator=(ProjectManager&&) noexcept = default;

void ProjectManager::newProject() {
    impl_->currentPath_.clear();
    impl_->modified_ = false;
    impl_->patientInfo_ = {};
    impl_->dicomRefs_ = {};
    impl_->displaySettings_ = {};
    impl_->viewState_ = {};
    impl_->notifyStateChange();
}

std::expected<void, ProjectError>
ProjectManager::saveProject(const std::filesystem::path& path) {
    // Build manifest
    nlohmann::json manifest = {
        {"format", "dicom_viewer_project"},
        {"version", kFormatVersion},
        {"created", currentTimestamp()},
        {"components", nlohmann::json::array(
            {"patient", "dicom_refs", "settings/display", "settings/view_state"}
        )}
    };

    ZipArchive zip;
    zip.addEntry("manifest.json", manifest.dump(2));
    zip.addEntry("patient.json", patientInfoToJson(impl_->patientInfo_).dump(2));
    zip.addEntry("dicom_refs.json", dicomRefsToJson(impl_->dicomRefs_).dump(2));
    zip.addEntry("settings/display.json",
                 displaySettingsToJson(impl_->displaySettings_).dump(2));
    zip.addEntry("settings/view_state.json",
                 viewStateToJson(impl_->viewState_).dump(2));

    auto result = zip.writeTo(path);
    if (!result) {
        switch (result.error()) {
            case ZipError::FileOpenFailed:
                return std::unexpected(ProjectError::FileOpenFailed);
            case ZipError::FileWriteFailed:
                return std::unexpected(ProjectError::FileWriteFailed);
            default:
                return std::unexpected(ProjectError::InternalError);
        }
    }

    impl_->currentPath_ = path;
    impl_->modified_ = false;
    addToRecent(path);
    impl_->notifyStateChange();
    return {};
}

std::expected<void, ProjectError>
ProjectManager::loadProject(const std::filesystem::path& path) {
    auto zipResult = ZipArchive::readFrom(path);
    if (!zipResult) {
        switch (zipResult.error()) {
            case ZipError::FileOpenFailed:
                return std::unexpected(ProjectError::FileOpenFailed);
            case ZipError::InvalidArchive:
                return std::unexpected(ProjectError::InvalidFormat);
            default:
                return std::unexpected(ProjectError::InternalError);
        }
    }

    auto& zip = *zipResult;

    // Validate manifest
    if (!zip.hasEntry("manifest.json")) {
        return std::unexpected(ProjectError::ManifestMissing);
    }

    auto manifestStr = zip.readEntryAsString("manifest.json");
    if (!manifestStr) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(*manifestStr);
    } catch (const nlohmann::json::parse_error&) {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    // Check format identifier
    if (manifest.value("format", "") != "dicom_viewer_project") {
        return std::unexpected(ProjectError::InvalidFormat);
    }

    // Check version compatibility
    int version = manifest.value("version", 0);
    if (version > kFormatVersion) {
        return std::unexpected(ProjectError::VersionIncompatible);
    }

    // Load patient info
    if (zip.hasEntry("patient.json")) {
        auto patientStr = zip.readEntryAsString("patient.json");
        if (patientStr) {
            try {
                impl_->patientInfo_ = patientInfoFromJson(
                    nlohmann::json::parse(*patientStr));
            } catch (...) {
                impl_->patientInfo_ = {};
            }
        }
    }

    // Load DICOM references
    if (zip.hasEntry("dicom_refs.json")) {
        auto refsStr = zip.readEntryAsString("dicom_refs.json");
        if (refsStr) {
            try {
                impl_->dicomRefs_ = dicomRefsFromJson(
                    nlohmann::json::parse(*refsStr));
            } catch (...) {
                impl_->dicomRefs_ = {};
            }
        }
    }

    // Load display settings
    if (zip.hasEntry("settings/display.json")) {
        auto displayStr = zip.readEntryAsString("settings/display.json");
        if (displayStr) {
            try {
                impl_->displaySettings_ = displaySettingsFromJson(
                    nlohmann::json::parse(*displayStr));
            } catch (...) {
                impl_->displaySettings_ = {};
            }
        }
    }

    // Load view state
    if (zip.hasEntry("settings/view_state.json")) {
        auto viewStr = zip.readEntryAsString("settings/view_state.json");
        if (viewStr) {
            try {
                impl_->viewState_ = viewStateFromJson(
                    nlohmann::json::parse(*viewStr));
            } catch (...) {
                impl_->viewState_ = {};
            }
        }
    }

    impl_->currentPath_ = path;
    impl_->modified_ = false;
    addToRecent(path);
    impl_->notifyStateChange();
    return {};
}

bool ProjectManager::isModified() const noexcept {
    return impl_->modified_;
}

void ProjectManager::markModified() {
    impl_->modified_ = true;
    impl_->notifyStateChange();
}

std::filesystem::path ProjectManager::currentPath() const {
    return impl_->currentPath_;
}

std::string ProjectManager::projectName() const {
    if (impl_->currentPath_.empty()) {
        return "Untitled";
    }
    return impl_->currentPath_.stem().string();
}

void ProjectManager::setPatientInfo(const PatientInfo& info) {
    impl_->patientInfo_ = info;
    impl_->modified_ = true;
}

const PatientInfo& ProjectManager::patientInfo() const noexcept {
    return impl_->patientInfo_;
}

void ProjectManager::setDicomReferences(const DicomReferences& refs) {
    impl_->dicomRefs_ = refs;
    impl_->modified_ = true;
}

const DicomReferences& ProjectManager::dicomReferences() const noexcept {
    return impl_->dicomRefs_;
}

void ProjectManager::setDisplaySettings(const DisplaySettings& settings) {
    impl_->displaySettings_ = settings;
    impl_->modified_ = true;
}

const DisplaySettings& ProjectManager::displaySettings() const noexcept {
    return impl_->displaySettings_;
}

void ProjectManager::setViewState(const ViewState& state) {
    impl_->viewState_ = state;
    impl_->modified_ = true;
}

const ViewState& ProjectManager::viewState() const noexcept {
    return impl_->viewState_;
}

void ProjectManager::addToRecent(const std::filesystem::path& path,
                                 const std::string& name) {
    std::string displayName = name.empty() ? path.stem().string() : name;
    std::string ts = currentTimestamp();

    // Remove existing entry with same path
    auto& list = impl_->recentProjects_;
    std::erase_if(list, [&](const RecentProject& rp) {
        return rp.path == path;
    });

    // Insert at front
    list.insert(list.begin(), RecentProject{path, displayName, ts});

    // Trim to max
    if (list.size() > static_cast<size_t>(kMaxRecentProjects)) {
        list.resize(kMaxRecentProjects);
    }

    impl_->saveRecentProjects();
}

std::vector<RecentProject> ProjectManager::recentProjects() const {
    return impl_->recentProjects_;
}

void ProjectManager::clearRecentProjects() {
    impl_->recentProjects_.clear();
    impl_->saveRecentProjects();
}

void ProjectManager::setRecentProjectsPath(
    const std::filesystem::path& path) {
    impl_->recentProjectsPath_ = path;
    impl_->loadRecentProjects();
}

void ProjectManager::setStateChangeCallback(StateChangeCallback callback) {
    impl_->stateChangeCallback_ = std::move(callback);
}

} // namespace dicom_viewer::core

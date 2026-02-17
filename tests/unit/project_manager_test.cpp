#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "core/project_manager.hpp"
#include "core/zip_archive.hpp"

using namespace dicom_viewer::core;
namespace fs = std::filesystem;

namespace {

/// Helper to create a temp file path that is cleaned up after test
class TempFile {
public:
    TempFile(const std::string& name)
        : path_(fs::temp_directory_path() / ("dicom_viewer_test_" + name)) {}
    ~TempFile() { fs::remove(path_); }
    const fs::path& path() const { return path_; }
private:
    fs::path path_;
};

} // anonymous namespace

// =============================================================================
// ZipArchive — Write and Read roundtrip
// =============================================================================

TEST(ZipArchiveTest, WriteAndReadRoundtrip) {
    TempFile tmp("test.zip");

    // Write
    ZipArchive writer;
    writer.addEntry("hello.txt", "Hello, World!");
    writer.addEntry("data/nested.json", R"({"key": "value"})");
    auto writeResult = writer.writeTo(tmp.path());
    ASSERT_TRUE(writeResult.has_value()) << "Write failed";

    // Read
    auto readResult = ZipArchive::readFrom(tmp.path());
    ASSERT_TRUE(readResult.has_value()) << "Read failed";

    auto& reader = *readResult;
    EXPECT_TRUE(reader.hasEntry("hello.txt"));
    EXPECT_TRUE(reader.hasEntry("data/nested.json"));
    EXPECT_FALSE(reader.hasEntry("nonexistent.txt"));

    auto hello = reader.readEntryAsString("hello.txt");
    ASSERT_TRUE(hello.has_value());
    EXPECT_EQ(*hello, "Hello, World!");

    auto nested = reader.readEntryAsString("data/nested.json");
    ASSERT_TRUE(nested.has_value());
    EXPECT_EQ(*nested, R"({"key": "value"})");
}

TEST(ZipArchiveTest, EntryNames) {
    ZipArchive zip;
    zip.addEntry("a.txt", "A");
    zip.addEntry("b.txt", "B");
    zip.addEntry("c/d.txt", "D");

    auto names = zip.entryNames();
    EXPECT_EQ(names.size(), 3);
}

TEST(ZipArchiveTest, ReadFromNonexistentFile) {
    auto result = ZipArchive::readFrom("/nonexistent/path/file.zip");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ZipError::FileOpenFailed);
}

TEST(ZipArchiveTest, ReadEntryNotFound) {
    ZipArchive zip;
    zip.addEntry("exists.txt", "data");
    auto result = zip.readEntry("missing.txt");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ZipError::EntryNotFound);
}

TEST(ZipArchiveTest, LargeDataCompression) {
    TempFile tmp("large.zip");

    // Create large repetitive data that compresses well
    std::string largeData(100000, 'A');
    for (size_t i = 0; i < largeData.size(); i += 100) {
        largeData[i] = static_cast<char>('A' + (i % 26));
    }

    ZipArchive writer;
    writer.addEntry("large.bin", largeData);
    auto writeResult = writer.writeTo(tmp.path());
    ASSERT_TRUE(writeResult.has_value());

    // Verify file is smaller than original data
    auto fileSize = fs::file_size(tmp.path());
    EXPECT_LT(fileSize, largeData.size());

    // Verify roundtrip
    auto readResult = ZipArchive::readFrom(tmp.path());
    ASSERT_TRUE(readResult.has_value());

    auto content = readResult->readEntryAsString("large.bin");
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(*content, largeData);
}

TEST(ZipArchiveTest, InvalidZipFile) {
    TempFile tmp("invalid.zip");

    // Write garbage data
    std::ofstream out(tmp.path(), std::ios::binary);
    out << "This is not a ZIP file";
    out.close();

    auto result = ZipArchive::readFrom(tmp.path());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ZipError::InvalidArchive);
}

// =============================================================================
// ProjectManager — Construction
// =============================================================================

TEST(ProjectManagerTest, DefaultState) {
    ProjectManager pm;
    EXPECT_FALSE(pm.isModified());
    EXPECT_TRUE(pm.currentPath().empty());
    EXPECT_EQ(pm.projectName(), "Untitled");
}

// =============================================================================
// ProjectManager — New project
// =============================================================================

TEST(ProjectManagerTest, NewProjectResetsState) {
    ProjectManager pm;
    pm.setPatientInfo(PatientInfo{"ID1", "Patient", "", "", "", "MR"});
    pm.markModified();

    pm.newProject();

    EXPECT_FALSE(pm.isModified());
    EXPECT_TRUE(pm.currentPath().empty());
    EXPECT_EQ(pm.projectName(), "Untitled");
    EXPECT_TRUE(pm.patientInfo().patientId.empty());
}

// =============================================================================
// ProjectManager — Save and Load roundtrip
// =============================================================================

TEST(ProjectManagerTest, SaveLoadRoundtrip) {
    TempFile tmp("project.flo");

    // Set up project state
    ProjectManager saver;
    saver.setPatientInfo(PatientInfo{
        "PAT001", "John Doe", "20240101",
        "4D Flow MRI Study", "Phase Contrast", "MR"
    });
    saver.setDicomReferences(DicomReferences{
        {"/path/to/dicom/001.dcm", "/path/to/dicom/002.dcm"},
        "1.2.3.4.5.6.7.8.9",
        "1.2.3.4.5.6.7.8"
    });
    saver.setDisplaySettings(DisplaySettings{400.0, 1500.0, true, 0.7});
    saver.setViewState(ViewState{42, 3, "coronal", "quad"});

    // Save
    auto saveResult = saver.saveProject(tmp.path());
    ASSERT_TRUE(saveResult.has_value()) << "Save failed";
    EXPECT_FALSE(saver.isModified());
    EXPECT_EQ(saver.currentPath(), tmp.path());

    // Load into new ProjectManager
    ProjectManager loader;
    auto loadResult = loader.loadProject(tmp.path());
    ASSERT_TRUE(loadResult.has_value()) << "Load failed";

    // Verify patient info
    EXPECT_EQ(loader.patientInfo().patientId, "PAT001");
    EXPECT_EQ(loader.patientInfo().patientName, "John Doe");
    EXPECT_EQ(loader.patientInfo().studyDate, "20240101");
    EXPECT_EQ(loader.patientInfo().modality, "MR");

    // Verify DICOM references
    EXPECT_EQ(loader.dicomReferences().filePaths.size(), 2);
    EXPECT_EQ(loader.dicomReferences().seriesInstanceUid, "1.2.3.4.5.6.7.8.9");

    // Verify display settings
    EXPECT_DOUBLE_EQ(loader.displaySettings().windowCenter, 400.0);
    EXPECT_DOUBLE_EQ(loader.displaySettings().windowWidth, 1500.0);
    EXPECT_TRUE(loader.displaySettings().overlayVisible);
    EXPECT_DOUBLE_EQ(loader.displaySettings().overlayOpacity, 0.7);

    // Verify view state
    EXPECT_EQ(loader.viewState().sliceIndex, 42);
    EXPECT_EQ(loader.viewState().phaseIndex, 3);
    EXPECT_EQ(loader.viewState().activeView, "coronal");
    EXPECT_EQ(loader.viewState().layoutMode, "quad");

    // Verify state after load
    EXPECT_FALSE(loader.isModified());
    EXPECT_EQ(loader.currentPath(), tmp.path());
}

// =============================================================================
// ProjectManager — Project name
// =============================================================================

TEST(ProjectManagerTest, ProjectNameFromPath) {
    TempFile tmp("my_study.flo");

    ProjectManager pm;
    pm.setPatientInfo(PatientInfo{"ID1", "Test", "", "", "", "CT"});
    auto result = pm.saveProject(tmp.path());
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(pm.projectName(), "dicom_viewer_test_my_study");
}

// =============================================================================
// ProjectManager — Modified tracking
// =============================================================================

TEST(ProjectManagerTest, ModifiedTracking) {
    ProjectManager pm;
    EXPECT_FALSE(pm.isModified());

    pm.setPatientInfo(PatientInfo{"ID1", "Test", "", "", "", "CT"});
    EXPECT_TRUE(pm.isModified());

    pm.newProject();
    EXPECT_FALSE(pm.isModified());

    pm.setDisplaySettings(DisplaySettings{100.0, 200.0, false, 0.5});
    EXPECT_TRUE(pm.isModified());

    pm.markModified();
    EXPECT_TRUE(pm.isModified());
}

TEST(ProjectManagerTest, SaveClearsModifiedFlag) {
    TempFile tmp("modified_test.flo");

    ProjectManager pm;
    pm.setPatientInfo(PatientInfo{"ID1", "Test", "", "", "", "CT"});
    EXPECT_TRUE(pm.isModified());

    auto result = pm.saveProject(tmp.path());
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(pm.isModified());
}

// =============================================================================
// ProjectManager — Error handling
// =============================================================================

TEST(ProjectManagerTest, LoadNonexistentFile) {
    ProjectManager pm;
    auto result = pm.loadProject("/nonexistent/path/project.flo");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ProjectError::FileOpenFailed);
}

TEST(ProjectManagerTest, LoadInvalidFile) {
    TempFile tmp("invalid.flo");

    // Write garbage
    std::ofstream out(tmp.path(), std::ios::binary);
    out << "Not a ZIP file at all";
    out.close();

    ProjectManager pm;
    auto result = pm.loadProject(tmp.path());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ProjectError::InvalidFormat);
}

TEST(ProjectManagerTest, LoadMissingManifest) {
    TempFile tmp("no_manifest.flo");

    // Write valid ZIP but without manifest.json
    ZipArchive zip;
    zip.addEntry("patient.json", "{}");
    (void)zip.writeTo(tmp.path());

    ProjectManager pm;
    auto result = pm.loadProject(tmp.path());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ProjectError::ManifestMissing);
}

TEST(ProjectManagerTest, LoadInvalidManifestFormat) {
    TempFile tmp("bad_manifest.flo");

    // Write valid ZIP with wrong format identifier
    ZipArchive zip;
    zip.addEntry("manifest.json", R"({"format": "wrong_format", "version": 1})");
    (void)zip.writeTo(tmp.path());

    ProjectManager pm;
    auto result = pm.loadProject(tmp.path());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ProjectError::InvalidFormat);
}

TEST(ProjectManagerTest, LoadIncompatibleVersion) {
    TempFile tmp("future_version.flo");

    // Write valid ZIP with future version
    ZipArchive zip;
    zip.addEntry("manifest.json",
        R"({"format": "dicom_viewer_project", "version": 999})");
    (void)zip.writeTo(tmp.path());

    ProjectManager pm;
    auto result = pm.loadProject(tmp.path());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ProjectError::VersionIncompatible);
}

// =============================================================================
// ProjectManager — State change callback
// =============================================================================

TEST(ProjectManagerTest, StateChangeCallbackNotified) {
    ProjectManager pm;
    int callCount = 0;
    pm.setStateChangeCallback([&]() { ++callCount; });

    pm.newProject();
    EXPECT_EQ(callCount, 1);

    pm.markModified();
    EXPECT_EQ(callCount, 2);
}

// =============================================================================
// ProjectManager — Flo file is valid ZIP
// =============================================================================

TEST(ProjectManagerTest, FloFileIsValidZip) {
    TempFile tmp("valid_zip.flo");

    ProjectManager pm;
    pm.setPatientInfo(PatientInfo{"PAT", "Test", "", "", "", "MR"});
    auto result = pm.saveProject(tmp.path());
    ASSERT_TRUE(result.has_value());

    // Verify the file can be read as a raw ZIP archive
    auto zipResult = ZipArchive::readFrom(tmp.path());
    ASSERT_TRUE(zipResult.has_value());

    auto& zip = *zipResult;
    EXPECT_TRUE(zip.hasEntry("manifest.json"));
    EXPECT_TRUE(zip.hasEntry("patient.json"));
    EXPECT_TRUE(zip.hasEntry("dicom_refs.json"));
    EXPECT_TRUE(zip.hasEntry("settings/display.json"));
    EXPECT_TRUE(zip.hasEntry("settings/view_state.json"));

    // Verify manifest JSON is valid
    auto manifestStr = zip.readEntryAsString("manifest.json");
    ASSERT_TRUE(manifestStr.has_value());
    EXPECT_NE(manifestStr->find("dicom_viewer_project"), std::string::npos);
}

// =============================================================================
// ProjectManager — Recent projects
// =============================================================================

TEST(ProjectManagerTest, AddToRecentBasic) {
    ProjectManager pm;
    EXPECT_TRUE(pm.recentProjects().empty());

    pm.addToRecent("/path/to/project1.flo");
    auto recent = pm.recentProjects();
    ASSERT_EQ(recent.size(), 1);
    EXPECT_EQ(recent[0].path, "/path/to/project1.flo");
    EXPECT_EQ(recent[0].name, "project1");
    EXPECT_FALSE(recent[0].timestamp.empty());
}

TEST(ProjectManagerTest, AddToRecentCustomName) {
    ProjectManager pm;
    pm.addToRecent("/path/to/project.flo", "My Study");
    auto recent = pm.recentProjects();
    ASSERT_EQ(recent.size(), 1);
    EXPECT_EQ(recent[0].name, "My Study");
}

TEST(ProjectManagerTest, AddToRecentDeduplication) {
    ProjectManager pm;
    pm.addToRecent("/path/a.flo");
    pm.addToRecent("/path/b.flo");
    pm.addToRecent("/path/a.flo");  // Re-add moves to front

    auto recent = pm.recentProjects();
    ASSERT_EQ(recent.size(), 2);
    EXPECT_EQ(recent[0].path, "/path/a.flo");  // Most recent
    EXPECT_EQ(recent[1].path, "/path/b.flo");
}

TEST(ProjectManagerTest, AddToRecentMaxLimit) {
    ProjectManager pm;
    for (int i = 0; i < 15; ++i) {
        pm.addToRecent("/path/project" + std::to_string(i) + ".flo");
    }
    auto recent = pm.recentProjects();
    EXPECT_EQ(recent.size(), static_cast<size_t>(ProjectManager::kMaxRecentProjects));
    // Most recent should be project14
    EXPECT_EQ(recent[0].path, "/path/project14.flo");
}

TEST(ProjectManagerTest, ClearRecentProjects) {
    ProjectManager pm;
    pm.addToRecent("/path/a.flo");
    pm.addToRecent("/path/b.flo");
    EXPECT_EQ(pm.recentProjects().size(), 2);

    pm.clearRecentProjects();
    EXPECT_TRUE(pm.recentProjects().empty());
}

TEST(ProjectManagerTest, RecentProjectsPersistence) {
    TempFile recentFile("recent.json");

    {
        ProjectManager pm;
        pm.setRecentProjectsPath(recentFile.path());
        pm.addToRecent("/path/study1.flo", "Study 1");
        pm.addToRecent("/path/study2.flo", "Study 2");
    }

    // Load into new instance
    ProjectManager pm2;
    pm2.setRecentProjectsPath(recentFile.path());
    auto recent = pm2.recentProjects();
    ASSERT_EQ(recent.size(), 2);
    EXPECT_EQ(recent[0].path, "/path/study2.flo");
    EXPECT_EQ(recent[0].name, "Study 2");
    EXPECT_EQ(recent[1].path, "/path/study1.flo");
    EXPECT_EQ(recent[1].name, "Study 1");
}

TEST(ProjectManagerTest, RecentProjectsPersistenceCorruptFile) {
    TempFile recentFile("corrupt_recent.json");

    // Write garbage
    {
        std::ofstream out(recentFile.path());
        out << "not valid json at all{{{";
    }

    ProjectManager pm;
    pm.setRecentProjectsPath(recentFile.path());
    EXPECT_TRUE(pm.recentProjects().empty());
}

TEST(ProjectManagerTest, SaveAutoAddsToRecent) {
    TempFile tmp("auto_recent.flo");

    ProjectManager pm;
    pm.setPatientInfo(PatientInfo{"ID1", "Test", "", "", "", "CT"});
    auto result = pm.saveProject(tmp.path());
    ASSERT_TRUE(result.has_value());

    auto recent = pm.recentProjects();
    ASSERT_EQ(recent.size(), 1);
    EXPECT_EQ(recent[0].path, tmp.path());
}

TEST(ProjectManagerTest, LoadAutoAddsToRecent) {
    TempFile tmp("load_recent.flo");

    // Save first
    {
        ProjectManager saver;
        saver.setPatientInfo(PatientInfo{"ID1", "Test", "", "", "", "MR"});
        (void)saver.saveProject(tmp.path());
    }

    // Load into fresh instance (no recent history from save)
    ProjectManager loader;
    auto result = loader.loadProject(tmp.path());
    ASSERT_TRUE(result.has_value());

    auto recent = loader.recentProjects();
    ASSERT_EQ(recent.size(), 1);
    EXPECT_EQ(recent[0].path, tmp.path());
}

TEST(ProjectManagerTest, ClearRecentPersists) {
    TempFile recentFile("clear_persist.json");

    {
        ProjectManager pm;
        pm.setRecentProjectsPath(recentFile.path());
        pm.addToRecent("/path/a.flo");
        pm.clearRecentProjects();
    }

    ProjectManager pm2;
    pm2.setRecentProjectsPath(recentFile.path());
    EXPECT_TRUE(pm2.recentProjects().empty());
}

TEST(ProjectManagerTest, NewProjectDoesNotClearRecent) {
    ProjectManager pm;
    pm.addToRecent("/path/a.flo");
    pm.newProject();
    EXPECT_EQ(pm.recentProjects().size(), 1);
}

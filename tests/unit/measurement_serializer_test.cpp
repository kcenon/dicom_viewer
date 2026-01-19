#include "services/export/measurement_serializer.hpp"

#include <gtest/gtest.h>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <filesystem>
#include <fstream>

namespace dicom_viewer::services {
namespace {

class MeasurementSerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "measurement_serializer_test";
        std::filesystem::create_directories(testDir_);

        // Create test session data
        session_.studyInstanceUID = "1.2.840.113619.2.1.1.1";
        session_.seriesInstanceUID = "1.2.840.113619.2.1.1.2";

        session_.patient.name = "Test Patient";
        session_.patient.patientId = "12345";
        session_.patient.dateOfBirth = "1980-01-01";
        session_.patient.sex = "M";
        session_.patient.studyDate = "2025-01-01";
        session_.patient.modality = "CT";
        session_.patient.studyDescription = "CT Chest";

        // Create test distance measurements
        DistanceMeasurement dm1;
        dm1.id = 1;
        dm1.label = "D1";
        dm1.point1 = {100.0, 50.0, 25.0};
        dm1.point2 = {150.0, 75.0, 25.0};
        dm1.distanceMm = 55.9;
        dm1.sliceIndex = 100;
        dm1.visible = true;
        session_.distances.push_back(dm1);

        DistanceMeasurement dm2;
        dm2.id = 2;
        dm2.label = "D2";
        dm2.point1 = {200.0, 100.0, 50.0};
        dm2.point2 = {250.0, 150.0, 50.0};
        dm2.distanceMm = 70.71;
        dm2.sliceIndex = 150;
        dm2.visible = false;
        session_.distances.push_back(dm2);

        // Create test angle measurements
        AngleMeasurement am1;
        am1.id = 1;
        am1.label = "A1";
        am1.vertex = {100.0, 100.0, 50.0};
        am1.point1 = {50.0, 100.0, 50.0};
        am1.point2 = {100.0, 50.0, 50.0};
        am1.angleDegrees = 90.0;
        am1.isCobbAngle = false;
        am1.sliceIndex = 50;
        am1.visible = true;
        session_.angles.push_back(am1);

        AngleMeasurement am2;
        am2.id = 2;
        am2.label = "Cobb";
        am2.vertex = {150.0, 150.0, 75.0};
        am2.point1 = {100.0, 150.0, 75.0};
        am2.point2 = {150.0, 100.0, 75.0};
        am2.angleDegrees = 45.0;
        am2.isCobbAngle = true;
        am2.sliceIndex = 75;
        am2.visible = true;
        session_.angles.push_back(am2);

        // Create test area measurements
        AreaMeasurement area1;
        area1.id = 1;
        area1.label = "ROI1";
        area1.type = RoiType::Ellipse;
        area1.areaMm2 = 1256.64;
        area1.areaCm2 = 12.5664;
        area1.perimeterMm = 125.66;
        area1.centroid = {150.0, 150.0, 75.0};
        area1.sliceIndex = 75;
        area1.semiAxisA = 20.0;
        area1.semiAxisB = 20.0;
        area1.visible = true;
        session_.areas.push_back(area1);

        AreaMeasurement area2;
        area2.id = 2;
        area2.label = "ROI2";
        area2.type = RoiType::Rectangle;
        area2.areaMm2 = 400.0;
        area2.areaCm2 = 4.0;
        area2.perimeterMm = 80.0;
        area2.centroid = {200.0, 200.0, 100.0};
        area2.sliceIndex = 100;
        area2.width = 20.0;
        area2.height = 20.0;
        area2.visible = true;
        session_.areas.push_back(area2);

        // Create test segmentation labels
        SegmentationLabel label1;
        label1.id = 1;
        label1.name = "Tumor";
        label1.color = LabelColor::fromRGBA8(255, 0, 0, 180);
        label1.opacity = 0.7;
        label1.visible = true;
        session_.labels.push_back(label1);

        SegmentationLabel label2;
        label2.id = 2;
        label2.name = "Liver";
        label2.color = LabelColor::fromRGBA8(0, 255, 0, 200);
        label2.opacity = 0.5;
        label2.visible = false;
        session_.labels.push_back(label2);

        // View state
        session_.windowWidth = 400.0;
        session_.windowCenter = 40.0;
        session_.slicePositions = {120, 64, 45};

        // Metadata
        session_.version = QString::fromStdString(MeasurementSerializer::CURRENT_VERSION);
        session_.created = QDateTime::currentDateTimeUtc();
        session_.modified = QDateTime::currentDateTimeUtc();
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    std::string readFile(const std::filesystem::path& path) {
        QFile file(QString::fromStdString(path.string()));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return "";
        }
        QTextStream stream(&file);
        return stream.readAll().toStdString();
    }

    std::filesystem::path testDir_;
    SessionData session_;
};

// =============================================================================
// SerializationError tests
// =============================================================================

TEST_F(MeasurementSerializerTest, SerializationErrorDefaultSuccess) {
    SerializationError error;
    EXPECT_TRUE(error.isSuccess());
    EXPECT_EQ(error.code, SerializationError::Code::Success);
}

TEST_F(MeasurementSerializerTest, SerializationErrorToString) {
    SerializationError error;
    error.code = SerializationError::Code::FileNotFound;
    error.message = "test.dvmeas";

    std::string result = error.toString();
    EXPECT_NE(result.find("File not found"), std::string::npos);
    EXPECT_NE(result.find("test.dvmeas"), std::string::npos);
}

TEST_F(MeasurementSerializerTest, SerializationErrorAllCodes) {
    std::vector<SerializationError::Code> codes = {
        SerializationError::Code::Success,
        SerializationError::Code::FileAccessDenied,
        SerializationError::Code::FileNotFound,
        SerializationError::Code::InvalidJson,
        SerializationError::Code::InvalidSchema,
        SerializationError::Code::VersionMismatch,
        SerializationError::Code::StudyMismatch,
        SerializationError::Code::InternalError
    };

    for (auto code : codes) {
        SerializationError error;
        error.code = code;
        error.message = "test";
        std::string str = error.toString();
        EXPECT_FALSE(str.empty());
    }
}

// =============================================================================
// MeasurementSerializer construction tests
// =============================================================================

TEST_F(MeasurementSerializerTest, DefaultConstruction) {
    MeasurementSerializer serializer;
    // Should not crash
}

TEST_F(MeasurementSerializerTest, MoveConstruction) {
    MeasurementSerializer serializer1;
    MeasurementSerializer serializer2(std::move(serializer1));
    // Should not crash
}

TEST_F(MeasurementSerializerTest, MoveAssignment) {
    MeasurementSerializer serializer1;
    MeasurementSerializer serializer2;
    serializer2 = std::move(serializer1);
    // Should not crash
}

// =============================================================================
// Static method tests
// =============================================================================

TEST_F(MeasurementSerializerTest, FileExtension) {
    EXPECT_STREQ(MeasurementSerializer::FILE_EXTENSION, ".dvmeas");
}

TEST_F(MeasurementSerializerTest, CurrentVersion) {
    EXPECT_STREQ(MeasurementSerializer::CURRENT_VERSION, "1.0.0");
}

TEST_F(MeasurementSerializerTest, ApplicationId) {
    EXPECT_STREQ(MeasurementSerializer::APPLICATION_ID, "DICOM Viewer");
}

TEST_F(MeasurementSerializerTest, GetFileFilter) {
    QString filter = MeasurementSerializer::getFileFilter();
    EXPECT_TRUE(filter.contains(".dvmeas"));
    EXPECT_TRUE(filter.contains("DICOM Viewer Measurements"));
}

TEST_F(MeasurementSerializerTest, GetSupportedVersions) {
    auto versions = MeasurementSerializer::getSupportedVersions();
    EXPECT_FALSE(versions.empty());
    EXPECT_TRUE(std::find(versions.begin(), versions.end(), "1.0.0") != versions.end());
}

// =============================================================================
// Save tests
// =============================================================================

TEST_F(MeasurementSerializerTest, SaveBasicSession) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "basic.dvmeas";

    auto result = serializer.save(session_, filePath);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_TRUE(std::filesystem::exists(filePath));

    std::string content = readFile(filePath);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("version"), std::string::npos);
    EXPECT_NE(content.find("1.0.0"), std::string::npos);
    EXPECT_NE(content.find("measurements"), std::string::npos);
}

TEST_F(MeasurementSerializerTest, SaveEmptySession) {
    MeasurementSerializer serializer;
    SessionData emptySession;
    auto filePath = testDir_ / "empty.dvmeas";

    auto result = serializer.save(emptySession, filePath);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_TRUE(std::filesystem::exists(filePath));
}

TEST_F(MeasurementSerializerTest, SaveToInvalidPath) {
    MeasurementSerializer serializer;
    auto filePath = std::filesystem::path("/nonexistent/directory/test.dvmeas");

    auto result = serializer.save(session_, filePath);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SerializationError::Code::FileAccessDenied);
}

// =============================================================================
// Load tests
// =============================================================================

TEST_F(MeasurementSerializerTest, LoadNonexistentFile) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "nonexistent.dvmeas";

    auto result = serializer.load(filePath);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SerializationError::Code::FileNotFound);
}

TEST_F(MeasurementSerializerTest, LoadInvalidJson) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "invalid.dvmeas";

    // Write invalid JSON
    QFile file(QString::fromStdString(filePath.string()));
    file.open(QIODevice::WriteOnly);
    file.write("{ invalid json }");
    file.close();

    auto result = serializer.load(filePath);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SerializationError::Code::InvalidJson);
}

TEST_F(MeasurementSerializerTest, LoadMissingVersion) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "no_version.dvmeas";

    // Write JSON without version
    QFile file(QString::fromStdString(filePath.string()));
    file.open(QIODevice::WriteOnly);
    file.write(R"({"measurements": {}})");
    file.close();

    auto result = serializer.load(filePath);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SerializationError::Code::InvalidSchema);
}

TEST_F(MeasurementSerializerTest, LoadUnsupportedVersion) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "future_version.dvmeas";

    // Write JSON with unsupported version
    QFile file(QString::fromStdString(filePath.string()));
    file.open(QIODevice::WriteOnly);
    file.write(R"({"version": "99.0.0", "measurements": {}})");
    file.close();

    auto result = serializer.load(filePath);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SerializationError::Code::VersionMismatch);
}

// =============================================================================
// Round-trip tests (save then load)
// =============================================================================

TEST_F(MeasurementSerializerTest, RoundtripBasicSession) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "roundtrip.dvmeas";

    // Save
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value()) << saveResult.error().toString();

    // Load
    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value()) << loadResult.error().toString();

    const auto& loaded = *loadResult;

    // Verify study info
    EXPECT_EQ(loaded.studyInstanceUID, session_.studyInstanceUID);
    EXPECT_EQ(loaded.seriesInstanceUID, session_.seriesInstanceUID);
    EXPECT_EQ(loaded.patient.name, session_.patient.name);
    EXPECT_EQ(loaded.patient.patientId, session_.patient.patientId);

    // Verify version
    EXPECT_EQ(loaded.version.toStdString(), MeasurementSerializer::CURRENT_VERSION);
}

TEST_F(MeasurementSerializerTest, RoundtripDistanceMeasurements) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "roundtrip_distances.dvmeas";

    // Save and load
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    // Verify distances
    ASSERT_EQ(loaded.distances.size(), session_.distances.size());
    for (size_t i = 0; i < session_.distances.size(); ++i) {
        EXPECT_EQ(loaded.distances[i].id, session_.distances[i].id);
        EXPECT_EQ(loaded.distances[i].label, session_.distances[i].label);
        EXPECT_DOUBLE_EQ(loaded.distances[i].distanceMm, session_.distances[i].distanceMm);
        EXPECT_EQ(loaded.distances[i].sliceIndex, session_.distances[i].sliceIndex);
        EXPECT_EQ(loaded.distances[i].visible, session_.distances[i].visible);

        for (int j = 0; j < 3; ++j) {
            EXPECT_DOUBLE_EQ(loaded.distances[i].point1[j], session_.distances[i].point1[j]);
            EXPECT_DOUBLE_EQ(loaded.distances[i].point2[j], session_.distances[i].point2[j]);
        }
    }
}

TEST_F(MeasurementSerializerTest, RoundtripAngleMeasurements) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "roundtrip_angles.dvmeas";

    // Save and load
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    // Verify angles
    ASSERT_EQ(loaded.angles.size(), session_.angles.size());
    for (size_t i = 0; i < session_.angles.size(); ++i) {
        EXPECT_EQ(loaded.angles[i].id, session_.angles[i].id);
        EXPECT_EQ(loaded.angles[i].label, session_.angles[i].label);
        EXPECT_DOUBLE_EQ(loaded.angles[i].angleDegrees, session_.angles[i].angleDegrees);
        EXPECT_EQ(loaded.angles[i].isCobbAngle, session_.angles[i].isCobbAngle);
        EXPECT_EQ(loaded.angles[i].sliceIndex, session_.angles[i].sliceIndex);
        EXPECT_EQ(loaded.angles[i].visible, session_.angles[i].visible);
    }
}

TEST_F(MeasurementSerializerTest, RoundtripAreaMeasurements) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "roundtrip_areas.dvmeas";

    // Save and load
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    // Verify areas
    ASSERT_EQ(loaded.areas.size(), session_.areas.size());
    for (size_t i = 0; i < session_.areas.size(); ++i) {
        EXPECT_EQ(loaded.areas[i].id, session_.areas[i].id);
        EXPECT_EQ(loaded.areas[i].label, session_.areas[i].label);
        EXPECT_EQ(loaded.areas[i].type, session_.areas[i].type);
        EXPECT_DOUBLE_EQ(loaded.areas[i].areaMm2, session_.areas[i].areaMm2);
        EXPECT_DOUBLE_EQ(loaded.areas[i].areaCm2, session_.areas[i].areaCm2);
        EXPECT_EQ(loaded.areas[i].sliceIndex, session_.areas[i].sliceIndex);
    }
}

TEST_F(MeasurementSerializerTest, RoundtripSegmentationLabels) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "roundtrip_labels.dvmeas";

    // Save and load
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    // Verify labels
    ASSERT_EQ(loaded.labels.size(), session_.labels.size());
    for (size_t i = 0; i < session_.labels.size(); ++i) {
        EXPECT_EQ(loaded.labels[i].id, session_.labels[i].id);
        EXPECT_EQ(loaded.labels[i].name, session_.labels[i].name);
        EXPECT_DOUBLE_EQ(loaded.labels[i].opacity, session_.labels[i].opacity);
        EXPECT_EQ(loaded.labels[i].visible, session_.labels[i].visible);

        auto originalColor = session_.labels[i].color.toRGBA8();
        auto loadedColor = loaded.labels[i].color.toRGBA8();
        EXPECT_EQ(loadedColor[0], originalColor[0]);
        EXPECT_EQ(loadedColor[1], originalColor[1]);
        EXPECT_EQ(loadedColor[2], originalColor[2]);
        EXPECT_EQ(loadedColor[3], originalColor[3]);
    }
}

TEST_F(MeasurementSerializerTest, RoundtripViewState) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "roundtrip_viewstate.dvmeas";

    // Save and load
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    // Verify view state
    EXPECT_DOUBLE_EQ(loaded.windowWidth, session_.windowWidth);
    EXPECT_DOUBLE_EQ(loaded.windowCenter, session_.windowCenter);
    EXPECT_EQ(loaded.slicePositions[0], session_.slicePositions[0]);
    EXPECT_EQ(loaded.slicePositions[1], session_.slicePositions[1]);
    EXPECT_EQ(loaded.slicePositions[2], session_.slicePositions[2]);
}

TEST_F(MeasurementSerializerTest, RoundtripEmptySession) {
    MeasurementSerializer serializer;
    SessionData emptySession;
    auto filePath = testDir_ / "roundtrip_empty.dvmeas";

    // Save and load
    auto saveResult = serializer.save(emptySession, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    EXPECT_TRUE(loaded.distances.empty());
    EXPECT_TRUE(loaded.angles.empty());
    EXPECT_TRUE(loaded.areas.empty());
    EXPECT_TRUE(loaded.labels.empty());
}

TEST_F(MeasurementSerializerTest, RoundtripWithLabelMapPath) {
    MeasurementSerializer serializer;
    session_.labelMapPath = std::filesystem::path("/path/to/labelmap.nrrd");
    auto filePath = testDir_ / "roundtrip_labelmap.dvmeas";

    // Save and load
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    ASSERT_TRUE(loaded.labelMapPath.has_value());
    EXPECT_EQ(loaded.labelMapPath->string(), session_.labelMapPath->string());
}

// =============================================================================
// Validate tests
// =============================================================================

TEST_F(MeasurementSerializerTest, ValidateValidFile) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "valid.dvmeas";

    // Save a valid session
    serializer.save(session_, filePath);

    auto result = serializer.validate(filePath);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(MeasurementSerializerTest, ValidateNonexistentFile) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "nonexistent.dvmeas";

    auto result = serializer.validate(filePath);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SerializationError::Code::FileNotFound);
}

TEST_F(MeasurementSerializerTest, ValidateInvalidJson) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "invalid.dvmeas";

    QFile file(QString::fromStdString(filePath.string()));
    file.open(QIODevice::WriteOnly);
    file.write("not json");
    file.close();

    auto result = serializer.validate(filePath);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SerializationError::Code::InvalidJson);
}

// =============================================================================
// isCompatible tests
// =============================================================================

TEST_F(MeasurementSerializerTest, IsCompatibleSameStudy) {
    EXPECT_TRUE(MeasurementSerializer::isCompatible(
        session_, session_.studyInstanceUID));
}

TEST_F(MeasurementSerializerTest, IsCompatibleDifferentStudy) {
    EXPECT_FALSE(MeasurementSerializer::isCompatible(
        session_, "different.study.uid"));
}

TEST_F(MeasurementSerializerTest, IsCompatibleEmptySessionUID) {
    SessionData emptyUidSession;
    EXPECT_TRUE(MeasurementSerializer::isCompatible(
        emptyUidSession, "any.study.uid"));
}

TEST_F(MeasurementSerializerTest, IsCompatibleEmptyCurrentUID) {
    EXPECT_TRUE(MeasurementSerializer::isCompatible(session_, ""));
}

// =============================================================================
// Unicode support tests
// =============================================================================

TEST_F(MeasurementSerializerTest, RoundtripUnicodeLabels) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "unicode.dvmeas";

    // Add unicode labels
    session_.patient.name = "Test Patient";
    session_.distances[0].label = "Distance";
    session_.labels[0].name = "Tumor";

    // Save and load
    auto saveResult = serializer.save(session_, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;

    EXPECT_EQ(loaded.patient.name, session_.patient.name);
    EXPECT_EQ(loaded.distances[0].label, session_.distances[0].label);
    EXPECT_EQ(loaded.labels[0].name, session_.labels[0].name);
}

// =============================================================================
// Large session tests
// =============================================================================

TEST_F(MeasurementSerializerTest, RoundtripLargeSession) {
    MeasurementSerializer serializer;
    auto filePath = testDir_ / "large.dvmeas";

    // Create large session with 500 measurements
    SessionData largeSession;
    largeSession.studyInstanceUID = "1.2.3.4.5";

    for (int i = 0; i < 500; ++i) {
        DistanceMeasurement dm;
        dm.id = i;
        dm.label = "D" + std::to_string(i);
        dm.point1 = {static_cast<double>(i), static_cast<double>(i), 0.0};
        dm.point2 = {static_cast<double>(i + 10), static_cast<double>(i + 10), 0.0};
        dm.distanceMm = 14.14 * (i + 1);
        dm.sliceIndex = i;
        largeSession.distances.push_back(dm);
    }

    // Save and load
    auto saveResult = serializer.save(largeSession, filePath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = serializer.load(filePath);
    ASSERT_TRUE(loadResult.has_value());

    const auto& loaded = *loadResult;
    EXPECT_EQ(loaded.distances.size(), 500);
}

}  // namespace

// =============================================================================
// Main with Qt Application
// =============================================================================

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

}  // namespace dicom_viewer::services

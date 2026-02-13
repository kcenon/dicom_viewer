#include <gtest/gtest.h>

#include "services/dicom_find_scu.hpp"
#include "services/dicom_echo_scu.hpp"
#include "services/pacs_config.hpp"

#include <thread>

using namespace dicom_viewer::services;

class DicomFindSCUTest : public ::testing::Test {
protected:
    void SetUp() override {
        findScu = std::make_unique<DicomFindSCU>();
    }

    void TearDown() override {
        findScu.reset();
    }

    std::unique_ptr<DicomFindSCU> findScu;
};

// Test DateRange functionality
TEST(DateRangeTest, SingleDate) {
    auto range = DateRange::singleDate("20240115");
    EXPECT_EQ(range.toDicomFormat(), "20240115");
}

TEST(DateRangeTest, FromDate) {
    auto range = DateRange::fromDate("20240101");
    EXPECT_EQ(range.toDicomFormat(), "20240101-");
}

TEST(DateRangeTest, ToDate) {
    auto range = DateRange::toDate("20241231");
    EXPECT_EQ(range.toDicomFormat(), "-20241231");
}

TEST(DateRangeTest, FullRange) {
    DateRange range;
    range.from = "20240101";
    range.to = "20241231";
    EXPECT_EQ(range.toDicomFormat(), "20240101-20241231");
}

TEST(DateRangeTest, EmptyRange) {
    DateRange range;
    EXPECT_EQ(range.toDicomFormat(), "");
}

// Test FindQuery default values
TEST(FindQueryTest, DefaultValues) {
    FindQuery query;
    EXPECT_EQ(query.root, QueryRoot::PatientRoot);
    EXPECT_EQ(query.level, QueryLevel::Study);
    EXPECT_FALSE(query.patientName.has_value());
    EXPECT_FALSE(query.patientId.has_value());
    EXPECT_FALSE(query.studyDate.has_value());
    EXPECT_FALSE(query.modality.has_value());
    EXPECT_FALSE(query.accessionNumber.has_value());
}

// Test FindResult structure
TEST(FindResultTest, DefaultValues) {
    FindResult result;
    EXPECT_EQ(result.latency.count(), 0);
    EXPECT_TRUE(result.patients.empty());
    EXPECT_TRUE(result.studies.empty());
    EXPECT_TRUE(result.series.empty());
    EXPECT_TRUE(result.images.empty());
}

TEST(FindResultTest, TotalCount) {
    FindResult result;
    result.patients.push_back(PatientResult{});
    result.patients.push_back(PatientResult{});
    result.studies.push_back(StudyResult{});
    EXPECT_EQ(result.totalCount(), 3);
}

// Test PatientResult default values
TEST(PatientResultTest, DefaultValues) {
    PatientResult result;
    EXPECT_TRUE(result.patientId.empty());
    EXPECT_TRUE(result.patientName.empty());
    EXPECT_TRUE(result.patientBirthDate.empty());
    EXPECT_TRUE(result.patientSex.empty());
    EXPECT_EQ(result.numberOfStudies, 0);
}

// Test StudyResult default values
TEST(StudyResultTest, DefaultValues) {
    StudyResult result;
    EXPECT_TRUE(result.studyInstanceUid.empty());
    EXPECT_TRUE(result.studyDate.empty());
    EXPECT_TRUE(result.studyTime.empty());
    EXPECT_TRUE(result.studyDescription.empty());
    EXPECT_TRUE(result.accessionNumber.empty());
    EXPECT_TRUE(result.referringPhysician.empty());
    EXPECT_TRUE(result.patientId.empty());
    EXPECT_TRUE(result.patientName.empty());
    EXPECT_TRUE(result.modalitiesInStudy.empty());
    EXPECT_EQ(result.numberOfSeries, 0);
    EXPECT_EQ(result.numberOfInstances, 0);
}

// Test SeriesResult default values
TEST(SeriesResultTest, DefaultValues) {
    SeriesResult result;
    EXPECT_TRUE(result.seriesInstanceUid.empty());
    EXPECT_TRUE(result.studyInstanceUid.empty());
    EXPECT_TRUE(result.modality.empty());
    EXPECT_EQ(result.seriesNumber, 0);
    EXPECT_TRUE(result.seriesDescription.empty());
    EXPECT_TRUE(result.seriesDate.empty());
    EXPECT_TRUE(result.seriesTime.empty());
    EXPECT_TRUE(result.bodyPartExamined.empty());
    EXPECT_EQ(result.numberOfInstances, 0);
}

// Test ImageResult default values
TEST(ImageResultTest, DefaultValues) {
    ImageResult result;
    EXPECT_TRUE(result.sopInstanceUid.empty());
    EXPECT_TRUE(result.sopClassUid.empty());
    EXPECT_TRUE(result.seriesInstanceUid.empty());
    EXPECT_EQ(result.instanceNumber, 0);
    EXPECT_TRUE(result.contentDate.empty());
    EXPECT_TRUE(result.contentTime.empty());
}

// Test DicomFindSCU construction
TEST_F(DicomFindSCUTest, DefaultConstruction) {
    EXPECT_NE(findScu, nullptr);
}

TEST_F(DicomFindSCUTest, MoveConstructor) {
    DicomFindSCU moved(std::move(*findScu));
    EXPECT_FALSE(moved.isQuerying());
}

TEST_F(DicomFindSCUTest, MoveAssignment) {
    DicomFindSCU other;
    other = std::move(*findScu);
    EXPECT_FALSE(other.isQuerying());
}

// Test initial state
TEST_F(DicomFindSCUTest, InitialStateNotQuerying) {
    EXPECT_FALSE(findScu->isQuerying());
}

// Test find with invalid config
TEST_F(DicomFindSCUTest, FindWithInvalidConfig) {
    PacsServerConfig config;  // Invalid - empty hostname
    FindQuery query;
    auto result = findScu->find(config, query);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

TEST_F(DicomFindSCUTest, FindWithEmptyHostname) {
    PacsServerConfig config;
    config.hostname = "";
    config.calledAeTitle = "PACS_SERVER";
    FindQuery query;
    auto result = findScu->find(config, query);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

// Test find with unreachable server (will fail to connect)
TEST_F(DicomFindSCUTest, FindWithUnreachableServer) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";  // TEST-NET-1, non-routable
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(2);  // Short timeout

    FindQuery query;
    query.patientName = "SMITH*";

    auto result = findScu->find(config, query);
    EXPECT_FALSE(result.has_value());
    // Should fail with connection error or timeout
    EXPECT_TRUE(
        result.error().code == PacsError::ConnectionFailed ||
        result.error().code == PacsError::Timeout ||
        result.error().code == PacsError::NetworkError
    );
}

// Test cancel functionality
TEST_F(DicomFindSCUTest, CancelDoesNotThrow) {
    EXPECT_NO_THROW(findScu->cancel());
}

// Test SOP Class UID constants
TEST(DicomFindSCUConstantsTest, PatientRootFindSOPClassUID) {
    EXPECT_STREQ(DicomFindSCU::PATIENT_ROOT_FIND_SOP_CLASS_UID,
                 "1.2.840.10008.5.1.4.1.2.1.1");
}

TEST(DicomFindSCUConstantsTest, StudyRootFindSOPClassUID) {
    EXPECT_STREQ(DicomFindSCU::STUDY_ROOT_FIND_SOP_CLASS_UID,
                 "1.2.840.10008.5.1.4.1.2.2.1");
}

// Test query level enum
TEST(QueryLevelTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(QueryLevel::Patient), 0);
    EXPECT_EQ(static_cast<int>(QueryLevel::Study), 1);
    EXPECT_EQ(static_cast<int>(QueryLevel::Series), 2);
    EXPECT_EQ(static_cast<int>(QueryLevel::Image), 3);
}

// Test query root enum
TEST(QueryRootTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(QueryRoot::PatientRoot), 0);
    EXPECT_EQ(static_cast<int>(QueryRoot::StudyRoot), 1);
}

// Test FindQuery with all fields
TEST(FindQueryTest, AllFieldsSet) {
    FindQuery query;
    query.root = QueryRoot::StudyRoot;
    query.level = QueryLevel::Series;
    query.patientName = "DOE^JOHN";
    query.patientId = "12345";
    query.studyDate = DateRange::singleDate("20240101");
    query.modality = "CT";
    query.accessionNumber = "ACC001";
    query.studyInstanceUid = "1.2.3.4.5";
    query.seriesNumber = 1;

    EXPECT_EQ(query.root, QueryRoot::StudyRoot);
    EXPECT_EQ(query.level, QueryLevel::Series);
    EXPECT_EQ(query.patientName.value(), "DOE^JOHN");
    EXPECT_EQ(query.patientId.value(), "12345");
    EXPECT_EQ(query.studyDate->toDicomFormat(), "20240101");
    EXPECT_EQ(query.modality.value(), "CT");
    EXPECT_EQ(query.accessionNumber.value(), "ACC001");
    EXPECT_EQ(query.studyInstanceUid.value(), "1.2.3.4.5");
    EXPECT_EQ(query.seriesNumber.value(), 1);
}

// =============================================================================
// Network interaction and query edge case tests (Issue #206)
// =============================================================================

TEST_F(DicomFindSCUTest, FindWithPatientLevelQuery) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";  // Non-routable
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(1);

    FindQuery query;
    query.level = QueryLevel::Patient;
    query.root = QueryRoot::PatientRoot;
    query.patientName = "DOE*";
    query.patientId = "12345";

    auto result = findScu->find(config, query);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(
        result.error().code == PacsError::ConnectionFailed ||
        result.error().code == PacsError::Timeout ||
        result.error().code == PacsError::NetworkError
    );
}

TEST_F(DicomFindSCUTest, FindWithSeriesLevelAndStudyRoot) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(1);

    FindQuery query;
    query.level = QueryLevel::Series;
    query.root = QueryRoot::StudyRoot;
    query.studyInstanceUid = "1.2.840.113619.2.55.3.604688119.969.1234567890.123";
    query.modality = "CT";

    auto result = findScu->find(config, query);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(
        result.error().code == PacsError::ConnectionFailed ||
        result.error().code == PacsError::Timeout ||
        result.error().code == PacsError::NetworkError
    );
}

TEST_F(DicomFindSCUTest, CancelDuringFindOperation) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(30);

    FindQuery query;
    query.patientName = "SMITH*";

    std::thread findThread([this, &config, &query]() {
        (void)findScu->find(config, query);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    findScu->cancel();

    findThread.join();
    EXPECT_FALSE(findScu->isQuerying());
}

TEST(FindQueryTest, WildcardPatternFields) {
    FindQuery query;
    query.patientName = "SM?TH*";
    query.patientId = "123*";
    query.studyDescription = "*CHEST*";
    query.accessionNumber = "ACC*";

    EXPECT_EQ(query.patientName.value(), "SM?TH*");
    EXPECT_EQ(query.patientId.value(), "123*");
    EXPECT_EQ(query.studyDescription.value(), "*CHEST*");
    EXPECT_EQ(query.accessionNumber.value(), "ACC*");
}

TEST(FindResultTest, TotalCountMixedResultTypes) {
    FindResult result;
    result.patients.resize(3);
    result.studies.resize(5);
    result.series.resize(10);
    result.images.resize(25);

    EXPECT_EQ(result.totalCount(), 43);
}

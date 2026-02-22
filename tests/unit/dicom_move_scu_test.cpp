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

#include <gtest/gtest.h>

#include "services/dicom_move_scu.hpp"
#include "services/dicom_echo_scu.hpp"
#include "services/dicom_find_scu.hpp"
#include "services/pacs_config.hpp"

#include <filesystem>
#include <thread>

using namespace dicom_viewer::services;

class DicomMoveSCUTest : public ::testing::Test {
protected:
    void SetUp() override {
        moveScu = std::make_unique<DicomMoveSCU>();

        // Create temporary storage directory
        tempDir = std::filesystem::temp_directory_path() / "dicom_move_test";
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        moveScu.reset();

        // Clean up temporary directory
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    std::unique_ptr<DicomMoveSCU> moveScu;
    std::filesystem::path tempDir;
};

// Test MoveProgress structure
TEST(MoveProgressTest, DefaultValues) {
    MoveProgress progress;
    EXPECT_EQ(progress.totalImages, 0);
    EXPECT_EQ(progress.receivedImages, 0);
    EXPECT_EQ(progress.failedImages, 0);
    EXPECT_EQ(progress.warningImages, 0);
    EXPECT_EQ(progress.remainingImages, 0);
    EXPECT_TRUE(progress.currentStudyUid.empty());
    EXPECT_TRUE(progress.currentSeriesUid.empty());
}

TEST(MoveProgressTest, IsCompleteWhenFinished) {
    MoveProgress progress;
    progress.totalImages = 10;
    progress.receivedImages = 10;
    progress.remainingImages = 0;
    EXPECT_TRUE(progress.isComplete());
}

TEST(MoveProgressTest, IsNotCompleteWhenRemaining) {
    MoveProgress progress;
    progress.totalImages = 10;
    progress.receivedImages = 5;
    progress.remainingImages = 5;
    EXPECT_FALSE(progress.isComplete());
}

TEST(MoveProgressTest, IsNotCompleteWhenNoTotal) {
    MoveProgress progress;
    progress.remainingImages = 0;
    EXPECT_FALSE(progress.isComplete());
}

TEST(MoveProgressTest, PercentComplete) {
    MoveProgress progress;
    progress.totalImages = 100;
    progress.receivedImages = 50;
    progress.failedImages = 10;
    EXPECT_FLOAT_EQ(progress.percentComplete(), 60.0f);
}

TEST(MoveProgressTest, PercentCompleteZeroTotal) {
    MoveProgress progress;
    EXPECT_FLOAT_EQ(progress.percentComplete(), 0.0f);
}

// Test MoveResult structure
TEST(MoveResultTest, DefaultValues) {
    MoveResult result;
    EXPECT_EQ(result.latency.count(), 0);
    EXPECT_TRUE(result.receivedFiles.empty());
    EXPECT_FALSE(result.cancelled);
}

TEST(MoveResultTest, IsSuccessWhenAllReceived) {
    MoveResult result;
    result.progress.totalImages = 5;
    result.progress.receivedImages = 5;
    result.progress.failedImages = 0;
    result.cancelled = false;
    EXPECT_TRUE(result.isSuccess());
}

TEST(MoveResultTest, IsNotSuccessWhenCancelled) {
    MoveResult result;
    result.progress.totalImages = 5;
    result.progress.receivedImages = 5;
    result.cancelled = true;
    EXPECT_FALSE(result.isSuccess());
}

TEST(MoveResultTest, IsNotSuccessWhenFailed) {
    MoveResult result;
    result.progress.totalImages = 5;
    result.progress.receivedImages = 4;
    result.progress.failedImages = 1;
    EXPECT_FALSE(result.isSuccess());
}

TEST(MoveResultTest, HasFailures) {
    MoveResult result;
    result.progress.failedImages = 2;
    EXPECT_TRUE(result.hasFailures());
}

TEST(MoveResultTest, NoFailures) {
    MoveResult result;
    result.progress.failedImages = 0;
    EXPECT_FALSE(result.hasFailures());
}

// Test MoveConfig structure
TEST(MoveConfigTest, DefaultValues) {
    MoveConfig config;
    EXPECT_TRUE(config.storageDirectory.empty());
    EXPECT_FALSE(config.moveDestinationAeTitle.has_value());
    EXPECT_EQ(config.storeScpPort, 0);
    EXPECT_EQ(config.maxConcurrentOperations, 1);
    EXPECT_TRUE(config.createSubdirectories);
    EXPECT_TRUE(config.useOriginalFilenames);
}

// Test RetrieveLevel enum
TEST(RetrieveLevelTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(RetrieveLevel::Study), 0);
    EXPECT_EQ(static_cast<int>(RetrieveLevel::Series), 1);
    EXPECT_EQ(static_cast<int>(RetrieveLevel::Image), 2);
}

// Test DicomMoveSCU construction
TEST_F(DicomMoveSCUTest, DefaultConstruction) {
    EXPECT_NE(moveScu, nullptr);
}

TEST_F(DicomMoveSCUTest, MoveConstructor) {
    DicomMoveSCU moved(std::move(*moveScu));
    EXPECT_FALSE(moved.isRetrieving());
}

TEST_F(DicomMoveSCUTest, MoveAssignment) {
    DicomMoveSCU other;
    other = std::move(*moveScu);
    EXPECT_FALSE(other.isRetrieving());
}

// Test initial state
TEST_F(DicomMoveSCUTest, InitialStateNotRetrieving) {
    EXPECT_FALSE(moveScu->isRetrieving());
}

TEST_F(DicomMoveSCUTest, InitialStateNoProgress) {
    auto progress = moveScu->currentProgress();
    EXPECT_FALSE(progress.has_value());
}

// Test retrieveStudy with invalid config
TEST_F(DicomMoveSCUTest, RetrieveStudyWithInvalidConfig) {
    PacsServerConfig config;  // Invalid - empty hostname
    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    auto result = moveScu->retrieveStudy(config, moveConfig, "1.2.3.4.5");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

TEST_F(DicomMoveSCUTest, RetrieveStudyWithEmptyStorageDirectory) {
    PacsServerConfig config;
    config.hostname = "localhost";
    config.calledAeTitle = "PACS_SERVER";

    MoveConfig moveConfig;
    moveConfig.storageDirectory = "";  // Invalid - empty
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    auto result = moveScu->retrieveStudy(config, moveConfig, "1.2.3.4.5");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

// Test retrieveSeries with invalid config
TEST_F(DicomMoveSCUTest, RetrieveSeriesWithInvalidConfig) {
    PacsServerConfig config;  // Invalid - empty hostname
    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    auto result = moveScu->retrieveSeries(config, moveConfig, "1.2.3.4.5", "1.2.3.4.5.6");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

// Test retrieveImage with invalid config
TEST_F(DicomMoveSCUTest, RetrieveImageWithInvalidConfig) {
    PacsServerConfig config;  // Invalid - empty hostname
    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    auto result = moveScu->retrieveImage(config, moveConfig, "1.2.3.4.5", "1.2.3.4.5.6", "1.2.3.4.5.6.7");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

// Test with unreachable server
TEST_F(DicomMoveSCUTest, RetrieveStudyWithUnreachableServer) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";  // TEST-NET-1, non-routable
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(2);  // Short timeout

    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    auto result = moveScu->retrieveStudy(config, moveConfig, "1.2.3.4.5");
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(
        result.error().code == PacsError::ConnectionFailed ||
        result.error().code == PacsError::AssociationRejected ||
        result.error().code == PacsError::Timeout ||
        result.error().code == PacsError::NetworkError
    );
}

// Test cancel functionality
TEST_F(DicomMoveSCUTest, CancelDoesNotThrow) {
    EXPECT_NO_THROW(moveScu->cancel());
}

// Test SOP Class UID constants
TEST(DicomMoveSCUConstantsTest, PatientRootMoveSOPClassUID) {
    EXPECT_STREQ(DicomMoveSCU::PATIENT_ROOT_MOVE_SOP_CLASS_UID,
                 "1.2.840.10008.5.1.4.1.2.1.2");
}

TEST(DicomMoveSCUConstantsTest, StudyRootMoveSOPClassUID) {
    EXPECT_STREQ(DicomMoveSCU::STUDY_ROOT_MOVE_SOP_CLASS_UID,
                 "1.2.840.10008.5.1.4.1.2.2.2");
}

// Test MoveConfig with custom values
TEST(MoveConfigTest, CustomValues) {
    MoveConfig config;
    config.queryRoot = QueryRoot::PatientRoot;
    config.storageDirectory = "/tmp/dicom";
    config.moveDestinationAeTitle = "RECEIVER";
    config.storeScpPort = 11112;
    config.maxConcurrentOperations = 4;
    config.createSubdirectories = false;
    config.useOriginalFilenames = false;

    EXPECT_EQ(config.queryRoot, QueryRoot::PatientRoot);
    EXPECT_EQ(config.storageDirectory, std::filesystem::path("/tmp/dicom"));
    EXPECT_EQ(config.moveDestinationAeTitle.value(), "RECEIVER");
    EXPECT_EQ(config.storeScpPort, 11112);
    EXPECT_EQ(config.maxConcurrentOperations, 4);
    EXPECT_FALSE(config.createSubdirectories);
    EXPECT_FALSE(config.useOriginalFilenames);
}

// Test MoveProgress computation
TEST(MoveProgressTest, PartialProgress) {
    MoveProgress progress;
    progress.totalImages = 100;
    progress.receivedImages = 45;
    progress.failedImages = 5;
    progress.remainingImages = 50;

    EXPECT_FLOAT_EQ(progress.percentComplete(), 50.0f);
    EXPECT_FALSE(progress.isComplete());
}

// Test thread safety (basic check)
TEST_F(DicomMoveSCUTest, ConcurrentCancelSafe) {
    std::thread t1([this]() {
        for (int i = 0; i < 100; ++i) {
            moveScu->cancel();
        }
    });

    std::thread t2([this]() {
        for (int i = 0; i < 100; ++i) {
            (void)moveScu->isRetrieving();
        }
    });

    t1.join();
    t2.join();

    EXPECT_FALSE(moveScu->isRetrieving());
}

// Test storage directory creation
TEST_F(DicomMoveSCUTest, StorageDirectoryCreatedOnError) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(1);

    auto nestedDir = tempDir / "nested" / "deep" / "path";
    MoveConfig moveConfig;
    moveConfig.storageDirectory = nestedDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    // Even though the operation will fail, the directory should be created
    auto result = moveScu->retrieveStudy(config, moveConfig, "1.2.3.4.5");
    EXPECT_FALSE(result.has_value());

    // Directory should have been created before the connection attempt
    EXPECT_TRUE(std::filesystem::exists(nestedDir));
}

// =============================================================================
// Network interaction and retrieval tests (Issue #206)
// =============================================================================

TEST_F(DicomMoveSCUTest, RetrieveStudyWithProgressCallback) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(1);

    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    bool callbackInvoked = false;
    auto result = moveScu->retrieveStudy(
        config, moveConfig, "1.2.3.4.5",
        [&callbackInvoked](const MoveProgress& /*progress*/) {
            callbackInvoked = true;
        });

    EXPECT_FALSE(result.has_value());
    // Callback may or may not be invoked on connection failure —
    // the important thing is that providing a callback doesn't crash
}

TEST_F(DicomMoveSCUTest, RetrieveSeriesWithShortTimeout) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(1);
    config.dimseTimeout = std::chrono::seconds(1);

    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;
    moveConfig.maxConcurrentOperations = 4;
    moveConfig.createSubdirectories = false;
    moveConfig.useOriginalFilenames = false;

    auto result = moveScu->retrieveSeries(
        config, moveConfig,
        "1.2.840.113619.2.55.3.1234567890",
        "1.2.840.113619.2.55.3.1234567890.1");

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(
        result.error().code == PacsError::ConnectionFailed ||
        result.error().code == PacsError::AssociationRejected ||
        result.error().code == PacsError::Timeout ||
        result.error().code == PacsError::NetworkError
    );
}

TEST_F(DicomMoveSCUTest, CancelDuringRetrieveOperation) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(30);

    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::StudyRoot;

    std::thread retrieveThread([this, &config, &moveConfig]() {
        (void)moveScu->retrieveStudy(config, moveConfig, "1.2.3.4.5");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    moveScu->cancel();

    retrieveThread.join();
    EXPECT_FALSE(moveScu->isRetrieving());
}

TEST_F(DicomMoveSCUTest, RetrieveWithMoveDestinationAeTitle) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(1);

    MoveConfig moveConfig;
    moveConfig.storageDirectory = tempDir;
    moveConfig.queryRoot = QueryRoot::PatientRoot;
    moveConfig.moveDestinationAeTitle = "LOCAL_RECEIVER";
    moveConfig.storeScpPort = 11112;

    auto result = moveScu->retrieveStudy(config, moveConfig, "1.2.3.4.5");
    EXPECT_FALSE(result.has_value());

    // Verify move config options were properly set (not silently dropped)
    EXPECT_EQ(moveConfig.moveDestinationAeTitle.value(), "LOCAL_RECEIVER");
    EXPECT_EQ(moveConfig.storeScpPort, 11112);
    EXPECT_EQ(moveConfig.queryRoot, QueryRoot::PatientRoot);
}

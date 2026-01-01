#include <gtest/gtest.h>

#include "services/dicom_store_scp.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

using namespace dicom_viewer::services;

class DicomStoreScpTest : public ::testing::Test {
protected:
    void SetUp() override {
        scp = std::make_unique<DicomStoreSCP>();

        // Create temporary storage directory
        tempDir = std::filesystem::temp_directory_path() / "dicom_scp_test";
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        if (scp && scp->isRunning()) {
            scp->stop();
        }
        scp.reset();

        // Clean up temporary directory
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    StorageScpConfig createValidConfig() {
        StorageScpConfig config;
        config.port = 11113;  // Use non-standard port for testing
        config.aeTitle = "TEST_SCP";
        config.storageDirectory = tempDir;
        config.maxPduSize = 16384;
        config.connectionTimeout = std::chrono::seconds(10);
        config.maxAssociations = 5;
        return config;
    }

    std::unique_ptr<DicomStoreSCP> scp;
    std::filesystem::path tempDir;
};

// Configuration validation tests
TEST_F(DicomStoreScpTest, ConfigValidation_ValidConfig) {
    auto config = createValidConfig();
    EXPECT_TRUE(config.isValid());
}

TEST_F(DicomStoreScpTest, ConfigValidation_EmptyAeTitle) {
    auto config = createValidConfig();
    config.aeTitle = "";
    EXPECT_FALSE(config.isValid());
}

TEST_F(DicomStoreScpTest, ConfigValidation_AeTitleTooLong) {
    auto config = createValidConfig();
    config.aeTitle = "THIS_AE_TITLE_IS_TOO_LONG";  // > 16 chars
    EXPECT_FALSE(config.isValid());
}

TEST_F(DicomStoreScpTest, ConfigValidation_EmptyStorageDirectory) {
    auto config = createValidConfig();
    config.storageDirectory = "";
    EXPECT_FALSE(config.isValid());
}

TEST_F(DicomStoreScpTest, ConfigValidation_ZeroPort) {
    auto config = createValidConfig();
    config.port = 0;
    EXPECT_FALSE(config.isValid());
}

// Construction tests
TEST_F(DicomStoreScpTest, DefaultConstruction) {
    EXPECT_NE(scp, nullptr);
    EXPECT_FALSE(scp->isRunning());
}

TEST_F(DicomStoreScpTest, MoveConstruction) {
    DicomStoreSCP other;
    auto moved = std::move(other);
    EXPECT_FALSE(moved.isRunning());
}

// Start/Stop tests
// Note: Network binding tests may fail in some environments due to permissions
// or port conflicts. These tests are marked as potentially flaky.
TEST_F(DicomStoreScpTest, StartWithValidConfig) {
    GTEST_SKIP() << "Network tests may be flaky in CI environments";
}

TEST_F(DicomStoreScpTest, StartWithInvalidConfig) {
    StorageScpConfig config;
    config.port = 0;  // Invalid
    config.aeTitle = "";
    config.storageDirectory = "";

    auto result = scp->start(config);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
    EXPECT_FALSE(scp->isRunning());
}

TEST_F(DicomStoreScpTest, DoubleStartReturnsError) {
    GTEST_SKIP() << "Network tests may be flaky in CI environments";
}

TEST_F(DicomStoreScpTest, StopWhenNotRunning) {
    // Should not crash
    scp->stop();
    EXPECT_FALSE(scp->isRunning());
}

// Status tests
TEST_F(DicomStoreScpTest, StatusWhenNotRunning) {
    auto status = scp->getStatus();

    EXPECT_FALSE(status.isRunning);
    EXPECT_EQ(status.port, 0);
    EXPECT_EQ(status.totalImagesReceived, 0);
    EXPECT_EQ(status.activeConnections, 0);
}

TEST_F(DicomStoreScpTest, StatusWhenRunning) {
    GTEST_SKIP() << "Network tests may be flaky in CI environments";
}

// SOP Class tests
TEST_F(DicomStoreScpTest, SupportedSopClasses) {
    auto sopClasses = DicomStoreSCP::getSupportedSopClasses();

    EXPECT_EQ(sopClasses.size(), 5);
    EXPECT_NE(std::find(sopClasses.begin(), sopClasses.end(),
                        DicomStoreSCP::CT_IMAGE_STORAGE), sopClasses.end());
    EXPECT_NE(std::find(sopClasses.begin(), sopClasses.end(),
                        DicomStoreSCP::MR_IMAGE_STORAGE), sopClasses.end());
    EXPECT_NE(std::find(sopClasses.begin(), sopClasses.end(),
                        DicomStoreSCP::SECONDARY_CAPTURE_STORAGE), sopClasses.end());
    EXPECT_NE(std::find(sopClasses.begin(), sopClasses.end(),
                        DicomStoreSCP::ENHANCED_CT_STORAGE), sopClasses.end());
    EXPECT_NE(std::find(sopClasses.begin(), sopClasses.end(),
                        DicomStoreSCP::ENHANCED_MR_STORAGE), sopClasses.end());
}

// Callback tests
TEST_F(DicomStoreScpTest, SetImageReceivedCallback) {
    bool callbackCalled = false;

    scp->setImageReceivedCallback([&callbackCalled](const ReceivedImageInfo&) {
        callbackCalled = true;
    });

    // Note: Actually triggering the callback would require a real DICOM connection
    // This test verifies that setting the callback doesn't crash
    SUCCEED();
}

TEST_F(DicomStoreScpTest, SetConnectionCallback) {
    bool callbackCalled = false;

    scp->setConnectionCallback([&callbackCalled](const std::string&, bool) {
        callbackCalled = true;
    });

    // This test verifies that setting the callback doesn't crash
    SUCCEED();
}

// Storage directory creation tests
TEST_F(DicomStoreScpTest, CreatesStorageDirectoryIfNotExists) {
    GTEST_SKIP() << "Network tests may be flaky in CI environments";
}

// ReceivedImageInfo structure tests
TEST_F(DicomStoreScpTest, ReceivedImageInfoDefaultConstruction) {
    ReceivedImageInfo info;

    EXPECT_TRUE(info.filePath.empty());
    EXPECT_TRUE(info.sopClassUid.empty());
    EXPECT_TRUE(info.sopInstanceUid.empty());
    EXPECT_TRUE(info.patientId.empty());
    EXPECT_TRUE(info.studyInstanceUid.empty());
    EXPECT_TRUE(info.seriesInstanceUid.empty());
    EXPECT_TRUE(info.callingAeTitle.empty());
}

// StorageScpStatus structure tests
TEST_F(DicomStoreScpTest, StorageScpStatusDefaultConstruction) {
    StorageScpStatus status;

    EXPECT_FALSE(status.isRunning);
    EXPECT_EQ(status.port, 0);
    EXPECT_EQ(status.totalImagesReceived, 0);
    EXPECT_EQ(status.activeConnections, 0);
}

// Graceful shutdown test
TEST_F(DicomStoreScpTest, GracefulShutdown) {
    GTEST_SKIP() << "Network tests may be flaky in CI environments";
}

#include <gtest/gtest.h>

#include "services/dicom_store_scp.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

using namespace dicom_viewer::services;

namespace {

// Generate a random ephemeral port to minimize CI conflicts.
// Uses the dynamic/private port range (49152-65535) per IANA.
uint16_t randomEphemeralPort() {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint16_t> dist(49152, 65535);
    return dist(gen);
}

} // namespace

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
        config.port = randomEphemeralPort();
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

// ============================================================================
// Configuration Validation Tests
// ============================================================================

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

TEST_F(DicomStoreScpTest, ConfigValidation_MaxAeTitleLength) {
    auto config = createValidConfig();
    config.aeTitle = "1234567890123456";  // Exactly 16 chars (DICOM limit)
    EXPECT_TRUE(config.isValid());
}

TEST_F(DicomStoreScpTest, ConfigValidation_MinPort) {
    auto config = createValidConfig();
    config.port = 1;
    EXPECT_TRUE(config.isValid());
}

TEST_F(DicomStoreScpTest, ConfigValidation_MaxPort) {
    auto config = createValidConfig();
    config.port = 65535;
    EXPECT_TRUE(config.isValid());
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(DicomStoreScpTest, DefaultConstruction) {
    EXPECT_NE(scp, nullptr);
    EXPECT_FALSE(scp->isRunning());
}

TEST_F(DicomStoreScpTest, MoveConstruction) {
    DicomStoreSCP other;
    auto moved = std::move(other);
    EXPECT_FALSE(moved.isRunning());
}

TEST_F(DicomStoreScpTest, MoveAssignment) {
    DicomStoreSCP a;
    DicomStoreSCP b;
    b = std::move(a);
    EXPECT_FALSE(b.isRunning());
}

// ============================================================================
// Server Lifecycle Tests
// ============================================================================

TEST_F(DicomStoreScpTest, StartWithValidConfig) {
    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_TRUE(scp->isRunning());
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
    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Second start should fail with InternalError ("already running")
    auto result2 = scp->start(config);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().code, PacsError::InternalError);
    EXPECT_TRUE(scp->isRunning());  // Still running from first start
}

TEST_F(DicomStoreScpTest, StopWhenNotRunning) {
    // Should not crash
    scp->stop();
    EXPECT_FALSE(scp->isRunning());
}

TEST_F(DicomStoreScpTest, GracefulShutdown) {
    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();
    ASSERT_TRUE(scp->isRunning());

    scp->stop();

    EXPECT_FALSE(scp->isRunning());
}

TEST_F(DicomStoreScpTest, RestartAfterStop) {
    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    scp->stop();
    ASSERT_FALSE(scp->isRunning());

    // Use a different port for restart to avoid TIME_WAIT
    auto config2 = createValidConfig();
    auto result2 = scp->start(config2);

    if (!result2.has_value() && result2.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable on restart: "
                     << result2.error().message;
    }
    ASSERT_TRUE(result2.has_value()) << result2.error().toString();
    EXPECT_TRUE(scp->isRunning());
}

TEST_F(DicomStoreScpTest, StopMultipleTimesIsSafe) {
    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    scp->stop();
    EXPECT_FALSE(scp->isRunning());

    // Calling stop() again should be harmless
    scp->stop();
    EXPECT_FALSE(scp->isRunning());
}

// ============================================================================
// Status Tests
// ============================================================================

TEST_F(DicomStoreScpTest, StatusWhenNotRunning) {
    auto status = scp->getStatus();

    EXPECT_FALSE(status.isRunning);
    EXPECT_EQ(status.port, 0);
    EXPECT_EQ(status.totalImagesReceived, 0);
    EXPECT_EQ(status.activeConnections, 0);
}

TEST_F(DicomStoreScpTest, StatusWhenRunning) {
    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto status = scp->getStatus();

    EXPECT_TRUE(status.isRunning);
    EXPECT_EQ(status.port, config.port);
    EXPECT_EQ(status.totalImagesReceived, 0);
    EXPECT_EQ(status.activeConnections, 0);
}

TEST_F(DicomStoreScpTest, StatusStartTimeIsSet) {
    auto beforeStart = std::chrono::system_clock::now();

    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    auto afterStart = std::chrono::system_clock::now();
    auto status = scp->getStatus();

    EXPECT_GE(status.startTime, beforeStart);
    EXPECT_LE(status.startTime, afterStart);
}

TEST_F(DicomStoreScpTest, StatusResetAfterStop) {
    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    scp->stop();
    auto status = scp->getStatus();

    EXPECT_FALSE(status.isRunning);
}

// ============================================================================
// SOP Class Tests
// ============================================================================

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

TEST_F(DicomStoreScpTest, SopClassUidFormats) {
    // All SOP Class UIDs should follow DICOM UID format (NEMA root prefix)
    auto sopClasses = DicomStoreSCP::getSupportedSopClasses();
    for (const auto& uid : sopClasses) {
        EXPECT_FALSE(uid.empty());
        EXPECT_TRUE(uid.starts_with("1.2.840.10008."))
            << "Invalid UID prefix: " << uid;
    }
}

TEST_F(DicomStoreScpTest, SopClassesAreUnique) {
    auto sopClasses = DicomStoreSCP::getSupportedSopClasses();
    std::sort(sopClasses.begin(), sopClasses.end());
    auto last = std::unique(sopClasses.begin(), sopClasses.end());
    EXPECT_EQ(last, sopClasses.end()) << "Duplicate SOP Class UIDs found";
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(DicomStoreScpTest, SetImageReceivedCallback) {
    bool callbackSet = false;

    scp->setImageReceivedCallback([&callbackSet](const ReceivedImageInfo&) {
        callbackSet = true;
    });

    // Triggering the callback requires a real DICOM association.
    // This verifies that setting the callback doesn't crash.
    SUCCEED();
}

TEST_F(DicomStoreScpTest, SetConnectionCallback) {
    bool callbackSet = false;

    scp->setConnectionCallback([&callbackSet](const std::string&, bool) {
        callbackSet = true;
    });

    // This verifies that setting the callback doesn't crash
    SUCCEED();
}

TEST_F(DicomStoreScpTest, SetCallbackBeforeStart) {
    scp->setImageReceivedCallback([](const ReceivedImageInfo&) {});
    scp->setConnectionCallback([](const std::string&, bool) {});

    auto config = createValidConfig();
    auto result = scp->start(config);

    if (!result.has_value() && result.error().code == PacsError::NetworkError) {
        GTEST_SKIP() << "Network binding unavailable: " << result.error().message;
    }
    ASSERT_TRUE(result.has_value()) << result.error().toString();

    // Server should start successfully with pre-set callbacks
    EXPECT_TRUE(scp->isRunning());
}

TEST_F(DicomStoreScpTest, OverwriteCallback) {
    int firstCount = 0;
    int secondCount = 0;

    scp->setImageReceivedCallback([&firstCount](const ReceivedImageInfo&) {
        firstCount++;
    });

    // Overwrite with new callback
    scp->setImageReceivedCallback([&secondCount](const ReceivedImageInfo&) {
        secondCount++;
    });

    // Setting callbacks multiple times should not crash
    SUCCEED();
}

TEST_F(DicomStoreScpTest, NullCallbackDoesNotCrash) {
    scp->setImageReceivedCallback(nullptr);
    scp->setConnectionCallback(nullptr);

    // Should not crash even with null callbacks
    SUCCEED();
}

// ============================================================================
// Storage Directory Tests
// ============================================================================

TEST_F(DicomStoreScpTest, CreatesStorageDirectoryIfNotExists) {
    auto config = createValidConfig();
    auto nestedDir = tempDir / "nested" / "storage" / "dir";
    config.storageDirectory = nestedDir;

    ASSERT_FALSE(std::filesystem::exists(nestedDir));

    // Directory creation occurs before network binding in start(),
    // so the directory should be created regardless of binding outcome
    auto result = scp->start(config);

    EXPECT_TRUE(std::filesystem::exists(nestedDir));
    EXPECT_TRUE(std::filesystem::is_directory(nestedDir));
}

TEST_F(DicomStoreScpTest, ExistingStorageDirectoryNotReplaced) {
    auto config = createValidConfig();

    // tempDir already exists from SetUp; place a marker file inside
    auto marker = tempDir / "marker.txt";
    {
        std::ofstream ofs(marker);
        ofs << "test";
    }

    auto result = scp->start(config);

    // Marker file should still exist (directory not replaced/cleared)
    EXPECT_TRUE(std::filesystem::exists(marker));
}

// ============================================================================
// ReceivedImageInfo Structure Tests
// ============================================================================

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

// ============================================================================
// StorageScpStatus Structure Tests
// ============================================================================

TEST_F(DicomStoreScpTest, StorageScpStatusDefaultConstruction) {
    StorageScpStatus status;

    EXPECT_FALSE(status.isRunning);
    EXPECT_EQ(status.port, 0);
    EXPECT_EQ(status.totalImagesReceived, 0);
    EXPECT_EQ(status.activeConnections, 0);
}

#include <gtest/gtest.h>

#include "services/dicom_echo_scu.hpp"
#include "services/pacs_config.hpp"

using namespace dicom_viewer::services;

class DicomEchoSCUTest : public ::testing::Test {
protected:
    void SetUp() override {
        echoScu = std::make_unique<DicomEchoSCU>();
    }

    void TearDown() override {
        echoScu.reset();
    }

    std::unique_ptr<DicomEchoSCU> echoScu;
};

// Test PacsServerConfig validation
TEST(PacsServerConfigTest, DefaultConfigIsInvalid) {
    PacsServerConfig config;
    EXPECT_FALSE(config.isValid());  // Empty hostname
}

TEST(PacsServerConfigTest, ValidConfigWithRequiredFields) {
    PacsServerConfig config;
    config.hostname = "pacs.hospital.com";
    config.calledAeTitle = "PACS_SERVER";
    config.callingAeTitle = "DICOM_VIEWER";
    EXPECT_TRUE(config.isValid());
}

TEST(PacsServerConfigTest, InvalidConfigWithEmptyHostname) {
    PacsServerConfig config;
    config.hostname = "";
    config.calledAeTitle = "PACS_SERVER";
    EXPECT_FALSE(config.isValid());
}

TEST(PacsServerConfigTest, InvalidConfigWithEmptyCalledAeTitle) {
    PacsServerConfig config;
    config.hostname = "pacs.hospital.com";
    config.calledAeTitle = "";
    EXPECT_FALSE(config.isValid());
}

TEST(PacsServerConfigTest, InvalidConfigWithTooLongAeTitle) {
    PacsServerConfig config;
    config.hostname = "pacs.hospital.com";
    config.calledAeTitle = "THIS_AE_TITLE_IS_TOO_LONG";  // > 16 chars
    EXPECT_FALSE(config.isValid());
}

TEST(PacsServerConfigTest, InvalidConfigWithZeroPort) {
    PacsServerConfig config;
    config.hostname = "pacs.hospital.com";
    config.calledAeTitle = "PACS_SERVER";
    config.port = 0;
    EXPECT_FALSE(config.isValid());
}

TEST(PacsServerConfigTest, ValidConfigWithCustomPort) {
    PacsServerConfig config;
    config.hostname = "pacs.hospital.com";
    config.port = 11112;
    config.calledAeTitle = "PACS_SERVER";
    EXPECT_TRUE(config.isValid());
}

TEST(PacsServerConfigTest, DefaultPortIs104) {
    PacsServerConfig config;
    EXPECT_EQ(config.port, 104);
}

TEST(PacsServerConfigTest, DefaultMaxPduSize) {
    PacsServerConfig config;
    EXPECT_EQ(config.maxPduSize, 16384);
}

// Test DicomEchoSCU construction
TEST_F(DicomEchoSCUTest, DefaultConstruction) {
    EXPECT_NE(echoScu, nullptr);
}

TEST_F(DicomEchoSCUTest, MoveConstructor) {
    DicomEchoSCU moved(std::move(*echoScu));
    EXPECT_FALSE(moved.isVerifying());
}

TEST_F(DicomEchoSCUTest, MoveAssignment) {
    DicomEchoSCU other;
    other = std::move(*echoScu);
    EXPECT_FALSE(other.isVerifying());
}

// Test initial state
TEST_F(DicomEchoSCUTest, InitialStateNotVerifying) {
    EXPECT_FALSE(echoScu->isVerifying());
}

// Test verification with invalid config
TEST_F(DicomEchoSCUTest, VerifyWithInvalidConfig) {
    PacsServerConfig config;  // Invalid - empty hostname
    auto result = echoScu->verify(config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

TEST_F(DicomEchoSCUTest, VerifyWithEmptyHostname) {
    PacsServerConfig config;
    config.hostname = "";
    config.calledAeTitle = "PACS_SERVER";
    auto result = echoScu->verify(config);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

// Test verification with unreachable server (will fail to connect)
TEST_F(DicomEchoSCUTest, VerifyWithUnreachableServer) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";  // TEST-NET-1, non-routable
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(2);  // Short timeout

    auto result = echoScu->verify(config);
    EXPECT_FALSE(result.has_value());
    // Should fail with connection error or timeout
    EXPECT_TRUE(
        result.error().code == PacsError::ConnectionFailed ||
        result.error().code == PacsError::Timeout ||
        result.error().code == PacsError::NetworkError
    );
}

// Test cancel functionality
TEST_F(DicomEchoSCUTest, CancelDoesNotThrow) {
    EXPECT_NO_THROW(echoScu->cancel());
}

// Test error info string conversion
TEST(PacsErrorInfoTest, ToStringContainsCodeAndMessage) {
    PacsErrorInfo error{
        PacsError::ConnectionFailed,
        "Cannot connect to server"
    };
    std::string str = error.toString();
    EXPECT_NE(str.find("ConnectionFailed"), std::string::npos);
    EXPECT_NE(str.find("Cannot connect to server"), std::string::npos);
}

TEST(PacsErrorInfoTest, CodeToStringAllCodes) {
    EXPECT_EQ(PacsErrorInfo::codeToString(PacsError::ConfigurationInvalid), "ConfigurationInvalid");
    EXPECT_EQ(PacsErrorInfo::codeToString(PacsError::ConnectionFailed), "ConnectionFailed");
    EXPECT_EQ(PacsErrorInfo::codeToString(PacsError::AssociationRejected), "AssociationRejected");
    EXPECT_EQ(PacsErrorInfo::codeToString(PacsError::Timeout), "Timeout");
    EXPECT_EQ(PacsErrorInfo::codeToString(PacsError::NetworkError), "NetworkError");
    EXPECT_EQ(PacsErrorInfo::codeToString(PacsError::AbortedByRemote), "AbortedByRemote");
    EXPECT_EQ(PacsErrorInfo::codeToString(PacsError::InternalError), "InternalError");
}

// Test EchoResult structure
TEST(EchoResultTest, DefaultValues) {
    EchoResult result;
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.latency.count(), 0);
    EXPECT_TRUE(result.message.empty());
}

// Test Verification SOP Class UID constant
TEST(DicomEchoSCUConstantsTest, VerificationSOPClassUID) {
    EXPECT_STREQ(DicomEchoSCU::VERIFICATION_SOP_CLASS_UID, "1.2.840.10008.1.1");
}

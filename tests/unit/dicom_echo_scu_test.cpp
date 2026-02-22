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

#include "services/dicom_echo_scu.hpp"
#include "services/dicom_store_scp.hpp"
#include "services/pacs_config.hpp"

#include <chrono>
#include <filesystem>
#include <random>
#include <thread>

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
    // Should fail with network/association error
    EXPECT_TRUE(
        result.error().code == PacsError::ConnectionFailed ||
        result.error().code == PacsError::AssociationRejected ||
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

// =============================================================================
// Network interaction tests (Issue #206)
// =============================================================================

namespace {

uint16_t randomEphemeralPort() {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint16_t> dist(49152, 65535);
    return dist(gen);
}

} // namespace

class DicomEchoSCUNetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        echoScu_ = std::make_unique<DicomEchoSCU>();
        scp_ = std::make_unique<DicomStoreSCP>();
        tempDir_ = std::filesystem::temp_directory_path() / "dicom_echo_net_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override {
        if (scp_ && scp_->isRunning()) {
            scp_->stop();
        }
        scp_.reset();
        echoScu_.reset();
        std::error_code ec;
        std::filesystem::remove_all(tempDir_, ec);
    }

    bool startLocalScp() {
        StorageScpConfig config;
        config.port = randomEphemeralPort();
        config.aeTitle = "ECHO_TEST_SCP";
        config.storageDirectory = tempDir_;
        config.connectionTimeout = std::chrono::seconds(10);
        auto result = scp_->start(config);
        if (!result.has_value()) {
            return false;
        }
        scpPort_ = config.port;
        return true;
    }

    PacsServerConfig createLocalScuConfig() {
        PacsServerConfig config;
        config.hostname = "127.0.0.1";
        config.port = scpPort_;
        config.calledAeTitle = "ECHO_TEST_SCP";
        config.callingAeTitle = "DICOM_VIEWER";
        config.connectionTimeout = std::chrono::seconds(5);
        return config;
    }

    std::unique_ptr<DicomEchoSCU> echoScu_;
    std::unique_ptr<DicomStoreSCP> scp_;
    std::filesystem::path tempDir_;
    uint16_t scpPort_ = 0;
};

TEST_F(DicomEchoSCUNetworkTest, VerifyAgainstLocalSCP) {
    if (!startLocalScp()) {
        GTEST_SKIP() << "Cannot start local SCP for echo network test";
    }

    auto config = createLocalScuConfig();
    auto result = echoScu_->verify(config);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_TRUE(result->success);
}

TEST_F(DicomEchoSCUNetworkTest, EchoLatencyIsPositive) {
    if (!startLocalScp()) {
        GTEST_SKIP() << "Cannot start local SCP for echo network test";
    }

    auto config = createLocalScuConfig();
    auto result = echoScu_->verify(config);

    ASSERT_TRUE(result.has_value()) << result.error().toString();
    EXPECT_GT(result->latency.count(), 0)
        << "Echo latency should be positive on localhost";
    EXPECT_LT(result->latency.count(), 5000)
        << "Echo latency should be under 5 seconds on localhost";
}

TEST_F(DicomEchoSCUNetworkTest, MultipleSuccessiveEchoCalls) {
    if (!startLocalScp()) {
        GTEST_SKIP() << "Cannot start local SCP for echo network test";
    }

    auto config = createLocalScuConfig();

    for (int i = 0; i < 5; ++i) {
        auto result = echoScu_->verify(config);
        ASSERT_TRUE(result.has_value())
            << "Echo #" << i << " failed: " << result.error().toString();
        EXPECT_TRUE(result->success);
    }
}

TEST_F(DicomEchoSCUNetworkTest, CancelDuringEchoOperation) {
    PacsServerConfig config;
    config.hostname = "192.0.2.1";  // Non-routable address
    config.port = 104;
    config.calledAeTitle = "PACS_SERVER";
    config.connectionTimeout = std::chrono::seconds(30);

    std::thread echoThread([this, &config]() {
        (void)echoScu_->verify(config);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    echoScu_->cancel();

    echoThread.join();
    EXPECT_FALSE(echoScu_->isVerifying());
}

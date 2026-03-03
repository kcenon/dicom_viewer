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

#include <services/dicomweb_service.hpp>

#include <pacs/web/rest_config.hpp>
#include <pacs/web/rest_types.hpp>

#include <string>

namespace {

using namespace dicom_viewer::services;

class DicomWebServiceTest : public ::testing::Test {
protected:
    DicomWebService service;
};

// --- Default State ---

TEST_F(DicomWebServiceTest, DefaultNotRunning) {
    EXPECT_FALSE(service.isRunning());
}

TEST_F(DicomWebServiceTest, DefaultConfigDisabled) {
    auto config = service.getConfig();
    EXPECT_FALSE(config.enabled);
}

TEST_F(DicomWebServiceTest, DefaultPortZero) {
    EXPECT_EQ(service.actualPort(), 0u);
}

// --- Configuration Validation ---

TEST_F(DicomWebServiceTest, ConfigureDisabledSucceeds) {
    DicomWebConfig config;
    config.enabled = false;
    auto result = service.configure(config);
    EXPECT_TRUE(result.has_value())
        << "Disabled configuration should always succeed";
}

TEST_F(DicomWebServiceTest, ConfigureZeroPortFails) {
    DicomWebConfig config;
    config.enabled = true;
    config.port = 0;
    auto result = service.configure(config);
    EXPECT_FALSE(result.has_value())
        << "Zero port should fail validation";
}

TEST_F(DicomWebServiceTest, ConfigureZeroConcurrencyFails) {
    DicomWebConfig config;
    config.enabled = true;
    config.concurrency = 0;
    auto result = service.configure(config);
    EXPECT_FALSE(result.has_value())
        << "Zero concurrency should fail validation";
}

TEST_F(DicomWebServiceTest, ConfigureValidSucceeds) {
    DicomWebConfig config;
    config.enabled = true;
    config.port = 18080;
    config.concurrency = 2;
    auto result = service.configure(config);
    EXPECT_TRUE(result.has_value())
        << "Valid configuration should succeed";
}

TEST_F(DicomWebServiceTest, ConfigurePreservesSettings) {
    DicomWebConfig config;
    config.enabled = true;
    config.bindAddress = "127.0.0.1";
    config.port = 9090;
    config.concurrency = 8;
    config.enableCors = false;
    config.corsAllowedOrigins = "https://example.com";
    config.requestTimeoutSeconds = 60;
    config.maxBodySize = 50 * 1024 * 1024;

    (void)service.configure(config);
    auto retrieved = service.getConfig();

    EXPECT_EQ(retrieved.bindAddress, "127.0.0.1");
    EXPECT_EQ(retrieved.port, 9090);
    EXPECT_EQ(retrieved.concurrency, 8u);
    EXPECT_FALSE(retrieved.enableCors);
    EXPECT_EQ(retrieved.corsAllowedOrigins, "https://example.com");
    EXPECT_EQ(retrieved.requestTimeoutSeconds, 60u);
    EXPECT_EQ(retrieved.maxBodySize, 50u * 1024 * 1024);
}

// --- Start Without Configure ---

TEST_F(DicomWebServiceTest, StartWithoutConfigureFails) {
    auto result = service.start();
    EXPECT_FALSE(result.has_value())
        << "Starting without configuration should fail";
}

TEST_F(DicomWebServiceTest, StartWhenDisabledFails) {
    DicomWebConfig config;
    config.enabled = false;
    (void)service.configure(config);

    auto result = service.start();
    EXPECT_FALSE(result.has_value())
        << "Starting when disabled should fail";
}

// --- Stop Safety ---

TEST_F(DicomWebServiceTest, StopWhenNotRunningIsSafe) {
    EXPECT_NO_THROW(service.stop());
}

TEST_F(DicomWebServiceTest, StopWhenDisabledIsSafe) {
    DicomWebConfig config;
    config.enabled = false;
    (void)service.configure(config);

    EXPECT_NO_THROW(service.stop());
}

TEST_F(DicomWebServiceTest, DoubleStopIsSafe) {
    EXPECT_NO_THROW(service.stop());
    EXPECT_NO_THROW(service.stop());
}

// --- DicomWebConfig::isValid ---

TEST_F(DicomWebServiceTest, DisabledConfigAlwaysValid) {
    DicomWebConfig config;
    config.enabled = false;
    config.port = 0;
    config.concurrency = 0;
    EXPECT_TRUE(config.isValid())
        << "Disabled config should always be valid regardless of fields";
}

TEST_F(DicomWebServiceTest, ValidEnabledConfig) {
    DicomWebConfig config;
    config.enabled = true;
    config.port = 8080;
    config.concurrency = 4;
    EXPECT_TRUE(config.isValid());
}

TEST_F(DicomWebServiceTest, InvalidEnabledConfigZeroPort) {
    DicomWebConfig config;
    config.enabled = true;
    config.port = 0;
    config.concurrency = 4;
    EXPECT_FALSE(config.isValid());
}

TEST_F(DicomWebServiceTest, InvalidEnabledConfigZeroConcurrency) {
    DicomWebConfig config;
    config.enabled = true;
    config.port = 8080;
    config.concurrency = 0;
    EXPECT_FALSE(config.isValid());
}

// --- Config Defaults ---

TEST_F(DicomWebServiceTest, ConfigDefaults) {
    DicomWebConfig config;
    EXPECT_FALSE(config.enabled);
    EXPECT_EQ(config.bindAddress, "0.0.0.0");
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.concurrency, 4u);
    EXPECT_TRUE(config.enableCors);
    EXPECT_EQ(config.corsAllowedOrigins, "*");
    EXPECT_FALSE(config.enableTls);
    EXPECT_TRUE(config.tlsCertPath.empty());
    EXPECT_TRUE(config.tlsKeyPath.empty());
    EXPECT_EQ(config.requestTimeoutSeconds, 30u);
    EXPECT_EQ(config.maxBodySize, 10u * 1024 * 1024);
}

// --- Move Semantics ---

TEST_F(DicomWebServiceTest, MoveConstruction) {
    DicomWebConfig config;
    config.enabled = false;
    config.port = 9999;
    (void)service.configure(config);

    DicomWebService moved(std::move(service));
    auto retrievedConfig = moved.getConfig();
    EXPECT_EQ(retrievedConfig.port, 9999);
}

TEST_F(DicomWebServiceTest, MoveAssignment) {
    DicomWebConfig config;
    config.enabled = false;
    config.port = 7777;
    (void)service.configure(config);

    DicomWebService other;
    other = std::move(service);
    auto retrievedConfig = other.getConfig();
    EXPECT_EQ(retrievedConfig.port, 7777);
}

// --- pacs::web Types ---

TEST_F(DicomWebServiceTest, PacsRestServerConfigDefaults) {
    pacs::web::rest_server_config config;
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.concurrency, 4u);
    EXPECT_TRUE(config.enable_cors);
    EXPECT_FALSE(config.enable_tls);
}

TEST_F(DicomWebServiceTest, PacsHttpStatusValues) {
    EXPECT_EQ(static_cast<uint16_t>(pacs::web::http_status::ok), 200);
    EXPECT_EQ(static_cast<uint16_t>(pacs::web::http_status::not_found), 404);
    EXPECT_EQ(static_cast<uint16_t>(pacs::web::http_status::internal_server_error), 500);
}

TEST_F(DicomWebServiceTest, PacsJsonHelpers) {
    auto errorJson = pacs::web::make_error_json("NOT_FOUND", "Resource not found");
    EXPECT_FALSE(errorJson.empty());
    EXPECT_NE(errorJson.find("NOT_FOUND"), std::string::npos);

    auto successJson = pacs::web::make_success_json("Operation completed");
    EXPECT_FALSE(successJson.empty());
    EXPECT_NE(successJson.find("success"), std::string::npos);
}

TEST_F(DicomWebServiceTest, PacsJsonEscape) {
    auto escaped = pacs::web::json_escape("hello \"world\"");
    EXPECT_NE(escaped.find("\\\""), std::string::npos);
    EXPECT_EQ(escaped.find("\"world\""), std::string::npos);
}

// --- TLS Config ---

TEST_F(DicomWebServiceTest, TlsConfigPreserved) {
    DicomWebConfig config;
    config.enabled = true;
    config.port = 8443;
    config.enableTls = true;
    config.tlsCertPath = "/path/to/cert.pem";
    config.tlsKeyPath = "/path/to/key.pem";

    (void)service.configure(config);
    auto retrieved = service.getConfig();

    EXPECT_TRUE(retrieved.enableTls);
    EXPECT_EQ(retrieved.tlsCertPath, "/path/to/cert.pem");
    EXPECT_EQ(retrieved.tlsKeyPath, "/path/to/key.pem");
}

} // anonymous namespace

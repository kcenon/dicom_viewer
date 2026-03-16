// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
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

#include <services/config/deployment_config.hpp>

#include <cstdlib>
#include <string>

namespace {

using namespace dicom_viewer::services;

static const char* kOnPremiseYaml = R"(
server:
  rest_port: 9090
  ws_port: 9091
  log_level: debug
  max_sessions: 16

redis:
  host: redis.local
  port: 6380
  database: 2
  max_memory_mb: 512

postgres:
  host: pg.local
  port: 5433
  database: test_db
  user: test_user
  ssl_mode: prefer

tls:
  cert_path: /etc/ssl/cert.pem
  key_path: /etc/ssl/key.pem
  ca_path: /etc/ssl/ca.pem

storage:
  type: filesystem
  base_path: /mnt/dicom

audit:
  sink_type: postgresql
  file_path: /var/log/audit.log
)";

static const char* kCloudYaml = R"(
server:
  rest_port: 8080
  max_sessions: 32

storage:
  type: s3
  s3_bucket: my-dicom-bucket
  s3_region: us-west-2

audit:
  sink_type: cloudwatch
  cloudwatch_group: /dicom/audit
  cloudwatch_stream: main
)";

// --- Parsing tests ---

TEST(DeploymentConfigTest, ParseOnPremiseConfig) {
    auto result = parseDeploymentConfig(kOnPremiseYaml);
    ASSERT_TRUE(result.has_value());

    const auto& cfg = *result;
    EXPECT_EQ(cfg.server.restPort, 9090);
    EXPECT_EQ(cfg.server.wsPort, 9091);
    EXPECT_EQ(cfg.server.logLevel, "debug");
    EXPECT_EQ(cfg.server.maxSessions, 16u);
}

TEST(DeploymentConfigTest, ParseRedisSection) {
    auto result = parseDeploymentConfig(kOnPremiseYaml);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->redis.host, "redis.local");
    EXPECT_EQ(result->redis.port, 6380);
    EXPECT_EQ(result->redis.database, 2u);
    EXPECT_EQ(result->redis.maxMemoryMb, 512u);
}

TEST(DeploymentConfigTest, ParsePostgresSection) {
    auto result = parseDeploymentConfig(kOnPremiseYaml);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->postgres.host, "pg.local");
    EXPECT_EQ(result->postgres.port, 5433);
    EXPECT_EQ(result->postgres.database, "test_db");
    EXPECT_EQ(result->postgres.user, "test_user");
    EXPECT_EQ(result->postgres.sslMode, "prefer");
}

TEST(DeploymentConfigTest, ParseTlsSection) {
    auto result = parseDeploymentConfig(kOnPremiseYaml);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->tls.certPath, "/etc/ssl/cert.pem");
    EXPECT_EQ(result->tls.keyPath, "/etc/ssl/key.pem");
    EXPECT_EQ(result->tls.caPath, "/etc/ssl/ca.pem");
}

TEST(DeploymentConfigTest, ParseStorageFilesystem) {
    auto result = parseDeploymentConfig(kOnPremiseYaml);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->storage.type, "filesystem");
    EXPECT_EQ(result->storage.basePath, "/mnt/dicom");
}

TEST(DeploymentConfigTest, ParseStorageS3) {
    auto result = parseDeploymentConfig(kCloudYaml);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->storage.type, "s3");
    EXPECT_EQ(result->storage.s3Bucket, "my-dicom-bucket");
    EXPECT_EQ(result->storage.s3Region, "us-west-2");
}

TEST(DeploymentConfigTest, ParseAuditCloudWatch) {
    auto result = parseDeploymentConfig(kCloudYaml);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->audit.sinkType, "cloudwatch");
    EXPECT_EQ(result->audit.cloudwatchGroup, "/dicom/audit");
    EXPECT_EQ(result->audit.cloudwatchStream, "main");
}

// --- Default values ---

TEST(DeploymentConfigTest, MissingSectionUsesDefaults) {
    const char* minimal = "server:\n  rest_port: 3000\n";
    auto result = parseDeploymentConfig(minimal);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->server.restPort, 3000);
    // Missing sections should use defaults
    EXPECT_EQ(result->redis.port, 6379);
    EXPECT_EQ(result->postgres.port, 5432);
    EXPECT_EQ(result->storage.type, "filesystem");
}

// --- Error cases ---

TEST(DeploymentConfigTest, EmptyContentReturnsError) {
    auto result = parseDeploymentConfig("");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DeploymentConfigError::ParseError);
}

TEST(DeploymentConfigTest, FileNotFoundReturnsError) {
    auto result = loadDeploymentConfig("/nonexistent/path/deployment.yaml");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DeploymentConfigError::FileNotFound);
}

// --- Environment variable resolution ---

TEST(DeploymentConfigTest, EnvironmentVariableResolution) {
    ::setenv("TEST_REDIS_PW_508", "resolved_value", 1);

    const char* yaml = R"(
redis:
  host: localhost
  password_env: TEST_REDIS_PW_508
)";

    auto result = parseDeploymentConfig(yaml);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->redis.password, "resolved_value");

    ::unsetenv("TEST_REDIS_PW_508");
}

TEST(DeploymentConfigTest, MissingEnvVarReturnsEmpty) {
    ::unsetenv("NONEXISTENT_VAR_508");

    const char* yaml = R"(
redis:
  host: localhost
  password_env: NONEXISTENT_VAR_508
)";

    auto result = parseDeploymentConfig(yaml);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->redis.password.empty());
}

// --- Comments handling ---

TEST(DeploymentConfigTest, CommentsIgnored) {
    const char* yaml = R"(
# Top-level comment
server:
  # Port comment
  rest_port: 4000
  ws_port: 4001  # Inline comments not supported in this parser
)";

    auto result = parseDeploymentConfig(yaml);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->server.restPort, 4000);
}

// --- Error string ---

TEST(DeploymentConfigTest, ErrorToString) {
    EXPECT_STREQ(toString(DeploymentConfigError::FileNotFound),
                 "deployment.yaml file not found");
    EXPECT_STREQ(toString(DeploymentConfigError::ParseError),
                 "Failed to parse deployment.yaml");
}

} // namespace

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

/**
 * @file deployment_config.hpp
 * @brief Deployment configuration loader for zero-code environment switching
 * @details Loads a 2-level YAML `deployment.yaml` and provides typed access
 *          to all configuration sections. Environment variable references
 *          (fields ending in `_env`) are resolved at load time.
 *
 * ## Supported Sections
 * - `server`: REST and WebSocket port, log level, max sessions
 * - `idp`: Identity provider type (ldap or cognito) — delegated to AuthProviderFactory
 * - `redis`: Session store connection
 * - `postgres`: Audit log connection
 * - `tls`: Certificate paths for nginx
 * - `storage`: DICOM storage backend (filesystem or s3)
 * - `audit`: Audit sink configuration
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Section structs
// ---------------------------------------------------------------------------

struct ServerConfig {
    uint16_t restPort = 8080;
    uint16_t wsPort = 8081;
    std::string logLevel = "info";
    uint32_t maxSessions = 8;
};

struct RedisDeployConfig {
    std::string host;
    uint16_t port = 6379;
    std::string password;
    uint32_t database = 0;
    uint32_t maxMemoryMb = 256;
};

struct PostgresDeployConfig {
    std::string host;
    uint16_t port = 5432;
    std::string database = "dicom_viewer";
    std::string user = "dicom_viewer";
    std::string password;
    std::string sslMode = "require";
};

struct TlsConfig {
    std::string certPath;
    std::string keyPath;
    std::string caPath;
};

struct StorageConfig {
    std::string type = "filesystem";   // filesystem | s3
    std::string basePath = "/data/dicom";
    std::string s3Bucket;
    std::string s3Region;
};

struct AuditDeployConfig {
    std::string sinkType = "postgresql";  // postgresql | cloudwatch | file
    std::string filePath = "/var/log/dicom_viewer/audit.log";
    std::string cloudwatchGroup;
    std::string cloudwatchStream;
};

// ---------------------------------------------------------------------------
// Top-level deployment config
// ---------------------------------------------------------------------------

struct DeploymentConfig {
    ServerConfig server;
    RedisDeployConfig redis;
    PostgresDeployConfig postgres;
    TlsConfig tls;
    StorageConfig storage;
    AuditDeployConfig audit;
    std::string idpConfigPath;  // Path passed to AuthProviderFactory
};

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------

enum class DeploymentConfigError : uint8_t {
    FileNotFound,
    ParseError,
    MissingRequiredField,
};

/**
 * @brief Human-readable error description
 */
[[nodiscard]] const char* toString(DeploymentConfigError err);

// ---------------------------------------------------------------------------
// Loader functions
// ---------------------------------------------------------------------------

/**
 * @brief Load deployment config from a YAML file
 * @param path Path to deployment.yaml
 * @return Populated config or error
 */
[[nodiscard]] std::expected<DeploymentConfig, DeploymentConfigError>
loadDeploymentConfig(const std::string& path);

/**
 * @brief Load deployment config from an in-memory YAML string
 * @param content YAML content
 * @return Populated config or error
 */
[[nodiscard]] std::expected<DeploymentConfig, DeploymentConfigError>
parseDeploymentConfig(const std::string& content);

} // namespace dicom_viewer::services

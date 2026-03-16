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

#include "services/config/deployment_config.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// Minimal 2-level YAML parser (reuses pattern from AuthProviderFactory)
// ---------------------------------------------------------------------------

namespace {

using YamlSection = std::unordered_map<std::string, std::string>;
using YamlDocument = std::unordered_map<std::string, YamlSection>;

std::string trim(std::string_view sv)
{
    const auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    const auto end = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(start, end - start + 1));
}

YamlDocument parseYaml(const std::string& content)
{
    YamlDocument doc;
    std::istringstream stream(content);
    std::string line;
    std::string section;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Skip comments
        const auto firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos && line[firstNonSpace] == '#') {
            continue;
        }

        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        const bool indented = line[0] == ' ' || line[0] == '\t';

        if (!indented) {
            section = key;
            doc[section];
        } else if (!section.empty()) {
            doc[section][key] = value;
        }
    }

    return doc;
}

/// Resolve _env suffix: if key ends with _env, read the environment variable
std::string resolveEnv(const std::string& value)
{
    if (value.empty()) return value;
    const char* env = std::getenv(value.c_str());
    return env ? env : "";
}

std::string get(const YamlSection& s, const std::string& key,
                const std::string& fallback = "")
{
    auto it = s.find(key);
    if (it != s.end() && !it->second.empty()) return it->second;

    // Check _env variant
    auto envIt = s.find(key + "_env");
    if (envIt != s.end() && !envIt->second.empty()) {
        return resolveEnv(envIt->second);
    }

    return fallback;
}

uint16_t getU16(const YamlSection& s, const std::string& key, uint16_t fallback)
{
    auto v = get(s, key);
    if (v.empty()) return fallback;
    try { return static_cast<uint16_t>(std::stoi(v)); }
    catch (...) { return fallback; }
}

uint32_t getU32(const YamlSection& s, const std::string& key, uint32_t fallback)
{
    auto v = get(s, key);
    if (v.empty()) return fallback;
    try { return static_cast<uint32_t>(std::stoul(v)); }
    catch (...) { return fallback; }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Error string
// ---------------------------------------------------------------------------

const char* toString(DeploymentConfigError err)
{
    switch (err) {
        case DeploymentConfigError::FileNotFound:
            return "deployment.yaml file not found";
        case DeploymentConfigError::ParseError:
            return "Failed to parse deployment.yaml";
        case DeploymentConfigError::MissingRequiredField:
            return "Required configuration field is missing";
    }
    return "Unknown error";
}

// ---------------------------------------------------------------------------
// Loader
// ---------------------------------------------------------------------------

std::expected<DeploymentConfig, DeploymentConfigError>
loadDeploymentConfig(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("DeploymentConfig: cannot open '{}'", path);
        return std::unexpected(DeploymentConfigError::FileNotFound);
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    auto result = parseDeploymentConfig(ss.str());
    if (result) {
        result->idpConfigPath = path;
        spdlog::info("DeploymentConfig: loaded from '{}'", path);
    }
    return result;
}

std::expected<DeploymentConfig, DeploymentConfigError>
parseDeploymentConfig(const std::string& content)
{
    auto doc = parseYaml(content);
    if (doc.empty()) {
        return std::unexpected(DeploymentConfigError::ParseError);
    }

    DeploymentConfig cfg;

    // --- server ---
    if (auto it = doc.find("server"); it != doc.end()) {
        const auto& s = it->second;
        cfg.server.restPort = getU16(s, "rest_port", 8080);
        cfg.server.wsPort = getU16(s, "ws_port", 8081);
        cfg.server.logLevel = get(s, "log_level", "info");
        cfg.server.maxSessions = getU32(s, "max_sessions", 8);
    }

    // --- redis ---
    if (auto it = doc.find("redis"); it != doc.end()) {
        const auto& s = it->second;
        cfg.redis.host = get(s, "host");
        cfg.redis.port = getU16(s, "port", 6379);
        cfg.redis.password = get(s, "password");
        cfg.redis.database = getU32(s, "database", 0);
        cfg.redis.maxMemoryMb = getU32(s, "max_memory_mb", 256);
    }

    // --- postgres ---
    if (auto it = doc.find("postgres"); it != doc.end()) {
        const auto& s = it->second;
        cfg.postgres.host = get(s, "host");
        cfg.postgres.port = getU16(s, "port", 5432);
        cfg.postgres.database = get(s, "database", "dicom_viewer");
        cfg.postgres.user = get(s, "user", "dicom_viewer");
        cfg.postgres.password = get(s, "password");
        cfg.postgres.sslMode = get(s, "ssl_mode", "require");
    }

    // --- tls ---
    if (auto it = doc.find("tls"); it != doc.end()) {
        const auto& s = it->second;
        cfg.tls.certPath = get(s, "cert_path");
        cfg.tls.keyPath = get(s, "key_path");
        cfg.tls.caPath = get(s, "ca_path");
    }

    // --- storage ---
    if (auto it = doc.find("storage"); it != doc.end()) {
        const auto& s = it->second;
        cfg.storage.type = get(s, "type", "filesystem");
        cfg.storage.basePath = get(s, "base_path", "/data/dicom");
        cfg.storage.s3Bucket = get(s, "s3_bucket");
        cfg.storage.s3Region = get(s, "s3_region");
    }

    // --- audit ---
    if (auto it = doc.find("audit"); it != doc.end()) {
        const auto& s = it->second;
        cfg.audit.sinkType = get(s, "sink_type", "postgresql");
        cfg.audit.filePath = get(s, "file_path", "/var/log/dicom_viewer/audit.log");
        cfg.audit.cloudwatchGroup = get(s, "cloudwatch_group");
        cfg.audit.cloudwatchStream = get(s, "cloudwatch_stream");
    }

    return cfg;
}

} // namespace dicom_viewer::services

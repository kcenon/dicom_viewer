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

#include "services/auth/auth_provider_factory.hpp"

#include "services/auth/ldap_auth_provider.hpp"
#include "services/auth/oidc_auth_provider.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

namespace dicom_viewer::services {

namespace {

// ---------------------------------------------------------------------------
// Minimal YAML parser (flat 2-level: section → key: value)
// ---------------------------------------------------------------------------

using YamlSection = std::unordered_map<std::string, std::string>;
using YamlDocument = std::unordered_map<std::string, YamlSection>;

std::string trimString(std::string_view sv)
{
    const auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(start, end - start + 1));
}

bool isComment(const std::string& line)
{
    const auto pos = line.find_first_not_of(" \t");
    return pos != std::string::npos && line[pos] == '#';
}

// Parse simple 2-level YAML:
//   section:
//     key: value
//   other_section:
//     key2: value2
YamlDocument parseSimpleYaml(const std::string& content)
{
    YamlDocument doc;
    std::istringstream stream(content);
    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        if (line.empty() || isComment(line)) {
            continue;
        }

        // Determine indentation level
        const bool isIndented = !line.empty() && (line[0] == ' ' || line[0] == '\t');

        const auto colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        const std::string key = trimString(line.substr(0, colonPos));
        const std::string value = trimString(line.substr(colonPos + 1));

        if (!isIndented) {
            // Section header (top-level key)
            currentSection = key;
            doc[currentSection]; // ensure section exists
        } else if (!currentSection.empty()) {
            // Key-value pair under current section
            doc[currentSection][key] = value;
        }
    }

    return doc;
}

// ---------------------------------------------------------------------------
// Secret resolution
// ---------------------------------------------------------------------------

// If fieldName ends with "_env", treat its value as an env var name and resolve it.
// Otherwise return the value as-is.
std::string resolveSecret(const std::string& fieldName, const std::string& value)
{
    if (fieldName.size() >= 4 && fieldName.substr(fieldName.size() - 4) == "_env") {
        if (value.empty()) {
            return {};
        }
        const char* envValue = std::getenv(value.c_str());
        return envValue ? std::string(envValue) : std::string{};
    }
    return value;
}

std::string getField(const YamlSection& section, const std::string& key,
                     const std::string& defaultValue = {})
{
    const auto it = section.find(key);
    if (it == section.end()) {
        return defaultValue;
    }
    return it->second;
}

// ---------------------------------------------------------------------------
// Role map loading
// ---------------------------------------------------------------------------

std::expected<std::map<std::string, std::string>, AuthFactoryError>
loadRoleMap(const std::string& roleMapPath)
{
    if (roleMapPath.empty()) {
        return std::map<std::string, std::string>{};
    }

    if (!std::filesystem::exists(roleMapPath)) {
        return std::unexpected(AuthFactoryError::RoleMapLoadError);
    }

    std::ifstream f(roleMapPath);
    if (!f) {
        return std::unexpected(AuthFactoryError::RoleMapLoadError);
    }

    try {
        auto j = nlohmann::json::parse(f);
        std::map<std::string, std::string> result;
        for (const auto& [group, role] : j.items()) {
            result[group] = role.get<std::string>();
        }
        return result;
    } catch (...) {
        return std::unexpected(AuthFactoryError::RoleMapLoadError);
    }
}

// ---------------------------------------------------------------------------
// Provider builders
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<AuthProvider>, AuthFactoryError>
buildLdapProvider(const YamlSection& idp)
{
    LdapAuthConfig config;

    config.ldapUrl = getField(idp, "ldap_url");
    if (config.ldapUrl.empty()) {
        return std::unexpected(AuthFactoryError::MissingRequiredField);
    }

    config.bindDn = getField(idp, "bind_dn");
    if (config.bindDn.empty()) {
        return std::unexpected(AuthFactoryError::MissingRequiredField);
    }

    // Resolve service account password (prefer env var)
    const std::string bindPwEnv = getField(idp, "bind_password_env");
    if (!bindPwEnv.empty()) {
        const char* env = std::getenv(bindPwEnv.c_str());
        config.bindPassword = env ? std::string(env) : std::string{};
    } else {
        config.bindPassword = getField(idp, "bind_password");
    }

    config.baseDn = getField(idp, "base_dn");
    if (config.baseDn.empty()) {
        return std::unexpected(AuthFactoryError::MissingRequiredField);
    }

    config.groupBaseDn = getField(idp, "group_base_dn", config.baseDn);

    const std::string groupAttr = getField(idp, "group_attribute");
    if (!groupAttr.empty()) {
        config.groupAttribute = groupAttr;
    }

    const std::string userFilter = getField(idp, "user_search_filter");
    if (!userFilter.empty()) {
        config.userSearchFilter = userFilter;
    }

    // Load role map
    const std::string roleMapPath = getField(idp, "role_map_path");
    auto roleMapResult = loadRoleMap(roleMapPath);
    if (!roleMapResult) {
        return std::unexpected(roleMapResult.error());
    }
    config.groupRoleMap = std::move(*roleMapResult);

    config.defaultRole    = getField(idp, "default_role", "Viewer");
    config.issuer         = getField(idp, "issuer", "dicom-viewer");
    config.audience       = getField(idp, "audience", "dicom-viewer-api");
    config.organization   = getField(idp, "organization", "hospital");
    config.privateKeyPath = getField(idp, "private_key_path");
    config.publicKeyPath  = getField(idp, "public_key_path");
    config.caCertPath     = getField(idp, "ca_cert_path");

    const std::string maxSessions = getField(idp, "max_concurrent_sessions");
    if (!maxSessions.empty()) {
        try { config.maxConcurrentSessions = std::stoi(maxSessions); }
        catch (...) { /* keep default */ }
    }

    const std::string connTimeout = getField(idp, "connection_timeout_ms");
    if (!connTimeout.empty()) {
        try { config.connectionTimeoutMs = std::stoi(connTimeout); }
        catch (...) {}
    }

    const std::string verifyCert = getField(idp, "verify_server_cert", "true");
    config.verifyServerCert = (verifyCert != "false" && verifyCert != "0");

    return std::make_unique<LdapAuthProvider>(config);
}

std::expected<std::unique_ptr<AuthProvider>, AuthFactoryError>
buildOidcProvider(const YamlSection& idp)
{
    OidcAuthConfig config;

    config.discoveryUrl = getField(idp, "discovery_url");
    config.jwksUrl      = getField(idp, "jwks_url");
    config.clientId     = getField(idp, "client_id");

    // Resolve client secret via env var if specified
    const std::string clientSecretEnv = getField(idp, "client_secret_env");
    if (!clientSecretEnv.empty()) {
        const char* env = std::getenv(clientSecretEnv.c_str());
        config.clientSecret = env ? std::string(env) : std::string{};
    } else {
        config.clientSecret = getField(idp, "client_secret");
    }

    config.region         = getField(idp, "region");
    config.issuer         = getField(idp, "issuer");
    config.audience       = getField(idp, "audience");
    config.roleClaimName  = getField(idp, "role_claim_name", "custom:role");
    config.orgClaimName   = getField(idp, "org_claim_name", "custom:organization");
    config.defaultRole    = getField(idp, "default_role", "Viewer");

    const std::string maxSessions = getField(idp, "max_concurrent_sessions");
    if (!maxSessions.empty()) {
        try { config.maxConcurrentSessions = std::stoi(maxSessions); }
        catch (...) {}
    }

    return std::make_unique<OidcAuthProvider>(config);
}

// ---------------------------------------------------------------------------
// Core factory logic
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<AuthProvider>, AuthFactoryError>
buildFromParsedYaml(const YamlDocument& doc)
{
    const auto idpIt = doc.find("idp");
    if (idpIt == doc.end()) {
        return std::unexpected(AuthFactoryError::MissingRequiredField);
    }

    const YamlSection& idp = idpIt->second;

    const std::string type = getField(idp, "type");
    if (type.empty()) {
        return std::unexpected(AuthFactoryError::MissingRequiredField);
    }

    if (type == "ldap" || type == "ad") {
        return buildLdapProvider(idp);
    }

    if (type == "cognito" || type == "oidc") {
        return buildOidcProvider(idp);
    }

    return std::unexpected(AuthFactoryError::UnknownProviderType);
}

} // namespace

// ---------------------------------------------------------------------------
// AuthProviderFactory public API
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<AuthProvider>, AuthFactoryError>
AuthProviderFactory::fromDeploymentYaml(const std::string& deploymentYamlPath)
{
    if (!std::filesystem::exists(deploymentYamlPath)) {
        return std::unexpected(AuthFactoryError::FileNotFound);
    }

    std::ifstream f(deploymentYamlPath);
    if (!f) {
        return std::unexpected(AuthFactoryError::FileNotFound);
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    return fromYamlString(ss.str());
}

std::expected<std::unique_ptr<AuthProvider>, AuthFactoryError>
AuthProviderFactory::fromYamlString(const std::string& yamlContent)
{
    if (yamlContent.empty()) {
        return std::unexpected(AuthFactoryError::ParseError);
    }

    const YamlDocument doc = parseSimpleYaml(yamlContent);
    if (doc.empty()) {
        return std::unexpected(AuthFactoryError::ParseError);
    }

    return buildFromParsedYaml(doc);
}

} // namespace dicom_viewer::services

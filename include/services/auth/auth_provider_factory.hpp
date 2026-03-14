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

/**
 * @file auth_provider_factory.hpp
 * @brief Factory that creates an AuthProvider from deployment.yaml
 * @details Reads the `idp` section of deployment.yaml and instantiates the
 *          appropriate AuthProvider implementation:
 *          - `type: ldap`    → LdapAuthProvider
 *          - `type: cognito` → OidcAuthProvider
 *
 * ## deployment.yaml IDP section example
 * @code
 * idp:
 *   type: ldap
 *   ldap_url: ldaps://ad.hospital.local:636
 *   bind_dn: CN=steamliner-svc,OU=Services,DC=hospital,DC=local
 *   bind_password_env: IDP_BIND_PASSWORD
 *   base_dn: DC=hospital,DC=local
 *   group_base_dn: OU=Groups,DC=hospital,DC=local
 *   group_attribute: member
 *   role_map_path: /etc/steamliner/role_map.json
 *   private_key_path: /etc/steamliner/jwt.key
 *   ca_cert_path: /etc/ssl/certs/hospital-ca.crt
 *   max_concurrent_sessions: 3
 * @endcode
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "services/auth/auth_provider.hpp"

#include <expected>
#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Error returned when the factory cannot create a provider
 */
enum class AuthFactoryError : uint8_t {
    FileNotFound,          ///< deployment.yaml does not exist at the given path
    ParseError,            ///< YAML structure is invalid or required keys are missing
    UnknownProviderType,   ///< `idp.type` value is not recognized
    MissingRequiredField,  ///< A required configuration field is absent
    RoleMapLoadError,      ///< role_map_path file could not be loaded or parsed
};

/**
 * @brief Creates an AuthProvider from a deployment.yaml configuration file
 *
 * The factory reads the `idp` section of deployment.yaml, loads any referenced
 * external files (role map JSON, TLS certificates), resolves environment-variable
 * references for secrets, and constructs the appropriate provider.
 *
 * ## Secret Resolution
 * Fields ending in `_env` are treated as environment variable names:
 * - `bind_password_env: IDP_BIND_PASSWORD` → reads `$IDP_BIND_PASSWORD`
 *
 * @trace SRS-FR-AUTH-004
 */
class AuthProviderFactory {
public:
    /**
     * @brief Create an AuthProvider from a deployment.yaml file
     * @param deploymentYamlPath Absolute or relative path to deployment.yaml
     * @return Unique_ptr to concrete AuthProvider on success, AuthFactoryError on failure
     */
    [[nodiscard]] static std::expected<std::unique_ptr<AuthProvider>, AuthFactoryError>
    fromDeploymentYaml(const std::string& deploymentYamlPath);

    /**
     * @brief Create an AuthProvider from an in-memory YAML string
     *
     * Useful for testing and environments where the config is injected
     * rather than read from disk.
     *
     * @param yamlContent YAML content string (must contain an `idp:` section)
     * @return Unique_ptr to concrete AuthProvider on success, AuthFactoryError on failure
     */
    [[nodiscard]] static std::expected<std::unique_ptr<AuthProvider>, AuthFactoryError>
    fromYamlString(const std::string& yamlContent);
};

} // namespace dicom_viewer::services

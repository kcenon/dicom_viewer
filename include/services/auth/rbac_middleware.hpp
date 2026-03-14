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
 * @file rbac_middleware.hpp
 * @brief Role-based access control (RBAC) for the headless server REST API
 * @details Provides the `Role` enum, `RbacChecker` pure-logic class, and
 *          per-endpoint permission constants.
 *
 * ## Role Hierarchy (ascending privilege)
 * | Role       | Value | Session | DICOM View   | Analysis | Report | Export | Admin |
 * |------------|-------|---------|-------------|----------|--------|--------|-------|
 * | Viewer     |  0    | No      | Read-only   | No       | No     | No     | No    |
 * | Researcher |  1    | No      | De-id only  | Yes      | No     | No     | No    |
 * | Clinician  |  2    | Yes     | Org-scoped  | Yes      | Yes    | Yes    | No    |
 * | Admin      |  3    | Yes     | All         | Yes      | Yes    | Yes    | Yes   |
 *
 * ## Usage in Route Handlers
 * ```cpp
 * CROW_ROUTE(app, "/api/v1/sessions")
 *     .methods(crow::HTTPMethod::Post)(
 *         [](const crow::request& req, crow::response& res) {
 *             auto& jwtCtx = app.get_context<JwtMiddleware>(req);
 *             if (!RbacChecker::check(jwtCtx, Role::Clinician, res)) return;
 *             // ... handler body
 *         });
 * ```
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief RBAC role enum with ordinal ordering (higher = more privilege)
 * @details Comparisons use the underlying integer value so that
 *          `role >= Role::Clinician` expresses "Clinician or higher".
 */
enum class Role : uint8_t {
    Viewer     = 0, ///< Read-only access to anonymized study lists
    Researcher = 1, ///< Analytical access to de-identified datasets
    Clinician  = 2, ///< Full clinical access within own organization
    Admin      = 3, ///< Unrestricted access across all organizations
};

/**
 * @brief Endpoint category constants for role requirements
 */
struct RequiredRole {
    static constexpr Role kPublic        = Role::Viewer;     ///< No auth needed (health check)
    static constexpr Role kViewOnly      = Role::Viewer;     ///< Read-only (study list, measurements)
    static constexpr Role kAnalysis      = Role::Researcher; ///< Analytical operations
    static constexpr Role kClinical      = Role::Clinician;  ///< Session create, upload, PACS, export
    static constexpr Role kAdmin         = Role::Admin;      ///< User management
};

/**
 * @brief Pure role-based access control logic (no Crow dependency)
 *
 * All methods are static so `RbacChecker` acts as a namespace for
 * RBAC policy functions that can be tested independently of the HTTP layer.
 *
 * @trace SRS-FR-AUTH-003
 */
class RbacChecker {
public:
    /**
     * @brief Parse a role string into a Role enum
     * @param roleStr Role string ("Viewer", "Researcher", "Clinician", "Admin")
     * @return Parsed Role, defaults to Role::Viewer for unknown strings
     */
    [[nodiscard]] static Role fromString(const std::string& roleStr);

    /**
     * @brief Convert a Role enum to its string representation
     */
    [[nodiscard]] static const char* toString(Role role);

    /**
     * @brief Check if a user role meets the minimum required role
     * @param userRole The authenticated user's role
     * @param required Minimum required role for the operation
     * @return true if userRole >= required
     */
    [[nodiscard]] static bool hasMinimumRole(Role userRole, Role required);

    /**
     * @brief Check organization-scoped resource access
     * @details Admins bypass org checks. All other roles are limited to
     *          resources within their own organization. A null/empty
     *          resourceOrg implies an org-neutral resource accessible to all.
     *
     * @param userRole Authenticated user's role
     * @param userOrg User's organization claim from JWT
     * @param resourceOrg Organization that owns the resource (empty = global)
     * @return true if access is permitted
     */
    [[nodiscard]] static bool isOrgAccessible(Role userRole,
                                               const std::string& userOrg,
                                               const std::string& resourceOrg);

    /**
     * @brief Check if a researcher can access a dataset
     * @details Researchers may only access de-identified (anonymized) datasets.
     *          The anonymization status is determined by the caller.
     *
     * @param userRole Authenticated user's role
     * @param isDeidentified Whether the dataset has been de-identified
     * @return true if the role is permitted to access the dataset
     */
    [[nodiscard]] static bool canAccessDataset(Role userRole, bool isDeidentified);
};

} // namespace dicom_viewer::services

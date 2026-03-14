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

#include "services/auth/rbac_middleware.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace dicom_viewer::services {

Role RbacChecker::fromString(const std::string& roleStr) {
    // Case-insensitive comparison
    std::string lower = roleStr;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "admin")      return Role::Admin;
    if (lower == "clinician")  return Role::Clinician;
    if (lower == "researcher") return Role::Researcher;
    // "viewer" and all unknown strings map to the most restrictive role
    return Role::Viewer;
}

const char* RbacChecker::toString(Role role) {
    switch (role) {
        case Role::Admin:      return "Admin";
        case Role::Clinician:  return "Clinician";
        case Role::Researcher: return "Researcher";
        case Role::Viewer:     return "Viewer";
    }
    return "Viewer";
}

bool RbacChecker::hasMinimumRole(Role userRole, Role required) {
    return static_cast<uint8_t>(userRole) >= static_cast<uint8_t>(required);
}

bool RbacChecker::isOrgAccessible(Role userRole,
                                   const std::string& userOrg,
                                   const std::string& resourceOrg) {
    // Admins bypass organization scoping
    if (userRole == Role::Admin) return true;

    // Resources with no organization are globally accessible
    if (resourceOrg.empty()) return true;

    // All other roles are restricted to their own organization
    return userOrg == resourceOrg;
}

bool RbacChecker::canAccessDataset(Role userRole, bool isDeidentified) {
    if (userRole == Role::Admin || userRole == Role::Clinician) {
        return true;  // Full access regardless of de-identification status
    }
    if (userRole == Role::Researcher) {
        return isDeidentified;  // Researchers may only access de-identified data
    }
    // Viewer: read-only, allowed only if de-identified
    return isDeidentified;
}

} // namespace dicom_viewer::services

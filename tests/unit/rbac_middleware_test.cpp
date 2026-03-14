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

#include "services/auth/rbac_middleware.hpp"

using namespace dicom_viewer::services;

// ============================================================================
// Role::fromString
// ============================================================================

TEST(RbacFromString, KnownRolesCaseInsensitive) {
    EXPECT_EQ(RbacChecker::fromString("Admin"),      Role::Admin);
    EXPECT_EQ(RbacChecker::fromString("admin"),      Role::Admin);
    EXPECT_EQ(RbacChecker::fromString("ADMIN"),      Role::Admin);
    EXPECT_EQ(RbacChecker::fromString("Clinician"),  Role::Clinician);
    EXPECT_EQ(RbacChecker::fromString("clinician"),  Role::Clinician);
    EXPECT_EQ(RbacChecker::fromString("Researcher"), Role::Researcher);
    EXPECT_EQ(RbacChecker::fromString("researcher"), Role::Researcher);
    EXPECT_EQ(RbacChecker::fromString("Viewer"),     Role::Viewer);
    EXPECT_EQ(RbacChecker::fromString("viewer"),     Role::Viewer);
}

TEST(RbacFromString, UnknownStringDefaultsToViewer) {
    EXPECT_EQ(RbacChecker::fromString(""),           Role::Viewer);
    EXPECT_EQ(RbacChecker::fromString("guest"),      Role::Viewer);
    EXPECT_EQ(RbacChecker::fromString("superuser"),  Role::Viewer);
    EXPECT_EQ(RbacChecker::fromString("radiologist"), Role::Viewer);
}

// ============================================================================
// Role::toString
// ============================================================================

TEST(RbacToString, AllRoles) {
    EXPECT_STREQ(RbacChecker::toString(Role::Admin),      "Admin");
    EXPECT_STREQ(RbacChecker::toString(Role::Clinician),  "Clinician");
    EXPECT_STREQ(RbacChecker::toString(Role::Researcher), "Researcher");
    EXPECT_STREQ(RbacChecker::toString(Role::Viewer),     "Viewer");
}

// ============================================================================
// hasMinimumRole — role hierarchy enforcement
// ============================================================================

class RbacHierarchyTest : public ::testing::Test {};

TEST_F(RbacHierarchyTest, AdminMeetsAllRequirements) {
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Admin, Role::Admin));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Admin, Role::Clinician));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Admin, Role::Researcher));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Admin, Role::Viewer));
}

TEST_F(RbacHierarchyTest, ClinicianMeetsClinicianAndBelow) {
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Clinician, Role::Admin));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Clinician, Role::Clinician));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Clinician, Role::Researcher));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Clinician, Role::Viewer));
}

TEST_F(RbacHierarchyTest, ResearcherMeetsResearcherAndViewer) {
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Researcher, Role::Admin));
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Researcher, Role::Clinician));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Researcher, Role::Researcher));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Researcher, Role::Viewer));
}

TEST_F(RbacHierarchyTest, ViewerMeetsOnlyViewer) {
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Viewer, Role::Admin));
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Viewer, Role::Clinician));
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Viewer, Role::Researcher));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Viewer, Role::Viewer));
}

// ============================================================================
// isOrgAccessible — organization scoping
// ============================================================================

class RbacOrgScopingTest : public ::testing::Test {
protected:
    const std::string kOrgA = "hospital-a";
    const std::string kOrgB = "hospital-b";
    const std::string kNoOrg = "";
};

TEST_F(RbacOrgScopingTest, AdminBypassesOrgScoping) {
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Admin, kOrgA, kOrgB));
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Admin, kOrgA, kOrgA));
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Admin, kOrgA, kNoOrg));
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Admin, kNoOrg, kOrgB));
}

TEST_F(RbacOrgScopingTest, ClinicianRestrictedToOwnOrg) {
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Clinician, kOrgA, kOrgA));
    EXPECT_FALSE(RbacChecker::isOrgAccessible(Role::Clinician, kOrgA, kOrgB));
}

TEST_F(RbacOrgScopingTest, ResearcherRestrictedToOwnOrg) {
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Researcher, kOrgA, kOrgA));
    EXPECT_FALSE(RbacChecker::isOrgAccessible(Role::Researcher, kOrgA, kOrgB));
}

TEST_F(RbacOrgScopingTest, ViewerRestrictedToOwnOrg) {
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Viewer, kOrgA, kOrgA));
    EXPECT_FALSE(RbacChecker::isOrgAccessible(Role::Viewer, kOrgA, kOrgB));
}

TEST_F(RbacOrgScopingTest, EmptyResourceOrgIsGloballyAccessible) {
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Clinician, kOrgA, kNoOrg));
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Researcher, kOrgA, kNoOrg));
    EXPECT_TRUE(RbacChecker::isOrgAccessible(Role::Viewer, kOrgA, kNoOrg));
}

// ============================================================================
// canAccessDataset — de-identification enforcement
// ============================================================================

TEST(RbacDatasetAccess, AdminAccessAllDatasets) {
    EXPECT_TRUE(RbacChecker::canAccessDataset(Role::Admin, true));
    EXPECT_TRUE(RbacChecker::canAccessDataset(Role::Admin, false));
}

TEST(RbacDatasetAccess, ClinicianAccessAllDatasets) {
    EXPECT_TRUE(RbacChecker::canAccessDataset(Role::Clinician, true));
    EXPECT_TRUE(RbacChecker::canAccessDataset(Role::Clinician, false));
}

TEST(RbacDatasetAccess, ResearcherOnlyDeidentified) {
    EXPECT_TRUE(RbacChecker::canAccessDataset(Role::Researcher, true));
    EXPECT_FALSE(RbacChecker::canAccessDataset(Role::Researcher, false));
}

TEST(RbacDatasetAccess, ViewerOnlyDeidentified) {
    EXPECT_TRUE(RbacChecker::canAccessDataset(Role::Viewer, true));
    EXPECT_FALSE(RbacChecker::canAccessDataset(Role::Viewer, false));
}

// ============================================================================
// RequiredRole constants — endpoint permission matrix
// ============================================================================

TEST(RbacRequiredRoles, PermissionMatrix) {
    // Viewer should meet kPublic and kViewOnly
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Viewer, RequiredRole::kPublic));
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Viewer, RequiredRole::kViewOnly));
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Viewer, RequiredRole::kAnalysis));
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Viewer, RequiredRole::kClinical));
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Viewer, RequiredRole::kAdmin));

    // Researcher cannot create sessions or export
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Researcher, RequiredRole::kClinical));

    // Clinician can do clinical but not admin
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Clinician, RequiredRole::kClinical));
    EXPECT_FALSE(RbacChecker::hasMinimumRole(Role::Clinician, RequiredRole::kAdmin));

    // Admin can do everything
    EXPECT_TRUE(RbacChecker::hasMinimumRole(Role::Admin, RequiredRole::kAdmin));
}

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

#include <services/audit_service.hpp>

#include <pacs/security/atna_audit_logger.hpp>
#include <pacs/security/atna_config.hpp>

#include <string>

namespace {

using namespace dicom_viewer::services;

class AuditServiceTest : public ::testing::Test {
protected:
    AuditService service;
};

// --- Default State ---

TEST_F(AuditServiceTest, DefaultNotEnabled) {
    EXPECT_FALSE(service.isEnabled());
}

TEST_F(AuditServiceTest, DefaultConfigDisabled) {
    auto config = service.getConfig();
    EXPECT_FALSE(config.enabled);
}

TEST_F(AuditServiceTest, DefaultStatisticsZero) {
    auto stats = service.getStatistics();
    EXPECT_EQ(stats.eventsSent, 0u);
    EXPECT_EQ(stats.eventsFailed, 0u);
}

// --- Configuration Validation ---

TEST_F(AuditServiceTest, ConfigureDisabledSucceeds) {
    AuditConfig config;
    config.enabled = false;
    auto result = service.configure(config);
    EXPECT_TRUE(result.has_value())
        << "Disabled configuration should always succeed";
}

TEST_F(AuditServiceTest, ConfigureEmptySourceIdFails) {
    AuditConfig config;
    config.enabled = true;
    config.auditSourceId = "";
    auto result = service.configure(config);
    EXPECT_FALSE(result.has_value())
        << "Empty audit source ID should fail validation";
}

TEST_F(AuditServiceTest, ConfigureEmptyHostFails) {
    AuditConfig config;
    config.enabled = true;
    config.host = "";
    auto result = service.configure(config);
    EXPECT_FALSE(result.has_value())
        << "Empty host should fail validation";
}

TEST_F(AuditServiceTest, ConfigureZeroPortFails) {
    AuditConfig config;
    config.enabled = true;
    config.port = 0;
    auto result = service.configure(config);
    EXPECT_FALSE(result.has_value())
        << "Zero port should fail validation";
}

TEST_F(AuditServiceTest, ConfigureValidUdpSucceeds) {
    AuditConfig config;
    config.enabled = true;
    config.auditSourceId = "TEST_VIEWER";
    config.host = "localhost";
    config.port = 514;
    config.protocol = AuditTransportProtocol::Udp;
    auto result = service.configure(config);
    EXPECT_TRUE(result.has_value())
        << "Valid UDP configuration should succeed";
}

TEST_F(AuditServiceTest, ConfigurePreservesSettings) {
    AuditConfig config;
    config.enabled = true;
    config.auditSourceId = "MY_SOURCE";
    config.host = "audit-server.local";
    config.port = 6514;
    config.protocol = AuditTransportProtocol::Tls;
    config.auditStorage = false;
    config.auditQuery = true;

    (void)service.configure(config);
    auto retrieved = service.getConfig();

    EXPECT_EQ(retrieved.auditSourceId, "MY_SOURCE");
    EXPECT_EQ(retrieved.host, "audit-server.local");
    EXPECT_EQ(retrieved.port, 6514);
    EXPECT_EQ(retrieved.protocol, AuditTransportProtocol::Tls);
    EXPECT_FALSE(retrieved.auditStorage);
    EXPECT_TRUE(retrieved.auditQuery);
}

// --- Enable/Disable ---

TEST_F(AuditServiceTest, EnableAfterConfigure) {
    AuditConfig config;
    config.enabled = true;
    config.host = "localhost";
    config.port = 514;
    (void)service.configure(config);

    EXPECT_TRUE(service.isEnabled());
}

TEST_F(AuditServiceTest, DisableAfterEnable) {
    AuditConfig config;
    config.enabled = true;
    config.host = "localhost";
    config.port = 514;
    (void)service.configure(config);

    service.setEnabled(false);
    EXPECT_FALSE(service.isEnabled());
}

TEST_F(AuditServiceTest, ReEnable) {
    AuditConfig config;
    config.enabled = true;
    config.host = "localhost";
    config.port = 514;
    (void)service.configure(config);

    service.setEnabled(false);
    service.setEnabled(true);
    EXPECT_TRUE(service.isEnabled());
}

// --- AuditConfig::isValid ---

TEST_F(AuditServiceTest, DisabledConfigAlwaysValid) {
    AuditConfig config;
    config.enabled = false;
    config.auditSourceId = "";
    config.host = "";
    config.port = 0;
    EXPECT_TRUE(config.isValid())
        << "Disabled config should always be valid regardless of fields";
}

TEST_F(AuditServiceTest, ValidEnabledConfig) {
    AuditConfig config;
    config.enabled = true;
    config.auditSourceId = "TEST";
    config.host = "localhost";
    config.port = 514;
    EXPECT_TRUE(config.isValid());
}

// --- Audit Methods (No-op When Disabled) ---

TEST_F(AuditServiceTest, AuditMethodsDoNotCrashWhenDisabled) {
    // Should not throw or crash when called without configuration
    EXPECT_NO_THROW(service.auditApplicationStart());
    EXPECT_NO_THROW(service.auditApplicationStop());
    EXPECT_NO_THROW(service.auditInstanceStored(
        "SRC_AE", "DST_AE", "1.2.3.4.5", "PAT001", true));
    EXPECT_NO_THROW(service.auditQuery(
        "CALLING_AE", "CALLED_AE", "STUDY", true));
    EXPECT_NO_THROW(service.auditAssociation("REMOTE_AE", true, true));
    EXPECT_NO_THROW(service.auditSecurityAlert("USER", "test alert"));
}

TEST_F(AuditServiceTest, AuditMethodsDoNotCrashWhenConfiguredButDisabled) {
    AuditConfig config;
    config.enabled = false;
    (void)service.configure(config);

    EXPECT_NO_THROW(service.auditApplicationStart());
    EXPECT_NO_THROW(service.auditInstanceStored(
        "SRC_AE", "DST_AE", "1.2.3.4.5", "PAT001", true));
}

// --- Statistics ---

TEST_F(AuditServiceTest, ResetStatisticsWhenNotConfigured) {
    EXPECT_NO_THROW(service.resetStatistics());
    auto stats = service.getStatistics();
    EXPECT_EQ(stats.eventsSent, 0u);
    EXPECT_EQ(stats.eventsFailed, 0u);
}

// --- Move Semantics ---

TEST_F(AuditServiceTest, MoveConstruction) {
    AuditConfig config;
    config.enabled = false;
    config.auditSourceId = "MOVE_TEST";
    (void)service.configure(config);

    AuditService moved(std::move(service));
    auto retrievedConfig = moved.getConfig();
    EXPECT_EQ(retrievedConfig.auditSourceId, "MOVE_TEST");
}

TEST_F(AuditServiceTest, MoveAssignment) {
    AuditConfig config;
    config.enabled = false;
    config.auditSourceId = "ASSIGN_TEST";
    (void)service.configure(config);

    AuditService other;
    other = std::move(service);
    auto retrievedConfig = other.getConfig();
    EXPECT_EQ(retrievedConfig.auditSourceId, "ASSIGN_TEST");
}

// --- Transport Protocol Enum ---

TEST_F(AuditServiceTest, TransportProtocolValues) {
    EXPECT_NE(static_cast<uint8_t>(AuditTransportProtocol::Udp),
              static_cast<uint8_t>(AuditTransportProtocol::Tls));
}

// --- pacs_system ATNA Audit Logger ---

TEST_F(AuditServiceTest, PacsAuditLoggerBuildApplicationActivity) {
    auto msg = pacs::security::atna_audit_logger::build_application_activity(
        "TEST_SOURCE", "test_app", true);
    EXPECT_FALSE(msg.active_participants.empty())
        << "Application activity message should have participants";
    EXPECT_EQ(msg.event_outcome, pacs::security::atna_event_outcome::success);
}

TEST_F(AuditServiceTest, PacsAuditLoggerToXmlNotEmpty) {
    auto msg = pacs::security::atna_audit_logger::build_application_activity(
        "TEST_SOURCE", "test_app", true);
    auto xml = pacs::security::atna_audit_logger::to_xml(msg);
    EXPECT_FALSE(xml.empty())
        << "XML serialization should produce non-empty output";
    EXPECT_NE(xml.find("AuditMessage"), std::string::npos)
        << "XML should contain AuditMessage element";
}

TEST_F(AuditServiceTest, PacsAuditLoggerBuildQuery) {
    auto msg = pacs::security::atna_audit_logger::build_query(
        "TEST_SOURCE", "CALLING_AE", "192.168.1.1", "STUDY", "PAT001");
    EXPECT_FALSE(msg.active_participants.empty());
    EXPECT_FALSE(msg.participant_objects.empty())
        << "Query message should have participant objects";
}

TEST_F(AuditServiceTest, PacsAuditLoggerBuildInstancesTransferred) {
    auto msg = pacs::security::atna_audit_logger::build_dicom_instances_transferred(
        "TEST_SOURCE", "SRC_AE", "10.0.0.1", "DST_AE", "10.0.0.2",
        "1.2.3.4.5", "PAT001", true);
    EXPECT_GE(msg.active_participants.size(), 2u)
        << "Transfer message should have source and destination participants";
}

TEST_F(AuditServiceTest, PacsAuditLoggerBuildSecurityAlert) {
    auto msg = pacs::security::atna_audit_logger::build_security_alert(
        "TEST_SOURCE", "USER01", "192.168.1.1", "Unauthorized access attempt");
    EXPECT_EQ(msg.event_outcome, pacs::security::atna_event_outcome::serious_failure);
}

// --- ATNA Config Defaults ---

TEST_F(AuditServiceTest, PacsDefaultAtnaConfig) {
    auto config = pacs::security::make_default_atna_config();
    EXPECT_FALSE(config.enabled)
        << "Default ATNA config should be disabled";
    EXPECT_FALSE(config.audit_source_id.empty());
    EXPECT_TRUE(config.audit_storage);
    EXPECT_TRUE(config.audit_query);
}

// --- Event Filtering ---

TEST_F(AuditServiceTest, EventFilteringDefaults) {
    AuditConfig config;
    EXPECT_TRUE(config.auditStorage);
    EXPECT_TRUE(config.auditQuery);
    EXPECT_TRUE(config.auditAuthentication);
    EXPECT_TRUE(config.auditSecurityAlerts);
}

} // anonymous namespace

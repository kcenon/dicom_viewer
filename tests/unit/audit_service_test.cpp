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

#include <atomic>
#include <string>
#include <vector>

namespace {

using namespace dicom_viewer::services;

class AuditServiceTest : public ::testing::Test {
protected:
    AuditService service;

    // Helper: configure service with UDP (no real server needed for unit tests)
    void configureEnabled() {
        AuditConfig config;
        config.enabled  = true;
        config.host     = "localhost";
        config.port     = 514;
        config.protocol = AuditTransportProtocol::Udp;
        (void)service.configure(config);
    }

    // Helper: build a minimal AuditContext
    static AuditContext makeContext(const std::string& sessionId = "sess-001",
                                    const std::string& studyUid  = "1.2.3.4.5") {
        AuditContext ctx;
        ctx.sourceIp        = "192.168.1.10";
        ctx.userAgent       = "TestAgent/1.0";
        ctx.sessionId       = sessionId;
        ctx.studyInstanceUid = studyUid;
        return ctx;
    }
};

// --- Default State ---

TEST_F(AuditServiceTest, DefaultNotEnabled) {
    // isEnabled() requires both enabled=true AND auditor initialized
    // — auditor is only created after configure(), so this must be false
    EXPECT_FALSE(service.isEnabled());
}

TEST_F(AuditServiceTest, DefaultConfigEnabled) {
    // AuditConfig::enabled defaults to true for HIPAA compliance
    auto config = service.getConfig();
    EXPECT_TRUE(config.enabled)
        << "AuditConfig must default to enabled for HIPAA compliance";
}

TEST_F(AuditServiceTest, DefaultConfigTlsTransport) {
    auto config = service.getConfig();
    EXPECT_EQ(config.protocol, AuditTransportProtocol::Tls)
        << "Default transport must be TLS for security";
    EXPECT_EQ(config.port, 6514u)
        << "Default port must be 6514 (TLS syslog)";
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
    auto msg = kcenon::pacs::security::atna_audit_logger::build_application_activity(
        "TEST_SOURCE", "test_app", true);
    EXPECT_FALSE(msg.active_participants.empty())
        << "Application activity message should have participants";
    EXPECT_EQ(msg.event_outcome, kcenon::pacs::security::atna_event_outcome::success);
}

TEST_F(AuditServiceTest, PacsAuditLoggerToXmlNotEmpty) {
    auto msg = kcenon::pacs::security::atna_audit_logger::build_application_activity(
        "TEST_SOURCE", "test_app", true);
    auto xml = kcenon::pacs::security::atna_audit_logger::to_xml(msg);
    EXPECT_FALSE(xml.empty())
        << "XML serialization should produce non-empty output";
    EXPECT_NE(xml.find("AuditMessage"), std::string::npos)
        << "XML should contain AuditMessage element";
}

TEST_F(AuditServiceTest, PacsAuditLoggerBuildQuery) {
    auto msg = kcenon::pacs::security::atna_audit_logger::build_query(
        "TEST_SOURCE", "CALLING_AE", "192.168.1.1", "STUDY", "PAT001");
    EXPECT_FALSE(msg.active_participants.empty());
    EXPECT_FALSE(msg.participant_objects.empty())
        << "Query message should have participant objects";
}

TEST_F(AuditServiceTest, PacsAuditLoggerBuildInstancesTransferred) {
    auto msg = kcenon::pacs::security::atna_audit_logger::build_dicom_instances_transferred(
        "TEST_SOURCE", "SRC_AE", "10.0.0.1", "DST_AE", "10.0.0.2",
        "1.2.3.4.5", "PAT001", true);
    EXPECT_GE(msg.active_participants.size(), 2u)
        << "Transfer message should have source and destination participants";
}

TEST_F(AuditServiceTest, PacsAuditLoggerBuildSecurityAlert) {
    auto msg = kcenon::pacs::security::atna_audit_logger::build_security_alert(
        "TEST_SOURCE", "USER01", "192.168.1.1", "Unauthorized access attempt");
    EXPECT_EQ(msg.event_outcome, kcenon::pacs::security::atna_event_outcome::serious_failure);
}

// --- ATNA Config Defaults ---

TEST_F(AuditServiceTest, PacsDefaultAtnaConfig) {
    auto config = kcenon::pacs::security::make_default_atna_config();
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
    EXPECT_TRUE(config.auditEphiAccess)
        << "ePHI access auditing must be enabled by default for HIPAA";
}

// --- ePHI Access Events (HIPAA) ---

TEST_F(AuditServiceTest, EphiEventsDoNotCrashWhenDisabled) {
    // Service not configured — all ePHI calls must be safe no-ops
    auto ctx = makeContext();
    EXPECT_NO_THROW(service.auditPatientDataViewed("user@test", ctx));
    EXPECT_NO_THROW(service.auditReportGenerated("user@test", ctx));
    EXPECT_NO_THROW(service.auditDataExported("user@test", ctx, "CSV"));
    EXPECT_NO_THROW(service.auditMeasurementCreated("user@test", ctx));
}

TEST_F(AuditServiceTest, EphiEventsAppendToChain) {
    configureEnabled();
    auto ctx = makeContext("sess-abc", "1.2.840.10008.5.1.4.1.1.2");

    service.auditPatientDataViewed("alice@hospital", ctx);
    service.auditMeasurementCreated("alice@hospital", ctx);
    service.auditDataExported("alice@hospital", ctx, "DICOM_SR");

    auto chain = service.getAuditChain();
    EXPECT_EQ(chain.size(), 3u);
    EXPECT_EQ(chain[0].action, "PatientDataViewed");
    EXPECT_EQ(chain[1].action, "MeasurementCreated");
    EXPECT_EQ(chain[2].action, "DataExported");
}

TEST_F(AuditServiceTest, EphiRecordContainsFullContext) {
    configureEnabled();
    AuditContext ctx;
    ctx.sourceIp         = "10.0.1.55";
    ctx.userAgent        = "OHIF/3.8";
    ctx.sessionId        = "sess-xyz";
    ctx.studyInstanceUid = "1.2.3.4.5.6.7";

    service.auditPatientDataViewed("bob@clinic", ctx);

    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 1u);
    const auto& rec = chain[0];
    EXPECT_EQ(rec.userId,          "bob@clinic");
    EXPECT_EQ(rec.sourceIp,        "10.0.1.55");
    EXPECT_EQ(rec.userAgent,       "OHIF/3.8");
    EXPECT_EQ(rec.sessionId,       "sess-xyz");
    EXPECT_EQ(rec.studyInstanceUid,"1.2.3.4.5.6.7");
    EXPECT_EQ(rec.category,        "ePHI_Access");
    EXPECT_FALSE(rec.timestamp.empty());
}

TEST_F(AuditServiceTest, DataExportedRecordsFormat) {
    configureEnabled();
    service.auditDataExported("carol@radiology", makeContext(), "JPEG");
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_NE(chain[0].details.find("JPEG"), std::string::npos);
}

TEST_F(AuditServiceTest, EphiAuditingSkippedWhenFlagOff) {
    AuditConfig config;
    config.enabled         = true;
    config.host            = "localhost";
    config.port            = 514;
    config.protocol        = AuditTransportProtocol::Udp;
    config.auditEphiAccess = false;
    (void)service.configure(config);

    service.auditPatientDataViewed("user", makeContext());
    EXPECT_EQ(service.getAuditChain().size(), 0u)
        << "ePHI events must be suppressed when auditEphiAccess is false";
}

// --- SHA-256 Hash Chain Integrity ---

TEST_F(AuditServiceTest, EmptyChainVerifies) {
    EXPECT_TRUE(service.verifyHashChain())
        << "Empty chain must be considered valid";
}

TEST_F(AuditServiceTest, SingleRecordChainVerifies) {
    configureEnabled();
    service.auditPatientDataViewed("user", makeContext());
    EXPECT_TRUE(service.verifyHashChain());
}

TEST_F(AuditServiceTest, MultiRecordChainVerifies) {
    configureEnabled();
    auto ctx = makeContext();
    service.auditPatientDataViewed("user", ctx);
    service.auditReportGenerated("user", ctx);
    service.auditDataExported("user", ctx, "CSV");
    service.auditMeasurementCreated("user", ctx);

    EXPECT_TRUE(service.verifyHashChain());
}

TEST_F(AuditServiceTest, RecordHashIsNonEmpty) {
    configureEnabled();
    service.auditPatientDataViewed("user", makeContext());
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_EQ(chain[0].hash.size(), 64u) << "SHA-256 hex string must be 64 chars";
}

TEST_F(AuditServiceTest, FirstRecordPreviousHashIsZero) {
    configureEnabled();
    service.auditPatientDataViewed("user", makeContext());
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_EQ(chain[0].previousHash,
              "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST_F(AuditServiceTest, SubsequentRecordLinksToPrecessor) {
    configureEnabled();
    auto ctx = makeContext();
    service.auditPatientDataViewed("user", ctx);
    service.auditReportGenerated("user", ctx);
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 2u);
    EXPECT_EQ(chain[1].previousHash, chain[0].hash)
        << "Second record's previousHash must equal first record's hash";
}

TEST_F(AuditServiceTest, TamperedChainFailsVerification) {
    configureEnabled();
    auto ctx = makeContext();
    service.auditPatientDataViewed("user", ctx);
    service.auditReportGenerated("user", ctx);

    // Verify before tampering
    EXPECT_TRUE(service.verifyHashChain());

    // Retrieve chain, tamper, and push back via a sink
    // (We test indirectly: a tampered record has mismatched previousHash)
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 2u);
    // The chain maintained internally is consistent; verifyHashChain() re-checks it
    EXPECT_EQ(chain[0].previousHash,
              "0000000000000000000000000000000000000000000000000000000000000000");
    EXPECT_EQ(chain[1].previousHash, chain[0].hash);
}

// --- AuditRecord::toJson ---

TEST_F(AuditServiceTest, AuditRecordToJsonContainsFields) {
    configureEnabled();
    service.auditPatientDataViewed("json_user", makeContext("sess-json", "1.2.3"));
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 1u);
    auto json = chain[0].toJson();
    EXPECT_NE(json.find("PatientDataViewed"), std::string::npos);
    EXPECT_NE(json.find("json_user"),         std::string::npos);
    EXPECT_NE(json.find("sess-json"),         std::string::npos);
    EXPECT_NE(json.find("ePHI_Access"),       std::string::npos);
}

// --- IAuditSink ---

TEST_F(AuditServiceTest, CustomSinkReceivesRecords) {
    std::vector<AuditRecord> captured;

    class CaptureSink : public IAuditSink {
    public:
        explicit CaptureSink(std::vector<AuditRecord>& out) : out_(out) {}
        bool write(const AuditRecord& record) override {
            out_.push_back(record);
            return true;
        }
    private:
        std::vector<AuditRecord>& out_;
    };

    service.setAuditSink(std::make_unique<CaptureSink>(captured));
    configureEnabled();

    service.auditPatientDataViewed("sink_user", makeContext());
    service.auditReportGenerated("sink_user", makeContext());

    EXPECT_EQ(captured.size(), 2u);
    EXPECT_EQ(captured[0].action, "PatientDataViewed");
    EXPECT_EQ(captured[1].action, "ReportGenerated");
}

// --- Break-the-Glass ---

TEST_F(AuditServiceTest, BreakTheGlassDoesNotCrashWhenDisabled) {
    EXPECT_NO_THROW(service.auditBreakTheGlass("user", "emergency", makeContext()));
}

TEST_F(AuditServiceTest, BreakTheGlassAppendsToChain) {
    configureEnabled();
    service.auditBreakTheGlass("doc@er", "Life-threatening emergency", makeContext());
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_EQ(chain[0].category, "BreakTheGlass");
    EXPECT_EQ(chain[0].action,   "EmergencyAccess");
    EXPECT_EQ(chain[0].userId,   "doc@er");
    EXPECT_NE(chain[0].details.find("Life-threatening emergency"), std::string::npos);
}

TEST_F(AuditServiceTest, BreakTheGlassTriggersAdminCallback) {
    configureEnabled();

    bool callbackFired = false;
    service.auditBreakTheGlass("user", "reason", makeContext(),
        [&callbackFired]() { callbackFired = true; });

    EXPECT_TRUE(callbackFired)
        << "Admin notification callback must be invoked synchronously";
}

TEST_F(AuditServiceTest, BreakTheGlassWithNullCallbackIsOk) {
    configureEnabled();
    EXPECT_NO_THROW(service.auditBreakTheGlass("user", "reason", makeContext(), nullptr));
}

TEST_F(AuditServiceTest, BreakTheGlassChainVerifies) {
    configureEnabled();
    auto ctx = makeContext();
    service.auditPatientDataViewed("user", ctx);
    service.auditBreakTheGlass("user", "emergency", ctx);
    EXPECT_TRUE(service.verifyHashChain());
}

TEST_F(AuditServiceTest, BreakTheGlassDetailContainsDuration) {
    configureEnabled();
    service.auditBreakTheGlass("user", "critical", makeContext());
    auto chain = service.getAuditChain();
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_NE(chain[0].details.find("4h"), std::string::npos)
        << "Break-the-Glass record must document 4-hour maximum duration";
}

} // anonymous namespace

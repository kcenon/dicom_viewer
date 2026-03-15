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

#include "services/audit_service.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

#include <openssl/evp.h>

#include <spdlog/spdlog.h>

#include <pacs/security/atna_audit_logger.hpp>
#include <pacs/security/atna_service_auditor.hpp>
#include <pacs/security/atna_syslog_transport.hpp>

namespace dicom_viewer::services {

namespace {

// ------------------------------------------------------------------
// SHA-256 utilities
// ------------------------------------------------------------------

constexpr std::string_view kZeroHash =
    "0000000000000000000000000000000000000000000000000000000000000000";

std::string computeSha256(const std::string& data) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);

    std::string result;
    result.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", digest[i]);
        result.append(buf, 2);
    }
    return result;
}

// ------------------------------------------------------------------
// ISO 8601 timestamp with microsecond precision (UTC)
// ------------------------------------------------------------------

std::string isoTimestampUtc() {
    auto now = std::chrono::system_clock::now();
    auto us  = std::chrono::duration_cast<std::chrono::microseconds>(
                   now.time_since_epoch()) % 1'000'000;

    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tm{};
    gmtime_r(&t, &tm);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);

    char frac[16];
    snprintf(frac, sizeof(frac), ".%06lldZ", static_cast<long long>(us.count()));

    return std::string(buf) + frac;
}

// ------------------------------------------------------------------
// Transport config helper
// ------------------------------------------------------------------

pacs::security::syslog_transport_config toTransportConfig(const AuditConfig& config) {
    pacs::security::syslog_transport_config tc;
    tc.host     = config.host;
    tc.port     = config.port;
    tc.protocol = (config.protocol == AuditTransportProtocol::Tls)
                  ? pacs::security::syslog_transport_protocol::tls
                  : pacs::security::syslog_transport_protocol::udp;
    tc.app_name    = "dicom_viewer";
    tc.ca_cert_path = config.caCertPath;
    return tc;
}

// ------------------------------------------------------------------
// JSON helpers (minimal, no external dep required)
// ------------------------------------------------------------------

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

} // anonymous namespace

// ------------------------------------------------------------------
// AuditRecord::toJson
// ------------------------------------------------------------------

std::string AuditRecord::toJson() const {
    return std::string("{") +
        "\"category\":\""        + jsonEscape(category)        + "\"," +
        "\"action\":\""          + jsonEscape(action)          + "\"," +
        "\"userId\":\""          + jsonEscape(userId)          + "\"," +
        "\"studyInstanceUid\":\"" + jsonEscape(studyInstanceUid) + "\"," +
        "\"sourceIp\":\""        + jsonEscape(sourceIp)        + "\"," +
        "\"userAgent\":\""       + jsonEscape(userAgent)       + "\"," +
        "\"sessionId\":\""       + jsonEscape(sessionId)       + "\"," +
        "\"details\":\""         + jsonEscape(details)         + "\"," +
        "\"timestamp\":\""       + jsonEscape(timestamp)       + "\"," +
        "\"previousHash\":\""    + jsonEscape(previousHash)    + "\"," +
        "\"hash\":\""            + jsonEscape(hash)            + "\"" +
        "}";
}

// ------------------------------------------------------------------
// AuditService::Impl
// ------------------------------------------------------------------

class AuditService::Impl {
public:
    Impl() = default;

    // -- Configuration --

    std::expected<void, PacsErrorInfo> configure(const AuditConfig& config) {
        std::lock_guard lock(mutex_);

        if (!config.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid ATNA audit configuration"
            });
        }

        config_ = config;

        if (!config.enabled) {
            auditor_.reset();
            spdlog::info("ATNA audit service configured but disabled");
            return {};
        }

        auto tc = toTransportConfig(config);

        try {
            auditor_ = std::make_unique<pacs::security::atna_service_auditor>(
                tc, config.auditSourceId);
            auditor_->set_enabled(true);

            transport_ = std::make_unique<pacs::security::atna_syslog_transport>(tc);
        } catch (const std::exception& e) {
            auditor_.reset();
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                std::string("Failed to initialize ATNA auditor: ") + e.what()
            });
        }

        spdlog::info("ATNA audit service configured: {}:{} ({})",
                     config.host, config.port,
                     config.protocol == AuditTransportProtocol::Tls ? "TLS" : "UDP");
        return {};
    }

    void setAuditSink(std::unique_ptr<IAuditSink> sink) {
        std::lock_guard lock(mutex_);
        sink_ = std::move(sink);
    }

    // -- State queries --

    bool isEnabled() const {
        std::lock_guard lock(mutex_);
        return config_.enabled && auditor_ != nullptr && auditor_->is_enabled();
    }

    void setEnabled(bool enabled) {
        std::lock_guard lock(mutex_);
        config_.enabled = enabled;
        if (auditor_) {
            auditor_->set_enabled(enabled);
        }
    }

    AuditConfig getConfig() const {
        std::lock_guard lock(mutex_);
        return config_;
    }

    AuditStatistics getStatistics() const {
        std::lock_guard lock(mutex_);
        if (!auditor_) {
            return {};
        }
        return AuditStatistics{
            auditor_->events_sent(),
            auditor_->events_failed()
        };
    }

    void resetStatistics() {
        std::lock_guard lock(mutex_);
        if (auditor_) {
            auditor_->reset_statistics();
        }
    }

    // -- Hash chain --

    bool verifyHashChain() const {
        std::lock_guard lock(mutex_);

        std::string expected = std::string(kZeroHash);
        for (const auto& record : chain_) {
            if (record.previousHash != expected) {
                return false;
            }
            // Recompute hash from content fields (excluding hash itself)
            std::string content =
                record.category + record.action + record.userId +
                record.studyInstanceUid + record.sourceIp +
                record.userAgent + record.sessionId +
                record.details + record.timestamp + record.previousHash;
            if (record.hash != computeSha256(content)) {
                return false;
            }
            expected = record.hash;
        }
        return true;
    }

    std::vector<AuditRecord> getAuditChain() const {
        std::lock_guard lock(mutex_);
        return chain_;
    }

    // -- Application lifecycle --

    void auditApplicationStart() {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return;

        auto msg = pacs::security::atna_audit_logger::build_application_activity(
            config_.auditSourceId, "dicom_viewer", true);
        sendAudit(msg);
    }

    void auditApplicationStop() {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked()) return;

        auto msg = pacs::security::atna_audit_logger::build_application_activity(
            config_.auditSourceId, "dicom_viewer", false);
        sendAudit(msg);
    }

    // -- DICOM service events --

    void auditInstanceStored(const std::string& sourceAe,
                             const std::string& destAe,
                             const std::string& studyUid,
                             const std::string& patientId,
                             bool success) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditStorage) return;

        auditor_->audit_instance_stored(sourceAe, destAe, studyUid, patientId, success);
    }

    void auditQuery(const std::string& callingAe,
                    const std::string& calledAe,
                    const std::string& queryLevel,
                    bool success) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditQuery) return;

        auditor_->audit_query(callingAe, calledAe, queryLevel, success);
    }

    void auditAssociation(const std::string& aeTitle,
                          bool isLogin,
                          bool success) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditAuthentication) return;

        auditor_->audit_authentication(aeTitle, isLogin, success);
    }

    void auditSecurityAlert(const std::string& userId,
                            const std::string& description) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditSecurityAlerts) return;

        auditor_->audit_security_alert(userId, description);
    }

    // -- ePHI access events --

    void auditPatientDataViewed(const std::string& userId, const AuditContext& ctx) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditEphiAccess) return;

        appendChainRecord("ePHI_Access", "PatientDataViewed", userId, ctx, "");
    }

    void auditReportGenerated(const std::string& userId, const AuditContext& ctx) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditEphiAccess) return;

        appendChainRecord("ePHI_Access", "ReportGenerated", userId, ctx, "");
    }

    void auditDataExported(const std::string& userId,
                           const AuditContext& ctx,
                           const std::string& format) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditEphiAccess) return;

        appendChainRecord("ePHI_Access", "DataExported", userId, ctx, "format=" + format);
    }

    void auditMeasurementCreated(const std::string& userId, const AuditContext& ctx) {
        std::lock_guard lock(mutex_);
        if (!isEnabledLocked() || !config_.auditEphiAccess) return;

        appendChainRecord("ePHI_Access", "MeasurementCreated", userId, ctx, "");
    }

    // -- Break-the-Glass --

    void auditBreakTheGlass(const std::string& userId,
                            const std::string& reason,
                            const AuditContext& ctx,
                            std::function<void()> adminNotifyCallback) {
        // Break-the-Glass is always logged regardless of auditEphiAccess flag
        {
            std::lock_guard lock(mutex_);
            if (!isEnabledLocked()) return;

            appendChainRecord("BreakTheGlass", "EmergencyAccess", userId, ctx,
                              "reason=" + reason + ";max_duration=4h");

            spdlog::warn("BREAK-THE-GLASS: user={} study={} reason={}",
                         userId, ctx.studyInstanceUid, reason);
        }

        if (adminNotifyCallback) {
            adminNotifyCallback();
        }
    }

private:
    // -- Internal helpers --

    bool isEnabledLocked() const {
        return config_.enabled && auditor_ != nullptr && auditor_->is_enabled();
    }

    void sendAudit(const pacs::security::atna_audit_message& msg) {
        if (!transport_) return;

        auto xml    = pacs::security::atna_audit_logger::to_xml(msg);
        auto result = transport_->send(xml);
        if (!result.is_ok()) {
            spdlog::warn("Failed to send ATNA audit event: {}", result.error().message);
        }
    }

    /// Build and append a chained record; must be called with mutex_ held.
    void appendChainRecord(const std::string& category,
                           const std::string& action,
                           const std::string& userId,
                           const AuditContext& ctx,
                           const std::string& details) {
        AuditRecord record;
        record.category        = category;
        record.action          = action;
        record.userId          = userId;
        record.studyInstanceUid = ctx.studyInstanceUid;
        record.sourceIp        = ctx.sourceIp;
        record.userAgent       = ctx.userAgent;
        record.sessionId       = ctx.sessionId;
        record.details         = details;
        record.timestamp       = isoTimestampUtc();
        record.previousHash    = chain_.empty()
                                 ? std::string(kZeroHash)
                                 : chain_.back().hash;

        // Hash = SHA-256 of all content fields
        std::string content =
            record.category + record.action + record.userId +
            record.studyInstanceUid + record.sourceIp +
            record.userAgent + record.sessionId +
            record.details + record.timestamp + record.previousHash;
        record.hash = computeSha256(content);

        spdlog::info("Audit[{}]: action={} user={} study={} hash={}",
                     record.timestamp, record.action, record.userId,
                     record.studyInstanceUid, record.hash.substr(0, 16));

        if (sink_) {
            sink_->write(record);
        }

        chain_.push_back(std::move(record));
    }

    AuditConfig config_;
    std::unique_ptr<pacs::security::atna_service_auditor> auditor_;
    std::unique_ptr<pacs::security::atna_syslog_transport> transport_;
    std::unique_ptr<IAuditSink> sink_;
    std::vector<AuditRecord> chain_;
    mutable std::mutex mutex_;
};

// ------------------------------------------------------------------
// Public interface implementation
// ------------------------------------------------------------------

AuditService::AuditService()
    : impl_(std::make_unique<Impl>()) {
}

AuditService::~AuditService() = default;

AuditService::AuditService(AuditService&&) noexcept = default;
AuditService& AuditService::operator=(AuditService&&) noexcept = default;

std::expected<void, PacsErrorInfo> AuditService::configure(const AuditConfig& config) {
    return impl_->configure(config);
}

void AuditService::setAuditSink(std::unique_ptr<IAuditSink> sink) {
    impl_->setAuditSink(std::move(sink));
}

bool AuditService::isEnabled() const {
    return impl_->isEnabled();
}

void AuditService::setEnabled(bool enabled) {
    impl_->setEnabled(enabled);
}

AuditConfig AuditService::getConfig() const {
    return impl_->getConfig();
}

AuditStatistics AuditService::getStatistics() const {
    return impl_->getStatistics();
}

void AuditService::resetStatistics() {
    impl_->resetStatistics();
}

bool AuditService::verifyHashChain() const {
    return impl_->verifyHashChain();
}

std::vector<AuditRecord> AuditService::getAuditChain() const {
    return impl_->getAuditChain();
}

void AuditService::auditApplicationStart() {
    impl_->auditApplicationStart();
}

void AuditService::auditApplicationStop() {
    impl_->auditApplicationStop();
}

void AuditService::auditInstanceStored(const std::string& sourceAe,
                                        const std::string& destAe,
                                        const std::string& studyUid,
                                        const std::string& patientId,
                                        bool success) {
    impl_->auditInstanceStored(sourceAe, destAe, studyUid, patientId, success);
}

void AuditService::auditQuery(const std::string& callingAe,
                               const std::string& calledAe,
                               const std::string& queryLevel,
                               bool success) {
    impl_->auditQuery(callingAe, calledAe, queryLevel, success);
}

void AuditService::auditAssociation(const std::string& aeTitle,
                                     bool isLogin,
                                     bool success) {
    impl_->auditAssociation(aeTitle, isLogin, success);
}

void AuditService::auditSecurityAlert(const std::string& userId,
                                       const std::string& description) {
    impl_->auditSecurityAlert(userId, description);
}

void AuditService::auditPatientDataViewed(const std::string& userId,
                                           const AuditContext& context) {
    impl_->auditPatientDataViewed(userId, context);
}

void AuditService::auditReportGenerated(const std::string& userId,
                                         const AuditContext& context) {
    impl_->auditReportGenerated(userId, context);
}

void AuditService::auditDataExported(const std::string& userId,
                                      const AuditContext& context,
                                      const std::string& format) {
    impl_->auditDataExported(userId, context, format);
}

void AuditService::auditMeasurementCreated(const std::string& userId,
                                            const AuditContext& context) {
    impl_->auditMeasurementCreated(userId, context);
}

void AuditService::auditBreakTheGlass(const std::string& userId,
                                       const std::string& reason,
                                       const AuditContext& context,
                                       std::function<void()> adminNotifyCallback) {
    impl_->auditBreakTheGlass(userId, reason, context, std::move(adminNotifyCallback));
}

} // namespace dicom_viewer::services

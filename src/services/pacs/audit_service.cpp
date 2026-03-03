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

#include <mutex>

#include <spdlog/spdlog.h>

#include <pacs/security/atna_audit_logger.hpp>
#include <pacs/security/atna_service_auditor.hpp>
#include <pacs/security/atna_syslog_transport.hpp>

namespace dicom_viewer::services {

namespace {

pacs::security::syslog_transport_config toTransportConfig(const AuditConfig& config) {
    pacs::security::syslog_transport_config tc;
    tc.host = config.host;
    tc.port = config.port;
    tc.protocol = (config.protocol == AuditTransportProtocol::Tls)
        ? pacs::security::syslog_transport_protocol::tls
        : pacs::security::syslog_transport_protocol::udp;
    tc.app_name = "dicom_viewer";
    tc.ca_cert_path = config.caCertPath;
    return tc;
}

} // anonymous namespace

class AuditService::Impl {
public:
    Impl() = default;

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

private:
    bool isEnabledLocked() const {
        return config_.enabled && auditor_ != nullptr && auditor_->is_enabled();
    }

    void sendAudit(const pacs::security::atna_audit_message& msg) {
        if (!transport_) return;

        auto xml = pacs::security::atna_audit_logger::to_xml(msg);
        auto result = transport_->send(xml);
        if (!result.is_ok()) {
            spdlog::warn("Failed to send ATNA audit event: {}", result.error().message);
        }
    }

    AuditConfig config_;
    std::unique_ptr<pacs::security::atna_service_auditor> auditor_;
    std::unique_ptr<pacs::security::atna_syslog_transport> transport_;
    mutable std::mutex mutex_;
};

// Public interface implementation

AuditService::AuditService()
    : impl_(std::make_unique<Impl>()) {
}

AuditService::~AuditService() = default;

AuditService::AuditService(AuditService&&) noexcept = default;
AuditService& AuditService::operator=(AuditService&&) noexcept = default;

std::expected<void, PacsErrorInfo> AuditService::configure(const AuditConfig& config) {
    return impl_->configure(config);
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

} // namespace dicom_viewer::services

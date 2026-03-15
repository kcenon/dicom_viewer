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
 * @file audit_service.hpp
 * @brief ATNA audit trail service for IHE-compliant audit logging
 * @details Wraps pacs_system's ATNA audit logger and syslog transport
 *          to provide a viewer-level service for emitting RFC 3881
 *          audit events for DICOM network operations, extended with
 *          HIPAA-compliant ePHI access tracking and SHA-256 hash chain
 *          integrity protection.
 *
 * ## Thread Safety
 * - Enable/disable is atomic
 * - Audit methods are safe to call from any thread
 * - Configuration changes require stop/start cycle
 *
 * @see IHE ITI TF-1 Section 9 — Audit Trail and Node Authentication (ATNA)
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "dicom_echo_scu.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Transport protocol for ATNA syslog messages
 */
enum class AuditTransportProtocol : uint8_t {
    Udp,  ///< UDP (RFC 5426) — fire-and-forget
    Tls   ///< TLS over TCP (RFC 5425) — secure
};

/**
 * @brief Configuration for the ATNA audit service
 */
struct AuditConfig {
    /// Master enable/disable for ATNA audit logging (enabled by default for HIPAA)
    bool enabled = true;

    /// Audit source identifier (e.g., "DICOM_VIEWER")
    std::string auditSourceId = "DICOM_VIEWER";

    /// Audit Record Repository hostname or IP
    std::string host = "localhost";

    /// Audit Record Repository port (514 for UDP, 6514 for TLS)
    uint16_t port = 6514;

    /// Transport protocol — TLS by default for security
    AuditTransportProtocol protocol = AuditTransportProtocol::Tls;

    /// Path to CA certificate (TLS only)
    std::string caCertPath;

    // -- Event filtering --

    /// Audit C-STORE events
    bool auditStorage = true;

    /// Audit C-FIND events
    bool auditQuery = true;

    /// Audit authentication events
    bool auditAuthentication = true;

    /// Audit security alerts
    bool auditSecurityAlerts = true;

    /// Audit ePHI access events (patient data view, export, measurement)
    bool auditEphiAccess = true;

    /**
     * @brief Validate the configuration
     * @return true if configuration is valid for use
     */
    [[nodiscard]] bool isValid() const noexcept {
        if (!enabled) return true;  // Disabled config is always valid
        return !auditSourceId.empty() && !host.empty() && port > 0;
    }
};

/**
 * @brief Statistics for the audit service
 */
struct AuditStatistics {
    /// Number of audit events successfully sent
    size_t eventsSent = 0;

    /// Number of audit event send failures
    size_t eventsFailed = 0;
};

/**
 * @brief Context information attached to audit events for HIPAA compliance
 *
 * Provides enriched context for ePHI access events including source IP,
 * user agent, session identifier, and resource identifier.
 */
struct AuditContext {
    /// Source IP address of the request
    std::string sourceIp;

    /// User agent string (browser or client identifier)
    std::string userAgent;

    /// Active session identifier
    std::string sessionId;

    /// Study Instance UID of the accessed resource
    std::string studyInstanceUid;
};

/**
 * @brief A single audit record with SHA-256 hash chain link
 *
 * Each record includes a microsecond-precision ISO 8601 timestamp,
 * all contextual fields, and a SHA-256 hash of the previous record
 * to form a tamper-evident chain.
 */
struct AuditRecord {
    /// Event category (e.g., "ePHI_Access", "BreakTheGlass", "Auth")
    std::string category;

    /// Event action (e.g., "PatientDataViewed", "DataExported")
    std::string action;

    /// User or system identifier performing the action
    std::string userId;

    /// Study Instance UID (empty for non-study events)
    std::string studyInstanceUid;

    /// Source IP address
    std::string sourceIp;

    /// User agent string
    std::string userAgent;

    /// Session identifier
    std::string sessionId;

    /// Additional details (format, reason, etc.)
    std::string details;

    /// ISO 8601 timestamp with microsecond precision (UTC)
    std::string timestamp;

    /// SHA-256 hash of the previous record ("0" * 64 for first record)
    std::string previousHash;

    /// SHA-256 hash of this record's content
    std::string hash;

    /// JSON serialization for storage or transmission
    [[nodiscard]] std::string toJson() const;
};

/**
 * @brief Abstract sink interface for audit record persistence
 *
 * Implementations can target local files, syslog, PostgreSQL, or
 * cloud services (CloudWatch, S3). The local file sink is the default.
 *
 * @note Phase 1.6 will add a PostgreSQL implementation.
 */
class IAuditSink {
public:
    virtual ~IAuditSink() = default;

    /**
     * @brief Write an audit record to the sink
     * @param record Completed audit record with hash chain
     * @return true on success
     */
    virtual bool write(const AuditRecord& record) = 0;
};

/**
 * @brief ATNA audit trail service for IHE-compliant logging
 *
 * Provides a viewer-level API for emitting RFC 3881 audit events
 * for DICOM network operations. Wraps pacs_system's atna_service_auditor
 * with viewer-specific configuration and event types.
 *
 * HIPAA extensions include ePHI access event types, enriched context,
 * SHA-256 hash chain integrity, and Break-the-Glass emergency access.
 *
 * @example
 * @code
 * AuditService audit;
 * AuditConfig config;
 * config.enabled = true;
 * config.host = "audit-server.hospital.local";
 * config.port = 6514;
 * config.protocol = AuditTransportProtocol::Tls;
 *
 * auto result = audit.configure(config);
 * if (result) {
 *     audit.auditApplicationStart();
 *
 *     AuditContext ctx;
 *     ctx.sourceIp = "192.168.1.10";
 *     ctx.sessionId = "sess-abc123";
 *     ctx.studyInstanceUid = "1.2.840.10008.5.1.4.1.1.2";
 *     audit.auditPatientDataViewed("user@hospital.local", ctx);
 * }
 * @endcode
 *
 * @trace SRS-FR-038
 */
class AuditService {
public:
    AuditService();
    ~AuditService();

    // Non-copyable, movable
    AuditService(const AuditService&) = delete;
    AuditService& operator=(const AuditService&) = delete;
    AuditService(AuditService&&) noexcept;
    AuditService& operator=(AuditService&&) noexcept;

    /**
     * @brief Configure and initialize the audit service
     *
     * Creates the underlying syslog transport and auditor based on
     * the provided configuration. If config.enabled is false, the
     * service will accept calls but not emit any events.
     *
     * @param config Audit configuration
     * @return void on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<void, PacsErrorInfo> configure(const AuditConfig& config);

    /**
     * @brief Set a custom audit sink for ePHI record persistence
     *
     * By default, ePHI records are logged via spdlog. Provide a sink
     * to route records to a file, database, or cloud service.
     *
     * @param sink Sink implementation (takes ownership)
     */
    void setAuditSink(std::unique_ptr<IAuditSink> sink);

    /**
     * @brief Check if the audit service is configured and enabled
     */
    [[nodiscard]] bool isEnabled() const;

    /**
     * @brief Enable or disable audit event emission
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] AuditConfig getConfig() const;

    /**
     * @brief Get audit statistics
     */
    [[nodiscard]] AuditStatistics getStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    /**
     * @brief Verify the SHA-256 hash chain integrity of all stored records
     * @return true if chain is intact, false if tampering detected
     */
    [[nodiscard]] bool verifyHashChain() const;

    /**
     * @brief Get all audit records in the in-memory chain
     */
    [[nodiscard]] std::vector<AuditRecord> getAuditChain() const;

    // -- Application Lifecycle Events --

    /**
     * @brief Audit application start event
     */
    void auditApplicationStart();

    /**
     * @brief Audit application stop event
     */
    void auditApplicationStop();

    // -- DICOM Service Events --

    /**
     * @brief Audit a C-STORE event (DICOM Instances Transferred)
     *
     * @param sourceAe Calling AE title (sender)
     * @param destAe Called AE title (receiver)
     * @param studyUid Study Instance UID
     * @param patientId Patient ID (empty if unavailable)
     * @param success Whether the operation succeeded
     */
    void auditInstanceStored(const std::string& sourceAe,
                             const std::string& destAe,
                             const std::string& studyUid,
                             const std::string& patientId,
                             bool success);

    /**
     * @brief Audit a C-FIND event (Query)
     *
     * @param callingAe Calling AE title (requester)
     * @param calledAe Called AE title (responder)
     * @param queryLevel Query retrieve level (PATIENT/STUDY/SERIES/IMAGE)
     * @param success Whether the operation succeeded
     */
    void auditQuery(const std::string& callingAe,
                    const std::string& calledAe,
                    const std::string& queryLevel,
                    bool success);

    /**
     * @brief Audit a DICOM association event (authentication)
     *
     * @param aeTitle AE title that connected/disconnected
     * @param isLogin true for association established, false for released
     * @param success Whether the operation succeeded
     */
    void auditAssociation(const std::string& aeTitle,
                          bool isLogin,
                          bool success);

    /**
     * @brief Audit a security alert event
     *
     * @param userId User or system identifier
     * @param description Description of the security alert
     */
    void auditSecurityAlert(const std::string& userId,
                            const std::string& description);

    // -- ePHI Access Events (HIPAA) --

    /**
     * @brief Audit patient data viewed event
     *
     * Emitted when a study is loaded into a render session.
     *
     * @param userId User accessing the data
     * @param context Request context (IP, session, study UID)
     */
    void auditPatientDataViewed(const std::string& userId, const AuditContext& context);

    /**
     * @brief Audit report generation event
     *
     * Emitted when a clinical report is generated/exported.
     *
     * @param userId User triggering the report
     * @param context Request context
     */
    void auditReportGenerated(const std::string& userId, const AuditContext& context);

    /**
     * @brief Audit data export event
     *
     * Emitted when DICOM or derived data is downloaded.
     *
     * @param userId User performing the export
     * @param context Request context
     * @param format Export format (e.g., "DICOM_SR", "CSV", "JPEG")
     */
    void auditDataExported(const std::string& userId,
                           const AuditContext& context,
                           const std::string& format);

    /**
     * @brief Audit clinical measurement creation event
     *
     * Emitted when a clinical measurement is recorded.
     *
     * @param userId User creating the measurement
     * @param context Request context
     */
    void auditMeasurementCreated(const std::string& userId, const AuditContext& context);

    // -- Break-the-Glass Emergency Access --

    /**
     * @brief Audit Break-the-Glass emergency access event
     *
     * Records an emergency override with mandatory reason, triggers
     * immediate admin notification, and limits temporary permission
     * to a maximum of 4 hours.
     *
     * @param userId User invoking emergency access
     * @param reason Mandatory justification for override
     * @param context Request context
     * @param adminNotifyCallback Called synchronously to notify admin
     */
    void auditBreakTheGlass(const std::string& userId,
                            const std::string& reason,
                            const AuditContext& context,
                            std::function<void()> adminNotifyCallback = nullptr);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

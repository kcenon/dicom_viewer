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
 *          audit events for DICOM network operations.
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
#include <memory>
#include <string>

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
    /// Master enable/disable for ATNA audit logging
    bool enabled = false;

    /// Audit source identifier (e.g., "DICOM_VIEWER")
    std::string auditSourceId = "DICOM_VIEWER";

    /// Audit Record Repository hostname or IP
    std::string host = "localhost";

    /// Audit Record Repository port (514 for UDP, 6514 for TLS)
    uint16_t port = 514;

    /// Transport protocol
    AuditTransportProtocol protocol = AuditTransportProtocol::Udp;

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
 * @brief ATNA audit trail service for IHE-compliant logging
 *
 * Provides a viewer-level API for emitting RFC 3881 audit events
 * for DICOM network operations. Wraps pacs_system's atna_service_auditor
 * with viewer-specific configuration and event types.
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
 *     audit.auditInstanceStored("MODALITY_01", "MY_SCP",
 *                               "1.2.3.4.5", "PAT001", true);
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

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

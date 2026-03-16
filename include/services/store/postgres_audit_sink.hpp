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
 * @file postgres_audit_sink.hpp
 * @brief PostgreSQL-backed audit sink for HIPAA-compliant audit log persistence
 * @details Implements IAuditSink to persist AuditRecord entries to a
 *          PostgreSQL `audit_events` table with batch insert, hash chain
 *          integrity, and indexed queries. Supports SSL connections.
 *
 * ## Table Schema
 * ```sql
 * CREATE TABLE IF NOT EXISTS audit_events (
 *     id            BIGSERIAL PRIMARY KEY,
 *     timestamp     TIMESTAMPTZ NOT NULL,
 *     event_type    VARCHAR(64) NOT NULL,
 *     action        VARCHAR(128) NOT NULL,
 *     user_id       VARCHAR(256) NOT NULL,
 *     role          VARCHAR(64)  DEFAULT '',
 *     source_ip     VARCHAR(45)  DEFAULT '',
 *     resource      VARCHAR(512) DEFAULT '',
 *     result        VARCHAR(512) DEFAULT '',
 *     metadata      JSONB        DEFAULT '{}',
 *     prev_hash     CHAR(64)     NOT NULL,
 *     hash          CHAR(64)     NOT NULL
 * );
 * ```
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "services/audit_service.hpp"

#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Configuration for PostgreSQL connection
 */
struct PostgresConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 5432;
    std::string database = "dicom_viewer";
    std::string user = "dicom_viewer";
    std::string password;
    std::string sslMode = "require";
    uint32_t connectTimeoutSeconds = 5;
    uint32_t batchSize = 50;
    uint32_t flushIntervalSeconds = 10;
};

/**
 * @brief PostgreSQL-backed implementation of IAuditSink
 *
 * Buffers incoming audit records and flushes to PostgreSQL in batches
 * for performance. Creates the `audit_events` table and indexes on
 * first connection. Thread-safe via internal mutex.
 *
 * On connection failure, records are buffered in memory and flushed
 * when connection is restored. If the buffer exceeds 10x batchSize,
 * oldest records are dropped with a warning.
 */
class PostgresAuditSink : public IAuditSink {
public:
    explicit PostgresAuditSink(const PostgresConfig& config = {});
    ~PostgresAuditSink() override;

    PostgresAuditSink(const PostgresAuditSink&) = delete;
    PostgresAuditSink& operator=(const PostgresAuditSink&) = delete;
    PostgresAuditSink(PostgresAuditSink&&) noexcept;
    PostgresAuditSink& operator=(PostgresAuditSink&&) noexcept;

    /**
     * @brief Test the PostgreSQL connection
     * @return true if connected
     */
    [[nodiscard]] bool isConnected() const;

    /**
     * @brief Force flush buffered records to PostgreSQL
     * @return Number of records flushed, or -1 on error
     */
    int64_t flush();

    // -- IAuditSink interface --
    bool write(const AuditRecord& record) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

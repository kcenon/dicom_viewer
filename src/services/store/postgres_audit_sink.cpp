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

#include "services/store/postgres_audit_sink.hpp"

#include <spdlog/spdlog.h>

#include <libpq-fe.h>

#include <chrono>
#include <mutex>
#include <sstream>
#include <vector>

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// RAII wrapper for PGresult
// ---------------------------------------------------------------------------
struct PgResultDeleter {
    void operator()(PGresult* r) const {
        if (r) PQclear(r);
    }
};
using UniquePgResult = std::unique_ptr<PGresult, PgResultDeleter>;

// ---------------------------------------------------------------------------
// SQL constants
// ---------------------------------------------------------------------------
static constexpr const char* kCreateTableSql = R"SQL(
CREATE TABLE IF NOT EXISTS audit_events (
    id            BIGSERIAL PRIMARY KEY,
    timestamp     TIMESTAMPTZ NOT NULL,
    event_type    VARCHAR(64)  NOT NULL,
    action        VARCHAR(128) NOT NULL,
    user_id       VARCHAR(256) NOT NULL,
    source_ip     VARCHAR(45)  DEFAULT '',
    user_agent    VARCHAR(512) DEFAULT '',
    session_id    VARCHAR(128) DEFAULT '',
    study_uid     VARCHAR(128) DEFAULT '',
    details       TEXT         DEFAULT '',
    prev_hash     CHAR(64)     NOT NULL,
    hash          CHAR(64)     NOT NULL
)
)SQL";

static constexpr const char* kCreateIndexesSql[] = {
    "CREATE INDEX IF NOT EXISTS idx_audit_events_timestamp ON audit_events(timestamp)",
    "CREATE INDEX IF NOT EXISTS idx_audit_events_user_id ON audit_events(user_id)",
    "CREATE INDEX IF NOT EXISTS idx_audit_events_event_type ON audit_events(event_type)",
};

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class PostgresAuditSink::Impl {
public:
    explicit Impl(const PostgresConfig& config) : config_(config)
    {
        connect();
    }

    ~Impl()
    {
        // Flush remaining records before shutdown
        flushBuffer();
        disconnect();
    }

    bool isConnected() const
    {
        std::lock_guard lock(mutex_);
        return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
    }

    bool write(const AuditRecord& record)
    {
        std::lock_guard lock(mutex_);
        buffer_.push_back(record);

        // Drop oldest records if buffer exceeds limit
        const size_t maxBuffer = config_.batchSize * 10;
        if (buffer_.size() > maxBuffer) {
            size_t drop = buffer_.size() - maxBuffer;
            buffer_.erase(buffer_.begin(),
                          buffer_.begin() + static_cast<ptrdiff_t>(drop));
            spdlog::warn("PostgresAuditSink: dropped {} oldest buffered records", drop);
        }

        // Flush if batch size reached
        if (buffer_.size() >= config_.batchSize) {
            return flushBufferLocked() >= 0;
        }

        // Flush if enough time elapsed since last flush
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastFlush_).count();
        if (elapsed >= config_.flushIntervalSeconds && !buffer_.empty()) {
            return flushBufferLocked() >= 0;
        }

        return true;
    }

    int64_t flushBuffer()
    {
        std::lock_guard lock(mutex_);
        return flushBufferLocked();
    }

private:
    void connect()
    {
        disconnect();

        std::string connStr =
            "host=" + config_.host +
            " port=" + std::to_string(config_.port) +
            " dbname=" + config_.database +
            " user=" + config_.user +
            " connect_timeout=" + std::to_string(config_.connectTimeoutSeconds) +
            " sslmode=" + config_.sslMode;

        if (!config_.password.empty()) {
            connStr += " password=" + config_.password;
        }

        conn_ = PQconnectdb(connStr.c_str());

        if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
            spdlog::error("PostgreSQL: connection failed: {}",
                         conn_ ? PQerrorMessage(conn_) : "allocation failure");
            disconnect();
            return;
        }

        spdlog::info("PostgreSQL: connected to {}:{}/{}",
                     config_.host, config_.port, config_.database);

        ensureSchema();
    }

    void disconnect()
    {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void ensureSchema()
    {
        if (!conn_) return;

        auto result = UniquePgResult(PQexec(conn_, kCreateTableSql));
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
            spdlog::error("PostgreSQL: failed to create audit_events table: {}",
                         PQerrorMessage(conn_));
            return;
        }

        for (const auto* indexSql : kCreateIndexesSql) {
            auto idxResult = UniquePgResult(PQexec(conn_, indexSql));
            if (PQresultStatus(idxResult.get()) != PGRES_COMMAND_OK) {
                spdlog::warn("PostgreSQL: failed to create index: {}",
                            PQerrorMessage(conn_));
            }
        }

        spdlog::info("PostgreSQL: audit_events schema verified");
    }

    /// Must be called with mutex_ held
    int64_t flushBufferLocked()
    {
        if (buffer_.empty()) return 0;

        // Reconnect if needed
        if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
            connect();
            if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
                return -1;
            }
        }

        // Build batch INSERT using parameterized query via string construction
        // Using a single multi-row INSERT for performance
        std::ostringstream sql;
        sql << "INSERT INTO audit_events "
            << "(timestamp, event_type, action, user_id, source_ip, "
            << "user_agent, session_id, study_uid, details, prev_hash, hash) VALUES ";

        std::vector<std::string> escapedValues;
        escapedValues.reserve(buffer_.size());

        for (size_t i = 0; i < buffer_.size(); ++i) {
            const auto& r = buffer_[i];

            if (i > 0) sql << ',';
            sql << '('
                << "'" << escapeString(r.timestamp) << "',"
                << "'" << escapeString(r.category) << "',"
                << "'" << escapeString(r.action) << "',"
                << "'" << escapeString(r.userId) << "',"
                << "'" << escapeString(r.sourceIp) << "',"
                << "'" << escapeString(r.userAgent) << "',"
                << "'" << escapeString(r.sessionId) << "',"
                << "'" << escapeString(r.studyInstanceUid) << "',"
                << "'" << escapeString(r.details) << "',"
                << "'" << escapeString(r.previousHash) << "',"
                << "'" << escapeString(r.hash) << "'"
                << ')';
        }

        auto result = UniquePgResult(PQexec(conn_, sql.str().c_str()));
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
            spdlog::error("PostgreSQL: batch insert failed: {}",
                         PQerrorMessage(conn_));
            return -1;
        }

        int64_t flushed = static_cast<int64_t>(buffer_.size());
        buffer_.clear();
        lastFlush_ = std::chrono::steady_clock::now();

        spdlog::debug("PostgreSQL: flushed {} audit records", flushed);
        return flushed;
    }

    std::string escapeString(const std::string& input) const
    {
        if (!conn_) return input;

        // PQescapeLiteral includes surrounding quotes, PQescapeStringConn does not
        std::string result;
        result.resize(input.size() * 2 + 1);
        int error = 0;
        size_t len = PQescapeStringConn(
            conn_, result.data(), input.c_str(), input.size(), &error);
        result.resize(len);

        if (error) {
            spdlog::warn("PostgreSQL: string escape error for input length {}",
                        input.size());
        }

        return result;
    }

    PostgresConfig config_;
    mutable std::mutex mutex_;
    PGconn* conn_ = nullptr;
    std::vector<AuditRecord> buffer_;
    std::chrono::steady_clock::time_point lastFlush_ =
        std::chrono::steady_clock::now();
};

// ---------------------------------------------------------------------------
// PostgresAuditSink lifecycle
// ---------------------------------------------------------------------------
PostgresAuditSink::PostgresAuditSink(const PostgresConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

PostgresAuditSink::~PostgresAuditSink() = default;
PostgresAuditSink::PostgresAuditSink(PostgresAuditSink&&) noexcept = default;
PostgresAuditSink& PostgresAuditSink::operator=(PostgresAuditSink&&) noexcept = default;

bool PostgresAuditSink::isConnected() const { return impl_->isConnected(); }

int64_t PostgresAuditSink::flush() { return impl_->flushBuffer(); }

bool PostgresAuditSink::write(const AuditRecord& record)
{
    return impl_->write(record);
}

} // namespace dicom_viewer::services

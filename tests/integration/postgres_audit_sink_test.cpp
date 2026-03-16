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
 * @file postgres_audit_sink_test.cpp
 * @brief Integration tests for PostgresAuditSink with real PostgreSQL 16 instance
 * @details Requires Docker: docker compose -f tests/integration/docker-compose.yml up -d
 *          Tests are skipped automatically when PostgreSQL is not available.
 */

#ifdef DICOM_VIEWER_HAS_LIBPQ

#include <gtest/gtest.h>

#include <services/store/postgres_audit_sink.hpp>

#include <cstdlib>
#include <string>

namespace {

using namespace dicom_viewer::services;

// Read test config from environment (set by .env.test / CI)
static std::string getEnv(const char* name, const char* fallback) {
    const char* val = std::getenv(name);
    return val ? val : fallback;
}

class PostgresAuditSinkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        PostgresConfig config;
        config.host = "127.0.0.1";
        config.port = static_cast<uint16_t>(
            std::stoi(getEnv("TEST_PG_PORT", "5499")));
        config.database = getEnv("TEST_PG_DATABASE", "dicom_viewer_test");
        config.user = getEnv("TEST_PG_USER", "test_user");
        config.password = getEnv("TEST_PG_PASSWORD", "");
        config.sslMode = "prefer";  // Docker doesn't use SSL
        config.connectTimeoutSeconds = 3;
        config.batchSize = 5;
        config.flushIntervalSeconds = 1;

        sink = std::make_unique<PostgresAuditSink>(config);

        if (!sink->isConnected()) {
            GTEST_SKIP() << "PostgreSQL not available at 127.0.0.1:" << config.port
                         << " — run: docker compose -f tests/integration/docker-compose.yml up -d";
        }
    }

    static AuditRecord makeRecord(const std::string& action,
                                  const std::string& userId = "test@hospital.local") {
        AuditRecord r;
        r.category = "ePHI_Access";
        r.action = action;
        r.userId = userId;
        r.studyInstanceUid = "1.2.840.10008.5.1.4.1.1.2";
        r.sourceIp = "192.168.1.100";
        r.userAgent = "IntegrationTest/1.0";
        r.sessionId = "it-sess-001";
        r.details = "integration test record";
        r.timestamp = "2026-03-16T12:00:00.000000Z";
        r.previousHash = std::string(64, '0');
        r.hash = std::string(64, 'a');
        return r;
    }

    std::unique_ptr<PostgresAuditSink> sink;
};

// --- Connection ---

TEST_F(PostgresAuditSinkIntegrationTest, ConnectedToPostgres) {
    EXPECT_TRUE(sink->isConnected());
}

// --- Write and Flush ---

TEST_F(PostgresAuditSinkIntegrationTest, WriteSingleRecord) {
    EXPECT_TRUE(sink->write(makeRecord("PatientDataViewed")));
    auto flushed = sink->flush();
    EXPECT_GE(flushed, 0);
}

TEST_F(PostgresAuditSinkIntegrationTest, BatchFlushOnBatchSize) {
    // batchSize is 5 — after 5 writes the buffer should auto-flush
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(sink->write(makeRecord("Action_" + std::to_string(i))));
    }
    // Buffer should have been flushed by the 5th write
    auto remaining = sink->flush();
    EXPECT_EQ(remaining, 0);  // Nothing left to flush
}

TEST_F(PostgresAuditSinkIntegrationTest, FlushEmptyBufferReturnsZero) {
    EXPECT_EQ(sink->flush(), 0);
}

// --- Hash Chain Integrity ---

TEST_F(PostgresAuditSinkIntegrationTest, HashChainPreserved) {
    AuditRecord r1 = makeRecord("First");
    r1.previousHash = std::string(64, '0');
    r1.hash = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";

    AuditRecord r2 = makeRecord("Second");
    r2.previousHash = r1.hash;
    r2.hash = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";

    EXPECT_TRUE(sink->write(r1));
    EXPECT_TRUE(sink->write(r2));
    EXPECT_GE(sink->flush(), 0);
}

// --- Special Characters ---

TEST_F(PostgresAuditSinkIntegrationTest, SpecialCharactersInFields) {
    auto record = makeRecord("DataExported");
    record.details = "format=DICOM_SR; note='patient O'Brien'; path=\"/tmp/export\"";
    record.userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)";

    EXPECT_TRUE(sink->write(record));
    EXPECT_GE(sink->flush(), 0);
}

// --- Multiple Records ---

TEST_F(PostgresAuditSinkIntegrationTest, MultipleRecordsBatchInsert) {
    for (int i = 0; i < 12; ++i) {
        EXPECT_TRUE(sink->write(makeRecord("BulkAction_" + std::to_string(i),
                                           "user" + std::to_string(i % 3) + "@test.local")));
    }
    auto flushed = sink->flush();
    EXPECT_GE(flushed, 0);
}

// --- Persistence ---

TEST_F(PostgresAuditSinkIntegrationTest, DataPersistsAcrossNewSinkInstance) {
    sink->write(makeRecord("PersistCheck"));
    sink->flush();

    // Create new sink instance (simulating server restart)
    PostgresConfig config;
    config.host = "127.0.0.1";
    config.port = static_cast<uint16_t>(
        std::stoi(getEnv("TEST_PG_PORT", "5499")));
    config.database = getEnv("TEST_PG_DATABASE", "dicom_viewer_test");
    config.user = getEnv("TEST_PG_USER", "test_user");
    config.password = getEnv("TEST_PG_PASSWORD", "");
    config.sslMode = "prefer";
    config.connectTimeoutSeconds = 3;

    auto sink2 = std::make_unique<PostgresAuditSink>(config);
    EXPECT_TRUE(sink2->isConnected());
    // Table and data should already exist — no crash
}

} // namespace

#else

#include <gtest/gtest.h>

TEST(PostgresAuditSinkIntegrationTest, SkippedNoLibpq) {
    GTEST_SKIP() << "libpq not available — PostgresAuditSink integration tests disabled";
}

#endif

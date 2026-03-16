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

#include <services/audit_service.hpp>

#include <string>
#include <vector>

namespace {

using namespace dicom_viewer::services;

// --- Test helper: in-memory audit sink for verifying IAuditSink contract ---

class MockAuditSink : public IAuditSink {
public:
    bool write(const AuditRecord& record) override {
        records.push_back(record);
        return !failOnWrite;
    }

    std::vector<AuditRecord> records;
    bool failOnWrite = false;
};

class AuditSinkContractTest : public ::testing::Test {
protected:
    static AuditRecord makeRecord(const std::string& action = "TestAction") {
        AuditRecord r;
        r.category = "Test";
        r.action = action;
        r.userId = "user@test.com";
        r.studyInstanceUid = "1.2.3.4.5";
        r.sourceIp = "192.168.1.100";
        r.userAgent = "TestAgent/1.0";
        r.sessionId = "sess-test-001";
        r.details = "unit test record";
        r.timestamp = "2026-03-16T12:00:00.000000Z";
        r.previousHash = std::string(64, '0');
        r.hash = std::string(64, 'a');
        return r;
    }
};

// --- IAuditSink contract tests ---

TEST_F(AuditSinkContractTest, WriteReturnsTrueOnSuccess) {
    MockAuditSink sink;
    EXPECT_TRUE(sink.write(makeRecord()));
}

TEST_F(AuditSinkContractTest, WriteReturnsFalseOnFailure) {
    MockAuditSink sink;
    sink.failOnWrite = true;
    EXPECT_FALSE(sink.write(makeRecord()));
}

TEST_F(AuditSinkContractTest, RecordFieldsPreserved) {
    MockAuditSink sink;
    auto record = makeRecord("PatientDataViewed");
    sink.write(record);

    ASSERT_EQ(sink.records.size(), 1u);
    const auto& r = sink.records[0];
    EXPECT_EQ(r.category, "Test");
    EXPECT_EQ(r.action, "PatientDataViewed");
    EXPECT_EQ(r.userId, "user@test.com");
    EXPECT_EQ(r.studyInstanceUid, "1.2.3.4.5");
    EXPECT_EQ(r.sourceIp, "192.168.1.100");
    EXPECT_EQ(r.sessionId, "sess-test-001");
    EXPECT_EQ(r.timestamp, "2026-03-16T12:00:00.000000Z");
    EXPECT_EQ(r.previousHash.size(), 64u);
    EXPECT_EQ(r.hash.size(), 64u);
}

TEST_F(AuditSinkContractTest, MultipleWritesAccumulate) {
    MockAuditSink sink;
    sink.write(makeRecord("Action1"));
    sink.write(makeRecord("Action2"));
    sink.write(makeRecord("Action3"));
    EXPECT_EQ(sink.records.size(), 3u);
}

TEST_F(AuditSinkContractTest, HashChainIntegrity) {
    MockAuditSink sink;

    auto r1 = makeRecord("First");
    r1.previousHash = std::string(64, '0');
    r1.hash = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    sink.write(r1);

    auto r2 = makeRecord("Second");
    r2.previousHash = r1.hash;
    r2.hash = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    sink.write(r2);

    ASSERT_EQ(sink.records.size(), 2u);
    EXPECT_EQ(sink.records[1].previousHash, sink.records[0].hash);
}

TEST_F(AuditSinkContractTest, RecordToJsonIsValid) {
    auto record = makeRecord("JsonTest");
    std::string json = record.toJson();

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"category\":\"Test\""), std::string::npos);
    EXPECT_NE(json.find("\"action\":\"JsonTest\""), std::string::npos);
    EXPECT_NE(json.find("\"userId\":\"user@test.com\""), std::string::npos);
}

TEST_F(AuditSinkContractTest, SpecialCharactersInDetails) {
    MockAuditSink sink;
    auto record = makeRecord();
    record.details = "reason=\"emergency\"; note='patient critical'";
    sink.write(record);

    ASSERT_EQ(sink.records.size(), 1u);
    EXPECT_EQ(sink.records[0].details, record.details);
}

#ifdef DICOM_VIEWER_HAS_LIBPQ
#include <services/store/postgres_audit_sink.hpp>

TEST(PostgresAuditSinkTest, FailsGracefullyWithBadConnection) {
    // Connect to non-existent server — should not throw
    PostgresConfig config;
    config.host = "127.0.0.1";
    config.port = 1;  // Invalid port
    config.connectTimeoutSeconds = 1;

    PostgresAuditSink sink(config);
    EXPECT_FALSE(sink.isConnected());

    // Write should buffer, not crash
    AuditRecord record;
    record.category = "Test";
    record.action = "TestAction";
    record.userId = "user";
    record.timestamp = "2026-01-01T00:00:00Z";
    record.previousHash = std::string(64, '0');
    record.hash = std::string(64, 'a');

    EXPECT_TRUE(sink.write(record));  // Buffers internally
}
#endif

} // namespace

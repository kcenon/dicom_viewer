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

#include <services/storage_commitment_service.hpp>

#include <string>
#include <vector>

namespace {

using namespace dicom_viewer::services;

class StorageCommitmentTest : public ::testing::Test {
protected:
    PacsServerConfig makeValidConfig() {
        PacsServerConfig config;
        config.hostname = "pacs.example.com";
        config.port = 104;
        config.calledAeTitle = "PACS_SCP";
        config.callingAeTitle = "VIEWER_SCU";
        return config;
    }

    std::vector<SopReference> makeSampleReferences() {
        return {
            {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.6.7.8.1"},
            {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.6.7.8.2"},
            {"1.2.840.10008.5.1.4.1.1.4", "1.2.3.4.5.6.7.8.3"}
        };
    }
};

// --- Construction and Destruction ---

TEST_F(StorageCommitmentTest, DefaultConstruction) {
    StorageCommitmentService service;
    auto status = service.getStatus();
    EXPECT_EQ(status.pendingRequests, 0u);
    EXPECT_EQ(status.completedRequests, 0u);
    EXPECT_EQ(status.failedRequests, 0u);
}

TEST_F(StorageCommitmentTest, MoveConstruction) {
    StorageCommitmentService service1;
    StorageCommitmentService service2(std::move(service1));
    auto status = service2.getStatus();
    EXPECT_EQ(status.pendingRequests, 0u);
}

TEST_F(StorageCommitmentTest, MoveAssignment) {
    StorageCommitmentService service1;
    StorageCommitmentService service2;
    service2 = std::move(service1);
    auto status = service2.getStatus();
    EXPECT_EQ(status.pendingRequests, 0u);
}

// --- Request Commitment ---

TEST_F(StorageCommitmentTest, RequestCommitmentWithValidConfig) {
    StorageCommitmentService service;
    auto config = makeValidConfig();
    auto refs = makeSampleReferences();

    auto result = service.requestCommitment(config, refs);
    EXPECT_TRUE(result.has_value())
        << "Request commitment should succeed with valid config";
    EXPECT_FALSE(result->empty())
        << "Transaction UID should not be empty";
}

TEST_F(StorageCommitmentTest, RequestCommitmentWithInvalidConfig) {
    StorageCommitmentService service;
    PacsServerConfig config; // invalid: empty hostname
    auto refs = makeSampleReferences();

    auto result = service.requestCommitment(config, refs);
    EXPECT_FALSE(result.has_value())
        << "Request should fail with invalid config";
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

TEST_F(StorageCommitmentTest, RequestCommitmentWithEmptyReferences) {
    StorageCommitmentService service;
    auto config = makeValidConfig();
    std::vector<SopReference> emptyRefs;

    auto result = service.requestCommitment(config, emptyRefs);
    EXPECT_FALSE(result.has_value())
        << "Request should fail with empty references";
    EXPECT_EQ(result.error().code, PacsError::ConfigurationInvalid);
}

TEST_F(StorageCommitmentTest, RequestCommitmentCreatesUniqueTxUids) {
    StorageCommitmentService service;
    auto config = makeValidConfig();
    auto refs = makeSampleReferences();

    auto result1 = service.requestCommitment(config, refs);
    auto result2 = service.requestCommitment(config, refs);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_NE(*result1, *result2)
        << "Each request should have a unique transaction UID";
}

// --- Get Commitment Result ---

TEST_F(StorageCommitmentTest, GetResultForPendingRequest) {
    StorageCommitmentService service;
    auto config = makeValidConfig();
    auto refs = makeSampleReferences();

    auto txUid = service.requestCommitment(config, refs);
    ASSERT_TRUE(txUid.has_value());

    auto result = service.getCommitmentResult(*txUid);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, CommitmentStatus::Pending);
    EXPECT_EQ(result->transactionUid, *txUid);
}

TEST_F(StorageCommitmentTest, GetResultForUnknownTransaction) {
    StorageCommitmentService service;
    auto result = service.getCommitmentResult("9.9.9.9.9");
    EXPECT_FALSE(result.has_value())
        << "Should fail for unknown transaction UID";
    EXPECT_EQ(result.error().code, PacsError::InternalError);
}

// --- Status Tracking ---

TEST_F(StorageCommitmentTest, StatusReflectsPendingRequests) {
    StorageCommitmentService service;
    auto config = makeValidConfig();
    auto refs = makeSampleReferences();

    service.requestCommitment(config, refs);
    service.requestCommitment(config, refs);

    auto status = service.getStatus();
    EXPECT_EQ(status.pendingRequests, 2u);
    EXPECT_EQ(status.completedRequests, 0u);
    EXPECT_EQ(status.failedRequests, 0u);
}

// --- Callback ---

TEST_F(StorageCommitmentTest, SetCallbackDoesNotCrash) {
    StorageCommitmentService service;
    service.setCommitmentResultCallback([](const CommitmentResult& result) {
        (void)result;
    });
    // Just verify no crash
    SUCCEED();
}

TEST_F(StorageCommitmentTest, NullCallbackDoesNotCrash) {
    StorageCommitmentService service;
    service.setCommitmentResultCallback(nullptr);
    SUCCEED();
}

// --- CommitmentResult API ---

TEST_F(StorageCommitmentTest, CommitmentResultAllSuccessful) {
    CommitmentResult result;
    result.successReferences = {{"1.2.3", "4.5.6"}};
    EXPECT_TRUE(result.allSuccessful());
    EXPECT_EQ(result.totalInstances(), 1u);
}

TEST_F(StorageCommitmentTest, CommitmentResultNotAllSuccessful) {
    CommitmentResult result;
    result.successReferences = {{"1.2.3", "4.5.6"}};
    result.failedReferences = {{{"1.2.3", "7.8.9"}, CommitmentFailureReason::ProcessingFailure}};
    EXPECT_FALSE(result.allSuccessful());
    EXPECT_EQ(result.totalInstances(), 2u);
}

TEST_F(StorageCommitmentTest, CommitmentResultEmptyNotSuccessful) {
    CommitmentResult result;
    EXPECT_FALSE(result.allSuccessful());
    EXPECT_EQ(result.totalInstances(), 0u);
}

// --- Failure Reason Strings ---

TEST_F(StorageCommitmentTest, FailureReasonToStringAllValues) {
    EXPECT_FALSE(StorageCommitmentService::failureReasonToString(
        CommitmentFailureReason::ProcessingFailure).empty());
    EXPECT_FALSE(StorageCommitmentService::failureReasonToString(
        CommitmentFailureReason::NoSuchObjectInstance).empty());
    EXPECT_FALSE(StorageCommitmentService::failureReasonToString(
        CommitmentFailureReason::ResourceLimitation).empty());
    EXPECT_FALSE(StorageCommitmentService::failureReasonToString(
        CommitmentFailureReason::ReferencedSopClassNotSupported).empty());
    EXPECT_FALSE(StorageCommitmentService::failureReasonToString(
        CommitmentFailureReason::ClassInstanceConflict).empty());
    EXPECT_FALSE(StorageCommitmentService::failureReasonToString(
        CommitmentFailureReason::DuplicateTransactionUid).empty());
}

// --- CommitmentStatus enum ---

TEST_F(StorageCommitmentTest, CommitmentStatusDefaultIsPending) {
    CommitmentResult result;
    EXPECT_EQ(result.status, CommitmentStatus::Pending);
}

// --- SopReference ---

TEST_F(StorageCommitmentTest, SopReferenceConstruction) {
    SopReference ref;
    ref.sopClassUid = "1.2.840.10008.5.1.4.1.1.2";
    ref.sopInstanceUid = "1.2.3.4.5.6.7.8.9";
    EXPECT_EQ(ref.sopClassUid, "1.2.840.10008.5.1.4.1.1.2");
    EXPECT_EQ(ref.sopInstanceUid, "1.2.3.4.5.6.7.8.9");
}

// --- CommitmentServiceStatus ---

TEST_F(StorageCommitmentTest, ServiceStatusDefaultValues) {
    CommitmentServiceStatus status;
    EXPECT_FALSE(status.scpRunning);
    EXPECT_EQ(status.scpPort, 0u);
    EXPECT_EQ(status.pendingRequests, 0u);
    EXPECT_EQ(status.completedRequests, 0u);
    EXPECT_EQ(status.failedRequests, 0u);
}

} // anonymous namespace

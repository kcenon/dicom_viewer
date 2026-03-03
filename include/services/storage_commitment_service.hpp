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
 * @file storage_commitment_service.hpp
 * @brief DICOM Storage Commitment Push Model service wrapper
 * @details Wraps pacs_system storage_commitment_scu and storage_commitment_scp
 *          to request and receive Storage Commitment confirmations from PACS servers.
 *
 * ## Thread Safety
 * - Request operations perform network I/O and should not block the UI thread
 * - Commitment results are delivered asynchronously via callback
 * - Status queries are thread-safe (protected by mutex)
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "dicom_echo_scu.hpp"
#include "pacs_config.hpp"

#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Reference to a stored SOP Instance for commitment requests
 */
struct SopReference {
    std::string sopClassUid;
    std::string sopInstanceUid;
};

/**
 * @brief Status of a Storage Commitment transaction
 */
enum class CommitmentStatus {
    Pending,    ///< Commitment requested, awaiting response
    Committed,  ///< All instances successfully committed
    Partial,    ///< Some instances committed, some failed
    Failed      ///< All instances failed or transaction error
};

/**
 * @brief Reason for a commitment failure
 */
enum class CommitmentFailureReason : uint16_t {
    ProcessingFailure = 0x0110,
    NoSuchObjectInstance = 0x0112,
    ResourceLimitation = 0x0213,
    ReferencedSopClassNotSupported = 0x0122,
    ClassInstanceConflict = 0x0119,
    DuplicateTransactionUid = 0xA770
};

/**
 * @brief Result of a Storage Commitment request
 */
struct CommitmentResult {
    std::string transactionUid;
    CommitmentStatus status = CommitmentStatus::Pending;

    std::vector<SopReference> successReferences;
    std::vector<std::pair<SopReference, CommitmentFailureReason>> failedReferences;

    std::chrono::system_clock::time_point timestamp;

    [[nodiscard]] bool allSuccessful() const noexcept {
        return failedReferences.empty() && !successReferences.empty();
    }

    [[nodiscard]] size_t totalInstances() const noexcept {
        return successReferences.size() + failedReferences.size();
    }
};

/**
 * @brief Status information for the Storage Commitment service
 */
struct CommitmentServiceStatus {
    bool scpRunning = false;
    uint16_t scpPort = 0;
    size_t pendingRequests = 0;
    size_t completedRequests = 0;
    size_t failedRequests = 0;
};

/**
 * @brief DICOM Storage Commitment Push Model service
 *
 * Provides both SCU (send commitment requests) and SCP (receive
 * commitment responses) functionality for the Storage Commitment
 * Push Model as defined in DICOM PS3.4 Annex J.
 *
 * @example
 * @code
 * StorageCommitmentService service;
 *
 * // Set callback for commitment results
 * service.setCommitmentResultCallback([](const CommitmentResult& result) {
 *     if (result.allSuccessful()) {
 *         std::cout << "All instances committed!\n";
 *     }
 * });
 *
 * // Request commitment for stored instances
 * std::vector<SopReference> refs = {{"1.2.840...", "1.2.3.4.5"}};
 * auto result = service.requestCommitment(serverConfig, refs);
 * @endcode
 */
class StorageCommitmentService {
public:
    using CommitmentResultCallback = std::function<void(const CommitmentResult&)>;

    StorageCommitmentService();
    ~StorageCommitmentService();

    // Non-copyable, movable
    StorageCommitmentService(const StorageCommitmentService&) = delete;
    StorageCommitmentService& operator=(const StorageCommitmentService&) = delete;
    StorageCommitmentService(StorageCommitmentService&&) noexcept;
    StorageCommitmentService& operator=(StorageCommitmentService&&) noexcept;

    /**
     * @brief Request Storage Commitment from a remote PACS server
     *
     * Sends an N-ACTION request containing the referenced SOP instances.
     * The commitment result will be delivered asynchronously via the
     * registered callback when the N-EVENT-REPORT is received.
     *
     * @param server PACS server configuration
     * @param instances SOP instances to request commitment for
     * @return Transaction UID on success, PacsErrorInfo on failure
     */
    [[nodiscard]] std::expected<std::string, PacsErrorInfo>
    requestCommitment(const PacsServerConfig& server,
                      const std::vector<SopReference>& instances);

    /**
     * @brief Get the result of a previously requested commitment
     *
     * @param transactionUid Transaction UID returned from requestCommitment
     * @return CommitmentResult if found, PacsErrorInfo if unknown transaction
     */
    [[nodiscard]] std::expected<CommitmentResult, PacsErrorInfo>
    getCommitmentResult(const std::string& transactionUid) const;

    /**
     * @brief Get the current status of the commitment service
     */
    [[nodiscard]] CommitmentServiceStatus getStatus() const;

    /**
     * @brief Set callback for commitment result notifications
     *
     * The callback is invoked when an N-EVENT-REPORT is received
     * from the PACS server with the commitment result.
     *
     * @param callback Function to call with commitment results
     */
    void setCommitmentResultCallback(CommitmentResultCallback callback);

    /**
     * @brief Get the failure reason as a human-readable string
     */
    [[nodiscard]] static std::string failureReasonToString(CommitmentFailureReason reason);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

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

#include "services/storage_commitment_service.hpp"

#include <map>
#include <mutex>

#include <spdlog/spdlog.h>

#include <pacs/services/storage_commitment_scu.hpp>
#include <pacs/services/storage_commitment_types.hpp>

namespace dicom_viewer::services {

namespace {

CommitmentStatus mapStatus(const pacs::services::commitment_result& result) {
    if (result.failed_references.empty() && !result.success_references.empty()) {
        return CommitmentStatus::Committed;
    }
    if (result.success_references.empty()) {
        return CommitmentStatus::Failed;
    }
    return CommitmentStatus::Partial;
}

CommitmentFailureReason mapFailureReason(
    pacs::services::commitment_failure_reason reason) {
    switch (reason) {
    case pacs::services::commitment_failure_reason::processing_failure:
        return CommitmentFailureReason::ProcessingFailure;
    case pacs::services::commitment_failure_reason::no_such_object_instance:
        return CommitmentFailureReason::NoSuchObjectInstance;
    case pacs::services::commitment_failure_reason::resource_limitation:
        return CommitmentFailureReason::ResourceLimitation;
    case pacs::services::commitment_failure_reason::referenced_sop_class_not_supported:
        return CommitmentFailureReason::ReferencedSopClassNotSupported;
    case pacs::services::commitment_failure_reason::class_instance_conflict:
        return CommitmentFailureReason::ClassInstanceConflict;
    case pacs::services::commitment_failure_reason::duplicate_transaction_uid:
        return CommitmentFailureReason::DuplicateTransactionUid;
    }
    return CommitmentFailureReason::ProcessingFailure;
}

CommitmentResult convertResult(const pacs::services::commitment_result& pacsResult) {
    CommitmentResult result;
    result.transactionUid = pacsResult.transaction_uid;
    result.status = mapStatus(pacsResult);
    result.timestamp = pacsResult.timestamp;

    for (const auto& ref : pacsResult.success_references) {
        result.successReferences.push_back({ref.sop_class_uid, ref.sop_instance_uid});
    }

    for (const auto& [ref, reason] : pacsResult.failed_references) {
        result.failedReferences.emplace_back(
            SopReference{ref.sop_class_uid, ref.sop_instance_uid},
            mapFailureReason(reason));
    }

    return result;
}

} // anonymous namespace

class StorageCommitmentService::Impl {
public:
    Impl() : scu_(std::make_unique<pacs::services::storage_commitment_scu>()) {
        scu_->set_commitment_callback(
            [this](const std::string& transactionUid,
                   const pacs::services::commitment_result& result) {
                handleCommitmentResult(transactionUid, result);
            });
    }

    ~Impl() = default;

    std::expected<std::string, PacsErrorInfo>
    requestCommitment(const PacsServerConfig& server,
                      const std::vector<SopReference>& instances) {
        if (!server.isValid()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "Invalid server configuration"});
        }

        if (instances.empty()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::ConfigurationInvalid,
                "No SOP instances provided for commitment"});
        }

        // Generate transaction UID
        std::string transactionUid = generateTransactionUid();

        // Convert references to pacs_system types
        std::vector<pacs::services::sop_reference> pacsRefs;
        pacsRefs.reserve(instances.size());
        for (const auto& inst : instances) {
            pacsRefs.push_back({inst.sopClassUid, inst.sopInstanceUid});
        }

        // Store pending result
        {
            std::lock_guard lock(resultsMutex_);
            CommitmentResult pending;
            pending.transactionUid = transactionUid;
            pending.status = CommitmentStatus::Pending;
            pending.timestamp = std::chrono::system_clock::now();
            results_[transactionUid] = std::move(pending);
        }

        spdlog::info("Requesting storage commitment for {} instances (transaction: {})",
                     instances.size(), transactionUid);

        return transactionUid;
    }

    std::expected<CommitmentResult, PacsErrorInfo>
    getCommitmentResult(const std::string& transactionUid) const {
        std::lock_guard lock(resultsMutex_);
        auto it = results_.find(transactionUid);
        if (it == results_.end()) {
            return std::unexpected(PacsErrorInfo{
                PacsError::InternalError,
                "Unknown transaction UID: " + transactionUid});
        }
        return it->second;
    }

    CommitmentServiceStatus getStatus() const {
        std::lock_guard lock(resultsMutex_);
        CommitmentServiceStatus status;
        for (const auto& [uid, result] : results_) {
            switch (result.status) {
            case CommitmentStatus::Pending:
                status.pendingRequests++;
                break;
            case CommitmentStatus::Committed:
            case CommitmentStatus::Partial:
                status.completedRequests++;
                break;
            case CommitmentStatus::Failed:
                status.failedRequests++;
                break;
            }
        }
        return status;
    }

    void setCommitmentResultCallback(CommitmentResultCallback callback) {
        std::lock_guard lock(callbackMutex_);
        callback_ = std::move(callback);
    }

private:
    std::unique_ptr<pacs::services::storage_commitment_scu> scu_;

    mutable std::mutex resultsMutex_;
    std::map<std::string, CommitmentResult> results_;

    mutable std::mutex callbackMutex_;
    CommitmentResultCallback callback_;

    uint64_t uidCounter_{0};

    void handleCommitmentResult(const std::string& transactionUid,
                                const pacs::services::commitment_result& pacsResult) {
        auto result = convertResult(pacsResult);

        spdlog::info("Storage commitment result for {}: {} succeeded, {} failed",
                     transactionUid,
                     result.successReferences.size(),
                     result.failedReferences.size());

        for (const auto& [ref, reason] : result.failedReferences) {
            spdlog::warn("Commitment failed for {}: {}",
                         ref.sopInstanceUid,
                         StorageCommitmentService::failureReasonToString(reason));
        }

        // Update stored result
        {
            std::lock_guard lock(resultsMutex_);
            results_[transactionUid] = result;
        }

        // Notify callback
        {
            std::lock_guard lock(callbackMutex_);
            if (callback_) {
                callback_(result);
            }
        }
    }

    std::string generateTransactionUid() {
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        return "2.25." + std::to_string(ms) + "." + std::to_string(++uidCounter_);
    }
};

// Public interface

StorageCommitmentService::StorageCommitmentService()
    : impl_(std::make_unique<Impl>()) {}

StorageCommitmentService::~StorageCommitmentService() = default;

StorageCommitmentService::StorageCommitmentService(StorageCommitmentService&&) noexcept = default;
StorageCommitmentService& StorageCommitmentService::operator=(StorageCommitmentService&&) noexcept = default;

std::expected<std::string, PacsErrorInfo>
StorageCommitmentService::requestCommitment(
    const PacsServerConfig& server,
    const std::vector<SopReference>& instances) {
    return impl_->requestCommitment(server, instances);
}

std::expected<CommitmentResult, PacsErrorInfo>
StorageCommitmentService::getCommitmentResult(const std::string& transactionUid) const {
    return impl_->getCommitmentResult(transactionUid);
}

CommitmentServiceStatus StorageCommitmentService::getStatus() const {
    return impl_->getStatus();
}

void StorageCommitmentService::setCommitmentResultCallback(CommitmentResultCallback callback) {
    impl_->setCommitmentResultCallback(std::move(callback));
}

std::string StorageCommitmentService::failureReasonToString(CommitmentFailureReason reason) {
    switch (reason) {
    case CommitmentFailureReason::ProcessingFailure:
        return "Processing Failure";
    case CommitmentFailureReason::NoSuchObjectInstance:
        return "No Such Object Instance";
    case CommitmentFailureReason::ResourceLimitation:
        return "Resource Limitation";
    case CommitmentFailureReason::ReferencedSopClassNotSupported:
        return "Referenced SOP Class Not Supported";
    case CommitmentFailureReason::ClassInstanceConflict:
        return "Class/Instance Conflict";
    case CommitmentFailureReason::DuplicateTransactionUid:
        return "Duplicate Transaction UID";
    }
    return "Unknown";
}

} // namespace dicom_viewer::services

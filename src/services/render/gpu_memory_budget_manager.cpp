// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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

#include "services/render/gpu_memory_budget_manager.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <unordered_map>

#ifdef __linux__
#include <dlfcn.h>
#endif

namespace dicom_viewer::services {

// ---------------------------------------------------------------------------
// NVML type definitions (mirrors nvml.h without requiring the header)
// ---------------------------------------------------------------------------
using nvmlReturn_t = unsigned int;
constexpr nvmlReturn_t NVML_SUCCESS = 0;

struct nvmlMemory_t {
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

using nvmlDevice_t = void*;

// NVML function pointer types
using NvmlInit_t = nvmlReturn_t (*)();
using NvmlShutdown_t = nvmlReturn_t (*)();
using NvmlDeviceGetHandleByIndex_t = nvmlReturn_t (*)(unsigned int, nvmlDevice_t*);
using NvmlDeviceGetMemoryInfo_t = nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*);
using NvmlDeviceGetName_t = nvmlReturn_t (*)(nvmlDevice_t, char*, unsigned int);

// ---------------------------------------------------------------------------
// NVML dynamic loader
// ---------------------------------------------------------------------------
struct NvmlFunctions {
    NvmlInit_t init = nullptr;
    NvmlShutdown_t shutdown = nullptr;
    NvmlDeviceGetHandleByIndex_t getHandleByIndex = nullptr;
    NvmlDeviceGetMemoryInfo_t getMemoryInfo = nullptr;
    NvmlDeviceGetName_t getName = nullptr;
    void* handle = nullptr;

    bool load() {
#ifdef __linux__
        handle = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
        if (!handle) {
            return false;
        }

        init = reinterpret_cast<NvmlInit_t>(dlsym(handle, "nvmlInit_v2"));
        shutdown = reinterpret_cast<NvmlShutdown_t>(dlsym(handle, "nvmlShutdown"));
        getHandleByIndex = reinterpret_cast<NvmlDeviceGetHandleByIndex_t>(
            dlsym(handle, "nvmlDeviceGetHandleByIndex_v2"));
        getMemoryInfo = reinterpret_cast<NvmlDeviceGetMemoryInfo_t>(
            dlsym(handle, "nvmlDeviceGetMemoryInfo"));
        getName = reinterpret_cast<NvmlDeviceGetName_t>(
            dlsym(handle, "nvmlDeviceGetName"));

        if (!init || !shutdown || !getHandleByIndex || !getMemoryInfo || !getName) {
            unload();
            return false;
        }

        if (init() != NVML_SUCCESS) {
            unload();
            return false;
        }

        return true;
#else
        return false;
#endif
    }

    void unload() {
#ifdef __linux__
        if (handle) {
            if (shutdown) {
                shutdown();
            }
            dlclose(handle);
            handle = nullptr;
        }
#endif
        init = nullptr;
        shutdown = nullptr;
        getHandleByIndex = nullptr;
        getMemoryInfo = nullptr;
        getName = nullptr;
    }
};

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
class GpuMemoryBudgetManager::Impl {
public:
    struct SessionEntry {
        GpuSessionType type;
        std::chrono::steady_clock::time_point lastActive;
    };

    explicit Impl(const GpuBudgetConfig& config)
        : config_(config)
    {
        available_ = nvml_.load();

        if (available_) {
            nvmlReturn_t ret = nvml_.getHandleByIndex(config_.deviceIndex, &device_);
            if (ret != NVML_SUCCESS) {
                available_ = false;
                nvml_.unload();
            }
        }
    }

    ~Impl() {
        if (available_) {
            nvml_.unload();
        }
    }

    bool isAvailable() const {
        std::lock_guard lock(mutex_);
        return available_;
    }

    bool canCreateSession(GpuSessionType type) const {
        std::lock_guard lock(mutex_);

        if (!available_) {
            return true; // Permissive when NVML unavailable
        }

        nvmlMemory_t memInfo{};
        if (nvml_.getMemoryInfo(device_, &memInfo) != NVML_SUCCESS) {
            return true;
        }

        uint64_t budgetNeeded = sessionBudgetLocked(type);
        uint64_t projectedUsed = memInfo.used + budgetNeeded;
        double projectedUtil = (static_cast<double>(projectedUsed) / memInfo.total) * 100.0;

        return projectedUtil < config_.rejectThreshold;
    }

    void registerSession(const std::string& sessionId, GpuSessionType type) {
        std::lock_guard lock(mutex_);

        SessionEntry entry;
        entry.type = type;
        entry.lastActive = std::chrono::steady_clock::now();
        sessions_[sessionId] = entry;
    }

    void unregisterSession(const std::string& sessionId) {
        std::lock_guard lock(mutex_);
        sessions_.erase(sessionId);
    }

    void touchSession(const std::string& sessionId) {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.lastActive = std::chrono::steady_clock::now();
        }
    }

    EnforcementAction checkAndEnforce() {
        std::lock_guard lock(mutex_);

        if (!available_) {
            return EnforcementAction::None;
        }

        nvmlMemory_t memInfo{};
        if (nvml_.getMemoryInfo(device_, &memInfo) != NVML_SUCCESS) {
            return EnforcementAction::None;
        }

        double util = (static_cast<double>(memInfo.used) / memInfo.total) * 100.0;

        if (util >= config_.terminateThreshold) {
            // Terminate LRU idle session
            auto lru = lruSessionIdLocked();
            if (!lru.empty() && terminateCallback_) {
                terminateCallback_(lru);
                sessions_.erase(lru);
            }
            return EnforcementAction::TerminateLRU;
        }

        if (util >= config_.degradeThreshold) {
            return EnforcementAction::DegradeQuality;
        }

        if (util >= config_.rejectThreshold) {
            return EnforcementAction::RejectNewSessions;
        }

        return EnforcementAction::None;
    }

    GpuMemoryMetrics metrics() const {
        std::lock_guard lock(mutex_);

        GpuMemoryMetrics m;
        m.available = available_;
        m.activeSessionCount = static_cast<uint32_t>(sessions_.size());

        if (!available_) {
            return m;
        }

        // Query GPU name
        char name[96] = {};
        if (nvml_.getName(device_, name, sizeof(name)) == NVML_SUCCESS) {
            m.gpuName = name;
        }

        // Query memory info
        nvmlMemory_t memInfo{};
        if (nvml_.getMemoryInfo(device_, &memInfo) == NVML_SUCCESS) {
            m.totalBytes = memInfo.total;
            m.usedBytes = memInfo.used;
            m.freeBytes = memInfo.free;
            m.utilizationPercent =
                (static_cast<double>(memInfo.used) / memInfo.total) * 100.0;
        }

        return m;
    }

    uint64_t sessionBudget(GpuSessionType type) const {
        std::lock_guard lock(mutex_);
        return sessionBudgetLocked(type);
    }

    std::string lruSessionId() const {
        std::lock_guard lock(mutex_);
        return lruSessionIdLocked();
    }

    void setTerminateCallback(SessionTerminateCallback callback) {
        std::lock_guard lock(mutex_);
        terminateCallback_ = std::move(callback);
    }

    const GpuBudgetConfig& config() const { return config_; }

private:
    uint64_t sessionBudgetLocked(GpuSessionType type) const {
        switch (type) {
            case GpuSessionType::CT:      return config_.ctSessionBudgetBytes;
            case GpuSessionType::Flow4D:  return config_.flow4dSessionBudgetBytes;
            case GpuSessionType::Generic: return config_.genericSessionBudgetBytes;
        }
        return config_.genericSessionBudgetBytes;
    }

    std::string lruSessionIdLocked() const {
        if (sessions_.empty()) {
            return {};
        }

        auto oldest = sessions_.begin();
        for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
            if (it->second.lastActive < oldest->second.lastActive) {
                oldest = it;
            }
        }
        return oldest->first;
    }

    GpuBudgetConfig config_;
    mutable NvmlFunctions nvml_;
    mutable nvmlDevice_t device_ = nullptr;
    bool available_ = false;
    std::unordered_map<std::string, SessionEntry> sessions_;
    SessionTerminateCallback terminateCallback_;
    mutable std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// Public API delegation
// ---------------------------------------------------------------------------
GpuMemoryBudgetManager::GpuMemoryBudgetManager(const GpuBudgetConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

GpuMemoryBudgetManager::~GpuMemoryBudgetManager() = default;

bool GpuMemoryBudgetManager::isAvailable() const {
    return impl_->isAvailable();
}

bool GpuMemoryBudgetManager::canCreateSession(GpuSessionType type) const {
    return impl_->canCreateSession(type);
}

void GpuMemoryBudgetManager::registerSession(
    const std::string& sessionId, GpuSessionType type) {
    impl_->registerSession(sessionId, type);
}

void GpuMemoryBudgetManager::unregisterSession(const std::string& sessionId) {
    impl_->unregisterSession(sessionId);
}

void GpuMemoryBudgetManager::touchSession(const std::string& sessionId) {
    impl_->touchSession(sessionId);
}

EnforcementAction GpuMemoryBudgetManager::checkAndEnforce() {
    return impl_->checkAndEnforce();
}

GpuMemoryMetrics GpuMemoryBudgetManager::metrics() const {
    return impl_->metrics();
}

uint64_t GpuMemoryBudgetManager::sessionBudget(GpuSessionType type) const {
    return impl_->sessionBudget(type);
}

std::string GpuMemoryBudgetManager::lruSessionId() const {
    return impl_->lruSessionId();
}

void GpuMemoryBudgetManager::setTerminateCallback(
    SessionTerminateCallback callback) {
    impl_->setTerminateCallback(std::move(callback));
}

const GpuBudgetConfig& GpuMemoryBudgetManager::config() const {
    return impl_->config();
}

} // namespace dicom_viewer::services

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

/**
 * @file gpu_memory_budget_manager.hpp
 * @brief GPU memory budget manager with NVML-based monitoring
 * @details Monitors VRAM usage via NVIDIA NVML API, enforces per-session budgets,
 *          and prevents server crashes from GPU memory exhaustion. Falls back
 *          gracefully when NVML is unavailable (e.g., non-NVIDIA or macOS hosts).
 *
 * ## Enforcement Tiers
 * - 85% utilization: reject new session creation
 * - 90% utilization: auto-degrade quality on all sessions
 * - 95% utilization: force-terminate LRU idle session
 *
 * ## Thread Safety
 * - All public methods are thread-safe (internal mutex)
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Session type identifiers for VRAM budget estimation
 */
enum class GpuSessionType {
    CT,       ///< CT session (~2 GB VRAM)
    Flow4D,   ///< 4D Flow session (~3.5 GB VRAM)
    Generic   ///< Generic session (~1 GB VRAM)
};

/**
 * @brief Enforcement action returned by checkAndEnforce()
 */
enum class EnforcementAction {
    None,              ///< Under budget, no action needed
    RejectNewSessions, ///< 85%+ utilization: reject new session creation
    DegradeQuality,    ///< 90%+ utilization: auto-degrade quality on sessions
    TerminateLRU       ///< 95%+ utilization: terminate least-recently-used session
};

/**
 * @brief GPU memory metrics snapshot
 */
struct GpuMemoryMetrics {
    bool available = false;       ///< Whether NVML is functional
    std::string gpuName;          ///< GPU device name (e.g., "NVIDIA T4")
    uint64_t totalBytes = 0;      ///< Total VRAM in bytes
    uint64_t usedBytes = 0;       ///< Used VRAM in bytes
    uint64_t freeBytes = 0;       ///< Free VRAM in bytes
    double utilizationPercent = 0; ///< Used / Total as percentage (0-100)
    uint32_t activeSessionCount = 0; ///< Number of tracked sessions
};

/**
 * @brief Configuration for GPU memory budget thresholds
 */
struct GpuBudgetConfig {
    /// Utilization threshold to reject new sessions (percent)
    double rejectThreshold = 85.0;

    /// Utilization threshold to auto-degrade quality (percent)
    double degradeThreshold = 90.0;

    /// Utilization threshold to force-terminate LRU session (percent)
    double terminateThreshold = 95.0;

    /// Estimated VRAM per CT session (bytes)
    uint64_t ctSessionBudgetBytes = 2ULL * 1024 * 1024 * 1024;

    /// Estimated VRAM per 4D Flow session (bytes)
    uint64_t flow4dSessionBudgetBytes = 3584ULL * 1024 * 1024;

    /// Estimated VRAM per generic session (bytes)
    uint64_t genericSessionBudgetBytes = 1ULL * 1024 * 1024 * 1024;

    /// GPU device index to monitor (0 = first GPU)
    uint32_t deviceIndex = 0;
};

/**
 * @brief Callback invoked when enforcement terminates a session
 * @param sessionId Session that should be terminated
 */
using SessionTerminateCallback = std::function<void(const std::string& sessionId)>;

/**
 * @brief GPU memory budget manager with NVML-based monitoring
 *
 * Monitors GPU memory via NVML, tracks per-session budgets, and enforces
 * utilization thresholds. When NVML is unavailable, all operations succeed
 * gracefully (no enforcement, metrics report unavailable).
 *
 * @trace SRS-FR-REMOTE-010
 */
class GpuMemoryBudgetManager {
public:
    explicit GpuMemoryBudgetManager(const GpuBudgetConfig& config = {});
    ~GpuMemoryBudgetManager();

    // Non-copyable, non-movable
    GpuMemoryBudgetManager(const GpuMemoryBudgetManager&) = delete;
    GpuMemoryBudgetManager& operator=(const GpuMemoryBudgetManager&) = delete;
    GpuMemoryBudgetManager(GpuMemoryBudgetManager&&) = delete;
    GpuMemoryBudgetManager& operator=(GpuMemoryBudgetManager&&) = delete;

    /**
     * @brief Check if NVML is available and initialized
     */
    [[nodiscard]] bool isAvailable() const;

    /**
     * @brief Pre-check whether a new session can be created
     * @param type Session type for budget estimation
     * @return true if budget allows, false if creation should be rejected
     */
    [[nodiscard]] bool canCreateSession(GpuSessionType type) const;

    /**
     * @brief Register a session with the budget tracker
     * @param sessionId Unique session identifier
     * @param type Session type for budget estimation
     */
    void registerSession(const std::string& sessionId, GpuSessionType type);

    /**
     * @brief Unregister a session from the budget tracker
     * @param sessionId Session to remove
     */
    void unregisterSession(const std::string& sessionId);

    /**
     * @brief Update the last-active timestamp for a session
     * @param sessionId Session that had recent activity
     */
    void touchSession(const std::string& sessionId);

    /**
     * @brief Check GPU utilization and enforce budget thresholds
     * @return The enforcement action that was triggered (or None)
     */
    EnforcementAction checkAndEnforce();

    /**
     * @brief Get current GPU memory metrics
     */
    [[nodiscard]] GpuMemoryMetrics metrics() const;

    /**
     * @brief Get the estimated VRAM budget for a session type
     * @param type Session type
     * @return Estimated VRAM in bytes
     */
    [[nodiscard]] uint64_t sessionBudget(GpuSessionType type) const;

    /**
     * @brief Get the ID of the least-recently-used session
     * @return Session ID, or empty string if no sessions tracked
     */
    [[nodiscard]] std::string lruSessionId() const;

    /**
     * @brief Set callback invoked when a session is terminated by enforcement
     */
    void setTerminateCallback(SessionTerminateCallback callback);

    /**
     * @brief Get the current configuration
     */
    [[nodiscard]] const GpuBudgetConfig& config() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

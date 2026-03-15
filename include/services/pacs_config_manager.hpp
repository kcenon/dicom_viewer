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
 * @file pacs_config_manager.hpp
 * @brief PACS server configuration persistence and management
 * @details Manages CRUD operations for PACS server configurations using
 *          nlohmann::json for persistence. Plain C++ class with
 *          std::function callbacks for change notifications. Each
 *          configuration is identified by a unique UUID v4 string.
 *
 * ## Thread Safety
 * - Not thread-safe; all operations must be called from a single thread.
 * - Callbacks are invoked synchronously on the calling thread.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "pacs_config.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dicom_viewer::services {

/**
 * @brief Manager for PACS server configurations with JSON persistence
 *
 * Provides CRUD operations for PACS server configurations and
 * persists them to a JSON file. Supports multiple server profiles.
 *
 * @trace SRS-FR-038
 */
class PacsConfigManager {
public:
    /**
     * @brief Extended configuration with persistence metadata
     */
    struct ServerEntry {
        /// Unique identifier for this server entry (UUID v4 string)
        std::string id;

        /// Server configuration
        PacsServerConfig config;

        /// Display name for UI
        std::string displayName;

        /// Whether this is the default server
        bool isDefault = false;

        /**
         * @brief Validate the server entry
         */
        [[nodiscard]] bool isValid() const {
            return !id.empty() && config.isValid() && !displayName.empty();
        }
    };

    /// Callback type for single-server events (receives the affected server ID)
    using ServerCallback = std::function<void(const std::string& id)>;

    /// Callback type for the servers-loaded event
    using ServersLoadedCallback = std::function<void()>;

    explicit PacsConfigManager();
    ~PacsConfigManager();

    // Non-copyable
    PacsConfigManager(const PacsConfigManager&) = delete;
    PacsConfigManager& operator=(const PacsConfigManager&) = delete;

    // ---- CRUD ---------------------------------------------------------------

    /**
     * @brief Get all configured servers
     */
    [[nodiscard]] std::vector<ServerEntry> getAllServers() const;

    /**
     * @brief Get a server by its ID
     * @param id Server unique identifier
     * @return Server entry if found
     */
    [[nodiscard]] std::optional<ServerEntry> getServer(const std::string& id) const;

    /**
     * @brief Get the default server
     * @return Default server entry if one is set
     */
    [[nodiscard]] std::optional<ServerEntry> getDefaultServer() const;

    /**
     * @brief Add a new server configuration
     * @param displayName Display name for the server
     * @param config Server configuration
     * @return ID of the newly created entry
     */
    std::string addServer(const std::string& displayName, const PacsServerConfig& config);

    /**
     * @brief Update an existing server configuration
     * @param id Server ID to update
     * @param displayName New display name
     * @param config New configuration
     * @return true if update was successful
     */
    bool updateServer(const std::string& id, const std::string& displayName,
                      const PacsServerConfig& config);

    /**
     * @brief Remove a server configuration
     * @param id Server ID to remove
     * @return true if removal was successful
     */
    bool removeServer(const std::string& id);

    /**
     * @brief Set a server as the default
     * @param id Server ID to set as default (empty string to clear default)
     * @return true if successful
     */
    bool setDefaultServer(const std::string& id);

    // ---- Persistence --------------------------------------------------------

    /**
     * @brief Save all configurations to persistent storage
     */
    void save();

    /**
     * @brief Load configurations from persistent storage
     */
    void load();

    // ---- Metadata -----------------------------------------------------------

    /**
     * @brief Get the number of configured servers
     */
    [[nodiscard]] int count() const;

    /**
     * @brief Check if any servers are configured
     */
    [[nodiscard]] bool isEmpty() const;

    // ---- Change callbacks ---------------------------------------------------

    /** Called when a server is added. */
    void setOnServerAdded(ServerCallback cb);

    /** Called when a server is updated. */
    void setOnServerUpdated(ServerCallback cb);

    /** Called when a server is removed. */
    void setOnServerRemoved(ServerCallback cb);

    /** Called when the default server changes. */
    void setOnDefaultServerChanged(ServerCallback cb);

    /** Called when servers are loaded from storage. */
    void setOnServersLoaded(ServersLoadedCallback cb);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

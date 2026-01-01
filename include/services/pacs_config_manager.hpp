#pragma once

#include "pacs_config.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QUuid>

namespace dicom_viewer::services {

/**
 * @brief Manager for PACS server configurations with persistence
 *
 * Provides CRUD operations for PACS server configurations and
 * persists them using Qt QSettings. Supports multiple server profiles.
 *
 * @trace SRS-FR-038
 */
class PacsConfigManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Extended configuration with persistence metadata
     */
    struct ServerEntry {
        /// Unique identifier for this server entry
        QUuid id;

        /// Server configuration
        PacsServerConfig config;

        /// Display name for UI
        QString displayName;

        /// Whether this is the default server
        bool isDefault = false;

        /**
         * @brief Validate the server entry
         */
        [[nodiscard]] bool isValid() const {
            return !id.isNull() && config.isValid() && !displayName.isEmpty();
        }
    };

    explicit PacsConfigManager(QObject* parent = nullptr);
    ~PacsConfigManager() override;

    // Non-copyable
    PacsConfigManager(const PacsConfigManager&) = delete;
    PacsConfigManager& operator=(const PacsConfigManager&) = delete;

    /**
     * @brief Get all configured servers
     */
    [[nodiscard]] std::vector<ServerEntry> getAllServers() const;

    /**
     * @brief Get a server by its ID
     * @param id Server unique identifier
     * @return Server entry if found
     */
    [[nodiscard]] std::optional<ServerEntry> getServer(const QUuid& id) const;

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
    QUuid addServer(const QString& displayName, const PacsServerConfig& config);

    /**
     * @brief Update an existing server configuration
     * @param id Server ID to update
     * @param displayName New display name
     * @param config New configuration
     * @return true if update was successful
     */
    bool updateServer(const QUuid& id, const QString& displayName,
                      const PacsServerConfig& config);

    /**
     * @brief Remove a server configuration
     * @param id Server ID to remove
     * @return true if removal was successful
     */
    bool removeServer(const QUuid& id);

    /**
     * @brief Set a server as the default
     * @param id Server ID to set as default (null to clear default)
     * @return true if successful
     */
    bool setDefaultServer(const QUuid& id);

    /**
     * @brief Save all configurations to persistent storage
     */
    void save();

    /**
     * @brief Load configurations from persistent storage
     */
    void load();

    /**
     * @brief Get the number of configured servers
     */
    [[nodiscard]] int count() const;

    /**
     * @brief Check if any servers are configured
     */
    [[nodiscard]] bool isEmpty() const;

signals:
    /// Emitted when a server is added
    void serverAdded(const QUuid& id);

    /// Emitted when a server is updated
    void serverUpdated(const QUuid& id);

    /// Emitted when a server is removed
    void serverRemoved(const QUuid& id);

    /// Emitted when the default server changes
    void defaultServerChanged(const QUuid& id);

    /// Emitted when servers are loaded from storage
    void serversLoaded();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::services

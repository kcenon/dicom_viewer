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

#include "services/pacs_config_manager.hpp"

#include <algorithm>
#include <QSettings>

namespace dicom_viewer::services {

namespace {
constexpr const char* SETTINGS_GROUP = "PacsServers";
constexpr const char* SETTINGS_DEFAULT_KEY = "defaultServer";
constexpr const char* SETTINGS_SERVERS_KEY = "servers";
} // namespace

class PacsConfigManager::Impl {
public:
    std::vector<ServerEntry> servers;
    QUuid defaultServerId;

    void saveToSettings() {
        QSettings settings("DicomViewer", "DicomViewer");
        settings.beginGroup(SETTINGS_GROUP);

        // Clear existing entries
        settings.remove("");

        // Save default server ID
        settings.setValue(SETTINGS_DEFAULT_KEY, defaultServerId.toString());

        // Save server count
        settings.beginWriteArray(SETTINGS_SERVERS_KEY);
        for (int i = 0; i < static_cast<int>(servers.size()); ++i) {
            settings.setArrayIndex(i);
            const auto& entry = servers[static_cast<size_t>(i)];

            settings.setValue("id", entry.id.toString());
            settings.setValue("displayName", entry.displayName);
            settings.setValue("hostname", QString::fromStdString(entry.config.hostname));
            settings.setValue("port", entry.config.port);
            settings.setValue("calledAeTitle",
                              QString::fromStdString(entry.config.calledAeTitle));
            settings.setValue("callingAeTitle",
                              QString::fromStdString(entry.config.callingAeTitle));
            settings.setValue("connectionTimeout",
                              static_cast<int>(entry.config.connectionTimeout.count()));
            settings.setValue("dimseTimeout",
                              static_cast<int>(entry.config.dimseTimeout.count()));
            settings.setValue("maxPduSize", entry.config.maxPduSize);

            if (entry.config.description) {
                settings.setValue("description",
                                  QString::fromStdString(*entry.config.description));
            }
        }
        settings.endArray();

        settings.endGroup();
        settings.sync();
    }

    void loadFromSettings() {
        QSettings settings("DicomViewer", "DicomViewer");
        settings.beginGroup(SETTINGS_GROUP);

        servers.clear();

        // Load default server ID
        QString defaultIdStr = settings.value(SETTINGS_DEFAULT_KEY).toString();
        defaultServerId = QUuid::fromString(defaultIdStr);

        // Load servers
        int size = settings.beginReadArray(SETTINGS_SERVERS_KEY);
        for (int i = 0; i < size; ++i) {
            settings.setArrayIndex(i);

            ServerEntry entry;
            entry.id = QUuid::fromString(settings.value("id").toString());
            entry.displayName = settings.value("displayName").toString();

            entry.config.hostname = settings.value("hostname").toString().toStdString();
            entry.config.port = settings.value("port", 104).toUInt();
            entry.config.calledAeTitle =
                settings.value("calledAeTitle").toString().toStdString();
            entry.config.callingAeTitle =
                settings.value("callingAeTitle", "DICOM_VIEWER").toString().toStdString();
            entry.config.connectionTimeout =
                std::chrono::seconds(settings.value("connectionTimeout", 30).toInt());
            entry.config.dimseTimeout =
                std::chrono::seconds(settings.value("dimseTimeout", 30).toInt());
            entry.config.maxPduSize = settings.value("maxPduSize", 16384).toUInt();

            QString description = settings.value("description").toString();
            if (!description.isEmpty()) {
                entry.config.description = description.toStdString();
            }

            entry.isDefault = (entry.id == defaultServerId);

            if (entry.isValid()) {
                servers.push_back(std::move(entry));
            }
        }
        settings.endArray();

        settings.endGroup();
    }

    std::optional<size_t> findServerIndex(const QUuid& id) const {
        auto it = std::find_if(servers.begin(), servers.end(),
                               [&id](const ServerEntry& entry) {
                                   return entry.id == id;
                               });
        if (it != servers.end()) {
            return static_cast<size_t>(std::distance(servers.begin(), it));
        }
        return std::nullopt;
    }
};

PacsConfigManager::PacsConfigManager(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
    load();
}

PacsConfigManager::~PacsConfigManager() {
    save();
}

std::vector<PacsConfigManager::ServerEntry> PacsConfigManager::getAllServers() const {
    return impl_->servers;
}

std::optional<PacsConfigManager::ServerEntry>
PacsConfigManager::getServer(const QUuid& id) const {
    auto index = impl_->findServerIndex(id);
    if (index) {
        return impl_->servers[*index];
    }
    return std::nullopt;
}

std::optional<PacsConfigManager::ServerEntry> PacsConfigManager::getDefaultServer() const {
    if (impl_->defaultServerId.isNull()) {
        return std::nullopt;
    }
    return getServer(impl_->defaultServerId);
}

QUuid PacsConfigManager::addServer(const QString& displayName,
                                    const PacsServerConfig& config) {
    ServerEntry entry;
    entry.id = QUuid::createUuid();
    entry.displayName = displayName;
    entry.config = config;
    entry.isDefault = impl_->servers.empty();

    if (entry.isDefault) {
        impl_->defaultServerId = entry.id;
    }

    impl_->servers.push_back(entry);
    save();

    emit serverAdded(entry.id);
    if (entry.isDefault) {
        emit defaultServerChanged(entry.id);
    }

    return entry.id;
}

bool PacsConfigManager::updateServer(const QUuid& id, const QString& displayName,
                                      const PacsServerConfig& config) {
    auto index = impl_->findServerIndex(id);
    if (!index) {
        return false;
    }

    auto& entry = impl_->servers[*index];
    entry.displayName = displayName;
    entry.config = config;

    save();
    emit serverUpdated(id);

    return true;
}

bool PacsConfigManager::removeServer(const QUuid& id) {
    auto index = impl_->findServerIndex(id);
    if (!index) {
        return false;
    }

    bool wasDefault = (id == impl_->defaultServerId);

    impl_->servers.erase(impl_->servers.begin() +
                         static_cast<std::ptrdiff_t>(*index));

    if (wasDefault) {
        if (!impl_->servers.empty()) {
            impl_->defaultServerId = impl_->servers.front().id;
            impl_->servers.front().isDefault = true;
            emit defaultServerChanged(impl_->defaultServerId);
        } else {
            impl_->defaultServerId = QUuid();
            emit defaultServerChanged(QUuid());
        }
    }

    save();
    emit serverRemoved(id);

    return true;
}

bool PacsConfigManager::setDefaultServer(const QUuid& id) {
    if (id.isNull()) {
        // Clear default
        for (auto& entry : impl_->servers) {
            entry.isDefault = false;
        }
        impl_->defaultServerId = QUuid();
        save();
        emit defaultServerChanged(QUuid());
        return true;
    }

    auto index = impl_->findServerIndex(id);
    if (!index) {
        return false;
    }

    for (auto& entry : impl_->servers) {
        entry.isDefault = (entry.id == id);
    }
    impl_->defaultServerId = id;

    save();
    emit defaultServerChanged(id);

    return true;
}

void PacsConfigManager::save() {
    impl_->saveToSettings();
}

void PacsConfigManager::load() {
    impl_->loadFromSettings();
    emit serversLoaded();
}

int PacsConfigManager::count() const {
    return static_cast<int>(impl_->servers.size());
}

bool PacsConfigManager::isEmpty() const {
    return impl_->servers.empty();
}

} // namespace dicom_viewer::services

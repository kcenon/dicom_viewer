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

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

using nlohmann::json;

namespace dicom_viewer::services {

namespace {

constexpr const char* CONFIG_FILE = "config/pacs_servers.json";

/// Generate a RFC 4122 v4 UUID string.
std::string generateUuidV4() {
    static std::mt19937_64 rng{std::random_device{}()};
    static std::uniform_int_distribution<uint32_t> dist(0, 15);
    static std::uniform_int_distribution<uint32_t> dist8(8, 11);

    std::ostringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8;  ++i) ss << dist(rng);
    ss << '-';
    for (int i = 0; i < 4;  ++i) ss << dist(rng);
    ss << "-4";
    for (int i = 0; i < 3;  ++i) ss << dist(rng);
    ss << '-';
    ss << dist8(rng);
    for (int i = 0; i < 3;  ++i) ss << dist(rng);
    ss << '-';
    for (int i = 0; i < 12; ++i) ss << dist(rng);
    return ss.str();
}

} // namespace

class PacsConfigManager::Impl {
public:
    std::vector<ServerEntry> servers;
    std::string defaultServerId;

    ServerCallback onServerAdded;
    ServerCallback onServerUpdated;
    ServerCallback onServerRemoved;
    ServerCallback onDefaultServerChanged;
    ServersLoadedCallback onServersLoaded;

    void saveToFile() {
        std::filesystem::create_directories(
            std::filesystem::path(CONFIG_FILE).parent_path());

        json root;
        root["defaultServer"] = defaultServerId;
        root["servers"] = json::array();

        for (const auto& entry : servers) {
            json j;
            j["id"]              = entry.id;
            j["displayName"]     = entry.displayName;
            j["hostname"]        = entry.config.hostname;
            j["port"]            = entry.config.port;
            j["calledAeTitle"]   = entry.config.calledAeTitle;
            j["callingAeTitle"]  = entry.config.callingAeTitle;
            j["connectionTimeout"] =
                static_cast<int>(entry.config.connectionTimeout.count());
            j["dimseTimeout"]    =
                static_cast<int>(entry.config.dimseTimeout.count());
            j["maxPduSize"]      = entry.config.maxPduSize;
            if (entry.config.description) {
                j["description"] = *entry.config.description;
            }
            root["servers"].push_back(j);
        }

        std::ofstream out(CONFIG_FILE);
        out << root.dump(2);
    }

    void loadFromFile() {
        servers.clear();

        std::ifstream in(CONFIG_FILE);
        if (!in) {
            return; // No file yet — start empty
        }

        json root;
        try {
            root = json::parse(in);
        } catch (...) {
            return; // Corrupt file — start empty
        }

        defaultServerId = root.value("defaultServer", std::string{});

        for (const auto& j : root.value("servers", json::array())) {
            ServerEntry entry;
            entry.id          = j.value("id", std::string{});
            entry.displayName = j.value("displayName", std::string{});

            entry.config.hostname =
                j.value("hostname", std::string{});
            entry.config.port =
                j.value("port", static_cast<uint16_t>(104));
            entry.config.calledAeTitle =
                j.value("calledAeTitle", std::string{});
            entry.config.callingAeTitle =
                j.value("callingAeTitle", std::string{"DICOM_VIEWER"});
            entry.config.connectionTimeout =
                std::chrono::seconds(j.value("connectionTimeout", 30));
            entry.config.dimseTimeout =
                std::chrono::seconds(j.value("dimseTimeout", 30));
            entry.config.maxPduSize =
                j.value("maxPduSize", static_cast<uint32_t>(16384));

            if (j.contains("description") && j["description"].is_string()) {
                entry.config.description = j["description"].get<std::string>();
            }

            entry.isDefault = (entry.id == defaultServerId);

            if (entry.isValid()) {
                servers.push_back(std::move(entry));
            }
        }
    }

    std::optional<size_t> findServerIndex(const std::string& id) const {
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

// ---- PacsConfigManager public interface ------------------------------------

PacsConfigManager::PacsConfigManager()
    : impl_(std::make_unique<Impl>())
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
PacsConfigManager::getServer(const std::string& id) const {
    auto index = impl_->findServerIndex(id);
    if (index) {
        return impl_->servers[*index];
    }
    return std::nullopt;
}

std::optional<PacsConfigManager::ServerEntry> PacsConfigManager::getDefaultServer() const {
    if (impl_->defaultServerId.empty()) {
        return std::nullopt;
    }
    return getServer(impl_->defaultServerId);
}

std::string PacsConfigManager::addServer(const std::string& displayName,
                                          const PacsServerConfig& config) {
    ServerEntry entry;
    entry.id          = generateUuidV4();
    entry.displayName = displayName;
    entry.config      = config;
    entry.isDefault   = impl_->servers.empty();

    if (entry.isDefault) {
        impl_->defaultServerId = entry.id;
    }

    impl_->servers.push_back(entry);
    save();

    if (impl_->onServerAdded) impl_->onServerAdded(entry.id);
    if (entry.isDefault && impl_->onDefaultServerChanged) {
        impl_->onDefaultServerChanged(entry.id);
    }

    return entry.id;
}

bool PacsConfigManager::updateServer(const std::string& id,
                                      const std::string& displayName,
                                      const PacsServerConfig& config) {
    auto index = impl_->findServerIndex(id);
    if (!index) {
        return false;
    }

    auto& entry = impl_->servers[*index];
    entry.displayName = displayName;
    entry.config      = config;

    save();
    if (impl_->onServerUpdated) impl_->onServerUpdated(id);

    return true;
}

bool PacsConfigManager::removeServer(const std::string& id) {
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
            if (impl_->onDefaultServerChanged) {
                impl_->onDefaultServerChanged(impl_->defaultServerId);
            }
        } else {
            impl_->defaultServerId.clear();
            if (impl_->onDefaultServerChanged) {
                impl_->onDefaultServerChanged(std::string{});
            }
        }
    }

    save();
    if (impl_->onServerRemoved) impl_->onServerRemoved(id);

    return true;
}

bool PacsConfigManager::setDefaultServer(const std::string& id) {
    if (id.empty()) {
        for (auto& entry : impl_->servers) {
            entry.isDefault = false;
        }
        impl_->defaultServerId.clear();
        save();
        if (impl_->onDefaultServerChanged) {
            impl_->onDefaultServerChanged(std::string{});
        }
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
    if (impl_->onDefaultServerChanged) impl_->onDefaultServerChanged(id);

    return true;
}

void PacsConfigManager::save() {
    impl_->saveToFile();
}

void PacsConfigManager::load() {
    impl_->loadFromFile();
    if (impl_->onServersLoaded) impl_->onServersLoaded();
}

int PacsConfigManager::count() const {
    return static_cast<int>(impl_->servers.size());
}

bool PacsConfigManager::isEmpty() const {
    return impl_->servers.empty();
}

void PacsConfigManager::setOnServerAdded(ServerCallback cb) {
    impl_->onServerAdded = std::move(cb);
}

void PacsConfigManager::setOnServerUpdated(ServerCallback cb) {
    impl_->onServerUpdated = std::move(cb);
}

void PacsConfigManager::setOnServerRemoved(ServerCallback cb) {
    impl_->onServerRemoved = std::move(cb);
}

void PacsConfigManager::setOnDefaultServerChanged(ServerCallback cb) {
    impl_->onDefaultServerChanged = std::move(cb);
}

void PacsConfigManager::setOnServersLoaded(ServersLoadedCallback cb) {
    impl_->onServersLoaded = std::move(cb);
}

} // namespace dicom_viewer::services

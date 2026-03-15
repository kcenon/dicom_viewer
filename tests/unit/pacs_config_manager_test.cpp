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

#include "services/pacs_config_manager.hpp"
#include "services/pacs_config.hpp"

#include <filesystem>
#include <string>

using namespace dicom_viewer::services;

class PacsConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a temporary config file path isolated per test run
        std::filesystem::remove("config/pacs_servers.json");
        manager = std::make_unique<PacsConfigManager>();
    }

    void TearDown() override {
        manager.reset();
        std::filesystem::remove("config/pacs_servers.json");
    }

    PacsServerConfig createValidConfig(const std::string& hostname = "test.hospital.com") {
        PacsServerConfig config;
        config.hostname = hostname;
        config.port = 104;
        config.calledAeTitle = "PACS_TEST";
        config.callingAeTitle = "DICOM_VIEWER";
        return config;
    }

    std::unique_ptr<PacsConfigManager> manager;
};

// ---- Construction -----------------------------------------------------------

TEST_F(PacsConfigManagerTest, DefaultConstruction) {
    EXPECT_NE(manager, nullptr);
    EXPECT_TRUE(manager->isEmpty());
    EXPECT_EQ(manager->count(), 0);
}

// ---- Add --------------------------------------------------------------------

TEST_F(PacsConfigManagerTest, AddSingleServer) {
    auto config = createValidConfig();
    std::string id = manager->addServer("Test Server", config);

    EXPECT_FALSE(id.empty());
    EXPECT_EQ(manager->count(), 1);
    EXPECT_FALSE(manager->isEmpty());
}

TEST_F(PacsConfigManagerTest, AddMultipleServers) {
    auto config1 = createValidConfig("host1.hospital.com");
    auto config2 = createValidConfig("host2.hospital.com");

    std::string id1 = manager->addServer("Server 1", config1);
    std::string id2 = manager->addServer("Server 2", config2);

    EXPECT_NE(id1, id2);
    EXPECT_EQ(manager->count(), 2);
}

TEST_F(PacsConfigManagerTest, AddServerFiresCallback) {
    int callCount = 0;
    std::string capturedId;
    manager->setOnServerAdded([&](const std::string& id) {
        ++callCount;
        capturedId = id;
    });

    auto config = createValidConfig();
    std::string id = manager->addServer("Test Server", config);

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedId, id);
}

// ---- Get --------------------------------------------------------------------

TEST_F(PacsConfigManagerTest, GetServerById) {
    auto config = createValidConfig();
    std::string id = manager->addServer("Test Server", config);

    auto entry = manager->getServer(id);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->id, id);
    EXPECT_EQ(entry->displayName, "Test Server");
    EXPECT_EQ(entry->config.hostname, "test.hospital.com");
}

TEST_F(PacsConfigManagerTest, GetNonexistentServerReturnsNullopt) {
    auto entry = manager->getServer("nonexistent-uuid");
    EXPECT_FALSE(entry.has_value());
}

TEST_F(PacsConfigManagerTest, GetAllServers) {
    manager->addServer("Server 1", createValidConfig("host1.com"));
    manager->addServer("Server 2", createValidConfig("host2.com"));
    manager->addServer("Server 3", createValidConfig("host3.com"));

    auto servers = manager->getAllServers();
    EXPECT_EQ(servers.size(), 3u);
}

// ---- Update -----------------------------------------------------------------

TEST_F(PacsConfigManagerTest, UpdateServer) {
    auto config = createValidConfig();
    std::string id = manager->addServer("Original Name", config);

    auto newConfig = createValidConfig("updated.hospital.com");
    bool result = manager->updateServer(id, "Updated Name", newConfig);

    EXPECT_TRUE(result);

    auto entry = manager->getServer(id);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->displayName, "Updated Name");
    EXPECT_EQ(entry->config.hostname, "updated.hospital.com");
}

TEST_F(PacsConfigManagerTest, UpdateNonexistentServerFails) {
    auto config = createValidConfig();
    bool result = manager->updateServer("nonexistent-uuid", "Name", config);
    EXPECT_FALSE(result);
}

TEST_F(PacsConfigManagerTest, UpdateServerFiresCallback) {
    auto config = createValidConfig();
    std::string id = manager->addServer("Test Server", config);

    int callCount = 0;
    std::string capturedId;
    manager->setOnServerUpdated([&](const std::string& cbId) {
        ++callCount;
        capturedId = cbId;
    });

    manager->updateServer(id, "Updated", config);

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedId, id);
}

// ---- Remove -----------------------------------------------------------------

TEST_F(PacsConfigManagerTest, RemoveServer) {
    auto config = createValidConfig();
    std::string id = manager->addServer("Test Server", config);

    bool result = manager->removeServer(id);
    EXPECT_TRUE(result);
    EXPECT_EQ(manager->count(), 0);
    EXPECT_FALSE(manager->getServer(id).has_value());
}

TEST_F(PacsConfigManagerTest, RemoveNonexistentServerFails) {
    bool result = manager->removeServer("nonexistent-uuid");
    EXPECT_FALSE(result);
}

TEST_F(PacsConfigManagerTest, RemoveServerFiresCallback) {
    auto config = createValidConfig();
    std::string id = manager->addServer("Test Server", config);

    int callCount = 0;
    std::string capturedId;
    manager->setOnServerRemoved([&](const std::string& cbId) {
        ++callCount;
        capturedId = cbId;
    });

    manager->removeServer(id);

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedId, id);
}

// ---- Default server ---------------------------------------------------------

TEST_F(PacsConfigManagerTest, FirstAddedServerBecomesDefault) {
    auto config = createValidConfig();
    std::string id = manager->addServer("First Server", config);

    auto defaultServer = manager->getDefaultServer();
    ASSERT_TRUE(defaultServer.has_value());
    EXPECT_EQ(defaultServer->id, id);
    EXPECT_TRUE(defaultServer->isDefault);
}

TEST_F(PacsConfigManagerTest, SetDefaultServer) {
    auto config1 = createValidConfig("host1.com");
    auto config2 = createValidConfig("host2.com");

    manager->addServer("Server 1", config1);
    std::string id2 = manager->addServer("Server 2", config2);

    bool result = manager->setDefaultServer(id2);
    EXPECT_TRUE(result);

    auto defaultServer = manager->getDefaultServer();
    ASSERT_TRUE(defaultServer.has_value());
    EXPECT_EQ(defaultServer->id, id2);
}

TEST_F(PacsConfigManagerTest, SetDefaultNonexistentServerFails) {
    bool result = manager->setDefaultServer("nonexistent-uuid");
    EXPECT_FALSE(result);
}

TEST_F(PacsConfigManagerTest, SetDefaultServerFiresCallback) {
    manager->addServer("Server 1", createValidConfig("host1.com"));
    std::string id2 = manager->addServer("Server 2", createValidConfig("host2.com"));

    int callCount = 0;
    std::string capturedId;
    manager->setOnDefaultServerChanged([&](const std::string& cbId) {
        ++callCount;
        capturedId = cbId;
    });

    manager->setDefaultServer(id2);

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(capturedId, id2);
}

TEST_F(PacsConfigManagerTest, RemoveDefaultServerSelectsNewDefault) {
    std::string id1 = manager->addServer("Server 1", createValidConfig("host1.com"));
    std::string id2 = manager->addServer("Server 2", createValidConfig("host2.com"));

    manager->removeServer(id1);

    auto defaultServer = manager->getDefaultServer();
    ASSERT_TRUE(defaultServer.has_value());
    EXPECT_EQ(defaultServer->id, id2);
}

// ---- ServerEntry validation -------------------------------------------------

TEST_F(PacsConfigManagerTest, ServerEntryValidation) {
    PacsConfigManager::ServerEntry entry;
    EXPECT_FALSE(entry.isValid());  // Empty id and config

    entry.id = "some-uuid";
    EXPECT_FALSE(entry.isValid());  // Still invalid config

    entry.config = createValidConfig();
    EXPECT_FALSE(entry.isValid());  // Empty displayName

    entry.displayName = "Test";
    EXPECT_TRUE(entry.isValid());
}

// ---- Persistence ------------------------------------------------------------

TEST_F(PacsConfigManagerTest, SaveAndLoad) {
    auto config = createValidConfig();
    std::string id = manager->addServer("Persistent Server", config);

    manager->save();

    auto newManager = std::make_unique<PacsConfigManager>();
    auto entry = newManager->getServer(id);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->displayName, "Persistent Server");
    EXPECT_EQ(entry->config.hostname, "test.hospital.com");
}

TEST_F(PacsConfigManagerTest, PersistencePreservesAllFields) {
    PacsServerConfig config;
    config.hostname = "test.hospital.com";
    config.port = 11112;
    config.calledAeTitle = "CALLED_AE";
    config.callingAeTitle = "CALLING_AE";
    config.connectionTimeout = std::chrono::seconds(45);
    config.dimseTimeout = std::chrono::seconds(60);
    config.maxPduSize = 32768;
    config.description = "Test Description";

    std::string id = manager->addServer("Full Config Server", config);
    manager->save();

    auto newManager = std::make_unique<PacsConfigManager>();
    auto entry = newManager->getServer(id);

    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->config.hostname, "test.hospital.com");
    EXPECT_EQ(entry->config.port, 11112);
    EXPECT_EQ(entry->config.calledAeTitle, "CALLED_AE");
    EXPECT_EQ(entry->config.callingAeTitle, "CALLING_AE");
    EXPECT_EQ(entry->config.connectionTimeout.count(), 45);
    EXPECT_EQ(entry->config.dimseTimeout.count(), 60);
    EXPECT_EQ(entry->config.maxPduSize, 32768u);
    ASSERT_TRUE(entry->config.description.has_value());
    EXPECT_EQ(*entry->config.description, "Test Description");
}

// ---- Load callback ----------------------------------------------------------

TEST_F(PacsConfigManagerTest, LoadFiresCallback) {
    int callCount = 0;
    manager->setOnServersLoaded([&]() { ++callCount; });
    manager->load();
    EXPECT_EQ(callCount, 1);
}

// ---- Edge cases -------------------------------------------------------------

TEST_F(PacsConfigManagerTest, RapidAddRemoveSequence) {
    for (int i = 0; i < 20; ++i) {
        auto config = createValidConfig("host" + std::to_string(i) + ".com");
        std::string id = manager->addServer("Server " + std::to_string(i), config);
        EXPECT_FALSE(id.empty());

        if (i % 3 == 0) {
            bool removed = manager->removeServer(id);
            EXPECT_TRUE(removed);
        }
    }

    // 20 added, 7 removed (i = 0,3,6,9,12,15,18) → 13 remaining
    EXPECT_EQ(manager->count(), 13);
}

TEST_F(PacsConfigManagerTest, DuplicateServerConfigAllowed) {
    auto config = createValidConfig("same.hospital.com");

    std::string id1 = manager->addServer("PACS Primary", config);
    std::string id2 = manager->addServer("PACS Backup", config);

    EXPECT_NE(id1, id2);
    EXPECT_EQ(manager->count(), 2);

    auto entry1 = manager->getServer(id1);
    auto entry2 = manager->getServer(id2);
    ASSERT_TRUE(entry1.has_value());
    ASSERT_TRUE(entry2.has_value());
    EXPECT_EQ(entry1->config.hostname, entry2->config.hostname);
    EXPECT_NE(entry1->displayName, entry2->displayName);
}

TEST_F(PacsConfigManagerTest, SpecialCharactersInDisplayName) {
    auto config = createValidConfig();

    std::string id1 = manager->addServer("Hospital (Main) - PACS/RIS #1", config);
    std::string id2 = manager->addServer("Dr. Smith's Clinic & Lab [v2.0]", config);
    std::string id3 = manager->addServer("PACS <Test> @Emergency Room", config);

    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_FALSE(id3.empty());
    EXPECT_EQ(manager->count(), 3);

    auto entry1 = manager->getServer(id1);
    ASSERT_TRUE(entry1.has_value());
    EXPECT_EQ(entry1->displayName, "Hospital (Main) - PACS/RIS #1");

    auto entry2 = manager->getServer(id2);
    ASSERT_TRUE(entry2.has_value());
    EXPECT_EQ(entry2->displayName, "Dr. Smith's Clinic & Lab [v2.0]");

    auto entry3 = manager->getServer(id3);
    ASSERT_TRUE(entry3.has_value());
    EXPECT_EQ(entry3->displayName, "PACS <Test> @Emergency Room");
}

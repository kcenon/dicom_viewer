#include <gtest/gtest.h>

#include "services/pacs_config_manager.hpp"
#include "services/pacs_config.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>

using namespace dicom_viewer::services;

class PacsConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Qt application if not already done
        if (!QCoreApplication::instance()) {
            int argc = 0;
            char* argv[] = {nullptr};
            app = std::make_unique<QCoreApplication>(argc, argv);
        }

        // Set up temporary settings location
        QSettings::setDefaultFormat(QSettings::IniFormat);

        manager = std::make_unique<PacsConfigManager>();
    }

    void TearDown() override {
        manager.reset();
        // Clear settings
        QSettings settings("DicomViewer", "DicomViewer");
        settings.remove("PacsServers");
    }

    PacsServerConfig createValidConfig(const std::string& hostname = "test.hospital.com") {
        PacsServerConfig config;
        config.hostname = hostname;
        config.port = 104;
        config.calledAeTitle = "PACS_TEST";
        config.callingAeTitle = "DICOM_VIEWER";
        return config;
    }

    std::unique_ptr<QCoreApplication> app;
    std::unique_ptr<PacsConfigManager> manager;
};

// Test construction
TEST_F(PacsConfigManagerTest, DefaultConstruction) {
    EXPECT_NE(manager, nullptr);
    EXPECT_TRUE(manager->isEmpty());
    EXPECT_EQ(manager->count(), 0);
}

// Test adding servers
TEST_F(PacsConfigManagerTest, AddSingleServer) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("Test Server", config);

    EXPECT_FALSE(id.isNull());
    EXPECT_EQ(manager->count(), 1);
    EXPECT_FALSE(manager->isEmpty());
}

TEST_F(PacsConfigManagerTest, AddMultipleServers) {
    auto config1 = createValidConfig("host1.hospital.com");
    auto config2 = createValidConfig("host2.hospital.com");

    QUuid id1 = manager->addServer("Server 1", config1);
    QUuid id2 = manager->addServer("Server 2", config2);

    EXPECT_NE(id1, id2);
    EXPECT_EQ(manager->count(), 2);
}

TEST_F(PacsConfigManagerTest, AddServerEmitsSignal) {
    QSignalSpy spy(manager.get(), &PacsConfigManager::serverAdded);

    auto config = createValidConfig();
    QUuid id = manager->addServer("Test Server", config);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<QUuid>(), id);
}

// Test retrieving servers
TEST_F(PacsConfigManagerTest, GetServerById) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("Test Server", config);

    auto entry = manager->getServer(id);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->id, id);
    EXPECT_EQ(entry->displayName, "Test Server");
    EXPECT_EQ(entry->config.hostname, "test.hospital.com");
}

TEST_F(PacsConfigManagerTest, GetNonexistentServerReturnsNullopt) {
    auto entry = manager->getServer(QUuid::createUuid());
    EXPECT_FALSE(entry.has_value());
}

TEST_F(PacsConfigManagerTest, GetAllServers) {
    manager->addServer("Server 1", createValidConfig("host1.com"));
    manager->addServer("Server 2", createValidConfig("host2.com"));
    manager->addServer("Server 3", createValidConfig("host3.com"));

    auto servers = manager->getAllServers();
    EXPECT_EQ(servers.size(), 3);
}

// Test updating servers
TEST_F(PacsConfigManagerTest, UpdateServer) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("Original Name", config);

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
    bool result = manager->updateServer(QUuid::createUuid(), "Name", config);
    EXPECT_FALSE(result);
}

TEST_F(PacsConfigManagerTest, UpdateServerEmitsSignal) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("Test Server", config);

    QSignalSpy spy(manager.get(), &PacsConfigManager::serverUpdated);
    manager->updateServer(id, "Updated", config);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<QUuid>(), id);
}

// Test removing servers
TEST_F(PacsConfigManagerTest, RemoveServer) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("Test Server", config);

    bool result = manager->removeServer(id);
    EXPECT_TRUE(result);
    EXPECT_EQ(manager->count(), 0);
    EXPECT_FALSE(manager->getServer(id).has_value());
}

TEST_F(PacsConfigManagerTest, RemoveNonexistentServerFails) {
    bool result = manager->removeServer(QUuid::createUuid());
    EXPECT_FALSE(result);
}

TEST_F(PacsConfigManagerTest, RemoveServerEmitsSignal) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("Test Server", config);

    QSignalSpy spy(manager.get(), &PacsConfigManager::serverRemoved);
    manager->removeServer(id);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<QUuid>(), id);
}

// Test default server
TEST_F(PacsConfigManagerTest, FirstAddedServerBecomesDefault) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("First Server", config);

    auto defaultServer = manager->getDefaultServer();
    ASSERT_TRUE(defaultServer.has_value());
    EXPECT_EQ(defaultServer->id, id);
    EXPECT_TRUE(defaultServer->isDefault);
}

TEST_F(PacsConfigManagerTest, SetDefaultServer) {
    auto config1 = createValidConfig("host1.com");
    auto config2 = createValidConfig("host2.com");

    QUuid id1 = manager->addServer("Server 1", config1);
    QUuid id2 = manager->addServer("Server 2", config2);

    bool result = manager->setDefaultServer(id2);
    EXPECT_TRUE(result);

    auto defaultServer = manager->getDefaultServer();
    ASSERT_TRUE(defaultServer.has_value());
    EXPECT_EQ(defaultServer->id, id2);
}

TEST_F(PacsConfigManagerTest, SetDefaultNonexistentServerFails) {
    bool result = manager->setDefaultServer(QUuid::createUuid());
    EXPECT_FALSE(result);
}

TEST_F(PacsConfigManagerTest, SetDefaultServerEmitsSignal) {
    auto config1 = createValidConfig("host1.com");
    auto config2 = createValidConfig("host2.com");

    manager->addServer("Server 1", config1);
    QUuid id2 = manager->addServer("Server 2", config2);

    QSignalSpy spy(manager.get(), &PacsConfigManager::defaultServerChanged);
    manager->setDefaultServer(id2);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<QUuid>(), id2);
}

TEST_F(PacsConfigManagerTest, RemoveDefaultServerSelectsNewDefault) {
    auto config1 = createValidConfig("host1.com");
    auto config2 = createValidConfig("host2.com");

    QUuid id1 = manager->addServer("Server 1", config1);
    QUuid id2 = manager->addServer("Server 2", config2);

    manager->removeServer(id1);

    auto defaultServer = manager->getDefaultServer();
    ASSERT_TRUE(defaultServer.has_value());
    EXPECT_EQ(defaultServer->id, id2);
}

// Test ServerEntry validation
TEST_F(PacsConfigManagerTest, ServerEntryValidation) {
    PacsConfigManager::ServerEntry entry;
    EXPECT_FALSE(entry.isValid());  // Empty id and config

    entry.id = QUuid::createUuid();
    EXPECT_FALSE(entry.isValid());  // Still invalid config

    entry.config = createValidConfig();
    EXPECT_FALSE(entry.isValid());  // Empty displayName

    entry.displayName = "Test";
    EXPECT_TRUE(entry.isValid());
}

// Test persistence
TEST_F(PacsConfigManagerTest, SaveAndLoad) {
    auto config = createValidConfig();
    QUuid id = manager->addServer("Persistent Server", config);

    // Save explicitly
    manager->save();

    // Create new manager to load from storage
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

    QUuid id = manager->addServer("Full Config Server", config);
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
    EXPECT_EQ(entry->config.maxPduSize, 32768);
    ASSERT_TRUE(entry->config.description.has_value());
    EXPECT_EQ(*entry->config.description, "Test Description");
}

// Test load signal
TEST_F(PacsConfigManagerTest, LoadEmitsSignal) {
    QSignalSpy spy(manager.get(), &PacsConfigManager::serversLoaded);
    manager->load();
    EXPECT_EQ(spy.count(), 1);
}

// =============================================================================
// Concurrency and edge case tests (Issue #206)
// =============================================================================

TEST_F(PacsConfigManagerTest, RapidAddRemoveSequence) {
    // Rapidly add and remove servers to test robustness
    for (int i = 0; i < 20; ++i) {
        auto config = createValidConfig("host" + std::to_string(i) + ".com");
        QUuid id = manager->addServer(
            QString("Server %1").arg(i), config);
        EXPECT_FALSE(id.isNull());

        // Remove every 3rd server (i = 0, 3, 6, 9, 12, 15, 18)
        if (i % 3 == 0) {
            bool removed = manager->removeServer(id);
            EXPECT_TRUE(removed);
        }
    }

    // 20 added, 7 removed → 13 remaining
    EXPECT_EQ(manager->count(), 13);
}

TEST_F(PacsConfigManagerTest, DuplicateServerConfigAllowed) {
    auto config = createValidConfig("same.hospital.com");

    QUuid id1 = manager->addServer("PACS Primary", config);
    QUuid id2 = manager->addServer("PACS Backup", config);

    // Same config, different entries with unique IDs
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

    QUuid id1 = manager->addServer(
        "Hospital (Main) - PACS/RIS #1", config);
    QUuid id2 = manager->addServer(
        "Dr. Smith's Clinic & Lab [v2.0]", config);
    QUuid id3 = manager->addServer(
        "PACS <Test> @Emergency Room", config);

    EXPECT_FALSE(id1.isNull());
    EXPECT_FALSE(id2.isNull());
    EXPECT_FALSE(id3.isNull());
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

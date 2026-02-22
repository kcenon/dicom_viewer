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

#include "ui/dialogs/pacs_config_dialog.hpp"
#include "services/pacs_config_manager.hpp"
#include "services/dicom_echo_scu.hpp"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

class PacsConfigDialog::Impl {
public:
    services::PacsConfigManager* manager = nullptr;

    QTableWidget* serverTable = nullptr;
    QPushButton* addButton = nullptr;
    QPushButton* editButton = nullptr;
    QPushButton* removeButton = nullptr;
    QPushButton* testButton = nullptr;
    QPushButton* setDefaultButton = nullptr;

    QUuid selectedId;
};

PacsConfigDialog::PacsConfigDialog(services::PacsConfigManager* manager,
                                   QWidget* parent)
    : QDialog(parent)
    , impl_(std::make_unique<Impl>())
{
    impl_->manager = manager;
    setWindowTitle(tr("PACS Server Configuration"));
    setMinimumSize(600, 400);
    setupUI();
    setupConnections();
    refreshServerList();
}

PacsConfigDialog::~PacsConfigDialog() = default;

void PacsConfigDialog::setupUI()
{
    auto mainLayout = new QVBoxLayout(this);

    // Server table
    auto tableGroup = new QGroupBox(tr("Configured Servers"), this);
    auto tableLayout = new QVBoxLayout(tableGroup);

    impl_->serverTable = new QTableWidget(this);
    impl_->serverTable->setColumnCount(5);
    impl_->serverTable->setHorizontalHeaderLabels({
        tr("Name"), tr("Host"), tr("Port"), tr("AE Title"), tr("Default")
    });
    impl_->serverTable->horizontalHeader()->setStretchLastSection(true);
    impl_->serverTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    impl_->serverTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->serverTable->setSelectionMode(QAbstractItemView::SingleSelection);
    impl_->serverTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    impl_->serverTable->verticalHeader()->setVisible(false);
    tableLayout->addWidget(impl_->serverTable);

    mainLayout->addWidget(tableGroup);

    // Button panel
    auto buttonLayout = new QHBoxLayout();

    impl_->addButton = new QPushButton(tr("&Add..."), this);
    impl_->editButton = new QPushButton(tr("&Edit..."), this);
    impl_->removeButton = new QPushButton(tr("&Remove"), this);
    impl_->testButton = new QPushButton(tr("&Test Connection"), this);
    impl_->setDefaultButton = new QPushButton(tr("Set &Default"), this);

    buttonLayout->addWidget(impl_->addButton);
    buttonLayout->addWidget(impl_->editButton);
    buttonLayout->addWidget(impl_->removeButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(impl_->testButton);
    buttonLayout->addWidget(impl_->setDefaultButton);

    mainLayout->addLayout(buttonLayout);

    // Dialog buttons
    auto dialogButtons = new QDialogButtonBox(
        QDialogButtonBox::Close, this);
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(dialogButtons);

    updateButtonStates();
}

void PacsConfigDialog::setupConnections()
{
    connect(impl_->addButton, &QPushButton::clicked,
            this, &PacsConfigDialog::onAddServer);
    connect(impl_->editButton, &QPushButton::clicked,
            this, &PacsConfigDialog::onEditServer);
    connect(impl_->removeButton, &QPushButton::clicked,
            this, &PacsConfigDialog::onRemoveServer);
    connect(impl_->testButton, &QPushButton::clicked,
            this, &PacsConfigDialog::onTestConnection);
    connect(impl_->setDefaultButton, &QPushButton::clicked,
            this, &PacsConfigDialog::onSetDefault);
    connect(impl_->serverTable, &QTableWidget::itemSelectionChanged,
            this, &PacsConfigDialog::onServerSelectionChanged);
    connect(impl_->serverTable, &QTableWidget::cellDoubleClicked,
            this, &PacsConfigDialog::onEditServer);

    connect(impl_->manager, &services::PacsConfigManager::serverAdded,
            this, &PacsConfigDialog::refreshServerList);
    connect(impl_->manager, &services::PacsConfigManager::serverUpdated,
            this, &PacsConfigDialog::refreshServerList);
    connect(impl_->manager, &services::PacsConfigManager::serverRemoved,
            this, &PacsConfigDialog::refreshServerList);
    connect(impl_->manager, &services::PacsConfigManager::defaultServerChanged,
            this, &PacsConfigDialog::refreshServerList);
}

void PacsConfigDialog::refreshServerList()
{
    impl_->serverTable->setRowCount(0);

    auto servers = impl_->manager->getAllServers();
    impl_->serverTable->setRowCount(static_cast<int>(servers.size()));

    for (int i = 0; i < static_cast<int>(servers.size()); ++i) {
        const auto& entry = servers[static_cast<size_t>(i)];

        auto nameItem = new QTableWidgetItem(entry.displayName);
        nameItem->setData(Qt::UserRole, entry.id.toString());

        impl_->serverTable->setItem(i, 0, nameItem);
        impl_->serverTable->setItem(i, 1, new QTableWidgetItem(
            QString::fromStdString(entry.config.hostname)));
        impl_->serverTable->setItem(i, 2, new QTableWidgetItem(
            QString::number(entry.config.port)));
        impl_->serverTable->setItem(i, 3, new QTableWidgetItem(
            QString::fromStdString(entry.config.calledAeTitle)));
        impl_->serverTable->setItem(i, 4, new QTableWidgetItem(
            entry.isDefault ? tr("Yes") : QString()));
    }

    updateButtonStates();
}

void PacsConfigDialog::onServerSelectionChanged()
{
    auto selected = impl_->serverTable->selectedItems();
    if (!selected.isEmpty()) {
        QString idStr = selected.first()->data(Qt::UserRole).toString();
        impl_->selectedId = QUuid::fromString(idStr);
    } else {
        impl_->selectedId = QUuid();
    }
    updateButtonStates();
}

void PacsConfigDialog::updateButtonStates()
{
    bool hasSelection = !impl_->selectedId.isNull();
    impl_->editButton->setEnabled(hasSelection);
    impl_->removeButton->setEnabled(hasSelection);
    impl_->testButton->setEnabled(hasSelection);
    impl_->setDefaultButton->setEnabled(hasSelection);
}

QUuid PacsConfigDialog::selectedServerId() const
{
    return impl_->selectedId;
}

namespace {
class ServerEditDialog : public QDialog {
public:
    ServerEditDialog(const QString& title, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(title);
        setModal(true);
        setupUI();
    }

    QString displayName() const { return displayNameEdit_->text(); }
    void setDisplayName(const QString& name) { displayNameEdit_->setText(name); }

    QString hostname() const { return hostnameEdit_->text(); }
    void setHostname(const QString& host) { hostnameEdit_->setText(host); }

    uint16_t port() const { return static_cast<uint16_t>(portSpinBox_->value()); }
    void setPort(uint16_t port) { portSpinBox_->setValue(port); }

    QString calledAeTitle() const { return calledAeTitleEdit_->text(); }
    void setCalledAeTitle(const QString& ae) { calledAeTitleEdit_->setText(ae); }

    QString callingAeTitle() const { return callingAeTitleEdit_->text(); }
    void setCallingAeTitle(const QString& ae) { callingAeTitleEdit_->setText(ae); }

    QString description() const { return descriptionEdit_->text(); }
    void setDescription(const QString& desc) { descriptionEdit_->setText(desc); }

    void accept() override {
        // Validate inputs
        if (displayName().isEmpty()) {
            QMessageBox::warning(this, tr("Validation Error"),
                tr("Display name is required."));
            return;
        }
        if (hostname().isEmpty()) {
            QMessageBox::warning(this, tr("Validation Error"),
                tr("Hostname is required."));
            return;
        }
        if (calledAeTitle().isEmpty()) {
            QMessageBox::warning(this, tr("Validation Error"),
                tr("Called AE Title is required."));
            return;
        }
        if (calledAeTitle().length() > 16) {
            QMessageBox::warning(this, tr("Validation Error"),
                tr("AE Title must be 16 characters or less."));
            return;
        }
        if (callingAeTitle().length() > 16) {
            QMessageBox::warning(this, tr("Validation Error"),
                tr("Calling AE Title must be 16 characters or less."));
            return;
        }
        QDialog::accept();
    }

private:
    void setupUI() {
        auto layout = new QVBoxLayout(this);
        auto form = new QFormLayout();

        displayNameEdit_ = new QLineEdit(this);
        displayNameEdit_->setPlaceholderText(tr("My PACS Server"));
        form->addRow(tr("Display Name:"), displayNameEdit_);

        hostnameEdit_ = new QLineEdit(this);
        hostnameEdit_->setPlaceholderText(tr("pacs.hospital.com"));
        form->addRow(tr("Hostname:"), hostnameEdit_);

        portSpinBox_ = new QSpinBox(this);
        portSpinBox_->setRange(1, 65535);
        portSpinBox_->setValue(104);
        form->addRow(tr("Port:"), portSpinBox_);

        calledAeTitleEdit_ = new QLineEdit(this);
        calledAeTitleEdit_->setMaxLength(16);
        calledAeTitleEdit_->setPlaceholderText(tr("PACS_SERVER"));
        form->addRow(tr("Called AE Title:"), calledAeTitleEdit_);

        callingAeTitleEdit_ = new QLineEdit(this);
        callingAeTitleEdit_->setMaxLength(16);
        callingAeTitleEdit_->setText("DICOM_VIEWER");
        form->addRow(tr("Calling AE Title:"), callingAeTitleEdit_);

        descriptionEdit_ = new QLineEdit(this);
        descriptionEdit_->setPlaceholderText(tr("Optional description"));
        form->addRow(tr("Description:"), descriptionEdit_);

        layout->addLayout(form);

        auto buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    QLineEdit* displayNameEdit_ = nullptr;
    QLineEdit* hostnameEdit_ = nullptr;
    QSpinBox* portSpinBox_ = nullptr;
    QLineEdit* calledAeTitleEdit_ = nullptr;
    QLineEdit* callingAeTitleEdit_ = nullptr;
    QLineEdit* descriptionEdit_ = nullptr;
};
} // namespace

void PacsConfigDialog::onAddServer()
{
    ServerEditDialog dialog(tr("Add PACS Server"), this);
    if (dialog.exec() == QDialog::Accepted) {
        services::PacsServerConfig config;
        config.hostname = dialog.hostname().toStdString();
        config.port = dialog.port();
        config.calledAeTitle = dialog.calledAeTitle().toStdString();
        config.callingAeTitle = dialog.callingAeTitle().toStdString();
        if (!dialog.description().isEmpty()) {
            config.description = dialog.description().toStdString();
        }

        impl_->manager->addServer(dialog.displayName(), config);
    }
}

void PacsConfigDialog::onEditServer()
{
    auto entry = impl_->manager->getServer(impl_->selectedId);
    if (!entry) {
        return;
    }

    ServerEditDialog dialog(tr("Edit PACS Server"), this);
    dialog.setDisplayName(entry->displayName);
    dialog.setHostname(QString::fromStdString(entry->config.hostname));
    dialog.setPort(entry->config.port);
    dialog.setCalledAeTitle(QString::fromStdString(entry->config.calledAeTitle));
    dialog.setCallingAeTitle(QString::fromStdString(entry->config.callingAeTitle));
    if (entry->config.description) {
        dialog.setDescription(QString::fromStdString(*entry->config.description));
    }

    if (dialog.exec() == QDialog::Accepted) {
        services::PacsServerConfig config = entry->config;
        config.hostname = dialog.hostname().toStdString();
        config.port = dialog.port();
        config.calledAeTitle = dialog.calledAeTitle().toStdString();
        config.callingAeTitle = dialog.callingAeTitle().toStdString();
        if (!dialog.description().isEmpty()) {
            config.description = dialog.description().toStdString();
        } else {
            config.description = std::nullopt;
        }

        impl_->manager->updateServer(impl_->selectedId, dialog.displayName(), config);
    }
}

void PacsConfigDialog::onRemoveServer()
{
    auto entry = impl_->manager->getServer(impl_->selectedId);
    if (!entry) {
        return;
    }

    auto result = QMessageBox::question(
        this, tr("Confirm Removal"),
        tr("Are you sure you want to remove '%1'?").arg(entry->displayName),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        impl_->manager->removeServer(impl_->selectedId);
        impl_->selectedId = QUuid();
    }
}

void PacsConfigDialog::onTestConnection()
{
    auto entry = impl_->manager->getServer(impl_->selectedId);
    if (!entry) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    services::DicomEchoSCU echo;
    auto result = echo.verify(entry->config);

    QApplication::restoreOverrideCursor();

    if (result) {
        QMessageBox::information(
            this, tr("Connection Test"),
            tr("Connection successful!\n\n"
               "Server: %1\n"
               "Latency: %2 ms")
                .arg(entry->displayName)
                .arg(result->latency.count()));
    } else {
        QMessageBox::warning(
            this, tr("Connection Test"),
            tr("Connection failed!\n\n"
               "Server: %1\n"
               "Error: %2")
                .arg(entry->displayName)
                .arg(QString::fromStdString(result.error().toString())));
    }
}

void PacsConfigDialog::onSetDefault()
{
    impl_->manager->setDefaultServer(impl_->selectedId);
}

} // namespace dicom_viewer::ui

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

// Ecosystem logger headers must precede Qt to avoid emit() macro conflict
#include <kcenon/common/interfaces/global_logger_registry.h>

#include "ui/dialogs/settings_dialog.hpp"
#include "core/app_log_level.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

namespace dicom_viewer::ui {

namespace {

struct LogLevelEntry {
    AppLogLevel level;
    const char* name;
    const char* description;
};

constexpr LogLevelEntry kLogLevels[] = {
    {AppLogLevel::Exception,   "Exception",   "Unintended errors only (crashes, unexpected failures)"},
    {AppLogLevel::Error,       "Error",       "Exception + intended error messages (validation failures)"},
    {AppLogLevel::Information, "Information", "Exception + Error + minimal information flow (default)"},
    {AppLogLevel::Debug,       "Debug",       "All messages including detailed traces"},
};

} // anonymous namespace

class SettingsDialog::Impl {
public:
    QComboBox* logLevelCombo = nullptr;
    QLabel* descriptionLabel = nullptr;
    int initialLevelIndex = 2; // Information
};

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , impl_(std::make_unique<Impl>())
{
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUI()
{
    setWindowTitle(tr("Preferences"));
    setMinimumWidth(400);

    auto* mainLayout = new QVBoxLayout(this);

    // Logging group box
    auto* loggingGroup = new QGroupBox(tr("Logging"));
    auto* loggingLayout = new QVBoxLayout(loggingGroup);

    auto* levelLayout = new QHBoxLayout;
    levelLayout->addWidget(new QLabel(tr("Log Level:")));

    impl_->logLevelCombo = new QComboBox;
    for (const auto& entry : kLogLevels) {
        impl_->logLevelCombo->addItem(tr(entry.name));
    }
    levelLayout->addWidget(impl_->logLevelCombo, 1);
    loggingLayout->addLayout(levelLayout);

    impl_->descriptionLabel = new QLabel;
    impl_->descriptionLabel->setWordWrap(true);
    impl_->descriptionLabel->setStyleSheet("color: gray; font-style: italic;");
    loggingLayout->addWidget(impl_->descriptionLabel);

    mainLayout->addWidget(loggingGroup);
    mainLayout->addStretch();

    // Dialog buttons
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(impl_->logLevelCombo, &QComboBox::currentIndexChanged,
            this, &SettingsDialog::onLogLevelChanged);
}

void SettingsDialog::loadSettings()
{
    QSettings settings;
    const int levelValue = settings.value("logging/level", 2).toInt();
    const auto appLevel = from_settings_value(levelValue);

    int index = static_cast<int>(appLevel);
    impl_->logLevelCombo->setCurrentIndex(index);
    impl_->initialLevelIndex = index;
    onLogLevelChanged(index);
}

void SettingsDialog::saveSettings()
{
    int index = impl_->logLevelCombo->currentIndex();
    auto level = kLogLevels[index].level;

    QSettings settings;
    settings.setValue("logging/level", to_settings_value(level));
}

void SettingsDialog::applyLogLevel()
{
    int index = impl_->logLevelCombo->currentIndex();
    auto appLevel = kLogLevels[index].level;
    auto ecoLevel = to_ecosystem_level(appLevel);

    auto& registry = kcenon::common::interfaces::GlobalLoggerRegistry::instance();
    auto logger = registry.get_default_logger();
    if (logger) {
        logger->set_level(ecoLevel);
    }
}

void SettingsDialog::accept()
{
    saveSettings();
    applyLogLevel();
    QDialog::accept();
}

void SettingsDialog::onLogLevelChanged(int index)
{
    if (index >= 0 && index < static_cast<int>(std::size(kLogLevels))) {
        impl_->descriptionLabel->setText(tr(kLogLevels[index].description));
    }
}

} // namespace dicom_viewer::ui

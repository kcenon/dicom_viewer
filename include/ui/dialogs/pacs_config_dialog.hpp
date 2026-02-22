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
 * @file pacs_config_dialog.hpp
 * @brief Dialog for managing PACS server configurations
 * @details Provides UI for adding, editing, and removing PACS servers with
 *          connection test via C-ECHO. Returns selected server ID
 *          on acceptance.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QDialog-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <memory>
#include <QDialog>
#include <QUuid>

namespace dicom_viewer::services {
class PacsConfigManager;
}

namespace dicom_viewer::ui {

/**
 * @brief Dialog for managing PACS server configurations
 *
 * Provides a UI for adding, editing, and removing PACS server
 * configurations. Includes a connection test button using C-ECHO.
 *
 * @trace SRS-FR-038
 */
class PacsConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit PacsConfigDialog(services::PacsConfigManager* manager,
                              QWidget* parent = nullptr);
    ~PacsConfigDialog() override;

    // Non-copyable
    PacsConfigDialog(const PacsConfigDialog&) = delete;
    PacsConfigDialog& operator=(const PacsConfigDialog&) = delete;

    /**
     * @brief Get the currently selected server ID
     */
    [[nodiscard]] QUuid selectedServerId() const;

private slots:
    void onAddServer();
    void onEditServer();
    void onRemoveServer();
    void onTestConnection();
    void onSetDefault();
    void onServerSelectionChanged();
    void refreshServerList();

private:
    void setupUI();
    void setupConnections();
    void updateButtonStates();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

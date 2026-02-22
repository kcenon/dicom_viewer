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
 * @file pacs_config.hpp
 * @brief PACS server connection configuration data structure
 * @details Defines the PacsServerConfig struct containing DICOM network
 *          parameters: hostname, port, AE titles (local and remote),
 *          and a unique identifier for configuration management.
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Configuration for a PACS/DICOM server
 *
 * Contains all necessary information to establish a connection
 * with a DICOM Application Entity (AE).
 *
 * @trace SRS-FR-038
 */
struct PacsServerConfig {
    /// Server hostname or IP address
    std::string hostname;

    /// DICOM port number (default: 104)
    uint16_t port = 104;

    /// Called AE Title (remote server's AE title)
    std::string calledAeTitle;

    /// Calling AE Title (this client's AE title)
    std::string callingAeTitle = "DICOM_VIEWER";

    /// Connection timeout
    std::chrono::seconds connectionTimeout{30};

    /// DIMSE timeout for response
    std::chrono::seconds dimseTimeout{30};

    /// Optional description for this server
    std::optional<std::string> description;

    /// Maximum PDU size for network transmission
    uint32_t maxPduSize = 16384;

    /**
     * @brief Validate the configuration
     * @return true if configuration is valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return !hostname.empty() &&
               !calledAeTitle.empty() &&
               !callingAeTitle.empty() &&
               calledAeTitle.length() <= 16 &&
               callingAeTitle.length() <= 16 &&
               port > 0;
    }
};

} // namespace dicom_viewer::services

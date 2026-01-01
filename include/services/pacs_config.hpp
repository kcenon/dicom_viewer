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

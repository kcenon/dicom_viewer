#pragma once

#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dicom_viewer::core {

/// Transfer Syntax categories
enum class TransferSyntaxCategory {
    Uncompressed,
    LossyCompression,
    LosslessCompression
};

/// Compression type
enum class CompressionType {
    None,
    JPEG,
    JPEGLossless,
    JPEG2000,
    JPEG2000Lossless,
    JPEGLS,
    RLE
};

/// Transfer Syntax information
struct TransferSyntaxInfo {
    std::string uid;
    std::string name;
    TransferSyntaxCategory category;
    CompressionType compressionType;
    bool isLittleEndian;
    bool isExplicitVR;
};

/// Error types for transfer syntax decoding
enum class TransferSyntaxError {
    UnsupportedTransferSyntax,
    DecodingFailed,
    InvalidPixelData,
    GDCMInitializationFailed
};

/// Error result with message
struct TransferSyntaxErrorInfo {
    TransferSyntaxError code;
    std::string message;
};

/**
 * @brief Transfer Syntax decoder and validator
 *
 * Provides DICOM Transfer Syntax detection, validation, and decoding support
 * using GDCM/ITK backend. Supports all commonly used compression formats
 * including JPEG, JPEG 2000, JPEG-LS, and RLE.
 *
 * @trace SRS-FR-003: Transfer Syntax Decoding
 * @trace PRD FR-001.3
 */
class TransferSyntaxDecoder {
public:
    TransferSyntaxDecoder();
    ~TransferSyntaxDecoder();

    // Non-copyable, movable
    TransferSyntaxDecoder(const TransferSyntaxDecoder&) = delete;
    TransferSyntaxDecoder& operator=(const TransferSyntaxDecoder&) = delete;
    TransferSyntaxDecoder(TransferSyntaxDecoder&&) noexcept;
    TransferSyntaxDecoder& operator=(TransferSyntaxDecoder&&) noexcept;

    /**
     * @brief Get Transfer Syntax information from UID
     * @param uid Transfer Syntax UID
     * @return TransferSyntaxInfo if supported, nullopt otherwise
     */
    static std::optional<TransferSyntaxInfo>
    getTransferSyntaxInfo(std::string_view uid);

    /**
     * @brief Check if a Transfer Syntax UID is supported
     * @param uid Transfer Syntax UID string
     * @return true if supported
     */
    static bool isSupported(std::string_view uid);

    /**
     * @brief Get all supported Transfer Syntax UIDs
     * @return Vector of supported Transfer Syntax UIDs
     */
    static std::vector<std::string> getSupportedUIDs();

    /**
     * @brief Get all supported Transfer Syntax information
     * @return Vector of TransferSyntaxInfo for all supported syntaxes
     */
    static std::vector<TransferSyntaxInfo> getSupportedTransferSyntaxes();

    /**
     * @brief Get Transfer Syntax name from UID
     * @param uid Transfer Syntax UID
     * @return Human-readable name or empty string if unknown
     */
    static std::string getTransferSyntaxName(std::string_view uid);

    /**
     * @brief Check if Transfer Syntax uses lossy compression
     * @param uid Transfer Syntax UID
     * @return true if lossy compression
     */
    static bool isLossyCompression(std::string_view uid);

    /**
     * @brief Check if Transfer Syntax uses any compression
     * @param uid Transfer Syntax UID
     * @return true if compressed (lossy or lossless)
     */
    static bool isCompressed(std::string_view uid);

    /**
     * @brief Get compression type from Transfer Syntax UID
     * @param uid Transfer Syntax UID
     * @return CompressionType enum value
     */
    static CompressionType getCompressionType(std::string_view uid);

    /**
     * @brief Validate GDCM can decode this Transfer Syntax
     * @param uid Transfer Syntax UID
     * @return Expected success or error info
     */
    std::expected<void, TransferSyntaxErrorInfo>
    validateDecoding(std::string_view uid) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /// Initialize internal transfer syntax map
    static const std::vector<TransferSyntaxInfo>& getTransferSyntaxMap();
};

// Well-known Transfer Syntax UIDs as compile-time constants
namespace transfer_syntax {

inline constexpr std::string_view ImplicitVRLittleEndian = "1.2.840.10008.1.2";
inline constexpr std::string_view ExplicitVRLittleEndian = "1.2.840.10008.1.2.1";
inline constexpr std::string_view ExplicitVRBigEndian = "1.2.840.10008.1.2.2";
inline constexpr std::string_view JPEGBaseline = "1.2.840.10008.1.2.4.50";
inline constexpr std::string_view JPEGExtended = "1.2.840.10008.1.2.4.51";
inline constexpr std::string_view JPEGLossless = "1.2.840.10008.1.2.4.70";
inline constexpr std::string_view JPEGLSLossless = "1.2.840.10008.1.2.4.80";
inline constexpr std::string_view JPEGLSNearLossless = "1.2.840.10008.1.2.4.81";
inline constexpr std::string_view JPEG2000Lossless = "1.2.840.10008.1.2.4.90";
inline constexpr std::string_view JPEG2000 = "1.2.840.10008.1.2.4.91";
inline constexpr std::string_view RLELossless = "1.2.840.10008.1.2.5";

} // namespace transfer_syntax

} // namespace dicom_viewer::core

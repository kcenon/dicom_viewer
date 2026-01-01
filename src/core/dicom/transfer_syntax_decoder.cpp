#include "core/transfer_syntax_decoder.hpp"

#include <algorithm>

#include <itkGDCMImageIO.h>

namespace dicom_viewer::core {

namespace {

/// Static initialization of Transfer Syntax map
const std::vector<TransferSyntaxInfo>& createTransferSyntaxMap()
{
    static const std::vector<TransferSyntaxInfo> map = {
        // Uncompressed Transfer Syntaxes
        {
            "1.2.840.10008.1.2",
            "Implicit VR Little Endian",
            TransferSyntaxCategory::Uncompressed,
            CompressionType::None,
            true,   // isLittleEndian
            false   // isExplicitVR
        },
        {
            "1.2.840.10008.1.2.1",
            "Explicit VR Little Endian",
            TransferSyntaxCategory::Uncompressed,
            CompressionType::None,
            true,
            true
        },
        {
            "1.2.840.10008.1.2.2",
            "Explicit VR Big Endian",
            TransferSyntaxCategory::Uncompressed,
            CompressionType::None,
            false,
            true
        },

        // JPEG Lossy
        {
            "1.2.840.10008.1.2.4.50",
            "JPEG Baseline (Process 1)",
            TransferSyntaxCategory::LossyCompression,
            CompressionType::JPEG,
            true,
            true
        },
        {
            "1.2.840.10008.1.2.4.51",
            "JPEG Extended (Process 2 & 4)",
            TransferSyntaxCategory::LossyCompression,
            CompressionType::JPEG,
            true,
            true
        },

        // JPEG Lossless
        {
            "1.2.840.10008.1.2.4.57",
            "JPEG Lossless, Non-Hierarchical (Process 14)",
            TransferSyntaxCategory::LosslessCompression,
            CompressionType::JPEGLossless,
            true,
            true
        },
        {
            "1.2.840.10008.1.2.4.70",
            "JPEG Lossless, Non-Hierarchical, First-Order Prediction",
            TransferSyntaxCategory::LosslessCompression,
            CompressionType::JPEGLossless,
            true,
            true
        },

        // JPEG-LS
        {
            "1.2.840.10008.1.2.4.80",
            "JPEG-LS Lossless",
            TransferSyntaxCategory::LosslessCompression,
            CompressionType::JPEGLS,
            true,
            true
        },
        {
            "1.2.840.10008.1.2.4.81",
            "JPEG-LS Near-Lossless",
            TransferSyntaxCategory::LossyCompression,
            CompressionType::JPEGLS,
            true,
            true
        },

        // JPEG 2000
        {
            "1.2.840.10008.1.2.4.90",
            "JPEG 2000 Image Compression (Lossless Only)",
            TransferSyntaxCategory::LosslessCompression,
            CompressionType::JPEG2000Lossless,
            true,
            true
        },
        {
            "1.2.840.10008.1.2.4.91",
            "JPEG 2000 Image Compression",
            TransferSyntaxCategory::LossyCompression,
            CompressionType::JPEG2000,
            true,
            true
        },

        // RLE
        {
            "1.2.840.10008.1.2.5",
            "RLE Lossless",
            TransferSyntaxCategory::LosslessCompression,
            CompressionType::RLE,
            true,
            true
        }
    };

    return map;
}

} // anonymous namespace

class TransferSyntaxDecoder::Impl {
public:
    itk::GDCMImageIO::Pointer gdcmIO;

    Impl() : gdcmIO(itk::GDCMImageIO::New()) {}
};

TransferSyntaxDecoder::TransferSyntaxDecoder()
    : impl_(std::make_unique<Impl>())
{
}

TransferSyntaxDecoder::~TransferSyntaxDecoder() = default;

TransferSyntaxDecoder::TransferSyntaxDecoder(TransferSyntaxDecoder&&) noexcept = default;
TransferSyntaxDecoder& TransferSyntaxDecoder::operator=(TransferSyntaxDecoder&&) noexcept = default;

const std::vector<TransferSyntaxInfo>& TransferSyntaxDecoder::getTransferSyntaxMap()
{
    return createTransferSyntaxMap();
}

std::optional<TransferSyntaxInfo>
TransferSyntaxDecoder::getTransferSyntaxInfo(std::string_view uid)
{
    const auto& map = getTransferSyntaxMap();
    auto it = std::find_if(map.begin(), map.end(),
        [uid](const TransferSyntaxInfo& info) {
            return info.uid == uid;
        });

    if (it != map.end()) {
        return *it;
    }
    return std::nullopt;
}

bool TransferSyntaxDecoder::isSupported(std::string_view uid)
{
    return getTransferSyntaxInfo(uid).has_value();
}

std::vector<std::string> TransferSyntaxDecoder::getSupportedUIDs()
{
    const auto& map = getTransferSyntaxMap();
    std::vector<std::string> uids;
    uids.reserve(map.size());

    for (const auto& info : map) {
        uids.push_back(info.uid);
    }

    return uids;
}

std::vector<TransferSyntaxInfo> TransferSyntaxDecoder::getSupportedTransferSyntaxes()
{
    return getTransferSyntaxMap();
}

std::string TransferSyntaxDecoder::getTransferSyntaxName(std::string_view uid)
{
    auto info = getTransferSyntaxInfo(uid);
    if (info) {
        return info->name;
    }
    return "";
}

bool TransferSyntaxDecoder::isLossyCompression(std::string_view uid)
{
    auto info = getTransferSyntaxInfo(uid);
    if (info) {
        return info->category == TransferSyntaxCategory::LossyCompression;
    }
    return false;
}

bool TransferSyntaxDecoder::isCompressed(std::string_view uid)
{
    auto info = getTransferSyntaxInfo(uid);
    if (info) {
        return info->category != TransferSyntaxCategory::Uncompressed;
    }
    return false;
}

CompressionType TransferSyntaxDecoder::getCompressionType(std::string_view uid)
{
    auto info = getTransferSyntaxInfo(uid);
    if (info) {
        return info->compressionType;
    }
    return CompressionType::None;
}

std::expected<void, TransferSyntaxErrorInfo>
TransferSyntaxDecoder::validateDecoding(std::string_view uid) const
{
    // Check if we have this in our supported list
    if (!isSupported(uid)) {
        return std::unexpected(TransferSyntaxErrorInfo{
            TransferSyntaxError::UnsupportedTransferSyntax,
            "Transfer Syntax not supported: " + std::string(uid)
        });
    }

    // GDCM/ITK handles all supported transfer syntaxes automatically
    // through itkGDCMImageIO. If the syntax is in our supported list,
    // GDCM can decode it.
    return {};
}

} // namespace dicom_viewer::core

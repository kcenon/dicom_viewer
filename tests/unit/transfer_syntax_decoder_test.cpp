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
#include "core/transfer_syntax_decoder.hpp"

using namespace dicom_viewer::core;

class TransferSyntaxDecoderTest : public ::testing::Test {
protected:
    TransferSyntaxDecoder decoder;
};

// Test supported Transfer Syntaxes
TEST_F(TransferSyntaxDecoderTest, ImplicitVRLittleEndianIsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::ImplicitVRLittleEndian));
}

TEST_F(TransferSyntaxDecoderTest, ExplicitVRLittleEndianIsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::ExplicitVRLittleEndian));
}

TEST_F(TransferSyntaxDecoderTest, JPEGBaselineIsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::JPEGBaseline));
}

TEST_F(TransferSyntaxDecoderTest, JPEGLosslessIsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::JPEGLossless));
}

TEST_F(TransferSyntaxDecoderTest, JPEG2000LosslessIsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::JPEG2000Lossless));
}

TEST_F(TransferSyntaxDecoderTest, JPEG2000IsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::JPEG2000));
}

TEST_F(TransferSyntaxDecoderTest, JPEGLSLosslessIsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::JPEGLSLossless));
}

TEST_F(TransferSyntaxDecoderTest, RLELosslessIsSupported) {
    EXPECT_TRUE(TransferSyntaxDecoder::isSupported(transfer_syntax::RLELossless));
}

// Test unsupported Transfer Syntax
TEST_F(TransferSyntaxDecoderTest, UnknownTransferSyntaxNotSupported) {
    EXPECT_FALSE(TransferSyntaxDecoder::isSupported("1.2.3.4.5.6.7.8.9"));
    EXPECT_FALSE(TransferSyntaxDecoder::isSupported(""));
    EXPECT_FALSE(TransferSyntaxDecoder::isSupported("invalid"));
}

// Test Transfer Syntax Info retrieval
TEST_F(TransferSyntaxDecoderTest, GetTransferSyntaxInfoReturnsCorrectData) {
    auto info = TransferSyntaxDecoder::getTransferSyntaxInfo(transfer_syntax::JPEGBaseline);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->uid, "1.2.840.10008.1.2.4.50");
    EXPECT_EQ(info->name, "JPEG Baseline (Process 1)");
    EXPECT_EQ(info->category, TransferSyntaxCategory::LossyCompression);
    EXPECT_EQ(info->compressionType, CompressionType::JPEG);
}

TEST_F(TransferSyntaxDecoderTest, GetTransferSyntaxInfoReturnsNulloptForUnknown) {
    auto info = TransferSyntaxDecoder::getTransferSyntaxInfo("unknown.uid");
    EXPECT_FALSE(info.has_value());
}

// Test compression detection
TEST_F(TransferSyntaxDecoderTest, IsCompressedReturnsTrueForCompressed) {
    EXPECT_TRUE(TransferSyntaxDecoder::isCompressed(transfer_syntax::JPEGBaseline));
    EXPECT_TRUE(TransferSyntaxDecoder::isCompressed(transfer_syntax::JPEG2000Lossless));
    EXPECT_TRUE(TransferSyntaxDecoder::isCompressed(transfer_syntax::RLELossless));
}

TEST_F(TransferSyntaxDecoderTest, IsCompressedReturnsFalseForUncompressed) {
    EXPECT_FALSE(TransferSyntaxDecoder::isCompressed(transfer_syntax::ImplicitVRLittleEndian));
    EXPECT_FALSE(TransferSyntaxDecoder::isCompressed(transfer_syntax::ExplicitVRLittleEndian));
}

// Test lossy compression detection
TEST_F(TransferSyntaxDecoderTest, IsLossyCompressionReturnsTrueForLossy) {
    EXPECT_TRUE(TransferSyntaxDecoder::isLossyCompression(transfer_syntax::JPEGBaseline));
    EXPECT_TRUE(TransferSyntaxDecoder::isLossyCompression(transfer_syntax::JPEG2000));
}

TEST_F(TransferSyntaxDecoderTest, IsLossyCompressionReturnsFalseForLossless) {
    EXPECT_FALSE(TransferSyntaxDecoder::isLossyCompression(transfer_syntax::JPEGLossless));
    EXPECT_FALSE(TransferSyntaxDecoder::isLossyCompression(transfer_syntax::JPEG2000Lossless));
    EXPECT_FALSE(TransferSyntaxDecoder::isLossyCompression(transfer_syntax::RLELossless));
    EXPECT_FALSE(TransferSyntaxDecoder::isLossyCompression(transfer_syntax::ImplicitVRLittleEndian));
}

// Test compression type detection
TEST_F(TransferSyntaxDecoderTest, GetCompressionTypeReturnsCorrectType) {
    EXPECT_EQ(TransferSyntaxDecoder::getCompressionType(transfer_syntax::JPEGBaseline),
              CompressionType::JPEG);
    EXPECT_EQ(TransferSyntaxDecoder::getCompressionType(transfer_syntax::JPEGLossless),
              CompressionType::JPEGLossless);
    EXPECT_EQ(TransferSyntaxDecoder::getCompressionType(transfer_syntax::JPEG2000Lossless),
              CompressionType::JPEG2000Lossless);
    EXPECT_EQ(TransferSyntaxDecoder::getCompressionType(transfer_syntax::JPEG2000),
              CompressionType::JPEG2000);
    EXPECT_EQ(TransferSyntaxDecoder::getCompressionType(transfer_syntax::JPEGLSLossless),
              CompressionType::JPEGLS);
    EXPECT_EQ(TransferSyntaxDecoder::getCompressionType(transfer_syntax::RLELossless),
              CompressionType::RLE);
    EXPECT_EQ(TransferSyntaxDecoder::getCompressionType(transfer_syntax::ImplicitVRLittleEndian),
              CompressionType::None);
}

// Test name retrieval
TEST_F(TransferSyntaxDecoderTest, GetTransferSyntaxNameReturnsCorrectName) {
    EXPECT_EQ(TransferSyntaxDecoder::getTransferSyntaxName(transfer_syntax::ImplicitVRLittleEndian),
              "Implicit VR Little Endian");
    EXPECT_EQ(TransferSyntaxDecoder::getTransferSyntaxName(transfer_syntax::RLELossless),
              "RLE Lossless");
}

TEST_F(TransferSyntaxDecoderTest, GetTransferSyntaxNameReturnsEmptyForUnknown) {
    EXPECT_EQ(TransferSyntaxDecoder::getTransferSyntaxName("unknown"), "");
}

// Test supported UIDs list
TEST_F(TransferSyntaxDecoderTest, GetSupportedUIDsReturnsAllUIDs) {
    auto uids = TransferSyntaxDecoder::getSupportedUIDs();

    // Should have at least the 8 required UIDs from the issue
    EXPECT_GE(uids.size(), 8u);

    // Verify required UIDs are present
    auto contains = [&uids](const std::string& uid) {
        return std::find(uids.begin(), uids.end(), uid) != uids.end();
    };

    EXPECT_TRUE(contains("1.2.840.10008.1.2"));       // Implicit VR LE
    EXPECT_TRUE(contains("1.2.840.10008.1.2.1"));     // Explicit VR LE
    EXPECT_TRUE(contains("1.2.840.10008.1.2.4.50"));  // JPEG Baseline
    EXPECT_TRUE(contains("1.2.840.10008.1.2.4.70"));  // JPEG Lossless
    EXPECT_TRUE(contains("1.2.840.10008.1.2.4.90"));  // JPEG 2000 Lossless
    EXPECT_TRUE(contains("1.2.840.10008.1.2.4.91"));  // JPEG 2000
    EXPECT_TRUE(contains("1.2.840.10008.1.2.4.80"));  // JPEG-LS Lossless
    EXPECT_TRUE(contains("1.2.840.10008.1.2.5"));     // RLE Lossless
}

// Test validation
TEST_F(TransferSyntaxDecoderTest, ValidateDecodingSucceedsForSupportedSyntax) {
    auto result = decoder.validateDecoding(transfer_syntax::ImplicitVRLittleEndian);
    EXPECT_TRUE(result.has_value());

    result = decoder.validateDecoding(transfer_syntax::JPEGBaseline);
    EXPECT_TRUE(result.has_value());

    result = decoder.validateDecoding(transfer_syntax::JPEG2000Lossless);
    EXPECT_TRUE(result.has_value());
}

TEST_F(TransferSyntaxDecoderTest, ValidateDecodingFailsForUnsupportedSyntax) {
    auto result = decoder.validateDecoding("1.2.3.4.5.6.7.8.9");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TransferSyntaxError::UnsupportedTransferSyntax);
}

// Test move semantics
TEST_F(TransferSyntaxDecoderTest, MoveConstructorWorks) {
    TransferSyntaxDecoder original;
    TransferSyntaxDecoder moved(std::move(original));

    auto result = moved.validateDecoding(transfer_syntax::ExplicitVRLittleEndian);
    EXPECT_TRUE(result.has_value());
}

TEST_F(TransferSyntaxDecoderTest, MoveAssignmentWorks) {
    TransferSyntaxDecoder original;
    TransferSyntaxDecoder assigned;
    assigned = std::move(original);

    auto result = assigned.validateDecoding(transfer_syntax::JPEGLossless);
    EXPECT_TRUE(result.has_value());
}

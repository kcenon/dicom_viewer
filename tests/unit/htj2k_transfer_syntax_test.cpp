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

#include <services/dicom_store_scp.hpp>

#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/compression/htj2k_codec.hpp>
#include <pacs/encoding/compression/codec_factory.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace {

using namespace dicom_viewer::services;

class Htj2kTransferSyntaxTest : public ::testing::Test {};

// --- HTJ2K Transfer Syntax UID Constants ---

TEST_F(Htj2kTransferSyntaxTest, LosslessUidCorrect) {
    EXPECT_STREQ(DicomStoreSCP::HTJ2K_LOSSLESS, "1.2.840.10008.1.2.4.201");
}

TEST_F(Htj2kTransferSyntaxTest, RpclUidCorrect) {
    EXPECT_STREQ(DicomStoreSCP::HTJ2K_RPCL, "1.2.840.10008.1.2.4.202");
}

TEST_F(Htj2kTransferSyntaxTest, LossyUidCorrect) {
    EXPECT_STREQ(DicomStoreSCP::HTJ2K_LOSSY, "1.2.840.10008.1.2.4.203");
}

// --- Supported Transfer Syntaxes List ---

TEST_F(Htj2kTransferSyntaxTest, SupportedTransferSyntaxesIncludesHtj2k) {
    auto ts = DicomStoreSCP::getSupportedTransferSyntaxes();

    auto contains = [&](const std::string& uid) {
        return std::find(ts.begin(), ts.end(), uid) != ts.end();
    };

    EXPECT_TRUE(contains(DicomStoreSCP::HTJ2K_LOSSLESS))
        << "Supported transfer syntaxes should include HTJ2K Lossless";
    EXPECT_TRUE(contains(DicomStoreSCP::HTJ2K_RPCL))
        << "Supported transfer syntaxes should include HTJ2K RPCL";
    EXPECT_TRUE(contains(DicomStoreSCP::HTJ2K_LOSSY))
        << "Supported transfer syntaxes should include HTJ2K Lossy";
}

TEST_F(Htj2kTransferSyntaxTest, SupportedTransferSyntaxesIncludesStandard) {
    auto ts = DicomStoreSCP::getSupportedTransferSyntaxes();

    auto contains = [&](const std::string& uid) {
        return std::find(ts.begin(), ts.end(), uid) != ts.end();
    };

    EXPECT_TRUE(contains("1.2.840.10008.1.2"))
        << "Should include Implicit VR Little Endian";
    EXPECT_TRUE(contains("1.2.840.10008.1.2.1"))
        << "Should include Explicit VR Little Endian";
}

TEST_F(Htj2kTransferSyntaxTest, SupportedTransferSyntaxesNotEmpty) {
    auto ts = DicomStoreSCP::getSupportedTransferSyntaxes();
    EXPECT_GE(ts.size(), 6u)
        << "Should have at least 6 transfer syntaxes (3 standard + 3 HTJ2K)";
}

// --- pacs_system Transfer Syntax Registry ---

TEST_F(Htj2kTransferSyntaxTest, Htj2kLosslessRegisteredInPacs) {
    auto ts = kcenon::pacs::encoding::find_transfer_syntax("1.2.840.10008.1.2.4.201");
    ASSERT_TRUE(ts.has_value())
        << "HTJ2K Lossless should be registered in pacs_system";
    EXPECT_TRUE(ts->is_encapsulated())
        << "HTJ2K Lossless should be encapsulated";
    EXPECT_TRUE(ts->is_valid());
}

TEST_F(Htj2kTransferSyntaxTest, Htj2kRpclRegisteredInPacs) {
    auto ts = kcenon::pacs::encoding::find_transfer_syntax("1.2.840.10008.1.2.4.202");
    ASSERT_TRUE(ts.has_value())
        << "HTJ2K RPCL should be registered in pacs_system";
    EXPECT_TRUE(ts->is_encapsulated());
}

TEST_F(Htj2kTransferSyntaxTest, Htj2kLossyRegisteredInPacs) {
    auto ts = kcenon::pacs::encoding::find_transfer_syntax("1.2.840.10008.1.2.4.203");
    ASSERT_TRUE(ts.has_value())
        << "HTJ2K Lossy should be registered in pacs_system";
    EXPECT_TRUE(ts->is_encapsulated());
}

// --- HTJ2K Codec Properties ---

TEST_F(Htj2kTransferSyntaxTest, CodecLosslessMode) {
    kcenon::pacs::encoding::compression::htj2k_codec codec(true, false);
    EXPECT_TRUE(codec.is_lossless_mode());
    EXPECT_FALSE(codec.is_lossy());
    EXPECT_FALSE(codec.is_rpcl_mode());
    EXPECT_EQ(codec.transfer_syntax_uid(), "1.2.840.10008.1.2.4.201");
}

TEST_F(Htj2kTransferSyntaxTest, CodecRpclMode) {
    kcenon::pacs::encoding::compression::htj2k_codec codec(true, true);
    EXPECT_TRUE(codec.is_lossless_mode());
    EXPECT_FALSE(codec.is_lossy());
    EXPECT_TRUE(codec.is_rpcl_mode());
    EXPECT_EQ(codec.transfer_syntax_uid(), "1.2.840.10008.1.2.4.202");
}

TEST_F(Htj2kTransferSyntaxTest, CodecLossyMode) {
    kcenon::pacs::encoding::compression::htj2k_codec codec(false, false);
    EXPECT_FALSE(codec.is_lossless_mode());
    EXPECT_TRUE(codec.is_lossy());
    EXPECT_EQ(codec.transfer_syntax_uid(), "1.2.840.10008.1.2.4.203");
}

TEST_F(Htj2kTransferSyntaxTest, CodecDefaultCompressionRatio) {
    kcenon::pacs::encoding::compression::htj2k_codec codec;
    EXPECT_FLOAT_EQ(codec.compression_ratio(),
                    kcenon::pacs::encoding::compression::htj2k_codec::kDefaultCompressionRatio);
}

TEST_F(Htj2kTransferSyntaxTest, CodecDefaultResolutionLevels) {
    kcenon::pacs::encoding::compression::htj2k_codec codec;
    EXPECT_EQ(codec.resolution_levels(),
              kcenon::pacs::encoding::compression::htj2k_codec::kDefaultResolutionLevels);
}

TEST_F(Htj2kTransferSyntaxTest, CodecCustomCompressionRatio) {
    kcenon::pacs::encoding::compression::htj2k_codec codec(false, false, 10.0f);
    EXPECT_FLOAT_EQ(codec.compression_ratio(), 10.0f);
}

TEST_F(Htj2kTransferSyntaxTest, CodecNameNotEmpty) {
    kcenon::pacs::encoding::compression::htj2k_codec codec;
    EXPECT_FALSE(codec.name().empty());
}

// --- Codec Factory ---

TEST_F(Htj2kTransferSyntaxTest, CodecFactoryCreatesHtj2kLossless) {
    auto codec = kcenon::pacs::encoding::compression::codec_factory::create("1.2.840.10008.1.2.4.201");
    EXPECT_NE(codec, nullptr)
        << "Codec factory should create HTJ2K Lossless codec";
}

TEST_F(Htj2kTransferSyntaxTest, CodecFactoryCreatesHtj2kRpcl) {
    auto codec = kcenon::pacs::encoding::compression::codec_factory::create("1.2.840.10008.1.2.4.202");
    EXPECT_NE(codec, nullptr)
        << "Codec factory should create HTJ2K RPCL codec";
}

TEST_F(Htj2kTransferSyntaxTest, CodecFactoryCreatesHtj2kLossy) {
    auto codec = kcenon::pacs::encoding::compression::codec_factory::create("1.2.840.10008.1.2.4.203");
    EXPECT_NE(codec, nullptr)
        << "Codec factory should create HTJ2K Lossy codec";
}

TEST_F(Htj2kTransferSyntaxTest, CodecFactoryReportsHtj2kSupported) {
    EXPECT_TRUE(kcenon::pacs::encoding::compression::codec_factory::is_supported("1.2.840.10008.1.2.4.201"));
    EXPECT_TRUE(kcenon::pacs::encoding::compression::codec_factory::is_supported("1.2.840.10008.1.2.4.202"));
    EXPECT_TRUE(kcenon::pacs::encoding::compression::codec_factory::is_supported("1.2.840.10008.1.2.4.203"));
}

// --- SOP Classes Still Correct ---

TEST_F(Htj2kTransferSyntaxTest, SopClassesUnchanged) {
    auto sop = DicomStoreSCP::getSupportedSopClasses();
    EXPECT_EQ(sop.size(), 5u)
        << "SOP classes list should remain unchanged (5 classes)";
}

} // anonymous namespace

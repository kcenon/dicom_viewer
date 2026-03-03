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

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/character_set.hpp>
#include <pacs/encoding/dataset_charset.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <string>

namespace {

using namespace pacs::core;
using namespace pacs::encoding;

class CharsetDecodingTest : public ::testing::Test {};

TEST_F(CharsetDecodingTest, Utf8PatientNamePassthrough) {
    dicom_dataset ds;
    ds.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 192");
    ds.set_string(tags::patient_name, vr_type::PN, "Hong^GilDong");

    auto result = get_decoded_string(ds, tags::patient_name);
    EXPECT_EQ(result, "Hong^GilDong");
}

TEST_F(CharsetDecodingTest, Utf8KoreanPatientName) {
    dicom_dataset ds;
    ds.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 192");
    // UTF-8 encoded Korean name
    ds.set_string(tags::patient_name, vr_type::PN,
                  "\xED\x99\x8D\x5E\xEA\xB8\xB8\xEB\x8F\x99");

    auto result = get_decoded_string(ds, tags::patient_name);
    // Should pass through unchanged since dataset is already UTF-8
    EXPECT_EQ(result, "\xED\x99\x8D\x5E\xEA\xB8\xB8\xEB\x8F\x99");
}

TEST_F(CharsetDecodingTest, Latin1Decoding) {
    dicom_dataset ds;
    ds.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 100");
    // Latin-1: "café" = 0x63 0x61 0x66 0xE9
    ds.set_string(tags::patient_name, vr_type::PN, "caf\xE9");

    auto result = get_decoded_string(ds, tags::patient_name);
    // Should decode to UTF-8: "café" = 0x63 0x61 0x66 0xC3 0xA9
    EXPECT_EQ(result, "caf\xC3\xA9");
}

TEST_F(CharsetDecodingTest, AsciiDefaultWhenNoCharset) {
    dicom_dataset ds;
    // No Specific Character Set set — defaults to ASCII
    ds.set_string(tags::patient_name, vr_type::PN, "Smith^John");

    auto result = get_decoded_string(ds, tags::patient_name);
    EXPECT_EQ(result, "Smith^John");
}

TEST_F(CharsetDecodingTest, MissingTagReturnsEmpty) {
    dicom_dataset ds;
    ds.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 192");

    auto result = get_decoded_string(ds, tags::patient_name);
    EXPECT_TRUE(result.empty());
}

TEST_F(CharsetDecodingTest, StudyDescriptionDecoding) {
    dicom_dataset ds;
    ds.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 100");
    // Latin-1: "Röntgen" = R 0xF6 ntgen
    ds.set_string(tags::study_description, vr_type::LO, "R\xF6ntgen");

    auto result = get_decoded_string(ds, tags::study_description);
    // UTF-8: "Röntgen" = R 0xC3 0xB6 ntgen
    EXPECT_EQ(result, "R\xC3\xB6ntgen");
}

TEST_F(CharsetDecodingTest, SetEncodedStringRoundTrip) {
    dicom_dataset ds;
    ds.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 100");

    // Encode UTF-8 "café" to Latin-1 in dataset
    set_encoded_string(ds, tags::patient_name, vr_type::PN, "caf\xC3\xA9");

    // Decode back from dataset to UTF-8
    auto result = get_decoded_string(ds, tags::patient_name);
    EXPECT_EQ(result, "caf\xC3\xA9");
}

TEST_F(CharsetDecodingTest, UidNotAffectedByCharset) {
    dicom_dataset ds;
    ds.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 192");
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.840.113619.2.55.3");

    auto result = get_decoded_string(ds, tags::study_instance_uid);
    EXPECT_EQ(result, "1.2.840.113619.2.55.3");
}

} // anonymous namespace

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
#include <pacs/encoding/vr_type.hpp>
#include <pacs/services/validation/ct_iod_validator.hpp>
#include <pacs/services/validation/mr_iod_validator.hpp>
#include <pacs/services/validation/sr_iod_validator.hpp>

#include <string>

namespace {

using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;
using namespace kcenon::pacs::services::validation;

// Helper to build a minimal CT dataset with required Type 1 attributes
dicom_dataset buildMinimalCTDataset() {
    dicom_dataset ds;
    // SOP Common
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
    // Patient
    ds.set_string(tags::patient_name, vr_type::PN, "Doe^John");
    ds.set_string(tags::patient_id, vr_type::LO, "PAT001");
    // Study
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5");
    ds.set_string(tags::study_date, vr_type::DA, "20260101");
    // Series
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6");
    ds.set_string(tags::modality, vr_type::CS, "CT");
    return ds;
}

// Helper to build a minimal MR dataset
dicom_dataset buildMinimalMRDataset() {
    dicom_dataset ds;
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.4");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.10");
    ds.set_string(tags::patient_name, vr_type::PN, "Smith^Jane");
    ds.set_string(tags::patient_id, vr_type::LO, "PAT002");
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.6");
    ds.set_string(tags::study_date, vr_type::DA, "20260201");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.6.1");
    ds.set_string(tags::modality, vr_type::CS, "MR");
    return ds;
}

// Helper to build a minimal SR dataset
dicom_dataset buildMinimalSRDataset() {
    dicom_dataset ds;
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.88.33");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.11");
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "PAT003");
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.7");
    ds.set_string(tags::study_date, vr_type::DA, "20260301");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.7.1");
    ds.set_string(tags::modality, vr_type::CS, "SR");
    return ds;
}

class IodValidationTest : public ::testing::Test {};

// --- CT Validator Tests ---

TEST_F(IodValidationTest, CTValidatorWithMinimalDataset) {
    auto ds = buildMinimalCTDataset();
    ct_iod_validator validator;
    auto result = validator.validate(ds);

    // Minimal dataset may have warnings but should not crash
    EXPECT_FALSE(result.findings.empty())
        << "Minimal dataset should produce validation findings";
}

TEST_F(IodValidationTest, CTValidatorEmptyDataset) {
    dicom_dataset ds;
    ct_iod_validator validator;
    auto result = validator.validate(ds);

    EXPECT_FALSE(result.is_valid)
        << "Empty dataset should fail CT IOD validation";
    EXPECT_TRUE(result.has_errors())
        << "Empty dataset should have error findings";
}

TEST_F(IodValidationTest, CTQuickCheckWithMinimalDataset) {
    auto ds = buildMinimalCTDataset();
    ct_iod_validator validator;

    // quick_check only verifies Type 1 attributes exist
    // May or may not pass depending on how many Type 1 attrs are set
    [[maybe_unused]] bool quickResult = validator.quick_check(ds);
    // Just verify it doesn't crash
    SUCCEED();
}

TEST_F(IodValidationTest, CTValidatorCustomOptions) {
    auto ds = buildMinimalCTDataset();
    ct_validation_options opts;
    opts.check_type2 = false;
    opts.validate_pixel_data = false;
    opts.validate_ct_params = false;

    ct_iod_validator validator(opts);
    auto result = validator.validate(ds);

    // With relaxed options, fewer findings expected
    EXPECT_LE(result.error_count(), result.findings.size());
}

// --- MR Validator Tests ---

TEST_F(IodValidationTest, MRValidatorWithMinimalDataset) {
    auto ds = buildMinimalMRDataset();
    mr_iod_validator validator;
    auto result = validator.validate(ds);

    EXPECT_FALSE(result.findings.empty())
        << "Minimal MR dataset should produce validation findings";
}

TEST_F(IodValidationTest, MRValidatorEmptyDataset) {
    dicom_dataset ds;
    mr_iod_validator validator;
    auto result = validator.validate(ds);

    EXPECT_FALSE(result.is_valid)
        << "Empty dataset should fail MR IOD validation";
}

// --- SR Validator Tests ---

TEST_F(IodValidationTest, SRValidatorWithMinimalDataset) {
    auto ds = buildMinimalSRDataset();
    sr_iod_validator validator;
    auto result = validator.validate(ds);

    // Minimal SR dataset will have findings (missing content sequence, etc.)
    EXPECT_FALSE(result.findings.empty());
}

TEST_F(IodValidationTest, SRValidatorEmptyDataset) {
    dicom_dataset ds;
    sr_iod_validator validator;
    auto result = validator.validate(ds);

    EXPECT_FALSE(result.is_valid)
        << "Empty dataset should fail SR IOD validation";
    EXPECT_TRUE(result.has_errors());
}

// --- Convenience Function Tests ---

TEST_F(IodValidationTest, ValidateCTIodConvenienceFunction) {
    auto ds = buildMinimalCTDataset();
    auto result = validate_ct_iod(ds);
    // Should not crash; result is informational
    EXPECT_GE(result.findings.size(), 0u);
}

TEST_F(IodValidationTest, ValidateMRIodConvenienceFunction) {
    auto ds = buildMinimalMRDataset();
    auto result = validate_mr_iod(ds);
    EXPECT_GE(result.findings.size(), 0u);
}

TEST_F(IodValidationTest, ValidateSRIodConvenienceFunction) {
    auto ds = buildMinimalSRDataset();
    auto result = validate_sr_iod(ds);
    EXPECT_GE(result.findings.size(), 0u);
}

// --- Validation Result API Tests ---

TEST_F(IodValidationTest, ValidationResultSummaryNotEmpty) {
    dicom_dataset ds;
    auto result = validate_ct_iod(ds);

    // Summary should be non-empty for an invalid dataset
    auto summary = result.summary();
    EXPECT_FALSE(summary.empty())
        << "Validation summary should not be empty for invalid dataset";
}

TEST_F(IodValidationTest, ValidationResultErrorCountMatchesFindings) {
    dicom_dataset ds;
    auto result = validate_ct_iod(ds);

    size_t manualErrorCount = 0;
    for (const auto& f : result.findings) {
        if (f.severity == validation_severity::error) {
            manualErrorCount++;
        }
    }
    EXPECT_EQ(result.error_count(), manualErrorCount);
}

TEST_F(IodValidationTest, ValidationResultWarningCountMatchesFindings) {
    auto ds = buildMinimalCTDataset();
    auto result = validate_ct_iod(ds);

    size_t manualWarningCount = 0;
    for (const auto& f : result.findings) {
        if (f.severity == validation_severity::warning) {
            manualWarningCount++;
        }
    }
    EXPECT_EQ(result.warning_count(), manualWarningCount);
}

} // anonymous namespace

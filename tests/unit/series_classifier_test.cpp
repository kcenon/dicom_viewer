#include "services/enhanced_dicom/series_classifier.hpp"

#include <gtest/gtest.h>
#include <itkMetaDataObject.h>

using namespace dicom_viewer::services;

namespace {

/// Helper to build a mock ITK MetaDataDictionary with given tags
class MockDicomBuilder {
public:
    MockDicomBuilder& set(const std::string& key, const std::string& value) {
        itk::EncapsulateMetaData<std::string>(dict_, key, value);
        return *this;
    }

    /// Convenience setters for common tags
    MockDicomBuilder& modality(const std::string& v)          { return set("0008|0060", v); }
    MockDicomBuilder& seriesDescription(const std::string& v) { return set("0008|103e", v); }
    MockDicomBuilder& imageType(const std::string& v)         { return set("0008|0008", v); }
    MockDicomBuilder& seriesUid(const std::string& v)         { return set("0020|000e", v); }
    MockDicomBuilder& scanningSequence(const std::string& v)  { return set("0018|0020", v); }
    MockDicomBuilder& phaseContrast(const std::string& v)     { return set("0018|9014", v); }
    MockDicomBuilder& manufacturer(const std::string& v)      { return set("0008|0070", v); }
    MockDicomBuilder& numberOfFrames(const std::string& v)    { return set("0028|0008", v); }
    MockDicomBuilder& siemensFlowDir(const std::string& v)    { return set("0051|1014", v); }
    MockDicomBuilder& philipsVenc(const std::string& v)       { return set("2001|101a", v); }
    MockDicomBuilder& geVenc(const std::string& v)            { return set("0019|10cc", v); }

    const itk::MetaDataDictionary& build() const { return dict_; }

private:
    itk::MetaDataDictionary dict_;
};

}  // anonymous namespace

// =============================================================================
// CT Detection
// =============================================================================

TEST(SeriesClassifierTest, CTDetectedByModality) {
    auto dict = MockDicomBuilder()
        .modality("CT")
        .seriesDescription("Body CT Angio")
        .seriesUid("1.2.3.4")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::CT);
    EXPECT_FALSE(result.is4DFlow);
    EXPECT_EQ(result.seriesUid, "1.2.3.4");
}

// =============================================================================
// DIXON Detection
// =============================================================================

TEST(SeriesClassifierTest, DIXONDetectedByDescription) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("t1_vibe_dixon_tra_bh_W")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::DIXON);
    EXPECT_FALSE(result.is4DFlow);
}

// =============================================================================
// StarVIBE Detection
// =============================================================================

TEST(SeriesClassifierTest, StarvibeDetectedByDescription) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("fl3d_starvibe_contrast")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Starvibe);
    EXPECT_FALSE(result.is4DFlow);
}

// =============================================================================
// TOF Detection
// =============================================================================

TEST(SeriesClassifierTest, TOFDetectedByDescription) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("TOF_3D_multi-slab")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::TOF);
    EXPECT_FALSE(result.is4DFlow);
}

// =============================================================================
// CINE Detection
// =============================================================================

TEST(SeriesClassifierTest, CINEDetectedByDescription) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("CINE_retro_SA_stack")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::CINE);
    EXPECT_FALSE(result.is4DFlow);
}

TEST(SeriesClassifierTest, CINENotConfusedWithPhaseContrast) {
    // CINE description but with PC scanning sequence → should be 4D Flow
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("CINE_PC_4Dflow")
        .scanningSequence("GR\\PC")
        .imageType("ORIGINAL\\PRIMARY\\P\\ND")
        .build();

    auto result = SeriesClassifier::classify(dict);
    // Phase contrast takes priority over CINE description
    EXPECT_TRUE(SeriesClassifier::is4DFlowType(result.type));
    EXPECT_TRUE(result.is4DFlow);
}

// =============================================================================
// 4D Flow — Siemens
// =============================================================================

TEST(SeriesClassifierTest, Flow4DMagnitudeSiemens) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("fl3d1r21_4DFlow")
        .scanningSequence("GR\\PC")
        .imageType("ORIGINAL\\PRIMARY\\M\\ND")
        .manufacturer("SIEMENS")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Flow4D_Magnitude);
    EXPECT_TRUE(result.is4DFlow);
}

TEST(SeriesClassifierTest, Flow4DPhaseAPSiemens) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("fl3d1r21_4DFlow_Phase_AP")
        .scanningSequence("GR\\PC")
        .imageType("ORIGINAL\\PRIMARY\\P\\ND")
        .manufacturer("SIEMENS")
        .siemensFlowDir("tp 0.0 AP 150.0")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Flow4D_Phase_AP);
    EXPECT_TRUE(result.is4DFlow);
}

TEST(SeriesClassifierTest, Flow4DPhaseFHSiemens) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("fl3d1r21_4DFlow_Phase_FH")
        .scanningSequence("GR\\PC")
        .imageType("ORIGINAL\\PRIMARY\\P\\ND")
        .manufacturer("SIEMENS")
        .siemensFlowDir("tp 0.0 FH 150.0")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Flow4D_Phase_FH);
    EXPECT_TRUE(result.is4DFlow);
}

TEST(SeriesClassifierTest, Flow4DPhaseRLSiemens) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("fl3d1r21_4DFlow_Phase_RL")
        .scanningSequence("GR\\PC")
        .imageType("ORIGINAL\\PRIMARY\\P\\ND")
        .manufacturer("SIEMENS")
        .siemensFlowDir("tp 0.0 RL 150.0")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Flow4D_Phase_RL);
    EXPECT_TRUE(result.is4DFlow);
}

// =============================================================================
// 4D Flow — Philips
// =============================================================================

TEST(SeriesClassifierTest, Flow4DPhaseAPPhilips) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("4DFlow_Phase_AP_150")
        .scanningSequence("PC")
        .imageType("ORIGINAL\\PRIMARY\\P\\NONE")
        .manufacturer("Philips Medical Systems")
        .philipsVenc("150")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Flow4D_Phase_AP);
    EXPECT_TRUE(result.is4DFlow);
}

// =============================================================================
// 4D Flow — GE
// =============================================================================

TEST(SeriesClassifierTest, Flow4DPhaseRLGE) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("4DFlow_RL_VENC150")
        .scanningSequence("GR\\PC")
        .imageType("ORIGINAL\\PRIMARY\\P\\NONE")
        .manufacturer("GE MEDICAL SYSTEMS")
        .geVenc("150")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Flow4D_Phase_RL);
    EXPECT_TRUE(result.is4DFlow);
}

// =============================================================================
// 2D VENC Detection
// =============================================================================

TEST(SeriesClassifierTest, VENC2DDetected) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("2D_VENC_through_plane")
        .scanningSequence("GR\\PC")
        .imageType("ORIGINAL\\PRIMARY\\P\\ND")
        .numberOfFrames("1")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::VENC_2D);
    EXPECT_FALSE(result.is4DFlow);
}

// =============================================================================
// PC-MRA Detection
// =============================================================================

TEST(SeriesClassifierTest, PCMRADetected) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("PC-MRA_reconstruction")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::PC_MRA);
    EXPECT_FALSE(result.is4DFlow);
}

TEST(SeriesClassifierTest, AngioDetectedAsPCMRA) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("MR_Angio_3D_sagittal")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::PC_MRA);
    EXPECT_FALSE(result.is4DFlow);
}

// =============================================================================
// Unknown Series
// =============================================================================

TEST(SeriesClassifierTest, UnknownSeriesType) {
    auto dict = MockDicomBuilder()
        .modality("MR")
        .seriesDescription("t2_tse_tra")
        .imageType("ORIGINAL\\PRIMARY\\M\\ND")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Unknown);
    EXPECT_FALSE(result.is4DFlow);
}

TEST(SeriesClassifierTest, EmptyMetadataReturnsUnknown) {
    itk::MetaDataDictionary dict;
    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.type, SeriesType::Unknown);
    EXPECT_FALSE(result.is4DFlow);
}

// =============================================================================
// Utility functions
// =============================================================================

TEST(SeriesClassifierTest, Is4DFlowTypeCheck) {
    EXPECT_TRUE(SeriesClassifier::is4DFlowType(SeriesType::Flow4D_Magnitude));
    EXPECT_TRUE(SeriesClassifier::is4DFlowType(SeriesType::Flow4D_Phase_AP));
    EXPECT_TRUE(SeriesClassifier::is4DFlowType(SeriesType::Flow4D_Phase_FH));
    EXPECT_TRUE(SeriesClassifier::is4DFlowType(SeriesType::Flow4D_Phase_RL));
    EXPECT_FALSE(SeriesClassifier::is4DFlowType(SeriesType::CT));
    EXPECT_FALSE(SeriesClassifier::is4DFlowType(SeriesType::CINE));
    EXPECT_FALSE(SeriesClassifier::is4DFlowType(SeriesType::DIXON));
    EXPECT_FALSE(SeriesClassifier::is4DFlowType(SeriesType::Unknown));
}

TEST(SeriesClassifierTest, SeriesToStringCoversAllTypes) {
    EXPECT_EQ(seriesToString(SeriesType::Flow4D_Magnitude), "4D Flow Magnitude");
    EXPECT_EQ(seriesToString(SeriesType::Flow4D_Phase_AP), "4D Flow Phase AP");
    EXPECT_EQ(seriesToString(SeriesType::Flow4D_Phase_FH), "4D Flow Phase FH");
    EXPECT_EQ(seriesToString(SeriesType::Flow4D_Phase_RL), "4D Flow Phase RL");
    EXPECT_EQ(seriesToString(SeriesType::PC_MRA), "PC-MRA");
    EXPECT_EQ(seriesToString(SeriesType::CINE), "CINE");
    EXPECT_EQ(seriesToString(SeriesType::DIXON), "DIXON");
    EXPECT_EQ(seriesToString(SeriesType::Starvibe), "StarVIBE");
    EXPECT_EQ(seriesToString(SeriesType::CT), "CT");
    EXPECT_EQ(seriesToString(SeriesType::TOF), "TOF");
    EXPECT_EQ(seriesToString(SeriesType::VENC_2D), "2D VENC");
    EXPECT_EQ(seriesToString(SeriesType::Unknown), "Unknown");
}

// =============================================================================
// Metadata preservation
// =============================================================================

TEST(SeriesClassifierTest, MetadataFieldsPreserved) {
    auto dict = MockDicomBuilder()
        .modality("CT")
        .seriesDescription("Chest CT w/ contrast")
        .seriesUid("1.2.840.113619.2.55.1234")
        .build();

    auto result = SeriesClassifier::classify(dict);
    EXPECT_EQ(result.modality, "CT");
    EXPECT_EQ(result.description, "Chest CT w/ contrast");
    EXPECT_EQ(result.seriesUid, "1.2.840.113619.2.55.1234");
}

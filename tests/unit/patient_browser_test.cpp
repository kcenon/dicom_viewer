#include <gtest/gtest.h>

#include <QApplication>
#include <QTreeWidget>

#include "ui/panels/patient_browser.hpp"

using namespace dicom_viewer::ui;

namespace {

// QApplication must exist for QWidget instantiation
int argc = 0;
char* argv[] = {nullptr};
QApplication app(argc, argv);

PatientInfo makePatient(const QString& id, const QString& name) {
    PatientInfo p;
    p.patientId = id;
    p.patientName = name;
    p.birthDate = "19800101";
    p.sex = "M";
    return p;
}

StudyInfo makeStudy(const QString& uid, const QString& desc) {
    StudyInfo s;
    s.studyInstanceUid = uid;
    s.studyDate = "20250101";
    s.studyDescription = desc;
    s.accessionNumber = "ACC001";
    s.modality = "MR";
    return s;
}

SeriesInfo makeSeries(const QString& uid, const QString& desc,
                      const QString& type = QString(), bool is4d = false) {
    SeriesInfo s;
    s.seriesInstanceUid = uid;
    s.seriesNumber = "1";
    s.seriesDescription = desc;
    s.modality = "MR";
    s.numberOfImages = 100;
    s.seriesType = type;
    s.is4DFlow = is4d;
    return s;
}

}  // anonymous namespace

// =============================================================================
// SeriesInfo classification fields
// =============================================================================

TEST(PatientBrowserTest, SeriesInfo_DefaultFields) {
    SeriesInfo info;
    EXPECT_TRUE(info.seriesType.isEmpty());
    EXPECT_FALSE(info.is4DFlow);
}

TEST(PatientBrowserTest, SeriesInfo_ClassificationFields) {
    SeriesInfo info;
    info.seriesType = "4D Flow Magnitude";
    info.is4DFlow = true;
    EXPECT_EQ(info.seriesType, "4D Flow Magnitude");
    EXPECT_TRUE(info.is4DFlow);
}

// =============================================================================
// Tree display with classification type suffix
// =============================================================================

class PatientBrowserTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        browser = std::make_unique<PatientBrowser>();
        browser->addPatient(makePatient("P001", "Test Patient"));
        browser->addStudy("P001", makeStudy("STUDY001", "Brain MRI"));
    }

    // Find the first series item in the tree (third level)
    QTreeWidgetItem* findSeriesItem() {
        auto* tree = browser->findChild<QTreeWidget*>();
        if (!tree || tree->topLevelItemCount() == 0) return nullptr;
        auto* patientItem = tree->topLevelItem(0);
        if (!patientItem || patientItem->childCount() == 0) return nullptr;
        auto* studyItem = patientItem->child(0);
        if (!studyItem || studyItem->childCount() == 0) return nullptr;
        return studyItem->child(0);
    }

    std::unique_ptr<PatientBrowser> browser;
};

TEST_F(PatientBrowserTreeTest, SeriesWithType_ShowsSuffix) {
    browser->addSeries("STUDY001",
        makeSeries("SER001", "fl3d_tra", "4D Flow Magnitude", true));

    auto* item = findSeriesItem();
    ASSERT_NE(item, nullptr);
    EXPECT_TRUE(item->text(0).contains("[4D Flow Magnitude]"));
    EXPECT_TRUE(item->text(0).startsWith("fl3d_tra"));
}

TEST_F(PatientBrowserTreeTest, SeriesWithUnknownType_NoSuffix) {
    browser->addSeries("STUDY001",
        makeSeries("SER001", "Unknown Series", "Unknown"));

    auto* item = findSeriesItem();
    ASSERT_NE(item, nullptr);
    EXPECT_FALSE(item->text(0).contains("["));
    EXPECT_EQ(item->text(0), "Unknown Series");
}

TEST_F(PatientBrowserTreeTest, SeriesWithEmptyType_NoSuffix) {
    browser->addSeries("STUDY001",
        makeSeries("SER001", "Generic Series"));

    auto* item = findSeriesItem();
    ASSERT_NE(item, nullptr);
    EXPECT_FALSE(item->text(0).contains("["));
    EXPECT_EQ(item->text(0), "Generic Series");
}

TEST_F(PatientBrowserTreeTest, SeriesWithCTType_ShowsSuffix) {
    browser->addSeries("STUDY001",
        makeSeries("SER001", "Chest CT", "CT"));

    auto* item = findSeriesItem();
    ASSERT_NE(item, nullptr);
    EXPECT_TRUE(item->text(0).contains("[CT]"));
}

TEST_F(PatientBrowserTreeTest, SeriesWithEmptyDescription_UsesSeriesNumber) {
    auto series = makeSeries("SER001", "", "CINE");
    series.seriesNumber = "5";
    browser->addSeries("STUDY001", series);

    auto* item = findSeriesItem();
    ASSERT_NE(item, nullptr);
    EXPECT_TRUE(item->text(0).contains("Series 5"));
    EXPECT_TRUE(item->text(0).contains("[CINE]"));
}

TEST_F(PatientBrowserTreeTest, ClearRemovesAllItems) {
    browser->addSeries("STUDY001",
        makeSeries("SER001", "Test", "CT"));

    browser->clear();

    auto* tree = browser->findChild<QTreeWidget*>();
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(tree->topLevelItemCount(), 0);
}

TEST_F(PatientBrowserTreeTest, SelectedSeriesUid_NoSelection) {
    browser->addSeries("STUDY001",
        makeSeries("SER001", "Test", "CT"));

    EXPECT_TRUE(browser->selectedSeriesUid().isEmpty());
}

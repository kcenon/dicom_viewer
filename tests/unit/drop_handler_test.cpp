#include "ui/drop_handler.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>
#include <QWidget>
#include <gtest/gtest.h>

using dicom_viewer::ui::DropHandler;
using dicom_viewer::ui::DropType;

namespace {

int argc = 1;
char arg0[] = "drop_handler_test";
char* argv[] = {arg0, nullptr};
QApplication app(argc, argv);

/// Create a minimal valid DICOM file (128-byte preamble + "DICM" magic)
void createDicomFile(const QString& path)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    // 128-byte preamble (zeros)
    QByteArray preamble(128, '\0');
    file.write(preamble);
    // DICM magic bytes
    file.write("DICM");
    // Minimal trailing data
    file.write(QByteArray(64, '\0'));
    file.close();
}

/// Create a non-DICOM file with arbitrary content
void createNonDicomFile(const QString& path)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("This is not a DICOM file");
    file.close();
}

} // anonymous namespace

// --- classifyPath tests ---

TEST(DropHandlerTest, ClassifyPath_EmptyPath_ReturnsUnknown)
{
    EXPECT_EQ(DropHandler::classifyPath(""), DropType::Unknown);
}

TEST(DropHandlerTest, ClassifyPath_FloFile_ReturnsProjectFile)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/test.flo"), DropType::ProjectFile);
}

TEST(DropHandlerTest, ClassifyPath_StlFile_ReturnsStlFile)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/model.stl"), DropType::StlFile);
}

TEST(DropHandlerTest, ClassifyPath_NiiFile_ReturnsMaskFile)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/mask.nii"), DropType::MaskFile);
}

TEST(DropHandlerTest, ClassifyPath_NiiGzFile_ReturnsMaskFile)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/mask.nii.gz"), DropType::MaskFile);
}

TEST(DropHandlerTest, ClassifyPath_NrrdFile_ReturnsMaskFile)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/mask.nrrd"), DropType::MaskFile);
}

TEST(DropHandlerTest, ClassifyPath_DcmFile_ReturnsDicomFolder)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/image.dcm"), DropType::DicomFolder);
}

TEST(DropHandlerTest, ClassifyPath_UnknownExtension_ReturnsUnknown)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/readme.txt"), DropType::Unknown);
}

TEST(DropHandlerTest, ClassifyPath_CaseInsensitive)
{
    EXPECT_EQ(DropHandler::classifyPath("/tmp/TEST.FLO"), DropType::ProjectFile);
    EXPECT_EQ(DropHandler::classifyPath("/tmp/Model.STL"), DropType::StlFile);
}

// --- isDicomFolder tests ---

TEST(DropHandlerTest, IsDicomFolder_NonExistentFolder_ReturnsFalse)
{
    EXPECT_FALSE(DropHandler::isDicomFolder("/nonexistent/path/xyz"));
}

TEST(DropHandlerTest, IsDicomFolder_EmptyFolder_ReturnsFalse)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    EXPECT_FALSE(DropHandler::isDicomFolder(tmpDir.path()));
}

TEST(DropHandlerTest, IsDicomFolder_FolderWithDcmExtension_ReturnsTrue)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    createNonDicomFile(tmpDir.path() + "/image.dcm");
    EXPECT_TRUE(DropHandler::isDicomFolder(tmpDir.path()));
}

TEST(DropHandlerTest, IsDicomFolder_FolderWithDicomMagicBytes_ReturnsTrue)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    // File without .dcm extension but with DICM magic
    createDicomFile(tmpDir.path() + "/image001");
    EXPECT_TRUE(DropHandler::isDicomFolder(tmpDir.path()));
}

TEST(DropHandlerTest, IsDicomFolder_FolderWithNonDicomFiles_ReturnsFalse)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    createNonDicomFile(tmpDir.path() + "/readme.txt");
    createNonDicomFile(tmpDir.path() + "/data.bin");
    EXPECT_FALSE(DropHandler::isDicomFolder(tmpDir.path()));
}

// --- classifyPath with directory ---

TEST(DropHandlerTest, ClassifyPath_DicomDirectory_ReturnsDicomFolder)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    createDicomFile(tmpDir.path() + "/slice001");
    EXPECT_EQ(DropHandler::classifyPath(tmpDir.path()), DropType::DicomFolder);
}

TEST(DropHandlerTest, ClassifyPath_EmptyDirectory_ReturnsUnknown)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    EXPECT_EQ(DropHandler::classifyPath(tmpDir.path()), DropType::Unknown);
}

// --- classifyDrop tests ---

TEST(DropHandlerTest, ClassifyDrop_NullMimeData_ReturnsUnknown)
{
    EXPECT_EQ(DropHandler::classifyDrop(nullptr), DropType::Unknown);
}

TEST(DropHandlerTest, ClassifyDrop_NoUrls_ReturnsUnknown)
{
    QMimeData mimeData;
    EXPECT_EQ(DropHandler::classifyDrop(&mimeData), DropType::Unknown);
}

TEST(DropHandlerTest, ClassifyDrop_EmptyUrls_ReturnsUnknown)
{
    QMimeData mimeData;
    mimeData.setUrls({});
    EXPECT_EQ(DropHandler::classifyDrop(&mimeData), DropType::Unknown);
}

TEST(DropHandlerTest, ClassifyDrop_FloUrl_ReturnsProjectFile)
{
    QMimeData mimeData;
    mimeData.setUrls({QUrl::fromLocalFile("/tmp/test.flo")});
    EXPECT_EQ(DropHandler::classifyDrop(&mimeData), DropType::ProjectFile);
}

TEST(DropHandlerTest, ClassifyDrop_StlUrl_ReturnsStlFile)
{
    QMimeData mimeData;
    mimeData.setUrls({QUrl::fromLocalFile("/tmp/model.stl")});
    EXPECT_EQ(DropHandler::classifyDrop(&mimeData), DropType::StlFile);
}

// --- DropHandler construction ---

TEST(DropHandlerTest, Constructor_SetsAcceptDrops)
{
    QWidget widget;
    DropHandler handler(&widget);
    EXPECT_TRUE(widget.acceptDrops());
}

TEST(DropHandlerTest, Constructor_NullTarget_NoCrash)
{
    DropHandler handler(nullptr);
    // Should not crash
}

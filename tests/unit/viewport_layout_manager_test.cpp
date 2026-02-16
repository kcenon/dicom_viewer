#include <gtest/gtest.h>

#include <QApplication>

#include "ui/viewport_layout_manager.hpp"
#include "ui/viewport_widget.hpp"

using namespace dicom_viewer::ui;

namespace {

// QApplication must exist for QWidget instantiation
int argc = 0;
char* argv[] = {nullptr};
QApplication app(argc, argv);

}  // anonymous namespace

// =============================================================================
// Construction and defaults
// =============================================================================

TEST(ViewportLayoutManagerTest, DefaultConstruction) {
    ViewportLayoutManager manager;
    EXPECT_EQ(manager.layoutMode(), LayoutMode::Single);
    EXPECT_EQ(manager.viewportCount(), 1);
    EXPECT_NE(manager.primaryViewport(), nullptr);
}

TEST(ViewportLayoutManagerTest, PrimaryViewportAlwaysValid) {
    ViewportLayoutManager manager;
    auto* primary = manager.primaryViewport();
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(manager.viewport(0), primary);
}

// =============================================================================
// Layout mode switching
// =============================================================================

TEST(ViewportLayoutManagerTest, SetLayoutModeDual) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::DualSplit);

    EXPECT_EQ(manager.layoutMode(), LayoutMode::DualSplit);
    EXPECT_EQ(manager.viewportCount(), 2);
    EXPECT_NE(manager.viewport(0), nullptr);
    EXPECT_NE(manager.viewport(1), nullptr);
}

TEST(ViewportLayoutManagerTest, SetLayoutModeQuad) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    EXPECT_EQ(manager.layoutMode(), LayoutMode::QuadSplit);
    EXPECT_EQ(manager.viewportCount(), 4);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NE(manager.viewport(i), nullptr) << "viewport " << i;
    }
}

TEST(ViewportLayoutManagerTest, SetLayoutModeSingleFromQuad) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);
    manager.setLayoutMode(LayoutMode::Single);

    EXPECT_EQ(manager.layoutMode(), LayoutMode::Single);
    EXPECT_EQ(manager.viewportCount(), 1);
}

TEST(ViewportLayoutManagerTest, ViewportOutOfRange_ReturnsNull) {
    ViewportLayoutManager manager;
    EXPECT_EQ(manager.viewport(1), nullptr);
    EXPECT_EQ(manager.viewport(-1), nullptr);
}

// =============================================================================
// Signal emission
// =============================================================================

TEST(ViewportLayoutManagerTest, LayoutModeChangedSignal) {
    ViewportLayoutManager manager;
    bool signalReceived = false;
    LayoutMode receivedMode = LayoutMode::Single;

    QObject::connect(&manager, &ViewportLayoutManager::layoutModeChanged,
                     [&](LayoutMode mode) {
        signalReceived = true;
        receivedMode = mode;
    });

    manager.setLayoutMode(LayoutMode::DualSplit);
    EXPECT_TRUE(signalReceived);
    EXPECT_EQ(receivedMode, LayoutMode::DualSplit);
}

TEST(ViewportLayoutManagerTest, SameMode_NoSignal) {
    ViewportLayoutManager manager;
    bool signalReceived = false;

    QObject::connect(&manager, &ViewportLayoutManager::layoutModeChanged,
                     [&](LayoutMode) { signalReceived = true; });

    manager.setLayoutMode(LayoutMode::Single);  // already Single
    EXPECT_FALSE(signalReceived);
}

// =============================================================================
// Quad split orientation tests
// =============================================================================

TEST(ViewportLayoutManagerTest, QuadSplit_Orientations) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    EXPECT_EQ(manager.viewport(0)->getSliceOrientation(), SliceOrientation::Axial);
    EXPECT_EQ(manager.viewport(1)->getSliceOrientation(), SliceOrientation::Sagittal);
    EXPECT_EQ(manager.viewport(2)->getSliceOrientation(), SliceOrientation::Coronal);
}

TEST(ViewportLayoutManagerTest, QuadSplit_3DViewport) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    EXPECT_EQ(manager.viewport(3)->getMode(), ViewportMode::VolumeRendering);
}

// =============================================================================
// DualSplit mode configuration
// =============================================================================

TEST(ViewportLayoutManagerTest, DualSplit_3DViewport) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::DualSplit);

    EXPECT_EQ(manager.viewport(1)->getMode(), ViewportMode::VolumeRendering);
}

// =============================================================================
// SliceOrientation tests (on ViewportWidget directly)
// =============================================================================

TEST(ViewportWidgetOrientationTest, DefaultOrientation) {
    ViewportWidget widget;
    EXPECT_EQ(widget.getSliceOrientation(), SliceOrientation::Axial);
}

TEST(ViewportWidgetOrientationTest, SetOrientation) {
    ViewportWidget widget;

    widget.setSliceOrientation(SliceOrientation::Coronal);
    EXPECT_EQ(widget.getSliceOrientation(), SliceOrientation::Coronal);

    widget.setSliceOrientation(SliceOrientation::Sagittal);
    EXPECT_EQ(widget.getSliceOrientation(), SliceOrientation::Sagittal);

    widget.setSliceOrientation(SliceOrientation::Axial);
    EXPECT_EQ(widget.getSliceOrientation(), SliceOrientation::Axial);
}

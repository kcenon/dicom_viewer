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

#include <QApplication>
#include <QSignalSpy>

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

// =============================================================================
// Active viewport tracking
// =============================================================================

TEST(ViewportLayoutManagerTest, DefaultActiveViewport) {
    ViewportLayoutManager manager;
    EXPECT_EQ(manager.activeViewportIndex(), 0);
    EXPECT_EQ(manager.activeViewport(), manager.primaryViewport());
}

TEST(ViewportLayoutManagerTest, SetActiveViewport_QuadSplit) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    manager.setActiveViewport(2);
    EXPECT_EQ(manager.activeViewportIndex(), 2);
    EXPECT_EQ(manager.activeViewport(), manager.viewport(2));
}

TEST(ViewportLayoutManagerTest, SetActiveViewport_OutOfRange_Ignored) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::DualSplit);

    manager.setActiveViewport(5);
    EXPECT_EQ(manager.activeViewportIndex(), 0);  // unchanged
}

TEST(ViewportLayoutManagerTest, ActiveViewportChanged_Signal) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    bool signalReceived = false;
    int receivedIndex = -1;
    ViewportWidget* receivedVp = nullptr;

    QObject::connect(&manager, &ViewportLayoutManager::activeViewportChanged,
                     [&](ViewportWidget* vp, int index) {
        signalReceived = true;
        receivedVp = vp;
        receivedIndex = index;
    });

    manager.setActiveViewport(3);
    EXPECT_TRUE(signalReceived);
    EXPECT_EQ(receivedIndex, 3);
    EXPECT_EQ(receivedVp, manager.viewport(3));
}

TEST(ViewportLayoutManagerTest, SetActiveViewport_SameIndex_NoSignal) {
    ViewportLayoutManager manager;
    bool signalReceived = false;

    QObject::connect(&manager, &ViewportLayoutManager::activeViewportChanged,
                     [&](ViewportWidget*, int) { signalReceived = true; });

    manager.setActiveViewport(0);  // already 0
    EXPECT_FALSE(signalReceived);
}

// =============================================================================
// Crosshair linking
// =============================================================================

TEST(ViewportLayoutManagerTest, CrosshairLink_DefaultDisabled) {
    ViewportLayoutManager manager;
    EXPECT_FALSE(manager.isCrosshairLinkEnabled());
}

TEST(ViewportLayoutManagerTest, CrosshairLink_EnableDisable) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    manager.setCrosshairLinkEnabled(true);
    EXPECT_TRUE(manager.isCrosshairLinkEnabled());

    manager.setCrosshairLinkEnabled(false);
    EXPECT_FALSE(manager.isCrosshairLinkEnabled());
}

TEST(ViewportLayoutManagerTest, CrosshairLink_Signal) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    QSignalSpy spy(&manager, &ViewportLayoutManager::crosshairLinkEnabledChanged);
    ASSERT_TRUE(spy.isValid());

    manager.setCrosshairLinkEnabled(true);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.at(0).at(0).toBool());

    manager.setCrosshairLinkEnabled(false);
    EXPECT_EQ(spy.count(), 2);
    EXPECT_FALSE(spy.at(1).at(0).toBool());
}

TEST(ViewportLayoutManagerTest, CrosshairLink_SameValue_NoSignal) {
    ViewportLayoutManager manager;

    QSignalSpy spy(&manager, &ViewportLayoutManager::crosshairLinkEnabledChanged);
    ASSERT_TRUE(spy.isValid());

    manager.setCrosshairLinkEnabled(false);  // already false
    EXPECT_EQ(spy.count(), 0);
}

TEST(ViewportLayoutManagerTest, CrosshairLink_EnableShowsLines) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    manager.setCrosshairLinkEnabled(true);
    // 2D viewports (0-2) should have crosshair lines visible
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(manager.viewport(i)->isCrosshairLinesVisible())
            << "viewport " << i;
    }
    // 3D viewport also gets lines set (no-op in rendering)
    EXPECT_TRUE(manager.viewport(3)->isCrosshairLinesVisible());
}

TEST(ViewportLayoutManagerTest, CrosshairLink_DisableHidesLines) {
    ViewportLayoutManager manager;
    manager.setLayoutMode(LayoutMode::QuadSplit);

    manager.setCrosshairLinkEnabled(true);
    manager.setCrosshairLinkEnabled(false);

    for (int i = 0; i < 4; ++i) {
        EXPECT_FALSE(manager.viewport(i)->isCrosshairLinesVisible())
            << "viewport " << i;
    }
}

TEST(ViewportLayoutManagerTest, CrosshairLink_ReconnectsOnLayoutChange) {
    ViewportLayoutManager manager;
    manager.setCrosshairLinkEnabled(true);

    // Switch to QuadSplit while linking is enabled
    manager.setLayoutMode(LayoutMode::QuadSplit);

    // All viewports should have crosshair lines visible
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(manager.viewport(i)->isCrosshairLinesVisible())
            << "viewport " << i;
    }
}

// =============================================================================
// ViewportWidget crosshair lines
// =============================================================================

TEST(ViewportWidgetCrosshairTest, CrosshairLines_DefaultHidden) {
    ViewportWidget widget;
    EXPECT_FALSE(widget.isCrosshairLinesVisible());
}

TEST(ViewportWidgetCrosshairTest, CrosshairLines_SetVisible) {
    ViewportWidget widget;
    widget.setCrosshairLinesVisible(true);
    EXPECT_TRUE(widget.isCrosshairLinesVisible());

    widget.setCrosshairLinesVisible(false);
    EXPECT_FALSE(widget.isCrosshairLinesVisible());
}

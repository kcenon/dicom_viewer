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

#include "ui/widgets/sp_mode_toggle.hpp"
#include "ui/widgets/phase_slider_widget.hpp"

using namespace dicom_viewer::ui;

namespace {

// Ensure QApplication exists for widget tests
class SPModeToggleTestFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test")};
            static QApplication app(argc, argv);
        }
    }
};

} // anonymous namespace

// =============================================================================
// SPModeToggle — Construction
// =============================================================================

TEST_F(SPModeToggleTestFixture, DefaultModeIsSlice) {
    SPModeToggle toggle;
    EXPECT_EQ(toggle.mode(), ScrollMode::Slice);
}

// =============================================================================
// SPModeToggle — Mode switching
// =============================================================================

TEST_F(SPModeToggleTestFixture, SetModeToPhase) {
    SPModeToggle toggle;
    toggle.setMode(ScrollMode::Phase);
    EXPECT_EQ(toggle.mode(), ScrollMode::Phase);
}

TEST_F(SPModeToggleTestFixture, SetModeToSlice) {
    SPModeToggle toggle;
    toggle.setMode(ScrollMode::Phase);
    toggle.setMode(ScrollMode::Slice);
    EXPECT_EQ(toggle.mode(), ScrollMode::Slice);
}

TEST_F(SPModeToggleTestFixture, SetSameModeNoOp) {
    SPModeToggle toggle;
    QSignalSpy spy(&toggle, &SPModeToggle::modeChanged);
    toggle.setMode(ScrollMode::Slice);  // Already in Slice mode
    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// SPModeToggle — Signal emission
// =============================================================================

TEST_F(SPModeToggleTestFixture, ModeChangedSignalNotEmittedOnExternalSet) {
    SPModeToggle toggle;
    QSignalSpy spy(&toggle, &SPModeToggle::modeChanged);

    // setMode is external programmatic change, should NOT emit signal
    toggle.setMode(ScrollMode::Phase);
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(toggle.mode(), ScrollMode::Phase);
}

// =============================================================================
// PhaseSliderWidget — S/P mode integration
// =============================================================================

TEST_F(SPModeToggleTestFixture, PhaseSliderDefaultScrollModeIsSlice) {
    PhaseSliderWidget slider;
    EXPECT_EQ(slider.scrollMode(), ScrollMode::Slice);
}

TEST_F(SPModeToggleTestFixture, PhaseSliderExposesScrollMode) {
    PhaseSliderWidget slider;
    // scrollMode should return the current S/P state
    EXPECT_EQ(slider.scrollMode(), ScrollMode::Slice);
}

// =============================================================================
// ScrollMode — Enum values
// =============================================================================

TEST(ScrollModeEnumTest, DistinctValues) {
    EXPECT_NE(static_cast<int>(ScrollMode::Slice),
              static_cast<int>(ScrollMode::Phase));
}

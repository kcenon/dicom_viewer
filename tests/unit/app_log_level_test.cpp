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

#include "core/app_log_level.hpp"

#include <gtest/gtest.h>

using namespace dicom_viewer;
using kcenon::common::interfaces::log_level;

TEST(AppLogLevelTest, ToEcosystemLevel) {
    EXPECT_EQ(to_ecosystem_level(AppLogLevel::Exception), log_level::critical);
    EXPECT_EQ(to_ecosystem_level(AppLogLevel::Error), log_level::error);
    EXPECT_EQ(to_ecosystem_level(AppLogLevel::Information), log_level::info);
    EXPECT_EQ(to_ecosystem_level(AppLogLevel::Debug), log_level::debug);
}

TEST(AppLogLevelTest, FromEcosystemLevel) {
    EXPECT_EQ(from_ecosystem_level(log_level::critical), AppLogLevel::Exception);
    EXPECT_EQ(from_ecosystem_level(log_level::error), AppLogLevel::Error);
    EXPECT_EQ(from_ecosystem_level(log_level::warning), AppLogLevel::Information);
    EXPECT_EQ(from_ecosystem_level(log_level::info), AppLogLevel::Information);
    EXPECT_EQ(from_ecosystem_level(log_level::debug), AppLogLevel::Debug);
    EXPECT_EQ(from_ecosystem_level(log_level::trace), AppLogLevel::Debug);
    EXPECT_EQ(from_ecosystem_level(log_level::off), AppLogLevel::Exception);
}

TEST(AppLogLevelTest, ToString) {
    EXPECT_EQ(to_string(AppLogLevel::Exception), "Exception");
    EXPECT_EQ(to_string(AppLogLevel::Error), "Error");
    EXPECT_EQ(to_string(AppLogLevel::Information), "Information");
    EXPECT_EQ(to_string(AppLogLevel::Debug), "Debug");
}

TEST(AppLogLevelTest, FromString) {
    EXPECT_EQ(app_log_level_from_string("Exception"), AppLogLevel::Exception);
    EXPECT_EQ(app_log_level_from_string("Error"), AppLogLevel::Error);
    EXPECT_EQ(app_log_level_from_string("Information"), AppLogLevel::Information);
    EXPECT_EQ(app_log_level_from_string("Debug"), AppLogLevel::Debug);
    EXPECT_EQ(app_log_level_from_string("unknown"), AppLogLevel::Information);
}

TEST(AppLogLevelTest, SettingsValueRoundTrip) {
    for (int i = 0; i <= 3; ++i) {
        auto level = from_settings_value(i);
        EXPECT_EQ(to_settings_value(level), i);
    }
}

TEST(AppLogLevelTest, InvalidSettingsValue) {
    EXPECT_EQ(from_settings_value(-1), AppLogLevel::Information);
    EXPECT_EQ(from_settings_value(4), AppLogLevel::Information);
    EXPECT_EQ(from_settings_value(100), AppLogLevel::Information);
}

TEST(AppLogLevelTest, EcosystemRoundTrip) {
    // Verify that AppLogLevel -> ecosystem -> AppLogLevel round-trips correctly
    for (auto level : {AppLogLevel::Exception, AppLogLevel::Error,
                       AppLogLevel::Information, AppLogLevel::Debug}) {
        auto eco = to_ecosystem_level(level);
        auto back = from_ecosystem_level(eco);
        EXPECT_EQ(back, level);
    }
}

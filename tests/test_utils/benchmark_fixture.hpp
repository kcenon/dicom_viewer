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

#pragma once

/// @file benchmark_fixture.hpp
/// @brief Reusable performance benchmark fixture for GoogleTest
///
/// Provides timing utilities and configurable thresholds for
/// performance regression testing across all service modules.

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <string>

namespace dicom_viewer::test_utils {

/// Multiplier for CI environments where hardware may be slower.
/// Set DICOM_VIEWER_BENCHMARK_MULTIPLIER environment variable to override.
inline double getBenchmarkMultiplier() {
    const char* env = std::getenv("DICOM_VIEWER_BENCHMARK_MULTIPLIER");
    if (env) {
        double val = std::atof(env);
        if (val > 0.0) {
            return val;
        }
    }
    return 1.0;
}

/// Base fixture for performance benchmarks.
/// Provides timing measurement and threshold assertions.
class PerformanceBenchmark : public ::testing::Test {
protected:
    /// Measure execution time of a callable
    /// @tparam Func Callable type
    /// @param func Function to measure
    /// @return Elapsed time in milliseconds
    template <typename Func>
    auto measureTime(Func&& func) -> std::chrono::milliseconds {
        auto start = std::chrono::steady_clock::now();
        std::forward<Func>(func)();
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    /// Measure execution time and return the result of the callable
    /// @tparam Func Callable type
    /// @param func Function to measure (must return a value)
    /// @param elapsed [out] Elapsed time in milliseconds
    /// @return Result of the callable
    template <typename Func>
    auto measureTimeWithResult(Func&& func,
                               std::chrono::milliseconds& elapsed)
        -> decltype(func()) {
        auto start = std::chrono::steady_clock::now();
        auto result = std::forward<Func>(func)();
        auto end = std::chrono::steady_clock::now();
        elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    /// Assert that elapsed time is within threshold (adjusted for CI)
    /// @param elapsed Measured time
    /// @param maxMs Maximum allowed time in milliseconds
    /// @param operationName Human-readable operation name for logging
    void assertWithinThreshold(std::chrono::milliseconds elapsed,
                               int64_t maxMs,
                               const std::string& operationName) {
        auto adjustedMax = static_cast<int64_t>(
            static_cast<double>(maxMs) * getBenchmarkMultiplier());

        std::cout << "[BENCHMARK] " << operationName << ": "
                  << elapsed.count() << "ms"
                  << " (threshold: " << adjustedMax << "ms)" << std::endl;

        EXPECT_LT(elapsed.count(), adjustedMax)
            << operationName << " took " << elapsed.count()
            << "ms, exceeding threshold of " << adjustedMax << "ms";
    }
};

}  // namespace dicom_viewer::test_utils

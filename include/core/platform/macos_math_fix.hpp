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

/**
 * @file macos_math_fix.hpp
 * @brief Fixes for macOS SDK math.h macro conflicts with C++ standard library
 *
 * macOS SDK versions (especially 15.4+, 26.x) have conflicts between:
 * - math.h C-style macros (isnan, signbit, etc.)
 * - C++ standard library's std::isnan, std::signbit, etc.
 *
 * Additionally, Qt's qmath.h uses std::hypot(x,y,z) which may not be available.
 *
 * This header provides workarounds for these issues.
 *
 * @note This file addresses issue #69
 */

#ifdef __APPLE__

// For newer macOS SDKs, we need to ensure math functions are available
// before any library tries to use them

// Include standard headers first
#include <cmath>
#include <type_traits>

// Provide fallback implementations for math functions if needed
namespace dicom_viewer::platform {

/**
 * @brief Portable isnan implementation
 */
template<typename T>
inline bool safe_isnan(T value) noexcept {
    return value != value;  // NaN is the only value not equal to itself
}

/**
 * @brief Portable isinf implementation
 */
template<typename T>
inline bool safe_isinf(T value) noexcept {
    return !safe_isnan(value) && safe_isnan(value - value);
}

/**
 * @brief Portable isfinite implementation
 */
template<typename T>
inline bool safe_isfinite(T value) noexcept {
    return !safe_isnan(value) && !safe_isinf(value);
}

/**
 * @brief Portable signbit implementation
 */
template<typename T>
inline bool safe_signbit(T value) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        return value < T(0) || (value == T(0) && T(1) / value < T(0));
    } else {
        return value < T(0);
    }
}

/**
 * @brief 3-argument hypot implementation (C++17)
 * Some SDK versions only provide 2-argument hypot
 */
template<typename T>
inline T safe_hypot(T x, T y, T z) noexcept {
    return std::sqrt(x*x + y*y + z*z);
}

} // namespace dicom_viewer::platform

#endif // __APPLE__


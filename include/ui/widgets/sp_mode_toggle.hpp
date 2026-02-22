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


/**
 * @file sp_mode_toggle.hpp
 * @brief Toggle widget for S/P (Slice/Phase) scroll mode
 * @details Provides two mutually exclusive buttons [S] [P] that switch
 *          scroll wheel behavior between slice navigation and
 *          phase navigation in 4D Flow MRI viewers.
 *
 * ## Thread Safety
 * - All methods must be called from the Qt UI thread (QWidget-derived)
 *
 * @author kcenon
 * @since 1.0.0
 */
#pragma once

#include <memory>
#include <QWidget>

namespace dicom_viewer::ui {

/**
 * @brief Scroll mode for viewer panels
 *
 * Controls whether mouse wheel scrolls through slices (S) or phases (P).
 */
enum class ScrollMode {
    Slice,   ///< Scroll wheel navigates slices (default)
    Phase    ///< Scroll wheel navigates cardiac phases
};

/**
 * @brief Toggle widget for S/P (Slice/Phase) scroll mode
 *
 * Provides two mutually exclusive buttons [S] [P] that switch
 * the scroll wheel behavior between slice navigation and phase
 * navigation in 4D Flow MRI viewers.
 *
 * @trace SRS-FR-048
 */
class SPModeToggle : public QWidget {
    Q_OBJECT

public:
    explicit SPModeToggle(QWidget* parent = nullptr);
    ~SPModeToggle() override;

    // Non-copyable
    SPModeToggle(const SPModeToggle&) = delete;
    SPModeToggle& operator=(const SPModeToggle&) = delete;

    /**
     * @brief Get current scroll mode
     */
    [[nodiscard]] ScrollMode mode() const;

public slots:
    /**
     * @brief Set the scroll mode
     * @param mode Slice or Phase mode
     */
    void setMode(ScrollMode mode);

signals:
    /**
     * @brief Emitted when the user changes the scroll mode
     * @param mode New scroll mode
     */
    void modeChanged(ScrollMode mode);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom_viewer::ui

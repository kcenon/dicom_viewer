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

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace dicom_viewer::services {

/**
 * @brief Abstract interface for undoable segmentation operations
 *
 * All segmentation operations that support undo/redo must implement
 * this interface. Commands are stored in a SegmentationCommandStack.
 *
 * @trace SRS-FR-023
 */
class ISegmentationCommand {
public:
    virtual ~ISegmentationCommand() = default;

    /**
     * @brief Execute or re-execute the command
     */
    virtual void execute() = 0;

    /**
     * @brief Reverse the effect of execute()
     */
    virtual void undo() = 0;

    /**
     * @brief Human-readable description of the operation
     */
    [[nodiscard]] virtual std::string description() const = 0;

    /**
     * @brief Estimated memory usage of stored undo data in bytes
     */
    [[nodiscard]] virtual size_t memoryUsage() const = 0;
};

/**
 * @brief Manages undo/redo history for segmentation operations
 *
 * Implements a command stack with configurable history depth (default ≥20).
 * When a new command is executed after an undo, the redo stack is cleared.
 * When the undo stack exceeds the maximum size, the oldest command is discarded.
 *
 * @trace SRS-FR-023
 */
class SegmentationCommandStack {
public:
    /// Callback when undo/redo availability changes
    using AvailabilityCallback = std::function<void(bool canUndo, bool canRedo)>;

    /**
     * @brief Construct with default max history of 20
     */
    SegmentationCommandStack();

    /**
     * @brief Construct with specified max history size
     * @param maxHistory Maximum number of undo steps (minimum 1)
     */
    explicit SegmentationCommandStack(size_t maxHistory);

    ~SegmentationCommandStack();

    // Non-copyable, movable
    SegmentationCommandStack(const SegmentationCommandStack&) = delete;
    SegmentationCommandStack& operator=(const SegmentationCommandStack&) = delete;
    SegmentationCommandStack(SegmentationCommandStack&&) noexcept;
    SegmentationCommandStack& operator=(SegmentationCommandStack&&) noexcept;

    /**
     * @brief Execute a command and push it onto the undo stack
     *
     * Clears the redo stack. If the undo stack exceeds maxHistorySize,
     * the oldest command is discarded.
     *
     * @param command Command to execute
     */
    void execute(std::unique_ptr<ISegmentationCommand> command);

    /**
     * @brief Undo the most recent command
     * @return true if an undo was performed
     */
    bool undo();

    /**
     * @brief Redo the most recently undone command
     * @return true if a redo was performed
     */
    bool redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool canUndo() const noexcept;

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool canRedo() const noexcept;

    /**
     * @brief Get the number of undoable commands
     */
    [[nodiscard]] size_t undoCount() const noexcept;

    /**
     * @brief Get the number of redoable commands
     */
    [[nodiscard]] size_t redoCount() const noexcept;

    /**
     * @brief Clear all history (both undo and redo stacks)
     */
    void clear();

    /**
     * @brief Get the maximum history size
     */
    [[nodiscard]] size_t maxHistorySize() const noexcept;

    /**
     * @brief Set the maximum history size
     * @param maxHistory New maximum (minimum 1)
     */
    void setMaxHistorySize(size_t maxHistory);

    /**
     * @brief Get the description of the next undo command
     * @return Description string, or empty if no undo available
     */
    [[nodiscard]] std::string undoDescription() const;

    /**
     * @brief Get the description of the next redo command
     * @return Description string, or empty if no redo available
     */
    [[nodiscard]] std::string redoDescription() const;

    /**
     * @brief Set callback for undo/redo availability changes
     */
    void setAvailabilityCallback(AvailabilityCallback callback);

private:
    void notifyAvailability();
    void trimUndoStack();

    std::deque<std::unique_ptr<ISegmentationCommand>> undoStack_;
    std::deque<std::unique_ptr<ISegmentationCommand>> redoStack_;
    size_t maxHistory_;
    AvailabilityCallback availabilityCallback_;
};

} // namespace dicom_viewer::services

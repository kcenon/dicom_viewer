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

#include "services/segmentation/segmentation_command.hpp"

#include <algorithm>

namespace dicom_viewer::services {

SegmentationCommandStack::SegmentationCommandStack()
    : maxHistory_(20)
{
}

SegmentationCommandStack::SegmentationCommandStack(size_t maxHistory)
    : maxHistory_(std::max(size_t{1}, maxHistory))
{
}

SegmentationCommandStack::~SegmentationCommandStack() = default;

SegmentationCommandStack::SegmentationCommandStack(
    SegmentationCommandStack&&) noexcept = default;
SegmentationCommandStack& SegmentationCommandStack::operator=(
    SegmentationCommandStack&&) noexcept = default;

void SegmentationCommandStack::execute(
    std::unique_ptr<ISegmentationCommand> command)
{
    if (!command) return;

    command->execute();
    undoStack_.push_back(std::move(command));

    // New command invalidates redo history
    redoStack_.clear();

    trimUndoStack();
    notifyAvailability();
}

bool SegmentationCommandStack::undo()
{
    if (undoStack_.empty()) return false;

    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();

    command->undo();
    redoStack_.push_back(std::move(command));

    notifyAvailability();
    return true;
}

bool SegmentationCommandStack::redo()
{
    if (redoStack_.empty()) return false;

    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();

    command->execute();
    undoStack_.push_back(std::move(command));

    notifyAvailability();
    return true;
}

bool SegmentationCommandStack::canUndo() const noexcept
{
    return !undoStack_.empty();
}

bool SegmentationCommandStack::canRedo() const noexcept
{
    return !redoStack_.empty();
}

size_t SegmentationCommandStack::undoCount() const noexcept
{
    return undoStack_.size();
}

size_t SegmentationCommandStack::redoCount() const noexcept
{
    return redoStack_.size();
}

void SegmentationCommandStack::clear()
{
    undoStack_.clear();
    redoStack_.clear();
    notifyAvailability();
}

size_t SegmentationCommandStack::maxHistorySize() const noexcept
{
    return maxHistory_;
}

void SegmentationCommandStack::setMaxHistorySize(size_t maxHistory)
{
    maxHistory_ = std::max(size_t{1}, maxHistory);
    trimUndoStack();
    notifyAvailability();
}

std::string SegmentationCommandStack::undoDescription() const
{
    if (undoStack_.empty()) return {};
    return undoStack_.back()->description();
}

std::string SegmentationCommandStack::redoDescription() const
{
    if (redoStack_.empty()) return {};
    return redoStack_.back()->description();
}

void SegmentationCommandStack::setAvailabilityCallback(
    AvailabilityCallback callback)
{
    availabilityCallback_ = std::move(callback);
}

void SegmentationCommandStack::notifyAvailability()
{
    if (availabilityCallback_) {
        availabilityCallback_(canUndo(), canRedo());
    }
}

void SegmentationCommandStack::trimUndoStack()
{
    while (undoStack_.size() > maxHistory_) {
        undoStack_.pop_front();
    }
}

} // namespace dicom_viewer::services

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

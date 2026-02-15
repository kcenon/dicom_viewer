#include "services/segmentation/brush_stroke_command.hpp"

#include <utility>

namespace dicom_viewer::services {

BrushStrokeCommand::BrushStrokeCommand(
    LabelMapType::Pointer labelMap,
    std::string operationDescription)
    : labelMap_(std::move(labelMap))
    , description_(std::move(operationDescription))
{
}

void BrushStrokeCommand::recordChange(
    size_t linearIndex, uint8_t oldLabel, uint8_t newLabel)
{
    if (oldLabel != newLabel) {
        changes_.push_back({linearIndex, oldLabel, newLabel});
    }
}

size_t BrushStrokeCommand::changeCount() const noexcept
{
    return changes_.size();
}

bool BrushStrokeCommand::hasChanges() const noexcept
{
    return !changes_.empty();
}

void BrushStrokeCommand::execute()
{
    if (!labelMap_) return;

    auto* buffer = labelMap_->GetBufferPointer();
    for (const auto& change : changes_) {
        buffer[change.linearIndex] = change.newLabel;
    }
}

void BrushStrokeCommand::undo()
{
    if (!labelMap_) return;

    auto* buffer = labelMap_->GetBufferPointer();
    for (const auto& change : changes_) {
        buffer[change.linearIndex] = change.oldLabel;
    }
}

std::string BrushStrokeCommand::description() const
{
    return description_;
}

size_t BrushStrokeCommand::memoryUsage() const
{
    return changes_.size() * sizeof(VoxelChange) + description_.size();
}

} // namespace dicom_viewer::services

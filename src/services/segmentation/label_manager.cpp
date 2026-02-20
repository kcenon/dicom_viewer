#include "services/segmentation/label_manager.hpp"
#include "services/segmentation/threshold_segmenter.hpp"
#include <kcenon/common/logging/log_macros.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkLabelStatisticsImageFilter.h>
#include <itkNiftiImageIO.h>
#include <itkNrrdImageIO.h>

namespace dicom_viewer::services {

// ============================================================================
// Implementation Class
// ============================================================================

class LabelManager::Impl {
public:
    // Label storage
    std::map<uint8_t, SegmentationLabel> labels_;

    // Label map
    LabelMapType::Pointer labelMap_;

    // Voxel spacing for volume calculation
    std::array<double, 3> spacing_ = {1.0, 1.0, 1.0};

    // Active label ID
    uint8_t activeLabel_ = 0;

    // Callbacks
    LabelChangeCallback labelChangeCallback_;
    LabelMapChangeCallback labelMapChangeCallback_;

    // Thread safety
    mutable std::mutex mutex_;

    // Get next available label ID
    [[nodiscard]] std::optional<uint8_t> getNextLabelId() const {
        for (uint8_t id = 1; id > 0; ++id) {  // Wraps at 255
            if (labels_.find(id) == labels_.end()) {
                return id;
            }
        }
        return std::nullopt;
    }

    // Clear pixels with specific label in label map
    void clearLabelPixels(uint8_t labelId) {
        if (!labelMap_) {
            return;
        }

        using IteratorType = itk::ImageRegionIterator<LabelMapType>;
        IteratorType it(labelMap_, labelMap_->GetLargestPossibleRegion());

        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            if (it.Get() == labelId) {
                it.Set(0);
            }
        }
    }

    // Notify label change
    void notifyLabelChange() {
        if (labelChangeCallback_) {
            labelChangeCallback_();
        }
    }

    // Notify label map change
    void notifyLabelMapChange() {
        if (labelMapChangeCallback_) {
            labelMapChangeCallback_();
        }
    }
};

// ============================================================================
// LabelManager Implementation
// ============================================================================

LabelManager::LabelManager() : pImpl_(std::make_unique<Impl>()) {}

LabelManager::~LabelManager() = default;

LabelManager::LabelManager(LabelManager&&) noexcept = default;
LabelManager& LabelManager::operator=(LabelManager&&) noexcept = default;

// ----------------------------------------------------------------------------
// Label Map Management
// ----------------------------------------------------------------------------

std::expected<void, SegmentationError> LabelManager::initializeLabelMap(
    int width,
    int height,
    int depth,
    std::array<double, 3> spacing
) {
    if (width <= 0 || height <= 0 || depth <= 0) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Dimensions must be positive"
        });
    }

    std::lock_guard lock(pImpl_->mutex_);

    // Create label map
    pImpl_->labelMap_ = LabelMapType::New();

    LabelMapType::IndexType start;
    start.Fill(0);

    LabelMapType::SizeType size;
    size[0] = static_cast<LabelMapType::SizeValueType>(width);
    size[1] = static_cast<LabelMapType::SizeValueType>(height);
    size[2] = static_cast<LabelMapType::SizeValueType>(depth);

    LabelMapType::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    pImpl_->labelMap_->SetRegions(region);

    LabelMapType::SpacingType itkSpacing;
    itkSpacing[0] = spacing[0];
    itkSpacing[1] = spacing[1];
    itkSpacing[2] = spacing[2];
    pImpl_->labelMap_->SetSpacing(itkSpacing);

    pImpl_->labelMap_->Allocate();
    pImpl_->labelMap_->FillBuffer(0);

    pImpl_->spacing_ = spacing;

    return {};
}

std::expected<void, SegmentationError>
LabelManager::setLabelMap(LabelMapType::Pointer labelMap) {
    if (!labelMap) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Label map is null"
        });
    }

    std::lock_guard lock(pImpl_->mutex_);
    pImpl_->labelMap_ = labelMap;

    auto spacing = labelMap->GetSpacing();
    pImpl_->spacing_ = {spacing[0], spacing[1], spacing[2]};

    return {};
}

LabelManager::LabelMapType::Pointer LabelManager::getLabelMap() const {
    std::lock_guard lock(pImpl_->mutex_);
    return pImpl_->labelMap_;
}

bool LabelManager::hasLabelMap() const noexcept {
    std::lock_guard lock(pImpl_->mutex_);
    return pImpl_->labelMap_ != nullptr;
}

// ----------------------------------------------------------------------------
// Label Management
// ----------------------------------------------------------------------------

std::expected<std::reference_wrapper<SegmentationLabel>, SegmentationError>
LabelManager::addLabel(const std::string& name, std::optional<LabelColor> color) {
    std::lock_guard lock(pImpl_->mutex_);

    auto nextId = pImpl_->getNextLabelId();
    if (!nextId) {
        LOG_ERROR("Maximum label count reached");
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Maximum number of labels (255) reached"
        });
    }

    uint8_t id = *nextId;
    LabelColor labelColor = color.value_or(LabelColorPalette::getColor(id));

    auto [it, inserted] = pImpl_->labels_.emplace(
        id,
        SegmentationLabel{id, name, labelColor}
    );

    LOG_INFO(std::format("Added label: id={}, name='{}'", id, name));
    pImpl_->notifyLabelChange();

    return std::ref(it->second);
}

std::expected<std::reference_wrapper<SegmentationLabel>, SegmentationError>
LabelManager::addLabel(uint8_t id, const std::string& name, const LabelColor& color) {
    if (id == 0) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID 0 is reserved for background"
        });
    }

    std::lock_guard lock(pImpl_->mutex_);

    if (pImpl_->labels_.find(id) != pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " already exists"
        });
    }

    auto [it, inserted] = pImpl_->labels_.emplace(
        id,
        SegmentationLabel{id, name, color}
    );

    pImpl_->notifyLabelChange();

    return std::ref(it->second);
}

std::expected<void, SegmentationError>
LabelManager::removeLabel(uint8_t id, bool clearPixels) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    if (clearPixels) {
        pImpl_->clearLabelPixels(id);
        pImpl_->notifyLabelMapChange();
    }

    pImpl_->labels_.erase(it);

    // Update active label if necessary
    if (pImpl_->activeLabel_ == id) {
        pImpl_->activeLabel_ = 0;
    }

    pImpl_->notifyLabelChange();

    return {};
}

SegmentationLabel* LabelManager::getLabel(uint8_t id) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it != pImpl_->labels_.end()) {
        return &it->second;
    }
    return nullptr;
}

const SegmentationLabel* LabelManager::getLabel(uint8_t id) const {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it != pImpl_->labels_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<SegmentationLabel> LabelManager::getAllLabels() const {
    std::lock_guard lock(pImpl_->mutex_);

    std::vector<SegmentationLabel> result;
    result.reserve(pImpl_->labels_.size());

    for (const auto& [id, label] : pImpl_->labels_) {
        result.push_back(label);
    }

    return result;
}

size_t LabelManager::getLabelCount() const noexcept {
    std::lock_guard lock(pImpl_->mutex_);
    return pImpl_->labels_.size();
}

bool LabelManager::hasLabel(uint8_t id) const noexcept {
    std::lock_guard lock(pImpl_->mutex_);
    return pImpl_->labels_.find(id) != pImpl_->labels_.end();
}

void LabelManager::clearAllLabels(bool clearLabelMap) {
    std::lock_guard lock(pImpl_->mutex_);

    pImpl_->labels_.clear();
    pImpl_->activeLabel_ = 0;

    if (clearLabelMap && pImpl_->labelMap_) {
        pImpl_->labelMap_->FillBuffer(0);
        pImpl_->notifyLabelMapChange();
    }

    pImpl_->notifyLabelChange();
}

// ----------------------------------------------------------------------------
// Active Label
// ----------------------------------------------------------------------------

std::expected<void, SegmentationError> LabelManager::setActiveLabel(uint8_t id) {
    std::lock_guard lock(pImpl_->mutex_);

    if (id != 0 && pImpl_->labels_.find(id) == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    pImpl_->activeLabel_ = id;
    pImpl_->notifyLabelChange();

    return {};
}

uint8_t LabelManager::getActiveLabel() const noexcept {
    std::lock_guard lock(pImpl_->mutex_);
    return pImpl_->activeLabel_;
}

SegmentationLabel* LabelManager::getActiveLabelObject() {
    std::lock_guard lock(pImpl_->mutex_);

    if (pImpl_->activeLabel_ == 0) {
        return nullptr;
    }

    auto it = pImpl_->labels_.find(pImpl_->activeLabel_);
    if (it != pImpl_->labels_.end()) {
        return &it->second;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Label Properties
// ----------------------------------------------------------------------------

std::expected<void, SegmentationError>
LabelManager::setLabelName(uint8_t id, const std::string& name) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    it->second.name = name;
    pImpl_->notifyLabelChange();

    return {};
}

std::expected<void, SegmentationError>
LabelManager::setLabelColor(uint8_t id, const LabelColor& color) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    it->second.color = color;
    pImpl_->notifyLabelChange();

    return {};
}

std::expected<void, SegmentationError>
LabelManager::setLabelOpacity(uint8_t id, double opacity) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    it->second.opacity = std::clamp(opacity, 0.0, 1.0);
    pImpl_->notifyLabelChange();

    return {};
}

std::expected<void, SegmentationError>
LabelManager::setLabelVisibility(uint8_t id, bool visible) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    it->second.visible = visible;
    pImpl_->notifyLabelChange();

    return {};
}

std::expected<bool, SegmentationError> LabelManager::toggleLabelVisibility(uint8_t id) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    it->second.visible = !it->second.visible;
    pImpl_->notifyLabelChange();

    return it->second.visible;
}

// ----------------------------------------------------------------------------
// Statistics
// ----------------------------------------------------------------------------

std::expected<void, SegmentationError>
LabelManager::computeLabelStatistics(uint8_t id, SourceImageType::Pointer sourceImage) {
    std::lock_guard lock(pImpl_->mutex_);

    auto it = pImpl_->labels_.find(id);
    if (it == pImpl_->labels_.end()) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidParameters,
            "Label ID " + std::to_string(id) + " not found"
        });
    }

    if (!pImpl_->labelMap_) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Label map not initialized"
        });
    }

    if (!sourceImage) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Source image is null"
        });
    }

    try {
        using StatsFilterType = itk::LabelStatisticsImageFilter<SourceImageType, LabelMapType>;
        auto statsFilter = StatsFilterType::New();
        statsFilter->SetInput(sourceImage);
        statsFilter->SetLabelInput(pImpl_->labelMap_);
        statsFilter->Update();

        if (statsFilter->HasLabel(id)) {
            auto& label = it->second;

            label.voxelCount = statsFilter->GetCount(id);
            label.meanHU = statsFilter->GetMean(id);
            label.stdHU = statsFilter->GetSigma(id);
            label.minHU = statsFilter->GetMinimum(id);
            label.maxHU = statsFilter->GetMaximum(id);

            // Calculate volume in mL
            double voxelVolume = pImpl_->spacing_[0] * pImpl_->spacing_[1] * pImpl_->spacing_[2];
            label.volumeML = static_cast<double>(*label.voxelCount) * voxelVolume / 1000.0;
        } else {
            it->second.clearStatistics();
        }

        return {};
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("ITK error: ") + e.what()
        });
    }
}

std::expected<void, SegmentationError>
LabelManager::computeAllStatistics(SourceImageType::Pointer sourceImage) {
    // We need to release the lock before calling computeLabelStatistics
    std::vector<uint8_t> labelIds;
    {
        std::lock_guard lock(pImpl_->mutex_);
        for (const auto& [id, label] : pImpl_->labels_) {
            labelIds.push_back(id);
        }
    }

    for (uint8_t id : labelIds) {
        auto result = computeLabelStatistics(id, sourceImage);
        if (!result) {
            return result;
        }
    }

    return {};
}

// ----------------------------------------------------------------------------
// Import/Export
// ----------------------------------------------------------------------------

std::expected<void, SegmentationError>
LabelManager::exportSegmentation(
    const std::filesystem::path& path,
    SegmentationFormat format
) const {
    std::lock_guard lock(pImpl_->mutex_);

    if (!pImpl_->labelMap_) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "Label map not initialized"
        });
    }

    try {
        using WriterType = itk::ImageFileWriter<LabelMapType>;
        auto writer = WriterType::New();
        writer->SetInput(pImpl_->labelMap_);
        writer->SetFileName(path.string());

        if (format == SegmentationFormat::NIfTI) {
            writer->SetImageIO(itk::NiftiImageIO::New());
        } else if (format == SegmentationFormat::NRRD) {
            writer->SetImageIO(itk::NrrdImageIO::New());
        }

        writer->Update();

        return {};
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("Failed to export: ") + e.what()
        });
    }
}

std::expected<void, SegmentationError>
LabelManager::importSegmentation(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "File not found: " + path.string()
        });
    }

    try {
        using ReaderType = itk::ImageFileReader<LabelMapType>;
        auto reader = ReaderType::New();
        reader->SetFileName(path.string());

        // Explicitly set ImageIO to avoid IO factory registration issues
        auto ext = path.extension().string();
        auto stem = path.stem().extension().string();
        if (ext == ".nrrd" || ext == ".nhdr") {
            reader->SetImageIO(itk::NrrdImageIO::New());
        } else if (ext == ".nii" || (ext == ".gz" && stem == ".nii")) {
            reader->SetImageIO(itk::NiftiImageIO::New());
        }

        reader->Update();

        std::lock_guard lock(pImpl_->mutex_);
        pImpl_->labelMap_ = reader->GetOutput();

        auto spacing = pImpl_->labelMap_->GetSpacing();
        pImpl_->spacing_ = {spacing[0], spacing[1], spacing[2]};

        // Detect labels in the image
        using IteratorType = itk::ImageRegionConstIterator<LabelMapType>;
        IteratorType it(pImpl_->labelMap_, pImpl_->labelMap_->GetLargestPossibleRegion());

        std::set<uint8_t> foundLabels;
        for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
            uint8_t value = it.Get();
            if (value > 0) {
                foundLabels.insert(value);
            }
        }

        // Add labels that don't exist yet
        for (uint8_t labelId : foundLabels) {
            if (pImpl_->labels_.find(labelId) == pImpl_->labels_.end()) {
                pImpl_->labels_.emplace(
                    labelId,
                    SegmentationLabel{
                        labelId,
                        "Label " + std::to_string(labelId),
                        LabelColorPalette::getColor(labelId)
                    }
                );
            }
        }

        pImpl_->notifyLabelMapChange();
        pImpl_->notifyLabelChange();

        return {};
    } catch (const itk::ExceptionObject& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("Failed to import: ") + e.what()
        });
    }
}

std::expected<void, SegmentationError>
LabelManager::exportLabelMetadata(const std::filesystem::path& path) const {
    std::lock_guard lock(pImpl_->mutex_);

    try {
        nlohmann::json j;
        j["version"] = "1.0";
        j["labels"] = nlohmann::json::array();

        for (const auto& [id, label] : pImpl_->labels_) {
            nlohmann::json labelJson;
            labelJson["id"] = label.id;
            labelJson["name"] = label.name;
            labelJson["color"] = {
                {"r", label.color.r},
                {"g", label.color.g},
                {"b", label.color.b},
                {"a", label.color.a}
            };
            labelJson["opacity"] = label.opacity;
            labelJson["visible"] = label.visible;

            if (label.volumeML) {
                labelJson["volumeML"] = *label.volumeML;
            }
            if (label.meanHU) {
                labelJson["meanHU"] = *label.meanHU;
            }
            if (label.stdHU) {
                labelJson["stdHU"] = *label.stdHU;
            }
            if (label.voxelCount) {
                labelJson["voxelCount"] = *label.voxelCount;
            }

            j["labels"].push_back(labelJson);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            return std::unexpected(SegmentationError{
                SegmentationError::Code::ProcessingFailed,
                "Failed to open file for writing: " + path.string()
            });
        }

        file << j.dump(2);

        return {};
    } catch (const std::exception& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("Failed to export metadata: ") + e.what()
        });
    }
}

std::expected<void, SegmentationError>
LabelManager::importLabelMetadata(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::InvalidInput,
            "File not found: " + path.string()
        });
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return std::unexpected(SegmentationError{
                SegmentationError::Code::ProcessingFailed,
                "Failed to open file: " + path.string()
            });
        }

        nlohmann::json j;
        file >> j;

        std::lock_guard lock(pImpl_->mutex_);

        for (const auto& labelJson : j["labels"]) {
            uint8_t id = labelJson["id"];

            SegmentationLabel label;
            label.id = id;
            label.name = labelJson["name"];
            label.color = LabelColor(
                labelJson["color"]["r"],
                labelJson["color"]["g"],
                labelJson["color"]["b"],
                labelJson["color"]["a"]
            );
            label.opacity = labelJson["opacity"];
            label.visible = labelJson["visible"];

            if (labelJson.contains("volumeML")) {
                label.volumeML = labelJson["volumeML"];
            }
            if (labelJson.contains("meanHU")) {
                label.meanHU = labelJson["meanHU"];
            }
            if (labelJson.contains("stdHU")) {
                label.stdHU = labelJson["stdHU"];
            }
            if (labelJson.contains("voxelCount")) {
                label.voxelCount = labelJson["voxelCount"];
            }

            pImpl_->labels_[id] = label;
        }

        pImpl_->notifyLabelChange();

        return {};
    } catch (const std::exception& e) {
        return std::unexpected(SegmentationError{
            SegmentationError::Code::ProcessingFailed,
            std::string("Failed to import metadata: ") + e.what()
        });
    }
}

// ----------------------------------------------------------------------------
// Callbacks
// ----------------------------------------------------------------------------

void LabelManager::setLabelChangeCallback(LabelChangeCallback callback) {
    std::lock_guard lock(pImpl_->mutex_);
    pImpl_->labelChangeCallback_ = std::move(callback);
}

void LabelManager::setLabelMapChangeCallback(LabelMapChangeCallback callback) {
    std::lock_guard lock(pImpl_->mutex_);
    pImpl_->labelMapChangeCallback_ = std::move(callback);
}

}  // namespace dicom_viewer::services

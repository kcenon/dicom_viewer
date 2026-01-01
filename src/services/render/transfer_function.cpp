#include "services/transfer_function_manager.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace dicom_viewer::services {

namespace {

// JSON serialization helpers (minimal implementation without external deps)
std::string escapeJson(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

std::string presetToJson(const TransferFunctionPreset& preset) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"name\": \"" << escapeJson(preset.name) << "\",\n";
    oss << "  \"windowWidth\": " << preset.windowWidth << ",\n";
    oss << "  \"windowCenter\": " << preset.windowCenter << ",\n";

    // Color points
    oss << "  \"colorPoints\": [\n";
    for (size_t i = 0; i < preset.colorPoints.size(); ++i) {
        const auto& [value, r, g, b] = preset.colorPoints[i];
        oss << "    [" << value << ", " << r << ", " << g << ", " << b << "]";
        if (i < preset.colorPoints.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";

    // Opacity points
    oss << "  \"opacityPoints\": [\n";
    for (size_t i = 0; i < preset.opacityPoints.size(); ++i) {
        const auto& [value, opacity] = preset.opacityPoints[i];
        oss << "    [" << value << ", " << opacity << "]";
        if (i < preset.opacityPoints.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";

    // Gradient opacity points
    oss << "  \"gradientOpacityPoints\": [\n";
    for (size_t i = 0; i < preset.gradientOpacityPoints.size(); ++i) {
        const auto& [value, opacity] = preset.gradientOpacityPoints[i];
        oss << "    [" << value << ", " << opacity << "]";
        if (i < preset.gradientOpacityPoints.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";

    oss << "}";
    return oss.str();
}

std::string presetsToJson(const std::vector<TransferFunctionPreset>& presets) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"version\": \"1.0\",\n";
    oss << "  \"presets\": [\n";
    for (size_t i = 0; i < presets.size(); ++i) {
        std::string presetJson = presetToJson(presets[i]);
        // Indent preset JSON
        std::istringstream iss(presetJson);
        std::string line;
        while (std::getline(iss, line)) {
            oss << "    " << line;
            if (!iss.eof()) oss << "\n";
        }
        if (i < presets.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

// Simple JSON parsing helpers
void skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(json[pos])) ++pos;
}

bool parseString(const std::string& json, size_t& pos, std::string& result) {
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '"') return false;
    ++pos;
    result.clear();
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += json[pos];
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }
    if (pos >= json.size()) return false;
    ++pos; // Skip closing quote
    return true;
}

bool parseNumber(const std::string& json, size_t& pos, double& result) {
    skipWhitespace(json, pos);
    size_t start = pos;
    if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) ++pos;
    while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '-' || json[pos] == '+')) ++pos;
    if (pos == start) return false;
    result = std::stod(json.substr(start, pos - start));
    return true;
}

bool expectChar(const std::string& json, size_t& pos, char c) {
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != c) return false;
    ++pos;
    return true;
}

bool parsePreset(const std::string& json, size_t& pos, TransferFunctionPreset& preset) {
    if (!expectChar(json, pos, '{')) return false;

    preset = TransferFunctionPreset{};

    while (true) {
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') { ++pos; break; }

        std::string key;
        if (!parseString(json, pos, key)) return false;
        if (!expectChar(json, pos, ':')) return false;

        if (key == "name") {
            if (!parseString(json, pos, preset.name)) return false;
        } else if (key == "windowWidth") {
            if (!parseNumber(json, pos, preset.windowWidth)) return false;
        } else if (key == "windowCenter") {
            if (!parseNumber(json, pos, preset.windowCenter)) return false;
        } else if (key == "colorPoints") {
            if (!expectChar(json, pos, '[')) return false;
            while (true) {
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ']') { ++pos; break; }
                if (!expectChar(json, pos, '[')) return false;
                double v, r, g, b;
                if (!parseNumber(json, pos, v)) return false;
                if (!expectChar(json, pos, ',')) return false;
                if (!parseNumber(json, pos, r)) return false;
                if (!expectChar(json, pos, ',')) return false;
                if (!parseNumber(json, pos, g)) return false;
                if (!expectChar(json, pos, ',')) return false;
                if (!parseNumber(json, pos, b)) return false;
                if (!expectChar(json, pos, ']')) return false;
                preset.colorPoints.emplace_back(v, r, g, b);
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
        } else if (key == "opacityPoints") {
            if (!expectChar(json, pos, '[')) return false;
            while (true) {
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ']') { ++pos; break; }
                if (!expectChar(json, pos, '[')) return false;
                double v, o;
                if (!parseNumber(json, pos, v)) return false;
                if (!expectChar(json, pos, ',')) return false;
                if (!parseNumber(json, pos, o)) return false;
                if (!expectChar(json, pos, ']')) return false;
                preset.opacityPoints.emplace_back(v, o);
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
        } else if (key == "gradientOpacityPoints") {
            if (!expectChar(json, pos, '[')) return false;
            while (true) {
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ']') { ++pos; break; }
                if (!expectChar(json, pos, '[')) return false;
                double v, o;
                if (!parseNumber(json, pos, v)) return false;
                if (!expectChar(json, pos, ',')) return false;
                if (!parseNumber(json, pos, o)) return false;
                if (!expectChar(json, pos, ']')) return false;
                preset.gradientOpacityPoints.emplace_back(v, o);
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
        } else {
            // Skip unknown value
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == '"') {
                std::string dummy;
                parseString(json, pos, dummy);
            } else if (pos < json.size() && json[pos] == '[') {
                int depth = 1;
                ++pos;
                while (pos < json.size() && depth > 0) {
                    if (json[pos] == '[') ++depth;
                    else if (json[pos] == ']') --depth;
                    ++pos;
                }
            } else if (pos < json.size() && json[pos] == '{') {
                int depth = 1;
                ++pos;
                while (pos < json.size() && depth > 0) {
                    if (json[pos] == '{') ++depth;
                    else if (json[pos] == '}') --depth;
                    ++pos;
                }
            } else {
                double dummy;
                parseNumber(json, pos, dummy);
            }
        }

        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') ++pos;
    }

    return true;
}

std::expected<std::vector<TransferFunctionPreset>, TransferFunctionErrorInfo>
parsePresetsFromJson(const std::string& json) {
    std::vector<TransferFunctionPreset> presets;
    size_t pos = 0;

    if (!expectChar(json, pos, '{')) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::ParseError, "Expected '{' at start"});
    }

    while (true) {
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') break;

        std::string key;
        if (!parseString(json, pos, key)) {
            return std::unexpected(TransferFunctionErrorInfo{
                TransferFunctionError::ParseError, "Failed to parse key"});
        }
        if (!expectChar(json, pos, ':')) {
            return std::unexpected(TransferFunctionErrorInfo{
                TransferFunctionError::ParseError, "Expected ':' after key"});
        }

        if (key == "presets") {
            if (!expectChar(json, pos, '[')) {
                return std::unexpected(TransferFunctionErrorInfo{
                    TransferFunctionError::ParseError, "Expected '[' for presets array"});
            }

            while (true) {
                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ']') { ++pos; break; }

                TransferFunctionPreset preset;
                if (!parsePreset(json, pos, preset)) {
                    return std::unexpected(TransferFunctionErrorInfo{
                        TransferFunctionError::ParseError, "Failed to parse preset"});
                }
                presets.push_back(std::move(preset));

                skipWhitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
        } else {
            // Skip unknown values
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == '"') {
                std::string dummy;
                parseString(json, pos, dummy);
            } else if (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '-')) {
                double dummy;
                parseNumber(json, pos, dummy);
            }
        }

        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') ++pos;
    }

    return presets;
}

} // anonymous namespace

class TransferFunctionManager::Impl {
public:
    std::unordered_map<std::string, TransferFunctionPreset> builtInPresets;
    std::unordered_map<std::string, TransferFunctionPreset> customPresets;

    Impl() {
        // Initialize built-in presets
        auto addBuiltIn = [this](TransferFunctionPreset preset) {
            builtInPresets[preset.name] = std::move(preset);
        };

        addBuiltIn(VolumeRenderer::getPresetCTBone());
        addBuiltIn(VolumeRenderer::getPresetCTSoftTissue());
        addBuiltIn(VolumeRenderer::getPresetCTLung());
        addBuiltIn(VolumeRenderer::getPresetCTAngio());
        addBuiltIn(VolumeRenderer::getPresetCTAbdomen());
        addBuiltIn(VolumeRenderer::getPresetMRIDefault());
    }
};

TransferFunctionManager::TransferFunctionManager()
    : impl_(std::make_unique<Impl>()) {}

TransferFunctionManager::~TransferFunctionManager() = default;

TransferFunctionManager::TransferFunctionManager(TransferFunctionManager&&) noexcept = default;
TransferFunctionManager& TransferFunctionManager::operator=(TransferFunctionManager&&) noexcept = default;

std::vector<std::string> TransferFunctionManager::getPresetNames() const {
    std::vector<std::string> names;
    names.reserve(impl_->builtInPresets.size() + impl_->customPresets.size());

    for (const auto& [name, _] : impl_->builtInPresets) {
        names.push_back(name);
    }
    for (const auto& [name, _] : impl_->customPresets) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> TransferFunctionManager::getBuiltInPresetNames() const {
    std::vector<std::string> names;
    names.reserve(impl_->builtInPresets.size());

    for (const auto& [name, _] : impl_->builtInPresets) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> TransferFunctionManager::getCustomPresetNames() const {
    std::vector<std::string> names;
    names.reserve(impl_->customPresets.size());

    for (const auto& [name, _] : impl_->customPresets) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::expected<TransferFunctionPreset, TransferFunctionErrorInfo>
TransferFunctionManager::getPreset(const std::string& name) const {
    // Check built-in first
    auto it = impl_->builtInPresets.find(name);
    if (it != impl_->builtInPresets.end()) {
        return it->second;
    }

    // Check custom
    it = impl_->customPresets.find(name);
    if (it != impl_->customPresets.end()) {
        return it->second;
    }

    return std::unexpected(TransferFunctionErrorInfo{
        TransferFunctionError::PresetNotFound,
        "Preset not found: " + name});
}

std::expected<void, TransferFunctionErrorInfo>
TransferFunctionManager::addCustomPreset(const TransferFunctionPreset& preset, bool overwrite) {
    // Cannot overwrite built-in presets
    if (impl_->builtInPresets.contains(preset.name)) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::DuplicatePreset,
            "Cannot overwrite built-in preset: " + preset.name});
    }

    // Check for existing custom preset
    if (!overwrite && impl_->customPresets.contains(preset.name)) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::DuplicatePreset,
            "Custom preset already exists: " + preset.name});
    }

    impl_->customPresets[preset.name] = preset;
    return {};
}

std::expected<void, TransferFunctionErrorInfo>
TransferFunctionManager::removeCustomPreset(const std::string& name) {
    if (impl_->builtInPresets.contains(name)) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::PresetNotFound,
            "Cannot remove built-in preset: " + name});
    }

    auto it = impl_->customPresets.find(name);
    if (it == impl_->customPresets.end()) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::PresetNotFound,
            "Custom preset not found: " + name});
    }

    impl_->customPresets.erase(it);
    return {};
}

bool TransferFunctionManager::isBuiltInPreset(const std::string& name) const {
    return impl_->builtInPresets.contains(name);
}

std::expected<void, TransferFunctionErrorInfo>
TransferFunctionManager::saveCustomPresets(const std::filesystem::path& filePath) const {
    std::vector<TransferFunctionPreset> presets;
    presets.reserve(impl_->customPresets.size());

    for (const auto& [_, preset] : impl_->customPresets) {
        presets.push_back(preset);
    }

    std::string json = presetsToJson(presets);

    std::ofstream file(filePath);
    if (!file) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::WriteError,
            "Failed to open file for writing: " + filePath.string()});
    }

    file << json;
    if (!file) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::WriteError,
            "Failed to write to file: " + filePath.string()});
    }

    return {};
}

std::expected<size_t, TransferFunctionErrorInfo>
TransferFunctionManager::loadCustomPresets(const std::filesystem::path& filePath, bool merge) {
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::FileNotFound,
            "File not found: " + filePath.string()});
    }

    std::ifstream file(filePath);
    if (!file) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::FileNotFound,
            "Failed to open file: " + filePath.string()});
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    auto result = parsePresetsFromJson(json);
    if (!result) {
        return std::unexpected(result.error());
    }

    if (!merge) {
        impl_->customPresets.clear();
    }

    size_t count = 0;
    for (auto& preset : *result) {
        // Skip built-in preset names
        if (!impl_->builtInPresets.contains(preset.name)) {
            impl_->customPresets[preset.name] = std::move(preset);
            ++count;
        }
    }

    return count;
}

std::expected<void, TransferFunctionErrorInfo>
TransferFunctionManager::exportPreset(const std::string& name,
                                       const std::filesystem::path& filePath) const {
    auto presetResult = getPreset(name);
    if (!presetResult) {
        return std::unexpected(presetResult.error());
    }

    std::vector<TransferFunctionPreset> presets{*presetResult};
    std::string json = presetsToJson(presets);

    std::ofstream file(filePath);
    if (!file) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::WriteError,
            "Failed to open file for writing: " + filePath.string()});
    }

    file << json;
    if (!file) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::WriteError,
            "Failed to write to file: " + filePath.string()});
    }

    return {};
}

std::expected<std::string, TransferFunctionErrorInfo>
TransferFunctionManager::importPreset(const std::filesystem::path& filePath, bool overwrite) {
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::FileNotFound,
            "File not found: " + filePath.string()});
    }

    std::ifstream file(filePath);
    if (!file) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::FileNotFound,
            "Failed to open file: " + filePath.string()});
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    auto result = parsePresetsFromJson(json);
    if (!result) {
        return std::unexpected(result.error());
    }

    if (result->empty()) {
        return std::unexpected(TransferFunctionErrorInfo{
            TransferFunctionError::ParseError,
            "No presets found in file"});
    }

    auto& preset = result->front();
    auto addResult = addCustomPreset(preset, overwrite);
    if (!addResult) {
        return std::unexpected(addResult.error());
    }

    return preset.name;
}

TransferFunctionPreset TransferFunctionManager::createPreset(
    const std::string& name,
    double windowWidth,
    double windowCenter,
    const std::vector<std::tuple<double, double, double, double>>& colorPoints,
    const std::vector<std::pair<double, double>>& opacityPoints,
    const std::vector<std::pair<double, double>>& gradientOpacityPoints) {

    return TransferFunctionPreset{
        .name = name,
        .windowWidth = windowWidth,
        .windowCenter = windowCenter,
        .colorPoints = colorPoints,
        .opacityPoints = opacityPoints,
        .gradientOpacityPoints = gradientOpacityPoints
    };
}

std::filesystem::path TransferFunctionManager::getDefaultPresetsDirectory() {
#ifdef _WIN32
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        std::filesystem::path path(appData);
        CoTaskMemFree(appData);
        return path / "DicomViewer" / "presets";
    }
    return std::filesystem::current_path() / "presets";
#else
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        struct passwd* pw = getpwuid(getuid());
        if (pw != nullptr) {
            home = pw->pw_dir;
        }
    }
    if (home != nullptr) {
        return std::filesystem::path(home) / ".config" / "dicom_viewer" / "presets";
    }
    return std::filesystem::current_path() / "presets";
#endif
}

} // namespace dicom_viewer::services

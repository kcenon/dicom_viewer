#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace dicom_viewer::logging {

enum class LogLevel {
    Trace = spdlog::level::trace,
    Debug = spdlog::level::debug,
    Info = spdlog::level::info,
    Warning = spdlog::level::warn,
    Error = spdlog::level::err,
    Critical = spdlog::level::critical,
    Off = spdlog::level::off
};

struct LogConfig {
    LogLevel level = LogLevel::Info;
    bool enableFileLogging = false;
    std::filesystem::path logDirectory;
    bool jsonFormat = false;
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v";
    size_t maxFileSize = 5 * 1024 * 1024;  // 5 MB
    size_t maxFiles = 3;
};

class LoggerFactory {
public:
    static std::shared_ptr<spdlog::logger> create(const std::string& name);

    static void configure(const LogConfig& config);

    static void setGlobalLevel(LogLevel level);

    static LogLevel getGlobalLevel();

    static void shutdown();

private:
    static LogConfig config_;
    static bool configured_;
};

}  // namespace dicom_viewer::logging

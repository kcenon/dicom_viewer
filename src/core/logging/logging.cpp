#include "core/logging.hpp"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace dicom_viewer::logging {

LogConfig LoggerFactory::config_ = {};
bool LoggerFactory::configured_ = false;

std::shared_ptr<spdlog::logger> LoggerFactory::create(const std::string& name) {
    auto existingLogger = spdlog::get(name);
    if (existingLogger) {
        return existingLogger;
    }

    std::vector<spdlog::sink_ptr> sinks;

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(static_cast<spdlog::level::level_enum>(config_.level));
    sinks.push_back(consoleSink);

    if (config_.enableFileLogging && !config_.logDirectory.empty()) {
        auto logFile = config_.logDirectory / (name + ".log");
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFile.string(),
            config_.maxFileSize,
            config_.maxFiles
        );
        fileSink->set_level(static_cast<spdlog::level::level_enum>(config_.level));
        sinks.push_back(fileSink);
    }

    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(static_cast<spdlog::level::level_enum>(config_.level));
    logger->set_pattern(config_.pattern);

    spdlog::register_logger(logger);

    return logger;
}

void LoggerFactory::configure(const LogConfig& config) {
    config_ = config;
    configured_ = true;

    spdlog::set_level(static_cast<spdlog::level::level_enum>(config.level));
    spdlog::set_pattern(config.pattern);
}

void LoggerFactory::setGlobalLevel(LogLevel level) {
    config_.level = level;
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));

    spdlog::apply_all([level](std::shared_ptr<spdlog::logger> logger) {
        logger->set_level(static_cast<spdlog::level::level_enum>(level));
    });
}

LogLevel LoggerFactory::getGlobalLevel() {
    return config_.level;
}

void LoggerFactory::shutdown() {
    spdlog::shutdown();
    configured_ = false;
}

}  // namespace dicom_viewer::logging

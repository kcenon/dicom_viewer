#pragma once

#include <kcenon/common/interfaces/logger_interface.h>

#include <string>

namespace dicom_viewer {

/**
 * @brief Application-level log levels with simplified 4-tier model.
 *
 * Provides a user-facing log level abstraction that maps to the ecosystem's
 * common::interfaces::log_level. The levels are hierarchical: setting the
 * level to Information captures Exception + Error + Information messages.
 */
enum class AppLogLevel {
    Exception = 0,    ///< Unintended errors (crashes, unexpected failures)
    Error = 1,        ///< Intended error messages (validation, user-facing errors)
    Information = 2,  ///< Minimal information flow (key operations, state transitions)
    Debug = 3         ///< Maximum information flow (detailed traces, variable dumps)
};

/**
 * @brief Convert AppLogLevel to ecosystem log_level.
 */
inline kcenon::common::interfaces::log_level to_ecosystem_level(AppLogLevel level) {
    using kcenon::common::interfaces::log_level;
    switch (level) {
        case AppLogLevel::Exception:   return log_level::critical;
        case AppLogLevel::Error:       return log_level::error;
        case AppLogLevel::Information: return log_level::info;
        case AppLogLevel::Debug:       return log_level::debug;
    }
    return log_level::info;
}

/**
 * @brief Convert ecosystem log_level to AppLogLevel.
 */
inline AppLogLevel from_ecosystem_level(kcenon::common::interfaces::log_level level) {
    using kcenon::common::interfaces::log_level;
    switch (level) {
        case log_level::critical: return AppLogLevel::Exception;  // fatal is alias
        case log_level::error:    return AppLogLevel::Error;
        case log_level::warning:  return AppLogLevel::Information; // warn is alias
        case log_level::info:     return AppLogLevel::Information;
        case log_level::debug:    return AppLogLevel::Debug;
        case log_level::trace:    return AppLogLevel::Debug;
        case log_level::off:      return AppLogLevel::Exception;
    }
    return AppLogLevel::Information;
}

/**
 * @brief Convert AppLogLevel to display string.
 */
inline std::string to_string(AppLogLevel level) {
    switch (level) {
        case AppLogLevel::Exception:   return "Exception";
        case AppLogLevel::Error:       return "Error";
        case AppLogLevel::Information: return "Information";
        case AppLogLevel::Debug:       return "Debug";
    }
    return "Information";
}

/**
 * @brief Convert string to AppLogLevel.
 */
inline AppLogLevel app_log_level_from_string(const std::string& str) {
    if (str == "Exception")   return AppLogLevel::Exception;
    if (str == "Error")       return AppLogLevel::Error;
    if (str == "Debug")       return AppLogLevel::Debug;
    return AppLogLevel::Information;
}

/**
 * @brief Convert AppLogLevel to integer for QSettings storage.
 */
inline int to_settings_value(AppLogLevel level) {
    return static_cast<int>(level);
}

/**
 * @brief Convert integer from QSettings to AppLogLevel.
 */
inline AppLogLevel from_settings_value(int value) {
    if (value >= 0 && value <= 3) {
        return static_cast<AppLogLevel>(value);
    }
    return AppLogLevel::Information;
}

}  // namespace dicom_viewer

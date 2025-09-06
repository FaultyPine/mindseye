#pragma once 

#include "core/me_defines.h"
#include "core/me_core.h"
struct EngineContext;

enum LogLevel
{
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,

    LOG_NUM_LEVELS,
};

MEAPI bool InitializeLogger();
MEAPI void ShutdownLogger();
MEAPI void SetLogLevel(LogLevel level, bool toggle);
MEAPI void LogMessage(LogLevel level, u32 logCategory, const char* lineEnd, const char* message, ...);

#define DECLARE_LOG_CATEGORY(name) u32 LogCategory##name = HashStringComptime(ME_MACRO_STRINGIZE(LogCategory##name));
#define FORWARD_DECLARE_LOG_CATEGORY(name) MEAPI extern u32 LogCategory##name;

FORWARD_DECLARE_LOG_CATEGORY(Default)

#define LOG_FATAL(message, ...) LogMessage(LOG_LEVEL_FATAL, LogCategoryDefault, "\n", message, __VA_ARGS__)
#define LOG_ERROR(message, ...) LogMessage(LOG_LEVEL_ERROR, LogCategoryDefault, "\n", message, __VA_ARGS__)
#define LOG_WARN(message, ...) LogMessage(LOG_LEVEL_WARN, LogCategoryDefault, "\n", message, __VA_ARGS__)
#define LOG_INFO(message, ...) LogMessage(LOG_LEVEL_INFO, LogCategoryDefault, "\n", message, __VA_ARGS__)
#define LOG_DEBUG(message, ...) LogMessage(LOG_LEVEL_DEBUG, LogCategoryDefault, "\n", message, __VA_ARGS__)
#define LOG_TRACE(message, ...) LogMessage(LOG_LEVEL_TRACE, LogCategoryDefault, "\n", message, __VA_ARGS__)

// TODO: convert all logs to use categories...


#pragma once 

#include "me_defines.h"
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

MEAPI bool InitializeLogger(EngineContext* engine);
MEAPI void ShutdownLogger();
MEAPI void SetLogLevel(LogLevel level, bool toggle);
MEAPI void LogMessage(LogLevel level, const char* message, ...);

#define LOG_FATAL(message, ...) LogMessage(LOG_LEVEL_FATAL, message, __VA_ARGS__)
#define LOG_ERROR(message, ...) LogMessage(LOG_LEVEL_ERROR, message, __VA_ARGS__)
#define LOG_WARN(message, ...) LogMessage(LOG_LEVEL_WARN, message, __VA_ARGS__)
#define LOG_INFO(message, ...) LogMessage(LOG_LEVEL_INFO, message, __VA_ARGS__)
#define LOG_DEBUG(message, ...) LogMessage(LOG_LEVEL_DEBUG, message, __VA_ARGS__)
#define LOG_TRACE(message, ...) LogMessage(LOG_LEVEL_TRACE, message, __VA_ARGS__)




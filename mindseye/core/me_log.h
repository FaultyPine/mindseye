#pragma once 

#include "me_defines.h"
// NOTE: logger includes both logging *and* assertions

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

#ifdef COMPILER_MSVC
C_LINKAGE void __cdecl __debugbreak(void);
#define DEBUG_BREAK __debugbreak()
#else
#define DEBUG_BREAK __builtin_trap()
#endif

MEAPI bool InitializeLogger();
MEAPI void ShutdownLogger();
MEAPI void SetLogLevel(LogLevel level, bool toggle);
MEAPI const char* TextFormat(const char *text, ...);
MEAPI void LogMessage(LogLevel level, const char* message, ...);

#define LOG_FATAL(message, ...) LogMessage(LOG_LEVEL_FATAL, message, __VA_ARGS__)
#define LOG_ERROR(message, ...) LogMessage(LOG_LEVEL_ERROR, message, __VA_ARGS__)
#define LOG_WARN(message, ...) LogMessage(LOG_LEVEL_WARN, message, __VA_ARGS__)
#define LOG_INFO(message, ...) LogMessage(LOG_LEVEL_INFO, message, __VA_ARGS__)
#define LOG_DEBUG(message, ...) LogMessage(LOG_LEVEL_DEBUG, message, __VA_ARGS__)
#define LOG_TRACE(message, ...) LogMessage(LOG_LEVEL_TRACE, message, __VA_ARGS__)


#ifdef ME_ASSERTIONS_ENABLED
#define ME_ASSERT(x) \
    if (!(x)) Unlikely { LOG_FATAL("%s | %s:%i", #x, __FILE__, __LINE__); DEBUG_BREAK; }
#define UNIMPLEMENTED() ME_ASSERT(!"Unimplemented!");
#else
#define ME_ASSERT(x)
#define UNIMPLEMENTED()
#endif


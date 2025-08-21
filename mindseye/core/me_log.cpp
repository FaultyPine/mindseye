
#include "me_log.h"

#include "core/me_memory.h"
#include "core/me_string.h"
#include "platform/me_os.h"
#include "external/stb/stb_sprintf.h"

static u32 LOG_LEVELS_ENABLED = 0;

// defaults
#define LOG_LEVEL_FATAL_ENABLED 1
#define LOG_LEVEL_ERROR_ENABLED 1
#define LOG_LEVEL_WARN_ENABLED 1
#define LOG_LEVEL_INFO_ENABLED 1
#define LOG_LEVEL_DEBUG_ENABLED 1
#define LOG_LEVEL_TRACE_ENABLED 1

bool InitializeLogger(EngineContext* engine)
{
    SetLogLevel(LOG_LEVEL_FATAL, LOG_LEVEL_FATAL_ENABLED);
    SetLogLevel(LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_ENABLED);
    SetLogLevel(LOG_LEVEL_WARN, LOG_LEVEL_WARN_ENABLED);
    SetLogLevel(LOG_LEVEL_INFO, LOG_LEVEL_INFO_ENABLED);
    SetLogLevel(LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG_ENABLED);
    SetLogLevel(LOG_LEVEL_TRACE, LOG_LEVEL_TRACE_ENABLED);
    return true;
}
void ShutdownLogger()
{
    // nothing going on here for now. When logging to file exists this will do something
}

void SetLogLevel(LogLevel level, bool toggle)
{
    SET_BIT(LOG_LEVELS_ENABLED, level, toggle);
}

#define TERMINAL_COLORED_OUTPUT_ENABLED 1

static const char* level_strings[6] = {"[FATAL]", "[ERROR]", "[WARN]", "[INFO]", "[DEBUG]", "[TRACE]"};

#ifdef OS_WINDOWS

#ifndef STD_INPUT_HANDLE
#define STD_INPUT_HANDLE    (-10)
#define STD_OUTPUT_HANDLE   (-11)
#define STD_ERROR_HANDLE    (-12)
#endif
EXT_IMPORT int
SetConsoleTextAttribute(
    void* hConsoleOutput,
    unsigned short wAttributes);
EXT_IMPORT void*
GetStdHandle(
    unsigned long nStdHandle);
static int terminal_colors[6] = {4, 4, 6, 2, 1, 1};

#else
static const char* terminal_colors[6] = {"\033[0;31m", "\033[0;31m", "\033[0;33m", "\033[0;32m", "\033[0;34m", "\033[0;34m"};
#endif

void SetTerminalColor(LogLevel level)
{
#ifdef OS_WINDOWS
    void* hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (level < 0)
    {
        int defaultWhite = 7;
        SetConsoleTextAttribute(hConsole, defaultWhite);
    }
    else
    {
        SetConsoleTextAttribute(hConsole, terminal_colors[level]);
    }
#else
    if (level < 0)
    {
        ConsolePrint("\033[0m");
    }
    else
    {
        ConsolePrint(terminal_colors[level]);
    }
#endif
}

void LogMessage(LogLevel level, const char* message, ...)
{
    if ((LOG_LEVELS_ENABLED & (1 << level)) == 0)
    {
        static bool OneTimeWarning = false;
        if (!OneTimeWarning && LOG_LEVELS_ENABLED == 0)
        {
            OneTimeWarning = true;
            ConsolePrint("[WARNING] No log levels enabled, logs will not be displayed"); // did you forget InitializeLogger()?
        }
        return;
    }

    constexpr s32 log_message_limit = 16000;
    char out_msg[log_message_limit]; // hardcoded log limit...

    va_list args;
    va_start(args, message);
    s32 bytesWritten = stbsp_vsnprintf(out_msg, log_message_limit, message, args);
    va_end(args);
    ME_ASSERT(bytesWritten < log_message_limit);
    const char* processedMsg = TextFormat("%s\n", out_msg);

    // append (optional)color and log level to message
#if TERMINAL_COLORED_OUTPUT_ENABLED
    SetTerminalColor(level);
    ConsolePrint(processedMsg);
    SetTerminalColor((LogLevel)-1);
#else
    ConsolePrint(processedMsg);
#endif
}

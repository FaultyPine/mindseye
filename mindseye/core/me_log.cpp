
#include "me_log.h"

#include "core/me_memory.h"
#include "core/me_string.h"
#include "platform/me_os.h"
#include "external/stb/stb_sprintf.h"

static u32 LOG_LEVELS_ENABLED = 0;
static u32 LOG_CATEGORIES_ENABLED = 0;

DECLARE_LOG_CATEGORY(Default)

bool InitializeLogger()
{
	// default all on, TODO: engine config
	LOG_LEVELS_ENABLED = ~0;
	LOG_CATEGORIES_ENABLED = ~0;
	meOSInitializeLogging();
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

void LogMessage(LogLevel level, const char* file, u32 line, u32 logCategory, const char* lineEnd, const char* message, ...)
{
    if ((LOG_LEVELS_ENABLED & (1 << level)) == 0 ||
		(LOG_CATEGORIES_ENABLED & (1 << logCategory)) == 0)
    {
        static bool OneTimeWarning = false;
        if (!OneTimeWarning && LOG_LEVELS_ENABLED == 0)
        {
            OneTimeWarning = true;
            ConsolePrint(STRING_LIT("[WARNING] No log levels enabled, logs will not be displayed")); // did you forget InitializeLogger()?
        }
        return;
    }

    constexpr s32 LOG_MSG_LIMIT = 16000;
    char msgBuffer[LOG_MSG_LIMIT]; // hardcoded log limit...

    va_list args;
    va_start(args, message);
    s32 bytesWritten = stbsp_vsnprintf(msgBuffer, LOG_MSG_LIMIT, message, args);
    va_end(args);
    ME_ASSERT(bytesWritten < LOG_MSG_LIMIT);
	StringView fileStr = StringFromCString(file);
	StringView outMsg = StringFormat("[%.*s:%i] %s%s", STRING_VAARGS(fileStr), line, msgBuffer, lineEnd);
	ME_ASSERT(outMsg.cstr());

    // append (optional)color and log level to message
#if TERMINAL_COLORED_OUTPUT_ENABLED
    SetTerminalColor(level);
    ConsolePrint(outMsg);
    SetTerminalColor((LogLevel)-1);
#else
    ConsolePrint(outMsg);
#endif
}

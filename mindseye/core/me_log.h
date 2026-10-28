#pragma once

#include "mindseye/core/me_defines.h"

#define FMTQUILL_USE_EXCEPTIONS 0
#ifdef ME_COMPILE_RUN
#include "quill/bundled/fmt/base.h"
#include <string_view>
#else
// DLLs share engine's Quill thread contexts/backend
#ifdef MEEXPORT
#define QUILL_DLL_EXPORT
#else
#define QUILL_DLL_IMPORT
#endif
#define QUILL_DISABLE_NON_PREFIXED_MACROS
#define QUILL_NO_EXCEPTIONS
#include "quill/Logger.h"
#include "quill/LogMacros.h"
#endif

struct LoggerConfig
{
    const char* filename = "MindseyeDebug.txt";
    u64 maxFileSize = 10 * 1024 * 1024;
    u32 maxBackupFiles = 5;
};

// Initialize/shutdown on the main thread, with producers stopped before shutdown.
MEAPI bool InitializeLogger(const LoggerConfig& config = {});
MEAPI void ShutdownLogger();
MEAPI void FlushLogger();

#ifdef ME_COMPILE_RUN
MEAPI void LogCompileRunMessage(const char* category, const char* message, fmtquill::format_args args);
template <typename... Args>
void LogCompileRun(const char* category, const char* message, Args&&... args)
{
    LogCompileRunMessage(category, message, fmtquill::make_format_args(args...));
}
#define ME_DECLARE_LOG_CATEGORY(name)
#define ME_DEFINE_LOG_CATEGORY(name)
#else
MEAPI void SetLogLevel(quill::LogLevel minimumLevel);
MEAPI void SetLogCategoryEnabled(const char* name, bool enabled);
MEAPI quill::Logger* GetLogCategoryLogger(const char* name);
// define log categories in a single translation unit. Other translation units can declare the same log categories if
// they want to use other ones
#define ME_DECLARE_LOG_CATEGORY(name) MEAPI quill::Logger* MELogCategory##name()
#define ME_DEFINE_LOG_CATEGORY(name) \
    EXT_EXPORT quill::Logger* MELogCategory##name() { return GetLogCategoryLogger(#name); }
#endif

ME_DECLARE_LOG_CATEGORY(Default);

// Quill takes std::string_view, so this translates our engine StringView/String types
template <typename T>
std::string_view LogSV(const T& string)
{
    return {string.data ? string.data : "", static_cast<size_t>(string.len)};
}

#ifdef ME_COMPILE_RUN
// Format on the host and print synchronously. I was seeing issues when using quill in CompileRun
#define ME_LOG(category, quillMacro, message, ...) \
    LogCompileRun(#category, message __VA_OPT__(,) __VA_ARGS__)
#else
// Quill filters by the category logger's level before evaluating arguments.
#define ME_LOG(category, quillMacro, message, ...) \
    do { \
        if (auto* meLogger = MELogCategory##category()) \
        { \
            quillMacro(meLogger, message __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while (0)
#endif

#define LOG_FATAL(category, message, ...) \
    do { ME_LOG(category, QUILL_LOG_CRITICAL, message __VA_OPT__(,) __VA_ARGS__); FlushLogger(); } while (0)
#define LOG_ERROR(category, message, ...) ME_LOG(category, QUILL_LOG_ERROR, message __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARN(category, message, ...) ME_LOG(category, QUILL_LOG_WARNING, message __VA_OPT__(,) __VA_ARGS__)
#define LOG_INFO(category, message, ...) ME_LOG(category, QUILL_LOG_INFO, message __VA_OPT__(,) __VA_ARGS__)
#define LOG_DEBUG(category, message, ...) ME_LOG(category, QUILL_LOG_DEBUG, message __VA_OPT__(,) __VA_ARGS__)
#define LOG_TRACE(category, message, ...) ME_LOG(category, QUILL_LOG_TRACE_L1, message __VA_OPT__(,) __VA_ARGS__)

#include "me_log.h"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/RotatingFileSink.h"
#include "platform/me_os.h"

namespace
{
// Shared ownership and sink types are required by Quill's API.
std::shared_ptr<quill::Sink> fileSink;
std::shared_ptr<quill::Sink> consoleSink;
const quill::PatternFormatterOptions pattern{
    "%(time) [%(thread_id)] [%(log_level)] [%(logger)] %(short_source_location) %(message)",
    "%H:%M:%S.%Qms", quill::Timezone::LocalTime};
}

ME_DEFINE_LOG_CATEGORY(Default);

MEAPI void LogCompileRunMessage(const char* category, const char* message, fmtquill::format_args args)
{
    if (!meOSHasStdout())
    {
        return;
    }
    fmtquill::print("[{}] ", category);
    fmtquill::vprint(message, args);
    fmtquill::print("\n");
    fflush(stdout);
}

bool InitializeLogger(const LoggerConfig& config)
{
    if (fileSink)
    {
        return true;
    }
    if (!config.filename || !*config.filename || config.maxFileSize < 512)
    {
        return false;
    }
    // log file always sits next to the executable
    char filename[ME_PATH_MAX];
    const char* logPath = config.filename;
    StringView folder = meOSGetExeFileFolder();
    if (folder.len + 1 + CStringLength(logPath) >= sizeof(filename))
    {
        return false;
    }
    StringFormatIntoBuf({filename, sizeof(filename)}, STRING_FMT "/%s", STRING_VAARGS(folder), logPath);
    logPath = filename;

    quill::RotatingFileSinkConfig fileConfig;
    fileConfig.set_open_mode('a');
    fileConfig.set_rotation_on_creation(true);
    fileConfig.set_rotation_max_file_size(config.maxFileSize);
    fileConfig.set_max_backup_files(config.maxBackupFiles);
    fileSink = quill::Frontend::create_or_get_sink<quill::RotatingFileSink>(logPath, fileConfig);
    if (meOSHasStdout())
    {
        consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("stdout");
    }
    quill::Backend::start();
    return true;
}

quill::Logger* GetLogCategoryLogger(const char* name)
{
    if (!fileSink)
    {
        return nullptr;
    }
    if (auto* logger = quill::Frontend::get_logger(name))
    {
        return logger;
    }
    auto* logger = consoleSink
        ? quill::Frontend::create_or_get_logger(name, {fileSink, consoleSink}, pattern)
        : quill::Frontend::create_or_get_logger(name, fileSink, pattern);
    logger->set_log_level(quill::LogLevel::TraceL3);
    return logger;
}

void SetLogLevel(quill::LogLevel minimumLevel)
{
    if (fileSink)
    {
        fileSink->set_log_level_filter(minimumLevel);
    }
    if (consoleSink)
    {
        consoleSink->set_log_level_filter(minimumLevel);
    }
}

void SetLogCategoryEnabled(const char* name, bool enabled)
{
    if (auto* logger = GetLogCategoryLogger(name))
    {
        logger->set_log_level(enabled ? quill::LogLevel::TraceL3 : quill::LogLevel::None);
    }
}

void FlushLogger()
{
    for (auto* logger : quill::Frontend::get_all_loggers())
    {
        logger->flush_log();
    }
}

void ShutdownLogger()
{
    if (!fileSink)
    {
        return;
    }
    for (auto* logger : quill::Frontend::get_all_loggers())
    {
        quill::Frontend::remove_logger_blocking(logger);
    }
    quill::Backend::stop();
    fileSink.reset();
    consoleSink.reset();
}

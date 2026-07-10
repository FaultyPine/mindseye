#include "me_cmdline.h"

#include "core/me_app.h"
#include <stdlib.h>

template <typename T>
T parseCmdLineArgFromString(StringView value)
{
    XASSERT(false);
    return {};
}
template<> s32 parseCmdLineArgFromString(StringView value)
{
    return atoi(value.data);
}
template<> const char* parseCmdLineArgFromString(StringView value)
{
    return value.data;
}
template<> StringView parseCmdLineArgFromString(StringView value)
{
    return value;
}
template<> bool parseCmdLineArgFromString(StringView value)
{
    ToLower(value);
    return value == STRING_LIT("true") || value == STRING_LIT("1");
}

void parseCmdLine(s32 argc, char* argv[], CommandLineArgs& args)
{
    for (s32 i = 1; i < argc; i++)
    {
        StringView arg = StringView(argv[i], CStringLength(argv[i]));
        #define X(name, type) if (StringCompare(arg, STRING_LIT("-" #name), StringOpFlags_CaseInsensitive)) \
        { args.has##name = true; args.ME_MACRO_CONCAT(,name) = parseCmdLineArgFromString<type>(arg); }

        COMMAND_LINE_ARGS_DECL
        #undef X
    }
}

void InitializeCmdLine(s32 argc, char* argv[])
{
    CommandLineArgs*& args = GetEngineCtx()->cmdLine;
    args = MENEW(&GetEngineCtx()->engineArena, CommandLineArgs);
    parseCmdLine(argc, argv, *args);
}

const CommandLineArgs& GetCommandLineArgs()
{
    return *GetEngineCtx()->cmdLine;
}
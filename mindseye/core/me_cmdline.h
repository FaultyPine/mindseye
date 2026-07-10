#pragma once

#include "core/me_defines.h"

#define COMMAND_LINE_ARGS_DECL \
X(ResourceDir, const char*)\
X(ShouldRunTests, bool)

enum CommandLineArgOption
{
#define X(name, type) CmdLine_##name,
COMMAND_LINE_ARGS_DECL
#undef X
};

struct CommandLineArgs
{
#define X(name, type) \
    bool has##name = false;\
    type name = {};

COMMAND_LINE_ARGS_DECL
#undef X
};

#define CMDLINE_HAS(name) (GetCommandLineArgs().has##name)

void InitializeCmdLine(s32 argc, char* argv[]);
const CommandLineArgs& GetCommandLineArgs();



#include "me_os_win.h"
#include "core/me_string.h"

#include <stdlib.h>
#include <shellapi.h>
#pragma comment(lib, "shell32")

#include "core/me_log.h"
#include "core/me_memory.h"

C_LINKAGE MEAPI EntryPointFunc globalEntryPointFunc;

// from shell32
EXT_IMPORT LPWSTR * CommandLineToArgvW(
    LPCWSTR lpCmdLine,
    int     *pNumArgs
);

int me_os_win_main(int argc, char** argv)
{
    return globalEntryPointFunc ? globalEntryPointFunc(argc, argv) : 1;
}

int me_os_win_main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    #define MAX_COMMAND_LINE_OPTIONS 50
    #define MAX_COMMAND_LINE_OPTION_LENGTH 100
    char formattedCmdLine[MAX_COMMAND_LINE_OPTIONS][MAX_COMMAND_LINE_OPTION_LENGTH];
    ME_MEMCLEAR(formattedCmdLine, sizeof(formattedCmdLine));
    int argc;
    LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
    for (int i = 0; i < argc; i++)
    {
        wcharToNarrow(argv[i], formattedCmdLine[i], MAX_COMMAND_LINE_OPTION_LENGTH);
    }
    return globalEntryPointFunc ? globalEntryPointFunc(argc, (char**)formattedCmdLine) : 1;
}




void* LoadDynamicLibrary(const char* name)
{
    return (void*)LoadLibraryA(name);
}


#include "me_os_win.h"

#include <stdlib.h>
#include <shellapi.h>
#pragma comment(lib, "shell32")

#include "core/me_log.h"
#include "core/me_memory.h"

static EntryPointFunc globalEntryPointFunc = nullptr;


void __InternalRegisterOSEntryPoint(EntryPointFunc entryPoint)
{
    if (globalEntryPointFunc)
    {
        LOG_ERROR("Cannot register OS entry point more than once!");
    }
    globalEntryPointFunc = entryPoint;
}

void ConsolePrint(const char* text, u32 textLen) 
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD charsWritten;
    WriteConsole(hConsole, text, textLen, &charsWritten, nullptr);
}

// from shell32
MEAPI LPWSTR * CommandLineToArgvW(
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
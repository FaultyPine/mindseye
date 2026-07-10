

#include "mindseye/core/me_defines.h"
#include "mindseye/core/me_log.h"
#include "mindseye/platform/me_os.h"


#ifdef OS_WINDOWS

#include <stdlib.h> // for the argc and argv macros... would rather not have to pull this whole thing in
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <shellapi.h>

#define MAX_CMDLINE_ARGS 50
#define MAX_CMDLINE_ARG_SIZE 100

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    int nArgs;
    LPWSTR* cmdlineList = CommandLineToArgvW(GetCommandLine(), &nArgs);
    char cmdlineContent[MAX_CMDLINE_ARGS][MAX_CMDLINE_ARG_SIZE];
    char* argv[MAX_CMDLINE_ARGS];
    for (int i = 0; i < nArgs; i++)
    {
        wcharToNarrow(cmdlineList[i], &cmdlineContent[i][0], MAX_CMDLINE_ARG_SIZE);
        argv[i] = cmdlineContent[i];
    }
    LocalFree(cmdlineList);
    return meOSMain(nArgs, argv);
}
#endif
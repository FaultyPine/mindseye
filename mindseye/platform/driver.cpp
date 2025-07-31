

#include "mindseye/core/me_defines.h"
#include "mindseye/core/me_log.h"

#ifdef OS_WINDOWS

#include "mindseye/platform/windows/me_os_win.cpp"
#include <stdlib.h> // for the argc and argv macros... would rather not have to pull this whole thing in

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    void* gameLib = LoadDynamicLibrary("testbed.dll");
    if (!gameLib)
    {
        LOG_ERROR("Failed to load game library!");
        return 1;
    }
    meOSWinMain(__argc, __argv);
}

#else
#error unsupported os driver entrypoint
#endif
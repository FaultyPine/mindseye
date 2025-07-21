


#include "core/me_defines.h"

#ifdef OS_WINDOWS
#include "windows/me_os_win.cpp"
#endif


static EntryPointFunc globalEntryPointFunc = nullptr;

int me_os_platform_main(int argc, char** argv)
{
    return me_os_win_main(argc, argv);
}

void __InternalRegisterOSEntryPoint(EntryPointFunc entryPoint)
{
    if (globalEntryPointFunc)
    {
        LOG_ERROR("Cannot register OS entry point more than once!");
    }
    globalEntryPointFunc = entryPoint;
}

void ConsolePrint(const char* text) 
{
    OutputDebugStringA(text);
}





#include "core/me_defines.h"

#ifdef OS_WINDOWS
#include "windows/me_os_win.cpp"
#endif


void meOSCreateWindow(WindowCreationParams creationParams, EngineContext* engine)
{
#ifdef OS_WINDOWS
    return meOSWinCreateWindow(creationParams, engine);
#else
#error "Unimplemented OS entrypoint"
#endif
}
struct EngineContext;
void meOSTick(EngineContext* engine)
{
#ifdef OS_WINDOWS
    meOSWinTick(engine);
#else
#error "Unimplemented OS entrypoint"
#endif
}

void* meOSReserveVirtualMemory(u64 size)
{
#ifdef OS_WINDOWS
    return meOSWinReserveVirtualMemory(size);
#else
#error "Unimplemented OS entrypoint"
#endif
}

int meOSPlatformMain(int argc, char** argv)
{
#ifdef OS_WINDOWS
    return meOSWinMain(argc, argv);
#else
#error "Unimplemented OS entrypoint"
#endif
}

void ConsolePrint(const char* text) 
{
#ifdef OS_WINDOWS
    OutputDebugStringA(text);
#endif
}


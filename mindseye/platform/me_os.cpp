


#include "core/me_defines.h"

#ifdef OS_WINDOWS
#include "windows/me_os_win.cpp"
#endif
#include "core/me_log.h"

#ifndef ME_CORE_ONLY

void meOSCreateWindow(WindowCreationParams creationParams, EngineContext* engine)
{
#ifdef OS_WINDOWS
    return meOSWinCreateWindow(creationParams, engine);
#else
#error "Unimplemented OS meOSCreateWindow"
#endif
}
struct EngineContext;
void meOSTick(EngineContext* engine)
{
#ifdef OS_WINDOWS
    meOSWinTick(engine);
#else
#error "Unimplemented OS meOSTick"
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

#endif


void* meOSReserveVirtualMemory(u64 size)
{
#ifdef OS_WINDOWS
    return meOSWinReserveVirtualMemory(size);
#else
#error "Unimplemented OS meOSReserveVirtualMemory"
#endif
}

const char* meOSFsDirectorySeperator()
{
#ifdef OS_WINDOWS
    return "\\";
#else
#error "Unimplemented OS meOSFsDirectorySeperator"
#endif
}

OSFileReference::~OSFileReference()
{
    if (flags & ScopedFile)
    {
        meOSWinCloseFile(*this);
    }
}

bool meOSOpenFile(OSFileReference& file, const char* path, OSFileFlags flags)
{
#ifdef OS_WINDOWS
    return meOSWinOpenFile(file, path, flags);
    #else
#error "Unimplemented OS meOSOpenFile"
#endif
}

bool meOSCloseFile(OSFileReference& file)
{
#ifdef OS_WINDOWS
    return meOSWinCloseFile(file);
    #else
#error "Unimplemented OS meOSOpenFile"
#endif
}

bool meOSReadFileContents(const OSFileReference& file, void* backingBuffer, size_t backingBufferSize)
{
#ifdef OS_WINDOWS
    return meOSWinReadFileContents(file, backingBuffer, backingBufferSize);
#else
#error "Unimplemented OS meOSReadFileContents"
#endif
}

u64 meOSGetFileSize(const OSFileReference& file)
{
#ifdef OS_WINDOWS
	return meOSWinGetFileSize(file);
#else
#error "Unimplemented OS getfilesize"
#endif
}

void ConsolePrint(const char* text) 
{
#ifdef OS_WINDOWS
    OutputDebugStringA(text);
#endif
}


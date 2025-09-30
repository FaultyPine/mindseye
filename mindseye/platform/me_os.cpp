


#include "core/me_defines.h"

#ifdef OS_WINDOWS
#include "windows/me_os_win.cpp"
#endif
#include "core/me_log.h"

static OSStateView g_osData;

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


#endif

void meOSInitializeLogging()
{
	#ifdef OS_WINDOWS
	
	#else
	#error "unimplemented os meOSInitializeLogging"
	#endif
}

void* meOSReserveVirtualMemory(u64 size)
{
#ifdef OS_WINDOWS
    return meOSWinReserveVirtualMemory(size);
#else
#error "Unimplemented OS meOSReserveVirtualMemory"
#endif
}

void* meOSCommitVirtualMemory(void* ptr, u64 size)
{
	#ifdef OS_WINDOWS
    return meOSWinCommitVirtualMemory(ptr, size);
	#else
	#error "Unimplemented OS meOSCommitVirtualMemory"
	#endif
}

void meOSFreeVirtualMemory(
	void* data)
{
	#ifdef OS_WINDOWS
    return meOSWinFreeVirtualMemory(data);
	#else
	#error "Unimplemented OS meOSFreeVirtualMemory"
	#endif
}

char meOSFsDirectorySeperator()
{
#ifdef OS_WINDOWS
    return '\\';
#else
	return '/';
#endif
}

OSFileReference::~OSFileReference()
{
    if (flags & ScopedFile)
    {
        meOSWinCloseFile(*this);
    }
}

bool meOSEnsureDirectoriesExist(const char* pathCStr);

bool meOSOpenFile(OSFileReference& file, StringView path, OSFileFlags flags)
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

void ConsolePrint(StringView text) 
{
	#ifdef OS_WINDOWS
	void* hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteFile(hConsole, text.data, text.len, nullptr, nullptr);
	#if BUILD_DEBUG
	OutputDebugStringA(text.data);
	#endif
	#endif
}




#include "core/me_defines.h"

// NOTE: static on purpose. So it is separate from other allocated data
// so that we don't accidentally include it in record/replay savestates
static OSStateView g_osData;

#ifdef OS_WINDOWS
#include "windows/me_os_win.cpp"
#endif
#include "core/me_log.h"



void meOSInitializeLogging()
{
	#ifdef OS_WINDOWS
	
	#else
	#error "unimplemented os meOSInitializeLogging"
	#endif
}

StringView meOSFsDirectorySeperator()
{
//#ifdef OS_WINDOWS
//    return '\\';
//#else
	return STRING_LIT("/");
//#endif
}

OSFileReference::~OSFileReference()
{
    if (flags & OSFileFlags_ScopedFile)
    {
        meOSCloseFile(*this);
    }
}


bool meOSSetFileCursor(
	const OSFileReference& file,
	u64 offset,
	OSFileCursorMode mode)
{
	DWORD moveMethod = 0;
	switch (mode)
	{
		case BEGIN: { moveMethod = FILE_BEGIN; break; }
		case CURRENT: { moveMethod = FILE_CURRENT; break; }
		case END: { moveMethod = FILE_END; break; }
		default: break;
	}
	LARGE_INTEGER offsetLi;
	offsetLi.QuadPart = offset;
	DWORD result = SetFilePointerEx(file.fileHandle, offsetLi, nullptr, moveMethod);
	bool success = result != INVALID_SET_FILE_POINTER;
	if (!success)
	{
		LOG_WARN("Failed to set file cursor %s", file.path);
	}
	return success;
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

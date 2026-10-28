


#include "core/me_defines.h"
#include "platform/me_os.h"
#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include "core/me_log.h"

ME_DEFINE_LOG_CATEGORY(Platform);



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

meMemoryMappedFile::~meMemoryMappedFile()
{
    if (flags & OSFileFlags_ScopedFile)
    {
        meOSUnmapFile(*this);
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
		LOG_WARN(Platform, "Failed to set file cursor {}", file.path);
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

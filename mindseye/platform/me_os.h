#pragma once

#include "mindseye/core/me_defines.h"
#include "mindseye/core/me_arena.h"
#include "mindseye/core/me_string.h"
struct EngineContext;

struct WindowCreationParams
{
    String name = STRING_LIT("Mindseye");
    u32 width = 1280;
    u32 height = 720;
};

typedef void(*OnOSWindowResize)(s32 width, s32 height);
typedef void(*OnOSMouseMove)(s32 mx, s32 my);

struct MouseState
{
    s32 mouseX = 0;
    s32 mouseY = 0;
    s32 scroll = 0;
    enum MouseButtons : u32
    {
        LBUTTON,
        RBUTTON,
        MBUTTON,
    };
    u32 buttons = 0;
};

struct OSStateView
{
    // TODO: make these events so multiple systems can subscribe
    OnOSWindowResize onResizeCB = nullptr;
    OnOSMouseMove onMouseMove = nullptr;
    MouseState mouseState = {};
    u32 windowWidth = 0;
    u32 windowHeight = 0; 
	u64 ticksPerSecond = 0;
	u64 ticksAtAppStart = 0;
	u64 GetTicksUsec() const;
#ifdef OS_WINDOWS
    void* hwnd = nullptr;
    void* hinstance = nullptr;
#else
#endif
};
extern OSStateView g_osData;

#ifdef OS_WINDOWS
#define NOMINMAX
#define UNICODE
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#else
#error unknown os-specific defines
#endif

enum OSFileFlags
{
	OnlyIfExists = NTH_BIT(0),
	StompExisting = NTH_BIT(1),
    ScopedFile = NTH_BIT(2),
	DeleteOnFileClose = NTH_BIT(3),
};

enum OSFileCursorMode
{
	BEGIN, CURRENT, END
};
struct OSFileReference;

MEAPI void ConsolePrint(StringView text);
MEAPI void* LoadDynamicLibrary(const char* name);
MEAPI void* GetFunctionPtr(void* module, StringView functionName);

MEAPI void meOSInitializeLogging();
MEAPI s32 meOSMain(s32 argc, char** argv);

MEAPI void meOSCreateWindow(
	WindowCreationParams creationParams, 
	EngineContext* engine);

MEAPI void meOSTick(
	EngineContext* engine);

MEAPI void* meOSReserveVirtualMemory(
	u64 size);

MEAPI void* meOSCommitVirtualMemory(
	void* ptr,
	u64 size);

MEAPI void meOSFreeVirtualMemory(
	void* data);

MEAPI StringView meOSFsDirectorySeperator();

#define ME_OS_OPENFILE(varname, path, flags) OSFileReference varname; meOSOpenFile(varname, path, flags);

MEAPI bool meOSOpenFile(
	OSFileReference& file, 
	StringView path, 
	OSFileFlags flags = OSFileFlags(0));

MEAPI bool meOSCloseFile(
	OSFileReference& file);

MEAPI bool meOSDeleteFile(
	OSFileReference& file);

MEAPI bool meOSReadFileContents(
	const OSFileReference& file, 
	void* backingBuffer, 
	size_t backingBufferSize);

MEAPI bool meOSWriteFileContent(
	const OSFileReference& file,
	void* buffer,
	size_t amtToWrite);

MEAPI bool meOSSetFileCursor(
	const OSFileReference& file,
	u64 offset,
	OSFileCursorMode mode);

MEAPI size_t meOSGetFileSize(
	const OSFileReference& file);

MEAPI bool meOSFileExists(
	const OSFileReference& file);

MEAPI bool meOSFileDelete(
	const OSFileReference& file);

MEAPI StringView meOSGetExeFilepath();
MEAPI StringView meOSGetExeFileFolder();
MEAPI StringView meOSGetWorkingDir();

MEAPI String meOSResolveRelativeToAbsPath(
	meAllocator* allocator,
	StringView potentiallyRelativePath);

MEAPI u32 meOSGetThreadID();

struct OSFileReference
{
    ~OSFileReference();
    OSFileFlags flags = OSFileFlags(0);
    #ifdef OS_WINDOWS
    void* fileHandle = 0;
    char path[PATH_MAX] = {};
    #else
    #error unsupported filereference platform
    #endif

	// TODO: implement copy/assign/move 
	// and take OSFileFlags::ScopedFile into account
	OSFileReference() = default;
	OSFileReference(StringView str)
	{
		bool result = meOSOpenFile(*this, str, (OSFileFlags)(OSFileFlags::ScopedFile | OSFileFlags::OnlyIfExists));
		ME_ASSERT(result);
	}
	void InitWithoutOpening(StringView str)
	{
		ME_MEMCLEAR((void*)path, PATH_MAX);
		StringCopy({path, PATH_MAX}, str);
	}
	bool HasOpenFile() const { return reinterpret_cast<s64>(fileHandle) != -1; }
};

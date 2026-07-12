#pragma once

#include "mindseye/core/me_defines.h"
#include "mindseye/core/me_arena.h"
#include "mindseye/core/me_string.h"
#include "mindseye/core/containers/dynarray.h"
#include "platform/me_input.h"
struct EngineContext;

struct WindowCreationParams
{
    String name = STRING_LIT("Mindseye");
    u32 width = 1280;
    u32 height = 720;
};

typedef void(*OnOSWindowResize)(s32 width, s32 height);
typedef void(*OnOSMouseMove)(s32 mx, s32 my);

struct OSStateView
{
    // TODO: make these events so multiple systems can subscribe
    OnOSWindowResize onResizeCB = nullptr;
	meMouseInput mouseState = {};
	meKeyboardInput keyboardState = {};
    u32 windowWidth = 0;
    u32 windowHeight = 0;
	u64 ticksPerSecond = 0;
	u64 ticksAtAppStart = 0;
	meOSCursorState cursorState = FREE;
	bool userInputBlocked = false;
	u64 GetTicksUsec() const;
#ifdef OS_WINDOWS
    void* hwnd = nullptr;
    void* hinstance = nullptr;
	bool useRawInput = false;
#else
#endif
};

#ifdef OS_WINDOWS
#define NOMINMAX
#define UNICODE
#else
#error unknown os-specific defines
#endif

typedef u32 OSFileFlags;
enum OSFileFlags_
{
	OSFileFlags_OnlyIfExists      = NTH_BIT(0),
	OSFileFlags_StompExisting     = NTH_BIT(1),
    OSFileFlags_ScopedFile        = NTH_BIT(2),
	OSFileFlags_DeleteOnFileClose = NTH_BIT(3),
	OSFileFlags_IsDirectory       = NTH_BIT(4),
    OSFileFlags_ReadOnly          = NTH_BIT(5),
};

enum OSFileCursorMode { BEGIN, CURRENT, END };
struct OSFileReference;

MEAPI void  ConsolePrint(StringView text);
MEAPI void* LoadDynamicLibrary(const char* name);
MEAPI void  UnloadDynamicLibrary(void* module);
MEAPI void* GetFunctionPtr(void* module, StringView functionName);
MEAPI bool  meOSCopyFile(const char* src, const char* dst);

// ── Virtual memory ───────────────────────────────────────────────────────────

MEAPI void* meOSReserveVirtualMemory(u64 size);
MEAPI void* meOSCommitVirtualMemory(void* ptr, u64 size);
MEAPI void  meOSFreeVirtualMemory(void* data);
MEAPI void  meOSCommitMemory(void* ptr, u64 size);
// Reserve + commit with MEM_WRITE_WATCH. Free with meOSFreeVirtualMemory.
MEAPI void* meOSAllocWriteTracked(u64 size);
// Returns addresses of pages written since the last reset. pageSize is filled with the system page size.
MEAPI void  meOSGetWrittenAddresses(void* base, u64 size, void** pagesOut, u64* countInOut, u64* pageSize);
MEAPI void  meOSResetWrittenAddresses(void* base, u64 size);

// ── Memory-mapped files ───────────────────────────────────────────────────────

typedef u32 meMapFileFlags;
enum meMapFileFlags_
{
    meMapFileFlags_None        = 0,
    meMapFileFlags_ReserveOnly = NTH_BIT(0), // SEC_RESERVE: commit sub-ranges with meOSCommitMemory before use
};

struct meMemoryMappedFile
{
    void* ptr  = nullptr;
    u64   size = 0;
#ifdef OS_WINDOWS
    void* fileHandle    = nullptr;
    void* mappingHandle = nullptr;
#endif
};
// path == nullptr: backed by system pagefile.
MEAPI bool meOSMapFile(meMemoryMappedFile& out, const char* path, u64 size, meMapFileFlags flags = meMapFileFlags_None);
MEAPI void meOSUnmapFile(meMemoryMappedFile& mapping);

// ── Process ───────────────────────────────────────────────────────────────────

typedef void(*meProcessExitCallback)(s32 exitCode, void* userData);
// onExit = nullptr: returns process handle. onExit provided: fires callback on exit, returns nullptr.
MEAPI void* meOSRunProcessAsync(const char* workingDir, StringView command, meProcessExitCallback onExit = nullptr, void* userData = nullptr);
MEAPI s32   meOSWaitForProcess(void* processHandle);

// ── Window / tick ─────────────────────────────────────────────────────────────

MEAPI void meOSInitializeLogging();
MEAPI s32  meOSMain(s32 argc, char** argv);
MEAPI void meOSCreateWindow(WindowCreationParams creationParams, EngineContext* engine);
MEAPI void meOSTick(EngineContext* engine);

// ── Filesystem ────────────────────────────────────────────────────────────────

MEAPI StringView meOSFsDirectorySeperator();
MEAPI bool       meOSEnsureDirectoriesExist(const char* pathCstr);

#define ME_OS_OPENFILE(varname, path, flags) OSFileReference varname; meOSOpenFile(varname, path, flags);

MEAPI bool   meOSOpenFile(OSFileReference& file, StringView path, OSFileFlags flags = OSFileFlags(0));
MEAPI bool   meOSCloseFile(OSFileReference& file);
MEAPI bool   meOSDeleteFile(OSFileReference& file);
MEAPI bool   meOSReadFileContents(const OSFileReference& file, void* backingBuffer, size_t backingBufferSize);
MEAPI bool   meOSWriteFileContent(const OSFileReference& file, void* buffer, size_t amtToWrite);
MEAPI bool   meOSSetFileCursor(const OSFileReference& file, u64 offset, OSFileCursorMode mode);
MEAPI size_t meOSGetFileSize(const OSFileReference& file);
MEAPI bool   meOSFileExists(const OSFileReference& file);
MEAPI bool   meOSFileDelete(const OSFileReference& file);
MEAPI bool   meOSReadDirectory(const OSFileReference& folder, DynArray<OSFileReference>& result);

struct FileTimestamps { u64 lastWrite = 0; u64 lastRead = 0; u64 created = 0; };
MEAPI FileTimestamps meOSGetFileTimestamps(const OSFileReference& file);

MEAPI StringView meOSGetExeFilepath();
MEAPI StringView meOSGetExeFileFolder();
MEAPI StringView meOSGetWorkingDir();
MEAPI String     meOSResolveRelativeToAbsPath(meAllocator* allocator, StringView potentiallyRelativePath);
MEAPI u32        meOSGetThreadID();
MEAPI void       meOSSetCursorState(meOSCursorState state, OSStateView& osState);
MEAPI bool       AmIBeingDebugged();

struct OSFileReference
{
    ~OSFileReference();
    OSFileFlags flags = OSFileFlags(0);
    #ifdef OS_WINDOWS
    void* fileHandle = 0;
    char path[ME_PATH_MAX] = {};
    #else
    #error unsupported filereference platform
    #endif

	// TODO: implement copy/assign/move
	OSFileReference() = default;

	void InitWithoutOpening(StringView str)
	{
		StringCopy({path, ME_PATH_MAX}, str);
		ME_ASSERT(path[str.len] == '\0');
	}
	bool HasOpenFile() const { return reinterpret_cast<s64>(fileHandle) != -1; }
	StringView GetPath() const { return StringFromCString(path); }
};

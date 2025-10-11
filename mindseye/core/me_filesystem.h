#pragma once

#include "core/me_defines.h"

// TODO: revamp this. I want to to ONLY be able to issue BATCH ASYNC filesystem requests.
// If i actually want a single sync request, I'd pass n=1 and call meWaitForFileOp after issuing the request, or provide a helper func that explicitly does that waiting.
MEAPI bool meReadFileContents(StringView filepath, meSpan backingBuffer);

MEAPI size_t meGetFileSize(const char* filepath);

MEAPI StringView meFsGetDirectorySeperator();

// NOTE: will NormalizePathSeperators on the passed in path
MEAPI StringView meFsGetFileFromFullPath(StringView path);

MEAPI StringView msFsGetDirFromPath(StringView path);

MEAPI void meFsNormalizePathSeperators(StringView str);

// looks for the given file
// first, tries the file string itself
// if the file string isn't a relative path, tries looking in current working dir
// then tries looking in all parent dirs to the current working dir
MEAPI StringView meFsScanOutForFile(StringView file, meAllocator* allocator);

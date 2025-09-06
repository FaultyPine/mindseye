#pragma once

#include "core/me_defines.h"

// TODO: revamp this. I want to to ONLY be able to issue BATCH ASYNC filesystem requests.
// If i actually want a single sync request, I'd pass n=1 and call meWaitForFileOp after issuing the request, or provide a helper func that explicitly does that waiting.
MEAPI bool meReadFileContents(const char* filepath, meSpan backingBuffer);
MEAPI size_t meGetFileSize(const char* filepath);
MEAPI const char* meFsGetDirectorySeperator();




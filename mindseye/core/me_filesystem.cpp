#include "me_filesystem.h"

#include "platform/me_os.h"
#include "core/me_log.h"

bool meReadFileContents(const char* filepath, meSpan backingBuffer)
{
    OSFileReference file = OSFileReference{.flags = ScopedFile};
    if (!meOSOpenFile(file, filepath))
    {
        LOG_ERROR("[meFS] failed to open file %s", filepath);
        return false;
    }
    bool result = meOSReadFileContents(file, backingBuffer.data, backingBuffer.size);
    return result;
}

size_t meGetFileSize(const char* filepath)
{
    UNIMPLEMENTED();
    return 0;
}


const char* meFsGetDirectorySeperator()
{
    return meOSFsDirectorySeperator();
}

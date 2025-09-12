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


char meFsGetDirectorySeperator()
{
    return meOSFsDirectorySeperator();
}

void meFsNormalizePathSeperators(StringView str)
{
	for (u32 i = 0; i < str.len; i++)
	{
		if (str.data[i] == '\\')
		{
			str.data[i] = '/';
		}
	}
}

StringView meFsGetFilepathFromPath(StringView path)
{
	char dirSepC = meFsGetDirectorySeperator();
	StringView dirSep = StringView(&dirSepC, 1);
	meFsNormalizePathSeperators(dirSep);
	s32 lastDirSep = FindInStringRev(path, dirSep, 0, StringOpFlags_IdxAfterNeedle);
	if (lastDirSep == -1)
	{
		return path;
	}
	StringView result = path.OffsetView(lastDirSep);
	return result;
}

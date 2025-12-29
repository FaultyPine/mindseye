#include "me_filesystem.h"

#include "mindseye/platform/me_os.h"
#include "mindseye/core/me_log.h"

bool meReadFileContents(StringView filepath, meSpan backingBuffer)
{
    OSFileReference file;
    if (!meOSOpenFile(file, filepath, (OSFileFlags)(OSFileFlags::ScopedFile | OSFileFlags::OnlyIfExists)))
    {
        LOG_ERROR("[meFS] failed to open file %.*s", STRING_VAARGS(filepath));
        return false;
    }
    bool result = meOSReadFileContents(file, backingBuffer.data, backingBuffer.size);
    return result;
}

StringView meFsGetDirectorySeperator()
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

StringView meFsGetFileFromFullPath(StringView path)
{
	StringView dirSep = meFsGetDirectorySeperator();
	meFsNormalizePathSeperators(dirSep);
	s32 lastDirSep = FindInStringRev(path, dirSep, 0, StringOpFlags_IdxAfterNeedle);
	if (lastDirSep == -1)
	{
		return path;
	}
	StringView result = path.OffsetView(lastDirSep);
	return result;
}

StringView msFsGetDirFromPath(StringView path)
{
	StringView dirSep = meFsGetDirectorySeperator();
	meFsNormalizePathSeperators(dirSep);
	meFsNormalizePathSeperators(path);
	s32 lastDirSep = FindInStringRev(path, dirSep, 0, StringOpFlags_IdxAfterNeedle);
	if (lastDirSep == -1)
	{
		return path;
	}
	if (FindInString(path, STRING_LIT("."), lastDirSep) == -1)
	{
		// Assumption: files will always have an extension
		// TODO: in debug mode do an actual OS check here for folder/file
		return path;
	}
	StringView result = StringView(path.data, MEMAX(0, lastDirSep-1));
	return result;
}


StringView meFsScanOutForFile(StringView fileStr)
{
	meAllocator* allocator = GetTLScratch();
	OSFileReference file;
	file.InitWithoutOpening(fileStr);
	if (meOSFileExists(file))
	{
		return meMove(String(fileStr, allocator));
	}
	String absPath = meOSResolveRelativeToAbsPath(allocator, fileStr);
	file.InitWithoutOpening(absPath);
	if (meOSFileExists(file))
	{
		return absPath;
	}
	String result = absPath;
	StringBuilder builder = StringBuilder(allocator);
	while (result)
	{
		// take abs path, chop out furthest nested directory and check existance one at a time
		s32 lastFolderEnd = FindInStringRev(result, meFsGetDirectorySeperator());
		if (lastFolderEnd == -1)
		{
			break;
		}
		s32 lastFolderStart = FindInStringRev(result, meFsGetDirectorySeperator(), result.len - lastFolderEnd);
		if (lastFolderStart == -1)
		{
			break;
		}
		StringView beforeFolder = result.OffsetView(0, lastFolderStart);
		StringView afterFolder = result.OffsetView(lastFolderEnd);
		builder.Append(beforeFolder);
		builder.Append(afterFolder);
		file.InitWithoutOpening(builder);
		result = builder;
		if (meOSFileExists(file))
		{
			return result;
		}
		builder.Clear();
	}
	return {};
}
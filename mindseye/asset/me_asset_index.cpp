
#include "me_asset_index.h"


meAssetIndex& meAssetIndexGet()
{
	return *GetEngineCtx()->assetIndex;
}


StringView meAssetIndexGetFilesystemPath(
	const MAID& maid)
{
	meAssetIndex& assetIndex = meAssetIndexGet();
	auto it = assetIndex.filesystemPaths.find(maid);
	if (it != assetIndex.filesystemPaths.end())
	{
		return it->second;
	}
	return {};
}

void OnFoundAssetFile(
	meAssetIndex& assetIndex,
	const OSFileReference& file)
{

}


#define ME_ASSET_INDEX_FILE "meAssetIndex.idx"

void meAssetIndexInitialize(EngineContext* engine)
{
	engine->assetIndex = MENEW(&engine->engineArena, meAssetIndex);
	meAssetIndex& assetIndex = meAssetIndexGet();
	// TODO: async job this whole func
	StringView dataDir = meAssetGetResourceDir();
	StringView assetIndexFilePath = StringFormatTmp(STRING_FMT STRING_FMT STRING_FMT, 
					dataDir, meFsGetDirectorySeperator(), STRING_LIT(ME_ASSET_INDEX_FILE));
	OSFileReference assetIndexFile = {};
	assetIndexFile.InitWithoutOpening(assetIndexFilePath);
	bool didExist = meOSFileExists(assetIndexFile);
	if (didExist)
	{
		// TODO: load asset index from cache
	}
	else
	{
		// init asset index from nothing
		
		// gather files from filesystem
		OSFileReference dataDirectory = {};
		dataDirectory.InitWithoutOpening(dataDir);
		DynArray<OSFileReference> dataFiles = DynArrayCreate<OSFileReference>(GetTLScratch());
		if (!meFsRecursiveDirectoryWalk(dataDirectory, dataFiles))
		{
			LOG_ERROR("Failed to discover all data files from data dir " STRING_FMT, STRING_VAARGS(dataDir));
		}

		// discover asset files
		for (DynArray_Foreach(dataFiles, i))
		{
			const OSFileReference& file = dataFiles[i];
			if (file.flags & OSFileFlags_IsDirectory)
			{
				continue;
			}
			StringView filename = StringFromCString(file.path);
			if (FindInString(filename, STRING_LIT(ME_ASSET_EXTENSION)) != -1)
			{
				OnFoundAssetFile(assetIndex, file);
			}
		}
	}
}


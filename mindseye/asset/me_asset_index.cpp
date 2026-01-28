
#include "me_asset_index.h"


meAssetIndex& meAssetIndexGet()
{
	return *GetEngineCtx()->assetIndex;
}


StringView meAssetIndexGetFilesystemPath(
	const MAID& maid)
{
	meAssetIndex& assetIndex = meAssetIndexGet();
	auto it = assetIndex.assetToPathMap.find(maid);
	if (it != assetIndex.assetToPathMap.end())
	{
		return it->second;
	}
	return {};
}

MAID meAssetIndexGetMAIDFromPath(
	const StringView& path)
{
	meAssetIndex& assetIndex = meAssetIndexGet();
	auto it = assetIndex.pathToAssetsMap.find(path);
	if (it != assetIndex.pathToAssetsMap.end())
	{
		return it->second;
	}
	return {};
}

StringView assetTypeFileExtensions[] =
{
	#define X(name, ext) STRING_LIT(ext),
	ME_DECLARE_ASSET_TYPES
	#undef X
};

meAssetType MapFileOrPathToAssetType(StringView path)
{
	StringView filename = meFsGetFileFromFullPath(path);
	s32 commonExt = FindInStringRev(filename, STRING_LIT(ME_ASSET_EXTENSION));
	s32 idx = FindInStringRev(filename, STRING_LIT("."), 
							  commonExt != -1 ? (filename.len - commonExt) : 0, StringOpFlags_IdxAfterNeedle);
	if (idx != -1)
	{
		StringView extension = filename.OffsetView(idx, commonExt != -1 ? Math::Abs(commonExt - idx) : ME_INT_MAX);
		for (u32 type = MABadData; type < NUM_ASSET_TYPES; type++)
		{
			StringView typeExt = assetTypeFileExtensions[type];
			if (StringCompare(extension, typeExt))
			{
				return meAssetType(type);
			}
		}
	}
	return MABadData;
}

// TODO: async process & join at end
void OnFoundAssetFile(
	meAssetIndex& assetIndex,
	const OSFileReference& file)
{
	StringView filepath = file.GetPath();
	meAssetType type = MapFileOrPathToAssetType(filepath);
	meAssetIdent fileIdent = meAssetIdent(filepath, type);
	// TODO: we don't need to fully load here
	// it would make more sense to only deserialize here, and leave
	// the "next" phases of loading for later
	meAssetRequestLoad(&fileIdent); 
	meAssetWaitUntilLoadstage(meSpanTyped<meAssetIdent>(&fileIdent, 1), Loaded);
	if (meAsset* asset = meAssetTryGet(fileIdent))
	{
		const meAssetIdent& loadedIdent = asset->ident;
		assetIndex.assetToPathMap[loadedIdent.id] = loadedIdent.diskIdent;
		assetIndex.pathToAssetsMap[loadedIdent.diskIdent] = loadedIdent.id;
	}
	else
	{
		LOG_WARN("Tried to load " STRING_FMT " for asset index but failed", filepath);
	}
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
	// TODO: load asset index from cache
	didExist = false; // TMP
	if (didExist)
	{
		UNIMPLEMENTED();
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
		u32 numAssetsDiscovered = 0;
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
				numAssetsDiscovered++;
			}
		}
		LOG_INFO("[AssetIndex] Discovered %d assets", numAssetsDiscovered);
	}
}


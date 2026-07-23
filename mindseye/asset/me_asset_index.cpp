
#include "me_asset_index.h"

#include "asset/me_asset.h"
#include "core/me_serialize.h"

#define ME_ASSET_INDEX_DEBUGLOG 0

meAssetIndex& meAssetIndexGet()
{
	return *GetEngineCtx()->assetIndex;
}

const meAssetIndex& meAssetIndexGetRO()
{
    return meAssetIndexGet();
}

void meAssetIndexRecordDependency(MAID owner, MAID dep)
{
	if (!owner || !dep)
	{
		return;
	}
	meAssetIndex& assetIndex = meAssetIndexGet();
	auto it = assetIndex.dependencyMap.find(owner);
	if (it == assetIndex.dependencyMap.end())
	{
		assetIndex.dependencyMap[owner] = DynArrayCreate<MAID>(GetDefaultAllocator());
		it = assetIndex.dependencyMap.find(owner);
	}
	DynArrayPush(it->second, dep);
}

meSpanTyped<MAID> meAssetIndexGetDependencies(MAID owner)
{
	if (!owner)
	{
		return {};
	}
	meAssetIndex& assetIndex = meAssetIndexGet();
	auto it = assetIndex.dependencyMap.find(owner);
	if (it == assetIndex.dependencyMap.end())
	{
		return {};
	}
	DynArray<MAID>& deps = it->second;
	return meSpanTyped<MAID>(deps.data, DynArrayGetSize(deps));
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
	if (!path)
	{
		return {};
	}
	StringView assetPath = meAssetGetRelPathForResource(path);
	meAssetIndex& assetIndex = meAssetIndexGet();
	auto it = assetIndex.pathToAssetsMap.find(assetPath);
	if (it != assetIndex.pathToAssetsMap.end())
	{
		return it->second;
	}
	return {};
}

u32 meAssetIndexGetUniqueID(MAID maid)
{
    if (!maid)
    {
        return 0;
    }
    meAssetIndex& assetIndex = meAssetIndexGet();
	auto it = assetIndex.serializedUniqueIdentifiers.find(maid);
	if (it != assetIndex.serializedUniqueIdentifiers.end())
	{
		return it->second;
	}
	return 0;
}

void meAssetIndexRegisterRelation(
	const StringView& path,
	const MAID& maid)
{
    if (!path || !maid)
    {
        return;
    }
	meAssetIndex& assetIndex = meAssetIndexGet();
	String pathCopy(path); // if path == assetToPathMap[maid] we need a temp copy or else we end up use-after-freeing this string mem
	assetIndex.assetToPathMap[maid] = pathCopy;
	assetIndex.pathToAssetsMap[pathCopy] = maid;
}


meAssetType meAssetFindAssetTypeFromFilepath(
    StringView path)
{
	StringView filename = meFsGetFileFromFullPath(path);
	s32 commonExt = FindInStringRev(filename, STRING_LIT(ME_ASSET_EXTENSION));
    // assumes the asset ext is the last extension
	s32 idx = FindInStringRev(filename, STRING_LIT("."), 
							  commonExt != -1 ? (filename.len - commonExt) : 0, StringOpFlags_IdxAfterNeedle);
	if (idx != -1)
	{
		StringView extension = filename.OffsetView(idx, commonExt != -1 ? Math::Abs(commonExt - idx) : ME_INT_MAX);
		for (u32 type = MABadData; type < NUM_ASSET_TYPES; type++)
		{
			StringView typeExt = meAssetFileExtFromType((meAssetType)type);
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
	meAssetType type = meAssetFindAssetTypeFromFilepath(filepath);
	StringView assetPath = meAssetGetAbsPathForResource(filepath);
	meAssetLoader* loader = meAssetSystemGet().assetLoaders[type];
	const meTypeDescriptor& typeDesc = *loader->assetTypeDesc;
	u32 size = typeDesc.size;
	Allocation outSerialized = MEALLOC(GetTLScratch(), size);
	meSerializeResult result;
    // BOOKMARK: Don't deserialize just for the asset index.
    // This should be able to JUST read the header to generate metadata about the asset
    // without actually deserializing it. I.E. disk path, MAID, any searchable metadata tags
	DeserializeContext ctx = {};
	ctx.mode = meSerializationMode_Text;
	ctx.typeDesc = &typeDesc;
	ctx.externalDataAllocator = GetTLScratch();
	ctx.outputData = outSerialized;
	ctx.outResult = &result;
	DeserializeFromFileBlocking(assetPath, ctx);
	if (result == meSerializeResult::SER_SUCCESS)
	{
		meSpan assetHeaderData = meSerializeTryGetAssetHeader(typeDesc, outSerialized);
		if (assetHeaderData)
		{
			const MAID& header = *(MAID*)assetHeaderData.data;
			if (header)
			{
				assetIndex.assetToPathMap[header] = filepath;
				assetIndex.pathToAssetsMap[filepath] = header;
				assetIndex.serializedUniqueIdentifiers[header] = result.serializedUniqueIdentifier;
			}
			else
			{
				LOG_ERROR("Failed to read asset header from " STRING_FMT, STRING_VAARGS(filepath));
			}
		}
		else
		{
			LOG_ERROR("Tried to serialize " STRING_FMT " from disk, but couldn't find an asset header field", STRING_VAARGS(meAssetTypeToString(type)));
		}
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
	StringView dataDir = meAssetGetResourceDir();
	StringView assetIndexFilePath = StringFormatTmp(STRING_FMT STRING_FMT STRING_FMT, 
		STRING_VAARGS(dataDir), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(STRING_LIT(ME_ASSET_INDEX_FILE)));
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
		
		// TODO: parallelize this whole thing!
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
    #if ME_ASSET_INDEX_DEBUGLOG
    for (const auto& [asset,path] : meAssetIndexGetRO().assetToPathMap)
    {
        LOG_INFO("%llu %u | " STRING_FMT, asset.GetID(), asset.GetType(), STRING_VAARGS(path));
    }
    #endif
}


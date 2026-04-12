#include "me_asset.h"

#include "asset/me_asset_index.h"
#include "core/me_defines.h"
#include "core/me_cmdline.h"
#include "core/me_string.h"
#include "core/me_filesystem.h"
#include "core/me_event.h"


MEEVENT_DECLARE_STATIC(registerAssetLoader);

static bool MEASSET_DEBUG_SINGLETHREADED_LOAD = 1;
constexpr u32 NUM_ASSET_COMPILER_THREADS = 1;

meAssetSystem& meAssetSystemGet()
{
	return *GetEngineCtx()->assetSystem;
}

StringView meAssetTypeToString(meAssetType type)
{
	switch (type)
	{
		#define X(type, ext) case type: return STRING_LIT(#type);
		ME_DECLARE_ASSET_TYPES
		#undef X
		default: return STRING_LIT("UnknownAssetType");
	}
}

StringView meAssetFileExtFromType(meAssetType type)
{
    switch (type)
    {
        #define X(name, ext) case name: return STRING_LIT(ext);
        ME_DECLARE_ASSET_TYPES
        #undef X
        default: return {};
    }
}

void MAIDSerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext& ctx)
{
	MAID* maid = (MAID*)ctx.data;
	char buf[32];
	int len = stbsp_snprintf(buf, sizeof(buf), "%llu", maid->GetID());
	*(json*)ctx.outputData.data = std::string_view(buf, len);
}

StringView meAssetGetProjectRootResourceDir(EngineContext* engine)
{
	StringView userAppConfigFile = engine->userConfig.projectRootConfigFile;
	if (userAppConfigFile)
	{
		String userAppConfigPathAbs = meOSResolveRelativeToAbsPath(GetTLScratch(), userAppConfigFile);
		StringView userAppConfigDir = msFsGetDirFromPath(userAppConfigPathAbs);
		return userAppConfigDir;
	}
	return {};
}

void meAssetInitialize(EngineContext* engine)
{
    engine->assetSystem = MENEW(&engine->engineArena, meAssetSystem);
	for (u32 i = 0; i < NUM_ASSET_TYPES; i++)
	{
		engine->assetSystem->registries[i].assets.reserve(100);
	}
    const CommandLineArgs& cmdline = GetCommandLineArgs();
	if (cmdline.hasResourceDir)
	{
		StringView cmdlineResDir = StringView(cmdline.ResourceDir, CStringLength(cmdline.ResourceDir));
		meAssetSetResourceDir(cmdlineResDir);
	}
	else
	{
		StringView rootResDir = meAssetGetProjectRootResourceDir(engine);
		meAssetSetResourceDir(rootResDir ? rootResDir : meOSGetWorkingDir());
	}
	engine->assetSystem->assetCompilerJobs.Initialize(&engine->engineArena, NUM_ASSET_COMPILER_THREADS);
}

void meAssetInitializeLate(EngineContext* engine)
{
	registerAssetLoader( meEventPayload{ &engine->engineArena });
}

void meAssetTeardown(EngineContext* engine)
{
	engine->assetSystem->assetCompilerJobs.Shutdown();
    MEDELETE(&engine->engineArena, meAssetSystem, engine->assetSystem);
}

void meAssetRegisterLoader(meAssetLoader* loader)
{
    meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetType type = loader->assetType;
	ME_ASSERT(assetSystem.assetLoaders[type] == nullptr && "Not allowed to overwrite existing asset loader type");
	ME_ASSERT(type != MABadData);
	ME_ASSERT(loader->resourcePool != nullptr);
	ME_ASSERT(loader->assetTypeDesc != nullptr);
    // any "asset" must have a header with it's own asset id
    // if you hit this assert, make sure your asset struct has ME_ASSET_STRUCTURE as the first field
	ME_ASSERT(loader->assetTypeDesc->fields[0].thisType == &TD_MAID);
	assetSystem.assetLoaders[type] = loader;
}

MAID meAssetCreateNewAssetID(meAssetType type)
{
	// NOTE: randomness!
    // TODO: use engine random once i implement that
	f64 time = GetTimeUsec();
	u32 randomNumber = HashBytes((u8*)&time, sizeof(time));
	MAID newMaid = MAID(randomNumber, type);
	return newMaid;
}

meAsset meAssetCreateNew(
	meAssetType type,
	StringView filename)
{
    meAssetSystem& assetSystem = meAssetSystemGet();
	MAID newMaid = meAssetCreateNewAssetID(type);
	StringView relPath = meAssetGetRelPathForResource(filename);
	meAssetIndexRegisterRelation(relPath, newMaid);

	meAssetLoader* loader = assetSystem.assetLoaders[type];
	ME_ASSERT(loader);
	meResourcePoolBase* resourcePool = loader->resourcePool;
	Eye newRuntimeResource = resourcePool->Load();
	void* opaqueAssetData = resourcePool->GetOpaque(newRuntimeResource);
	MAID* assetHeader = (MAID*)opaqueAssetData;
	*assetHeader = newMaid;

	meAsset newAsset = meAsset(newRuntimeResource, newMaid);
	meAssetTypeRegistry& reg = assetSystem.registries[type];
	RWLockWrite lock(reg.lock);
	reg.assets[newMaid] = newAsset; // copy
	return meMove(newAsset);
}

Eye meAssetCreateNewResource(meAssetType type)
{
    meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetLoader* loader = assetSystem.assetLoaders[type];
	ME_ASSERT(loader);
	meResourcePoolBase* resourcePool = loader->resourcePool;
	Eye newRuntimeResource = resourcePool->Load();
    return newRuntimeResource;
}

MAID::MAID(u64 id, meAssetType type)
{
    SetID(id);
    SetType(type);
}

meAssetLoadStage meAssetLoader::meAssetWaitForLoadstage(
	const MAID& maid,
	meAssetLoadStage loadStage)
{
	meAsset* asset = meAssetTryGet(maid);
	constexpr u32 maxAttempts = 1000;
	u32 attempts = 0;
	while (asset && asset->loadStage != loadStage && attempts++ < maxAttempts)
	{
		meThreadSleep(1); // TMP
		//GetAssetSystem().assetCompilerJobs.WaitOnJob(assetJobId);
		asset = meAssetTryGet(maid);
	}
	return asset ? asset->loadStage : Unloaded;
}

meJobId meAssetRequestLoad(
	MAID* assetIdents, 
	u32 numAssets,
    meAssetOnAssetLoadCb cb)
{
	meAssetSystem& assetSystem = meAssetSystemGet();
	for (u32 i = 0; i < numAssets; i++)
	{
		const MAID& assetIdent = assetIdents[i];
		ME_ASSERT(assetIdent);
		meAssetType assetType = meAssetType(assetIdent.GetType());
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		meAssetLoadStage stage = Unloaded;
		if (!loader)
		{
			LOG_ERROR("Tried to load asset type that doesn't have an implemented loader");
			return {}; // dev error, should never happen, unrecoverable
		}
		meAsset* asset = meAssetTryGet(assetIdent);
		// if it's already loaded, noop
		if (asset)
		{
			stage = asset->loadStage;
		}
		if (stage == Unloaded)
		{
			{ // add the slot in, and mark it as "loading"
				meAsset notYetLoadedData = meAsset(assetIdent, Loading);
				meAssetTypeRegistry& reg = assetSystem.registries[assetType];
				RWLockWrite lock(reg.lock);
				reg.assets[assetIdent] = notYetLoadedData;
			}
			struct AssetCompilerJobData
			{
				MAID ident;
				meAssetLoader* loader;
				meAssetOnAssetLoadCb cb;
			};
			assetSystem.assetBeginLoadingEvent(meEventPayload((void*)&assetIdent));
			AssetCompilerJobData jobData = {};
			jobData.ident = assetIdent;
			jobData.loader = loader;
			jobData.cb = cb;
			auto fn = [jobData, &assetSystem]() 
			{
				meAsset* asset = meAssetTryGet(jobData.ident);
				ME_ASSERT(asset);
				jobData.loader->meAssetLoad(*asset);
				ME_ASSERT(asset->id);
				ME_ASSERT(asset->loadStage == Loaded && asset->runtimeHandle);
				// each asset type can respond to loaded events
				jobData.loader->meAssetOnLoad(*asset);
				// globally, systems can also respond
				assetSystem.assetFinishedLoadingEvent(meEventPayload((void*)&jobData.ident));
				// individual callsites can also respond
				if (jobData.cb)
				{
					jobData.cb(*asset);
				}
			};
			if (MEASSET_DEBUG_SINGLETHREADED_LOAD)
			{
				fn();
			}
			else
			{
				meJobId compilerJobId = assetSystem.assetCompilerJobs.Execute(fn);
				return compilerJobId;
			}
		}
	}
	return {};
}

bool meAssetWaitUntilLoadstage(
	meSpanTyped<MAID> assetIdents, 
	meAssetLoadStage loadStage)
{
    for (u32 i = 0; i < assetIdents.size; i++)
	{
		// dispatch to the loader for this asset type
		const MAID& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdents[i].GetType());
		const meAssetSystem& assetSystem = meAssetSystemGet();
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		if (loader)
		{
			meAssetLoadStage loadedStage = loader->meAssetWaitForLoadstage(assetIdent, loadStage);
			if (loadedStage != loadStage)
			{
				return false;
			}
		}
	}
	return true;
}

meJobId meAssetRequestWrite(
	meSpanTyped<MAID> assetIdents,
	meAssetOnAssetLoadCb onWriteCb)
{
	meAssetSystem& assetSystem = meAssetSystemGet();
	for (u32 i = 0; i < assetIdents.size; i++)
	{
		const MAID& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdent.GetType());
		meAssetLoader* loader = assetSystem.assetLoaders[assetType];
		meAssetLoadStage stage = Unloaded;
		if (!loader)
		{
			LOG_ERROR("Tried to write asset type that doesn't have an implemented loader");
			return {}; // engine dev error, should never happen
		}
		meAsset* asset = meAssetTryGet(assetIdent);
		// if it's already loaded, noop
		if (asset)
		{
			stage = asset->loadStage;
		}
		if (stage == Loaded)
		{
			struct AssetCompilerJobData
			{
				MAID ident;
				meAssetLoader* loader;
				meAssetOnAssetLoadCb cb;
			};
			assetSystem.assetBeginLoadingEvent(meEventPayload((void*)&assetIdent));
			AssetCompilerJobData jobData = {};
			jobData.ident = assetIdent;
			jobData.loader = loader;
			jobData.cb = onWriteCb;
			auto fn = [jobData, &assetSystem]() 
			{
				meAsset* asset = meAssetTryGet(jobData.ident);
				ME_ASSERT(asset && asset->loadStage == Loaded && asset->runtimeHandle && asset->id);
				jobData.loader->meAssetWrite(*asset);
				if (jobData.cb)
				{
					jobData.cb(*asset);
				}
				assetSystem.assetFinishedLoadingEvent(meEventPayload((void*)&jobData.ident));
			};
			if (MEASSET_DEBUG_SINGLETHREADED_LOAD)
			{
				fn();
			}
			else
			{
				meJobId compilerJobId = assetSystem.assetCompilerJobs.Execute(fn);
				return compilerJobId;
			}
		}
	}
	return {};
}

void meAssetLoader::meAssetLoad(meAsset& asset)
{
	ME_ASSERT(asset.id.GetType() == assetType);
	meAllocator* allocator = resourcePool->GetPayloadAllocator();
	// TODO: implement async loading, so this would return loadStage=Loading
	// and would itself enqueue more asset compiling jobs for the individual parts of the asset
	meResourcePoolBase* pool = resourcePool;
	asset.runtimeHandle = pool->Load();
	void* outAsset = pool->GetOpaque(asset.runtimeHandle);
	StringView diskPath = meAssetIndexGetFilesystemPath(asset.id);
	StringView assetPath = meAssetGetAbsPathForResource(diskPath);
	meSerializeResult result = DeserializeFromFileBlocking(assetPath, allocator, *assetTypeDesc, meSpan(outAsset, assetTypeDesc->size));
	if (result)
	{
		ME_ASSERT(assetTypeDesc->fields[0].thisType == &TD_MAID);
		MAID* header = (MAID*)outAsset;
		ME_ASSERT(header->GetID() == asset.id.GetID());
		header->SetType(asset.id.GetType());
	}
	else
	{
		LOG_WARN("Failed to load asset " STRING_FMT, STRING_VAARGS(diskPath));
	}
	asset.loadStage = result ? Loaded : Unloaded;
}

void meAssetLoader::meAssetWrite(meAsset& asset)
{
	meResourcePoolBase* pool = resourcePool;
	if (!asset.runtimeHandle || asset.loadStage != Loaded)
	{
		LOG_WARN("Attempted to write an unloaded asset");
		return;
	}
	ME_ASSERT(assetTypeDesc->fields[0].thisType == &TD_MAID);

	void* assetData = pool->GetOpaque(asset.runtimeHandle);
	StringView diskPath = meAssetIndexGetFilesystemPath(asset.id);
	StringView assetPath = meAssetGetAbsPathForResource(diskPath);
	meAllocator* tempAllocator = GetTLScratch();
	StringView assetSerializedString = {};
	meSerializeResult res = SerializeToTextBlocking(*assetTypeDesc, assetData, tempAllocator, assetSerializedString);
	ME_ASSERT(res == meSerializeResult::SER_SUCCESS);
	// TODO: split the actual writing out into a separate thing?
	OSFileReference file;
	meOSOpenFile(file, assetPath, (OSFileFlags_StompExisting | OSFileFlags_ScopedFile));
	if (!meOSWriteFileContent(file, assetSerializedString.data, assetSerializedString.len))
	{
		LOG_ERROR("Failed to write asset to file. filename = " STRING_FMT "\nassetString = " STRING_FMT, STRING_VAARGS(assetPath), STRING_VAARGS(assetSerializedString));
	}
}

meAsset* meAssetTryGet(MAID assetID)
{
	meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetTypeRegistry& reg = assetSystem.registries[assetID.GetType()];
	RWLockRead(reg.lock);
	auto it = reg.assets.find(assetID);
	if (it == reg.assets.end())
	{
		return nullptr;
	}
	return &it->second;
}

void meAssetSetResourceDir(StringView dir)
{
    GetEngineCtx()->assetSystem->resourceDir = dir;
	LOG_INFO("Set resource dir = " STRING_FMT, STRING_VAARGS(dir));
}

StringView meAssetGetResourceDir()
{
    return GetEngineCtx()->assetSystem->resourceDir;
}

StringView meAssetGetAbsPathForResource(StringView resourcePath)
{
	if (!resourcePath)
	{
		return {};
	}
	meFsNormalizePathSeperators(resourcePath);
	StringView result = resourcePath;
	if (FindInString(resourcePath, meAssetGetResourceDir()) == -1)
	{
		StringView resDir = meAssetGetResourceDir();
		result = StringFormatTmp("%.*s%.*s%.*s", STRING_VAARGS(resDir), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(resourcePath));
	}
	return result;
}

StringView meAssetGetRelPathForResource(StringView resourcePath)
{
	StringView resDir = meAssetGetResourceDir();
	s32 idx = FindInString(resourcePath, resDir);
	if (idx != -1)
	{
		StringView cropped = resourcePath.OffsetView(resDir.len);
		cropped = EatChars(cropped, meFsGetDirectorySeperator());
		return cropped;
	}
	return resourcePath;
}

meSpan meSerializeTryGetAssetHeader(
	const meTypeDescriptor& typeDesc,
	meSpan serializedBuffer)
{
	for (u32 i = 0; i < typeDesc.fields.size; i++)
	{
		// search top-level fields for asset header type
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (field.thisType == &TD_MAID && StringCompare(field.name, STRING_LIT(ME_ASSET_HEADER_FIELDNAME)))
		{
			meSpan result = serializedBuffer.Subspan(field.offsetBits * 8, field.size);
			return result;
		}
	}
	return {};
}


StringView meAssetEnsurePathHasGoodExtension(
    const StringView& assetPath, 
    meAssetType inputType)
{
    meAssetType guessedType = meAssetFindAssetTypeFromFilepath(assetPath);
    if (guessedType != MABadData)
    {
        return assetPath;
    }
    // if we can't figure out the asset type from the filepath
    // chop off the extension and add the proper one to the end
    s32 firstDot = FindInString(assetPath, STRING_LIT("."));
    StringView noExtStr = firstDot != -1 ? assetPath.OffsetView(firstDot) : assetPath;
    return StringFormatTmp(STRING_FMT "." STRING_FMT ME_ASSET_EXTENSION, 
        STRING_VAARGS(noExtStr), STRING_VAARGS(meAssetFileExtFromType(inputType)));
}


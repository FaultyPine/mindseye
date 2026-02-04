#include "me_asset.h"

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

StringView MAIDSerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext ctx)
{
	MAID* maid = (MAID*)ctx.data;
	StringBuilder builder = StringBuilder(ctx.allocator);
	// we don't serialize the asset type because it's implicit
	builder.AppendFormat("%llu", maid->GetID());
	return builder;
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
	engine->assetSystem->assetRegistry.reserve(500);
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
	registerAssetLoader( meEventPayload{ &engine->engineArena });
}

void meAssetTeardown(EngineContext* engine)
{
	engine->assetSystem->assetCompilerJobs.Shutdown();
    MEDELETE(&engine->engineArena, meAssetSystem, engine->assetSystem);
}

void meAssetRegisterLoader(meAssetLoader* loader, meAssetType type)
{
    meAssetSystem& assetSystem = meAssetSystemGet();
	ME_ASSERT(assetSystem.assetLoaders[type] == nullptr && "Not allowed to overwrite existing asset loader type");
	assetSystem.assetLoaders[type] = loader;
}

meAssetIdent meAssetGetIdentFromPath(
	StringView path)
{
	StringView assetPath = meAssetGetRelPathForResource(path);
	MAID maid = meAssetIndexGetMAIDFromPath(assetPath);
	meAssetIdent result = {};
	result.diskIdent = assetPath;
	result.id = maid;
	result.assetUniqueIdentifier = 0; // ?
	return meMove(result);
}

void meAssetCreateNew(
	StringView filename,
	meAssetType type)
{
    meAssetSystem& assetSystem = meAssetSystemGet();
	meAssetLoader* loader = assetSystem.assetLoaders[type];
	ME_ASSERT(loader);
	loader->meAssetCreate(filename);
}

MAID::MAID(u64 id, meAssetType type)
{
    SetID(id);
    SetType(type);
}

meAssetLoadStage meAssetLoader::meAssetWaitForLoadstage(
	const meAssetIdent& ident,
	meAssetLoadStage loadStage)
{
	meAsset* asset = meAssetTryGet(ident);
	constexpr u32 maxAttempts = 1000;
	u32 attempts = 0;
	while (asset && asset->loadStage != loadStage && attempts++ < maxAttempts)
	{
		meThreadSleep(1); // TMP
		//GetAssetSystem().assetCompilerJobs.WaitOnJob(assetJobId);
		asset = meAssetTryGet(ident);
	}
	return asset ? asset->loadStage : Unloaded;
}

meJobId meAssetRequestLoad(
	meAssetIdent* assetIdents, 
	u32 numAssets,
    meAssetOnAssetLoadCb cb)
{
	meAssetSystem& assetSystem = meAssetSystemGet();
	for (u32 i = 0; i < numAssets; i++)
	{
		const meAssetIdent& assetIdent = assetIdents[i];
		ME_ASSERT(assetIdent);
		meAssetType assetType = meAssetType(assetIdent.id.GetType());
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
				RWLockWrite(assetSystem.assetRegistryLock);
				assetSystem.assetRegistry[assetIdent] = notYetLoadedData;
			}
			struct AssetCompilerJobData
			{
				meAssetIdent ident;
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
				ME_ASSERT(asset->ident);
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
	meSpanTyped<meAssetIdent> assetIdents, 
	meAssetLoadStage loadStage)
{
    for (u32 i = 0; i < assetIdents.size; i++)
	{
		// dispatch to the loader for this asset type
		const meAssetIdent& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdents[i].id.GetType());
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
	meSpanTyped<meAssetIdent> assetIdents,
	meAssetOnAssetLoadCb onWriteCb)
{
	meAssetSystem& assetSystem = meAssetSystemGet();
	for (u32 i = 0; i < assetIdents.size; i++)
	{
		const meAssetIdent& assetIdent = assetIdents[i];
		meAssetType assetType = meAssetType(assetIdent.id.GetType());
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
				meAssetIdent ident;
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
				ME_ASSERT(asset && asset->loadStage == Loaded && asset->runtimeHandle);
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

meAsset* meAssetTryGet(meAssetIdent assetID)
{
	meAssetSystem& assetSystem = meAssetSystemGet();
	RWLockRead(assetSystem.assetRegistryLock);
	auto it = assetSystem.assetRegistry.find(assetID);
	if (it == assetSystem.assetRegistry.end())
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

meSpan meSerializeTryGetAssetIdentHeader(
	const meTypeDescriptor& typeDesc,
	meSpan serializedBuffer)
{
	for (u32 i = 0; i < typeDesc.fields.size; i++)
	{
		// search top-level fields for asset ident type
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (field.thisType == &TD_MEASSETIDENT)
		{
			meSpan result = serializedBuffer.Subspan(field.offsetBits * 8, field.size);
			return result;
		}
	}
	return {};
}

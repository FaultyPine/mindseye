#include "me_asset.h"

#include "core/me_defines.h"
#include "core/me_cmdline.h"
#include "core/me_string.h"
#include "core/me_filesystem.h"
#include "core/me_event.h"


MEEVENT_DECLARE_STATIC(registerAssetLoader);

static bool MEASSET_DEBUG_SINGLETHREADED_LOAD = 0;
constexpr u32 NUM_ASSET_COMPILER_THREADS = 1;

meAssetSystem& meAssetSystemGet()
{
	return *GetEngineCtx()->assetSystem;
}

void meAssetInitialize(EngineContext* engine)
{
    engine->assetSystem = MENEW(&engine->engineArena, meAssetSystem);
	engine->assetSystem->assetRegistry.reserve(500);
    const CommandLineArgs& cmdline = GetCommandLineArgs();
	StringView cmdlineResDir = StringView(cmdline.ResourceDir, CStringLength(cmdline.ResourceDir));
    meAssetSetResourceDir(cmdline.hasResourceDir ? cmdlineResDir : meOSGetWorkingDir());
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

meAssetIdent::meAssetIdent(StringView diskIdent, meAssetType type)
{
    this->diskIdent = diskIdent;
    u32 identID = HashBytes((u8*)diskIdent.data, diskIdent.len);
	// NOTE: maid takes a 48 bit identifier. Currently passing a 32 bit hash, so 16 of our id bits aren't used...
	this->id = diskIdent ? MAID(identID, type) : MAID{};
}

meAssetIdent::meAssetIdent(StringView diskIdent, MAID maid)
{
	this->diskIdent = diskIdent;
	this->id = maid;
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
				ME_ASSERT(asset->loadStage == Loaded && asset->runtimeHandle);
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

StringView meAssetResource(StringView resourcePath)
{
	meFsNormalizePathSeperators(resourcePath);
	StringView result = resourcePath;
	if (FindInString(resourcePath, meAssetGetResourceDir()) == -1)
	{
		StringView resDir = meAssetGetResourceDir();
		result = StringFormatTmp("%.*s%.*s%.*s", STRING_VAARGS(resDir), STRING_VAARGS(meFsGetDirectorySeperator()), STRING_VAARGS(resourcePath));
	}
	return result;
}
